#include "exchangeapplication.h"
#include <boost/algorithm/string.hpp>
#include <atlconv.h>
#include <sstream>
#include "ruleengine.h"
#include "clusterutil.h"
#include "host.h"
#include "exchangeexception.h"
#include "util/common/util.h"
#include "service.h"
#include "system.h"
#include "Clusteroperation.h"

SVERROR ExchangeApplication::init(const std::string& dnsname)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    SVERROR bRet = SVS_FALSE ;
    m_exchaDiscoveryImplPtr.reset( new ExchangeDiscoveryImpl() ) ;
    m_exchametaDataDiscoveryImplPtr.reset( new ExchangeMetaDataDiscoveryImpl() ) ;
    if( m_exchaDiscoveryImplPtr->init(dnsname) == SVS_OK )
    {
        if( m_exchametaDataDiscoveryImplPtr->init(dnsname) == SVS_OK )
        {
            bRet = SVS_OK ;
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR, "ExchangeMetaDataDiscoveryImpl::init failed\n") ;
        }
    }
    else
    {
        DebugPrintf(SV_LOG_ERROR, "ExchangeDiscoveryImpl::init failed\n") ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}
ExchangeApplication::ExchangeApplication(bool DiscoverDependetHosts)
:m_discoverDependencies(DiscoverDependetHosts)
{
    m_isInstalled = false ;
}

ExchangeApplication::ExchangeApplication(const std::string& evsName, bool DiscoverDependetHosts)
:m_discoverDependencies(DiscoverDependetHosts)
{
    m_evsList.push_back(evsName) ;
}

ExchangeApplication::ExchangeApplication(std::list<std::string> evsNamesList, bool DiscoverDependetHosts) 
:m_discoverDependencies(DiscoverDependetHosts),
m_evsList(evsNamesList)
{
    m_isInstalled = false ;
}

SVERROR ExchangeApplication::discoverApplication()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    SVERROR bRet = SVS_OK ;

    DebugPrintf(SV_LOG_DEBUG, "Found %d Active instances in this node to dicover.\n", m_exchHosts.size()) ;
    std::list<std::string>::const_iterator exchHostsIter = m_exchHosts.begin() ;
    while( exchHostsIter != m_exchHosts.end() )
    {
        ExchAppVersionDiscInfo discoveryInfo ;
		try
		{
			if( m_exchaDiscoveryImplPtr->discoverExchangeApplication(*exchHostsIter, discoveryInfo) == SVS_OK )
			{
				m_exchaDiscoveryImplPtr->Display(discoveryInfo) ;
			}
			else
			{
				discoveryInfo.m_errCode = DISCOVERY_FAIL ;
				DebugPrintf(SV_LOG_WARNING, "ExchangeDiscoveryImpl::discoverExchangeApplication for %s\n", exchHostsIter->c_str()) ;
				bRet = SVS_FALSE;
			}
			m_exchDiscoveryInfoMap.insert(std::make_pair(*exchHostsIter, discoveryInfo)) ;
		}
		catch(ExchageException& ex)
		{
			DebugPrintf(SV_LOG_ERROR, "Exception while discovering the Exchange application\n") ;
			DebugPrintf(SV_LOG_ERROR, "Exception Details : %s\n", ex.to_string().c_str()) ;
			discoveryInfo.m_cn = *exchHostsIter;
			discoveryInfo.m_errCode = DISCOVERY_FAIL ;
			discoveryInfo.m_errString = "Exception while discovering the Exchange application" ;
			discoveryInfo.m_appType = INM_APP_EXCHNAGE ;
			m_exchDiscoveryInfoMap.insert(std::make_pair(*exchHostsIter, discoveryInfo)) ;
			bRet = SVS_FALSE ;
		}
		catch(...)
		{
			DebugPrintf(SV_LOG_ERROR, "Unhandled exception while discovering the Exchange application\n") ;
			discoveryInfo.m_cn = *exchHostsIter;
			discoveryInfo.m_errCode = DISCOVERY_FAIL ;
			discoveryInfo.m_errString = "Unhandled exception while discovering the Exchange application" ;
			discoveryInfo.m_appType = INM_APP_EXCHNAGE ;
			m_exchDiscoveryInfoMap.insert(std::make_pair(*exchHostsIter, discoveryInfo)) ;
			bRet = SVS_FALSE ;
		}
        exchHostsIter++ ;
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}
SVERROR ExchangeApplication::discoverMetaData()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    SVERROR bRet = SVS_FALSE ;

	if( m_exchDiscoveryInfoMap.size() != 0 )
	{
        std::map<std::string, ExchAppVersionDiscInfo>::const_iterator discoveryInfoIter = m_exchDiscoveryInfoMap.begin() ;
        while( discoveryInfoIter != m_exchDiscoveryInfoMap.end() )
        {
			ExchangeMetaData metadata ;
			InmProtectedAppType appType = discoveryInfoIter->second.m_appType;
			try
			{
				bRet = SVS_FALSE ; 
				if( discoveryInfoIter->second.m_errCode == PASSIVE_INSTANCE )
				{
					metadata.errCode = PASSIVE_INSTANCE ;
					metadata.errString = "" ;
					m_exchMetaDataMap.insert(std::make_pair(discoveryInfoIter->first, metadata)) ;
					bRet = SVS_OK;
				}
				else
				{					
					if( appType == INM_APP_EXCH2003 || appType == INM_APP_EXCH2007 || appType == INM_APP_EXCHNAGE )
					{
						if( m_exchametaDataDiscoveryImplPtr->getExchangeMetaData(discoveryInfoIter->first, metadata) == SVS_OK )
						{
							DebugPrintf(SV_LOG_DEBUG, "Successfully Discovered Exchange MetaData\n") ;
							if( m_discoverDependencies )
							{
								std::list<std::string> publicFolderMailBoxServers ;
								metadata.getPublicFolderDBsMailBoxServerNameList(publicFolderMailBoxServers) ;
								std::list<std::string>::iterator publicFolderHostIter = publicFolderMailBoxServers.begin() ;
								while( publicFolderHostIter != publicFolderMailBoxServers.end() )
								{
									std::string publicFolderMailBoxServerName = *publicFolderHostIter;
									if( isfound(m_exchHosts, publicFolderMailBoxServerName) == false && strcmpi(publicFolderMailBoxServerName.c_str(), Host::GetInstance().GetHostName().c_str()) != 0 )
									{
										DebugPrintf(SV_LOG_DEBUG, "public folder MailBox Serevr Name: %s\n", publicFolderMailBoxServerName.c_str()) ;
										trimSpaces(publicFolderMailBoxServerName) ;
										if( publicFolderMailBoxServerName.compare("") == 0 )
										{
											publicFolderHostIter++ ;
											continue ;
										}
										m_exchHosts.push_back(publicFolderMailBoxServerName) ;
										ExchAppVersionDiscInfo discoveryInfo ;
										m_exchaDiscoveryImplPtr->discoverExchangeApplication(publicFolderMailBoxServerName, discoveryInfo, false) ;
										//m_exchDiscoveryInfoMap.insert(std::make_pair(discoveryInfoIter->first, discoveryInfo)) ;
										m_exchDiscoveryInfoMap.insert(std::make_pair(publicFolderMailBoxServerName, discoveryInfo)) ;

									}
									publicFolderHostIter++ ;
								}
								m_exchametaDataDiscoveryImplPtr->Display(metadata) ;
							}
							bRet = SVS_OK ;
						}
						else
						{
							DebugPrintf(SV_LOG_ERROR, "Failed to discover exchange metadata.\n");
							metadata.errCode = DISCOVERY_FAIL;
							metadata.errString = "Failed to discover exchange metadata.";
						}
						m_exchMetaDataMap.insert(std::make_pair(discoveryInfoIter->first, metadata)) ;
					}
					else if( appType == INM_APP_EXCH2010 )
					{
						Exchange2k10MetaData metadata2k10;
						if( m_exchametaDataDiscoveryImplPtr->getExchange2k10MetaData(discoveryInfoIter->first, metadata2k10) == SVS_OK )
						{
							DebugPrintf(SV_LOG_DEBUG, "Successfully Discovered Exchange MetaData\n") ;
							bRet = SVS_OK ;
							m_exchametaDataDiscoveryImplPtr->dumpMetaData(metadata2k10);
						}
						else
						{
							DebugPrintf(SV_LOG_ERROR, "Failed to dicover Exchange 2010 metadata.\n");
							metadata2k10.errCode = DISCOVERY_FAIL;
							metadata2k10.errString = "Failed to discover exchange 2010 metadata.";
						}
						m_exch2k10MetaDataMap.insert(std::make_pair(discoveryInfoIter->first, metadata2k10)) ;
					}
				}
			}
			catch(ExchageException &ex)
			{
				DebugPrintf(SV_LOG_ERROR, "Exception while discovering metadata of the Exchange application\n") ;
				DebugPrintf(SV_LOG_ERROR, "Exception Details : \n", ex.to_string().c_str()) ;
				if( appType == INM_APP_EXCH2003 || appType == INM_APP_EXCH2007 || appType == INM_APP_EXCHNAGE )
				{
					metadata.errCode = DISCOVERY_METADATANOTAVAILABLE ;
					metadata.errString = "Exception while discovering metadata of the Exchange application" ;
					m_exchMetaDataMap.insert(std::make_pair(discoveryInfoIter->first, metadata)) ;
				}
				else if( appType == INM_APP_EXCH2010 )
				{
					Exchange2k10MetaData metadata2k10;
					metadata2k10.errCode = DISCOVERY_METADATANOTAVAILABLE ;
					metadata2k10.errString = "Exception while discovering metadata of the Exchange application" ;
					m_exch2k10MetaDataMap.insert(std::make_pair(discoveryInfoIter->first, metadata2k10)) ;
				}
				bRet = SVS_FALSE ; 
			}
			catch(...)
			{
				DebugPrintf(SV_LOG_ERROR, "Unhandled Exception while discovering metadata of the Exchange application\n") ;
				if( appType == INM_APP_EXCH2003 || appType == INM_APP_EXCH2007 || appType == INM_APP_EXCHNAGE )
				{
					metadata.errCode = DISCOVERY_METADATANOTAVAILABLE ;
					metadata.errString = "Unhandled Exception while disocvering metadata of Exchange application" ;
					m_exchMetaDataMap.insert(std::make_pair(discoveryInfoIter->first, metadata)) ;
				}
				else if( appType == INM_APP_EXCH2010 )
				{
					Exchange2k10MetaData metadata2k10;
					metadata2k10.errCode = DISCOVERY_METADATANOTAVAILABLE ;
					metadata2k10.errString = "Unhandled Exception while disocvering metadata of Exchange application" ;
					m_exch2k10MetaDataMap.insert(std::make_pair(discoveryInfoIter->first, metadata2k10)) ;
				}
				bRet = SVS_FALSE ; 
			}
            discoveryInfoIter++ ;
        }
    }    
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}

std::string ExchangeApplication::summary(std::list<ValidatorPtr>& list)
{
		std::wstringstream stream ;
		USES_CONVERSION;

        std::list<ValidatorPtr>::iterator ruleiter = list.begin() ;
        stream << "\n\n\t\tMicrosoft Exchange Pre-Requisites Validation Results" << std::endl <<std::endl ;
	    while (ruleiter!= list.end() )
	    {
            RuleEngine::getInstance()->RunRule((*ruleiter)) ;
			std::string RuleId = Validator::getInmRuleIdStringFromID((*ruleiter)->getRuleId() );
            stream << "\t\t\tRule Name\t\t:" << A2W(RuleId.c_str()) << "\n";
            stream << "\t\t\tResult\t\t\t:" << A2W((*ruleiter)->statusToString().c_str()) << std::endl ;
		    ruleiter++ ;
	    }

		stream << std::endl << std::endl ;
		return W2A(stream.str().c_str()) ;
}
SVERROR ExchangeApplication::getEVSNamesList(std::list<std::string>& activeInstanceNameList, std::list<std::string>& passiveInstanceNameList)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	SVERROR bRet = SVS_FALSE ;
    DebugPrintf(SV_LOG_INFO, "Checking for cluster resources wrt to Exchange application\n") ;
    std::list<std::string> exchInfoStoreInstanceNames ;
    if( getResourcesByType("", "Microsoft Exchange Information Store", exchInfoStoreInstanceNames) == SVS_OK )
    {
        DebugPrintf(SV_LOG_INFO, "Number of resources of Type Exchange Informaiton Store Instance %d\n", exchInfoStoreInstanceNames.size()) ;
        std::list<std::string>::iterator & stringIter = exchInfoStoreInstanceNames.begin() ;
        while( stringIter != exchInfoStoreInstanceNames.end() )
        {
            std::string resourceName = *stringIter;
            CLUSTER_RESOURCE_STATE state ;
            std::string groupName, nodeName ;
            if( getResourceState("", resourceName, state, groupName, nodeName ) == SVS_OK ) 
            {
				OSVERSIONINFOEX osVersionInfo ;
				getOSVersionInfo(osVersionInfo) ;
				if(osVersionInfo.dwMajorVersion >= WIN2K8_SERVER_VER)
				{
					std::list<std::string> networknameKeyList;
					networknameKeyList.push_back("NetworkName");
					std::map<std::string, std::string> outputPropertyMap; 
					if(getResourcePropertiesMap("",resourceName,networknameKeyList,outputPropertyMap) == SVS_OK)
					{
						bRet = SVS_OK ;
						std::string evsName;
						if(outputPropertyMap.find("NetworkName") != outputPropertyMap.end())
							evsName = outputPropertyMap.find("NetworkName")->second;
						DebugPrintf(SV_LOG_DEBUG, "Node Name is %s , Host name is %s \n",nodeName.c_str(), Host::GetInstance().GetHostName().c_str() );
						m_isInstalled = true ;
						boost::algorithm::to_upper(evsName) ;
						if( strcmpi(nodeName.c_str(), Host::GetInstance().GetHostName().c_str() ) == 0 )
						{
							DebugPrintf(SV_LOG_DEBUG, "Virtual server %s is active on localhost\n", evsName.c_str()) ;
							activeInstanceNameList.push_back(evsName);
						}
						else
						{
							DebugPrintf(SV_LOG_DEBUG, "Virtual server %s is passive on localhost \n", evsName.c_str()) ;
							DebugPrintf(SV_LOG_DEBUG, "This node is in passive state for the mail box server %s\n", (*stringIter).c_str() );						    							
							passiveInstanceNameList.push_back(evsName);
						}
					}
					else
					{
						DebugPrintf(SV_LOG_ERROR, "Failed to get The Properties of resource %s \n", resourceName.c_str() );
					}
				}
				else
				{
					std::list<std::string> exchInfoStoreDependencyList; 
					if(getDependedResorceNameListOfType(resourceName,"Microsoft Exchange System Attendant",exchInfoStoreDependencyList) == SVS_OK)
					{
						std::list<std::string>::iterator& exchInfoDependIter = exchInfoStoreDependencyList.begin();
						while(exchInfoDependIter != exchInfoStoreDependencyList.end())
						{
							std::string exchInfoStoreDepnName = *exchInfoDependIter;
							if( getResourceState("", exchInfoStoreDepnName, state, groupName, nodeName ) == SVS_OK ) 
							{
								std::list<std::string> exchSysAtndDepndList;
								if(getDependedResorceNameListOfType(exchInfoStoreDepnName,"Network Name",exchSysAtndDepndList) == SVS_OK)
								{
									bRet = SVS_OK ;
									std::list<std::string>::iterator& exchSysAtndDepndIter = exchSysAtndDepndList.begin();
									while(exchSysAtndDepndIter != exchSysAtndDepndList.end())
									{
										std::string depnNetworkName = *exchSysAtndDepndIter;
										if(getResourceState("", depnNetworkName, state, groupName, nodeName ) == SVS_OK )
										{
											std::list<std::string> networknameKeyList;
											networknameKeyList.push_back("Name");
											std::map<std::string, std::string> outputPropertyMap;
											if(getResourcePropertiesMap("",depnNetworkName,networknameKeyList,outputPropertyMap) == SVS_OK)
											{
												std::string evsName;
												if(outputPropertyMap.find("Name") != outputPropertyMap.end())
													evsName = outputPropertyMap.find("Name")->second;
												DebugPrintf(SV_LOG_DEBUG, "Node Name is %s , Host name is %s \n",nodeName.c_str(), Host::GetInstance().GetHostName().c_str() );
												m_isInstalled = true ;
												boost::algorithm::to_upper(evsName) ;
												if( strcmpi(nodeName.c_str(), Host::GetInstance().GetHostName().c_str() ) == 0 )
												{
													DebugPrintf(SV_LOG_DEBUG, "Virtual server %s is active on localhost\n", evsName.c_str()) ;
													activeInstanceNameList.push_back(evsName);
												}
												else
												{
													DebugPrintf(SV_LOG_DEBUG, "Virtual server %s is passive on localhost \n", evsName.c_str()) ;
													DebugPrintf(SV_LOG_DEBUG, "This node is in passive state for the mail box server %s\n", (*stringIter).c_str() );						    							
													passiveInstanceNameList.push_back(evsName);
												}
											}
											else
											{
												DebugPrintf(SV_LOG_ERROR, "Failed to get The Properties of resource %s \n", depnNetworkName.c_str() );
											}
										}
										else
										{
											DebugPrintf(SV_LOG_ERROR, "Failed to get the status of resource: %s \n",  depnNetworkName.c_str() );
										}
										exchSysAtndDepndIter++;
									}
								}
								else
								{
									DebugPrintf(SV_LOG_ERROR, "Failed to get the dependencylist of resource: %s of type \"Network Name\" \n", exchInfoStoreDepnName.c_str() );
								}
							}
							else
							{
								DebugPrintf(SV_LOG_ERROR, "Failed to get the status of resource: %s \n",  exchInfoStoreDepnName.c_str() );
							}
							exchInfoDependIter++;
						}
					}
					else
					{
						DebugPrintf(SV_LOG_ERROR, "Failed to get the dependencylist of resource: %s of type \"Microsoft Exchange System Attendant\" \n", resourceName.c_str() );
					}
				}
            }
            else
            {
                DebugPrintf(SV_LOG_ERROR, "Failed to get the status of resource: %s \n", resourceName.c_str() );
            }
            stringIter++ ;
        }
    }
    if( bRet != SVS_OK )
    {
        throw "Failed to enumerate the Cluster for the exchange virtual server names\n" ;
    }
    return bRet;
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}
void ExchangeApplication::prepareExchHostsList()
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	SVERROR bRet = SVS_FALSE ;
	m_exchHosts.clear() ;
    std::list<std::string> activeInstanceNameList, passiveInstanceNameList;
	bool bIsClusterNode = isClusterNode();
    if( !bIsClusterNode && m_evsList.size() )
	{
		m_exchHosts = m_evsList ;
	}
	else if(bIsClusterNode)
	{
		getEVSNamesList(activeInstanceNameList, passiveInstanceNameList) ;
		if( m_evsList.size() == 0 )
		{
			m_evsList = activeInstanceNameList ;
			m_evsList.insert(m_evsList.end(), passiveInstanceNameList.begin(), passiveInstanceNameList.end() ) ;
		}
		std::list<std::string>::iterator evsListBegIter =  m_evsList.begin();
		std::list<std::string>::iterator evsListEndIter =  m_evsList.end();
		while(evsListBegIter != evsListEndIter)
		{
			if(isfound(activeInstanceNameList, *evsListBegIter))
			{
				m_exchHosts.push_back(*evsListBegIter) ;
			}
			else
			{
				DebugPrintf(SV_LOG_DEBUG, "Virtual server %s is passive on localhost \n", evsListBegIter->c_str()) ;
				DebugPrintf(SV_LOG_DEBUG, "This node is in passive state for the mail box server %s\n", evsListBegIter->c_str() );						    							
				ExchAppVersionDiscInfo discoveryInfo ;
				discoveryInfo.m_cn = *evsListBegIter ;
				discoveryInfo.m_errCode = PASSIVE_INSTANCE;
				discoveryInfo.m_errString = "PASSIVE_INSTANCE";
				m_exchDiscoveryInfoMap.insert(std::make_pair(*evsListBegIter, discoveryInfo)) ;									
			}
			evsListBegIter++;
		}
	}
    InmServiceStatus status ;
    if( isServiceInstalled("MSExchangeIS",  status ) == SVS_OK && status == INM_SVCSTAT_INSTALLED )
    {
        m_isInstalled = true ;
        std::string hostname = Host::GetInstance().GetHostName() ;
        if(m_exchaDiscoveryImplPtr->isInstalled(hostname)  )
        {
            DebugPrintf(SV_LOG_INFO, "Exchange Application is installed on localhost %s\n", hostname.c_str()) ;
            boost::algorithm::to_upper(hostname);
            m_exchHosts.push_back(hostname) ;
        }
        else
        {
            DebugPrintf(SV_LOG_INFO, "MSExchangeIS service is available in local node and exchange not found for %s in the AD\n", hostname.c_str()) ;
            if( !m_exchHosts.size() && !passiveInstanceNameList.size() )
            {
                DebugPrintf(SV_LOG_ERROR, "The EVS Names list is empty.. Cannot continue the discovery\n") ;
                throw "The current node is not in consistent state to continue the exchange discovery\n" ;
            }
        }
    }
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}

bool ExchangeApplication::isInstalled()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	bool bRet = false ; 
    try
    {
        InmServiceStatus status ;
        if( isServiceInstalled("MSExchangeIS",  status ) == SVS_OK && status == INM_SVCSTAT_NOTINSTALLED )
        {
            DebugPrintf(SV_LOG_DEBUG, "ExchangeIS service is not installed on the local machine.") ;
            return false ;
        }
    }
    catch(const char* ex )
    {
        DebugPrintf(SV_LOG_ERROR, "Caught exception while checking service status %s\n", ex) ;
    }
	if( init("")  != SVS_OK )
    {
		DebugPrintf(SV_LOG_ERROR, "Error while initilization\n");
        throw "Unable to Initialize ExchangeApplication object.. Exchange discovery cannot be continued" ;
	}
	else
	{
	    prepareExchHostsList() ;
        if( !m_isInstalled )
        {
	        std::list<std::string>::const_iterator iterator = m_exchHosts.begin() ;
            while( iterator != m_exchHosts.end() )
            {
	            if( m_exchaDiscoveryImplPtr->isInstalled(*iterator) )
	            {
		            DebugPrintf(SV_LOG_DEBUG, "exchange is installed on %s\n", iterator->c_str()) ;
		            m_isInstalled = true ;
		            break ;
	            }
	            iterator++ ;
            }
        }
	}
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return m_isInstalled ;
}

bool ExchangeApplication::isInstalledInDomain(std::list<std::string>& exchHosts)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	bool bRet = false ; 
	if( init("")  != SVS_OK )
    {
		DebugPrintf(SV_LOG_ERROR, "Error while initilization\n");
        throw "Unable to Initialize ExchangeApplication object.. Exchange discovery cannot be continued" ;
	}
	else
	{
        m_isInstalled = false ;
		std::list<std::string>::const_iterator iterator = m_exchHosts.begin() ;
        if( !m_isInstalled )
        {
		    if( m_exchaDiscoveryImplPtr->isInstalled(exchHosts) )
		    {
                m_exchHosts = exchHosts ;
			    m_isInstalled = true ;
		    }
        }
	}
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return m_isInstalled ;
}
void ExchangeApplication::clean()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    m_exchMetaDataMap.clear();
    m_exchDiscoveryInfoMap.clear();
    m_exchHosts.clear();
	m_exch2k10MetaDataMap.clear();
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}
std::list<std::string> ExchangeApplication::getActiveInstances()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    std::list<std::string> InstanceNamesList ;
    std::map<std::string, ExchAppVersionDiscInfo>::iterator exchandeDiscoveryIter = m_exchDiscoveryInfoMap.begin() ;
    while( exchandeDiscoveryIter != m_exchDiscoveryInfoMap.end() )
    {
        if( exchandeDiscoveryIter->second.m_errCode == DISCOVERY_SUCCESS )
        {
        	DebugPrintf(SV_LOG_DEBUG, "pushing the instance name %s\n", exchandeDiscoveryIter->second.m_cn.c_str()) ;
            InstanceNamesList.push_back(exchandeDiscoveryIter->second.m_cn) ;
        }
        exchandeDiscoveryIter++ ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return InstanceNamesList ;
}
std::list<std::string> ExchangeApplication::getInstances()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    std::list<std::string> InstanceNamesList ;
    std::map<std::string, ExchAppVersionDiscInfo>::iterator exchandeDiscoveryIter = m_exchDiscoveryInfoMap.begin() ;
    while( exchandeDiscoveryIter != m_exchDiscoveryInfoMap.end() )
    {
        DebugPrintf(SV_LOG_DEBUG, "pushing the instance name %s\n", exchandeDiscoveryIter->second.m_cn.c_str()) ;
        DebugPrintf(SV_LOG_DEBUG, "active or passive %d\n", exchandeDiscoveryIter->second.m_errCode) ;
        InstanceNamesList.push_back(exchandeDiscoveryIter->second.m_cn) ;
        exchandeDiscoveryIter++ ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return InstanceNamesList ;
}

std::map<std::string, std::list<std::string> > ExchangeApplication::getVersionInstancesMap()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    std::map<std::string, std::list<std::string> > versionInstancesMap ;
    std::list<std::string> InstanceNamesList ;
    std::string version ;
    std::map<std::string, ExchAppVersionDiscInfo>::iterator exchandeDiscoveryIter = m_exchDiscoveryInfoMap.begin() ;
    while( exchandeDiscoveryIter != m_exchDiscoveryInfoMap.end() )
    {
        InstanceNamesList.push_back(exchandeDiscoveryIter->second.m_cn) ;
        switch( exchandeDiscoveryIter->second.m_appType )
        {
        case INM_APP_EXCH2003 :
            version = "EXCHANGE2003" ;
            break ;
        case INM_APP_EXCH2007 :
            version = "EXCHANGE2007" ;
            break ;
        case INM_APP_EXCH2010 :
            version = "EXCHANGE2010" ;
            break ;
        }
        DebugPrintf(SV_LOG_DEBUG, "instance %s\n", exchandeDiscoveryIter->second.m_cn.c_str()) ;
        exchandeDiscoveryIter++ ;
    }
    DebugPrintf(SV_LOG_DEBUG, "version %s\n", version.c_str()) ;
    versionInstancesMap.insert(std::make_pair(version, InstanceNamesList)) ;
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return versionInstancesMap ;
}

void ExchangeApplication::getdependentaPrivateMailServers(std::set<std::string>& mailServers, const std::string& pfs)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    ExchangeApplicationPtr exchPtr ;
    exchPtr.reset( new ExchangeApplication() ) ;
    std::list<std::string> exchHosts ;
    exchPtr->isInstalledInDomain(exchHosts) ;
    std::list<std::string>::const_iterator iter = exchHosts.begin() ;
    while( iter != exchHosts.end() )
    {
        exchPtr.reset( new ExchangeApplication(*iter) ) ;
        if( exchPtr->isInstalled() )
        {
            exchPtr->discoverApplication() ;
            exchPtr->discoverMetaData() ;
            std::map<std::string, ExchangeMetaData>::iterator dataIter = exchPtr->m_exchMetaDataMap.begin() ;
            while( dataIter != exchPtr->m_exchMetaDataMap.end() )
            {
                std::list<ExchangeStorageGroupMetaData>::iterator stIter = dataIter->second.m_storageGrps.begin() ;
                while( stIter != dataIter->second.m_storageGrps.end() )
                {
                    std::list<ExchangeMDBMetaData>::iterator dbIter = stIter->m_mdbMetaDataList.begin() ;
                    while( dbIter != stIter->m_mdbMetaDataList.end() )
                    {
                        DebugPrintf(SV_LOG_DEBUG, "exchName %s\n", dataIter->first.c_str()) ;
                        DebugPrintf(SV_LOG_DEBUG, "\tstorage group name %s\n",  stIter->m_storageGrpName.c_str()) ;
                        DebugPrintf(SV_LOG_DEBUG, "\tmail store name %s\n", dbIter->m_defaultPublicFolderDBs_MailBoxServerName.c_str()) ;
                        DebugPrintf(SV_LOG_DEBUG, "\tpublic folder host name %s\n", dbIter->m_defaultPublicFolderDBs_MailBoxServerName.c_str()) ;
						if( strcmpi( pfs.c_str(), dbIter->m_defaultPublicFolderDBs_MailBoxServerName.c_str() ) == 0 && 
							strcmpi(pfs.c_str(),dataIter->first.c_str()) != 0)
                        {
                            mailServers.insert( dataIter->first ) ;
                        }
                        dbIter ++ ;
                    }
                    stIter++ ;
                }
                dataIter++ ;
            }
            std::map<std::string, Exchange2k10MetaData>::iterator data2k10Iter = exchPtr->m_exch2k10MetaDataMap.begin() ;
            while( data2k10Iter != exchPtr->m_exch2k10MetaDataMap.end() )
            {
                std::list<ExchangeMDBMetaData>::iterator dbIter = data2k10Iter->second.m_mdbMetaDataList.begin() ;
                while( dbIter != data2k10Iter->second.m_mdbMetaDataList.end() )
                {

                    DebugPrintf(SV_LOG_DEBUG, "exchName %s\n", data2k10Iter->first.c_str()) ;
                    DebugPrintf(SV_LOG_DEBUG, "\tmail store name %s\n", dbIter->m_defaultPublicFolderDBs_MailBoxServerName.c_str()) ;
                    DebugPrintf(SV_LOG_DEBUG, "\tpublic folder host name %s\n", dbIter->m_defaultPublicFolderDBs_MailBoxServerName.c_str()) ;
					if( strcmpi( pfs.c_str(), dbIter->m_defaultPublicFolderDBs_MailBoxServerName.c_str() ) == 0 )
                    {
                        mailServers.insert( dataIter->first ) ;
                    }
                    dbIter ++ ;
                }
                data2k10Iter++ ;
            }
        }
        iter++ ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}