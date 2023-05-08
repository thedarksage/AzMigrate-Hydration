/*
+------------------------------------------------------------------------------------+
Copyright(c) Microsoft Corp. 2021
+------------------------------------------------------------------------------------+
File    : AgentConfig.h

Description : This file contains the details to configure mobility agent to use
              a V2A RCM Appliance .

+------------------------------------------------------------------------------------+
*/


#ifndef AGENT_CONFIG_H
#define AGENT_CONFIG_H

#define GUID_REGEX "[0-9a-fA-F]{8}-([0-9a-fA-F]{4}-){3}[0-9a-fA-F]{12}"

#include "json_writer.h"
#include "json_reader.h"

#include <string>
#include <vector>

#include <boost/algorithm/string.hpp>

#include "RcmProxyDetails.h"

// V2A RCM related config file names
static std::string const SourceConfigFile("SourceConfig.json");
static std::string const ALRConfigFile("ALRConfig.json");

namespace RcmClientLib {

    class AgentConfigInput
    {
    public:
        /// \brief represents the machine system UUID
        std::string BiosId;

        /// \brief represetns the machine FQDN
        std::string Fqdn;

        /// \brief a serializer for JSON
        void serialize(JSON::Adapter& adapter)
        {
            JSON::Class root(adapter, "AgentConfigInput", false);

            JSON_E(adapter, BiosId);
            JSON_T(adapter, Fqdn);
        }

        /// \brief serializer method with ptree as input
        void serialize(ptree &node)
        {
            JSON_P(node, BiosId);
            JSON_P(node, Fqdn);
        }
    };

    /// \brief contains applaince and the vault details in which the appliance is registered.
    /// the vault info is used to check if the appliance switch is within the same
    /// vault or to different vault.
    class AgentConfiguration
    {
    public:
        /// \brief subscription id that is part of the vault details
        std::string SubscriptionId;

        /// \brief container id that is part of the vault details
        std::string ContainerId;

        /// \brief container name that is part of the vault details
        std::string ContainerUniqueName;

        /// \brief resource id that is part of the vault details
        std::string ResourceId;

        /// \brief resource location that is part of the vault details
        std::string ResourceLocation;

        /// \brief the biosid for which this config is generated
        std::string BiosId;

        /// \brief the fqdn for which this config is generated
        std::string Fqdn;

        /// \brief the client certificate with private key, base64 encoded
        std::string ClientCertificate;

        /// \brief the RCM proxy settings
        RcmProxyTransportSetting RcmProxyTransportSettings;

        /// \brief a serializer for JSON
        void serialize(JSON::Adapter& adapter)
        {
            JSON::Class root(adapter, "AgentConfiguration", false);

            JSON_E(adapter, SubscriptionId);
            JSON_E(adapter, ContainerId);
            JSON_E(adapter, ContainerUniqueName);
            JSON_E(adapter, ResourceId);
            JSON_E(adapter, ResourceLocation);
            JSON_E(adapter, BiosId);
            JSON_E(adapter, Fqdn);
            JSON_E(adapter, ClientCertificate);
            JSON_T(adapter, RcmProxyTransportSettings);
        }

        /// \brief serializer method with ptree as input
        void serialize(ptree &node)
        {
            JSON_P(node, SubscriptionId);
            JSON_P(node, ContainerId);
            JSON_P(node, ContainerUniqueName);
            JSON_P(node, ResourceId);
            JSON_P(node, ResourceLocation);
            JSON_P(node, BiosId);
            JSON_P(node, Fqdn);
            JSON_P(node, ClientCertificate);
            JSON_CL(node, RcmProxyTransportSettings);
        }

        bool IsSameVault(const AgentConfiguration& rhs) const
        {
            return boost::iequals(SubscriptionId, rhs.SubscriptionId) &&
                boost::iequals(ContainerId, rhs.ContainerId) &&
                boost::iequals(ContainerUniqueName, rhs.ContainerUniqueName) &&
                boost::iequals(ResourceId, rhs.ResourceId) &&
                boost::iequals(ResourceLocation, rhs.ResourceLocation);
        }

        const std::string str() const
        {
            std::stringstream ss;
            ss << MAKE_NAME_VALUE_STR(SubscriptionId)
                << MAKE_NAME_VALUE_STR(ContainerId)
                << MAKE_NAME_VALUE_STR(ContainerUniqueName)
                << MAKE_NAME_VALUE_STR(ResourceId)
                << MAKE_NAME_VALUE_STR(ResourceLocation)
                << MAKE_NAME_VALUE_STR(BiosId)
                << MAKE_NAME_VALUE_STR(Fqdn)
                << MAKE_NAME_VALUE_STR(ContainerId)
                << MAKE_NAME_VALUE_STR(ContainerId)
                << MAKE_NAME_VALUE_STR(ContainerId)
                << (ClientCertificate.empty() ? "empty," : "not-empty redacted")
                << RcmProxyTransportSettings.str();

            return ss.str();
        }
    };

    /// \brief contains the agent machine details 
    class AgentConfigurationMigrationInput
    {
    public:
        /// \brief represents the machine system UUID
        std::string BiosId;

        /// \brief represetns the machine FQDN
        std::string Fqdn;

        /// \brief a serializer for JSON
        void serialize(JSON::Adapter& adapter)
        {
            JSON::Class root(adapter, "AgentConfigInput", false);

            JSON_E(adapter, BiosId);
            JSON_T(adapter, Fqdn);
        }

        /// \brief serializer method with ptree as input
        void serialize(ptree& node)
        {
            JSON_P(node, BiosId);
            JSON_P(node, Fqdn);
        }

    };

    /// \brief agent configuration from rcmproxy
    class AgentConfigurationMigration 
    {
    public:
        /// \brief Agent Configuration RCM Stack MIgration
        AgentConfiguration MobilityAgentConfig;

        /// \brief Server Signature
        std::string ServerSignature;
       
        /// \brief a serializer for JSON
        void serialize(JSON::Adapter& adapter)
        {
            JSON::Class root(adapter, "AgentConfigForMigrationToRcmStackOutput", false);

            JSON_T(adapter, MobilityAgentConfig);
            JSON_E(adapter, ServerSignature);
        }

        /// \brief serializer method with ptree as input
        void serialize(ptree& node)
        {
            JSON_CL(node, MobilityAgentConfig);
            JSON_P(node, ServerSignature);
        }

        const std::string str() const
        {
            std::stringstream ss;
            ss << MobilityAgentConfig.str()
                << MAKE_NAME_VALUE_STR(ServerSignature);

            return ss.str();
        }
    };

    /// \brief contains the On Prem ALR machine input details
    class CreateMobilityAgentConfigForOnPremAlrMachineInput
    {
    public:
        /// \brief represents the machine system UUID
        std::string MobilityAgentResourceId;

        /// \brief represents the machine system UUID
        std::string BiosId;

        /// \brief represetns the machine FQDN
        std::string Fqdn;

        CreateMobilityAgentConfigForOnPremAlrMachineInput(
            std::string mobilityAgentResourceId,
            std::string biosId,
            std::string fqdn)
        {
            MobilityAgentResourceId = mobilityAgentResourceId;
            BiosId = biosId;
            Fqdn = fqdn;
        }

        /// \brief a serializer for JSON
        void serialize(JSON::Adapter& adapter)
        {
            JSON::Class root(adapter, "CreateMobilityAgentConfigForOnPremAlrMachineInput", false);

            JSON_E(adapter, MobilityAgentResourceId);
            JSON_E(adapter, BiosId);
            JSON_T(adapter, Fqdn);
        }

        /// \brief serializer method with ptree as input
        void serialize(ptree& node)
        {
            JSON_P(node, MobilityAgentResourceId);
            JSON_P(node, BiosId);
            JSON_P(node, Fqdn);
        }

    };

    class ALRMachineConfig {
    public:
        std::string OnPremMachineBiosId;

        std::string Passphrase;

        void serialize(JSON::Adapter& adapter)
        {
            JSON::Class root(adapter, "ALRMachineConfig", false);

            JSON_E(adapter, OnPremMachineBiosId);
            JSON_T(adapter, Passphrase);
        }

        void serialize(ptree& node)
        {
            JSON_P(node, OnPremMachineBiosId);
            JSON_P(node, Passphrase);
        }
    };

    /// \brief contains the On Prem ALR machine output details
    class CreateMobilityAgentConfigForOnPremAlrMachineOutput : public AgentConfigurationMigration
    {
    };

    class AgentConfigHelpers {
    public:

        /// \brief return serialized object of AgentConfigInput
        static std::string GetAgentConfigInputDecoded();

        /// \brief return base64 encoded serialized object of AgentConfigInput
        static std::string GetAgentConfigInput();

        /// \brief validates if the provided config is for the VM agent is running
        static bool IsConfigValid(const AgentConfiguration& agentConfig);

        /// \brief validates if provided ALR config is valid or not
        static bool IsConfigValid(const ALRMachineConfig& alrMachineConfig, std::string& errMsg);

        /// \brief return the agent configuration from the provided file path
        /// throws when fails to get the source config
        static void GetSourceAgentConfig(const std::string& sourceConfigFilePath,
            AgentConfiguration& agentConfig);

        /// \brief update RcmProxyTransport settings in source config file
        static void UpdateRcmProxyTransportSettings(const std::string& sourceConfigFilePath,
            const RcmProxyTransportSetting& rcmProxySettings);

        static void PersistSourceAgentConfig(const std::string& sourceConfigFilePath,
            AgentConfiguration& agentConfig);

        /// \brief Get ALR Machine config from file
        static SVSTATUS GetALRMachineConfig(const std::string& alrConfigFilePath,
            ALRMachineConfig& alrMachineConfig, std::stringstream& strErrMsg);

        /// \brief Persist ALR Machine config to file
        static SVSTATUS PersistALRMachineConfig(const std::string& alrConfigFilePath,
            ALRMachineConfig& alrMachineConfig, std::string& strErrMsg);

    private:
        static AgentConfiguration       m_cachedPersistedAgentConfig;
        static boost::shared_mutex      m_agentSettingsMutex;

        /// \brief fetch config from config file
        template <typename ConfigTemplate>
        static SVSTATUS GetConfig(const std::string& configFilePath,
            ConfigTemplate& config, std::stringstream& strErrMsg);

        /// \brief persist config to config file
        template <typename ConfigTemplate>
        static SVSTATUS PersistConfig(const std::string& configFilePath,
            ConfigTemplate& config, std::stringstream& strErrMsg);

    };
}

#endif
