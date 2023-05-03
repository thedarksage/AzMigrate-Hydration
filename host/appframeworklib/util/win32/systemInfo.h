#ifndef SYSTEM_INFO_H
#define SYSTEM_INFO_H
struct HostInfo
{
    std::string m_hostName ;
    std::string m_domainName ;
    std::string m_userName ;
    std::string m_ipaddress ;
    bool m_isCluster ;
    std::list<std::pair<std::string, std::string> >getPropertyList() ;
    std::string m_ErrorString;
    long m_ErrorCode;
} ;
struct BootConfigInfo
{
    std::string m_BootDirectory ;
    std::string m_Description ;
    std::string m_ScratchDirectory ;
    std::string m_TempDirectory ;
    std::list<std::pair<std::string, std::string> >getPropertyList() ;
    std::string m_ErrorString;
    long m_ErrorCode;
} ;


struct OsInfo
{
    std::string m_BootDevice ;
    std::string m_Caption ;
    std::string m_CSDVersion ;
    std::string m_LastBootUpTime ;
    std::string m_SystemDirectory ;
    std::string m_SystemDrive ;
    std::string m_WindowsDirectory ;
	std::string m_pagefile;
	DWORD m_osMajorVersion;
	DWORD m_osMinorVersion;
	WORD m_wServicePackMajorVersion;
	WORD m_wServicePackMinorVersion;
	std::list<std::string> m_installedHotFixsList;
    std::list<std::pair<std::string, std::string> >getPropertyList() ;
    std::string m_ErrorString;
    long m_ErrorCode;
} ;

struct NetworkAdapterConfig
{
	std::string m_defaultGateway ;
	bool m_dhcpEnabled ;
	std::string m_dhcpServer ;
	std::string m_dnsDomain ;
	std::string m_dnsHostName ;
    bool m_domainDNSRegistrationEnabled ;
	bool m_fullDNSRegistrationEnabled ;
	std::string m_ipaddress[100] ;
	unsigned long m_no_ipsConfigured;
	bool m_ipEnabled ;
	std::string m_ipsubnet[100] ;
	std::string m_macAddress ;
	std::list<std::pair<std::string, std::string> >getPropertyList() ;
    std::string m_ErrorString;
    long m_ErrorCode;
} ;

struct ProcessorInfo
{
    SV_UINT m_AddressWidth ;
	SV_UINT m_Architecture ;
	std::string m_Description ;
    SV_UINT m_DataWidth ;
	std::string m_DeviceId ;
	SV_UINT m_Family ;
	std::string m_Manufacturer ;
    std::list<std::pair<std::string, std::string> >getPropertyList() ;
    std::string m_ErrorString;
    long m_ErrorCode;
} ;

struct SystemInfo
{
    BootConfigInfo m_bootConfig ;
    OsInfo m_OperatingSystemInfo ;
    ProcessorInfo m_processorInfo ;
    HostInfo m_hostInfo ;
	std::list<NetworkAdapterConfig> m_nwAdapterList;
    void clean()
    {
        m_nwAdapterList.clear();
    }
    std::string m_ErrorString;
    long m_ErrorCode;
} ;
#endif