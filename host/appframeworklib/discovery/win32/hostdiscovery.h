#ifndef __DISCOVERY__H
#define __DISCOVERY__H
#include "appglobals.h"
#include "host.h"
#include "wmi/systemwmi.h"
#include "vssutil.h"
#include "ruleengine/validator.h"
#include "clusterutil.h"
#include "service.h"

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
	void clean()
	{
		m_hostName = "";
		m_domainName = "";
		m_userName = "";
		m_ipaddress = "";
		m_isCluster = false;
		m_ErrorString = "";
		m_ErrorCode = 0;
	}
} ;

struct OsInfo
{
    std::string m_CSDVersion ;
    std::string m_SystemDirectory ;
    std::string m_SystemDrive ;
    std::string m_WindowsDirectory ;
	DWORD m_osMajorVersion;
	DWORD m_osMinorVersion;
	WORD m_wServicePackMajorVersion;
	WORD m_wServicePackMinorVersion;
	std::list<std::string> m_installedHotFixsList;
    std::list<std::pair<std::string, std::string> >getPropertyList() ;
    std::string m_ErrorString;
    long m_ErrorCode;
	void clean()
	{
		m_CSDVersion = "";
		m_SystemDirectory = "";
		m_SystemDrive = "";
		m_WindowsDirectory = "";
		m_osMajorVersion = 0;
		m_osMinorVersion = 0;
		m_wServicePackMajorVersion = 0;
		m_wServicePackMinorVersion = 0;
		m_installedHotFixsList.clear();
		m_ErrorString = "";
		m_ErrorCode = 0;
	}
	~OsInfo()
	{
		clean();
	}
} ;

struct ProcessorInfo
{
	DWORD m_dwProcessorType;
	WORD m_wProcessorArchitecture;
    std::list<std::pair<std::string, std::string> >getPropertyList() ;
    std::string m_ErrorString;
    long m_ErrorCode;
	void clean()
	{
		m_dwProcessorType = 0;
		m_wProcessorArchitecture = 0;
		m_ErrorString = "";
		m_ErrorCode = 0;
	}
} ;

struct SystemInfo
{
    OsInfo m_OperatingSystemInfo ;
    ProcessorInfo m_processorInfo ;
    HostInfo m_hostInfo ;
	std::list<NetworkAdapterConfig> m_nwAdapterList;
    void clean()
    {
		m_OperatingSystemInfo.clean();
		m_processorInfo.clean();
		m_hostInfo.clean();
        m_nwAdapterList.clear();
		m_ErrorString = "";
		m_ErrorCode = 0;
    }
	~SystemInfo()
	{
		clean();
	}
    std::string m_ErrorString;
    long m_ErrorCode;
} ;

struct HostLevelDiscoveryInfo
{
    InmFirewallStatus m_fwStatus ;
    std::list<InmService> m_svcList ;
    std::list<InmProcess> m_procList ;
    std::list<VssProviderProperties> m_vssProviderList; 
    SystemInfo m_sysInfo ;
	bool isThisClusterNode;
	ClusterInformation m_clusterInfo ;
	std::string m_ErrorString;
    long m_ErrorCode;

    std::string summary(std::list<ValidatorPtr> rules) ;
    void clean()
    {
	   m_fwStatus = FIREWALL_STAT_NONE;
       m_svcList.clear();
       m_procList.clear();
       m_vssProviderList.clear();
       m_sysInfo.clean(); 
       m_clusterInfo.clean();
	   isThisClusterNode = false;
	   m_ErrorString = "";
	   m_ErrorCode = 0;
    }
	~HostLevelDiscoveryInfo()
	{
		clean();
	}
	void updateToDiscoveryInfo(DiscoveryInfo& discoveryInfo);
} ;

SVERROR getHostLevelDiscoveryInfo(HostLevelDiscoveryInfo& info) ;
SVERROR discoverHostLevelsvcInfo(std::list<InmService>& serviceList) ;
void dumpHostLvelelDiscoveredInfo( HostLevelDiscoveryInfo info );
SVERROR GetSystemBootInfo( OsInfo &osInfo );
#endif