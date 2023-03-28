#ifndef _WINDOWS_
#include <Windows.h>
#endif
#include <windns.h>
#include <sstream>

#include "wmi/systemwmi.h"
#include "systemvalidators.h"
#include "service.h"
#include "system.h"
#include "host.h"
#include "ldap/ldap.h"
#include "util/common/util.h"
#include "ADInterface.h"
#include "hostdiscovery.h"

ServiceValidator::ServiceValidator(const std::string& name, const std::string& description,
                                   const InmService service, InmServiceStatus serviceStatus,
                                   InmRuleIds ruleId)
:Validator(name, description, ruleId),
m_status(serviceStatus)
{
    m_services.push_back(service) ;
}

ServiceValidator::ServiceValidator(const InmService service, InmServiceStatus serviceStatus,
                                   InmRuleIds ruleId)
:Validator(ruleId),
m_status(serviceStatus)
{
    m_services.push_back(service) ;
}


ServiceValidator::ServiceValidator(const std::string& name, const std::string& description,
                                   const std::list<InmService> services, InmServiceStatus serviceStatus,
                                   InmRuleIds ruleId)
:Validator(name, description, ruleId),
m_services(services),
m_status(serviceStatus)
{
}

ServiceValidator::ServiceValidator(const std::list<InmService> services, 
                                                      InmServiceStatus serviceStatus,
                                                      InmRuleIds ruleId)
:Validator(ruleId),
m_services(services),
m_status(serviceStatus)
{

}
bool ServiceValidator::canfix()
{
    return true ;
}

SVERROR ServiceValidator::fix()
{
/*
    DebugPrintf(SV_LOG_DEBUG, "ENETERD %s\n", FUNCTION_NAME) ;
    SVERROR bRet = SVS_FALSE ;
    SVERROR fixed = SVS_FALSE ;
    InmRuleStatus ruleStatus = INM_RULESTAT_NONE ;

    std::stringstream stream ;

    if( canfix() )
    {
        std::list<std::string>::iterator serviceNameIter ;
         serviceNameIter = m_serviceNames.begin() ;
        if( m_status == INM_SVCSTAT_STOPPED )
        {
            while( serviceNameIter != m_serviceNames.end() )
            {
                if( StopService(*serviceNameIter) == SVS_OK )
                {
                    fixed = SVS_OK ;
                }
                else
                {
                    fixed = SVS_FALSE ;
                    stream << "Failed to stop the service.. " << serviceNameIter->c_str() << std::endl ;
                }
            }
            serviceNameIter++ ;
        }
        else if( m_status == INM_SVCSTAT_RUNNING )
        {
            while( serviceNameIter != m_serviceNames.end() )
            {
                if( StartSvc(*serviceNameIter) == SVS_OK )
                {
                    fixed = SVS_OK ;
                }
                else
                {
                    stream << "Failed to start the service.. " << serviceNameIter->c_str() << std::endl ;
                    fixed = SVS_FALSE ;
                }
                serviceNameIter++ ;
            }
        }
        if( fixed != SVS_OK )
        {
            ruleStatus = INM_RULESTAT_FAILED ;
        }
        else
        {
            ruleStatus = INM_RULESTAT_FIXED ;
            bRet = SVS_OK ;
        }
    }
    setRuleMessage(stream.str()) ;
    if( ruleStatus == INM_RULESTAT_FIXED )
    {
        setRuleExitCode(RULE_PASSED) ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
    */
    return SVS_FALSE ;
}



SVERROR ServiceValidator::evaluate()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    SVERROR bRet = SVS_FALSE ;
    InmRuleStatus ruleStatus = INM_RULESTAT_NONE ;
    std::stringstream stream ;
	InmRuleErrorCode errorCode = INM_ERROR_NONE ;
    std::list<InmService>::iterator serviceIter = m_services.begin() ;
    while( serviceIter != m_services.end() )
    {
        if( m_status == INM_SVCSTAT_INSTALLED || 
            m_status == INM_SVCSTAT_NOTINSTALLED )
        {

            if( serviceIter->m_svcStatus == m_status )
                {
                    if( INM_SVCSTAT_INSTALLED )
                    {
                    stream << "Service " << serviceIter->m_serviceName<< " is installed on the machine\n" ;
                    }
                    else
                    {
                    stream << "Service " << serviceIter->m_serviceName << " is not installed on the machine\n" ;
                    }
                    if( ruleStatus != INM_RULESTAT_FAILED )
                    {
                        ruleStatus = INM_RULESTAT_PASSED ;
                        errorCode = RULE_PASSED ;
						stream << "Service " << serviceIter->m_serviceName << " is installed on the machine\n" ;
                        bRet = SVS_OK ;
                    }
                }
                else
                {
                    if( INM_SVCSTAT_NOTINSTALLED )
                    {
                    stream << "Service " << serviceIter->m_serviceName << " is installed on the machine\n" ;
                    }
                    else
                    {
                    stream << "Service " << serviceIter->m_serviceName << " is not installed on the machine\n" ;
                    }
                    ruleStatus = INM_RULESTAT_FAILED ;
                    errorCode = SERVICE_STATUS_ERROR ;
                    bRet = SVS_FALSE ;
                }
            }
        else
        {
            if( serviceIter->m_svcStatus != m_status )
            {
               switch( m_status )
               {
                   case INM_SVCSTAT_NONE :
                   break ;
                   case INM_SVCSTAT_ACCESSDENIED :
                       stream << "Access not denied for " << " Service " << serviceIter->m_serviceName ;
                        break ;
                   case INM_SVCSTAT_NOTINSTALLED :
                        stream << "Service "<< serviceIter->m_serviceName << " installed\n" ;
                        break ;
                   case INM_SVCSTAT_INSTALLED :
                        stream << "Service "<< serviceIter->m_serviceName << " not installed\n" ;
                        break ;
                   case INM_SVCSTAT_STOPPED :
                        stream << "Service "<< serviceIter->m_serviceName << " not stopped\n" ;
                        break ;
                   case INM_SVCSTAT_START_PENDING : 
                        stream << "Service " << serviceIter->m_serviceName << " not start pending\n" ;
                        break ;
                   case INM_SVCSTAT_STOP_PENDING: 
                        stream << "Service " << serviceIter->m_serviceName << " not stop pending\n" ;
                        break ;
                   case INM_SVCSTAT_RUNNING : 
                        stream << "Service " << serviceIter->m_serviceName << " not running\n " ;
                        break ;
                   case INM_SVCSTAT_CONTINUE_PENDING :
                        stream << "Service "<< serviceIter->m_serviceName << " not continue pending\n" ;
                        break ;
                   case INM_SVCSTAT_PAUSE_PENDING : 
                        stream << "Service "<< serviceIter->m_serviceName << " not pause pending\n" ;
                        break ;
                   case INM_SVCSTAT_PAUSED :
                        stream << "Service " << serviceIter->m_serviceName << " not paused\n" ;
                        break ;
               }
               ruleStatus = INM_RULESTAT_FAILED ;
               errorCode = SERVICE_STATUS_ERROR ;
            }
			else
			{
				stream << "Service: " << serviceIter->m_serviceName << " is running\n " ;
			}
        }
        serviceIter++ ;
    }
    if( ruleStatus != INM_RULESTAT_FAILED )
    {
        setStatus( INM_RULESTAT_PASSED ) ;
        setRuleExitCode( RULE_PASSED ) ;
        bRet = SVS_OK ;

    }
    else
    {
	    setStatus( ruleStatus ) ;
        setRuleExitCode( errorCode ) ;
    }
    setRuleMessage( stream.str() ) ;
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}

ProcessValidator::ProcessValidator(const std::string& name, 
                                   const std::string& description,
                                   const std::string processName, 
                                   InmProcessStatus processStat, InmRuleIds ruleId)

:Validator(name, description, ruleId),
m_processName(processName),
m_status(processStat)
{
}

ProcessValidator::ProcessValidator(const std::string processName, 
                                   InmProcessStatus processStat, 
                                   InmRuleIds ruleId)
:Validator(ruleId)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    m_processName = processName ;
    m_status = processStat ;
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}
bool ProcessValidator::canfix()
{
    DebugPrintf(SV_LOG_DEBUG, "ENETERD %s\n", FUNCTION_NAME) ;
    bool bRet = false ;
    return bRet ;
}

SVERROR ProcessValidator::fix()
{
    DebugPrintf(SV_LOG_DEBUG, "ENETERD %s\n", FUNCTION_NAME) ;
    SVERROR bRet = SVS_FALSE ;
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}



SVERROR ProcessValidator::evaluate()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    SVERROR bRet = SVE_FAIL ;
    setStatus(INM_RULESTAT_FAILED) ;
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}

FirewallValidator::FirewallValidator(const std::string& name, 
                                     const std::string& description,
                                     InmFirewallStatus currentStatus, 
                                     InmFirewallStatus requiredStatus,
                                     InmRuleIds ruleId)
:Validator(name, description, ruleId),
m_currentStatus(currentStatus),
m_requiredStatus(requiredStatus)

{
}

FirewallValidator::FirewallValidator(InmFirewallStatus currentStatus,
                                     InmFirewallStatus requiredStatus, InmRuleIds ruleId)
:Validator(ruleId),
m_currentStatus(currentStatus),
m_requiredStatus(requiredStatus)
{
}


bool FirewallValidator::canfix()
{
    DebugPrintf(SV_LOG_DEBUG, "ENETERD %s\n", FUNCTION_NAME) ;
    bool bRet = false ;
    return bRet ;
}

SVERROR FirewallValidator::fix()
{
    DebugPrintf(SV_LOG_DEBUG, "ENETERD %s\n", FUNCTION_NAME) ;    
    return SVS_FALSE ;
}



SVERROR FirewallValidator::evaluate()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    SVERROR bRet = SVE_FAIL ;
    InmRuleStatus ruleStatus = INM_RULESTAT_NONE ;
	std::stringstream stream;
    InmRuleErrorCode errorCode = INM_ERROR_NONE ;
    if( m_currentStatus == m_requiredStatus )
        {
            if( FIREWALL_STAT_DISABLED == m_requiredStatus )
            {
			    stream << "Firewall status disabled\n";
            }
            else
            {
                stream << "Firewall status is enabled\n";
            }
            bRet = SVS_OK ;
            ruleStatus = INM_RULESTAT_PASSED ;
			errorCode = RULE_PASSED ;
        }
        else
        {
            ruleStatus = INM_RULESTAT_FAILED ;
			stream << "Failed to get Firewall status\n";
			errorCode = FIREWALL_STATUS_CHECK_ERROR;
			
        }
    setRuleMessage(stream.str()) ;
    setRuleExitCode(errorCode) ;
    setStatus(ruleStatus) ;
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}

DynamicDNSUpdateValidator::DynamicDNSUpdateValidator(const std::string& name, 
                                                     const std::string& description,
                                                                                            InmDnsUpdateType currentUpdateType, 
                                                                                            InmDnsUpdateType requiredUpdateType, 
                                                     InmRuleIds ruleId)
:Validator(name, description, ruleId),
m_currentUpdateType(currentUpdateType),
m_requiredUpdateType(requiredUpdateType)
{
}
DynamicDNSUpdateValidator::DynamicDNSUpdateValidator(InmDnsUpdateType currentUpdateType, 
                                                                                            InmDnsUpdateType requiredUpdateType, 
                                                     InmRuleIds ruleId)
:Validator(ruleId),
m_currentUpdateType(currentUpdateType),
m_requiredUpdateType(requiredUpdateType)
{
}
bool DynamicDNSUpdateValidator::canfix()
{
    DebugPrintf(SV_LOG_DEBUG, "ENETERD %s\n", FUNCTION_NAME) ;
    bool bRet = true ;
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}

SVERROR DynamicDNSUpdateValidator::fix()
{
    DebugPrintf(SV_LOG_DEBUG, "ENETERD %s\n", FUNCTION_NAME) ;
    SVERROR bRet = SVS_FALSE ;
    InmRuleStatus ruleStatus = INM_RULESTAT_NONE ;
    InmRuleErrorCode errorCode = INM_ERROR_NONE ; 
    std::stringstream stream ;
    if( canfix() )
    {
       int disablednsUpdate = 0 ;
       if( m_requiredUpdateType == DNS_UPDATE_TYPE_NONDYNAMIC )
       {
             disablednsUpdate = 1 ;
       }
       if(setDynamicDNSUpdateKey(disablednsUpdate) != SVS_OK)
        {
            stream <<"Dynamic DNS Update Check failed for one/more network interfaces\n"; 
            ruleStatus = INM_RULESTAT_FAILED ;
            errorCode = DYNAMIC_DNS_UPDATE_ERROR ;
        }
        else
        {
            stream <<"Dynamic DNS Update Check Passed for all network interfaces\n"; 
            ruleStatus = INM_RULESTAT_FIXED ;
            errorCode = RULE_PASSED ;
            bRet = SVS_OK ;
        }
        setStatus(ruleStatus) ;
        setRuleExitCode(errorCode) ;
        setRuleMessage(stream.str()) ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}


SVERROR DynamicDNSUpdateValidator::evaluate()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    SVERROR bRet = SVE_FAIL ;
    InmRuleStatus ruleStatus = INM_RULESTAT_NONE ;
    InmRuleErrorCode errorCode = INM_ERROR_NONE ; 
    std::stringstream stream ;


    if( m_currentUpdateType == m_requiredUpdateType )
    {
        stream <<"Dynamic DNS Update Check Passed for all network interfaces\n"; 
        ruleStatus = INM_RULESTAT_PASSED ;
		errorCode = RULE_PASSED ;
        bRet = SVS_OK ;
    }
    else
    {
        stream <<"Dynamic DNS Update Check failed for one/more network interfaces\n"; 
        ruleStatus = INM_RULESTAT_FAILED ;
        errorCode = DYNAMIC_DNS_UPDATE_ERROR ;
    }
    setRuleMessage(stream.str());
    setRuleExitCode(errorCode) ;
    setStatus( ruleStatus ) ;
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}


DNSUpdatePrivilageValidator::DNSUpdatePrivilageValidator(const std::string& name, 
                                                         const std::string& description,
                                                         const std::string & hostnameforPermissionChecks,
                                                         InmRuleIds ruleId)
:Validator(name, description, ruleId),
m_hostnameforPermissionChecks(hostnameforPermissionChecks)
{
}

DNSUpdatePrivilageValidator::DNSUpdatePrivilageValidator(const std::string & hostnameforPermissionChecks, InmRuleIds ruleId)
:Validator(ruleId),
m_hostnameforPermissionChecks(hostnameforPermissionChecks)
{
}

bool DNSUpdatePrivilageValidator::canfix()
{
    bool bRet = false ;
    return bRet ;
}

SVERROR DNSUpdatePrivilageValidator::fix()
{
    SVERROR retStatus = SVS_FALSE ;
    return retStatus ;	
}
SVERROR DNSUpdatePrivilageValidator::evaluate()
{
	DebugPrintf(SV_LOG_DEBUG, "ENETERD %s\n", FUNCTION_NAME) ;
    SVERROR retStatus = SVS_OK ;
    HANDLE h = NULL ;
    readAndChangeServiceLogOn(h) ;

	DNS_STATUS dnsStatus = ERROR_SUCCESS ;
	PIP4_ARRAY aipServers = NULL; 
	PDNS_RECORD ppBypassedQueryResults = NULL;
	PDNS_RECORD ppStandardQueryResults = NULL;
    InmRuleStatus ruleStatus = INM_RULESTAT_NONE ;
    InmRuleErrorCode errorCode = INM_ERROR_NONE ;
	std::stringstream stream;
    std::string dependentRule ;
    if( isDependentRuleFailed(dependentRule) )
    {
        stream <<  "Skipping this rule as dependent rule : " << dependentRule << " failed " << std::endl;
        ruleStatus = INM_RULESTAT_SKIPPED ;
    }
    else
    {
        ADInterface ad;
        dnsStatus = ad.CheckDNSPermissions(m_hostnameforPermissionChecks);
        if(dnsStatus != ERROR_SUCCESS)
        {
            DebugPrintf(SV_LOG_ERROR, "Failed to check DNS update permissions: %08X \n", dnsStatus);
		    stream << "Failed to check DNS update permissions for " <<  m_hostnameforPermissionChecks  << "Error Code: " << dnsStatus  << std::endl ;
		    errorCode = DNS_SERVER_QUERY_ERROR ;
            ruleStatus = INM_RULESTAT_FAILED ;
        }
        else
        {
            ruleStatus = INM_RULESTAT_PASSED  ;
            DebugPrintf(SV_LOG_DEBUG, " This User has Sufficient Privilege for DNS Update. \n");
            stream << "This User has Sufficient Privilege for DNS Update for the entry "<< m_hostnameforPermissionChecks << std::endl  ;
            errorCode = RULE_PASSED ;
        }
    }
	setRuleMessage(stream.str());
    setStatus(ruleStatus) ;
    setRuleExitCode(errorCode) ;
	revertToOldLogon();
    if( h != NULL )
    {
        if( CloseHandle(h) == FALSE )
        {
            DebugPrintf(SV_LOG_ERROR, "Failed while closing User Token Handle %ld\n", GetLastError()) ;
        }
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return retStatus ;	
}



ADUpdatePrivilageValidator::ADUpdatePrivilageValidator(const std::string& name, 
                                                       const std::string& description,
                                                       const std::string & hostnameforPermissionChecks, 
                                                       InmRuleIds ruleId)
:Validator(name, description, ruleId),
m_hostnameforPermissionChecks(hostnameforPermissionChecks)
{
}

ADUpdatePrivilageValidator::ADUpdatePrivilageValidator(const std::string & hostnameforPermissionChecks, InmRuleIds ruleId)
:Validator(ruleId),
m_hostnameforPermissionChecks(hostnameforPermissionChecks)
{
}

bool ADUpdatePrivilageValidator::canfix()
{
    DebugPrintf(SV_LOG_DEBUG, "ENETERD %s\n", FUNCTION_NAME) ;
    bool bRet = false ;
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}

SVERROR ADUpdatePrivilageValidator::fix()
{
    DebugPrintf(SV_LOG_DEBUG, "ENETERD %s\n", FUNCTION_NAME) ;
    SVERROR retStatus = SVS_FALSE ;
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return retStatus ;	
}

SVERROR ADUpdatePrivilageValidator::evaluate()
{
	DebugPrintf(SV_LOG_DEBUG, "ENETERD %s\n", FUNCTION_NAME) ;
    SVERROR retStatus = SVS_FALSE ;
	std::stringstream ruleResultStream;
	ldapConnection ldapconn("");
	std::string statusString; 
    std::string dependentRule ;
    HANDLE h = NULL ;
    readAndChangeServiceLogOn(h) ;
	/*if( isDependentRuleFailed(dependentRule) )
	{
		ruleResultStream <<  "Skipping this rule as dependent rule : " << dependentRule << " failed " << std::endl;
		setStatus(INM_RULESTAT_SKIPPED) ;
	}
	else
	{*/
	    if( ldapconn.checkADPrivileges(m_hostnameforPermissionChecks, statusString) == SVS_OK )
	    {
		    setStatus(INM_RULESTAT_PASSED) ;
		    setRuleExitCode(RULE_PASSED);
	    }
	    else
	    {
		    setStatus(INM_RULESTAT_FAILED) ;	
		    setRuleExitCode(AD_PRIVILEGES_CHECK_FAILED_ERROR);
	    }
    //}
	setRuleMessage(statusString);
	revertToOldLogon();
    if( h != NULL )
    {
        if( CloseHandle(h) == FALSE )
        {
            DebugPrintf(SV_LOG_ERROR, "Failed while closing User Token Handle %ld\n", GetLastError()) ;
        }
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return retStatus ;	
}

MultipleNwAdaptersRule::MultipleNwAdaptersRule(const std::string& name, 
                                                const std::string& description, 
                                                InmRuleIds ruleId)
:Validator(name, description, ruleId)
{
}

MultipleNwAdaptersRule::MultipleNwAdaptersRule(InmRuleIds ruleId)
:Validator(ruleId)
{
}
SVERROR MultipleNwAdaptersRule::evaluate()
{
    WMISysInfoImpl sysInfoImpl ;
    std::list<NetworkAdapterConfig> list1 ;
    SVERROR bRet = SVS_FALSE ;
	 std::stringstream stream ;
    if( sysInfoImpl.loadNetworkAdapterConfiguration(list1 ) == SVS_OK )
    {
        bRet = SVS_OK ;
        if(list1.size() > 1 )
        {
            stream << "DR Fail over might be tricky as machine has " <<  (unsigned long)list1.size()<<" network adapters." <<std::endl ;
            stream << "Machine should have only one active network adapter" << std::endl ;
            setStatus(INM_RULESTAT_FAILED) ;
			setRuleMessage(stream.str());
			setRuleExitCode(MULTI_NIC_VALIDATION_ERROR);
        }
        else
        {
            setStatus(INM_RULESTAT_PASSED) ;
			stream << "Machine has one NIC"<< std::endl;
			setRuleMessage(stream.str());
			setRuleExitCode(RULE_PASSED);
        }
    }
    else
    {
        DebugPrintf(SV_LOG_ERROR, "loadNetworkAdapterConfiguration Faild\n") ;
        setStatus(INM_RULESTAT_FAILED) ;
		stream << "loadNetworkAdapterConfiguration Failed "<< std::endl;
		setRuleMessage(stream.str());
		setRuleExitCode(MULTI_NIC_VALIDATION_ERROR);
    }
    return bRet ;
}

SVERROR MultipleNwAdaptersRule::fix()
{
    return SVS_FALSE ;
}
bool MultipleNwAdaptersRule::canfix()
{
    return SVS_FALSE ;
}
bool DNSAvailabilityValidator::canfix()
{
    DebugPrintf(SV_LOG_DEBUG, "ENETERD %s\n", FUNCTION_NAME) ;
    bool bRet = false ;
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}


SVERROR DNSAvailabilityValidator::fix()
{
    DebugPrintf(SV_LOG_DEBUG, "ENETERD %s\n", FUNCTION_NAME) ;
    SVERROR bRet = SVS_FALSE ;
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}
DNSAvailabilityValidator::DNSAvailabilityValidator(const std::string& name, 
                                                const std::string& description, 
                                                InmRuleIds ruleId)
:Validator(name, description, ruleId)
{
}

DNSAvailabilityValidator::DNSAvailabilityValidator(InmRuleIds ruleId)
:Validator(ruleId)
{
}


SVERROR DNSAvailabilityValidator::evaluate()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    SVERROR retStatus = SVS_FALSE ;
	std::string szrecordsOwner = Host::GetInstance().GetHostName();

	DNS_STATUS dnsStatus = ERROR_SUCCESS ;
	PIP4_ARRAY aipServers = NULL; 
	PDNS_RECORD ppBypassedQueryResults = NULL;
	PDNS_RECORD ppStandardQueryResults = NULL;
    InmRuleStatus ruleStatus = INM_RULESTAT_NONE ;
    InmRuleErrorCode errorCode = RULE_PASSED ;
	std::stringstream stream;
   
	dnsStatus = DnsQuery( szrecordsOwner.c_str(), DNS_TYPE_A, DNS_QUERY_BYPASS_CACHE, aipServers, &ppBypassedQueryResults, NULL );

	if( dnsStatus != ERROR_SUCCESS )
	{
		DebugPrintf(SV_LOG_ERROR, "Failed to Query DNS server over BYPASS. Error Code: %08X \n", dnsStatus);
		stream << "Failed to Query DNS server over BYPASS.  Error Code: " << dnsStatus ;
		errorCode = DNS_AVAILABILITY_ERROR ;
        ruleStatus = INM_RULESTAT_FAILED ;
	}
	else
	{
		dnsStatus = DnsQuery( szrecordsOwner.c_str(), DNS_TYPE_A, DNS_QUERY_STANDARD, aipServers, &ppStandardQueryResults, NULL );
		if( dnsStatus != ERROR_SUCCESS )
		{
			DebugPrintf(SV_LOG_ERROR, "Failed to Query DNS server over Redirect(standard way) %08X\n", dnsStatus);
			stream << "Failed to Query DNS server over Redirect(standard way). Error Code:  " << dnsStatus ;
			errorCode =  DNS_AVAILABILITY_ERROR;
			ruleStatus = INM_RULESTAT_FAILED ;
		}
		else
		{
			DebugPrintf(SV_LOG_DEBUG, "Successfully Connect to DNS Server\n");
			stream << "Successfully Connect to DNS Server";
			ruleStatus = INM_RULESTAT_PASSED ;
			retStatus = S_OK;
		}
	}
	if(ppBypassedQueryResults)
		DnsRecordListFree(ppBypassedQueryResults, DnsFreeRecordListDeep);
	if(ppStandardQueryResults)
		DnsRecordListFree(ppStandardQueryResults, DnsFreeRecordListDeep);
	setRuleMessage(stream.str());
	setRuleExitCode(errorCode);
	setStatus(ruleStatus) ;
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return retStatus ;
}

DCAvailabilityValidator::DCAvailabilityValidator(const std::string& name, 
                                                const std::string& description, 
                                                InmRuleIds ruleId)
:Validator(name, description, ruleId)
{
}

DCAvailabilityValidator::DCAvailabilityValidator(InmRuleIds ruleId)
:Validator(ruleId)
{
}

bool DCAvailabilityValidator::canfix()
{
    DebugPrintf(SV_LOG_DEBUG, "ENETERD %s\n", FUNCTION_NAME) ;
    bool bRet = false ;
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}
SVERROR DCAvailabilityValidator::fix()
{
    DebugPrintf(SV_LOG_DEBUG, "ENETERD %s\n", FUNCTION_NAME) ;
    SVERROR bRet = SVS_FALSE ;
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}
SVERROR DCAvailabilityValidator::evaluate()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    SVERROR bRet = SVS_FALSE ;
	std::stringstream ruleResultStream;
	ldapConnection ldapconn("");
	std::string statusString;
	InmRuleErrorCode errorCode = RULE_PASSED ;
	InmRuleStatus ruleStatus = INM_RULESTAT_NONE ;
	if(ldapconn.init() == SVS_OK && 
	   ldapconn.ConnectToServer() == SVS_OK )
	{
		DebugPrintf(SV_LOG_WARNING, "Successfully connect to  DomainController\n");
		ruleResultStream << "Successfully connect to  DomainController";
		ruleStatus = INM_RULESTAT_PASSED ;
	}
	else
	{
		DebugPrintf(SV_LOG_ERROR, "Failed to connect DomainController\n");
		ruleResultStream << "Failed to connect DomainController";
		errorCode = DC_AVAILABILITY_ERROR;
		ruleStatus = INM_RULESTAT_FAILED ;
	}
	setRuleMessage(ruleResultStream.str());
	setRuleExitCode(errorCode);
	setStatus(ruleStatus) ;
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}

HostDCCheckValidator::HostDCCheckValidator(const std::string& name, 
                                                const std::string& description, 
                                                InmRuleIds ruleId)
:Validator(name, description, ruleId)
{
}

HostDCCheckValidator::HostDCCheckValidator(InmRuleIds ruleId)
:Validator(ruleId)
{
}
bool HostDCCheckValidator::canfix()
{
    DebugPrintf(SV_LOG_DEBUG, "ENETERD %s\n", FUNCTION_NAME) ;
    bool bRet = false ;
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}


SVERROR HostDCCheckValidator::fix()
{
	DebugPrintf(SV_LOG_DEBUG, "ENETERD %s\n", FUNCTION_NAME) ;
	SVERROR bRet = SVS_FALSE ;
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return bRet ;
}

SVERROR HostDCCheckValidator::evaluate()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    SVERROR bRet = SVS_FALSE ;
	std::stringstream ruleResultStream;
	std::string hostname = Host::GetInstance().GetHostName();
	ldapConnection ldapconn(hostname);
	InmRuleErrorCode errorCode = RULE_PASSED ;
	InmRuleStatus ruleStatus = INM_RULESTAT_NONE ;
	std::string statusString; 
    std::string dependentRule ;
    if( isDependentRuleFailed(dependentRule) )
    {
        ruleResultStream <<  "Skipping this rule as dependent rule : " << dependentRule << " failed " << std::endl;
        setStatus(INM_RULESTAT_SKIPPED) ;
    }
    else
    {	    
	    if(ldapconn.init() == SVS_OK && 
	       ldapconn.ConnectToServer() == SVS_OK )
	    {
            DebugPrintf(SV_LOG_ERROR, "Successfully connect to  DomainController on %s. \n", hostname.c_str());
            ruleResultStream << "Successfully connect to  Domain Controller on " << hostname << std::endl ;
            ruleResultStream << "Application Cannot be failed over if the application host itself is a Domain Controller  " << hostname << std::endl ;
		    errorCode = HOST_DC_ERROR;
		    ruleStatus = INM_RULESTAT_FAILED ;
	    }
	    else
	    {
		    DebugPrintf(SV_LOG_DEBUG, "Failed to connect DomainController\n");
		    ruleResultStream << "Failed to connect DomainController.This host is not DC";
		    ruleStatus = INM_RULESTAT_PASSED ;
	    }
    }
	setRuleMessage(ruleResultStream.str());
	setRuleExitCode(errorCode);
	setStatus(ruleStatus) ;
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}

CacheDirCapacityCheckValidator::CacheDirCapacityCheckValidator(const std::string& name, 
                                       const std::string& description, 
                                       InmRuleIds ruleId)
:Validator(name, description, ruleId)
{
}

CacheDirCapacityCheckValidator::CacheDirCapacityCheckValidator(InmRuleIds ruleId)
:Validator(ruleId)

{
}

SVERROR CacheDirCapacityCheckValidator::evaluate()
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	SVERROR bRet = SVS_FALSE ;
	InmRuleStatus ruleStatus = INM_RULESTAT_NONE ;
	InmRuleErrorCode ruleErrorCode = RULE_STAT_NONE;
	std::stringstream resultStram ;
	std::string cachedir = "";
	SV_ULONGLONG capacity = 0, freeSpace = 0, expectedFreeSpace = 0;
	if(CheckCacheDirSpace(cachedir, capacity, freeSpace, expectedFreeSpace) == true)
	{
		DebugPrintf(SV_LOG_DEBUG,"Cache Drive has enough free sapce\n");
		resultStram << "Cache Drive has enough free sapce. "<< std::endl ;
		ruleStatus = INM_RULESTAT_PASSED;
		ruleErrorCode = RULE_PASSED;
		bRet = SVS_OK;
	}
	else
	{
		DebugPrintf(SV_LOG_DEBUG,"Cache Drive does not have enough free sapce\n");
		resultStram << "Cache Drive does not have enough free sapce. " << std::endl ;
		ruleStatus = INM_RULESTAT_FAILED;
		ruleErrorCode = CACHE_DIR_CAPACITIY_ERROR;
	}
	resultStram << "Cache Directory: " << cachedir << ", " << std::endl;
	resultStram << "Total Drive Capacity: " << capacity << " B, " << std::endl;
	resultStram << "Available Drive FreeSpace: " << freeSpace << " B, " << std::endl;
	resultStram << "Expected Minimum Drive Free Space: " << expectedFreeSpace << " B, " << std::endl;
	float freeSpacePercent = 0, expectedFreeSpacePercet = 0;
	if(capacity != 0)
	{
		freeSpace = freeSpace*100;
		expectedFreeSpace = expectedFreeSpace*100;
		freeSpacePercent = freeSpace/capacity;
		expectedFreeSpacePercet = expectedFreeSpace/capacity;
		resultStram << "Available FreeSpace : " << freeSpacePercent << "% " << std::endl;
		resultStram << "Expected Minimum FreeSpace : " << expectedFreeSpacePercet << "%" << std::endl;
	}
	setStatus(ruleStatus) ;
	setRuleExitCode(ruleErrorCode);
	setRuleMessage(resultStram.str());
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return bRet ;
}

bool CacheDirCapacityCheckValidator::canfix()
{
    return false ;
}


SVERROR CacheDirCapacityCheckValidator::fix()
{
    return SVS_FALSE ;
}

CMMinSpacePerPairCheckValidator::CMMinSpacePerPairCheckValidator(const std::string& name, 
                                       const std::string& description,
									   const SV_ULONGLONG& totalNumberOfPairs,
                                       InmRuleIds ruleId)
:Validator(name, description, ruleId),
m_totalNumberOfPairs(totalNumberOfPairs)
{
}

CMMinSpacePerPairCheckValidator::CMMinSpacePerPairCheckValidator(const SV_ULONGLONG& totalNumberOfPairs, InmRuleIds ruleId)
:Validator(ruleId),
m_totalNumberOfPairs(totalNumberOfPairs)
{
}

SVERROR CMMinSpacePerPairCheckValidator::evaluate()
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	SVERROR bRet = SVS_FALSE ;
	InmRuleStatus ruleStatus = INM_RULESTAT_NONE ;
	InmRuleErrorCode ruleErrorCode = RULE_STAT_NONE;
	std::stringstream resultStram ;
	std::string cachedir = "";
	SV_ULONGLONG capacity = 0, freeSpace = 0, expectedFreeSpace = 0, totalSpaceForCM = 0, totalCMSpaceReq = 0;
	DebugPrintf(SV_LOG_DEBUG, "The Total Number of Pairs for which need to check the Cache Memory: %ld\n", m_totalNumberOfPairs) ;
	
	if(checkCMMinReservedSpacePerPair(cachedir, capacity, freeSpace, expectedFreeSpace, m_totalNumberOfPairs, totalSpaceForCM, totalCMSpaceReq) == true)
	{
		DebugPrintf(SV_LOG_DEBUG,"Cache drive has enough free space which is more/equal to minimum  reserved space per pair\n");
		resultStram << "Cache drive has enough free space which is more/equal to minimum  reserved space per pair."<< std::endl ;
		ruleStatus = INM_RULESTAT_PASSED;
		ruleErrorCode = RULE_PASSED;
		bRet = SVS_OK;
	}
	else
	{
		DebugPrintf(SV_LOG_DEBUG,"Cache drive does not have enough free space which is more/equal to minimum  reserved space per pair\n");
		resultStram << "Cache drive does not have enough free space which is more/equal to minimum  reserved space per pair." << std::endl ;
		ruleStatus = INM_RULESTAT_FAILED;
		ruleErrorCode = CM_MIN_SPACE_PER_PAIR_ERROR;
	}
	resultStram << "Cache Directory: " << cachedir << ", " << std::endl;
	resultStram << "Total Drive Capacity: " << capacity << " B, " << std::endl;
	resultStram << "Available Drive FreeSpace: " << freeSpace << " B, " << std::endl;
	resultStram << "Expected Minimum Reserved Space per pair: " << expectedFreeSpace << " B, " << std::endl;
	resultStram << "The Total Number of Pairs: " << m_totalNumberOfPairs << ", " << std::endl;
	resultStram << "Total Cache Memory Required for all the pairs: " << totalCMSpaceReq << " B, " << std::endl;
	resultStram << "Total Cache Memory Available including consumed Space: " << totalSpaceForCM << " B, " << std::endl;

	setStatus(ruleStatus) ;
	setRuleExitCode(ruleErrorCode);
	setRuleMessage(resultStram.str());
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return bRet ;
}

bool CMMinSpacePerPairCheckValidator::canfix()
{
    return false ;
}


SVERROR CMMinSpacePerPairCheckValidator::fix()
{
    return SVS_FALSE ;
}

IPEqualityValidator::IPEqualityValidator(const std::string& name,
						const std::string& description, 
						const std::string& dnsrecordName1, 
						const std::string& dnsrecordName2,
						const bool& bShouldEqual,
						InmRuleIds ruleId)
:Validator(name, description, ruleId),
m_dnsrecordName1(dnsrecordName1),
m_dnsrecordName2(dnsrecordName2),
m_bShouldEqual(bShouldEqual)
{
}

IPEqualityValidator::IPEqualityValidator(const std::string& dnsrecordName1, 
										 const std::string& dnsrecordName2,
										 const bool& bShouldEqual,
										 InmRuleIds ruleId)
:Validator(ruleId),
m_dnsrecordName1(dnsrecordName1),
m_dnsrecordName2(dnsrecordName2),
m_bShouldEqual(bShouldEqual)
{
}

SVERROR IPEqualityValidator::evaluate()
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
	SVERROR retStatus = SVS_FALSE;
    ADInterface ad ;
    bool same ;
    SVERROR bret ;
    bret = ad.CompareIPs(m_dnsrecordName1, m_dnsrecordName2, same) ;
    InmRuleStatus status = INM_RULESTAT_FAILED ;
    InmRuleErrorCode ruleStatus = INM_MISMATCH_ERROR;
    std::stringstream resultStram ;
    resultStram << "Checking the IP Addresses for the following records in dns " << std::endl ;
    resultStram << "Record 1: " << m_dnsrecordName1<< std::endl ;
    resultStram << "Record 2: " << m_dnsrecordName2<< std::endl ;
    if( bret == SVS_OK )
    {
        if( same )
        {
            resultStram << "Both the IP Addreess are matched with each other in DNS."<< std::endl;
            if(m_bShouldEqual)
            {
                status = INM_RULESTAT_PASSED;
                ruleStatus = RULE_PASSED;			
            }
        }
        else
        {
            resultStram << "Both the IP Addreess are differnt from each other in DNS.";
            if(!m_bShouldEqual)
            {
                status = INM_RULESTAT_PASSED;
                ruleStatus = RULE_PASSED;			
            }
        }
    }
    else
    {
        resultStram << "Failed to query the IP Addresses from DNS" << std::endl ;
    }
	setStatus(status) ;
	setRuleExitCode(ruleStatus);
	setRuleMessage(resultStram.str());
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
	return retStatus;
}

bool IPEqualityValidator::canfix()
{
    return false ;
}

SVERROR IPEqualityValidator::fix()
{
    return SVS_FALSE ;
}

VSSRollupCheckValidator::VSSRollupCheckValidator(const std::string& name,
						const std::string& description, 
						InmRuleIds ruleId)
:Validator(name, description, ruleId)
{
}

VSSRollupCheckValidator::VSSRollupCheckValidator(InmRuleIds ruleId)
:Validator(ruleId)
{
}

SVERROR VSSRollupCheckValidator::evaluate()
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
	SVERROR retStatus = SVS_FALSE;
	InmRuleStatus status = INM_RULESTAT_PASSED ;
	std::stringstream resultStram ;
	InmRuleErrorCode ruleStatus = RULE_PASSED;
	OsInfo osInfo;
	if(GetSystemBootInfo(osInfo) == SVS_OK)
	{
		DWORD dwosMajorVersion = osInfo.m_osMajorVersion;
		DWORD dwosMinorVersion = osInfo.m_osMinorVersion;
		WORD wServicePackMajorVersion = osInfo.m_wServicePackMajorVersion;
		WORD wServicePackMinorVersion = osInfo.m_wServicePackMinorVersion;
		if((dwosMajorVersion == 5 && dwosMinorVersion == 2))
		{
			if(wServicePackMajorVersion < 2)
			{	
				resultStram << "Service Pack 2 should be minimal for Windows 2003 server to improve the reliability, scalability, and memory optimization of the Volume Shadow Copy Service (VSS).";
				status = INM_RULESTAT_FAILED;
				ruleStatus = VSS_ROLEUP_ERROR;
			}
			else if(isfound(osInfo.m_installedHotFixsList, "KB940349-v3") == false)
			{
				resultStram << "Windows KB940349 Update Should be installed for Windows 2003 server to improve the reliability, scalability, and memory optimization of the Volume Shadow Copy Service (VSS).";
				status = INM_RULESTAT_FAILED;
				ruleStatus = VSS_ROLEUP_ERROR;							
			}
			else
			{
				resultStram << "VSS rollup check is successful.";
			}
		}
		else
		{
			status = INM_RULESTAT_SKIPPED;
			resultStram.str("VSS Rollup Check is skipped. Because this is not Windows 2003 Server.");
		}
	}
	setStatus(status) ;
	setRuleExitCode(ruleStatus);
	setRuleMessage(resultStram.str());
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
	return retStatus;
}

bool VSSRollupCheckValidator::canfix()
{
    return false ;
}

SVERROR VSSRollupCheckValidator::fix()
{
    return SVS_FALSE ;
}