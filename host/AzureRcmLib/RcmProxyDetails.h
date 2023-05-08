/*
+------------------------------------------------------------------------------------+
Copyright(c) Microsoft Corp. 2019
+------------------------------------------------------------------------------------+
File    : RcmProxyDetails.h

Description :   RCM Proxy is the control plane for V2A scenarios. This class defines
the properties of RCM Proxy.

+------------------------------------------------------------------------------------+
*/


#ifndef RCM_PROXY_DETAILS_H
#define RCM_PROXY_DETAILS_H


#include <string>
#include <vector>

#include <boost/filesystem.hpp>
#include <boost/thread/locks.hpp>
#include <boost/thread/shared_mutex.hpp>

#include "svstatus.h"
#include "json_reader.h"
#include "json_writer.h"

#include "ServerAuthDetails.h"

namespace RcmClientLib {

    /// \brief RCM control plane types
    namespace ControlPathTransport {
        const char RcmControlPlane[] = "RcmControlPlane";
        const char RcmProxyControlPlane[] = "RcmProxyControlPlane";
    }
    
    class RcmProxyTransportSetting
    {
    public:
        /// \brief Initializes a new instance of the class.
        RcmProxyTransportSetting()
            : Port(0)
        {
        }

        /// \brief machine ID of the RCM proxy.
        std::string Id;

        /// \brief Friendly name of the RCM proxy.
        std::string FriendlyName;

        /// \brief version of the RCM proxy.
        std::string AgentVersion;

        /// \brief IP address for on premise source agents to communicate with
        /// the RCM proxy service.
        std::vector<std::string> IpAddresses;

        /// \brief Nat IP address for on premise source agents to communicate with
        /// the RCM proxy service.
        std::string NatIpAddress;

        /// \brief port to be used for communicating with the RCM proxy.
        uint32_t Port;

        /// \brief auth details for Rcm proxy
        RcmProxyAuthDetail RcmProxyAuthDetails;

        /// \brief serializer method for JSON
        void serialize(JSON::Adapter& adapter)
        {
            JSON::Class root(adapter, "RcmProxyTransportSetting", false);

            JSON_E(adapter, Id);
            JSON_E(adapter, FriendlyName);
            JSON_E(adapter, AgentVersion);
            JSON_E(adapter, IpAddresses);
            JSON_E(adapter, NatIpAddress);
            JSON_E(adapter, Port);
            JSON_T(adapter, RcmProxyAuthDetails);
        }

        /// \brief serializer method with ptree as input
        void serialize(ptree &node)
        {
            JSON_P(node, Id);
            JSON_P(node, FriendlyName);
            JSON_P(node, AgentVersion);
            JSON_VP(node, IpAddresses);
            JSON_P(node, NatIpAddress);
            JSON_P(node, Port);
            JSON_CL(node, RcmProxyAuthDetails);
        }

        bool operator==(const RcmProxyTransportSetting& rhs) const
        {
            return boost::iequals(Id, rhs.Id) &&
                boost::iequals(FriendlyName, rhs.FriendlyName) &&
                boost::iequals(AgentVersion, rhs.AgentVersion) &&
                (IpAddresses == rhs.IpAddresses) &&
                boost::iequals(NatIpAddress, rhs.NatIpAddress) &&
                (Port == rhs.Port) &&
                (RcmProxyAuthDetails == rhs.RcmProxyAuthDetails);
        }

        const std::string str() const
        {
            std::stringstream ss;
            ss << MAKE_NAME_VALUE_STR(Id)
                << MAKE_NAME_VALUE_STR(FriendlyName)
                << MAKE_NAME_VALUE_STR(AgentVersion)
                << "IpAddresses : ";
            for (std::vector<std::string>::const_iterator iter = IpAddresses.begin();
                iter != IpAddresses.end(); iter++)
            {
                ss << (*iter) << ",";
            }
            ss << MAKE_NAME_VALUE_STR(NatIpAddress)
                << MAKE_NAME_VALUE_STR(Port)
                << RcmProxyAuthDetails.str();

            return ss.str();
        }
    };

    ///  \brief represents input for source agent to switch appliance for
    ///  on-prem to Azure replication scenario.
    class SourceAgentOnPremToAzureSwitchApplianceInput
    {
    public:

        /// \brief in-memory copy of the RCM proxy settings
        RcmProxyTransportSetting   ControlPathTransportSettings;

        /// \brief serializer method for JSON
        void serialize(JSON::Adapter& adapter)
        {
            JSON::Class root(adapter, "SourceAgentOnPremToAzureSwitchApplianceInput", false);

            JSON_T(adapter, ControlPathTransportSettings);
        }

        /// \brief serializer method with ptree as input
        void serialize(ptree &node)
        {
            JSON_CL(node, ControlPathTransportSettings);
        }

    };

    ///  \brief represents input for start draining to appliance for 
    ///  resume replication scenario
    class OnPremToAzureCsStackMachineStartDrainingInput
    {
    public:

        /// \brief ProtectedDiskIds for unblocking the draining to appliance ps
        std::vector<std::string> ProtectedDiskIds;

        /// \brief serializer method for JSON
        void serialize(JSON::Adapter& adapter)
        {
            JSON::Class root(adapter, "OnPremToAzureCsStackMachineStartDrainingInput", false);

            JSON_T(adapter, ProtectedDiskIds);
        }

        /// \brief serializer method with ptree as input
        void serialize(ptree& node)
        {
            JSON_VP(node, ProtectedDiskIds);
        }

    };

    /// \brief persists the RCM proxy settings received from RCM
    /// the service (svagents) retrievs the settings from RCM
    /// sychronization is achived by file lock
    class RcmProxySettingsCacher {

        /// \brief cache file location for RCM proxy settings
        boost::filesystem::path m_cacheFilePathRCMProxySettings;

        /// \brief error message 
        mutable std::string m_errMsg;

        /// \brief a checksum to verify if the RCM proxy settings have changed
        std::string m_prevChecksumRCMProxySettings;

        /// \brief last modified timestamp of RCM proxy settings file
        time_t m_MTime;

        /// \brief in-memory copy of the RCM proxy settings
        RcmProxyTransportSetting   m_rcmProxySettings;

        /// \brief read write lock to serialize access to settings
        mutable boost::shared_mutex    m_rcmProxySettingsMutex;

    public:

        RcmProxySettingsCacher(const std::string& cacheFilePathRCMProxySettings = std::string());

        /// \brief writes the RCM proxy settings to the file if there is a change
        SVSTATUS PersistRCMProxySettings(RcmProxyTransportSetting& settings);

        /// \brief reads the settings from cache file
        SVSTATUS Read(RcmProxyTransportSetting& settings) const;

        /// \brief returns the last modified time of the cache file
        time_t GetLastModifiedTime() const;

        /// \brief returns error message
        const std::string Error()
        {
            return m_errMsg;
        }
    };

    typedef boost::shared_ptr<RcmProxySettingsCacher> RcmProxySettingsCacherPtr;
}

#endif
