#ifndef __SYSTEM__WMI__H
#define __SYSTEM__WMI__H
#include <boost/shared_ptr.hpp>
#include "appglobals.h"
#include "wmi.h"

struct NetworkAdapterConfig
{
	std::string m_defaultGateway ;
	bool m_dhcpEnabled ;
	std::string m_dhcpServer ;
	std::string m_dnsDomain ;
	std::string m_dnsHostName ;
    bool m_domainDNSRegistrationEnabled ;
	bool m_fullDNSRegistrationEnabled ;
	std::vector<std::string> m_ipaddress ;
	unsigned long m_no_ipsConfigured;
	bool m_ipEnabled ;
	std::vector<std::string> m_ipsubnet ;
	std::string m_macAddress ;
	std::list<std::pair<std::string, std::string> >getPropertyList() ;
    std::string m_ErrorString;
    long m_ErrorCode;
	NetworkAdapterConfig()
	{
		m_dhcpEnabled = false;
		m_domainDNSRegistrationEnabled = false;
		m_fullDNSRegistrationEnabled = false;
		m_no_ipsConfigured = 0;
		m_ErrorCode = 0;
	}
} ;

class WMISysInfoImpl
{
    bool m_init ;
    WMIConnector m_wmiConnector ;
    std::string m_wmiProvider ;
public:
    WMISysInfoImpl()
    {
        m_init = false ;
    }
    SVERROR init() ;
    SVERROR loadNetworkAdapterConfiguration(std::list<NetworkAdapterConfig>& nwAdapterList) ;
	SVERROR getInstalledHotFixIds(std::list<std::string>& installedHostFixsList);
} ;




#endif