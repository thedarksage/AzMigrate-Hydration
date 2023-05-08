#ifndef _MIGRATION_POLICIES_H
#define _MIGRATION_POLICIES_H

#pragma once

#include "json_reader.h"
#include "logger.h"

#include <string>
#include <boost/asio/ip/address.hpp>
#include <boost/algorithm/string.hpp>

struct RcmRegistratonMachineInfo
{
    std::string HostId;
    std::string ApplianceFqdn;
    std::string IpAddresses;
    std::string RcmProxyPort;
    std::string Otp;
    std::string IsCredentialLessDiscovery;
    std::string ClientRequestId;

    std::vector<std::string> getApplianceAddresses() const
    {
        DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
        std::vector<std::string> applianceAddresses;
        std::vector<std::string> ipv4addresses;
        
        if (!IpAddresses.empty())
        {
            boost::split(ipv4addresses, IpAddresses, boost::is_any_of(","));
            for (int i = 0; i < ipv4addresses.size(); i++)
            {
                boost::system::error_code ec;
                boost::asio::ip::address_v4::from_string(ipv4addresses[i], ec);
                if (!ec)
                    applianceAddresses.push_back(ipv4addresses[i]);
            }
        }       

        if (!ApplianceFqdn.empty())
            applianceAddresses.push_back(ApplianceFqdn);

        DebugPrintf(SV_LOG_DEBUG, "EXITED %s, ApplianceAddresses = %s\n",
            FUNCTION_NAME, boost::algorithm::join(applianceAddresses, ",").c_str());
        return applianceAddresses;
    }

    void serialize(JSON::Adapter& adapter)
    {
        JSON::Class root(adapter, "RcmRegistratonMachineInfo", false);

        JSON_E(adapter, HostId);
        JSON_E(adapter, ApplianceFqdn);
        JSON_E(adapter, IpAddresses);
        JSON_E(adapter, RcmProxyPort);
        JSON_E(adapter, Otp);
        JSON_E(adapter, IsCredentialLessDiscovery);
        JSON_E(adapter, ClientRequestId);
    }

    void serialize(ptree& node)
    {
        JSON_P(node, HostId);
        JSON_P(node, ApplianceFqdn);
        JSON_P(node, IpAddresses);
        JSON_P(node, RcmProxyPort);
        JSON_P(node, Otp);
        JSON_P(node, IsCredentialLessDiscovery);
        JSON_P(node, ClientRequestId);

        return;
    }
};
struct RcmFinalReplicationCycleInfo
{
    std::string HostId;
    std::string ClientRequestId;
    std::string TagType;
    std::string TagGuid;

    void serialize(JSON::Adapter& adapter)
    {
        JSON::Class root(adapter, "RcmFinalReplicationCycleInfo", false);

        JSON_E(adapter, HostId);
        JSON_E(adapter, ClientRequestId);
        JSON_E(adapter, TagType);
        JSON_E(adapter, TagGuid);
    }

    void serialize(ptree& node)
    {
        JSON_P(node, HostId);
        JSON_P(node, ClientRequestId);
        JSON_P(node, TagType);
        JSON_P(node, TagGuid);

        return;
    }
};
struct RcmResumeReplicationInfo
{
    std::string HostId;
    std::string ClientRequestId;

    void serialize(JSON::Adapter& adapter)
    {
        JSON::Class root(adapter, "RcmResumeReplicationInfo", false);

        JSON_E(adapter, HostId);
        JSON_E(adapter, ClientRequestId);
    }

    void serialize(ptree& node)
    {
        JSON_P(node, HostId);
        JSON_P(node, ClientRequestId);

        return;
    }
};
struct RcmMigrationInfo
{
    std::string HostId;
    std::string ClientRequestId;

    void serialize(JSON::Adapter& adapter)
    {
        JSON::Class root(adapter, "RcmMigrationInfo", false);

        JSON_E(adapter, HostId);
        JSON_E(adapter, ClientRequestId);
    }

    void serialize(ptree& node)
    {
        JSON_P(node, HostId);
        JSON_P(node, ClientRequestId);

        return;
    }
};

#endif