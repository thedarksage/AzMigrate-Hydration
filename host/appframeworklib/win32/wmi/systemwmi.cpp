#ifndef SECURITY_WIN32
#define SECURITY_WIN32
#endif
#include "appglobals.h"
#include <string>
#include <sstream>
#include <atlbase.h>
#include <Security.h>
#include "host.h"
#include "systemwmi.h"
#include "system.h"
#include "hostagenthelpers.h"


SVERROR WMISysInfoImpl::init()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    SVERROR  bRet = SVS_FALSE ;
    if( m_init == false )
    {
        m_wmiProvider = "root\\CIMV2" ;
        if( m_wmiConnector.ConnectToServer(Host::GetInstance().GetHostName(), m_wmiProvider)  == 0 )
        {
            DebugPrintf(SV_LOG_DEBUG, "Connection to WMI provider %s is successful\n", m_wmiProvider.c_str()) ;
            m_init = true ;
            bRet = SVS_OK ;
        }
        else
        {
            DebugPrintf(SV_LOG_WARNING, "Connection to WMI provider %s is failed\n", m_wmiProvider.c_str()) ;
        }
    }
    else
    {
        DebugPrintf(SV_LOG_DEBUG, "WMI Interface already initialized\n") ;
        bRet = SVS_OK ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}

std::list<std::pair<std::string, std::string> > NetworkAdapterConfig::getPropertyList()
{
   std::list<std::pair<std::string, std::string> > list ;
   list.push_back(std::make_pair("MAC Address", this->m_macAddress)) ;
   for( unsigned long i=0; i < m_no_ipsConfigured; i++ )
   {
		list.push_back(std::make_pair("IP Address", this->m_ipaddress[i])) ;
   }
   if(this->m_dhcpEnabled)
   {
	   list.push_back(std::make_pair("DHCP", "ENABLED")) ;
   }
   else
   {
	   list.push_back(std::make_pair("DHCP", "NOT ENABLED")) ;
   }
   if(this->m_fullDNSRegistrationEnabled)
   {
	   list.push_back(std::make_pair("Full DNS Registration", "ENABLED")) ;
   }
   else
   {
	   list.push_back(std::make_pair("Full DNS Registration", "NOT ENABLED")) ;	
   }
   return list ;	
}	

SVERROR WMISysInfoImpl::loadNetworkAdapterConfiguration(std::list<NetworkAdapterConfig>& nwAdapterList)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    SVERROR  retStatus = SVS_FALSE ;
    if( init() != SVS_OK )
    {
        return retStatus;
    }
    std::stringstream stream; 
    std::stringstream query ;
    query << "SELECT * FROM Win32_NetworkAdapterConfiguration WHERE IPEnabled=True" ;
    USES_CONVERSION ;
    IEnumWbemClassObject* pEnumerator = NULL ;
    if( init() == SVS_OK )
    {
        if( m_wmiConnector.execQuery(query.str(), &pEnumerator) == SVS_OK )
        {
            retStatus = SVS_OK ;
            IWbemClassObject *pclsObj = NULL ;
            ULONG uReturn = 0;

            while (pEnumerator)
            {
                HRESULT hr = pEnumerator->Next(/*WBEM_INFINITE*/120*1000, 1, &pclsObj, &uReturn);
                if( SUCCEEDED(hr) )//Introduced 2min time limit to the wmi enumeration api
                {
                    if(0 == uReturn)
                    {
                        break;
                    }
                    NetworkAdapterConfig nwAdapterConfig ;
                    nwAdapterConfig.m_ErrorCode = DISCOVERY_SUCCESS;
                    bool bRet = true;
                    VARIANT vtProp;
                    USES_CONVERSION ;
                    HRESULT hrVar;
                    hrVar = pclsObj->Get(L"Description",0,&vtProp, 0, 0);
                    if(!FAILED(hrVar))
                    {
                        std::string description = W2A(vtProp.bstrVal);
                        if(description.find("VMware") == 0 || 
                            description.find("Virtual") == 0 ||
                            description.find("VMnet") == 0)
                        {
							VariantClear(&vtProp);
                            pclsObj->Release() ;
                            continue;
                        }
                    }
                    VariantClear(&vtProp);

                    hrVar = pclsObj->Get(L"IPEnabled", 0, &vtProp, 0, 0);
                    if(!FAILED(hrVar))
                    {
                        nwAdapterConfig.m_ipEnabled = vtProp.bVal ;
                    }
                    VariantClear(&vtProp);
                    hrVar = pclsObj->Get(L"DHCPEnabled", 0, &vtProp, 0, 0);
                    if(!FAILED(hrVar))
                    {
                        nwAdapterConfig.m_dhcpEnabled = vtProp.bVal ;
                    }
                    VariantClear(&vtProp);
                    vtProp.bstrVal = NULL;
                    hrVar = pclsObj->Get(L"MACAddress", 0, &vtProp, 0, 0);
                    if(!FAILED(hrVar))
                    {
                        nwAdapterConfig.m_macAddress = W2A(vtProp.bstrVal);

                    }
                    else
                    {
                        stream <<"Failed to get MACAddress";
                        bRet = false;
                    }
                    VariantClear(&vtProp);
					hrVar = pclsObj->Get(L"SettingID", 0, &vtProp, 0, 0);
					if(!FAILED(hrVar))
					{
						if(GetPhysicalIpAndSubnetMasksFromRegistry(W2A(vtProp.bstrVal),
							nwAdapterConfig.m_ipaddress,
							nwAdapterConfig.m_ipsubnet) != SVS_OK)
						{
							stream << "Failed to get IP Address(s) and its Subnetmask(s) from registry" << std::endl;
							bRet = false;
						}
						else
						{
							nwAdapterConfig.m_no_ipsConfigured = nwAdapterConfig.m_ipaddress.size();
						}
					}
					else
					{
						stream << "Failed to get adapter guid. Could not get IP and subnetmask information" << std::endl;
						bRet = false;
					}
					VariantClear(&vtProp);
                    hrVar = pclsObj->Get(L"DomainDNSRegistrationEnabled", 0, &vtProp, 0, 0);
                    if(!FAILED(hrVar))
                    {
                        nwAdapterConfig.m_domainDNSRegistrationEnabled = vtProp.bVal ;
                    }
                    VariantClear(&vtProp);
                    hrVar = pclsObj->Get(L"FullDNSRegistrationEnabled", 0, &vtProp, 0, 0);
                    if(!FAILED(hrVar))
                    {
                        nwAdapterConfig.m_fullDNSRegistrationEnabled = vtProp.bVal ;
                    }
                    if(bRet == false)
                    {
						nwAdapterConfig.m_ErrorCode = DISCOVERY_FAIL;
                        nwAdapterConfig.m_ErrorString = std::string(stream.str());
                    }
                    nwAdapterList.push_back(nwAdapterConfig) ;
                    VariantClear(&vtProp);
                    pclsObj->Release() ;
                }
                else
                {
					DebugPrintf(SV_LOG_ERROR, "Failed in IWbemClassObject next. hr = %08X return = %ld\n",hr, uReturn);
					break;
                }
            }
            pEnumerator->Release();
        }
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return retStatus ;
}

SVERROR WMISysInfoImpl::getInstalledHotFixIds(std::list<std::string>& installedHostFixsList)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;	
	SVERROR retStatus = SVS_FALSE;
	if( init() != SVS_OK )
	{
		return retStatus;
	}
	std::stringstream stream; 
	std::stringstream query ;
	query << "SELECT HotFixID FROM Win32_QuickFixEngineering" ;
	USES_CONVERSION ;
	IEnumWbemClassObject* pEnumerator = NULL ;

	if( m_wmiConnector.execQuery(query.str(), &pEnumerator) == SVS_OK )
	{
		retStatus = SVS_OK ;
		IWbemClassObject *pclsObj = NULL ;
		ULONG uReturn = 0;

        while (pEnumerator)
        {
            HRESULT hr = pEnumerator->Next(/*WBEM_INFINITE*/120*1000, 1, &pclsObj, &uReturn);
            if( SUCCEEDED(hr) )//Introduced 2min time limit to the wmi enumeration api
            {
                if(0 == uReturn)
                {
                    break;
                }
                VARIANT vtProp;
                USES_CONVERSION ;
                HRESULT hrVar;
                hrVar = pclsObj->Get(L"HotFixID",0,&vtProp, 0, 0);
                if(!FAILED(hrVar))
                {
                    std::string hotFixId = W2A(vtProp.bstrVal);
                    installedHostFixsList.push_back(hotFixId);
                }
                VariantClear(&vtProp);
                pclsObj->Release() ;
            }
            else
            {
                DebugPrintf(SV_LOG_ERROR, "Failed in IWbemClassObject next. hr = %08X return = %ld\n",hr, uReturn);
				break;
            }
        }
		pEnumerator->Release();
	}
		
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return retStatus;
}