#include "appglobals.h"
#include <sstream>
#include <boost/algorithm/string.hpp>
#include "host.h"
#include "exchangediscovery.h"
#include "service.h"

boost::shared_ptr<ExchangeDiscoveryImpl> ExchangeDiscoveryImpl::m_instancePtr ;

boost::shared_ptr<ExchangeDiscoveryImpl> ExchangeDiscoveryImpl::getInstance()
{
    if( m_instancePtr.get() == NULL )
    {
        m_instancePtr.reset(new ExchangeDiscoveryImpl()) ;
    }
    return m_instancePtr ;
}

SVERROR ExchangeDiscoveryImpl::init(const std::string& dnsname)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    SVERROR bRet = SVS_FALSE ;
    m_dnsname = dnsname ;
    if( m_ldapHelper.get() == NULL )
    {
        m_ldapHelper.reset(new ExchangeLdapHelper() ) ;
        if( m_ldapHelper->init(m_dnsname) == SVS_OK )
        {
            bRet = SVS_OK ;
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR, "ExchangeLdapHelper::init Failed\n") ;
            m_ldapHelper.reset() ;
        }
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}

//SVERROR ExchangeDiscoveryImpl::discoverExchVersionServices(std::list<InmService>& serviceList)
//{
//    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
//    SVERROR bRet = SVS_OK ;
//    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
//    return bRet ;
//}
SVERROR ExchangeDiscoveryImpl::discoverExchangeVersionInfo(ExchAppVersionDiscInfo & exchVersionInfo)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    SVERROR bRet = SVS_FALSE ;
    exchVersionInfo.m_errCode = DISCOVERY_FAIL;
    std::stringstream stream;
    std::multimap<std::string, std::string> attrMap ;

	try
	{
		if( m_ldapHelper->getExchangeServerAttributes(m_hostname, attrMap) == SVS_OK )
		{
			DebugPrintf(SV_LOG_DEBUG, "AD queries are for entry %s\n", m_hostname.c_str()) ;
			DebugPrintf(SV_LOG_DEBUG, "HostName %s\n", Host::GetInstance().GetHostName().c_str()) ;
	        
			if( strcmpi(Host::GetInstance().GetHostName().c_str(),m_hostname.c_str()) == 0 )
			{
				exchVersionInfo.m_isClustered  = false ;
			}
			else
			{
				exchVersionInfo.m_isClustered  = true ;
			}

			bRet = SVS_OK ;
			exchVersionInfo.m_errCode = DISCOVERY_SUCCESS;
			if(attrMap.find("msExchDataPath") != attrMap.end())
			{
				exchVersionInfo.m_dataPath = attrMap.find("msExchDataPath")->second;
			}
			if(attrMap.find("msExchInstallPath") != attrMap.end())
			{
				exchVersionInfo.m_installPath = attrMap.find("msExchInstallPath")->second;
			}
			if(attrMap.find("serialNumber") != attrMap.end())
			{
				exchVersionInfo.m_version = attrMap.find("serialNumber")->second ;
			}
			if( exchVersionInfo.m_version.find("Version 6.5") != std::string::npos )
			{
				exchVersionInfo.m_appType = INM_APP_EXCH2003;
			}
			else if( exchVersionInfo.m_version.find("Version 8.") != std::string::npos )
			{
				exchVersionInfo.m_appType = INM_APP_EXCH2007;
			}
			else if( exchVersionInfo.m_version.find("Version 14.") != std::string::npos )
			{
				exchVersionInfo.m_appType = INM_APP_EXCH2010;
			}
			else
			{
				exchVersionInfo.m_appType = INM_APP_EXCHNAGE;
			}
			if(attrMap.find("cn") != attrMap.end())
			{
				exchVersionInfo.m_cn = attrMap.find("cn")->second ;
			}
			boost::algorithm::to_upper(exchVersionInfo.m_cn) ;
			if(attrMap.find("distinguishedName") != attrMap.end())
			{
				exchVersionInfo.m_dn = attrMap.find("distinguishedName")->second ;
			}
			exchVersionInfo.m_adminstrativeGrpName = "Dummy Admin Grop Name" ;
			exchVersionInfo.m_organizationName = "Dummy Organization Name" ;
			if(m_ldapHelper->getServerMaxMailstores(exchVersionInfo.m_dn,exchVersionInfo.m_nMaxMailStores) == SVS_OK)
				exchVersionInfo.m_edition = m_ldapHelper->getEdition(exchVersionInfo.m_nMaxMailStores,exchVersionInfo.m_appType);
			if( attrMap.find("msExchELCAuditLogPath") != attrMap.end() )
			{
				exchVersionInfo.m_auditLogPath = attrMap.find("msExchELCAuditLogPath") ->second;
			}
			if( attrMap.find("msExchTransportPipelineTracingPath") != attrMap.end() )
			{
				exchVersionInfo.m_pipelineTracingPath = attrMap.find("msExchTransportPipelineTracingPath")->second ;
			}
			if( attrMap.find("msExchTransportConnectivityLogPath") != attrMap.end() )
			{
				exchVersionInfo.m_transportConnectivityLogPath = attrMap.find("msExchTransportConnectivityLogPath")->second ;
			}
			if( attrMap.find("msExchTransportMessageTrackingPath") != attrMap.end() )
			{
				exchVersionInfo.m_transportMsgTrackingPath = attrMap.find("msExchTransportMessageTrackingPath")->second ;
			}
			if( attrMap.find("msExchTransportPickupDirectoryPath") != attrMap.end() )
			{
				exchVersionInfo.m_transportPickupDirPath = attrMap.find("msExchTransportPickupDirectoryPath")->second ;
			}
			if( attrMap.find("msExchTransportReceiveProtocolLogPath") != attrMap.end() )
			{
				exchVersionInfo.m_transportRecvProtocolLogPath = attrMap.find("msExchTransportReceiveProtocolLogPath")->second ;
			}
			if( attrMap.find("msExchTransportReplayDirectoryPath") != attrMap.end() )
			{
				exchVersionInfo.m_transportReplyDirPath = attrMap.find("msExchTransportReplayDirectoryPath")->second ;
			}
			if( attrMap.find("msExchTransportRoutingLogPath") != attrMap.end() )
			{
				exchVersionInfo.m_transportRoutingLogPath = attrMap.find("msExchTransportRoutingLogPath")->second ;
			}
			if( attrMap.find("msExchTransportSendProtocolLogPath") != attrMap.end() )
			{
				exchVersionInfo.m_transportSendProtocolLogPath = attrMap.find("msExchTransportSendProtocolLogPath")->second ;
			}
			exchVersionInfo.m_serverCurrentRoles = 0 ;
			if( attrMap.find("msExchCurrentServerRoles") != attrMap.end()  )
			{            
				exchVersionInfo.m_serverCurrentRoles = atoi(attrMap.find("msExchCurrentServerRoles")->second.c_str()) ;
			}
	        
			exchVersionInfo.m_serverRole = 0 ;    
			if( attrMap.find("msExchServerRole") != attrMap.end()  )
			{
				exchVersionInfo.m_serverRole = atoi(attrMap.find("msExchServerRole")->second.c_str()) ;
			}
			exchVersionInfo.m_isMta = false ;
			if( attrMap.find("msExchResponsibleMTAServer") != attrMap.end()  )
			{
				std::string msExchResponsibleMTAServerStr = attrMap.find("msExchResponsibleMTAServer")->second ;
				boost::algorithm::to_upper(msExchResponsibleMTAServerStr) ;
				if( msExchResponsibleMTAServerStr.find(exchVersionInfo.m_cn) != std::string::npos)
				{
					exchVersionInfo.m_isMta = true ;
					DebugPrintf(SV_LOG_DEBUG, "%s is a MTA\n", exchVersionInfo.m_cn.c_str()) ;
				}
				else
				{
					exchVersionInfo.m_isMta = false ;
					DebugPrintf(SV_LOG_DEBUG, "%s is a Non-MTA\n", exchVersionInfo.m_cn.c_str()) ;
				}
			}
			exchVersionInfo.m_clusterStorageType = 0 ;
			if( attrMap.find("msExchClusterStorageType") != attrMap.end()  )
			{
				exchVersionInfo.m_clusterStorageType = atoi(attrMap.find("msExchClusterStorageType")->second.c_str()) ;
			}
			//if( discoverExchVersionServices(exchVersionInfo.m_services) == SVS_OK )
			//{
			//	DebugPrintf(SV_LOG_DEBUG, "ExchangeDiscoveryImpl::discoverExchVersionServices Successful\n") ;                
			//}
			//else
			//{
			//	DebugPrintf(SV_LOG_ERROR, "ExchangeDiscoveryImpl::discoverExchVersionServices Failed\n") ;
			//}  No use ...
			stream << "Successfully discovered :"<<std::endl;
			stream << "Exchange hostname :"<<m_hostname;
			stream << "Exchange version:"<<exchVersionInfo.m_version;
			stream << "Exchange Edition:"<<exchVersionInfo.m_edition;
		}
		else
		{
			DebugPrintf(SV_LOG_ERROR, "ExchangeLdapHelper::getExchangeServerAttributes Failed\n") ;
			stream << "Failed to get exchange server atrributes";
			bRet = SVS_FALSE ;
		}
	}
	catch(...)
	{
		DebugPrintf(SV_LOG_ERROR, "Got Exception: ExchangeLdapHelper::getExchangeServerAttributes failed\n") ;
		stream << "Failed to get exchange server atrributes due to an unhandled exception" ;
		bRet = SVS_FALSE ;
	}
    exchVersionInfo.m_errString = stream.str();
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}

SVERROR ExchangeDiscoveryImpl::discoverExchAppLevelServices(std::list<InmService>& serviceList, InmProtectedAppType appType)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	SVERROR bRet = SVS_FALSE ;
	if(appType == INM_APP_EXCH2003)
	{
		//Exchange 2003 Specific Services..
		InmService MSExchangeISSvc("MSExchangeIS") ;
		QuerySvcConfig(MSExchangeISSvc) ;
		serviceList.push_back(MSExchangeISSvc) ;

		InmService MSExchangeSASvc("MSExchangeSA") ;
		QuerySvcConfig(MSExchangeSASvc) ;
		serviceList.push_back(MSExchangeSASvc) ;

		InmService MSExchangeMGMTSvc("MSExchangeMGMT") ;
		QuerySvcConfig(MSExchangeMGMTSvc) ;
		serviceList.push_back(MSExchangeMGMTSvc) ;

		InmService RESvc("RESvc") ;
		QuerySvcConfig(RESvc) ;
		serviceList.push_back(RESvc) ;

		InmService MSExchangeMTASvc("MSExchangeMTA") ;
		QuerySvcConfig(MSExchangeMTASvc) ;
		serviceList.push_back(MSExchangeMTASvc) ;

		InmService MSExchangeESSvc("MSExchangeES") ;
		QuerySvcConfig(MSExchangeESSvc) ;
		serviceList.push_back(MSExchangeESSvc) ;
	}
	else if(appType == INM_APP_EXCH2007)
	{
		InmService MSExchangeISSvc("MSExchangeIS") ;
		QuerySvcConfig(MSExchangeISSvc) ;
		serviceList.push_back(MSExchangeISSvc) ;
    
		InmService MSExchangeSASvc("MSExchangeSA") ;
		QuerySvcConfig(MSExchangeSASvc) ;
		serviceList.push_back(MSExchangeSASvc) ;		
	}
	else if(appType == INM_APP_EXCH2010)
	{
		//Exchange 2010 Specific Services.. We Do not have any specific services to start and stop while fail over/ fail back.
	}
    bRet = SVS_OK ;
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}

bool ExchangeDiscoveryImpl::isInstalled(std::list<std::string>& exchHosts)
{
    if(  m_ldapHelper->isExchangeInstalled(exchHosts) )
    {
        DebugPrintf(SV_LOG_DEBUG, "Exchange is installed on %d\n", exchHosts.size()) ;
        return true ;
    }
    else
    {
        DebugPrintf(SV_LOG_DEBUG, "Exchange is not installed in domain\n") ;
        return false ;
    }

}
bool ExchangeDiscoveryImpl::isInstalled(const std::string& exchHost)
{
    if(  m_ldapHelper->isExchangeInstalled(exchHost) )
    {
        DebugPrintf(SV_LOG_DEBUG, "Exchange is installed on %s\n", exchHost.c_str()) ;
        return true ;
    }
    else
    {
        DebugPrintf(SV_LOG_DEBUG, "Exchange is not installed on %s\n", exchHost.c_str()) ;
        return false ;
    }
}
SVERROR ExchangeDiscoveryImpl::discoverExchangeApplication(const std::string& exchHost, ExchAppVersionDiscInfo& exchDiscInfo, bool bDiscSvs)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    SVERROR bRet = SVS_FALSE ;
    m_hostname = exchHost ;

    if(  m_ldapHelper->isExchangeInstalled(m_hostname) )
    {
        if(discoverExchangeVersionInfo(exchDiscInfo) == SVS_OK)
			bRet = SVS_OK ;
    }
    if( exchDiscInfo.m_cn.compare("") != 0 )
    {
        InmServiceStatus status ;
        bool discoveryFail = false ;
        if( isServiceInstalled("MSExchangeIS",  status ) == SVS_OK && status == INM_SVCSTAT_INSTALLED )
        {
            exchDiscInfo.m_errCode = DISCOVERY_SUCCESS ;
        }
    }
    else
    {
        exchDiscInfo.m_cn = exchHost ;
        exchDiscInfo.m_errCode = DISCOVERY_FAIL ;
        exchDiscInfo.m_appType = INM_APP_EXCHNAGE ;
    }
	if( bDiscSvs )
	{
		if( discoverExchAppLevelServices(exchDiscInfo.m_services, exchDiscInfo.m_appType) == SVS_OK )
		{
			DebugPrintf(SV_LOG_DEBUG, "ExchangeDiscoveryImpl::discoverExchAppLevelServices Successuful\n") ;
		}
		else
		{
			DebugPrintf(SV_LOG_ERROR, "ExchangeDiscoveryImpl::discoverExchAppLevelServices Failed\n") ;
		}
	}
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}

void ExchangeDiscoveryImpl::Display(const ExchAppVersionDiscInfo& appInfo)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    DebugPrintf(SV_LOG_DEBUG, "AuditLogPath %s\n", appInfo.m_auditLogPath.c_str());
    DebugPrintf(SV_LOG_DEBUG, "DataPath %s\n", appInfo.m_dataPath.c_str());
    DebugPrintf(SV_LOG_DEBUG, "Install Path %s\n", appInfo.m_installPath.c_str());
    DebugPrintf(SV_LOG_DEBUG, "PipeLineTracingPath %s\n",appInfo.m_pipelineTracingPath.c_str());
    DebugPrintf(SV_LOG_DEBUG, "TransportConnectivityLogPath %s\n", appInfo.m_transportConnectivityLogPath.c_str());
    DebugPrintf(SV_LOG_DEBUG, "TransportMsgTrackingPath %s\n", appInfo.m_transportMsgTrackingPath.c_str());
    DebugPrintf(SV_LOG_DEBUG, "TransportPickupPath %s\n", appInfo.m_transportPickupDirPath.c_str());
    DebugPrintf(SV_LOG_DEBUG, "Transport Recieve Protocol Log Path %s\n", appInfo.m_transportRecvProtocolLogPath.c_str());
    DebugPrintf(SV_LOG_DEBUG, "TransportReplayDirPath %s\n", appInfo.m_transportReplyDirPath.c_str());
    DebugPrintf(SV_LOG_DEBUG, "TransportRoutingLogPath %s\n", appInfo.m_transportRoutingLogPath.c_str());
    DebugPrintf(SV_LOG_DEBUG, "TransportSendProtocolLogPath %s\n", appInfo.m_transportSendProtocolLogPath.c_str());
    DebugPrintf(SV_LOG_DEBUG, "Version %s\n", appInfo.m_version.c_str());
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}

void ExchangeDiscoveryImpl::SetHostName(const std::string& hostname)
{
    m_hostname = hostname ;
}

std::list<std::pair<std::string, std::string> > ExchAppVersionDiscInfo::getPropertyList()
{
    std::list<std::pair<std::string, std::string> > propertyList ;
    propertyList.push_back(std::make_pair("cn", m_cn)) ;
    propertyList.push_back(std::make_pair("Exchange Version", this->m_version)) ;
	propertyList.push_back(std::make_pair("Exchange Edition", this->m_edition)) ;
    propertyList.push_back(std::make_pair("Exchange Data Path", this->m_dataPath)) ;
    propertyList.push_back(std::make_pair("Exchange Install Path", this->m_installPath)) ;
    propertyList.push_back(std::make_pair("Server Role", this->ServerRoleToString())) ;
	propertyList.push_back(std::make_pair("Maximum Allowed MailStores", this->m_nMaxMailStores));
    
    if( this->m_auditLogPath.compare("") != 0 )
    {
        propertyList.push_back(std::make_pair("Audit Log Path", m_auditLogPath)) ;
    }
    if( this->m_transportConnectivityLogPath.compare("") != 0 )
    {
        propertyList.push_back(std::make_pair("Transport Connectivity Log Path", this->m_transportConnectivityLogPath)) ;
    }
    if( this->m_transportMsgTrackingPath.compare("") != 0 )
    {
        propertyList.push_back(std::make_pair("Transport Message Tracking Path",this->m_transportMsgTrackingPath)) ;
    }
    if( this->m_transportPickupDirPath.compare("") != 0 )
    {
        propertyList.push_back(std::make_pair("Transpor Pickup Directory Path", this->m_transportPickupDirPath)) ;
    }
    if( this->m_transportRecvProtocolLogPath.compare("") != 0 )
    {
        propertyList.push_back(std::make_pair("Transport Recieve Protocol Log Path", this->m_transportRecvProtocolLogPath)) ;
    }
    if( this->m_transportReplyDirPath.compare("") != 0 )
    {
        propertyList.push_back(std::make_pair("Transport Replay Directory Path", this->m_transportReplyDirPath)) ;
    }
    if( this->m_transportRoutingLogPath.compare("") != 0 )
    {
        propertyList.push_back(std::make_pair("Transport Routing Log Path", this->m_transportRoutingLogPath)) ;
    }
    if( this->m_transportSendProtocolLogPath.compare("") != 0 )
    {
        propertyList.push_back(std::make_pair("Transport Send Protocol Log Path", this->m_transportSendProtocolLogPath)) ;
    }
    if( this->m_clusterStorageType != 0 )
    {
        propertyList.push_back(std::make_pair("Cluster Storage Type", ClusterStorageTypeToStr())) ;
    }
    propertyList.push_back(std::make_pair("Server Current Roles", ServerCurrentRoleToString())) ;

    return propertyList ;
}

std::string ExchApplDiscoveryInfo::summary()
{
    std::list<ExchAppVersionDiscInfo>::iterator ExchAppVersionDiscInfoIter = m_exchAppVerDiscInfoList.begin() ;
    std::stringstream stream ;
    if( this->m_exchAppVerDiscInfoList.size() != 0 )
    {
        while( ExchAppVersionDiscInfoIter != m_exchAppVerDiscInfoList.end() )
        {
            stream << ExchAppVersionDiscInfoIter->summary() <<std::endl ;
            ExchAppVersionDiscInfoIter++ ;
        }
    }
    return stream.str() ;
}

std::string ExchAppVersionDiscInfo::summary()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    std::stringstream stream ;
    if( getPropertyList().size() != 0 )
    {
        stream << "\n\tMicrosoft Exchange Server Discovery Information\n\n" ;
        std::list<std::pair<std::string, std::string> > propertyList = getPropertyList() ;
        std::list<std::pair<std::string, std::string> >::const_iterator propertyIter = propertyList.begin() ;
        while( propertyIter != propertyList.end() )
        {
            stream << "\t\t " << propertyIter->first << "\t\t : " << propertyIter->second <<std::endl ;
            propertyIter++ ;
        }
    }
    stream <<std::endl ;
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return stream.str() ;
}

std::string ExchAppVersionDiscInfo::ServerCurrentRoleToString() const
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    std::string roleString = "" ;
    if( m_serverCurrentRoles & 2 )
    {
            roleString = "Mailbox role ; " ;
    }
    if( m_serverCurrentRoles & 4 )
    {
            roleString += "Client Access role (CAS) ; " ;
    }
    if( m_serverCurrentRoles & 16 )
    {
            roleString += "Unified Messaging role ; " ;
    }
    if( m_serverCurrentRoles & 32 )
    {
            roleString += "Hub Transport role ; " ;
    }
    if( m_serverCurrentRoles & 64 )
    {
            roleString += "Edge Transport role ; " ;
    }
    if( m_isMta )
    {
        roleString += "MTA Server ; " ;
    }
    else
    {
        roleString += "Non-MTA Server ; " ;
    }
    if( roleString.compare("") != 0 )
    {
        roleString.erase(roleString.length() - 3) ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return roleString ;
}


std::string ExchAppVersionDiscInfo::ClusterStorageTypeToStr() const 
{
    std::string clusterStorageType = "Disabled" ;
    switch( m_clusterStorageType )
    {
        case 1 :
            clusterStorageType = "NonShared - CCR environment" ;
            break ;
        case 2 :
            clusterStorageType = "Shared - Single Copy Cluster" ;
            break ;
    }
    return clusterStorageType;
}

std::string ExchAppVersionDiscInfo::ServerRoleToString() const
{
    std::string role ;
    switch( m_serverRole )
    {
        case 1 :
            role = "Front-End Server" ;
            break ;
        default :
            role = "Back-End Server" ;
            break ;
    }
    return role ;

}