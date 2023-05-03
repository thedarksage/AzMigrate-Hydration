/*
+------------------------------------------------------------------------------------+
Copyright(c) Microsoft Corp. 2018
+------------------------------------------------------------------------------------+
File		:	NetworkEntity.h

Description	:   NetworkAdapter class is defind as per the RCM NetworkAdapter class. The member
names should match that defined in the RCM NetworkAdapter class. Uses the ESJ JSON
formatter to serialize and unserialize JSON string.

+------------------------------------------------------------------------------------+
*/

#ifndef _NETWORK_ENTITY_H
#define _NETWORK_ENTITY_H

#include "json_writer.h"
#include "json_reader.h"

#include <string>
#include <vector>
#include <boost/shared_ptr.hpp>

namespace RcmClientLib
{

    /// \brief Contains information about network configuration.
    class NetworkConfiguration
    {
    public:
        /// \brief IP address.
        std::string IpAddress;

        /// \brief subnet mask.
        std::string SubnetMask;

        /// \brief primary DNS server name.
        std::string PrimaryDnsServer;

        /// \brief alternate DNS server names.
        std::string AlternateDnsServers;

        /// \brief gateway server name.
        std::string GatewayServer;

        /// \brief a serialize function for JSON serializer 
        void serialize(JSON::Adapter& adapter)
        {
            JSON::Class root(adapter, "NetworkConfiguration", false);

            JSON_E(adapter, IpAddress);
            JSON_E(adapter, SubnetMask);
            JSON_E(adapter, PrimaryDnsServer);
            JSON_E(adapter, AlternateDnsServers);
            JSON_T(adapter, GatewayServer);
        }
    };

    /// \brief Contains information about network adapter.
    class NetworkAdaptor
    {
    public:
        /// \brief physical (MAC) address.
        /// Note: Source agent will only report network adaptors with one or more physical
        /// IP address.
        std::string PhysicalAddress;

        /// \brief network adaptor name.
        std::string Name;

        /// \brief manufacturer name.
        std::string Manufacturer;

        /// Gets or sets a value indicating whether DHCP is enabled or not.
        bool IsDhcpEnabled;

        /// Gets or sets network configuration information.
        std::vector<NetworkConfiguration> NetworkConfigurations;


        NetworkAdaptor()
            : IsDhcpEnabled(false)
        {}

        NetworkAdaptor(std::string physicalAddress,
            std::string name,
            std::string manufacturer,
            std::string maxSpeed,
            bool isDhcpEnabled,
            std::vector<NetworkConfiguration> vNetConfigs)
            :PhysicalAddress(physicalAddress),
            Name(name),
            Manufacturer(manufacturer),
            IsDhcpEnabled(isDhcpEnabled),
            NetworkConfigurations(vNetConfigs.begin(), vNetConfigs.end())
        {}

        /// \brief a serialize function for JSON serializer 
        void serialize(JSON::Adapter& adapter)
        {
            JSON::Class root(adapter, "NetworkAdaptor", false);

            JSON_E(adapter, PhysicalAddress);
            JSON_E(adapter, Name);
            JSON_E(adapter, Manufacturer);
            JSON_E(adapter, IsDhcpEnabled);
            JSON_T(adapter, NetworkConfigurations);
        }
    };

    typedef boost::shared_ptr<NetworkAdaptor>   NetworkAdaptorPtr_t;
    typedef boost::shared_ptr<NetworkConfiguration>   NetworkConfigurationPtr_t;

}
#endif
