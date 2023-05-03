#ifdef SECURITY_WIN16
#undef SECURITY_WIN16
#endif
#ifdef SECURITY_KERNEL
#undef SECURITY_KERNEL
#endif

#ifndef SECURITY_WIN32
#define SECURITY_WIN32
#endif 
#include <boost/lexical_cast.hpp>
#include "hostdiscovery.h"
#include <atlbase.h>
#include "ruleengine.h"
#include "service.h"
#include "portablehelpersmajor.h"
#include <Security.h>
#include "consistency/taggenerator.h"
#include "system.h"
#include "util/win32/vssutil.h"

SVERROR GetProcessorInfo(ProcessorInfo& procInfo)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    SVERROR  bRet = SVS_OK ;
	SYSTEM_INFO systemInfo;
	GetSystemInfo( &systemInfo );
	procInfo.m_dwProcessorType = systemInfo.dwProcessorType;
	procInfo.m_wProcessorArchitecture = systemInfo.wProcessorArchitecture;
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}

SVERROR GetHostInfo( HostInfo& hostInfo )
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    SVERROR  bRet = SVS_OK ;
	USES_CONVERSION;
	hostInfo.m_hostName = Host::GetInstance().GetHostName();
    hostInfo.m_ipaddress = Host::GetInstance().GetIPAddress(); 
	CHAR* nameBuffer;
	ULONG  size = 256;
	nameBuffer = new CHAR[256];
	if( GetUserName( nameBuffer, &size ) != 0 )
	{
		hostInfo.m_userName = nameBuffer;
		if( nameBuffer != NULL )
		{
			delete[] nameBuffer;
		}
	}
	if( isClusterNode() )
	{
		hostInfo.m_isCluster = true;
	}
	else
	{
		hostInfo.m_isCluster = false;
	}
    getDnsName(hostInfo.m_domainName)  ;
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}

SVERROR GetSystemBootInfo( OsInfo &osInfo )
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	SVERROR  retErrorSattus = SVS_OK ;
	USES_CONVERSION;
	
	TCHAR pszSysDirPath[64];
	if( GetSystemDirectory(pszSysDirPath, 64) != 0 )
	{
		osInfo.m_SystemDirectory = pszSysDirPath;
		DebugPrintf(SV_LOG_DEBUG,"System Dir: %s \n", osInfo.m_SystemDirectory.c_str()); 
	}
	else
	{
		retErrorSattus = SVS_FALSE;
		DebugPrintf(SV_LOG_ERROR, "GetSystemDirectory() failed. ERROR: %d\n", GetLastError() ) ;
	}	
	
	TCHAR pszwinDirPath[64];
	if( GetWindowsDirectory(pszwinDirPath, 64) != 0 )
	{
		osInfo.m_WindowsDirectory = pszwinDirPath;
		DebugPrintf(SV_LOG_DEBUG,"Windows Dir Path: %s \n", osInfo.m_WindowsDirectory.c_str()); 
	}
	else
	{
		retErrorSattus = SVS_FALSE;
		DebugPrintf(SV_LOG_ERROR, "GetWindowsDirectory() failed.ERROR: %d\n", GetLastError() ) ;
	}
	OSVERSIONINFOEX versionInfo;
	versionInfo.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
	if( GetVersionEx( (OSVERSIONINFO*)(&versionInfo) ) != 0 )
	{
		osInfo.m_CSDVersion = versionInfo.szCSDVersion;
		osInfo.m_osMajorVersion = versionInfo.dwMajorVersion;
		osInfo.m_osMinorVersion = versionInfo.dwMinorVersion;
		osInfo.m_wServicePackMajorVersion = versionInfo.wServicePackMajor;
		osInfo.m_wServicePackMinorVersion = versionInfo.wServicePackMinor;

		DebugPrintf(SV_LOG_DEBUG,"CDS Version: %s \n", osInfo.m_CSDVersion.c_str()); 
	}
	else
	{
		retErrorSattus = SVS_FALSE;
		DebugPrintf(SV_LOG_ERROR, "GetVersionEx() failed. ERROR: %d\n", GetLastError() ) ;
	}

	char driveLetter[256];
    if (SVGetVolumePathName(osInfo.m_SystemDirectory.c_str(), driveLetter, ARRAYSIZE(driveLetter)) == TRUE)
	{
		osInfo.m_SystemDrive = driveLetter;
		DebugPrintf(SV_LOG_DEBUG,"System Drive: %s \n", osInfo.m_CSDVersion.c_str()); 
	}
	else
	{
		retErrorSattus = SVS_FALSE;
		DebugPrintf(SV_LOG_ERROR, "SVGetVolumePathName() failed. ERROR: %d\n", GetLastError() ) ;
	}

	WMISysInfoImpl wmiSysImpleObj;
    if(wmiSysImpleObj.getInstalledHotFixIds(osInfo.m_installedHotFixsList) != SVS_OK)
	{
		DebugPrintf(SV_LOG_ERROR, "getInstalledHotFixIds() failed.\n") ;		
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return retErrorSattus;
}

SVERROR GetSystemInformation( SystemInfo& sysInfo )
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    SVERROR  bRet = SVS_FALSE ;
    sysInfo.m_ErrorCode = DISCOVERY_SUCCESS;
	if( GetHostInfo(sysInfo.m_hostInfo) != SVS_OK )
    {
        DebugPrintf(SV_LOG_ERROR, "Failed to Get the Host Information\n") ;
        sysInfo.m_hostInfo.m_ErrorCode = DISCOVERY_FAIL;
        sysInfo.m_hostInfo.m_ErrorString = std::string("Failed to Get the Host Information");
    }
    else
    {
        DebugPrintf(SV_LOG_DEBUG, "Successfully got the Host Information\n") ;
        sysInfo.m_hostInfo.m_ErrorCode = DISCOVERY_SUCCESS;
        sysInfo.m_hostInfo.m_ErrorString = std::string("Successfully got the Host Information");
    }
    if( GetSystemBootInfo(sysInfo.m_OperatingSystemInfo) == SVS_OK )
    {
        DebugPrintf(SV_LOG_DEBUG, "Successfully got the Operating System Information\n") ;
        sysInfo.m_OperatingSystemInfo.m_ErrorCode = DISCOVERY_SUCCESS;
        sysInfo.m_OperatingSystemInfo.m_ErrorString = std::string("Successfully got the Operating System Information");
    }
    else
    {
		DebugPrintf(SV_LOG_ERROR, "Failed to Get the Operating System Information\n") ;
        sysInfo.m_OperatingSystemInfo.m_ErrorCode = DISCOVERY_FAIL;
        sysInfo.m_OperatingSystemInfo.m_ErrorString = std::string("Failed to Get the Operating System Information");
    }

    if( GetProcessorInfo(sysInfo.m_processorInfo) != SVS_OK )
    {
        DebugPrintf(SV_LOG_ERROR, "Failed to Get the Processor Information\n") ;
        sysInfo.m_processorInfo.m_ErrorCode = DISCOVERY_FAIL;
        sysInfo.m_processorInfo.m_ErrorString = std::string("Failed to Get the Processor Information");
    }
    else
    {
        DebugPrintf(SV_LOG_DEBUG, "Successfully got the Processor Information\n") ;
        sysInfo.m_processorInfo.m_ErrorCode = DISCOVERY_FAIL;
        sysInfo.m_processorInfo.m_ErrorString = std::string("Successfully got the Processor Information");
    }

	WMISysInfoImpl wmiSysImpleObj;
    if( wmiSysImpleObj.loadNetworkAdapterConfiguration( sysInfo.m_nwAdapterList ) != SVS_OK )
    {
        DebugPrintf(SV_LOG_ERROR, "Failed to Get the NetWork Adapters Information\n") ;
        
    }
    else
    {
        DebugPrintf(SV_LOG_DEBUG, "Successfully got the NetWork Adapters Information\n") ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}

SVERROR getHostLevelDiscoveryInfo(HostLevelDiscoveryInfo& info)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    SVERROR bRet = SVS_OK ;
    info.m_ErrorCode = DISCOVERY_FAIL;
    std::stringstream stream;
    bool bDiscovery = true;
    if(GetSystemInformation(info.m_sysInfo) != SVS_OK)
    {
        stream << "Failed to get System Informaion\n";
        bDiscovery = false;
        info.m_sysInfo.m_ErrorCode = DISCOVERY_FAIL;
        info.m_sysInfo.m_ErrorString = std::string("Failed to get System Informaion");
	}
	else
	{
		info.m_sysInfo.m_ErrorCode = DISCOVERY_SUCCESS;	
    }
    info.m_fwStatus = FIREWALL_STAT_ENABLED ;

	/*TagGenerator obj;
    obj.listVssProps(info.m_vssProviderList);
	//=> This function is removed from TagGenerator class. For detials visit class declarations.
	*/
	//Commenting list providers code as this information is not using anywhere.
	//listVssProviders(info.m_vssProviderList);

	info.isThisClusterNode = false;
	if(getClusterInfo( info.m_clusterInfo ) != SVS_OK)
    {
        stream << "Failed to get cluster Informaion\n";
        info.m_clusterInfo.m_ErrorCode = DISCOVERY_FAIL;
        info.m_clusterInfo.m_ErrorString = std::string("Failed to get cluster Informaion");
        bDiscovery = false;
    }
    else
    {
		info.isThisClusterNode = true;
		ClusterGroupOp clusGrpOp;
		if(clusGrpOp.GetClusGroupsInfo(info.m_clusterInfo.m_groupsInfo) != ERROR_SUCCESS)
		{
			stream << "Failed to get cluster groups Informaion\n";
			info.m_clusterInfo.m_ErrorCode = DISCOVERY_FAIL;
			info.m_clusterInfo.m_ErrorString = std::string("Failed to get cluster groups Informaion");
			bDiscovery = false;
		}
		else
			info.m_clusterInfo.m_ErrorCode = DISCOVERY_SUCCESS;
    }
	if(discoverHostLevelsvcInfo(info.m_svcList) != SVS_OK)
    {
        stream << "Failed to host level services information\n";
        bDiscovery = false;
    }
    if( bDiscovery)
    {
        info.m_ErrorCode = DISCOVERY_SUCCESS;
    }
	dumpHostLvelelDiscoveredInfo(info);
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}

SVERROR discoverHostLevelsvcInfo(std::list<InmService>& serviceList)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    SVERROR  bRet = SVS_OK ;
    InmService frSvc("frsvc") ;
    QuerySvcConfig(frSvc) ;
	serviceList.push_back(frSvc) ;
    InmService svagentsSvc("svagents") ;
    QuerySvcConfig(svagentsSvc) ;
    serviceList.push_back(svagentsSvc) ;
	InmService vssSvc("VSS");
	QuerySvcConfig(vssSvc);
	serviceList.push_back(vssSvc);
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}

std::list<std::pair<std::string, std::string> > HostInfo::getPropertyList()
{
    std::list<std::pair<std::string, std::string> > list ;
    list.push_back(std::make_pair("Host Name", this->m_hostName)) ;
    list.push_back(std::make_pair("Domain Name", this->m_domainName)) ;
    list.push_back(std::make_pair("User Name", this->m_userName)) ;
    return list ;
}

std::list<std::pair<std::string, std::string> > OsInfo::getPropertyList()
{
    std::list<std::pair<std::string, std::string> > list ;
    list.push_back(std::make_pair("CSD Version ", this->m_CSDVersion ) ) ;
	list.push_back(std::make_pair("System Drive ", this->m_SystemDrive)) ;
	list.push_back(std::make_pair("Windows Dir ", this->m_WindowsDirectory)) ;
	return list;
}

std::list<std::pair<std::string, std::string> > ProcessorInfo::getPropertyList()
{
	std::string processorArchitecture = "", processorType = "";
	std::list<std::pair<std::string, std::string> > list ;
    if( this->m_wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64 )
	{
		processorArchitecture = "x64 (AMD or Intel)";
	}
	else if( this->m_wProcessorArchitecture == PROCESSOR_ARCHITECTURE_IA64 )
	{
		processorArchitecture = "Intel Itanium Processor Family (IPF)";
	}
	else if( this->m_wProcessorArchitecture == PROCESSOR_ARCHITECTURE_INTEL )
	{
		processorArchitecture = "x86";
	}
	else if( this->m_wProcessorArchitecture == PROCESSOR_ARCHITECTURE_UNKNOWN )
	{
		processorArchitecture = "Unknown architecture";
	}
	
	if( this->m_dwProcessorType == PROCESSOR_INTEL_386 )
	{
		processorType = "Intel 386";
	}
	else if( this->m_dwProcessorType == PROCESSOR_INTEL_486 )
	{
		processorType = "Intel 486";
	}
	else if( this->m_dwProcessorType == PROCESSOR_INTEL_PENTIUM )
	{
		processorType = "Intel Pentium";
	}
	else if( this->m_dwProcessorType == PROCESSOR_INTEL_IA64 )
	{
		processorType = "Intel IA64";
	}
	else if( this->m_dwProcessorType == PROCESSOR_AMD_X8664 )
	{
		processorType = "AMD X8664";
	}
	list.push_back(std::make_pair("Processor Architecture ", processorArchitecture)) ;
	list.push_back(std::make_pair("Processor Type ", processorType)) ;
    return list ;
}

std::string HostLevelDiscoveryInfo::summary(std::list<ValidatorPtr> rules)
{
	std::list<ValidatorPtr> list ;
	std::stringstream stream ;
    stream << "Host Discovery Information\n\n" ;
	stream << "\tHost Name\t\t\t: " << m_sysInfo.m_hostInfo.m_hostName <<std::endl;
    stream << "\tIP Address\t\t\t: " << m_sysInfo.m_hostInfo.m_ipaddress <<std::endl;
	stream << "\tDomain Name\t\t\t: " << m_sysInfo.m_hostInfo.m_domainName <<std::endl;
    stream << "\tService Pack\t\t\t: " << m_sysInfo.m_OperatingSystemInfo.m_CSDVersion << std::endl ;
    stream << "\tSystem Drive\t\t\t: " << m_sysInfo.m_OperatingSystemInfo.m_SystemDrive << std::endl ;
    stream << "\tSystem Network Details " << std::endl;
    std::list<NetworkAdapterConfig>::iterator nwAdaptorListIter =  m_sysInfo.m_nwAdapterList.begin();
    while(nwAdaptorListIter != m_sysInfo.m_nwAdapterList.end())
    {
        stream << "\t\tMac Address\t\t: " << nwAdaptorListIter->m_macAddress << std::endl;
        stream << "\t\tIP Address(es) \t\t " << std::endl;
        for( unsigned long i=0; i < nwAdaptorListIter->m_no_ipsConfigured; i++ )
        {
            stream << "\t\t\t\t\t: " << nwAdaptorListIter->m_ipaddress[i] << std::endl;
        }
        if(nwAdaptorListIter->m_dhcpEnabled)
        {
            stream << "\t\tDHCP       \t\t: " << "ENABLED" << std::endl;
        }
        else
        {
            stream << "\t\tDHCP       \t\t: " << "NOT ENABLED" << std::endl;
        }
        if(nwAdaptorListIter->m_fullDNSRegistrationEnabled)
        {
            stream << "\t\tFull DNS Registration\t: " << "ENABLED" << std::endl;
        }
        else
        {
            stream << "\t\tFull DNS Registration\t: " << "NOT ENABLED" << std::endl;
        }
        stream << std::endl;
        nwAdaptorListIter++;
    }
    stream << "\n\tOperating System Details " << std::endl;
    std::list<std::pair<std::string, std::string> > osinfopairList = m_sysInfo.m_OperatingSystemInfo.getPropertyList();
    std::list<std::pair<std::string, std::string> >::iterator osinfopairListIter = osinfopairList.begin();
    while( osinfopairListIter != osinfopairList.end() )
    {
        stream << "\t\t " << osinfopairListIter->first << "\t\t: " << osinfopairListIter->second << std::endl;
        osinfopairListIter++;
    }

    stream << "\n\tProcessor Details " << std::endl;
    std::list<std::pair<std::string, std::string> > processorpairList = m_sysInfo.m_processorInfo.getPropertyList();
    std::list<std::pair<std::string, std::string> >::iterator processorpairListIter = processorpairList.begin();
    while( processorpairListIter != processorpairList.end() )
    {
        stream << "\t\t " << processorpairListIter->first << "\t\t: " << processorpairListIter->second << std::endl;
        processorpairListIter++;
    }

    std::list<VssProviderProperties>::iterator vssIter = this->m_vssProviderList.begin() ;
	stream << "\n\tHost Services Information\n";
	std::list<InmService>::iterator svcIter = this->m_svcList.begin();
	while(svcIter != this->m_svcList.end())
	{
        std::list<std::pair<std::string, std::string> > properties = svcIter->getProperties() ;
        std::list<std::pair<std::string, std::string> >::iterator propIter = properties.begin() ;
        while( propIter != properties.end() )
        {
            stream << "\t\t" << propIter->first << "\t\t\t:" <<propIter->second <<std::endl;
            propIter++ ;
        }
        stream << std::endl ;
		svcIter++;
	}
    
    std::list<ValidatorPtr>::const_iterator iter = rules.begin() ;
    stream << "\n\n\tPre-Requisites Validation Results" << std::endl <<std::endl ;
	while (iter != rules.end() )
	{
		std::string RuleId = Validator::getInmRuleIdStringFromID((*iter)->getRuleId() );
		stream << "\t\tRule Name\t\t:" << RuleId << "\n";
        stream << "\t\tResult\t\t\t:" << (*iter)->statusToString() << std::endl ;
		iter++ ;
	}

    return stream.str() ;
}

void dumpHostLvelelDiscoveredInfo(HostLevelDiscoveryInfo info)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	if( info.isThisClusterNode )
	{
		DebugPrintf(SV_LOG_DEBUG, "\n");
		DebugPrintf(SV_LOG_DEBUG, "Cluster Name: %s\n", info.m_clusterInfo.m_clusterName.c_str());
		DebugPrintf(SV_LOG_DEBUG, "Node Name: %s\n\n", info.m_clusterInfo.m_nodeName.c_str());
		DebugPrintf(SV_LOG_DEBUG, "Available Network Name Resources details..\n\n");
		std::map<std::string, NetworkResourceInfo>::iterator resourceInfoIter = info.m_clusterInfo.m_networkInfoMap.begin();
		while( resourceInfoIter != info.m_clusterInfo.m_networkInfoMap.end() )
		{
			DebugPrintf(SV_LOG_DEBUG, "Network Name: %s\n", resourceInfoIter->first.c_str());
			DebugPrintf(SV_LOG_DEBUG, "State: %d\n", resourceInfoIter->second.m_resInfo.m_resourceStatus );
			DebugPrintf(SV_LOG_DEBUG, "Group Name: %s\n", resourceInfoIter->second.m_resInfo.m_ownerGroup.c_str() );
			DebugPrintf(SV_LOG_DEBUG, "Owner Name:  %s\n", resourceInfoIter->second.m_resInfo.m_ownerNodeName.c_str() );
			std::list<std::string> dependentIps = resourceInfoIter->second.m_dependendentIpList ;
			DebugPrintf(SV_LOG_DEBUG, "Size %d\n", dependentIps.size()) ;
			std::list<std::string>::iterator dependendentIpListIter = dependentIps.begin();
			while( dependendentIpListIter != dependentIps.end())
			{
				DebugPrintf(SV_LOG_DEBUG, "Depended Ip Address:  %s\n", (*dependendentIpListIter).c_str() );
				dependendentIpListIter++;
			}
			resourceInfoIter++;
		}
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}

void HostLevelDiscoveryInfo::updateToDiscoveryInfo(DiscoveryInfo& discoveryInfo)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	SystemInfo sysInfo = m_sysInfo ;
    std::list<NetworkAdapterConfig> nwAdapterConfigList = sysInfo.m_nwAdapterList ;
    
    //Network adapters information
	if(discoveryInfo.m_nwInfo.size() <= 0)
	{
		std::list<NetworkAdapterConfig>::const_iterator nwAdapterIter = nwAdapterConfigList.begin() ;
		while( nwAdapterIter != nwAdapterConfigList.end() )
		{
			DebugPrintf(SV_LOG_DEBUG, "adding nic information\n") ;
			NICInformation nicInfo ;
			std::string macAddress ;
			nicInfo.m_nicProperties.insert(std::make_pair("MACAddress", nwAdapterIter->m_macAddress)) ;
			for( unsigned long i = 0 ; i < nwAdapterIter->m_no_ipsConfigured ; i++ )
			{
				nicInfo.m_IpAddresses.push_back(nwAdapterIter->m_ipaddress[i]) ;
			}
            nicInfo.m_nicProperties.insert(std::make_pair("ErrorCode",boost::lexical_cast<std::string>(nwAdapterIter->m_ErrorCode))) ;
            nicInfo.m_nicProperties.insert(std::make_pair("ErrorString",nwAdapterIter->m_ErrorString)) ;
            discoveryInfo.m_nwInfo.push_back(nicInfo) ;
			nwAdapterIter++ ;
		}
	}
    //Cluster Information
	ClusInformation clusInfo ;
	std::string clusterName = m_clusterInfo.m_clusterName ;
    std::string clusterId = m_clusterInfo.m_clusterID ;
	clusInfo.m_clusProperties.insert(std::make_pair("ClusterName", clusterName )) ;
	clusInfo.m_clusProperties.insert(std::make_pair("ClusterId", clusterId)) ; 
	clusInfo.m_clusProperties.insert(std::make_pair("QuorumType", m_clusterInfo.m_quorumInfo.type)) ; 
	if(!m_clusterInfo.m_quorumInfo.resource.empty())
		clusInfo.m_clusProperties.insert(std::make_pair("QuorumResource", m_clusterInfo.m_quorumInfo.resource)) ;
	if(!m_clusterInfo.m_quorumInfo.path.empty())
		clusInfo.m_clusProperties.insert(std::make_pair("QuorumPath", m_clusterInfo.m_quorumInfo.path)) ;
    std::string ErrorCode = boost::lexical_cast<std::string>(m_clusterInfo.m_ErrorCode);
    std::string ErrorStr = m_clusterInfo.m_ErrorString ;
	clusInfo.m_nodes.assign(m_clusterInfo.m_nodes.begin(),m_clusterInfo.m_nodes.end());
    if( clusterName.compare("") == 0 || 
        clusterId.compare("") == 0 ||
        m_clusterInfo.m_networkInfoMap.size() == 0 ||
		m_clusterInfo.m_nodes.size() == 0)
    {
        ErrorCode = "-1" ;
        ErrorStr = "The Cluster Information is not complete/unavailable" ;
    }
    clusInfo.m_clusProperties.insert(std::make_pair("ErrorCode", ErrorCode)) ;     
    clusInfo.m_clusProperties.insert(std::make_pair("ErrorString", ErrorStr)) ;

	std::map<std::string, NetworkResourceInfo> clusNwResourceMap = m_clusterInfo.m_networkInfoMap ;
	std::map<std::string, NetworkResourceInfo>::const_iterator clusNwResourceIter = clusNwResourceMap.begin() ;
	while( clusNwResourceIter != clusNwResourceMap.end() )
	{
		ClusterNwResourceInfo nwResourceInfo ;
		nwResourceInfo.m_resourceProperties.insert(std::make_pair("VirtualServerName", clusNwResourceIter->first)) ;
		nwResourceInfo.m_resourceProperties.insert(std::make_pair("OwnerNode", clusNwResourceIter->second.m_resInfo.m_ownerNodeName)) ;
		nwResourceInfo.m_resourceProperties.insert(std::make_pair("OwnerNode", clusNwResourceIter->second.m_resInfo.m_ownerNodeName)) ;
		std::string resourceStatus = boost::lexical_cast<std::string>(clusNwResourceIter->second.m_resInfo.m_resourceStatus) ;
		nwResourceInfo.m_resourceProperties.insert(std::make_pair("ResourceCurrentState", clusNwResourceIter->second.m_resInfo.m_ownerNodeName)) ;
		std::list<std::string> dependentIpsList = clusNwResourceIter->second.m_dependendentIpList ;
		std::list<std::string>::const_iterator ipIter = dependentIpsList.begin() ;
		while( ipIter != dependentIpsList.end() )
		{
			nwResourceInfo.m_ipsList.push_back(*ipIter) ;
			ipIter ++ ;
		}
		clusInfo.m_nwNameInfoList.push_back( nwResourceInfo ) ;
		clusNwResourceIter++ ;
	}
	if(m_clusterInfo.m_groupsInfo.size() > 0)
	{
		for(size_t iGroup=0; iGroup<m_clusterInfo.m_groupsInfo.size(); iGroup++)
		{
			ClusGroupInfo grpInfo;
			grpInfo.m_GrpProps["Id"] =  m_clusterInfo.m_groupsInfo[iGroup].id;
			grpInfo.m_GrpProps["Name"] = m_clusterInfo.m_groupsInfo[iGroup].name;
			grpInfo.m_GrpProps["OwnerNode"] = m_clusterInfo.m_groupsInfo[iGroup].ownerNode;
			grpInfo.m_GrpProps["State"] = ClusterGroupStateMsg(m_clusterInfo.m_groupsInfo[iGroup].state);
			grpInfo.m_GrpProps["ErrorString"] = 
				m_clusterInfo.m_groupsInfo[iGroup].errorMsg.empty() ? "Success" : m_clusterInfo.m_groupsInfo[iGroup].errorMsg;

			std::vector<std::string>::const_iterator begin_iter = m_clusterInfo.m_groupsInfo[iGroup].prefOwners.begin(),
				end_iter = m_clusterInfo.m_groupsInfo[iGroup].prefOwners.end();
			for( ;begin_iter != end_iter; begin_iter++)
				grpInfo.m_prefOwners.push_back(*begin_iter);
			
			for(size_t iRes=0; iRes < m_clusterInfo.m_groupsInfo[iGroup].resources.size(); iRes++)
			{
				ClusResourceInfo resInfo;
				resInfo.m_ResourceProps["Id"] = m_clusterInfo.m_groupsInfo[iGroup].resources[iRes].id;
				resInfo.m_ResourceProps["Name"] = m_clusterInfo.m_groupsInfo[iGroup].resources[iRes].name;
				resInfo.m_ResourceProps["Type"] = m_clusterInfo.m_groupsInfo[iGroup].resources[iRes].type;
				resInfo.m_ResourceProps["ErrorString"] = 
					m_clusterInfo.m_groupsInfo[iGroup].resources[iRes].errorMsg.empty() ? "Success": m_clusterInfo.m_groupsInfo[iGroup].resources[iRes].errorMsg;
				
				resInfo.m_ResourceProps["State"] = ClusterResourceStateMsg(
					m_clusterInfo.m_groupsInfo[iGroup].resources[iRes].state);
				
				resInfo.m_ResourceProps.insert(m_clusterInfo.m_groupsInfo[iGroup].resources[iRes].otherProp.begin(),
					m_clusterInfo.m_groupsInfo[iGroup].resources[iRes].otherProp.end());
				
				begin_iter = m_clusterInfo.m_groupsInfo[iGroup].resources[iRes].dependencies.begin();
				end_iter = m_clusterInfo.m_groupsInfo[iGroup].resources[iRes].dependencies.end();
				for( ;begin_iter != end_iter; begin_iter++)
					resInfo.m_Dependencies.push_back(*begin_iter);

				grpInfo.m_Resources[m_clusterInfo.m_groupsInfo[iGroup].resources[iRes].name] = resInfo;
			}
			clusInfo.m_ClusGroups[m_clusterInfo.m_groupsInfo[iGroup].name] = grpInfo; 
		}
	}
	discoveryInfo.ClusterInformation = clusInfo ;
	if(discoveryInfo.m_vssProviders.size() <= 0)
	{
		std::list<VssProviderProperties> vssProvidersList = m_vssProviderList ;
		std::list<VssProviderProperties>::iterator vssIter = vssProvidersList.begin() ;
		while( vssIter != vssProvidersList.end() )
		{
			VSSProviderDetails vssProviderDetails ;
			std::list<std::pair<std::string, std::string> > propertiesList = vssIter->getPropList() ;
			std::list<std::pair<std::string, std::string> >::const_iterator propertiesIter = propertiesList.begin() ;
			while( propertiesIter != propertiesList.end() )
			{
				if(propertiesIter->first.compare("Name") == 0 )
				{
					vssProviderDetails.m_providerProperties.insert(std::make_pair("ProviderName", propertiesIter->second)) ;
				}
				else if(propertiesIter->first.compare("GUID") == 0 )
				{
					vssProviderDetails.m_providerProperties.insert(std::make_pair("ProviderGuid", propertiesIter->second)) ;
				}
				else if(propertiesIter->first.compare("Type") == 0 )
				{
					vssProviderDetails.m_providerProperties.insert(std::make_pair("ProviderType", propertiesIter->second)) ;
				}
				propertiesIter++ ;
			}
			discoveryInfo.m_vssProviders.push_back(vssProviderDetails) ;
			vssIter++ ;
	    }
        discoveryInfo.m_properties.insert(std::make_pair("DomainName", m_sysInfo.m_hostInfo.m_domainName));
	}
   DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}