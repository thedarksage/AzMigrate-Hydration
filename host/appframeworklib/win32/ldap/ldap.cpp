#include "appglobals.h"
#include "ldap.h"
#include <Winber.h>
#include <sstream>
#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>
#include "appexception.h"
#include "Adinterface.h"
#include "inmsafeint.h"
#include "inmageex.h"

SVERROR ldapConnection::InitSession()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    SVERROR InmRet = SVS_FALSE ;
    if( m_dnsname.compare("") == 0 )
    {
        m_ldap = ldap_init(NULL, LDAP_PORT) ;
    }
    else
    {
        m_ldap = ldap_init((PCHAR)m_dnsname.c_str(), LDAP_PORT) ;
    }
    if( m_ldap != NULL )
    {
        InmRet = SVS_OK ;
    }
    else
    {
        DebugPrintf(SV_LOG_ERROR, "ldap_init failed with error 0x%x\n", LdapGetLastError()) ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return InmRet ;
}
SVERROR ldapConnection::InitSSLSession()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    SVERROR InmRet = SVS_FALSE ;
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return InmRet ;
}
SVERROR ldapConnection::Bind(const std::string& dn)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    SVERROR InmRet = SVS_FALSE ;
    int iRtn = 0 ;
	m_configDN = dn;
    if( (iRtn = ldap_bind_s(m_ldap, (PCHAR)dn.c_str(), NULL, LDAP_AUTH_NEGOTIATE)) == LDAP_SUCCESS )
    {
        InmRet = SVS_OK ;
		m_bBinded = true;
    }
    else
    {
        DebugPrintf(SV_LOG_ERROR, "ldap_bind_s failed. Error 0x%x\n", iRtn ) ;
        throw "Failed to authenticates a client to the LDAP server\n" ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return InmRet ;
}

SVERROR ldapConnection::SetOption(int option, void* buffer)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    SVERROR InmRet = SVS_FALSE ;
    int iRtn = 0 ;
    if( (iRtn = ldap_set_option(m_ldap, option, buffer)) == LDAP_SUCCESS )
    {
        InmRet = SVS_OK ;
    }
    else
    {
        DebugPrintf(SV_LOG_ERROR, "ldap_set_option failed. Error:%0X\n", iRtn);
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return InmRet ;
}
ldapConnection::~ldapConnection()
{
    if( m_ldap )
    {
        ldap_unbind_s(m_ldap) ;
    }
    m_ldap = NULL ;
}
ldapConnection::ldapConnection(const std::string& dnsname)
{
    m_dnsname = dnsname ;
    m_ldap = NULL ;
	m_configDN = "";
	m_bBinded = false;
}

SVERROR ldapConnection::init()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    SVERROR InmRet = SVS_FALSE ;
    DebugPrintf(SV_LOG_DEBUG, "Initializing Session\n") ;
    if( InitSession() == SVS_OK )
    {
        DebugPrintf(SV_LOG_DEBUG, "InitSession is successful\n") ;
        SV_ULONG version = LDAP_VERSION3;
        if( SetOption(LDAP_OPT_PROTOCOL_VERSION, (void*)&version) == SVS_OK )
        {
            InmRet = SVS_OK ;
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR, "SetOption Failed\n") ;
        }
    }
    else
    {
        DebugPrintf(SV_LOG_ERROR, "InitSession Failed\n") ; 
    }
    if( InmRet  != SVS_OK )
    {
        throw "Failed to initialize the ldap connection object\n" ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return InmRet ;
}
SVERROR ldapConnection::ConnectToServer()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    SVERROR InmRet = SVS_FALSE ;
    int iRtn = 0 ;
    if( (iRtn = ldap_connect(m_ldap, NULL)) == LDAP_SUCCESS )
    {
        InmRet  = SVS_OK ;
    }
    else
    {
        ldap_unbind_s(m_ldap) ;
        m_ldap = NULL ;
        DebugPrintf(SV_LOG_ERROR, "ldap_connect failed. Error 0x%x\n", iRtn ) ;
        if( m_dnsname.compare("") != 0 )
        {
            DebugPrintf(SV_LOG_ERROR, "The above error can be ignored if the %s doesn't host a domain controller\n", m_dnsname.c_str()) ;
        }
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return InmRet ;
}

SVERROR ldapConnection::Modify(ULONG operation, const std::string& userDn, const std::string& attrname, const std::list<std::string>& modValsList)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    SVERROR InmRet = SVS_FALSE ;
    LDAPMod ** InmLdapMods ;
    std::list<std::string>::size_type listsize;
    INM_SAFE_ARITHMETIC(listsize = InmSafeInt<std::list<std::string>::size_type>::Type(modValsList.size()) + 1, INMAGE_EX(modValsList.size()))
    size_t modslen;
    INM_SAFE_ARITHMETIC(modslen = InmSafeInt<size_t>::Type(sizeof(LDAPMod*)) * listsize, INMAGE_EX(sizeof(LDAPMod*))(listsize))
    InmLdapMods = (LDAPMod**) malloc(modslen);
    std::list<std::string>::const_iterator iter = modValsList.begin() ;
    size_t i = 0 ;
    for(; i < modValsList.size() ; i++)
    {
        InmLdapMods[i] = (LDAPMod*)iter->c_str();
        iter++ ;
    }
    InmLdapMods[i] = NULL ;
    if (ldap_modify_s(m_ldap, (PCHAR)userDn.c_str(), InmLdapMods) == LDAP_SUCCESS) 
    {
        InmRet = SVS_OK; 
    }
    else
    {
        DebugPrintf(SV_LOG_ERROR, "Modify Failed\n" ) ;
    }
    free(InmLdapMods) ;
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return InmRet ;
}

SVERROR ldapConnection::Search(const std::string& base, ULONG scope, const std::string& filter, const std::list<std::string>& attrsList, std::list<std::multimap<std::string, std::string > >& resultSet)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    SVERROR InmRet = SVS_FALSE ;
	
USES_CONVERSION;
    
	LDAPMessage* inm_ldp_msg = NULL;
    char** ldp_obj_attrs = NULL ;
    std::list<std::string>::size_type listsize;
    INM_SAFE_ARITHMETIC(listsize = InmSafeInt<std::list<std::string>::size_type>::Type(attrsList.size()) + 1, INMAGE_EX(attrsList.size()))
    size_t attrslen;
    INM_SAFE_ARITHMETIC(attrslen = InmSafeInt<size_t>::Type(sizeof(char*)) * listsize, INMAGE_EX(sizeof(char*))(listsize))
    ldp_obj_attrs = (char**) malloc(attrslen);
    std::list<std::string>::const_iterator attrIter = attrsList.begin() ;
    size_t i = 0 ;
    for( ; i < attrsList.size() ; i++ )
    {
        ldp_obj_attrs[i] = (char*)attrIter->c_str() ;
        attrIter++ ;
    }
	int size = i;
    ldp_obj_attrs[i] = NULL ;
    DebugPrintf(SV_LOG_DEBUG, "Base : %s\n", base.c_str()) ;
    DebugPrintf(SV_LOG_DEBUG, "filter : %s\n", filter.c_str()) ;
    ULONG retVal ;
	int nSearchAttempts = 0;
	LDAP_TIMEVAL search_time;
	search_time.tv_sec = APPAGENT_LDAP_SEARCH_TIMEOUT;
	do
	{
		if ( (retVal = ldap_search_ext_s(m_ldap, (PCHAR)base.c_str(), scope, (PCHAR)filter.c_str(), (PCHAR*)ldp_obj_attrs, 0,NULL,NULL, &search_time, 0, &inm_ldp_msg) ) == LDAP_SUCCESS) 
		{
			DebugPrintf(SV_LOG_DEBUG, "The number of entries returned %d\n", ldap_count_entries(m_ldap, inm_ldp_msg)) ;
			InmRet = SVS_OK ;
			char* dn = NULL;
			LDAPMessage* inm_entry = NULL;
			char* attr = NULL;
			char** pp_inm_vals = NULL;
			BerElement* inm_ber_ele;
			std::multimap<std::string, std::string> attrMap ;
			for(inm_entry = ldap_first_entry(m_ldap, inm_ldp_msg); inm_entry != NULL; inm_entry = ldap_next_entry(m_ldap, inm_entry)) 
			{
				if((dn = ldap_get_dn(m_ldap, inm_entry)) != NULL) 
				{
					ldap_memfree(dn);
				}
				attrMap.clear() ;
				for( attr = ldap_first_attribute(m_ldap, inm_entry, &inm_ber_ele); attr != NULL; attr = ldap_next_attribute(m_ldap, inm_entry, inm_ber_ele)) 
				{
					if ((pp_inm_vals = ldap_get_values(m_ldap, inm_entry, attr)) != NULL)  
					{
						for(int i = 0; pp_inm_vals[i] != NULL; i++) 
						{
							//DebugPrintf(SV_LOG_DEBUG, "attr pair :: %s <--> %s\n", attr, pp_inm_vals[i]) ;
							if( strcmpi(attr, "objectGUID") == 0 )
							{
								berval** bVal;
								bVal = ldap_get_values_len(m_ldap, inm_entry, attr);
								std::string objectGUID;
								for(int i=0; bVal[i] != NULL; i++)
								{
									LPWSTR szwGUID = new WCHAR[39];
									LPGUID pGUID = (LPGUID)bVal[i]->bv_val;
									::StringFromGUID2(*pGUID,szwGUID,39);
									std::string strGUID(W2T(szwGUID));
									//Remove the first '{' and last '}' characters from GUID
									objectGUID = ToLower(strGUID.substr(1,strGUID.length()-2));
									delete[] szwGUID;
									attrMap.insert(std::make_pair("objectGUID", objectGUID)) ;
								}
								DebugPrintf(SV_LOG_DEBUG, "Object GUID: %s\n", objectGUID.c_str());
								ldap_value_free_len(bVal);
							}
							else
							{
								attrMap.insert(std::make_pair(attr, pp_inm_vals[i])) ;
							}
						}
						ldap_value_free(pp_inm_vals);
					}
					ldap_memfree(attr);
				}
				if (inm_ber_ele != NULL) 
				{
					ber_free(inm_ber_ele,0);
				}
				resultSet.push_back(attrMap) ;
			}
			if(inm_entry != NULL)
			{
				ldap_msgfree(inm_entry);
			}
			break;
		}
		else
		{
			nSearchAttempts++;

			if( LDAP_SERVER_DOWN == retVal ||
				LDAP_UNAVAILABLE == retVal ||
				LDAP_NO_MEMORY == retVal ||
				LDAP_BUSY == retVal ||
				LDAP_LOCAL_ERROR == retVal ||
				LDAP_TIMEOUT == retVal ||
				LDAP_OPERATIONS_ERROR == retVal ||
				LDAP_TIMELIMIT_EXCEEDED == retVal
				)
			{
				DebugPrintf(SV_LOG_ERROR, "ldap search Failed with error: %s ( 0x%x)\n", ldap_err2string(retVal), retVal) ;
				
				if(inm_ldp_msg)
				{
					ldap_msgfree(inm_ldp_msg);
					inm_ldp_msg = NULL;
				}

				if(nSearchAttempts >= MAX_LDAP_SEARCH_ATTEMPTS)
				{
					DebugPrintf(SV_LOG_ERROR, "Maximum search attempts reached... Can not perform ldap search.\n") ;
					InmRet = SVS_FALSE;
					break;
				}
				//Clean the existing connection and retry search with new connection.
				DebugPrintf(SV_LOG_INFO,"Retrying the ldap search ...\n");
				if(!retryLdapConnection())
				{
					DebugPrintf(SV_LOG_ERROR,"Can not perform ldap search.\n");
					break;
				}
			}
			else
			{
				DebugPrintf(SV_LOG_ERROR, "Non retryable error occured. Ldap search Failed with error: %s ( 0x%x)\n", ldap_err2string(retVal), retVal) ;
				InmRet = SVS_FALSE;
				if(inm_ldp_msg)
				{
					ldap_msgfree(inm_ldp_msg);
					inm_ldp_msg = NULL;
				}
				break;
			}
		}
	}while(nSearchAttempts < MAX_LDAP_SEARCH_ATTEMPTS);
    if( inm_ldp_msg != NULL )
    {
        ldap_msgfree(inm_ldp_msg);
    }
    if( ldp_obj_attrs != NULL )
    {
			free( ldp_obj_attrs ) ;
			ldp_obj_attrs = NULL ;
    }
    if( InmRet != SVS_OK )
    {
        throw "Failed to perform Ldap Query" ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return InmRet ;
}

SVERROR ldapConnection::getNamingContexts(std::list<std::string>& namingContexts)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    int oldVal = 0 ;
    SVERROR InmRet = SVS_FALSE ;
    ULONG option =  0;
    if( SetOption(LDAP_OPT_REFERRALS, &option) == SVS_OK )
    {
        std::list<std::string> attrList ;
        attrList.push_back("namingContexts");
        std::list<std::multimap<std::string, std::string> > resultSet ;
        if( Search("", LDAP_SCOPE_BASE, "(objectclass=*)", attrList, resultSet) == SVS_OK )
        {
            InmRet = SVS_OK ;
            std::list<std::multimap<std::string, std::string> >::const_iterator resultIter ;
            resultIter = resultSet.begin() ;
            while( resultIter != resultSet.end() )
            {
                std::multimap<std::string, std::string> attrMap = *resultIter ;
                std::multimap<std::string, std::string>::iterator attrMapIter= attrMap.begin() ;
                while( attrMapIter != attrMap.end() )
                {
                    namingContexts.push_back(attrMapIter->second) ;
                    //DebugPrintf(SV_LOG_DEBUG, "Naming Context: %s\n", attrMapIter->second.c_str()) ;
                    attrMapIter++ ;
                }
                resultIter++ ;
            }
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR, "Search Failed\n") ;
        }
    }
    else
    {
        DebugPrintf(SV_LOG_ERROR, "SetOption Failed %d\n", LdapGetLastError()); 
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return InmRet ;
}

SVERROR ldapConnection::getConfigurationDn(std::string& configDn)
{    
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    SVERROR InmRet = SVS_FALSE ;
    std::list<std::string> namingContextsList ;
    if( getNamingContexts(namingContextsList) == SVS_OK )
    {
        std::list<std::string>::const_iterator namingContextIter = namingContextsList.begin() ;
        while( namingContextIter != namingContextsList.end() )
        {
            if( namingContextIter->find("CN=Configuration") == 0 )
            {
                configDn = *namingContextIter ;
                InmRet = SVS_OK ;
                break ;
            }
            namingContextIter++ ;
        }
        if( configDn.compare("") == 0 )
        {
            DebugPrintf(SV_LOG_ERROR, "Failed to get the Configuration Naming Context\n") ;
        }
    }
    else
    {
        DebugPrintf(SV_LOG_ERROR, "Failed to get the naming contexts\n") ;
    }
  
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return InmRet ;
}
bool ldapConnection::retryLdapConnection()
{
	bool bConnection = true;
	if(m_ldap)
	{
		ldap_unbind(m_ldap);
		m_ldap = NULL;
	}
	ULONG ulSearchSizeLimit = 0;
	LDAP_TIMEVAL ltConnTimeout;
	ltConnTimeout.tv_sec = 0;			
	if((m_ldap = getLdapConnectionCustom(ulSearchSizeLimit, ltConnTimeout, true)) == NULL)
	{
		DebugPrintf(SV_LOG_ERROR, "Unable to connect to directory server. \n");
		bConnection = false;
	}
	
	return bConnection;
}

LDAP* ldapConnection::getLdapConnectionCustom(ULONG ulSearchSizeLimit, LDAP_TIMEVAL ltConnTimeout, bool bBind)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	LDAP* pLdapConnection = NULL;
    ULONG ulVersion = LDAP_VERSION3;           // Set to LDAP V3 instead of V2
    ULONG lRtn = 0, usConnectAttempts = 0;
	
	while(usConnectAttempts < MAX_LDAP_CONNECT_ATTEMPTS)
	{
		if( usConnectAttempts )
		{
			Sleep( 20 * 1000);
            DebugPrintf(SV_LOG_DEBUG, "Attempting to re-connect to LDAP server...\n");
		}
		pLdapConnection = ldap_init(NULL, LDAP_PORT);
		if (pLdapConnection == NULL)
		{
			ULONG ulInitErr = 0;
			ulInitErr = LdapGetLastError();
			DebugPrintf(SV_LOG_DEBUG, "ldap_init failed ");
			usConnectAttempts++;
			if(usConnectAttempts == MAX_LDAP_CONNECT_ATTEMPTS)
			{
				DebugPrintf(SV_LOG_WARNING, "Maximum connection attempts has been reached...\n");
				break;
			}
			else
			{
				continue;
			}
		}
		
		lRtn = ldap_set_option(pLdapConnection, LDAP_OPT_PROTOCOL_VERSION, (void*) &ulVersion);

		if(lRtn != LDAP_SUCCESS)
		{
			DebugPrintf(SV_LOG_ERROR, "ldap_set_option failed. \n");
			if(pLdapConnection)
			{
				ldap_unbind_s(pLdapConnection);
				pLdapConnection = NULL;
			}
			usConnectAttempts++;
			if(usConnectAttempts == MAX_LDAP_CONNECT_ATTEMPTS)
			{
				DebugPrintf(SV_LOG_WARNING, "Maximum connection attempts has been reached...\n");
				break;
			}
			else
			{
				continue;
			}
		}

		lRtn = ldap_set_option(pLdapConnection, LDAP_OPT_SIZELIMIT, (void*) &ulSearchSizeLimit);
		if(lRtn != LDAP_SUCCESS)
		{
		DebugPrintf(SV_LOG_ERROR, "ldap_set_option failed. \n");
			if(pLdapConnection)
			{
				ldap_unbind_s(pLdapConnection);
				pLdapConnection = NULL;
			}
			usConnectAttempts++;
			if(usConnectAttempts == MAX_LDAP_CONNECT_ATTEMPTS)
			{
				DebugPrintf(SV_LOG_WARNING, "Maximum connection attempts has been reached...\n");
				break;
			}
			else
			{
				continue;
			}
		}
		if( ltConnTimeout.tv_sec )
            lRtn = ldap_connect(pLdapConnection, &ltConnTimeout);
		else
            lRtn = ldap_connect(pLdapConnection, NULL);

		if(lRtn != LDAP_SUCCESS) 
		{			
			DebugPrintf(SV_LOG_ERROR, "ldap_connect failed.\n");
			if(pLdapConnection)
			{
				ldap_unbind_s(pLdapConnection);
				pLdapConnection = NULL;
			}
			usConnectAttempts++;
			if(usConnectAttempts == MAX_LDAP_CONNECT_ATTEMPTS)
			{
				DebugPrintf(SV_LOG_WARNING, "Maximum connection attempts has been reached...\n");
				break;
			}
			else
			{
				continue;
			}
		}
		if(bBind & m_bBinded)
		{			
			PCHAR pContext = NULL;
			if( !m_configDN.empty() )
			{
				pContext = (const PCHAR) m_configDN.c_str();
			}

			lRtn = ldap_bind_s(
					pLdapConnection,                 
					pContext,  
					NULL,                            
					LDAP_AUTH_NEGOTIATE);           
			if(lRtn != LDAP_SUCCESS) 
			{
				DebugPrintf(SV_LOG_ERROR, "ldap_bind_s failed. \n");
				if(pLdapConnection)
				{
					ldap_unbind_s(pLdapConnection);
					pLdapConnection = NULL;
				}
				usConnectAttempts++;
				if(usConnectAttempts == MAX_LDAP_CONNECT_ATTEMPTS)
				{
					DebugPrintf(SV_LOG_WARNING, "Maximum connection attempts has been reached...\n");
					break;
				}
				else
				{
					continue;
				}
			}
		}
		break;
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;    
	return pLdapConnection;
}

SVERROR ldapConnection::checkADPrivileges(const std::string& szSourceHost, std::string& statusString )
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	SVERROR retStatus = SVS_FALSE;
	ADInterface ad;
	
	ULONG ulResult;
	ulResult = ad.checkADPrivileges( szSourceHost );
	if( ulResult != LDAP_NO_SUCH_ATTRIBUTE && ulResult != 0 )
	{
		statusString = "Service Account has InSufficient Previliges for changing the AD entry for " ;
        statusString +=  szSourceHost  ;
        statusString +=  " Error Code: " ;
        statusString += boost::lexical_cast<std::string> (ulResult) ;
		statusString += "\n";
	}
	else
	{
		retStatus = SVS_OK;
		statusString = "Service Account has Sufficient Previliges to modify AD entry for ";
        statusString += szSourceHost ;
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return retStatus; 
}

