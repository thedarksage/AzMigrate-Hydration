
#include "RcmContracts.h"
#include "fingerprintmgr.h"

using namespace RcmClientLib;

std::string ProcessServerTransportSettings::GetIPAddress() const
{
    std::string IPAddress;
    if (!NatIpAddress.empty())
    {
        IPAddress = NatIpAddress;
    }
    else
    {
        // because of PHP serialize limitations of the TRANSPORT_CONNECTION_SETTINGS
        // for V2A Legacy, sending comma separated list of addresses instead of
        // a vector of addresses. The lowest layer in the connection (see client.h)
        // makes use of the list of addresses to iterate while connecting to server
        std::stringstream ssAddresses;

        if (!FriendlyName.empty())
            ssAddresses << FriendlyName;

        std::vector<std::string>::const_iterator ipIter = IpAddresses.begin();
        for (/**/; ipIter != IpAddresses.end(); ipIter++)
        {
            if (!ssAddresses.str().empty())
                ssAddresses << ",";
            ssAddresses << *ipIter;
        }

        IPAddress = ssAddresses.str();
    }

    // automatically load the fingerprint when the IP address is queried
    g_serverCertFingerprintMgr.setProcessServerCertFingerprint(
        ProcessServerAuthDetails.ServerCertThumbprint);

    return IPAddress;
}