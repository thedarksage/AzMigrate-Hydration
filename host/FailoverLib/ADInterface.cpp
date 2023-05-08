#include "stdafx.h"
#include "ADInterface.h"
using namespace std;
#include "defs.h"
#include <time.h>
#include "hostagenthelpers.h"
#include "hostagenthelpers_ported.h"
#include "portablehelpersmajor.h"
#include "localconfigurator.h"
#include "spn.h"
#include "inmsafecapis.h"
#include "inmsafeint.h"
#include "inmageex.h"
#include <ace\Process_Manager.h>

#define CMD_FLUSH_DNS_CACHE		"ipconfig /flushdns"

const string CN                       = "CN";
const string DN                       = "DN";
const string CN_SEPARATOR_COMMA		  = ",";
const string CN_SEPARATOR_SLASH		  = "/";
const string CN_SEPARATOR_SEMICOLON	  = ";";

const string homeMTA                  = "homeMTA";
const string homeMDB                  = "homeMDB";
const string msExchPatchMDB           = "msExchPatchMDB";
const string msExchStorageGroup		  = "msExchStorageGroup";
const string msExchPrivateMDB		  = "msExchPrivateMDB";
const string msExchPublicMDB		  =	"msExchPublicMDB";
const string ResponsibleMTAServer	  =	"msExchResponsibleMTAServer";
const string deliveryMechanism		  = "deliveryMechanism";
const string OBJECT_GUID			  = "objectGUID";
const string LEGACY_EXCHANGE_DN		  = "legacyExchangeDN";
const string MAIL_GATEWAY			  = "mailGateway";
const string ATTRIBUTE_BINARY		  = "Binary";
const string ATTRIBUTE_STRING		  = "String";

#define VALID_CHAR_IN_CN_SIZE   69
char Token[VALID_CHAR_IN_CN_SIZE]={'a','b','c','d','e','f','g','h','i','j','k','l','m','n','o','p','q','r','s','t','u','v','w','x','y','z','1','2','3','4','5','6','7','8','9','0','\\','/','.',',','=',' ','!','@','#','$','%','^','&','*','(',')','{','}','[',']','-','_','|','+','~','`',':',';','"','\'','<','>','?'};
// Exchange Server related parameters
const char EXCH_INSTALL_PATH_VALUE_NAME[] = "ExchangeServerAdmin";
const char EXCH_SETUP_REG_KEY[] = "SOFTWARE\\Microsoft\\Exchange\\Setup";
const char *exchValues[] = {"exchangeMDB", "exchangeRFR", "SMTPSVC", "exchangeAB", "SMTP"};
const char *nonUserNamingContexts[] = { "DC=DomainDnsZones,DC=", "CN=Schema,CN=Configuration,DC=", "DC=ForestDnsZones,DC=" };

ADInterface::ADInterface()
{
	m_bAudit = false;
}

ADInterface::ADInterface(const string& domainName)
{
	m_domainName = domainName;
	m_bAudit = false;
}

ADInterface::~ADInterface(void)
{
}

SVERROR
ADInterface::DNSChangeIP(string szSource, string szTarget, string szIPAddress, string dnsServIp, string dnsDomain, string user, string password, bool bDNSFailover)
{
    DNS_RECORD* pSource = NULL, *pTarget = NULL, *pRedirectedSource = NULL;
    ULONG ulRet = 0;
	PIP4_ARRAY pSrvIPList = NULL;      
	HANDLE hToken = NULL;
	BOOL ccode = 0;
	BOOL impOutput = 0;

	//Need to allocate memory for IP4_ARRAY structure.
	if(!dnsServIp.empty())
	{
		pSrvIPList = (PIP4_ARRAY) LocalAlloc(LPTR,sizeof(IP4_ARRAY));
		if(!pSrvIPList){
			printf("Memory allocation failed \n");
			exit(1);
		}
		pSrvIPList->AddrCount = 1;
		pSrvIPList->AddrArray[0] = inet_addr(dnsServIp.c_str()); //DNS server IP address
	}
	
	//display DNS Server Name
	std::string dnsServerName;
	getDnsServerName(dnsServerName);
	if(!dnsServerName.empty())
		cout << endl << " DNS Server : " << dnsServerName << endl;

    ulRet = DnsQuery(szSource.c_str(),
                DNS_TYPE_A,
                DNS_QUERY_BYPASS_CACHE,
                pSrvIPList,
                &pSource,
                NULL);

    if(ulRet != 0)
    {
			cout << "Could not lookup DNS entry for:" << szSource.c_str() << " ErrorCode: " << ulRet<<" [Hexadecimal: "<<std::hex<<ulRet<<"]\n";
        goto cleanup_and_exit;
    }

	cout << szSource.c_str() << " FQDN = " << pSource->pName << "\n";

#ifdef SCOUT_WIN2K_SUPPORT
	ulRet = DnsQuery(szSource.c_str(),
                DNS_TYPE_A,
                DNS_QUERY_USE_TCP_ONLY,
                pSrvIPList,
                &pRedirectedSource,
                NULL);
#else
    ulRet = DnsQuery(szSource.c_str(),
                DNS_TYPE_A,
                DNS_QUERY_WIRE_ONLY,
                pSrvIPList,
                &pRedirectedSource,
                NULL);
#endif

    if(ulRet != 0)
    {
			cout<<"Could not lookup DNS entry for:" << szSource.c_str() << " ErrorCode: " << ulRet <<" [Hexadecimal: "<<std::hex<<ulRet<< "]\n";
        goto cleanup_and_exit;
    }

	// don't care for target during a failback
	if (bDNSFailover) {
#ifdef SCOUT_WIN2K_SUPPORT
		ulRet = DnsQuery(szTarget.c_str(),
					DNS_TYPE_A,
					DNS_QUERY_USE_TCP_ONLY,
	                pSrvIPList,
		            &pTarget,
			        NULL);

#else
		ulRet = DnsQuery(szTarget.c_str(),
					DNS_TYPE_A,
					DNS_QUERY_WIRE_ONLY,
	                pSrvIPList,
		            &pTarget,
			        NULL);
#endif

		if(ulRet != 0)
		{
			cout<<"Could not lookup DNS entry for" << szTarget.c_str() << " ErrorCode:" << ulRet <<" [Hexadecimal: "<<std::hex<<ulRet<<"]\n";
			goto cleanup_and_exit;
		}
		
		cout << szTarget.c_str() << " FQDN = " << pTarget->pName << "\n";
	}
    

    //Copy target address to source record so that source will point to target
    	if (bDNSFailover)
		pRedirectedSource->Data.A = pTarget->Data.A;
	else {
		DNS_A_DATA data;
		data.IpAddress = inet_addr(szIPAddress.c_str());
		pRedirectedSource->Data.A = data;
	}

	if(!dnsServIp.empty())
	{
		ccode = LogonUser(user.c_str(),
					dnsDomain.c_str(),
					password.c_str(),
					LOGON32_LOGON_NEW_CREDENTIALS,
					LOGON32_PROVIDER_WINNT50,
					&hToken);		

		if(ccode == 0)
			cout << "Error in LogonUser(). Error Code: " << GetLastError() << endl;

		if(hToken==NULL)
		{
			cout << "Couldn't get handle in LogonUser()." << endl;
			goto cleanup_and_exit;
		}

		impOutput = ImpersonateLoggedOnUser(hToken);

		//Clean up password 
		SecureZeroMemory(&password[0],password.size());		

	}	

    //Replace the DNS record with new IP Address
	ulRet = DnsReplaceRecordSet(pRedirectedSource,
                DNS_UPDATE_SECURITY_USE_DEFAULT,
                NULL,
                pSrvIPList,
                NULL);
	
	if( impOutput )
		RevertToSelf();
	
    if(ulRet == 0 )
    {
		if (bDNSFailover)
			cout << "\n***** DNS record for " << szSource.c_str() <<" now modified to point to "<< szTarget.c_str() << " successfully\n";
		else
			cout << "\n***** DNS record for " << szSource.c_str() <<" now modified to point to "<< szIPAddress.c_str() << " successfully\n";
    }
	else {
		cout<<"Failed to update DNS record" << " ErrorCode:" << ulRet<<" [Hexadecimal: "<<std::hex<<ulRet<<"]\n";
		goto cleanup_and_exit;
	}

cleanup_and_exit:

    if(pRedirectedSource)
        DnsRecordListFree(pRedirectedSource, DnsFreeRecordListDeep);

    if(pSource)
        DnsRecordListFree(pSource, DnsFreeRecordListDeep);

    if(pTarget)
        DnsRecordListFree(pTarget, DnsFreeRecordListDeep);

	if( pSrvIPList )
	{
		LocalFree(pSrvIPList);
	}
	
	if( hToken )
	{
		if( CloseHandle(hToken) == 0 )
		{
			cout << "[ERROR]: Failed to close handle to the security token.\n";
		}
	}

	return ulRet;
}
string ADInterface::getDNSDomainName()
{
	string dnsDomainName;
	LDAP *pLdapConnection = NULL;
	ULONG ulSearchlimit = 0;
	LDAP_TIMEVAL time;
	time.tv_sec = 0;
	pLdapConnection = getLdapConnectionCustom("",ulSearchlimit,time);
	if(NULL == pLdapConnection)
	{
		cout << "[ERROR]: Can not find domain name." << endl;
	}
	else
	{
		PCHAR strDomain = NULL;
		ULONG ulReturn = LDAP_SUCCESS;
		do
		{
			ulReturn = ldap_get_option(pLdapConnection,LDAP_OPT_DNSDOMAIN_NAME,&strDomain);
			if(LDAP_SUCCESS == ulReturn)
				break;
			else if(ulReturn == LDAP_SERVER_DOWN		|| 
					ulReturn == LDAP_UNAVAILABLE		||
					ulReturn == LDAP_NO_MEMORY			||
					ulReturn == LDAP_BUSY				||
					ulReturn == LDAP_LOCAL_ERROR		||
					ulReturn == LDAP_TIMEOUT			||
					ulReturn == LDAP_OPERATIONS_ERROR	||
					ulReturn == LDAP_TIMELIMIT_EXCEEDED )
			{
				if(pLdapConnection)
				{
					ldap_unbind(pLdapConnection);
					pLdapConnection = NULL;
				}
				pLdapConnection = getLdapConnectionCustom("",ulSearchlimit,time);
				if(NULL == pLdapConnection)
				{
					cout << "[ERROR]: Can not find domain name." << endl;
					return dnsDomainName;
				}
			}
			else
			{
				cout << "Can not find domain name. Error: " << ldap_err2string(ulReturn) << endl;
				return dnsDomainName;
			}
		}while(1);
		if(strDomain)
		{
			dnsDomainName = strDomain;
			DebugPrintf(SV_LOG_INFO,"LDAP: connected to the domain %s\n",dnsDomainName.c_str());
		}
		ulReturn = ldap_unbind(pLdapConnection);
	}
	return dnsDomainName;
}
string ADInterface::getRootDomainName()
{
	string rootDomainName;
	string configDn;
	if(getConfigurationDN(configDn) == SVS_OK)
	{
		string domainDn = configDn.substr(configDn.find_first_of(",")+1);
		do
		{
			size_t startPos = domainDn.find_first_of("=")+1;

			if(rootDomainName.empty())
				if(domainDn.find_first_of(",") != string::npos)
					rootDomainName = domainDn.substr(startPos,domainDn.find_first_of(",") - startPos);
				else
				{
					rootDomainName = domainDn.substr(startPos);
					break;
				}
			else
				if(domainDn.find_first_of(",") != string::npos)
					rootDomainName += "." + domainDn.substr(startPos,domainDn.find_first_of(",") - startPos);
				else
				{
					rootDomainName += "." + domainDn.substr(startPos);
					break;
				}
			
			domainDn = domainDn.substr(domainDn.find_first_of(",")+1);

		}while(1);
	}

	string domainName = getDNSDomainName();
	cout<< "Domain: " << domainName << endl
		<< "Root Domain: " << rootDomainName << endl;
	if(domainName.empty())
	{
		DebugPrintf(SV_LOG_ERROR,"Could not get domain name using ldap interface.\n \
			If there is a parent-child domain configuration then the remote domain users will not migrate.\n");
		rootDomainName = "";
	}
	else
	{
		size_t domainPos = domainName.find(rootDomainName);
		if(strcmpi(domainName.c_str(),rootDomainName.c_str()) == 0 )
		{
			DebugPrintf(SV_LOG_DEBUG,"domain and root-domain are same.\n \
									 root domain name: %s \
									 domain name: %s",rootDomainName.c_str(),domainName.c_str());
			rootDomainName = "";
		}
		else
		{
			DebugPrintf(SV_LOG_DEBUG,"Root domain: %s",rootDomainName.c_str());
		}
	}
	return rootDomainName;
}

SVERROR
ADInterface::failoverExchangeServerUsers(const string& sourceExchangeServerName, const string& destinationExchangeServerName, 
										 bool bAutoRefineFilters, bool bDryRun, const list<string> &proSGs, bool bFailover, bool bSldRoot)
{
	if(strcmpi(sourceExchangeServerName.c_str(), destinationExchangeServerName.c_str()) == 0)
	{
		cout << "[INFO]: Skipping user migration as source & target servers specified are same.\n";
		return SVS_OK;
	}
	list <string> namingContexts;
	ULONG lRtn = 0, option = 1;
    
    ULONG ulSearchSizeLimit = 0;//QUERY_RESULT_SIZE;	// Limit number of users fetched at a time to 50
	LDAP_TIMEVAL ltConnectionTimeout;
	ltConnectionTimeout.tv_sec = 0;		//CONNECTION_ESTABLISHMENT_TIMEOUT;

	string rootDomainName = bSldRoot ? "" : getRootDomainName();
	/*if(rootDomainName.empty())
	{
		cout << "\n[FATAL ERROR]: Unable to find domain/parentdomain name. \n";
		return SVE_FAIL;
	}*/

	LDAP* pLdapConnection = NULL;
	
	if( (pLdapConnection = getLdapConnectionCustom("", ulSearchSizeLimit, ltConnectionTimeout, false,rootDomainName)) == NULL )
	{
		if(!rootDomainName.empty())
		{
			cout<< "[Error]: Failed to connect to the domain " << rootDomainName << endl
				<< "If the exchange server have mailboxes for remote domain users then those users will not migrate."<< endl
				<< "Trying to connect to the default direcotry server." << endl;
			if( (pLdapConnection = getLdapConnectionCustom("", ulSearchSizeLimit, ltConnectionTimeout, false)) == NULL )
			{
				cout << "\n[FATAL ERROR]: Unable to connect to directory server. \n";
				return SVE_FAIL;
			}
		}
		else
		{
			cout << "\n[FATAL ERROR]: Unable to connect to directory server. \n";
			return SVE_FAIL;
		}
	}
		
    if (getNamingContexts(pLdapConnection, namingContexts) != SVS_OK) 
	{
		DebugPrintf(SV_LOG_FATAL, "could not get naming contexts\n");
		cerr << "could not get naming contexts" << std::endl;
	}

    list <string>::const_iterator iter;
    for (iter=namingContexts.begin(); iter!=namingContexts.end(); ++iter) 
    {
        if(findPosIgnoreCase((*iter), "CN=Configuration") == 0)
        {
            setMsExchPatchMdbFlag(pLdapConnection,(*iter),destinationExchangeServerName, bDryRun,rootDomainName);
        }
    }
	
	lRtn = ldap_set_option( pLdapConnection, LDAP_OPT_REFERRALS, &option); 
	if ( 0 != lRtn )
	{
		//DebugPrintf( SV_LOG_FATAL, "ldap_set_option: %s\n", ldap_err2string( lRtn ) );
		cout << "Could not set the option of LDAP_OPT_REFERRALS to ON." << endl
				<< "[" << lRtn << "] " << ldap_err2string( lRtn ) << endl;
	}
	
	ULONG uiDryrunMarkedUsers = 0, uiDryrunNotMarkedUsers = 0, uiMigrated = 0, uiNotMigrated = 0;
	for (iter=namingContexts.begin(); iter!=namingContexts.end(); ++iter) 
	{	
		USHORT usSkipContext;
		for(usSkipContext = 0; usSkipContext < 3; usSkipContext++)
		{
            if(iter->find(nonUserNamingContexts[usSkipContext]) != string::npos)
				break;
		}

		if(usSkipContext < 3)
		{
			cout << "Skipping the context for user search : " << *iter << endl;
			continue;
		}        
		
		cout << "\nSearching for users in the context : " << *iter << endl;		
		if(pLdapConnection == NULL)
		{
			pLdapConnection = getLdapConnectionCustom("", ulSearchSizeLimit, ltConnectionTimeout, false,rootDomainName);
			if(pLdapConnection == NULL)
			{
                cout << "\n[FATAL ERROR]: Unable to connect to directory server. \n";
				continue;
			}
		}

		//TODO: Retry bind
		// Bind with current process credentials, should have query permissions on AD
		USHORT usBindAttempts = 0;
		do
		{
			if(usBindAttempts)
			{
                cout << "\nRe-trying to bind to the context: " << (*iter) << "\n";
			}

            lRtn = ldap_bind_s(pLdapConnection,(const PCHAR) (*iter).c_str(),NULL,LDAP_AUTH_NEGOTIATE); 
            if(lRtn != LDAP_SUCCESS)
			{
				//DebugPrintf(SV_LOG_FATAL, "ldap_bind_s failed with 0x%lx.\n", lRtn);
				cout << "[ERROR][" << lRtn << "] ldap_bind_s failed. " << ldap_err2string(lRtn) << endl;
				continue;
			}
			else
			{
                //cout << "ldap_bind_s succeeded:" << lRtn << std::endl;
				break;
			}
			usBindAttempts++;
			if(usBindAttempts == MAX_LDAP_BIND_ATTEMPTS)
			{
				cout << "\n[WARNING]: Maximum attempts to bind to the context " << (*iter) << " has been reached.\n";
				break;
			}
		}while(1);

        InitiallizeFilter(bAutoRefineFilters);

		bool bChangeFilter = true;
		int nRetryFilterAttempts = 0;
		string szFilter;
        while(1)
        {
			if(bChangeFilter)
			{
				szFilter = getNextFilter();
				nRetryFilterAttempts = 0; //Reset retry count for each filter.
			}
			else
			{
				bChangeFilter = true;
				if( ++nRetryFilterAttempts >= MAX_RETRY_ATTEMPTS) {	
					cout << "[ERROR] :Reached maximun retry attempts. Skipping current filter."
						 << "\nSome of the users might be missed from migration."<<endl
						 << "Skipped filter is [" << szFilter << "]" << endl;
					continue;
				}
				else {
					cout << "Retrying search with same filter..." << endl;
				}
			}
            //cout<<"Filter String:"<<szFilter.c_str()<<"\n";
            
            if(szFilter.empty())
                break;
            
		    // Perform a synchronous search of the current NamingContext
		    // all user objects that have a "person" category.
		    ULONG errorCode = LDAP_SUCCESS;
            PCHAR pSearchFilter = (PCHAR) szFilter.c_str();

            PCHAR pAttributes[4];
		    pAttributes[0] = (PCHAR)homeMTA.c_str();
		    pAttributes[1] = (PCHAR)homeMDB.c_str();
		    pAttributes[2] = (PCHAR)msExchHomeServerName.c_str();
		    pAttributes[3] = NULL;

			int refinefilterflag = 0;

			bool bSearchErr = false, bConnectErr = false;
			LDAPSearch* pSearch = NULL;
			do
			{
				pSearch = ldap_search_init_page(pLdapConnection,				
							(const PCHAR) (*iter).c_str(),  // DN to start search
							LDAP_SCOPE_SUBTREE,				 // Indicate the search scope
							pSearchFilter,						// Search Filter
							pAttributes,					// Retrieve list of attributes
							0,								//retrieve both attributesand values
							0,								//Server Controls
							0,								//Client Controls
							0,								//Time Limit
							0,								//Size Limit per page
							0);								//Sort Keys
				
				if(NULL == pSearch)
					errorCode = LdapGetLastError();
				else
					errorCode = LDAP_SUCCESS;
				
				if( errorCode == LDAP_SUCCESS )
					break;
				if(errorCode == LDAP_PARTIAL_RESULTS || errorCode == LDAP_SIZELIMIT_EXCEEDED)
				{
					//cout << "partial/sizelimit exceeded...\n";
					refinefilterflag = 1;
					break;
				}
				else if( errorCode == LDAP_SERVER_DOWN || 
					errorCode == LDAP_UNAVAILABLE ||
					errorCode == LDAP_NO_MEMORY ||
					errorCode == LDAP_BUSY ||
					errorCode == LDAP_LOCAL_ERROR ||
					errorCode == LDAP_TIMEOUT ||
					errorCode == LDAP_OPERATIONS_ERROR ||
					errorCode == LDAP_TIMELIMIT_EXCEEDED)
				{
					cout << "[ERROR][" << errorCode << "]: ldap_search_init_page failed. " << ldap_err2string(errorCode) << endl;
					if(pLdapConnection)
					{
						if(ldap_unbind_s(pLdapConnection)==ERROR_SUCCESS)
						{
							cout << "Cleaning up current connection...\n";
							pLdapConnection = NULL;
						}
					}

     				if( (pLdapConnection = getLdapConnectionCustom(*iter, ulSearchSizeLimit, ltConnectionTimeout,true,rootDomainName)) == NULL )
					{
						bConnectErr = true;
						break;	
					}
					else
					{
						cout << "Re-connected successfully...\n";
						//cout << "conn = " << pLdapConnection << endl;
					}
				}				
				else
				{
					//DebugPrintf(SV_LOG_FATAL, "ldap_search_s failed with: 0x%0lx \n",errorCode);
					cout << "[FATAL ERROR][:" << errorCode << "]: ldap_search_init_page failed. " << ldap_err2string(errorCode) << endl;
					bSearchErr = true;	
					break;
				}
			}while(1);
			
			if( refinefilterflag && bAutoRefineFilters )
			{
				RefineFilter();
				continue;	//continue search in this context
			}

			if( bConnectErr )
			{
				cout << "[INFO]: Maximum connection attempts " << MAX_LDAP_CONNECT_ATTEMPTS << " has been reached...\n";
				cout << "[ERROR]: Unable to connect to directory server for context: " << *iter << endl;
                cout << "[INFO]: Moving to next context, if any\n";
				break;	//search next context
			}
		
			if( bSearchErr )
			{
				cout << "[ERROR] Error encountered while searching directory server, moving to next context, if any\n";
				break;	//search next context
			}
			ULONG ulSearchPageErr = LDAP_SUCCESS;
			while(LDAP_SUCCESS == ulSearchPageErr)
			{
				l_timeval timeout;
				timeout.tv_sec = INM_LDAP_MAX_SEARCH_TIME_LIMT_SEC;
				ULONG ulTotalCount = 0;
				LDAPMessage* pSearchResult = NULL;
				ulSearchPageErr = ldap_get_next_page_s(pLdapConnection,pSearch,&timeout,
					SEARCH_PAGE_SIZE,
					&ulTotalCount,
					&pSearchResult);
				if(LDAP_NO_RESULTS_RETURNED == ulSearchPageErr)
					break; // Reached the end of pages.
				else if(LDAP_PARTIAL_RESULTS == ulSearchPageErr ||
					LDAP_SIZELIMIT_EXCEEDED == ulSearchPageErr)
				{
					if(bAutoRefineFilters)
						RefineFilter();
					break;
				}
				else if( LDAP_SERVER_DOWN == ulSearchPageErr || 
					LDAP_UNAVAILABLE == ulSearchPageErr ||
					LDAP_NO_MEMORY == ulSearchPageErr ||
					LDAP_BUSY == ulSearchPageErr ||
					LDAP_LOCAL_ERROR == ulSearchPageErr ||
					LDAP_TIMEOUT == ulSearchPageErr ||
					LDAP_OPERATIONS_ERROR == ulSearchPageErr ||
					LDAP_TIMELIMIT_EXCEEDED == ulSearchPageErr)
				{
					cout << "[Error]: ldap_get_next_page_s failed with error :" << ulSearchPageErr << endl;
					bChangeFilter = false;
					break;
				}
				else if(LDAP_SUCCESS != ulSearchPageErr)
				{
					cout << "[Error]: ldap_get_next_page_s failed with error :" << ulSearchPageErr << endl
						<< "[Warning]: Some of the exchange users might not migrater to target" << endl;
					break;
				}
				// Get the number of entries returned.
				ULONG numberOfEntries;
				numberOfEntries = ldap_count_entries(pLdapConnection, pSearchResult);
				if(0 == numberOfEntries)
				{
					if(pSearchResult != NULL) 
					{
						ldap_msgfree(pSearchResult);
						pSearchResult = NULL;
					}
					continue;
				}
				//cout << "Found " << numberOfEntries << " users " << endl;

				
				LDAPMessage* pLdapMessageEntry = NULL;
				BerElement* pBerEncoding = NULL;
				PCHAR pAttribute = NULL;
				
				char* pStatusMessage;
				PCHAR* ppValue = NULL;
				ULONG iCount = 0;
				ULONG iValue = 0;

				for( iCount=0; iCount < numberOfEntries; iCount++ )
				{
					bool failoverUser = false;
					
					if( !iCount ) 
					{
						pLdapMessageEntry = ldap_first_entry(pLdapConnection, pSearchResult);
					}
					else
					{
						pLdapMessageEntry = ldap_next_entry(pLdapConnection, pLdapMessageEntry);
					}

					
					pStatusMessage = (!iCount ? "ldap_first_entry" : "ldap_next_entry");
					if( pLdapMessageEntry == NULL )
					{
						DebugPrintf(SV_LOG_FATAL, "%s failed with 0x%0lx \n", pStatusMessage, LdapGetLastError());
						cout << pStatusMessage << "failed with:" << LdapGetLastError() << std::endl;
						if( pSearchResult != NULL)
						{
							ldap_msgfree(pSearchResult);
							pSearchResult = NULL;
						}
						continue;
					}
					//printf("ENTRY NUMBER %i \n", iCount);

					PCHAR pEntryDN = ldap_get_dn(pLdapConnection, pLdapMessageEntry);
					if( pEntryDN == NULL )
					{
						DebugPrintf(SV_LOG_FATAL, "%s failed with 0x%0lx \n", pStatusMessage, LdapGetLastError());
						cout << pStatusMessage << "failed with:" << LdapGetLastError() << std::endl;
						if( pSearchResult != NULL)
						{
							ldap_msgfree(pSearchResult);
							pSearchResult = NULL;
						}
						continue;
					}

					//cout << "\n*****Processing user: " << pEntryDN << std::endl;
					map <string, char**> attributes;
					map<string, berval** > attributesBin;
					char **pValues = NULL;
					string srcMTA, tgtMTA;

					//Initialize with appropriate MTA servers
					srcMTA = (this->srcMTAExchServerName.empty()) ? sourceExchangeServerName : this->srcMTAExchServerName;
					tgtMTA = (this->tgtMTAExchServerName.empty()) ? destinationExchangeServerName : this->tgtMTAExchServerName;

					// Get the first attribute name
					pAttribute = ldap_first_attribute(pLdapConnection, pLdapMessageEntry, &pBerEncoding);

					while(pAttribute != NULL)
					{
						//cout << "Processing Attriubte " << pAttribute << std::endl;

						string szSearchString = "";
						ppValue = NULL;
						pValues = NULL;

						// Get the string values.
						ppValue = ldap_get_values(pLdapConnection, pLdapMessageEntry, pAttribute);
						if(ppValue == NULL)
						{
							cout << ": [NO ATTRIBUTE VALUE RETURNED]" << std::endl;
						}
						// Output the attribute values
						else
						{
							iValue = ldap_count_values(ppValue);
							if(!iValue)
							{
								cout << ": [BAD VALUE LIST]" << std::endl;
							}
							else
							{
								// The attributes are not guaranteed to return in the same order as specified
								// attributes with no values are not returned
								if (homeMTA == pAttribute) {
									szSearchString += *ppValue;
								}
								else if (homeMDB == pAttribute) {
									szSearchString += *ppValue;
								}
								else if (msExchHomeServerName == pAttribute) {
									szSearchString += *ppValue;
								}
								else {
									DebugPrintf(SV_LOG_WARNING, "Unexpected attribute\n");
								}

								// if this user belongs to the source server, modify entry and save it as it needs to be migrated
								if(szSearchString.empty() == false && homeMDB != pAttribute &&
									( (findPosIgnoreCase(szSearchString, sourceExchangeServerName) != string::npos) || 
									(findPosIgnoreCase(szSearchString, srcMTA) != string::npos) ) ) 

								{
									pValues = (char **) calloc(2, sizeof(char *));
									string replacementDN;
									ULONG numReplaced;
									int pos;
									string serverNameFromEntry;
									string searchPpValue;
									if (findPosIgnoreCase(pAttribute, msExchHomeServerName) == 0)
									{	
										searchPpValue = convertoToLower(*ppValue, strlen(*ppValue));
										pos = searchPpValue.find(string("servers/cn="));
										serverNameFromEntry.assign(searchPpValue.c_str(), pos + strlen("servers/cn="), szSearchString.length());

										if(serverNameFromEntry.length() == sourceExchangeServerName.length())
										{
											if (switchCNValuesSpecial(sourceExchangeServerName, destinationExchangeServerName, *ppValue, replacementDN, numReplaced, "/").failed())									{
												DebugPrintf (SV_LOG_FATAL, "Could not switch over to the new server \n");
												cout << "Could not switch over to the new server \n";
											}
											else {
												//cout << "Modified msExchHomeServerName Attribute Value to " << replacementDN.c_str() << std::endl;											
											}
										}
									}
									else
									{
										if( findPosIgnoreCase(szSearchString, string(string("=") + srcMTA + ",")) != string::npos)
										{
											if (switchCNValues(srcMTA, tgtMTA, *ppValue, replacementDN, numReplaced, ",").failed())
											{
												DebugPrintf (SV_LOG_FATAL, "Could not switch over to the new server \n");
												cout << "Could not switch over to the new server \n";
											}		
											else
											{
												//cout << "Modified homeMTA Value to " << replacementDN.c_str() << std::endl;
											}
										}
									}
                                    size_t valuelen;
                                    INM_SAFE_ARITHMETIC(valuelen = InmSafeInt<size_t>::Type(replacementDN.length())+1, INMAGE_EX(replacementDN.length()))
									pValues[0] = (char *)calloc(valuelen, sizeof(char));
                                    inm_strcpy_s(pValues[0], valuelen, replacementDN.c_str());
									pValues[1] = NULL;
									attributes[pAttribute] = pValues;
								}
								// for the homeMDB attribute, failover users only if they are part of the storage group being failed over
								else if(szSearchString.empty() == false && homeMDB == pAttribute) {
									list<string>::const_iterator sgIter = proSGs.begin();
									if (proSGs.empty()) {	// empty list of protected storage group => user must be failed over
										failoverUser = true;
									}
									else {
										for (; sgIter != proSGs.end(); sgIter++) {
											string replacementDN;
											ULONG numReplaced;
											switchCNValues(destinationExchangeServerName, sourceExchangeServerName, *sgIter, replacementDN, 
															numReplaced, ",");
											string dnLower = convertoToLower(szSearchString, szSearchString.length());										
											//cout << "Processing homeMDB : " << dnLower.c_str() << std::endl;
											string sgLower = convertoToLower(replacementDN, replacementDN.length());
											//cout << "Searching For Storage Group : " << sgLower.c_str() << std::endl;
											if (strstr(dnLower.c_str(), sgLower.c_str()) != 0) {
												failoverUser = true;
												break;
											}
										}
									}

									if (failoverUser) {
										pValues = (char **) calloc(2, sizeof(char *));
										string replacementDN;
										ULONG numReplaced;
										if (switchCNValues(sourceExchangeServerName, destinationExchangeServerName, *ppValue, replacementDN, numReplaced, ",").failed())
										{
											DebugPrintf (SV_LOG_FATAL, "Could not switch over to the new server \n");
										}
										else {
											//cout << "Modified homeMDB Value to " << replacementDN.c_str() << std::endl;
										}

                                        size_t valuelen;
                                        INM_SAFE_ARITHMETIC(valuelen = InmSafeInt<size_t>::Type(replacementDN.length())+1, INMAGE_EX(replacementDN.length()))
										pValues[0] = (char *)calloc(valuelen, sizeof(char));
                                        inm_strcpy_s(pValues[0], valuelen, replacementDN.c_str());
										pValues[1] = NULL;
										attributes[pAttribute] = pValues;
									}
								}

								// Output more values if available
								ULONG z;
								for(z=1; z<iValue; z++)
								{
									DebugPrintf(SV_LOG_FATAL, "Multiple attribute values detected, %s", ppValue[z]);
									cerr << "Multiple attribute values detected:" << ppValue[z] << std::endl;
								}
							}
						}

						// Free memory.
						if(ppValue != NULL)  {
							ldap_value_free(ppValue);
						}
						ppValue = NULL;
						ldap_memfree(pAttribute);
	    		        
						// Get next attribute name.
						pAttribute = ldap_next_attribute(pLdapConnection, pLdapMessageEntry, pBerEncoding);
					}

					//ATTR: homeMTA: 
					//CN=Microsoft MTA,CN=BUNKER,CN=Servers,CN=First Administrative Group,CN=Administrative Groups,CN=Abhai Systems,CN=Microsoft Exchange,CN=Services,CN=Configuration,DC=inside,DC=abhaisystems,DC=com     
					//ATTR: homeMDB: 
					//CN=Mailbox Store (BUNKER),CN=First Storage Group,CN=InformationStore,CN=BUNKER,CN=Servers,CN=First Administrative Group,CN=Administrative Groups,CN=Abhai Systems,CN=Microsoft Exchange,CN=Services,CN=Configuration,DC=inside,DC=abhaisystems,DC=com

					// If any of the queried attributes were returned, replace the servername with the destination servername
					if (failoverUser) 
					{
						if( bDryRun )
						{
							cout << "User marked for migration : " << pEntryDN << endl;
							uiDryrunMarkedUsers++;
						}
						else if (modifyADEntry(pLdapConnection, pEntryDN, attributes, attributesBin) == SVE_FAIL)
						{
							cout << "\n****** Failed to update user entry " << pEntryDN << std::endl;
							uiNotMigrated++;
						}
						else
						{
							cout << "Migrating user: " << pEntryDN << std::endl;
							uiMigrated++;
						}
					}
					else {
						cout << "User NOT marked for migration : " << pEntryDN << std::endl;
						uiDryrunNotMarkedUsers++;
					}

					// free memory
					map<string, char**>::const_iterator iter = attributes.begin();
					for (; iter != attributes.end(); iter++) 
					{
						pValues = iter->second;
						for (int index=0; pValues[index] != NULL; index++) {
							free(pValues[index]);
						}
						free(pValues);
					}

					if( pBerEncoding != NULL ) {
						ber_free(pBerEncoding,0);
					}
					pBerEncoding = NULL;

					if (pEntryDN != NULL) {
						ldap_memfree(pEntryDN);
					}
					pEntryDN = NULL;

					if (ppValue != NULL)
						ldap_value_free(ppValue);
					ppValue = NULL;
				}

				if(pSearchResult != NULL) 
				{
					ldap_msgfree(pSearchResult);
					pSearchResult = NULL;
				}
			}
			
			ldap_search_abandon_page(pLdapConnection,pSearch);
		}
    }

	if( !bDryRun )
	{
		cout << "\n***** Successfully migrated a total of " << uiMigrated << " users\n";
		if( uiNotMigrated )
		{
			cout << "\n***** Failed to migrate a total of " << uiNotMigrated << " users\n\n"; 
		}
	}
	else
	{
		cout << "\n***** Total number of users on source server : " << uiDryrunNotMarkedUsers + uiDryrunMarkedUsers << endl;
		cout << "\n***** Total number of users marked for migration : " << uiDryrunMarkedUsers << endl;
		if( uiDryrunNotMarkedUsers )
		{
			cout << "\n***** Total number of users NOT marked for migration : " << uiDryrunNotMarkedUsers << endl;
		}
	}

	//_strtime( tmpbuf2 );
	//cout << "Start Time: " << tmpbuf1 << " End Time: " << tmpbuf2 << std::endl;
	return SVS_OK;
}

bool
ADInterface::setMsExchPatchMdbFlag(LDAP *pLdapConnection, string szNameContext, string szTargetServerName, bool bDryRun,const std::string otherDomain)
{
    // Bind with current process credentials, should have query permissions on AD
    string szNamingContext = "CN=Microsoft Exchange,CN=Services";
    szNamingContext += ",";
    szNamingContext += szNameContext;

	ULONG lRtn = ldap_bind_s(pLdapConnection,                 // Session Handle
				(const PCHAR) szNamingContext.c_str(),   // Use the naming context from the input list
				NULL,                            // if it is NULL it will use current user or service credentials 
				LDAP_AUTH_NEGOTIATE);            // LDAP authentication mode

	if(lRtn != LDAP_SUCCESS)
	{
		return false;
	}
	
	ULONG errorCode = LDAP_SUCCESS;
	LDAPMessage* pSearchResult;

    PCHAR pSearchFilter = "(|(objectClass=msExchPrivateMDB)(objectClass=msExchPublicMDB))";
	PCHAR pAttributes[2];
    pAttributes[0] = (PCHAR)msExchPatchMDB.c_str();
	pAttributes[1] = NULL;
		
	ULONG ulSearchSizeLimit = 0;	// Limit number of users fetched at a time
	LDAP_TIMEVAL ltConnectionTimeout;
	ltConnectionTimeout.tv_sec = 0;		// time out --> CONNECTION_ESTABLISHMENT_TIMEOUT;	
	bool bSearchErr = false, bConnectErr = false;

	do
	{
		errorCode = ldap_search_s(pLdapConnection,				
				(const PCHAR) szNamingContext.c_str(),  // DN to start search
				LDAP_SCOPE_SUBTREE,				 // Indicate the search scope
				pSearchFilter,						// Search Filter
				pAttributes,					// Retrieve list of attributes
				0,								//retrieve both attributesand values
				&pSearchResult);				// this is out parameter - Search results
	    
		if( errorCode == LDAP_SUCCESS )
			break;    
	    
		if( errorCode == LDAP_SERVER_DOWN || 
			errorCode == LDAP_UNAVAILABLE ||
			errorCode == LDAP_NO_MEMORY ||
			errorCode == LDAP_BUSY ||
			errorCode == LDAP_LOCAL_ERROR ||
			errorCode == LDAP_TIMEOUT ||
			errorCode == LDAP_OPERATIONS_ERROR ||
			errorCode == LDAP_TIMELIMIT_EXCEEDED)
		{
			cout << "[ERROR][" << errorCode << "]: ldap_search_s failed. " << ldap_err2string(errorCode) << endl;
			if(pLdapConnection)
			{
				if(ldap_unbind_s(pLdapConnection)==ERROR_SUCCESS)
				{
					cout << "Cleaning up current connection...\n";
					pLdapConnection = NULL;
				}
			}
			if(pSearchResult)
			{
				ldap_msgfree(pSearchResult);
				pSearchResult = NULL;
			}
     		if( (pLdapConnection = getLdapConnectionCustom(szNamingContext, ulSearchSizeLimit, ltConnectionTimeout,true,otherDomain)) == NULL )
			{
				bConnectErr = true;
				break;	
			}
		}
		else
		{
			//DebugPrintf(SV_LOG_FATAL, "ldap_search_s failed with: 0x%0lx \n",errorCode);
			cout << "[FATAL ERROR][:" << errorCode << "]: ldap_search_s failed. " << ldap_err2string(errorCode) << endl;
			if(pSearchResult != NULL)
			{
				ldap_msgfree(pSearchResult);
				pSearchResult = NULL;						
			}
			bSearchErr = true;	
			break;
		}
	}while(1);

	if( bConnectErr )
	{
		cout << "[INFO]: Maximum connection attempts " << MAX_LDAP_CONNECT_ATTEMPTS << " has been reached...\n";
		cout << "[ERROR]: Unable to connect to directory server." << endl;
		return false;
	}

	if( bSearchErr )
	{
		cout << "[ERROR] Error encountered while searching directory server.\n";
		return false;
	}

    ULONG numberOfEntries;
    numberOfEntries = ldap_count_entries(pLdapConnection, pSearchResult);
    if(numberOfEntries == NULL)
    {
	    DebugPrintf(SV_LOG_WARNING, "ldap_count_entries failed with 0x%0lx \n", errorCode);
	    if(pSearchResult != NULL) 
	    {
		    ldap_msgfree(pSearchResult);
			pSearchResult = NULL;
		    return false;
	    }
    }

    LDAPMessage* pLdapMessageEntry = NULL;

    while(1)
    {
        if(pLdapMessageEntry == NULL)
            pLdapMessageEntry = ldap_first_entry(pLdapConnection, pSearchResult);
        else
            pLdapMessageEntry = ldap_next_entry(pLdapConnection, pLdapMessageEntry);

        if(pLdapMessageEntry == NULL)
            break;

        PCHAR pEntryDN = ldap_get_dn(pLdapConnection, pLdapMessageEntry);
	    if( pEntryDN == NULL )
	    {
            cout<<" DN is null\n";
		    if( pSearchResult != NULL)
			{
				ldap_msgfree(pSearchResult);
				pSearchResult = NULL;
			}
		    return false;
	    }

        string szTargetServerDN = pEntryDN;
        string szTargetNameToMatch ="CN=";
        szTargetNameToMatch += szTargetServerName;
		szTargetNameToMatch += ",CN=Servers";

        //Search whether this is a target MDB that needs to be modified.
        if(findPosIgnoreCase(szTargetServerDN, szTargetNameToMatch) == string::npos)
            continue;
		ULONG ldrc;
        if (bDryRun)
			ldrc = modifyPatchMDBDryRun(pLdapConnection, pEntryDN);
		else 
			ldrc = modifyPatchMDB(pLdapConnection, pEntryDN);

	    if (LDAP_SUCCESS == ldrc)
        {
            cout << "\n*****Changed PatchMDB settings for entry:"<< szTargetServerDN << std::endl;
		}
		else
		{
            cout << "\tFailed changing  PatchMDB settings for entry:"<< szTargetServerDN << " Error:"<< ldap_err2string( LdapGetLastError())<<std::endl;
		}
    }

	return true;
}

SVERROR
ADInterface::getNamingContexts(LDAP *pLdapConnection, list<string>& namingContexts) 
{
  int rc, i;
  char *matched_msg = NULL, *error_msg = NULL;
  LDAPMessage  *pSearchResult, *pLdapMessageEntry;
  BerElement  *ber;
  char    *pAttribute;
  char    **vals;
  char    *attrs[2];
  ULONG ulSearchSizeLimit = 0;	// Limit number of users fetched at a time
  LDAP_TIMEVAL ltConnectionTimeout;
  ltConnectionTimeout.tv_sec = 0;		//CONNECTION_ESTABLISHMENT_TIMEOUT;

  // Verify that the connection handle is valid. 
  if ( pLdapConnection == NULL ) {
    DebugPrintf( SV_LOG_FATAL, "Invalid connection handle.\n" );
    return( SVE_INVALIDARG );
  }

  // Set automatic referral processing off. 
  ULONG option =  0;
  rc = ldap_set_option( pLdapConnection, LDAP_OPT_REFERRALS, &option); //LDAP_OPT_OFF
  if ( 0 != rc ) {
#if WIN32
	rc = LdapGetLastError();
#else 
	rc = ldap_get_lderrno( pLdapConnection, NULL, NULL );
#endif
    DebugPrintf( SV_LOG_FATAL, "ldap_set_option: %s\n", ldap_err2string( rc ) );
	return( SVE_FAIL );
  }

  // Search the root DSE for namingContexts,
  // Could/Should use rootDomainNamingContext instead as we don't care about config,dns contexts etc..
  attrs[0] = "namingContexts"; 
  attrs[1] = NULL;

  do
  {
	    rc = ldap_search_ext_s( pLdapConnection, "", LDAP_SCOPE_BASE, "(objectclass=*)", attrs,
			0, NULL, NULL, NULL, 0, &pSearchResult );

		//cout << "IP Address:" << pLdapConnection->ld_host<<"\n";
		//Check the search results. 
		if( rc == LDAP_SUCCESS )
		{
			break;
		}
			
		switch( rc ) {
			// If successful, the root DSE was found. 
			case LDAP_SUCCESS:
				break;
			// If the root DSE was not found, the server does not comply with the LDAPv3 protocol
			case LDAP_PARTIAL_RESULTS: case LDAP_NO_SUCH_OBJECT: 
			case LDAP_OPERATIONS_ERROR: case LDAP_PROTOCOL_ERROR:
				DebugPrintf(SV_LOG_FATAL, "LDAP server returned result code %d (%s).\n"
				"This server does not support the LDAPv3 protocol.\n", rc, ldap_err2string( rc ) );
				return( SVE_NOTIMPL );
			// If any other value is returned, an error must have occurred. 
			
			case LDAP_SERVER_DOWN:
			case LDAP_UNAVAILABLE:
			case LDAP_NO_MEMORY:
			case LDAP_BUSY:
			case LDAP_LOCAL_ERROR:
			case LDAP_TIMEOUT:	
			case LDAP_TIMELIMIT_EXCEEDED:
				cout << "[ERROR][" << rc << "]: ldap_search_ext_s failed. " << ldap_err2string(rc) << endl;
				if(pLdapConnection)
				{
					if(ldap_unbind_s(pLdapConnection)==ERROR_SUCCESS)
					{
						cout << "Cleaning up current connection...\n";
						pLdapConnection = NULL;
					}
				}
				if(pSearchResult)
				{
					ldap_msgfree(pSearchResult);
					pSearchResult = NULL;
				}

				if( (pLdapConnection = getLdapConnectionCustom(NULL, ulSearchSizeLimit, ltConnectionTimeout, false)) == NULL )
				{
					cout << "\n[FATAL ERROR]: Unable to connect to directory server. \n";
					return SVE_FAIL;
				}
				break;	
			default:
				DebugPrintf( SV_LOG_FATAL, "ldap_search_ext_s: %s\n", ldap_err2string( rc ) );
				return( SVE_FAIL );
		}        
  }while(1);

  // Since only one entry should have matched, get that entry. 
  pLdapMessageEntry = ldap_first_entry( pLdapConnection, pSearchResult );
  if ( pLdapMessageEntry == NULL ) {
    DebugPrintf( SV_LOG_FATAL, "ldap_search_ext_s: Unable to get root DSE.\n");
	if (NULL != pSearchResult) 
	{
		ldap_msgfree( pSearchResult );
		pSearchResult = NULL;
	}
	return( SVE_FAIL );
  }
  
  // Iterate through each attribute in the entry. 
  //cout<<"Iterate through all attributes\n";

  for ( pAttribute = ldap_first_attribute( pLdapConnection, pLdapMessageEntry, &ber );
    pAttribute != NULL; pAttribute = ldap_next_attribute( pLdapConnection, pLdapMessageEntry, ber ) ) {
      
    // Print each value of the attribute.
    if ((vals = ldap_get_values( pLdapConnection, pLdapMessageEntry, pAttribute)) != NULL ) 
	{
      for ( i = 0; vals[i] != NULL; i++ ) 
	  {
        //cout << pAttribute<< vals[i]<<"\n";
		namingContexts.push_back(string(vals[i]));
	  }
      // Free memory allocated by ldap_get_values(). 
      ldap_value_free( vals );
    }
    
    // Free memory allocated by ldap_first_attribute(). 
    ldap_memfree( pAttribute );
  }
  
  // Free memory allocated by ldap_first_attribute(). 
  if ( ber != NULL ) {
    ber_free( ber, 0 );
  }

  // Free memory allocated by ldap_search_ext_s().
  if( pSearchResult != NULL)
  {
	  ldap_msgfree(pSearchResult);
	  pSearchResult = NULL;
  }

  return( SVS_OK );
}

SVERROR 
ADInterface::switchCNValues(const string& search, const string& replace, const string& sourceDN, string& replacementDN, ULONG& numReplaced, string szToken) 
{
	// @TODO: Fix errant replaces of LHS values. e.g: We will cause an error if a replacement server is called "CN" or "DC" etc.
	// @TODO: Fix case insensitive compares based on locale

    SVERROR rc = SVS_OK;
    ULONG ulSourceLength;
    INM_SAFE_ARITHMETIC(ulSourceLength = InmSafeInt<ULONG>::Type((ULONG) sourceDN.length()) + 1 /* Length + null character */, INMAGE_EX((ULONG) sourceDN.length()))

    char *sourceDNCopy = (char *) calloc(ulSourceLength, sizeof(char));
    inm_memcpy_s(sourceDNCopy,ulSourceLength, sourceDN.c_str(), ulSourceLength * sizeof(char));

    vector<char *> vToken;
    char *pToken = strtok(sourceDNCopy, szToken.c_str());

    while(pToken)
    {
        vToken.push_back(pToken); //Push token on stack
        pToken = strtok(NULL, szToken.c_str());
    }

    for(ULONG i = 0; i < vToken.size(); i++)
    {
        string szReplace;

        if(findPosIgnoreCase(vToken[i], "CN=") == 0)
        {
            searchAndReplaceToken(search, replace, vToken[i] + 3, szReplace, numReplaced);
            replacementDN += vToken[i][0];
            replacementDN += vToken[i][1];
            replacementDN += vToken[i][2];
            replacementDN += szReplace + szToken;
        }
        else
        {
            replacementDN += vToken[i];
            replacementDN += szToken;
        }
    }

    //Erase last comma
    replacementDN.erase(replacementDN.size() - 1);

	return rc;
}

// Helper method to search and switch CN values of input DN's, returns the new DN and number of 
// replacements made. The replacementDN will always be returned with the correct value, even when
// number of replacements are 0
SVERROR
ADInterface::searchAndReplaceToken(const string& search, 
						 const string& replace, 
						 const string& source, 
						 string& replacement,
						 ULONG& numReplaced)
{
	SVERROR rc = SVS_OK;
	numReplaced = 0;
	replacement = source;
    long lAdjustmentForLength = 0;
	string::size_type i = 0;
    string::size_type ulOffset = 0;
    char* szTmpToken = NULL;

    size_t toklen;
    INM_SAFE_ARITHMETIC(toklen = InmSafeInt<size_t>::Type(source.length()) + 1, INMAGE_EX(source.length()))
    szTmpToken = (char *) malloc(toklen);
    inm_memcpy_s(szTmpToken, (source.length() + 1),source.c_str(), source.length() + 1);

    vector<char *> vToken;
    char *pToken = strtok(szTmpToken, " ()");

    string szSearchLower = convertoToLower(search, search.length());
    while(pToken) {

        string szTokenLower = convertoToLower(pToken, strlen(pToken));
        if(szTokenLower.compare(szSearchLower) == 0)
        {
            ulOffset = pToken - szTmpToken + lAdjustmentForLength; //Get the relative offset of this token inside target string
		    replacement.replace(ulOffset, search.length(), replace);
            lAdjustmentForLength += (long)(replace.length() - search.length()); //Adjust the length difference between replace and search strings
		    numReplaced++;
        }

        pToken = strtok(NULL, " ()");
	}

	return rc;
}

size_t 
ADInterface::findPosIgnoreCase(const string szSource, const string szSearch)
{
    size_t st = convertoToLower(szSource, szSource.length()).find(convertoToLower(szSearch, szSearch.length()));
    return st;
}

string 
ADInterface::convertToUpper(const string szStringToConvert)
{
	size_t length = szStringToConvert.length();
    string szConvertedString;
    size_t tmplen;
    INM_SAFE_ARITHMETIC(tmplen = InmSafeInt<size_t>::Type(length) + 1, INMAGE_EX(length))
    char *tmp = (char *)malloc(tmplen);
    const char *pStringToConvertPtr = szStringToConvert.c_str();

    ZeroMemory(tmp, length + 1);

    for(unsigned long i = 0; i < length; i++)
    {
        tmp[i] = toupper(pStringToConvertPtr[i]);
	}
    

    szConvertedString = tmp;
    free(tmp);
    return szConvertedString;
}



string 
ADInterface::convertoToLower(const string szStringToConvert, size_t length)
{
    string szConvertedString;
    size_t tmplen;
    INM_SAFE_ARITHMETIC(tmplen = InmSafeInt<size_t>::Type(length) + 1, INMAGE_EX(length))
    char *tmp = (char *)malloc(tmplen);
    const char *pStringToConvertPtr = szStringToConvert.c_str();

    ZeroMemory(tmp, length + 1);

    for(unsigned long i = 0; i < length; i++)
    {
        tmp[i] = tolower(pStringToConvertPtr[i]);
    }

    szConvertedString = tmp;
    free(tmp);
    return szConvertedString;
}

void 
ADInterface::InitiallizeFilter(bool bAutoRefineFilters)
{
	string filter;
    vRemainingFiltersStack.clear();
    if(bAutoRefineFilters)
    {
        for(int i = 0; i < VALID_CHAR_IN_CN_SIZE; i++)
        {
			if(Token[i] == ' ')
			{
				continue;
			}
			
			switch(Token[i])
			{			
			case '(':
				filter = "\\28";
				break;
			case ')':
				filter = "\\29";
                break;
			case '\\':
                filter = "\\5c";
                break;			
			case '*':
				filter = "\\2a";
				break;			
			default:
				filter = Token[i];
			}
            vRemainingFiltersStack.push_back(filter);
        }
    }
    else
    {
        //TODO: Fix duplicate user entries
        string szToken = "";
        vRemainingFiltersStack.push_back(szToken);
    }
}

void
ADInterface::RefineFilter()
{
	string temp;
    for(int i = 0; i < VALID_CHAR_IN_CN_SIZE; i++)
    {	
		switch(Token[i])
		{	
		case '(':
			temp = "\\28";
			break;
		case ')':
			temp = "\\29";
            break;
		case '\\':
            temp = "\\5c";
            break;			
		case '*':
			temp = "\\2a";
			break;			
		default:
			temp = Token[i];
		}
		vRemainingFiltersStack.push_back(szFltCurToken + temp);
		ULONG ulSize = vRemainingFiltersStack.size();
		if( ulSize > 1000 )
            cout << "************Size : " << ulSize << endl;
    }
}

string 
ADInterface::getNextFilter()
{
    if(vRemainingFiltersStack.size() == 0)
    {
        return "";
    }

    //pop top of stack
    string szToken;
	szToken = vRemainingFiltersStack[vRemainingFiltersStack.size() - 1];
    vRemainingFiltersStack.pop_back();

    szFltCurToken = szToken;

    string szFilter = "(&";

    if(szDefaultFilter.length() > 0)
        szFilter += szDefaultFilter;

    if(szFltUserDefinedFilter.length() > 0)
        szFilter += "(" + szFltUserDefinedFilter + ")";

    szFilter += "(cn=";
    szFilter += szToken.c_str();

    string szCloseToken = "*))";
    szFilter += szCloseToken;

    return szFilter;
}

SVERROR
ADInterface::searchAndCompareStorageGroups(const string& srcExchServerNameIn, const string& tgtExchServerNameIn,
										   list<string>& extraStorageGroups, const list<string>& proVolumes,
										   map<string, string>& srcTotgtVolMapping, list<string>& proSGs, const string& sgDN)
{
	string srcExchServerName = convertToUpper(srcExchServerNameIn);
	string tgtExchServerName = convertToUpper(tgtExchServerNameIn);

	// search and compare the storage Groups on source and target exchange servers
	bool bSrcSearch = true;
	bool bTgtSearch = true;

	// find storage groups on the source
	list<string> srcStorageGroupsCN, tgtStorageGroupsCN, srcStorageGroupsDN, tgtStorageGroupsDN;
	if (searchEntryinAD(srcExchServerName, srcStorageGroupsCN, srcStorageGroupsDN, string("(objectclass=msExchStorageGroup)")) == SVE_FAIL) 
	{
		bSrcSearch = false;
	}

	// find storage groups on the target
	if (searchEntryinAD(tgtExchServerName, tgtStorageGroupsCN, tgtStorageGroupsDN, string("(objectclass=msExchStorageGroup)")) == SVE_FAIL) 
	{
		bTgtSearch = false;
	}

	if (bSrcSearch == false || bTgtSearch == false) 
	{
		cout << "Failover Cannot Continue " << std::endl;
		return SVE_FAIL;
	}

	list<string> missingStorageGroups, matchingStorageGroups;
	// find the missing, matching and extra storage groups on the target
	compareDNs(srcExchServerName, tgtExchServerName, srcStorageGroupsDN, tgtStorageGroupsDN, 
			   missingStorageGroups, extraStorageGroups, matchingStorageGroups);

	// So far the list contains all the missing and matching storage groups
	// However, we want to fail over only the protected storage groups
	// so, determine the list of protected storage groups from the list of protected volumes
	list<string> proMissingSGs;
	findProtectedStorageGroups(srcExchServerName, tgtExchServerName, missingStorageGroups, proVolumes, proMissingSGs, sgDN);

	//cout << "Source and Target Storage Groups Do not exactly match\n\n";
	if (proMissingSGs.empty() == false) 
	{
		cout << "Attempting to create the missing storage Groups ...Please Wait\n";
		list<string>::const_iterator iter = proMissingSGs.begin();
		for (iter=proMissingSGs.begin(); iter != proMissingSGs.end(); iter++) {
			if (createStorageGroupsOnTarget(srcExchServerName, tgtExchServerName, (*iter), srcTotgtVolMapping) == SVE_FAIL)
			{
				cout << "Failed to create the missing storage Groups On Target. Failover cannot continue\n";
				return SVE_FAIL;
			}
			else 
			{
				cout << "Successfully created the missing storage Groups On Target.\n\n";
			}
		}
	}

	list<string> proMatchingSGs;
	findProtectedStorageGroups(srcExchServerName, tgtExchServerName, matchingStorageGroups, proVolumes, proMatchingSGs, sgDN);

	if (proMatchingSGs.empty() == false)
	{
		if (updateMatchingStorageGroupsOnTarget(srcExchServerName, tgtExchServerName, proMatchingSGs, srcTotgtVolMapping) == SVE_FAIL)
		{
			cout << "Failed to update matching storage Groups On Target. Failover cannot continue\n";
			return SVE_FAIL;
		}
		else 
		{
			//cout << "Successfully updated the matching storage Groups On Target.\n\n";
		}
	}

	// build a unique list of protected storage group DNs
	proSGs = proMissingSGs;
	list<string>::const_iterator iter = proMatchingSGs.begin();
	for (; iter != proMatchingSGs.end(); iter++) {
		proSGs.push_back(*iter);
	}

	return SVS_OK;
}

SVERROR 
ADInterface::searchEntryinAD(const string& exchangeServerName, 
						list<string>& entryCNList, 
						list<string>& entryDNList,
						const string& filter)
{
	// find configuration Naming Context DN
	string configurationDN;
	if (getConfigurationDN(configurationDN) == SVE_FAIL) 
	{
		cout << "Configuration Naming Context not found\n";
		return SVE_FAIL;
	}	

	// get an Ldap connection
	LDAP* pLdapConnection = getLdapConnection();
	if (pLdapConnection == NULL)
	{
		cout << "Failed to connect to the ldap server\n";
		return SVE_FAIL;
	}

	// ldap bind
	ULONG errorCode = LDAP_SUCCESS;
	ULONG lRtn = 0;
	lRtn = ldap_bind_s(
				pLdapConnection,							// Session Handle
				(const PCHAR) configurationDN.c_str(),		// Use the naming context from the input list
				NULL,										// if it is NULL it will use current user or service credentials 
				LDAP_AUTH_NEGOTIATE);						// LDAP authentication mode
	if(lRtn != LDAP_SUCCESS)
	{
		cout << "ldap_bind_s failed with:" << lRtn << std::endl;
		return SVE_FAIL;
	}

	PCHAR pAttributes[2];
	pAttributes[0] = "cn";
	pAttributes[1] = NULL;
	LDAPMessage* pSearchResult;

	ULONG ulSearchSizeLimit = 0;	// Limit number of users fetched at a time
	LDAP_TIMEVAL ltConnectionTimeout;
	ltConnectionTimeout.tv_sec = 0;		//CONNECTION_ESTABLISHMENT_TIMEOUT;	
	bool bSearchErr = false, bConnectErr = false;

	do
	{
		errorCode = ldap_search_s(pLdapConnection,						
					(const PCHAR) configurationDN.c_str(),	// DN to start search
					LDAP_SCOPE_SUBTREE,						 // Indicate the search scope
					(const PCHAR) filter.c_str(),			// Search Filter
					pAttributes,							// Retrieve list of attributes
					0,										//retrieve both attributesand values
					&pSearchResult);						// this is out parameter - Search results

		if( errorCode == LDAP_SUCCESS )
			break;    
	    
		if( errorCode == LDAP_SERVER_DOWN || 
			errorCode == LDAP_UNAVAILABLE ||
			errorCode == LDAP_NO_MEMORY ||
			errorCode == LDAP_BUSY ||
			errorCode == LDAP_LOCAL_ERROR ||
			errorCode == LDAP_TIMEOUT ||
			errorCode == LDAP_OPERATIONS_ERROR ||
			errorCode == LDAP_TIMELIMIT_EXCEEDED)
		{
			cout << "[ERROR][" << errorCode << "]: ldap_search_s failed. " << ldap_err2string(errorCode) << endl;
			if(pLdapConnection)
			{
				if(ldap_unbind_s(pLdapConnection)==ERROR_SUCCESS)
				{
					cout << "Cleaning up current connection...\n";
					pLdapConnection = NULL;
				}
			}
			if(pSearchResult)
			{
				ldap_msgfree(pSearchResult);
				pSearchResult = NULL;
			}

     		if( (pLdapConnection = getLdapConnectionCustom(configurationDN, ulSearchSizeLimit, ltConnectionTimeout)) == NULL )
			{
				bConnectErr = true;
				break;	
			}
		}
		else
		{
			//DebugPrintf(SV_LOG_FATAL, "ldap_search_s failed with: 0x%0lx \n",errorCode);
			cout << "[FATAL ERROR][:" << errorCode << "]: ldap_search_s failed. " << ldap_err2string(errorCode) << endl;
			if(pSearchResult != NULL)
			{
				ldap_msgfree(pSearchResult);
				pSearchResult = NULL;						
			}
			bSearchErr = true;	
			break;
		}
	}while(1);

	if( bConnectErr )
	{
		cout << "[INFO]: Maximum connection attempts " << MAX_LDAP_CONNECT_ATTEMPTS << " has been reached...\n";
		cout << "[ERROR]: Unable to connect to directory server." << endl;
		return SVE_FAIL;
	}

	if( bSearchErr )
	{
		cout << "[ERROR] Error encountered while searching directory server.\n";
		return SVE_FAIL;
	}
	ULONG numberOfEntries = ldap_count_entries(pLdapConnection, pSearchResult);

	// Check if there are any storage groups configured at all
	if(numberOfEntries == NULL)
	{
	    DebugPrintf(SV_LOG_WARNING, "ldap_count_entries failed with 0x%0lx \n", errorCode);
		//cout << "No Entries Found\n";
	    if( pSearchResult != NULL)
		{
            ldap_msgfree(pSearchResult);
			pSearchResult = NULL;
		}
		
		return SVS_OK;
	}

	LDAPMessage* pLdapMessageEntry = NULL;
	PCHAR pAttribute = NULL;
	BerElement* pBerEncoding = NULL;
	PCHAR* ppValue = NULL;
	ULONG iValue = 0;
	string entryDN;
	string entryDNLowerCase;

	//string exchServerCN = "CN=";
	//exchServerCN += exchangeServerName;
	//string exchServerCNLowerCase = convertoToLower(exchServerCN, exchServerCN.length());

	string exchServerCN = exchangeServerName;
	string exchServerCNLowerCase = convertoToLower(exchServerCN, exchServerCN.length());

	string searchFor;
	// iterate through all storage groups
	for (int index=0; index < (int)numberOfEntries; index++)
	{
	    if(pLdapMessageEntry == NULL)
	        pLdapMessageEntry = ldap_first_entry(pLdapConnection, pSearchResult);
	    else
	        pLdapMessageEntry = ldap_next_entry(pLdapConnection, pLdapMessageEntry);

	    if(pLdapMessageEntry == NULL)
	        break;

		// get storage group DN
		entryDN = ldap_get_dn(pLdapConnection, pLdapMessageEntry);
		entryDNLowerCase = convertoToLower(entryDN, entryDN.length());

		// Is this entry part of the concerned exchange server		
		//cout <<"entryDNLowerCase=" << entryDNLowerCase.c_str() << endl;
		//cout <<"exchServerCNLowerCase=" << exchServerCNLowerCase.c_str() << endl;
		searchFor  = string("cn=") + exchServerCNLowerCase;
		searchFor += ",cn=servers";
	//	cout << "searchFor=" << searchFor.c_str() << endl;
		if(strstr(entryDNLowerCase.c_str(), searchFor.c_str()) == NULL)
		{
			searchFor.clear();
			// move to the next entry
			continue;
		}

		// iterate through all requested attributes on a storage group. Currently it's just "cn"
		for ( pAttribute = ldap_first_attribute( pLdapConnection, pLdapMessageEntry, &pBerEncoding );
				pAttribute != NULL; pAttribute = ldap_next_attribute( pLdapConnection, pLdapMessageEntry, pBerEncoding ) ) 
		{
      
			// Print each value of the attribute.
			if ((ppValue = ldap_get_values( pLdapConnection, pLdapMessageEntry, pAttribute)) != NULL ) 
			{
				for ( int i = 0; ppValue[i] != NULL; i++ ) 
				{
					// save the CN and the DN
					entryCNList.push_back(string(ppValue[i]));
					entryDNList.push_back(entryDN);
				}
				// Free memory allocated by ldap_get_values(). 
				ldap_value_free( ppValue );
			}
    
			// Free memory allocated by ldap_first_attribute(). 
			ldap_memfree( pAttribute );
		}
	}

	// Free memory allocated by ldap_search_ext_s().
	if( pSearchResult != NULL)
	{
		ldap_msgfree(pSearchResult);
		pSearchResult = NULL;
	}
	if( pLdapConnection )
	{
		ldap_unbind_s(pLdapConnection);
		pLdapConnection = NULL;
	}

	return SVS_OK;
}
SVERROR 
ADInterface::searchSmtpEntryinAD(const string& exchangeServerName, 
						list<string>& entryCNList, 
						list<string>& entryDNList,
						const string& filter)
{
	// find configuration Naming Context DN
	string configurationDN;
	if (getConfigurationDN(configurationDN) == SVE_FAIL) 
	{
		cout << "Configuration Naming Context not found\n";
		return SVE_FAIL;
	}

	// get an Ldap connection
	LDAP* pLdapConnection = getLdapConnection();
	if (pLdapConnection == NULL)
	{
		cout << "Failed to connection to the ldap server\n";
		return SVE_FAIL;
	}

	// ldap bind
	ULONG errorCode = LDAP_SUCCESS;
	ULONG lRtn = 0;
	lRtn = ldap_bind_s(
				pLdapConnection,							// Session Handle
				(const PCHAR) configurationDN.c_str(),		// Use the naming context from the input list
				NULL,										// if it is NULL it will use current user or service credentials 
				LDAP_AUTH_NEGOTIATE);						// LDAP authentication mode
	if(lRtn != LDAP_SUCCESS)
	{
		cout << "ldap_bind_s failed with:" << lRtn << std::endl;
		return SVE_FAIL;
	}

	PCHAR pAttributes[2];
	pAttributes[0] = "cn";
	pAttributes[1] = NULL;
	LDAPMessage* pSearchResult;
	

    //TODO:Reset filter for more smtp entries
	ULONG ulSearchSizeLimit = 0;	// Limit number of users fetched at a time
	LDAP_TIMEVAL ltConnectionTimeout;
	ltConnectionTimeout.tv_sec = 0;		//CONNECTION_ESTABLISHMENT_TIMEOUT;	
	bool bSearchErr = false, bConnectErr = false;

	do
	{
		errorCode = ldap_search_s(pLdapConnection,			
					(const PCHAR) configurationDN.c_str(),	// DN to start search
					LDAP_SCOPE_SUBTREE,						 // Indicate the search scope
					(const PCHAR) filter.c_str(),			// Search Filter
					pAttributes,							// Retrieve list of attributes
					0,										//retrieve both attributesand values
					&pSearchResult);						// this is out parameter - Search results

		if( errorCode == LDAP_SUCCESS )
			break;    
	    
		if( errorCode == LDAP_SERVER_DOWN || 
			errorCode == LDAP_UNAVAILABLE ||
			errorCode == LDAP_NO_MEMORY ||
			errorCode == LDAP_BUSY ||
			errorCode == LDAP_LOCAL_ERROR ||
			errorCode == LDAP_TIMEOUT ||
			errorCode == LDAP_OPERATIONS_ERROR ||
			errorCode == LDAP_TIMELIMIT_EXCEEDED)
		{
			cout << "[ERROR][" << errorCode << "]: ldap_search_s failed. " << ldap_err2string(errorCode) << endl;
			if(pLdapConnection)
			{
				if(ldap_unbind_s(pLdapConnection)==ERROR_SUCCESS)
				{
					cout << "Cleaning up current connection...\n";
					pLdapConnection = NULL;
				}
			}
			if(pSearchResult)
			{
				ldap_msgfree(pSearchResult);
				pSearchResult = NULL;
			}

     		if( (pLdapConnection = getLdapConnectionCustom(configurationDN, ulSearchSizeLimit, ltConnectionTimeout)) == NULL )
			{
				bConnectErr = true;
				break;	
			}
		}
		else
		{
			//DebugPrintf(SV_LOG_FATAL, "ldap_search_s failed with: 0x%0lx \n",errorCode);
			cout << "[FATAL ERROR][:" << errorCode << "]: ldap_search_s failed. " << ldap_err2string(errorCode) << endl;
			if(pSearchResult != NULL)
			{
				ldap_msgfree(pSearchResult);
				pSearchResult = NULL;						
			}
			bSearchErr = true;
			break;
		}
	}while(1);

	if( bConnectErr )
	{
		cout << "[INFO]: Maximum connection attempts " << MAX_LDAP_CONNECT_ATTEMPTS << " has been reached...\n";
		cout << "[ERROR]: Unable to connect to directory server." << endl;
		return SVE_FAIL;
	}

	if( bSearchErr )
	{
		cout << "[ERROR] Error encountered while searching directory server.\n";
		return SVE_FAIL;
	}

	ULONG numberOfEntries = ldap_count_entries(pLdapConnection, pSearchResult);

	// Check if there are any storage groups configured at all
	if(numberOfEntries == NULL)
	{
	    DebugPrintf(SV_LOG_WARNING, "ldap_count_entries failed with 0x%0lx \n", errorCode);
		//cout << "No Entries Found\n";
	    if(pSearchResult != NULL) 
	    {
		    ldap_msgfree(pSearchResult);
			pSearchResult = NULL;
	    }
		
		return SVS_OK;
	}

	LDAPMessage* pLdapMessageEntry = NULL;
	PCHAR pAttribute = NULL;
	BerElement* pBerEncoding = NULL;
	PCHAR* ppValue = NULL;
	ULONG iValue = 0;
	string entryDN;
	string entryDNLowerCase;

	//string exchServerCN = "CN=";
	//exchServerCN += exchangeServerName;
	//string exchServerCNLowerCase = convertoToLower(exchServerCN, exchServerCN.length());

	string exchServerCN = exchangeServerName;
	string exchServerCNLowerCase = convertoToLower(exchServerCN, exchServerCN.length());

	string searchFor;
	// iterate through all storage groups
	for (int index=0; index < (int)numberOfEntries; index++)
	{
	    if(pLdapMessageEntry == NULL)
	        pLdapMessageEntry = ldap_first_entry(pLdapConnection, pSearchResult);
	    else
	        pLdapMessageEntry = ldap_next_entry(pLdapConnection, pLdapMessageEntry);

	    if(pLdapMessageEntry == NULL)
	        break;

		// get storage group DN
		entryDN = ldap_get_dn(pLdapConnection, pLdapMessageEntry);
		entryDNLowerCase = convertoToLower(entryDN, entryDN.length());

		// Is this entry part of the concerned exchange server		
		//cout <<"entryDNLowerCase=" << entryDNLowerCase.c_str() << endl;
		//cout <<"exchServerCNLowerCase=" << exchServerCNLowerCase.c_str() << endl;
		searchFor = "(";
		searchFor += exchServerCNLowerCase + "-{";
		if(strstr(entryDNLowerCase.c_str(), searchFor.c_str()) == NULL)
		{
			searchFor.clear();
			// move to the next entry
			continue;
		}

		// iterate through all requested attributes on a storage group. Currently it's just "cn"
		for ( pAttribute = ldap_first_attribute( pLdapConnection, pLdapMessageEntry, &pBerEncoding );
				pAttribute != NULL; pAttribute = ldap_next_attribute( pLdapConnection, pLdapMessageEntry, pBerEncoding ) ) 
		{
      
			// Print each value of the attribute.
			if ((ppValue = ldap_get_values( pLdapConnection, pLdapMessageEntry, pAttribute)) != NULL ) 
			{
				for ( int i = 0; ppValue[i] != NULL; i++ ) 
				{
					// save the CN and the DN
					entryCNList.push_back(string(ppValue[i]));
					entryDNList.push_back(entryDN);
				}
				// Free memory allocated by ldap_get_values(). 
				ldap_value_free( ppValue );
			}
    
			// Free memory allocated by ldap_first_attribute(). 
			ldap_memfree( pAttribute );
		}
	}

	// Free memory allocated by ldap_search_ext_s().
	if( pSearchResult != NULL)
	{
        ldap_msgfree(pSearchResult);
		pSearchResult = NULL;
	}

	if( pLdapConnection )
	{
		ldap_unbind_s(pLdapConnection);
		pLdapConnection = NULL;
	}

	return SVS_OK;
}


void
ADInterface::compareDNs(const string& srcExchServerName, const string& tgtExchServerName, const list<string>& srcDNs, 
						  const list<string>& tgtDNs,
						  list<string>& missingDNs,
						  list<string>& extraDNs,
						  list<string>& matchingDNs)
{
	list<string>::const_iterator srcIter;
	list<string>::const_iterator tgtIter;
	bool found=false;

	for (srcIter=srcDNs.begin(); srcIter != srcDNs.end(); srcIter++)
	{
		string replacementDN;
		ULONG numReplaced;
		switchCNValues(srcExchServerName, tgtExchServerName, (*srcIter), replacementDN, numReplaced, CN_SEPARATOR_COMMA);
		if (replacementDN.empty()) 
		{
			cout << "error replacing in DN " << replacementDN.c_str() << std::endl;
			continue;
		}
		found = false;

		//cout << "Source DN = " << (*srcIter).c_str() << std::endl;
		for (tgtIter = tgtDNs.begin(); tgtIter != tgtDNs.end(); tgtIter++) 
		{
			//cout << "Target CNs = " << (*tgtIter).c_str() << std::endl;
			if (strcmpi(replacementDN.c_str(), (*tgtIter).c_str()) == 0)
			{
				found = true;
				//cout << "************** FOUND matching DN " << (*srcIter).c_str() << std::endl;
				matchingDNs.push_back(*tgtIter);
				break;
			}
		}
		if (false == found) 
		{
			// source storage group not found in target
			//cout << "Source Storage Group " << (*srcIter) << " is missing in target\n";
			// track the missing storage groups so that they can be created on the target later
			missingDNs.push_back(replacementDN);
		}
	}

	for (tgtIter=tgtDNs.begin(); tgtIter != tgtDNs.end(); tgtIter++)
	{
		found = false;

		string replacementDN;
		ULONG numReplaced;
		switchCNValues(tgtExchServerName, srcExchServerName, (*tgtIter), replacementDN, numReplaced, CN_SEPARATOR_COMMA);
		if (replacementDN.empty()) 
		{
			cout << "error replacing in DN " << replacementDN.c_str() << std::endl;
			continue;
		}

		for (srcIter = srcDNs.begin(); srcIter != srcDNs.end(); srcIter++) 
		{
			if (strcmpi((*srcIter).c_str(), replacementDN.c_str()) == 0)
			{
				found = true;
				break;
			}
		}
		if (false == found) 
		{
			// target storage group not found in source
			cout << "Found Extra DN = " << (*tgtIter).c_str() << std::endl;
			extraDNs.push_back(*tgtIter);
		}
	}
}

SVERROR
ADInterface::createStorageGroupsOnTarget(const string& srcExchServerName,
						   const string& tgtExchServerName, const string& missingStorageGroup,
						   map<string, string>& srcTotgtVolMapping)
{
	// find configuration Naming Context DN
	string configurationDN;
	if (getConfigurationDN(configurationDN) == SVE_FAIL) 
	{
		cout << "Configuration Naming Context not found\n";
		return SVE_FAIL;
	}

	// get an Ldap connection
	LDAP* pLdapConnection = getLdapConnection();
	if (pLdapConnection == NULL)
	{
		cout << "Failed to connection to the ldap server\n";
		return SVE_FAIL;
	}

	// ldap bind
	ULONG errorCode = LDAP_SUCCESS;
	ULONG lRtn = 0;
	lRtn = ldap_bind_s(	pLdapConnection,
				(const PCHAR) configurationDN.c_str(),		// Use the naming context from the input list
				NULL,										// if it is NULL it will use current user or service credentials 
				LDAP_AUTH_NEGOTIATE);						// LDAP authentication mode
	if(lRtn != LDAP_SUCCESS)
	{
		cout << "ldap_bind_s failed with:" << lRtn << std::endl;
		return SVE_FAIL;
	}

	LDAPMessage* pSearchResult;
	string filter;
	string exchServerCN = "CN=";
	map<string, char** > attrValuePairs;
	map<string, berval** > attrValuePairsBin;
	char** pValues;
	berval** pValuesBin;
	exchServerCN += srcExchServerName;
	string exchServerCNLowerCase = convertoToLower(exchServerCN, exchServerCN.length());

	cout << "creating storage group : " << missingStorageGroup.c_str() << std::endl;
	// the only difference between the source and the target DN is the server name "CN=<server_name>"
	// Construct a new DN by replacing the "CN=<source_server_name>" entry in the obtained DN to 
	// "CN=<target_server_name>".
	string replacementDN;
	ULONG numReplaced;
	switchCNValues(tgtExchServerName, srcExchServerName, missingStorageGroup, replacementDN, numReplaced, CN_SEPARATOR_COMMA);
	if (replacementDN.empty()) 
	{
		cout << "error replacing in DN " << replacementDN.c_str() << std::endl;
		return SVE_FAIL;
	}
	// construct the filter
	filter = "(distinguishedName=";
	filter += replacementDN.c_str();
	filter += ")";

	ULONG ulSearchSizeLimit = 0;	// Limit number of users fetched at a time
	LDAP_TIMEVAL ltConnectionTimeout;
	ltConnectionTimeout.tv_sec = 0;		//CONNECTION_ESTABLISHMENT_TIMEOUT;	
	bool bSearchErr = false, bConnectErr = false;

	do
	{
		errorCode = ldap_search_s(pLdapConnection,						
					(const PCHAR) configurationDN.c_str(),	// DN to start search
					LDAP_SCOPE_SUBTREE,						 // Indicate the search scope
					(const PCHAR) filter.c_str(),			// Search Filter
					NULL,									// Retrieve list of attributes
					0,										//retrieve both attributesand values
					&pSearchResult);						// this is out parameter - Search results
		if( errorCode == LDAP_SUCCESS )
			break;    
	    
		if( errorCode == LDAP_SERVER_DOWN || 
			errorCode == LDAP_UNAVAILABLE ||
			errorCode == LDAP_NO_MEMORY ||
			errorCode == LDAP_BUSY ||
			errorCode == LDAP_LOCAL_ERROR ||
			errorCode == LDAP_TIMEOUT ||
			errorCode == LDAP_OPERATIONS_ERROR ||
			errorCode == LDAP_TIMELIMIT_EXCEEDED)
		{
			cout << "[ERROR][" << errorCode << "]: ldap_search_s failed. " << ldap_err2string(errorCode) << endl;
			if(pLdapConnection)
			{
				if(ldap_unbind_s(pLdapConnection)==ERROR_SUCCESS)
				{
					cout << "Cleaning up current connection...\n";
					pLdapConnection = NULL;
				}
			}
			if(pSearchResult)
			{
				ldap_msgfree(pSearchResult);
				pSearchResult = NULL;
			}

     		if( (pLdapConnection = getLdapConnectionCustom(configurationDN, ulSearchSizeLimit, ltConnectionTimeout)) == NULL )
			{
				bConnectErr = true;
				break;	
			}
		}
		else
		{
			//DebugPrintf(SV_LOG_FATAL, "ldap_search_s failed with: 0x%0lx \n",errorCode);
			cout << "[FATAL ERROR][:" << errorCode << "]: ldap_search_s failed. " << ldap_err2string(errorCode) << endl;
			if(pSearchResult != NULL)
			{
				ldap_msgfree(pSearchResult);
				pSearchResult = NULL;						
			}
			bSearchErr = true;
			break;
		}
	}while(1);

	if( bConnectErr )
	{
		cout << "[INFO]: Maximum connection attempts " << MAX_LDAP_CONNECT_ATTEMPTS << " has been reached...\n";
		cout << "[ERROR]: Unable to connect to directory server." << endl;
		return SVE_FAIL;
	}

	if( bSearchErr )
	{
		cout << "[ERROR] Error encountered while searching directory server.\n";
		return SVE_FAIL;
	}
	
	ULONG numberOfEntries = ldap_count_entries(pLdapConnection, pSearchResult);

	// Check if there are any storage groups configured at all. NOTE: This is a fatal error
	// because the reason whey we are here is because this storage group did exist in source
	if(numberOfEntries == NULL)
	{
		cout  << "Failed to find entry " << replacementDN.c_str() << " LDAP error = " << errorCode << std::endl;
	    if(pSearchResult != NULL) 
	    {
		    ldap_msgfree(pSearchResult);
			pSearchResult = NULL;
	    }
		
		return SVE_FAIL;
	}

	LDAPMessage* pLdapMessageEntry = NULL;
	PCHAR pAttribute = NULL;
	BerElement* pBerEncoding = NULL;
	PCHAR* ppValue = NULL;
	berval** binData;
	ULONG iValue = 0;
	string entryDN;
	string entryDNLowerCase;

	// iterate through all storage groups
	for (int index=0; index < (int)numberOfEntries; index++)
	{
	    if(pLdapMessageEntry == NULL)
	        pLdapMessageEntry = ldap_first_entry(pLdapConnection, pSearchResult);
	    else
	        pLdapMessageEntry = ldap_next_entry(pLdapConnection, pLdapMessageEntry);

	    if(pLdapMessageEntry == NULL)
	        break;

		// get storage group DN
		entryDN = ldap_get_dn(pLdapConnection, pLdapMessageEntry);

		string replacementAttrValue;
		// iterate through all requested attributes on a storage group. Currently it's just "cn"
		for ( pAttribute = ldap_first_attribute( pLdapConnection, pLdapMessageEntry, &pBerEncoding );
				pAttribute != NULL; pAttribute = ldap_next_attribute( pLdapConnection, pLdapMessageEntry, pBerEncoding ) ) 
		{
			// skip attribute objectGUID
			if ((strcmpi(pAttribute, "distinguishedName") == 0) ||
				(strcmpi(pAttribute, "cn") == 0) ||					
				(strcmpi(pAttribute, "name") == 0) ||
				(strcmpi(pAttribute, "objectClass") == 0) ||
				(strcmpi(pAttribute, "objectGUID") == 0))
			{
				ldap_memfree( pAttribute );
				continue;
			}

			// handle binary attributes
			//cout << "Processing Attribute = " << pAttribute << "\n";
			if ((strcmpi(pAttribute, "unmergedAtts") == 0) || 
				(strcmpi(pAttribute, "dSASignature") == 0) ||
				(strcmpi(pAttribute, "mS-DS-ConsistencyGuid") == 0) ||
				(strcmpi(pAttribute, "msExchUnmergedAttsPt") == 0) ||
				(strcmpi(pAttribute, "partialAttributeDeletionList") == 0) ||
				(strcmpi(pAttribute, "partialAttributeSet") == 0) ||
				(strcmpi(pAttribute, "replPropteryMetaData") == 0) ||
				(strcmpi(pAttribute, "replUpToDateVector") == 0) ||
				(strcmpi(pAttribute, "repsFrom") == 0) ||
				(strcmpi(pAttribute, "repsTo") == 0) ||
				(strcmpi(pAttribute, "replicationSignature") == 0))
			{
				if ((binData = ldap_get_values_len( pLdapConnection, pLdapMessageEntry, pAttribute)) != NULL )
				{
					//cout << "Found " << ldap_count_values_len(binData) << " Values\n";
					
                    size_t vbinlen;
                    INM_SAFE_ARITHMETIC(vbinlen = InmSafeInt<ULONG>::Type(ldap_count_values_len(binData))+1, INMAGE_EX(ldap_count_values_len(binData)))
					pValuesBin = (berval **) calloc(vbinlen, sizeof(berval *));
					ULONG i;
					for ( i = 0; i < ldap_count_values_len(binData); i++ )
					{
						//cout << "Length = " << binData[i]->bv_len << "\n";
						pValuesBin[i] = ber_bvdup(binData[i]);
					}
					pValuesBin[i] = NULL;

					attrValuePairsBin[string(pAttribute)] = pValuesBin;

					// Free memory allocated by ldap_get_values_len(). 
					ldap_value_free_len( binData );
				}
			}
			// handle non-binary (string) attributes
			else if ((ppValue = ldap_get_values( pLdapConnection, pLdapMessageEntry, pAttribute)) != NULL )
			{
				// allocate memory for multivalued attributes. Max of 16 values
				//cout << "Found " << ldap_count_values(ppValue) << " attributes\n";
				char drive[256];
				int valIndex = 0;
                size_t valuelen;
                INM_SAFE_ARITHMETIC(valuelen = InmSafeInt<ULONG>::Type(ldap_count_values(ppValue))+1, INMAGE_EX(ldap_count_values(ppValue)))
				pValues = (char **) calloc(valuelen, sizeof(char *));
				for ( int i = 0; ppValue[i] != NULL; i++, valIndex++ )
				{
					replacementAttrValue.clear();
					// switch CN values that may have the source server name
					//cout << "Found Attribute = " << pAttribute << " Value = " << ppValue[i] << std::endl;
					switchCNValues(srcExchServerName, tgtExchServerName, ppValue[i], replacementAttrValue,
									numReplaced, CN_SEPARATOR_COMMA);

					// handle the case where the value of the "msExchESEParamLogFilePath" differs on source and target
					if ((srcTotgtVolMapping.empty() == false) && (strcmpi(pAttribute, "msExchESEParamLogFilePath") == 0)) {
						char *srcDrive = (char *)(replacementAttrValue.c_str());
						// PR#10815: Long Path support
                        SVGetVolumePathName(srcDrive, drive, ARRAYSIZE(drive));
						int len = strlen(drive);
						if(drive[len-1] == '\\')
							drive[len-1] = '\0';						
						const char *tgtDrive = srcTotgtVolMapping[string(drive)].c_str();
						replacementAttrValue.replace(0, strlen(tgtDrive), tgtDrive);
					}

					// handle the case where the value of "dSCorePropagationData" contains duplicate values
					if( (strcmpi(pAttribute, "dSCorePropagationData") == 0) &&  (valIndex > 0) )
					{							
                            if(strcmpi(pValues[valIndex-1], replacementAttrValue.c_str()) == 0)
							{
								valIndex--;
                                continue;
							}
					}
                    size_t pvslen;
                    INM_SAFE_ARITHMETIC(pvslen = InmSafeInt<size_t>::Type(replacementAttrValue.length())+1, INMAGE_EX(replacementAttrValue.length()))
					// copy the attribute value					
					pValues[valIndex] = (char *)calloc(pvslen, sizeof(char));
                    inm_strcpy_s(pValues[valIndex], pvslen, replacementAttrValue.c_str());
					//cout << "Replaced with Attribute = " << pAttribute << " Value = " << pValues[valIndex] << std::endl;					
				}
				pValues[valIndex] = NULL;
				attrValuePairs[string(pAttribute)] = pValues;

				// Free memory allocated by ldap_get_values(). 
				ldap_value_free( ppValue );
			}
	
			// Free memory allocated by ldap_first_attribute(). 
			ldap_memfree( pAttribute );
		}

		// add this storage group pointed to by iter to AD with all its attributes
		//cout << "\nCreating storage Group " << missingStorageGroup.c_str() << std::endl;
		addEntryToAD(pLdapConnection, missingStorageGroup, "msExchStorageGroup", attrValuePairs, attrValuePairsBin);

		// free memory of non-binary attributes
		map<string, char**>::const_iterator iter = attrValuePairs.begin();
		for (; iter != attrValuePairs.end(); iter++) {
			pValues = iter->second;
			for (int index=0; pValues[index] != NULL; index++) {
				free(pValues[index]);
			}
			free(pValues);
		}

		// free memory of binary attributes
		map<string, PBERVAL*>::const_iterator iterBin = attrValuePairsBin.begin();
		for (; iterBin != attrValuePairsBin.end(); iterBin++) {
			pValuesBin = iterBin->second;
			for (int index=0; pValuesBin[index] != NULL; index++) {
				ber_bvfree(pValuesBin[index]);
			}
			free(pValuesBin);
		}
	}

	//cout << "\n******Successfully created Storage Group " << missingStorageGroup.c_str() << std::endl;
	if( pLdapConnection )
	{
		ldap_unbind_s(pLdapConnection);
		pLdapConnection = NULL;
	}
	return SVS_OK;
}

SVERROR
ADInterface::updateMatchingStorageGroupsOnTarget(const string& srcExchServerName,
						   const string& tgtExchServerName,
						   const list<string>& matchingStorageGroups,
						   map<string, string>& srcTotgtVolMapping)
{
	// find configuration Naming Context DN
	string configurationDN;
	if (getConfigurationDN(configurationDN) == SVE_FAIL) 
	{
		cout << "Configuration Naming Context not found\n";
		return SVE_FAIL;
	}

	// get an Ldap connection
	LDAP* pLdapConnection = getLdapConnection();
	if (pLdapConnection == NULL)
	{
		cout << "Failed to connection to the ldap server\n";
		return SVE_FAIL;
	}

	// ldap bind
	ULONG errorCode = LDAP_SUCCESS;
	ULONG lRtn = 0;
	lRtn = ldap_bind_s(pLdapConnection,						
				(const PCHAR) configurationDN.c_str(),		// the distinguished name that is use to bind.
				NULL,										// if it is NULL it will use current user or service credentials 
				LDAP_AUTH_NEGOTIATE);						// LDAP authentication mode
	if(lRtn != LDAP_SUCCESS)
	{
		cout << "ldap_bind_s failed with:" << lRtn << std::endl;
		return SVE_FAIL;
	}

	LDAPMessage* pSearchResult;
	list<string>::const_iterator iter;
	string filter;
	string exchServerCN = "CN=";
	map<string, char** > attrValuePairs;
	map<string, berval** > attrValuePairsBin;
	char** pValues;
	berval** pValuesBin;
	exchServerCN += srcExchServerName;
	string exchServerCNLowerCase = convertoToLower(exchServerCN, exchServerCN.length());
	string replacementDN;

	// search in source: for all the missing storage group on target
	for (iter=matchingStorageGroups.begin(); iter != matchingStorageGroups.end(); iter++) 
	{
		// the only difference between the source and the target DN is the server name "CN=<server_name>"
		// Construct a new DN by replacing the "CN=<source_server_name>" entry in the obtained DN to 
		// "CN=<target_server_name>".
		replacementDN.clear();
		ULONG numReplaced;
		switchCNValues(tgtExchServerName, srcExchServerName, (*iter), replacementDN, numReplaced, CN_SEPARATOR_COMMA);
		if (replacementDN.empty()) 
		{
			cout << "error replacing in DN " << replacementDN.c_str() << std::endl;
			continue;
		}
		// construct the filter
		filter = "(distinguishedName=";
		filter += replacementDN.c_str();
		filter += ")";
			
		ULONG ulSearchSizeLimit = 0;	// Limit number of users fetched at a time
		LDAP_TIMEVAL ltConnectionTimeout;
		ltConnectionTimeout.tv_sec = 0;		//CONNECTION_ESTABLISHMENT_TIMEOUT;	
		bool bSearchErr = false, bConnectErr = false;

		do
		{
			errorCode = ldap_search_s(pLdapConnection,						// Session handle
						(const PCHAR) configurationDN.c_str(),	// DN to start search
						LDAP_SCOPE_SUBTREE,						 // Indicate the search scope
						(const PCHAR) filter.c_str(),			// Search Filter
						NULL,									// Retrieve list of attributes
						0,										//retrieve both attributesand values
						&pSearchResult);						// this is out parameter - Search results
	    	if( errorCode == LDAP_SUCCESS )
				break;    
		    
			if( errorCode == LDAP_SERVER_DOWN || 
				errorCode == LDAP_UNAVAILABLE ||
				errorCode == LDAP_NO_MEMORY ||
				errorCode == LDAP_BUSY ||
				errorCode == LDAP_LOCAL_ERROR ||
				errorCode == LDAP_TIMEOUT ||
				errorCode == LDAP_OPERATIONS_ERROR ||
				errorCode == LDAP_TIMELIMIT_EXCEEDED)
			{
				cout << "[ERROR][" << errorCode << "]: ldap_search_s failed. " << ldap_err2string(errorCode) << endl;
				if(pLdapConnection)
				{
					if(ldap_unbind_s(pLdapConnection)==ERROR_SUCCESS)
					{
						cout << "Cleaning up current connection...\n";
						pLdapConnection = NULL;
					}
				}
				if(pSearchResult)
				{
					ldap_msgfree(pSearchResult);
					pSearchResult = NULL;
				}

     			if( (pLdapConnection = getLdapConnectionCustom(configurationDN, ulSearchSizeLimit, ltConnectionTimeout)) == NULL )
				{
					bConnectErr = true;
					break;	
				}
			}
			else
			{
				//DebugPrintf(SV_LOG_FATAL, "ldap_search_s failed with: 0x%0lx \n",errorCode);
				cout << "[FATAL ERROR][:" << errorCode << "]: ldap_search_s failed. " << ldap_err2string(errorCode) << endl;
				if(pSearchResult != NULL)
				{
					ldap_msgfree(pSearchResult);
					pSearchResult = NULL;						
				}
				bSearchErr = true;
				break;
			}
		}while(1);

		if( bConnectErr )
		{
			cout << "[INFO]: Maximum connection attempts " << MAX_LDAP_CONNECT_ATTEMPTS << " has been reached...\n";
			cout << "[ERROR]: Unable to connect to directory server." << endl;
			continue; //next sg
		}

		if( bSearchErr )
		{
			cout << "[ERROR] Error encountered while searching directory server.\n";
			continue; //next sg
		}		
		ULONG numberOfEntries = ldap_count_entries(pLdapConnection, pSearchResult);

		// Check if there are any storage groups configured at all. NOTE: This is a fatal error
		// because the reason whey we are here is because this storage group did exist in source
		if(numberOfEntries == NULL)
		{
			cout  << "Failed to find entry " << replacementDN.c_str() << " LDAP error = " << errorCode << std::endl;
		    if(pSearchResult != NULL) 
		    {
			    ldap_msgfree(pSearchResult);
				pSearchResult = NULL;
		    }
			return SVE_FAIL;
		}

		LDAPMessage* pLdapMessageEntry = NULL;
		PCHAR pAttribute = NULL;
		BerElement* pBerEncoding = NULL;
		PCHAR* ppValue = NULL;
		berval** binData;
		ULONG iValue = 0;
		string entryDN;
		string entryDNLowerCase;

		// iterate through all storage groups
		for (int index=0; index < (int)numberOfEntries; index++)
		{
		    if(pLdapMessageEntry == NULL)
		        pLdapMessageEntry = ldap_first_entry(pLdapConnection, pSearchResult);
		    else
		        pLdapMessageEntry = ldap_next_entry(pLdapConnection, pLdapMessageEntry);

		    if(pLdapMessageEntry == NULL)
		        break;

			// get storage group DN
			entryDN = ldap_get_dn(pLdapConnection, pLdapMessageEntry);

			string replacementAttrValue;
			// iterate through all requested attributes on a storage group. Currently it's just "cn"
			for ( pAttribute = ldap_first_attribute( pLdapConnection, pLdapMessageEntry, &pBerEncoding );
					pAttribute != NULL; pAttribute = ldap_next_attribute( pLdapConnection, pLdapMessageEntry, pBerEncoding ) ) 
			{
				// skip attribute objectGUID
				if ((strcmpi(pAttribute, "distinguishedName") == 0) ||
					(strcmpi(pAttribute, "cn") == 0) ||					
					(strcmpi(pAttribute, "name") == 0) ||
					(strcmpi(pAttribute, "objectClass") == 0) ||
					(strcmpi(pAttribute, "objectGUID") == 0))
				{
					ldap_memfree( pAttribute );
					continue;
				}

				// handle binary attributes
				//cout << "Processing Attribute = " << pAttribute << "\n";
				if ((strcmpi(pAttribute, "unmergedAtts") == 0) || 
					(strcmpi(pAttribute, "dSASignature") == 0) ||
					(strcmpi(pAttribute, "mS-DS-ConsistencyGuid") == 0) ||
					(strcmpi(pAttribute, "msExchUnmergedAttsPt") == 0) ||
					(strcmpi(pAttribute, "partialAttributeDeletionList") == 0) ||
					(strcmpi(pAttribute, "partialAttributeSet") == 0) ||
					(strcmpi(pAttribute, "replPropteryMetaData") == 0) ||
					(strcmpi(pAttribute, "replUpToDateVector") == 0) ||
					(strcmpi(pAttribute, "repsFrom") == 0) ||
					(strcmpi(pAttribute, "repsTo") == 0) ||
					(strcmpi(pAttribute, "replicationSignature") == 0))
				{
					if ((binData = ldap_get_values_len( pLdapConnection, pLdapMessageEntry, pAttribute)) != NULL )
					{
						//cout << "Found " << ldap_count_values_len(binData) << " Values\n";
                        size_t vbinlen;
                        INM_SAFE_ARITHMETIC(vbinlen = InmSafeInt<ULONG>::Type(ldap_count_values_len(binData))+1, INMAGE_EX(ldap_count_values_len(binData)))
						pValuesBin = (berval **) calloc(vbinlen, sizeof(berval *));
						ULONG i;
						for ( i = 0; i < ldap_count_values_len(binData); i++ )
						{
							//cout << "Length = " << binData[i]->bv_len << "\n";
							pValuesBin[i] = ber_bvdup(binData[i]);
						}
						pValuesBin[i] = NULL;

						attrValuePairsBin[string(pAttribute)] = pValuesBin;

						// Free memory allocated by ldap_get_values_len(). 
						ldap_value_free_len( binData );
					}
				}
				else if ((strcmpi(pAttribute, "msExchESEParamLogFilePath") == 0) ||
						(strcmpi(pAttribute, "msExchESEParamLogFileSize") == 0) ||
						(strcmpi(pAttribute, "msExchESEParamSystemPath") == 0) ||
						(strcmpi(pAttribute, "msExchESEParamBaseName") == 0) ||
						(strcmpi(pAttribute, "msExchESEParamDbExtenstionSize") == 0) ||
						(strcmpi(pAttribute, "msExchESEParamEnableIndexChecking") == 0) ||
						(strcmpi(pAttribute, "msExchESEParamEnableOnlineDefrag") == 0) ||
						(strcmpi(pAttribute, "msExchESEParamPageFragment") == 0) ||
						(strcmpi(pAttribute, "msExchESEParamPageTempDBMin") == 0) ||
						(strcmpi(pAttribute, "msExchESEParamZeroDatabaseDuringBackup") == 0) ||
						(strcmpi(pAttribute, "msExchRecovery") == 0))
				{
					// handle non-binary (string) attributes
					if ((ppValue = ldap_get_values( pLdapConnection, pLdapMessageEntry, pAttribute)) != NULL )
					{ 
						// allocate memory for multivalued attributes. Max of 16 values
						//cout << "Found " << ldap_count_values(ppValue) << " attributes\n";
                        size_t valuelen;
                        INM_SAFE_ARITHMETIC(valuelen = InmSafeInt<ULONG>::Type(ldap_count_values(ppValue))+1, INMAGE_EX(ldap_count_values(ppValue)))
						pValues = (char **) calloc(valuelen, sizeof(char *));
						ULONG i;
						for ( i = 0; ppValue[i] != NULL; i++ )
						{
							char drive[256];
							replacementAttrValue.clear();
							// switch CN values that may have the source server name
							//cout << "Found Attribute = " << pAttribute << " Value = " << ppValue[i] << std::endl;
							switchCNValues(srcExchServerName, tgtExchServerName, ppValue[i], replacementAttrValue,
											numReplaced, CN_SEPARATOR_COMMA);
							// replace the source drive letter with the target drive letter
							if (srcTotgtVolMapping.empty() == false) {
								if ((strcmpi(pAttribute, "msExchESEParamLogFilePath") == 0) ||
									(strcmpi(pAttribute, "msExchESEParamSystemPath") == 0)) {
									char *srcDrive = (char *)(replacementAttrValue.c_str());
									cout << "replacementAttrValue.c_str() = "<<srcDrive << endl;
									// PR#10815: Long Path support
                                    SVGetVolumePathName(srcDrive, drive, ARRAYSIZE(drive));
									int len = strlen(drive);
									if(drive[len-1] == '\\')
									drive[len-1] = '\0';									
									const char *tgtDrive = srcTotgtVolMapping[string(drive)].c_str();									
									replacementAttrValue.replace(0, strlen(tgtDrive), tgtDrive);									
								}
							}

							size_t pvslen;
                            INM_SAFE_ARITHMETIC(pvslen = InmSafeInt<size_t>::Type(replacementAttrValue.length())+1, INMAGE_EX(replacementAttrValue.length()))
							// copy the attribute value
							pValues[i] = (char *)calloc(pvslen, sizeof(char));
                            inm_strcpy_s(pValues[i], pvslen, replacementAttrValue.c_str());
							//cout << "Replaced Attribute = " << pAttribute << " Value = " << pValues[i] << std::endl;
						}
						if(i==0)
						{
							free(pValues);
						}
						else
						{
							pValues[i] = NULL;
							attrValuePairs[string(pAttribute)] = pValues;
						}

						// Free memory allocated by ldap_get_values(). 
						ldap_value_free( ppValue );
					}
				}
		
				// Free memory allocated by ldap_first_attribute(). 
				ldap_memfree( pAttribute );
			}

			// add this storage group pointed to by iter to AD with all its attributes
			//cout << "\nCreating storage Group " << (*iter ).c_str() << std::endl;
			modifyADEntry(pLdapConnection, (*iter), attrValuePairs, attrValuePairsBin);

			// free memory of non-binary attributes
			map<string, char**>::const_iterator iter = attrValuePairs.begin();
			for (; iter != attrValuePairs.end(); iter++) {
				pValues = iter->second;
				for (int index=0; pValues[index] != NULL; index++) {
					free(pValues[index]);
				}
				free(pValues);
			}

			// free memory of binary attributes
			map<string, PBERVAL*>::const_iterator iterBin = attrValuePairsBin.begin();
			for (; iterBin != attrValuePairsBin.end(); iterBin++) {
				pValuesBin = iterBin->second;
				for (int index=0; pValuesBin[index] != NULL; index++) {
					ber_bvfree(pValuesBin[index]);
				}
				free(pValuesBin);
			}
		}
	}

	//cout << "Completed creating storage groups on target" << std::endl;
	if( pLdapConnection )
	{
		ldap_unbind_s(pLdapConnection);
		pLdapConnection = NULL;
	}
	
	return SVS_OK;
}

SVERROR
ADInterface::searchAndCompareInformationStores(const string& srcExchServerNameIn, const string& tgtExchServerNameIn,
											   list<string> &extraIS)
{
	string srcExchServerName = convertToUpper(srcExchServerNameIn);
	string tgtExchServerName = convertToUpper(tgtExchServerNameIn);

	// search and compare the Information Subtree on source and target exchange servers
	bool bSrcSearch = true;
	bool bTgtSearch = true;

	// find storage groups on the source
	list<string> srcISCN, tgtISCN, srcISDN, tgtISDN;
	if (searchEntryinAD(srcExchServerName, srcISCN, srcISDN, string("(objectclass=msExchInformationStore)")) == SVE_FAIL) 
	{
		bSrcSearch = false;
	}

	// find storage groups on the target
	if (searchEntryinAD(tgtExchServerName, tgtISCN, tgtISDN, string("(objectclass=msExchInformationStore)")) == SVE_FAIL) 
	{
		bTgtSearch = false;
	}

	if (bSrcSearch == false || bTgtSearch == false) 
	{
		cout << "Failover Cannot Continue " << std::endl;
		return SVE_FAIL;
	}

	cout << "Searching for Information Stores on the source\n";
	list<string> missingIS, matchingIS;
	// find the missing, matching and extra storage groups on the target
	compareDNs(srcExchServerName, tgtExchServerName, srcISDN, tgtISDN, missingIS, extraIS, matchingIS);

	//cout << "Source and Target Storage Groups Do not exactly match\n\n";
	if (missingIS.empty() == false) 
	{
		cout << "Attempting to create the missing storage Groups ...Please Wait\n";
		if (createInformationStoresOnTarget(srcExchServerName, tgtExchServerName, missingIS) == SVE_FAIL)
		{
			cout << "Failed to create the missing Information Stores On Target. Failover cannot continue\n";
			return SVE_FAIL;
		}
		else 
		{
			//cout << "Successfully created the missing Information Stores On Target.\n\n";
		}
	}
	if (matchingIS.empty() == false)
	{
		if (updateMatchingInformationStoresOnTarget(srcExchServerName, tgtExchServerName, matchingIS) == SVE_FAIL)
		{
			cout << "Failed to update matching Information Stores On Target. Failover cannot continue\n";
			return SVE_FAIL;
		}
		else 
		{
			//cout << "Successfully updated the matching Information Stores On Target.\n\n";
		}
		
	}

	return SVS_OK;
}

SVERROR
ADInterface::createInformationStoresOnTarget(const string& srcExchServerName,
						   const string& tgtExchServerName,
						   const list<string>& informationStores)
{
	// find configuration Naming Context DN
	string configurationDN;
	if (getConfigurationDN(configurationDN) == SVE_FAIL) 
	{
		cout << "Configuration Naming Context not found\n";
		return SVE_FAIL;
	}

	// get an Ldap connection
	LDAP* pLdapConnection = getLdapConnection();
	if (pLdapConnection == NULL)
	{
		cout << "Failed to connection to the ldap server\n";
		return SVE_FAIL;
	}

	// ldap bind
	ULONG errorCode = LDAP_SUCCESS;
	ULONG lRtn = 0;
	lRtn = ldap_bind_s(pLdapConnection,							
				(const PCHAR) configurationDN.c_str(),		// the distinguished name that is use to bind.
				NULL,										// if it is NULL it will use current user or service credentials 
				LDAP_AUTH_NEGOTIATE);						// LDAP authentication mode
	if(lRtn != LDAP_SUCCESS)
	{
		cout << "ldap_bind_s failed with:" << lRtn << std::endl;
		return SVE_FAIL;
	}

	LDAPMessage* pSearchResult;
	list<string>::const_iterator iter;
	string filter;
	string exchServerCN = "CN=";
	map<string, char** > attrValuePairs;
	map<string, berval** > attrValuePairsBin;
	char** pValues;
	berval** pValuesBin;
	exchServerCN += srcExchServerName;
	string exchServerCNLowerCase = convertoToLower(exchServerCN, exchServerCN.length());

	// search in source: for all the missing storage group on target
	for (iter=informationStores.begin(); iter != informationStores.end(); iter++) 
	{
		// the only difference between the source and the target DN is the server name "CN=<server_name>"
		// Construct a new DN by replacing the "CN=<source_server_name>" entry in the obtained DN to 
		// "CN=<target_server_name>".
		string replacementDN;
		ULONG numReplaced;
		switchCNValues(tgtExchServerName, srcExchServerName, (*iter), replacementDN, numReplaced, CN_SEPARATOR_COMMA);
		if (replacementDN.empty()) 
		{
			cout << "error replacing in DN " << replacementDN.c_str() << std::endl;
			continue;
		}
		// construct the filter
		filter = "(distinguishedName=";
		filter += replacementDN;
		filter += ")";

		ULONG ulSearchSizeLimit = 0;	// Limit number of users fetched at a time
		LDAP_TIMEVAL ltConnectionTimeout;
		ltConnectionTimeout.tv_sec = 0;		//CONNECTION_ESTABLISHMENT_TIMEOUT;	
		bool bSearchErr = false, bConnectErr = false;

		do
		{
			errorCode = ldap_search_s(pLdapConnection,						
					(const PCHAR) configurationDN.c_str(),	// DN to start search
					LDAP_SCOPE_SUBTREE,						 // Indicate the search scope
					(const PCHAR) filter.c_str(),			// Search Filter
					NULL,									// Retrieve list of attributes
					0,										//retrieve both attributesand values
					&pSearchResult);						// this is out parameter - Search results

	    	if( errorCode == LDAP_SUCCESS )
				break;    
		    
			if( errorCode == LDAP_SERVER_DOWN || 
				errorCode == LDAP_UNAVAILABLE ||
				errorCode == LDAP_NO_MEMORY ||
				errorCode == LDAP_BUSY ||
				errorCode == LDAP_LOCAL_ERROR ||
				errorCode == LDAP_TIMEOUT ||
				errorCode == LDAP_OPERATIONS_ERROR ||
				errorCode == LDAP_TIMELIMIT_EXCEEDED)
			{
				cout << "[ERROR][" << errorCode << "]: ldap_search_s failed. " << ldap_err2string(errorCode) << endl;
				if(pLdapConnection)
				{
					if(ldap_unbind_s(pLdapConnection)==ERROR_SUCCESS)
					{
						cout << "Cleaning up current connection...\n";
						pLdapConnection = NULL;
					}
				}
				if(pSearchResult)
				{
					ldap_msgfree(pSearchResult);
					pSearchResult = NULL;
				}

     			if( (pLdapConnection = getLdapConnectionCustom(configurationDN, ulSearchSizeLimit, ltConnectionTimeout)) == NULL )
				{
					bConnectErr = true;
					break;	
				}
			}
			else
			{
				//DebugPrintf(SV_LOG_FATAL, "ldap_search_s failed with: 0x%0lx \n",errorCode);
				cout << "[FATAL ERROR][:" << errorCode << "]: ldap_search_s failed. " << ldap_err2string(errorCode) << endl;
				if(pSearchResult != NULL)
				{
					ldap_msgfree(pSearchResult);
					pSearchResult = NULL;						
				}
				bSearchErr = true;
				break;
			}
		}while(1);

		if( bConnectErr )
		{
			cout << "[INFO]: Maximum connection attempts " << MAX_LDAP_CONNECT_ATTEMPTS << " has been reached...\n";
			cout << "[ERROR]: Unable to connect to directory server." << endl;
			continue; //next is
		}

		if( bSearchErr )
		{
			cout << "[ERROR] Error encountered while searching directory server.\n";
			continue; //next is
		}		
		ULONG numberOfEntries = ldap_count_entries(pLdapConnection, pSearchResult);

		// Check if there are any storage groups configured at all. NOTE: This is a fatal error
		// because the reason whey we are here is because this storage group did exist in source
		if(numberOfEntries == NULL)
		{
			cout  << "Failed to find entry " << replacementDN.c_str() << "LDAP error = " << errorCode << std::endl;
		    if(pSearchResult != NULL) 
		    {
			    ldap_msgfree(pSearchResult);
				pSearchResult = NULL;
		    }
			
			return SVE_FAIL;
		}

		LDAPMessage* pLdapMessageEntry = NULL;
		PCHAR pAttribute = NULL;
		BerElement* pBerEncoding = NULL;
		PCHAR* ppValue = NULL;
		berval** binData;
		ULONG iValue = 0;
		string entryDN;
		string entryDNLowerCase;

		// iterate through all storage groups
		for (int index=0; index < (int)numberOfEntries; index++)
		{
		    if(pLdapMessageEntry == NULL)
		        pLdapMessageEntry = ldap_first_entry(pLdapConnection, pSearchResult);
		    else
		        pLdapMessageEntry = ldap_next_entry(pLdapConnection, pLdapMessageEntry);

		    if(pLdapMessageEntry == NULL)
		        break;

			// get storage group DN
			entryDN = ldap_get_dn(pLdapConnection, pLdapMessageEntry);

			string replacementAttrValue;
			// iterate through all requested attributes on a storage group. Currently it's just "cn"
			for ( pAttribute = ldap_first_attribute( pLdapConnection, pLdapMessageEntry, &pBerEncoding );
					pAttribute != NULL; pAttribute = ldap_next_attribute( pLdapConnection, pLdapMessageEntry, pBerEncoding ) ) 
			{
				// skip attribute objectGUID
				if ((strcmpi(pAttribute, "distinguishedName") == 0) ||
					(strcmpi(pAttribute, "cn") == 0) ||					
					(strcmpi(pAttribute, "name") == 0) ||
					(strcmpi(pAttribute, "objectClass") == 0) ||
					(strcmpi(pAttribute, "objectGUID") == 0))
				{
					ldap_memfree( pAttribute );
					continue;
				}

				// handle binary attributes
				//cout << "Processing Attribute = " << pAttribute << "\n";
				if ((strcmpi(pAttribute, "unmergedAtts") == 0) || 
					(strcmpi(pAttribute, "dSASignature") == 0) ||
					(strcmpi(pAttribute, "mS-DS-ConsistencyGuid") == 0) ||
					(strcmpi(pAttribute, "msExchUnmergedAttsPt") == 0) ||
					(strcmpi(pAttribute, "partialAttributeDeletionList") == 0) ||
					(strcmpi(pAttribute, "partialAttributeSet") == 0) ||
					(strcmpi(pAttribute, "replPropteryMetaData") == 0) ||
					(strcmpi(pAttribute, "replUpToDateVector") == 0) ||
					(strcmpi(pAttribute, "repsFrom") == 0) ||
					(strcmpi(pAttribute, "repsTo") == 0) ||
					(strcmpi(pAttribute, "replicationSignature") == 0))
				{
					if ((binData = ldap_get_values_len( pLdapConnection, pLdapMessageEntry, pAttribute)) != NULL )
					{
						//cout << "Found " << ldap_count_values_len(binData) << " Values\n";
						
                        size_t vbinlen;
                        INM_SAFE_ARITHMETIC(vbinlen = InmSafeInt<ULONG>::Type(ldap_count_values_len(binData))+1, INMAGE_EX(ldap_count_values_len(binData)))
						pValuesBin = (berval **) calloc(vbinlen, sizeof(berval *));
						ULONG i;
						for ( i = 0; i < ldap_count_values_len(binData); i++ )
						{
							//cout << "Length = " << binData[i]->bv_len << "\n";
							pValuesBin[i] = ber_bvdup(binData[i]);
						}
						pValuesBin[i] = NULL;

						attrValuePairsBin[string(pAttribute)] = pValuesBin;

						// Free memory allocated by ldap_get_values_len(). 
						ldap_value_free_len( binData );
					}
				}
				// handle non-binary (string) attributes
				else if ((ppValue = ldap_get_values( pLdapConnection, pLdapMessageEntry, pAttribute)) != NULL )
				{
					int valIndex = 0;
					// allocate memory for multivalued attributes. Max of 16 values
					//cout << "Found " << ldap_count_values(ppValue) << " attributes\n";
                    size_t valuelen;
                    INM_SAFE_ARITHMETIC(valuelen = InmSafeInt<ULONG>::Type(ldap_count_values(ppValue))+1, INMAGE_EX(ldap_count_values(ppValue)))
					pValues = (char **) calloc(valuelen, sizeof(char *));
					for ( int i = 0; ppValue[i] != NULL; i++, valIndex++ )
					{
						replacementAttrValue.clear();
						// switch CN values that may have the source server name
						//cout << "Found Attribute = " << pAttribute << " Value = " << ppValue[i] << std::endl;
						switchCNValues(srcExchServerName, tgtExchServerName, ppValue[i], replacementAttrValue,
										numReplaced, CN_SEPARATOR_COMMA);
						// handle the case where the value of "dSCorePropagationData" contains duplicate values
						if( (strcmpi(pAttribute, "dSCorePropagationData") == 0) &&  (valIndex > 0) )
						{							
								if(strcmpi(pValues[valIndex-1], replacementAttrValue.c_str()) == 0)
								{
									valIndex--;
									continue;
								}
						}
						// copy the attribute value
                        size_t pvslen;
                        INM_SAFE_ARITHMETIC(pvslen = InmSafeInt<size_t>::Type(replacementAttrValue.length())+1, INMAGE_EX(replacementAttrValue.length()))
						pValues[valIndex] = (char *)calloc(pvslen, sizeof(char));
                        inm_strcpy_s(pValues[valIndex], pvslen, replacementAttrValue.c_str());
						//cout << "Replaced Attribute = " << pAttribute << " Value = " << pValues[valIndex] << std::endl;
					}
					pValues[valIndex] = NULL;
					attrValuePairs[string(pAttribute)] = pValues;

					// Free memory allocated by ldap_get_values(). 
					ldap_value_free( ppValue );
				}
		
				// Free memory allocated by ldap_first_attribute(). 
				ldap_memfree( pAttribute );
			}

			// add this storage group pointed to by iter to AD with all its attributes
			//cout << "\nCreating Information Store " << (*iter ).c_str() << std::endl;
			addEntryToAD(pLdapConnection, (*iter), "msExchInformationStore", attrValuePairs, attrValuePairsBin);

			// free memory of non-binary attributes
			map<string, char**>::const_iterator iter = attrValuePairs.begin();
			for (; iter != attrValuePairs.end(); iter++) {
				pValues = iter->second;
				for (int index=0; pValues[index] != NULL; index++) {
					free(pValues[index]);
				}
				free(pValues);
			}

			// free memory of binary attributes
			map<string, PBERVAL*>::const_iterator iterBin = attrValuePairsBin.begin();
			for (; iterBin != attrValuePairsBin.end(); iterBin++) {
				pValuesBin = iterBin->second;
				for (int index=0; pValuesBin[index] != NULL; index++) {
					ber_bvfree(pValuesBin[index]);
				}
				free(pValuesBin);
			}
		}
	}

	//cout << "Completed creating Information Stores on target" << std::endl;
	if( pLdapConnection )
	{
		ldap_unbind_s(pLdapConnection);
		pLdapConnection = NULL;
	}
	return SVS_OK;
}

SVERROR
ADInterface::updateMatchingInformationStoresOnTarget(const string& srcExchServerName,
						   const string& tgtExchServerName,
						   const list<string>& informationStores)
{
	// find configuration Naming Context DN
	string configurationDN;
	if (getConfigurationDN(configurationDN) == SVE_FAIL) 
	{
		cout << "Configuration Naming Context not found\n";
		return SVE_FAIL;
	}

	// get an Ldap connection
	LDAP* pLdapConnection = getLdapConnection();
	if (pLdapConnection == NULL)
	{
		cout << "Failed to connection to the ldap server\n";
		return SVE_FAIL;
	}

	// ldap bind
	ULONG errorCode = LDAP_SUCCESS;
	ULONG lRtn = 0;
	lRtn = ldap_bind_s(pLdapConnection,							
				(const PCHAR) configurationDN.c_str(),		// the distinguished name that is use to bind.
				NULL,										// if it is NULL it will use current user or service credentials 
				LDAP_AUTH_NEGOTIATE);						// LDAP authentication mode
	if(lRtn != LDAP_SUCCESS)
	{
		cout << "ldap_bind_s failed with:" << lRtn << std::endl;
		return SVE_FAIL;
	}

	LDAPMessage* pSearchResult;
	list<string>::const_iterator iter;
	string filter;
	string exchServerCN = "CN=";
	map<string, char** > attrValuePairs;
	map<string, berval** > attrValuePairsBin;
	char** pValues;
	berval** pValuesBin;
	exchServerCN += srcExchServerName;
	string exchServerCNLowerCase = convertoToLower(exchServerCN, exchServerCN.length());

	// search in source: for all the missing storage group on target
	for (iter=informationStores.begin(); iter != informationStores.end(); iter++) 
	{
		// the only difference between the source and the target DN is the server name "CN=<server_name>"
		// Construct a new DN by replacing the "CN=<source_server_name>" entry in the obtained DN to 
		// "CN=<target_server_name>".
		string replacementDN;
		ULONG numReplaced;
		switchCNValues(tgtExchServerName, srcExchServerName, (*iter), replacementDN, numReplaced, CN_SEPARATOR_COMMA);
		if (replacementDN.empty()) 
		{
			cout << "error replacing in DN " << replacementDN.c_str() << std::endl;
			continue;
		}
		// construct the filter
		filter = "(distinguishedName=";
		filter += replacementDN;
		filter += ")";

		ULONG ulSearchSizeLimit = 0;	// Limit number of users fetched at a time
		LDAP_TIMEVAL ltConnectionTimeout;
		ltConnectionTimeout.tv_sec = 0;		//CONNECTION_ESTABLISHMENT_TIMEOUT;	
		bool bSearchErr = false, bConnectErr = false;

		do
		{
			errorCode = ldap_search_s(pLdapConnection,						
						(const PCHAR) configurationDN.c_str(),	// DN to start search
						LDAP_SCOPE_SUBTREE,						 // Indicate the search scope
						(const PCHAR) filter.c_str(),			// Search Filter
						NULL,									// Retrieve list of attributes
						0,										//retrieve both attributesand values
						&pSearchResult);						// this is out parameter - Search results

	    	if( errorCode == LDAP_SUCCESS )
				break;    
		    
			if( errorCode == LDAP_SERVER_DOWN || 
				errorCode == LDAP_UNAVAILABLE ||
				errorCode == LDAP_NO_MEMORY ||
				errorCode == LDAP_BUSY ||
				errorCode == LDAP_LOCAL_ERROR ||
				errorCode == LDAP_TIMEOUT ||
				errorCode == LDAP_OPERATIONS_ERROR ||
				errorCode == LDAP_TIMELIMIT_EXCEEDED)
			{
				cout << "[ERROR][" << errorCode << "]: ldap_search_s failed. " << ldap_err2string(errorCode) << endl;
				if(pLdapConnection)
				{
					if(ldap_unbind_s(pLdapConnection)==ERROR_SUCCESS)
					{
						cout << "Cleaning up current connection...\n";
						pLdapConnection = NULL;
					}
				}
				if(pSearchResult)
				{
					ldap_msgfree(pSearchResult);
					pSearchResult = NULL;
				}

     			if( (pLdapConnection = getLdapConnectionCustom(configurationDN, ulSearchSizeLimit, ltConnectionTimeout)) == NULL )
				{
					bConnectErr = true;
					break;	
				}
			}
			else
			{
				//DebugPrintf(SV_LOG_FATAL, "ldap_search_s failed with: 0x%0lx \n",errorCode);
				cout << "[FATAL ERROR][:" << errorCode << "]: ldap_search_s failed. " << ldap_err2string(errorCode) << endl;
				if(pSearchResult != NULL)
				{
					ldap_msgfree(pSearchResult);
					pSearchResult = NULL;						
				}
				bSearchErr = true;
				break;
			}
		}while(1);

		if( bConnectErr )
		{
			cout << "[INFO]: Maximum connection attempts " << MAX_LDAP_CONNECT_ATTEMPTS << " has been reached...\n";
			cout << "[ERROR]: Unable to connect to directory server." << endl;
			continue; //next is
		}

		if( bSearchErr )
		{
			cout << "[ERROR] Error encountered while searching directory server.\n";
			continue; //next is
		}		
		
		ULONG numberOfEntries = ldap_count_entries(pLdapConnection, pSearchResult);

		// Check if there are any storage groups configured at all. NOTE: This is a fatal error
		// because the reason whey we are here is because this storage group did exist in source
		if(numberOfEntries == NULL)
		{
			cout  << "Failed to find entry " << replacementDN.c_str() <<  std::endl;
		    if(pSearchResult != NULL) 
		    {
			    ldap_msgfree(pSearchResult);
				pSearchResult = NULL;
		    }
			
			return SVE_FAIL;
		}

		LDAPMessage* pLdapMessageEntry = NULL;
		PCHAR pAttribute = NULL;
		BerElement* pBerEncoding = NULL;
		PCHAR* ppValue = NULL;
		berval** binData = NULL;
		ULONG iValue = 0;
		string entryDN;
		string entryDNLowerCase;

		// iterate through all storage groups
		for (int index=0; index < (int)numberOfEntries; index++)
		{
		    if(pLdapMessageEntry == NULL)
		        pLdapMessageEntry = ldap_first_entry(pLdapConnection, pSearchResult);
		    else
		        pLdapMessageEntry = ldap_next_entry(pLdapConnection, pLdapMessageEntry);

		    if(pLdapMessageEntry == NULL)
		        break;

			// get Information Store DN
			entryDN = ldap_get_dn(pLdapConnection, pLdapMessageEntry);

			string replacementAttrValue;
			// iterate through all requested attributes on a storage group. Currently it's just "cn"
			for ( pAttribute = ldap_first_attribute( pLdapConnection, pLdapMessageEntry, &pBerEncoding );
					pAttribute != NULL; pAttribute = ldap_next_attribute( pLdapConnection, pLdapMessageEntry, pBerEncoding ) ) 
			{
				// skip attribute objectGUID
				if ((strcmpi(pAttribute, "distinguishedName") == 0) ||
					(strcmpi(pAttribute, "cn") == 0) ||					
					(strcmpi(pAttribute, "name") == 0) ||
					(strcmpi(pAttribute, "objectClass") == 0) ||
					(strcmpi(pAttribute, "objectGUID") == 0))
				{
					ldap_memfree( pAttribute );
					continue;
				}

				// handle binary attributes
				//cout << "Processing Attribute = " << pAttribute << "\n";
				if ((strcmpi(pAttribute, "unmergedAtts") == 0) || 
					(strcmpi(pAttribute, "dSASignature") == 0) ||
					(strcmpi(pAttribute, "mS-DS-ConsistencyGuid") == 0) ||
					(strcmpi(pAttribute, "msExchUnmergedAttsPt") == 0) ||
					(strcmpi(pAttribute, "partialAttributeDeletionList") == 0) ||
					(strcmpi(pAttribute, "partialAttributeSet") == 0) ||
					(strcmpi(pAttribute, "replPropteryMetaData") == 0) ||
					(strcmpi(pAttribute, "replUpToDateVector") == 0) ||
					(strcmpi(pAttribute, "repsFrom") == 0) ||
					(strcmpi(pAttribute, "repsTo") == 0) ||
					(strcmpi(pAttribute, "replicationSignature") == 0))
				{
					if ((binData = ldap_get_values_len( pLdapConnection, pLdapMessageEntry, pAttribute)) != NULL )
					{
						//cout << "Found " << ldap_count_values_len(binData) << " Values\n";
						
                        size_t vbinlen;
                        INM_SAFE_ARITHMETIC(vbinlen = InmSafeInt<ULONG>::Type(ldap_count_values_len(binData))+1, INMAGE_EX(ldap_count_values_len(binData)))
						pValuesBin = (berval **) calloc(vbinlen, sizeof(berval *));
						ULONG i;
						for ( i = 0; i < ldap_count_values_len(binData); i++ )
						{
							//cout << "Length = " << binData[i]->bv_len << "\n";
							pValuesBin[i] = ber_bvdup(binData[i]);
						}
						pValuesBin[i] = NULL;

						attrValuePairsBin[string(pAttribute)] = pValuesBin;

						// Free memory allocated by ldap_get_values_len(). 
						ldap_value_free_len( binData );
					}
				}
				else if ((strcmpi(pAttribute, "msExchESEParamCircularLog") == 0) || 
						(strcmpi(pAttribute, "msExchESEParamCommitDefault") == 0) || 
						(strcmpi(pAttribute, "msExchESEParamLogFileSize") == 0) ||
						(strcmpi(pAttribute, "msExchESEParamBaseName") == 0) ||
						(strcmpi(pAttribute, "msExchESEParamDbExtenstionSize") == 0) ||
						(strcmpi(pAttribute, "msExchESEParamEnableIndexChecking") == 0) ||
						(strcmpi(pAttribute, "msExchESEParamEnableOnlineDefrag") == 0) ||
						(strcmpi(pAttribute, "msExchESEParamPageFragment") == 0) ||
						(strcmpi(pAttribute, "msExchESEParamPageTempDBMin") == 0) ||
						(strcmpi(pAttribute, "msExchESEParamZeroDatabaseDuringBackup") == 0) ||
						(strcmpi(pAttribute, "msExchESEParamEnableSortedRetrieveColumns") == 0) ||
						(strcmpi(pAttribute, "msExchMaxRestoreStorageGroups") == 0) ||
						(strcmpi(pAttribute, "msExchMaxStorageGroups") == 0) ||
						(strcmpi(pAttribute, "msExchMaxStoresPerGroup") == 0) ||
						(strcmpi(pAttribute, "msExchRecovery") == 0))
				{
					// handle non-binary (string) attributes
					if ((ppValue = ldap_get_values( pLdapConnection, pLdapMessageEntry, pAttribute)) != NULL )
					{ 
						// allocate memory for multivalued attributes. Max of 16 values
						//cout << "Found " << ldap_count_values(ppValue) << " attributes\n";
                        size_t valuelen;
                        INM_SAFE_ARITHMETIC(valuelen = InmSafeInt<ULONG>::Type(ldap_count_values(ppValue))+1, INMAGE_EX(ldap_count_values(ppValue)))
						pValues = (char **) calloc(valuelen, sizeof(char *));
						ULONG i;
						for ( i = 0; ppValue[i] != NULL; i++ )
						{
							replacementAttrValue.clear();
							// switch CN values that may have the source server name
							//cout << "Found Attribute = " << pAttribute << " Value = " << ppValue[i] << std::endl;
							switchCNValues(srcExchServerName, tgtExchServerName, ppValue[i], replacementAttrValue,
											numReplaced, CN_SEPARATOR_COMMA);
							// copy the attribute value
                            size_t pvslen;
                            INM_SAFE_ARITHMETIC(pvslen = InmSafeInt<size_t>::Type(replacementAttrValue.length())+1, INMAGE_EX(replacementAttrValue.length()))
							pValues[i] = (char *)calloc(pvslen, sizeof(char));
                            inm_strcpy_s(pValues[i], pvslen, replacementAttrValue.c_str());
							//cout << "Replaced Attribute = " << pAttribute << " Value = " << pValues[i] << std::endl;
						}
						pValues[i] = NULL;
						attrValuePairs[string(pAttribute)] = pValues;

						// Free memory allocated by ldap_get_values(). 
						ldap_value_free( ppValue );
					}
				}
		
				// Free memory allocated by ldap_first_attribute(). 
				ldap_memfree( pAttribute );
			}

			//cout << "\nUpdating Information Store " << (*iter ).c_str() << std::endl
			modifyADEntry(pLdapConnection, (*iter), attrValuePairs, attrValuePairsBin);

			// free memory of non-binary attributes
			map<string, char**>::const_iterator iter = attrValuePairs.begin();
			for (; iter != attrValuePairs.end(); iter++) {
				pValues = iter->second;
				for (int index=0; pValues[index] != NULL; index++) {
					free(pValues[index]);
				}
				free(pValues);
			}

			// free memory of binary attributes
			map<string, PBERVAL*>::const_iterator iterBin = attrValuePairsBin.begin();
			for (; iterBin != attrValuePairsBin.end(); iterBin++) {
				pValuesBin = iterBin->second;
				for (int index=0; pValuesBin[index] != NULL; index++) {
					ber_bvfree(pValuesBin[index]);
				}
				free(pValuesBin);
			}
		}
	}

	//cout << "Completed updating Information Store on target" << std::endl;
	if( pLdapConnection )
	{
		ldap_unbind_s(pLdapConnection);
		pLdapConnection = NULL;
	}
	return SVS_OK;
}

SVERROR
ADInterface::processExchangeTree(const string& sourceExchangeServerName, const string& destinationExchangeServerName,
								 const list<string>& proVolumes, map<string, string>& srcTotgtVolMapping, 
								 const string& sgDN, list<string>& proSGs, bool deleteExchConfigOnTarget,
								 const string& pfSrcExchServerName, const string& pfTgtExchServerName)
{
	// search and compare information stores
	list<string> extraIS;
	if (searchAndCompareInformationStores(sourceExchangeServerName, destinationExchangeServerName, extraIS) == SVE_FAIL) {
		cout << "Failed to create Information Store Failover cannot continue\n";
		return SVE_FAIL;
	}
	cout << "\n******Successfully created/updated the information stores\n";

	// search and compare storage groups
	list<string> extraStorageGroups;
	if (searchAndCompareStorageGroups(sourceExchangeServerName, destinationExchangeServerName, extraStorageGroups,
									  proVolumes, srcTotgtVolMapping, proSGs, sgDN) == SVE_FAIL) 
	{
		cout << "Searching and Compare failed for Storage Groups. Failover cannot continue\n";
		return SVE_FAIL;
	}
	cout << "\n******Successfully created/updated storage groups\n";

	// search and compare private and public mailbox stores		
	list<string> extraPublicMailstores, extraPrivMailStores;
	if (searchAndComparePrivatePublicMailboxStores(sourceExchangeServerName, destinationExchangeServerName, extraPublicMailstores, 
												   extraPrivMailStores, proSGs, pfSrcExchServerName, pfTgtExchServerName,
												   srcTotgtVolMapping) == SVE_FAIL)
	{
		cout << "Searching and Compare failed for Public and Private MailStores. Failover cannot continue\n";
		return SVE_FAIL;
	}
	cout << "\n******Successfully created/updated Public and Private MailStores\n\n";

	// delete all the extra entries from the Exchange Tree
	if (deleteExchConfigOnTarget)
		deleteInformationStoreSubtreeFromTarget(extraIS, extraStorageGroups, extraPublicMailstores, extraPrivMailStores);

	return SVS_OK;
}

SVERROR
ADInterface::deleteInformationStoreSubtreeFromTarget(const list<string>& informationStores, const list<string>& storageGroups,
													const list<string>& publicStores, const list<string>& privStores)
{
	// delete the extra private folder stores
	if (deleteEntryFromAD(privStores) == SVE_FAIL)
		cout << "Failed to Delete the Private Mailbox Stores from Target\n";

	// delete the extra public folder stores
	if (deleteEntryFromAD(publicStores) == SVE_FAIL)
		cout << "Failed to Delete the Public Folder Stores from Target\n";

	// delete the extra storage groups
	if (deleteEntryFromAD(storageGroups) == SVE_FAIL)
		cout << "Failed to Delete Storage Groups from Target\n";

	// delete the extra information stores
	if (deleteEntryFromAD(informationStores) == SVE_FAIL)
		cout << "Failed to Delete the Information Stores from Target\n";

	return SVS_OK;
}

LDAP*
ADInterface::getLdapConnection()
{
	LDAP* pLdapConnection = NULL;

	// initialize LDAP
	/*if (m_domainName.empty()) {
		pLdapConnection = ldap_init(NULL, LDAP_PORT);
	}
	else {
		pLdapConnection = ldap_init((PCHAR)(m_domainName.c_str()), LDAP_PORT);
	}

    if (pLdapConnection == NULL)
    {
		cout <<"ldap_init failed with 0x.\n"<< LdapGetLastError();
        return NULL;
    }
    else 
	{
		DebugPrintf(SV_LOG_DEBUG, "ldap_init succeeded \n");
	}

    ULONG version = LDAP_VERSION3;             // Set to LDAP V3 instead of V2
    ULONG numReturns = 50;                     // Limit number of users fetched at a time to 50
    ULONG lRtn = 0;

	lRtn = ldap_set_option(pLdapConnection, LDAP_OPT_PROTOCOL_VERSION, (void*) &version);

	if(lRtn == LDAP_SUCCESS) 
	{
        DebugPrintf(SV_LOG_DEBUG, "ldap version set to 3.0 \n");
	}
	else
    {
        DebugPrintf(SV_LOG_FATAL, "ldap setoption error:%0lX\n", lRtn);
		cerr << "ldap setoption error:" << lRtn << std::endl;
		return NULL;
    }

	// Connect to the server
	lRtn = ldap_connect(pLdapConnection, NULL);
	if(lRtn != LDAP_SUCCESS) 
	{
		DebugPrintf(SV_LOG_FATAL, "ldap_connect failed with 0x%lx.\n", lRtn);
		cerr << "ldap_connect failed with:" << lRtn << std::endl;
		return NULL;
	}
*/
	ULONG ulSearchSizeLimit = 0;
	LDAP_TIMEVAL ltConnectionTimeout;
	ltConnectionTimeout.tv_sec = 0;		//CONNECTION_ESTABLISHMENT_TIMEOUT;		
	if((pLdapConnection = getLdapConnectionCustom("", ulSearchSizeLimit, ltConnectionTimeout, false)) == NULL)
	{
		cout << "\n[FATAL ERROR]: Unable to connect to directory server. \n";
		return NULL;
	}
	else
	{		
        return pLdapConnection;
	}
}

LDAP*
ADInterface::getLdapConnectionCustom(const string szContext, ULONG ulSearchSizeLimit, LDAP_TIMEVAL ltConnectionTimeout, bool bBind,const std::string& otherDomain)
{
	LDAP* pLdapConnection = NULL;
    ULONG ulVersion = LDAP_VERSION3;           // Set to LDAP V3 instead of V2
    ULONG lRtn = 0, usConnectAttempts;
	
	usConnectAttempts = 0;
	while(usConnectAttempts < MAX_LDAP_CONNECT_ATTEMPTS)
	{
		if( usConnectAttempts )
		{
			Sleep( DELAY_BETWEEN_CONNECT_ATTEMPTS * 1000);
            cout << "Attempting to re-connect to LDAP server...\n";
		}

		if(!otherDomain.empty())
			pLdapConnection = ldap_init((PCHAR)otherDomain.c_str(),LDAP_PORT);
		else if (m_domainName.empty())
			pLdapConnection = ldap_init(NULL, LDAP_PORT);
		else
			pLdapConnection = ldap_init((PCHAR)(m_domainName.c_str()), LDAP_PORT);

		if (pLdapConnection == NULL)
		{
			ULONG ulInitErr = 0;
			ulInitErr = LdapGetLastError();
			cout << "[ERROR][" << ulInitErr << "] ldap_init failed. " << ldap_err2string(ulInitErr) << endl;
			usConnectAttempts++;
			if(usConnectAttempts == MAX_LDAP_CONNECT_ATTEMPTS)
			{
				cout << "[WARNING]: Maximum connection attempts has been reached...\n";
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
			cout << "[ERROR][" << lRtn << "] ldap_set_option failed. [protocol version]" << ldap_err2string(lRtn) << endl;
			if(pLdapConnection)
			{
				ldap_unbind_s(pLdapConnection);
				pLdapConnection = NULL;
			}
			usConnectAttempts++;
			if(usConnectAttempts == MAX_LDAP_CONNECT_ATTEMPTS)
			{
				cout << "[WARNING]: Maximum connection attempts has been reached...\n";
				break;
			}
			else
			{
				continue;
			}
		}

		//if( ulSearchSizeLimit )
		//{
			lRtn = ldap_set_option(pLdapConnection, LDAP_OPT_SIZELIMIT, (void*) &ulSearchSizeLimit);

			if(lRtn != LDAP_SUCCESS)
			{
				cout << "[ERROR][" << lRtn << "] ldap_set_option failed. [search sizelimit]" << ldap_err2string(lRtn) << endl;
				if(pLdapConnection)
				{
					ldap_unbind_s(pLdapConnection);
					pLdapConnection = NULL;
				}
				usConnectAttempts++;
				if(usConnectAttempts == MAX_LDAP_CONNECT_ATTEMPTS)
				{
					cout << "[WARNING]: Maximum connection attempts has been reached...\n";
					break;
				}
				else
				{
					continue;
				}
			}
		//}

		//lRtn = ldap_set_option(pLdapConnection, LDAP_OPT_SEND_TIMEOUT, (void*) &ulSendTimeout);

		// Connect to the server
		if( ltConnectionTimeout.tv_sec )
            lRtn = ldap_connect(pLdapConnection, &ltConnectionTimeout);
		else
            lRtn = ldap_connect(pLdapConnection, NULL);

		if(lRtn != LDAP_SUCCESS) 
		{			
			cout << "[ERROR][" << lRtn << "] ldap_connect failed. " << ldap_err2string(lRtn) << endl;
			if(pLdapConnection)
			{
				ldap_unbind_s(pLdapConnection);
				pLdapConnection = NULL;
			}
			usConnectAttempts++;
			if(usConnectAttempts == MAX_LDAP_CONNECT_ATTEMPTS)
			{
				cout << "[WARNING]: Maximum connection attempts has been reached...\n";
				break;
			}
			else
			{
				continue;
			}
		}
		/*else
			cout << "ldap_connect success.\n";*/
		if(bBind)
		{			
			PCHAR pContext = NULL;
			if( !szContext.empty() )
			{
				pContext = (const PCHAR) szContext.c_str();
			}

			lRtn = ldap_bind_s(
					pLdapConnection,                 // Session Handle
					pContext,   // the distinguished name that is use to bind.
					NULL,                            // if it is NULL it will use current user or service credentials 
					LDAP_AUTH_NEGOTIATE);            // LDAP authentication mode
			if(lRtn != LDAP_SUCCESS) 
			{
				cout << "[ERROR][" << lRtn << "] ldap_bind_s failed. " << ldap_err2string(lRtn) << endl;
				if(pLdapConnection)
				{
					ldap_unbind_s(pLdapConnection);
					pLdapConnection = NULL;
				}
				usConnectAttempts++;
				if(usConnectAttempts == MAX_LDAP_CONNECT_ATTEMPTS)
				{
					cout << "[WARNING]: Maximum connection attempts has been reached...\n";
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
    
	return pLdapConnection;
}


SVERROR
ADInterface::getConfigurationDN(string& configurationDN)
{
	// get the naming contexts
	LDAP_TIMEVAL ltTimeout;
	ltTimeout.tv_sec = 0;
	LDAP *pLdapConnection = getLdapConnectionCustom("", 0, ltTimeout, false);
	if (pLdapConnection == NULL) 
	{
		cout << "Could not connect to LDAP server\n";
		return SVE_FAIL;
	}

	list<string> namingContexts;

	if (getNamingContexts(pLdapConnection, namingContexts) != SVS_OK) 
	{
		DebugPrintf(SV_LOG_FATAL, "could not get naming contexts\n");
		cerr << "could not get naming contexts" << std::endl;
		return SVE_FAIL;
	}

	// get the Configuration naming context to be used to initiate ldap searches
	// This is done for efficient searching. No need to search all the hiearchies
	list <string>::const_iterator iter;
    for (iter=namingContexts.begin(); iter!=namingContexts.end(); ++iter) 
    {
        if(findPosIgnoreCase((*iter), "CN=Configuration") == 0)
        {
			configurationDN = (*iter);
			break;
        }
    }

	// check if configuration DN is missing
	if (configurationDN.empty()) 
	{
		cout << "Configuration Naming context not found\n";
		return SVE_FAIL;
	}

	return SVS_OK;
}

SVERROR
ADInterface::getDomainDN(string& domainDN)
{
	// get the naming contexts
	LDAP *pLdapConnection = getLdapConnection();
	if (pLdapConnection == NULL) 
	{
		cout << "Could not connection to LDAP server\n";
		return SVE_FAIL;
	}

	list<string> namingContexts;

	if (getNamingContexts(pLdapConnection, namingContexts) != SVS_OK) 
	{
		DebugPrintf(SV_LOG_FATAL, "could not get naming contexts\n");
		cerr << "could not get naming contexts" << std::endl;
		return SVE_FAIL;
	}

	// get the Configuration naming context to be used to initiate ldap searches
	// This is done for efficient searching. No need to search all the hiearchies
	list <string>::const_iterator iter;
    for (iter=namingContexts.begin(); iter!=namingContexts.end(); ++iter) 
    {
		cout << "Naming context = " << (*iter) << std::endl;
        if(findPosIgnoreCase((*iter), "DC=testskynet,DC=corp") == 0)
        {
			domainDN = (*iter);
			break;
        }
    }

	// check if configuration DN is missing
	if (domainDN.empty()) 
	{
		cout << "Domain Naming context not found\n";
		return SVE_FAIL;
	}

	return SVS_OK;
}

SVERROR
ADInterface::addEntryToAD(LDAP *pLdapConnection, const string& entryDN, const string& objectClass, 
			 map<string, char**>& attrValuePairs, map<string, PBERVAL*>& attrValuePairsBin)
{
	LDAPMod *NewEntry[1024];
	LDAPMod *entry, OClass;
	ULONG errorCode = LDAP_SUCCESS;

	// construct the add request and add to AD

	// first add the object class
	char *oc_values[] = { (char *)objectClass.c_str(), NULL };
	OClass.mod_op = LDAP_MOD_ADD;
	OClass.mod_type = "objectClass";
	OClass.mod_values = oc_values;
	NewEntry[0] = &OClass;
	size_t size;

	// now add all the remaining attrbiutes
	map<string, char**>::const_iterator iter = attrValuePairs.begin();
	map<string, berval**>::const_iterator iterBin = attrValuePairsBin.begin();

	int i=1;

	//cout << "\nNon-Binary Attributes....\n\n";
	// iterate through non-binary attributes
	for (; iter != attrValuePairs.end(); iter++) {
		// skip the distinguished name as that should be set as the DN is created
	{
			entry = (LDAPMod *) malloc(sizeof(LDAPMod));
			entry->mod_op = LDAP_MOD_ADD;
			size = (*iter).first.length();
            size_t typelen;
            INM_SAFE_ARITHMETIC(typelen = InmSafeInt<size_t>::Type(size)+1, INMAGE_EX(size))
			entry->mod_type = (char *)calloc(typelen, sizeof(char));
			inm_strncpy_s(entry->mod_type, (size+1),(*iter).first.c_str(), size);
			entry->mod_values = (*iter).second;
			NewEntry[i++] = entry;
			//cout << "Adding Attribute = " << (char *)((*iter).first.c_str())
				//<< " Value = " << (char*)((*iter).second)<< std::endl;
		}
	}

	//cout << "\nBinary Attributes...\n\n";
	// iterate through binary attributes
	for (; iterBin != attrValuePairsBin.end(); iterBin++) {
		// skip the distinguished name as that should be set as the DN is created
	{
			entry = (LDAPMod *) malloc(sizeof(LDAPMod));
			entry->mod_op = LDAP_MOD_ADD | LDAP_MOD_BVALUES;
			size = (*iterBin).first.length();
            size_t typelen;
            INM_SAFE_ARITHMETIC(typelen = InmSafeInt<size_t>::Type(size)+1, INMAGE_EX(size))
			entry->mod_type = (char *)calloc(typelen, sizeof(char));
			inm_strncpy_s(entry->mod_type,(size+1), (*iterBin).first.c_str(), size);
			entry->mod_bvalues = (*iterBin).second;
			NewEntry[i++] = entry;
			//cout << "Adding Attribute = " << (char *)((*iterBin).first.c_str())
				//<< " Value = " << (char*)((*iterBin).second) << std::endl;
		}
	}

	// initialize last entry to NULL
	NewEntry[i] = NULL;

	cout << "\nAdding Entry DN =" << entryDN.c_str() << std::endl;

	errorCode = ldap_add_s(pLdapConnection, (const PCHAR) entryDN.c_str(), NewEntry);
	if (errorCode != LDAP_SUCCESS)
	{
		if (errorCode != LDAP_ALREADY_EXISTS) {
			cout << "Failed to add entry " << entryDN.c_str() 
				<< "\n" << "ErrorCode = [" << errorCode << "] " << ldap_err2string(errorCode) << std::endl;
			return SVE_FAIL;
		}
		else {
			cout << "\n*****DN " << entryDN.c_str() << " already exists\n";
		}
		
	}
	else {
		cout << "\n***** Successfully added entry " << entryDN.c_str() << " to AD" << std::endl;
	}

	// release memory
	for (int j=1; j < i; j++) {
		entry = NewEntry[j]; 
		free(entry->mod_type);
		free(entry);
	}

	return SVS_OK;
}

SVERROR
ADInterface::modifyADEntry(LDAP *pLdapConnection, const string& entryDN,
			 map<string, char**>& attrValuePairs, map<string, PBERVAL*>& attrValuePairsBin)
{
	LDAPMod *NewEntry[1024];
	LDAPMod *entry;
	ULONG errorCode = LDAP_SUCCESS;
	size_t size;

	// construct the modify request and add to AD
	// now add all the remaining attrbiutes
	map<string, char**>::const_iterator iter = attrValuePairs.begin();
	map<string, berval**>::const_iterator iterBin = attrValuePairsBin.begin();

	int i=0;
	// iterate through non-binary attributes
	for (; iter != attrValuePairs.end(); iter++) {
		// skip the distinguished name as that should be set as the DN is created
	{
			entry = (LDAPMod *) malloc(sizeof(LDAPMod));
			entry->mod_op = LDAP_MOD_REPLACE;
			size = (*iter).first.length();
            size_t typelen;
            INM_SAFE_ARITHMETIC(typelen = InmSafeInt<size_t>::Type(size)+1, INMAGE_EX(size))
			entry->mod_type = (char *)calloc(typelen, sizeof(char));
			inm_strncpy_s(entry->mod_type, (size+1),(*iter).first.c_str(), size);
			entry->mod_values = (*iter).second;
			NewEntry[i++] = entry;
			//cout << "Adding Attribute = " << (char *)((*iter).first.c_str()) << std::endl;
		}
	}

	// initialize last entry to NULL
	NewEntry[i] = NULL;

	//cout << "\nAdding Entry DN =" << entryDN.c_str() << std::endl;

	// make sure there is data to modify
	if (NewEntry[0] != NULL)
		errorCode = ldap_modify_s(pLdapConnection, (const PCHAR) entryDN.c_str(), NewEntry);

	if (errorCode != LDAP_SUCCESS)
	{	
		cout << "Opeation failed errorCode = "<< errorCode << std::endl;
		return SVE_FAIL;
	}

	//cout << "\n***** Successfully Updated entry " << entryDN.c_str() << " to AD" << std::endl;

	// release memory
	for (int j=0; j < i; j++) {
		entry = NewEntry[j]; 
		free(entry->mod_type);
		free(entry);
	}

	return SVS_OK;
}

SVERROR
ADInterface::deleteEntryFromAD(const list<string>& dns)
{
	// find configuration Naming Context DN
	string configurationDN;
	if (getConfigurationDN(configurationDN) == SVE_FAIL) 
	{
		cout << "Configuration Naming Context not found\n";
		return SVE_FAIL;
	}

	// get an Ldap connection
	LDAP* pLdapConnection = getLdapConnection();
	if (pLdapConnection == NULL)
	{
		cout << "Failed to connection to the ldap server\n";
		return SVE_FAIL;
	}

	// ldap bind
	ULONG errorCode = LDAP_SUCCESS;
	ULONG lRtn = 0;
	lRtn = ldap_bind_s(pLdapConnection,							
				(const PCHAR) configurationDN.c_str(),		// the distinguished name that is use to bind.
				NULL,										// if it is NULL it will use current user or service credentials 
				LDAP_AUTH_NEGOTIATE);						// LDAP authentication mode
	if(lRtn != LDAP_SUCCESS)
	{
		cout << "ldap_bind_s failed with:" << lRtn << std::endl;
		return SVE_FAIL;
	}

	list<string>::const_iterator iter = dns.begin();
	for (; iter != dns.end(); iter++) {
		//cout << "Deleting Entry DN = " << iter->c_str() << std::endl;
		errorCode = ldap_delete_s(pLdapConnection, (PCHAR)iter->c_str());
		if (errorCode == -1)
		{
			cout << "Opeation failed errorCode = "<< errorCode << std::endl;
			
			return SVE_FAIL;
		}
		else {
			cout << "\n***** Successfully deleted entry " << iter->c_str() << " From AD" << std::endl;
		}
	}

	if( pLdapConnection )
	{
		ldap_unbind_s(pLdapConnection);
		pLdapConnection = NULL;
	}
	return SVS_OK;
}

SVERROR
ADInterface::searchAndComparePrivatePublicMailboxStores(const string& srcExchServerNameIn, const string& tgtExchServerNameIn,
														list<string>& extraPublicMailStoresDN, list<string>& extraPrivMailStoresDN,
														const list<string>& proSGs, 
														const string& pfSrcExchServerName, const string& pfTgtExchServerName,
														map<string, string>& srcTotgtVolMapping)
{
	string srcExchServerName = convertToUpper(srcExchServerNameIn);
	string tgtExchServerName = convertToUpper(tgtExchServerNameIn);

	// find private mailbox stores on source
	list<string> srcPrivMailStoresCN, srcPrivMailStoresDN;
	if (searchEntryinAD(srcExchServerName, srcPrivMailStoresCN, srcPrivMailStoresDN, string("(objectclass=msExchPrivateMDB)")) == SVS_OK) {
		list<string>::const_iterator iter = srcPrivMailStoresDN.begin();
		for (;iter != srcPrivMailStoresDN.end(); iter++) {
			//cout << "\nPrivate Mailbox Stores on Source Server " << srcExchServerName.c_str() 
			//	<< " "<< (*iter).c_str() << std::endl;
		}
	}

	// find private mailbox stores on target
	list<string> tgtPrivMailStoresCN, tgtPrivMailStoresDN;
	if (searchEntryinAD(tgtExchServerName, tgtPrivMailStoresCN, tgtPrivMailStoresDN, string("(objectclass=msExchPrivateMDB)")) == SVS_OK) {
		list<string>::const_iterator iter = tgtPrivMailStoresDN.begin();
		for (;iter != tgtPrivMailStoresDN.end(); iter++) {
			//cout << "\nPrivate Mailbox Stores on Target Server " << tgtExchServerName.c_str() << " " 
			//	<< (*iter).c_str() << std::endl;
		}
	}

	list<string> srcPublicMailStoresCN, srcPublicMailStoresDN;
	list<string> tgtPublicMailStoresCN, tgtPublicMailStoresDN;
	if(!m_bAudit)
	{
		// find public mailbox stores on source		
		if (searchEntryinAD(srcExchServerName, srcPublicMailStoresCN, srcPublicMailStoresDN, string("(objectclass=msExchPublicMDB)")) == SVS_OK) {
			list<string>::const_iterator iter = srcPublicMailStoresDN.begin();
			for (;iter != srcPublicMailStoresDN.end(); iter++) {
				//cout << "\nPublic Mailbox Stores on " << srcExchServerName.c_str() << (*iter).c_str() << std::endl;
			}
		}

		// find public mailbox stores on target		
		if (searchEntryinAD(tgtExchServerName, tgtPublicMailStoresCN, tgtPublicMailStoresDN, string("(objectclass=msExchPublicMDB)")) == SVS_OK) {
			list<string>::const_iterator iter = tgtPublicMailStoresDN.begin();
			for (;iter != tgtPublicMailStoresDN.end(); iter++) {
				//cout << "\nPublic Mailbox Stores on Target" << tgtExchServerName.c_str() << (*iter).c_str() << std::endl;
			}
		}
	}

	// find the source private mailboxes missing on target
	string replacementDN;
	list<string>::const_iterator srcCNIter, tgtCNIter, srcDNIter, tgtDNIter;
	list<string> missingPrivMailStoresCN, missingPrivMailStoresDN, matchingPrivMailStoresCN, matchingPrivMailStoresDN;
	ULONG numReplaced;
	bool found=false;
	for (srcCNIter = srcPrivMailStoresCN.begin(), srcDNIter = srcPrivMailStoresDN.begin(); 
		srcCNIter != srcPrivMailStoresCN.end(), srcDNIter != srcPrivMailStoresDN.end(); 
		srcCNIter++, srcDNIter++) {
		found = false;
		replacementDN.clear();
		switchCNValues(srcExchServerName, tgtExchServerName, (*srcDNIter), replacementDN, numReplaced, CN_SEPARATOR_COMMA);

		//cout << "\nReplaced = " << (*srcDNIter).c_str() << std::endl;
		//cout << "with DN = " << replacementDN.c_str() << std::endl;
		for (tgtDNIter = tgtPrivMailStoresDN.begin(); tgtDNIter != tgtPrivMailStoresDN.end(); tgtDNIter++) {
			if (strcmpi(replacementDN.c_str(), (*tgtDNIter).c_str()) == 0)
			{
				found = true;
				cout << "\nPrivate MailStore " << replacementDN.c_str() << " matching on target" << std::endl;
				matchingPrivMailStoresCN.push_back(*srcCNIter);
				matchingPrivMailStoresDN.push_back(replacementDN);
				break;
			}
		}
		if (false == found) {
			//cout << "\nPrivate MailStore " << replacementDN.c_str() << " missing on target" << std::endl;
			missingPrivMailStoresCN.push_back(*srcCNIter);
			missingPrivMailStoresDN.push_back(replacementDN);

		}
	}

	list<string> missingPublicMailStoresCN, missingPublicMailStoresDN, matchingPublicMailStoresCN, matchingPublicMailStoresDN;
	if(!m_bAudit)
	{
		// find the source public mailboxes missing, matching, extra on target		
		for (srcCNIter = srcPublicMailStoresCN.begin(), srcDNIter = srcPublicMailStoresDN.begin(); 
			srcCNIter != srcPublicMailStoresCN.end(), srcDNIter != srcPublicMailStoresDN.end(); 
			srcCNIter++, srcDNIter++) {
			found = false;
			replacementDN.clear();
			switchCNValues(srcExchServerName, tgtExchServerName, (*srcDNIter), replacementDN, numReplaced, CN_SEPARATOR_COMMA);
			for (tgtDNIter = tgtPublicMailStoresDN.begin(); tgtDNIter != tgtPublicMailStoresDN.end(); tgtDNIter++) {
				if (strcmpi(replacementDN.c_str(), (*tgtDNIter).c_str()) == 0)
				{
					found = true;
					cout << "\nPublic MailStore " << replacementDN.c_str() << " matching on target" << std::endl;
					matchingPublicMailStoresCN.push_back(*srcCNIter);
					matchingPublicMailStoresDN.push_back(replacementDN);
					break;
				}
			}
			if (false == found) {
				missingPublicMailStoresCN.push_back(*srcCNIter);
				missingPublicMailStoresDN.push_back(replacementDN);
			}
		}


		for (tgtDNIter = tgtPublicMailStoresDN.begin(); tgtDNIter != tgtPublicMailStoresDN.end(); tgtDNIter++) {
			bool found = false;
			string replacementDN;
			ULONG numReplaced;
			switchCNValues(tgtExchServerName, srcExchServerName, (*tgtDNIter), replacementDN, numReplaced, CN_SEPARATOR_COMMA);
			for (srcDNIter = srcPublicMailStoresDN.begin(); srcDNIter != srcPublicMailStoresDN.end(); srcDNIter++) {
				if (strcmpi(replacementDN.c_str(), (*srcDNIter).c_str()) == 0)
				{
					found = true;
					break;
				}
			}
			if (false == found) {
				cout << "\nDN =" << (*tgtDNIter).c_str() << " extra on target" << std::endl;
				extraPublicMailStoresDN.push_back(*tgtDNIter);
			}
		}
	}

	for (tgtDNIter = tgtPrivMailStoresDN.begin(); tgtDNIter != tgtPrivMailStoresDN.end(); tgtDNIter++) {
		bool found = false;
		string replacementDN;
		ULONG numReplaced;
		switchCNValues(tgtExchServerName, srcExchServerName, (*tgtDNIter), replacementDN, numReplaced, CN_SEPARATOR_COMMA);
		for (srcDNIter = srcPrivMailStoresDN.begin(); srcDNIter != srcPrivMailStoresDN.end(); srcDNIter++) {
			if (strcmpi(replacementDN.c_str(), (*srcDNIter).c_str()) == 0)
			{
				found = true;
				break;
			}
		}
		if (false == found) {
			cout << "\nDN =" << (*tgtDNIter).c_str() << " extra on target" << std::endl;
			extraPrivMailStoresDN.push_back(*tgtDNIter);
		}
	}


	list<string> proMissingPublicMailStoresDN;
	SVERROR retCode = SVS_OK;
	list<string>::const_iterator iterDN;
	if(!m_bAudit)
	{		
		findProtectedMailstores(missingPublicMailStoresDN, proSGs, proMissingPublicMailStoresDN);
		
		// create the missing public folders		
		for (iterDN = proMissingPublicMailStoresDN.begin(); iterDN != proMissingPublicMailStoresDN.end(); iterDN++) 
		{
			retCode = createMailStoresPublic(srcExchServerName, tgtExchServerName, *iterDN, msExchPublicMDB, srcTotgtVolMapping);
			if (retCode == SVE_FAIL) 
			{
				cout << " Failed to Create Public Mail Stores on target\n";
				return retCode;
			}
		}
	}
	// determine exchange server version
	list<string> attrValues;
	getAttrValueList(convertoToLower(srcExchServerName, srcExchServerName.length()), attrValues, "serialNumber", "(objectclass=msExchExchangeServer)", "");
	list<string>::const_iterator iter = attrValues.begin();

	bool verExch2003 = true;
	if (findPosIgnoreCase((*iter), "Version 8") == 0)
		verExch2003 = false;

	if(!m_bAudit)
	{
		retCode = SVS_OK;
		if (verExch2003) {
			// create the corresponding SMTP connections object for the newly created Public Mail Stores
			retCode = createSMTPConnections(srcExchServerName, tgtExchServerName, proMissingPublicMailStoresDN);
			if (retCode == SVE_FAIL)
			{
				cout << " Failed to Create SMTP Connections for Public Folders on target\n";
				return retCode;
			}
		}	

		list<string> proMatchingPublicMailStoresDN;
		findProtectedMailstores(matchingPublicMailStoresDN, proSGs, proMatchingPublicMailStoresDN);

		// update the matching public folders
		for (iterDN = proMatchingPublicMailStoresDN.begin(); iterDN != proMatchingPublicMailStoresDN.end(); iterDN++) 
		{
			retCode = updateMailStoresPublic(srcExchServerName, tgtExchServerName, *iterDN, msExchPublicMDB, srcTotgtVolMapping);
			if (retCode == SVE_FAIL)
			{
				cout << " Failed to Update Public Mail Stores on target\n";
				return retCode;
			}
		}

		if (verExch2003) {
			// create the corresponding SMTP connections object for the existing Public Mail Stores if it doesn't exist
			retCode = createSMTPConnections(srcExchServerName, tgtExchServerName, proMatchingPublicMailStoresDN);
			if (retCode == SVE_FAIL)
			{
				cout << " Failed to Create SMTP Connections for Public Folders on target\n";
				return retCode;
			}
		}
	}

	list<string> proMissingPrivMailStoresDN;
	findProtectedMailstores(missingPrivMailStoresDN, proSGs, proMissingPrivMailStoresDN);

	for (iterDN = proMissingPrivMailStoresDN.begin(); iterDN != proMissingPrivMailStoresDN.end(); iterDN++) 
	{
		retCode = createMailStoresPrivate(srcExchServerName, tgtExchServerName, *iterDN, msExchPrivateMDB, srcTotgtVolMapping,
										  pfSrcExchServerName, pfTgtExchServerName);
		if (retCode == SVE_FAIL) 
		{
			cout << " Failed to Create Public Mail Stores on target\n";
			return retCode;
		}
	}

	if (verExch2003) {
		// create the corresponding SMTP connections object for the newly created Private Mail Stores
		retCode = createSMTPConnections(srcExchServerName, tgtExchServerName, proMissingPrivMailStoresDN);
		if (retCode == SVE_FAIL)
		{
			cout << " Failed to Create SMTP Connections for Private Mailboxes on target\n";
			return retCode;
		}
	}

	list<string> proMatchingPrivMailStoresDN;
	findProtectedMailstores(matchingPrivMailStoresDN, proSGs, proMatchingPrivMailStoresDN);

	// update the matching public folders
	for (iterDN = proMatchingPrivMailStoresDN.begin(); iterDN != proMatchingPrivMailStoresDN.end(); iterDN++) 
	{
		retCode = updateMailStoresPrivate(srcExchServerName, tgtExchServerName, *iterDN, msExchPrivateMDB, srcTotgtVolMapping, 
										  pfSrcExchServerName, pfTgtExchServerName);
		if (retCode == SVE_FAIL)
		{	
			cout << " Failed to Update Private Mail Stores on target\n";
			return retCode;
		}
	}

	if (verExch2003) {
		// create the corresponding SMTP connections object for the newly created Private Mail Stores
		retCode = createSMTPConnections(srcExchServerName, tgtExchServerName, proMatchingPrivMailStoresDN);
		if (retCode == SVE_FAIL)
		{
			cout << " Failed to Create SMTP Connections for Private Mailboxes on target\n";
			return retCode;
		}
	}

	//Set attributes homeMDB & homeMTA of CN="Microsoft System Attendant"
	string sysAttHomeMDBValue;

	// find configuration Naming Context DN
	string configurationDN;
	if (getConfigurationDN(configurationDN) == SVE_FAIL) 
	{
		cout << "Configuration Naming Context not found\n";
		return SVE_FAIL;
	}

	// get an Ldap connection
	LDAP* pLdapConnection = getLdapConnection();
	if (pLdapConnection == NULL)
	{
		cout << "Failed to connection to the ldap server\n";
		return SVE_FAIL;
	}

	// ldap bind
	ULONG errorCode = LDAP_SUCCESS;
	ULONG lRtn = 0;
	lRtn = ldap_bind_s(	pLdapConnection,
				(const PCHAR) configurationDN.c_str(),		// the distinguished name that is use to bind.
				NULL,										// if it is NULL it will use current user or service credentials 
				LDAP_AUTH_NEGOTIATE);						// LDAP authentication mode
	if(lRtn != LDAP_SUCCESS)
	{
		cout << "ldap_bind_s failed with:" << lRtn << std::endl;
		return SVE_FAIL;
	}

    retCode = getSystemAttendantHomeMDBValue(srcExchServerName, tgtExchServerName, configurationDN, pLdapConnection, sysAttHomeMDBValue); //Output variable
	if(!sysAttHomeMDBValue.empty())
	{			
        retCode = setHomeMDBForMicrosoftSystemAttendat(configurationDN, tgtExchServerName, pLdapConnection, sysAttHomeMDBValue);
		if (retCode == SVE_FAIL)
		{	
			cout << "[WARNING]: Failed to Update homeMDB of Microsoft System Attendant mailbox on target.\n"
				<< "         Please refer to solution document to overcome this by manually setting \n"
				<< "         homeMDB attribute of Microsoft System Attendant mailbox on target.\n";
			return SVS_OK; //TODO: Exit or Continue?
		}
	}
	else
	{
		cout << "[WARNING]: Failed to Update homeMDB of Microsoft System Attendant mailbox on target server.\n";
	}
	return SVS_OK;
}

SVERROR
ADInterface::createMailStoresPrivate(const string& srcExchServerName, const string& tgtExchServerName, 
									 const string& missingMailStoresDN, const string& objectClass,
									 map<string, string>& srcTotgtVolMapping,
									 const string& pfSrcExchServerName, const string& pfTgtExchServerName)
{
	// find configuration Naming Context DN
	string configurationDN;
	if (getConfigurationDN(configurationDN) == SVE_FAIL) 
	{
		cout << "Configuration Naming Context not found\n";
		return SVE_FAIL;
	}

	// get an Ldap connection
	LDAP* pLdapConnection = getLdapConnection();
	if (pLdapConnection == NULL)
	{
		cout << "Failed to connect to the ldap server\n";
		return SVE_FAIL;
	}

	// ldap bind
	ULONG errorCode = LDAP_SUCCESS;
	ULONG lRtn = 0;
	lRtn = ldap_bind_s(pLdapConnection,							
				(const PCHAR) configurationDN.c_str(),		// Use the naming context from the input list
				NULL,										// if it is NULL it will use current user or service credentials 
				LDAP_AUTH_NEGOTIATE);						// LDAP authentication mode
	if(lRtn != LDAP_SUCCESS)
	{
		cout << "ldap_bind_s failed with:" << lRtn << std::endl;
		return SVE_FAIL;
	}

	LDAPMessage* pSearchResult;
	list<string>::const_iterator iterCN, iterDN;
	string filter;
	map<string, char ** > attrValuePairs;
	map<string, berval** > attrValuePairsBin;
	char** pValues;

	//cout << "creating mailstore " << missingMailStoresDN.c_str() << std::endl;
	string replacementDN;
	ULONG numReplaced;

	// switch CN values of server to search the source
	string copyDN = string(missingMailStoresDN.c_str());
	switchCNValues(tgtExchServerName, srcExchServerName, copyDN, replacementDN, numReplaced, CN_SEPARATOR_COMMA);

	filter = "(objectClass=*)";

	//cout << "DN is : " << replacementDN.c_str() << std::endl;


	ULONG ulSearchSizeLimit = 0;	// Limit number of users fetched at a time
	LDAP_TIMEVAL ltConnectionTimeout;
	ltConnectionTimeout.tv_sec = 0;		//CONNECTION_ESTABLISHMENT_TIMEOUT;	
	bool bSearchErr = false, bConnectErr = false;

	do
	{
		errorCode = ldap_search_s(pLdapConnection,						
					(const PCHAR) replacementDN.c_str(),	// null-terminated string that contains the distinguished name of the entry at which to start the search
					LDAP_SCOPE_BASE,						 // Indicate the search scope
					(const PCHAR) filter.c_str(),			// Search Filter
					NULL,									// Retrieve list of attributes
					0,										//retrieve both attributesand values
					&pSearchResult);						// this is out parameter - Search results

		if( errorCode == LDAP_SUCCESS )
			break;    
	    
		if( errorCode == LDAP_SERVER_DOWN || 
			errorCode == LDAP_UNAVAILABLE ||
			errorCode == LDAP_NO_MEMORY ||
			errorCode == LDAP_BUSY ||
			errorCode == LDAP_LOCAL_ERROR ||
			errorCode == LDAP_TIMEOUT ||
			errorCode == LDAP_OPERATIONS_ERROR ||
			errorCode == LDAP_TIMELIMIT_EXCEEDED)
		{
			cout << "[ERROR][" << errorCode << "]: ldap_search_s failed. " << ldap_err2string(errorCode) << endl;
			if(pLdapConnection)
			{
				if(ldap_unbind_s(pLdapConnection)==ERROR_SUCCESS)
				{
					cout << "Cleaning up current connection...\n";
					pLdapConnection = NULL;
				}
			}
			if(pSearchResult)
			{
				ldap_msgfree(pSearchResult);
				pSearchResult = NULL;
			}

     		if( (pLdapConnection = getLdapConnectionCustom(configurationDN, ulSearchSizeLimit, ltConnectionTimeout)) == NULL )
			{
				bConnectErr = true;
				break;	
			}
		}
		else
		{
			//DebugPrintf(SV_LOG_FATAL, "ldap_search_s failed with: 0x%0lx \n",errorCode);
			cout << "[FATAL ERROR][:" << errorCode << "]: ldap_search_s failed. " << ldap_err2string(errorCode) << endl;
			if(pSearchResult != NULL)
			{
				ldap_msgfree(pSearchResult);
				pSearchResult = NULL;						
			}
			bSearchErr = true;
			break;
		}
	}while(1);

	if( bConnectErr )
	{
		cout << "[INFO]: Maximum connection attempts " << MAX_LDAP_CONNECT_ATTEMPTS << " has been reached...\n";
		cout << "[ERROR]: Unable to connect to directory server." << endl;
		return SVE_FAIL;
	}

	if( bSearchErr )
	{
		cout << "[ERROR] Error encountered while searching directory server.\n";
		return SVE_FAIL;
	}
	
	ULONG numberOfEntries = ldap_count_entries(pLdapConnection, pSearchResult);

	// Check if there are any storage groups configured at all. NOTE: This is a fatal error
	// because the reason whey we are here is because this storage group did exist in source
	if(numberOfEntries == NULL)
	{
		cout << "Failed to find entry " << replacementDN.c_str() << std::endl;
		cout << "ldap_count_entries failed with errorcode " << errorCode << std::endl;
	    if( pSearchResult != NULL)
		{
			ldap_msgfree(pSearchResult);
			pSearchResult = NULL;
		}
		
		return SVE_FAIL;
	}

	LDAPMessage* pLdapMessageEntry = NULL;
	PCHAR pAttribute = NULL;
	BerElement* pBerEncoding = NULL;
	berval** pValuesBin;
	berval** binData;
	PCHAR* ppValue = NULL;
	ULONG iValue = 0;
	string entryDN;
	string entryDNLowerCase;
	string replacementAttrValue;

	// iterate through all mail stores
	for (int index=0; index < (int)numberOfEntries; index++)
	{
	    if(pLdapMessageEntry == NULL)
	        pLdapMessageEntry = ldap_first_entry(pLdapConnection, pSearchResult);
	    else
	        pLdapMessageEntry = ldap_next_entry(pLdapConnection, pLdapMessageEntry);

	    if(pLdapMessageEntry == NULL)
	        break;

		// iterate through all requested attributes on a storage group. Currently it's just "cn"
		for ( pAttribute = ldap_first_attribute( pLdapConnection, pLdapMessageEntry, &pBerEncoding );
				pAttribute != NULL; pAttribute = ldap_next_attribute( pLdapConnection, pLdapMessageEntry, pBerEncoding ) ) {
	  
			if ((strcmpi(pAttribute, "objectGUID") == 0) ||
				(strcmpi(pAttribute, "distinguishedName") == 0) ||
				(strcmpi(pAttribute, "cn") == 0) ||					
				(strcmpi(pAttribute, "name") == 0) ||
				(strcmpi(pAttribute, "objectGUID") == 0) ||
				(strcmpi(pAttribute, "displayName") == 0) ||
				(findPosIgnoreCase(string(pAttribute), string("homeMDBBL")) == 0))
			{
				ldap_memfree(pAttribute);
				continue;
			}

			// handle binary attributes
			if ((strcmpi(pAttribute, "quotaNotificationSchedule") == 0) ||
				(strcmpi(pAttribute, "activationSchedule") == 0) ||
				(strcmpi(pAttribute, "msExchCIRebuildSchedule") == 0) ||
				(strcmpi(pAttribute, "msExchCIUpdateSchedule") == 0) ||
				(strcmpi(pAttribute, "msExchReplicationSchedule") == 0) ||
				(strcmpi(pAttribute, "msExchPolicyOptionList") == 0) ||
				(strcmpi(pAttribute, "msExchUnmergedAttsPt") == 0) ||
				(strcmpi(pAttribute, "delivExtContTypes") == 0) ||
				(strcmpi(pAttribute, "dSASignature") == 0) ||
				(strcmpi(pAttribute, "extensionData") == 0) ||
				(strcmpi(pAttribute, "formData") == 0) ||
				(strcmpi(pAttribute, "maximumObjectID") == 0) ||
				(strcmpi(pAttribute, "mS-DS-ConsistencyGuid") == 0) ||
				(strcmpi(pAttribute, "msExchCatalog") == 0) ||
				(strcmpi(pAttribute, "securityProtocol") == 0) ||
				(strcmpi(pAttribute, "userCert") == 0) ||
				(strcmpi(pAttribute, "userCertificate") == 0) ||
				(strcmpi(pAttribute, "userSMIMECertificate") == 0) ||
				(strcmpi(pAttribute, "partialAttributeSet") == 0) ||
				(strcmpi(pAttribute, "partialAttributeDeletionList") == 0) ||
				(strcmpi(pAttribute, "replicationSignature") == 0) ||
				(strcmpi(pAttribute, "replPropertyMetaData") == 0) ||
				(strcmpi(pAttribute, "replUpToDateVector") == 0) ||
				(strcmpi(pAttribute, "repsFrom") == 0) ||
				(strcmpi(pAttribute, "repsTo") == 0) ||
				(strcmpi(pAttribute, "unmergedAtts") == 0) ||
				(strcmpi(pAttribute, "wellKnownObjects") == 0) ||
				(strcmpi(pAttribute, "otherWellKnownObjects") == 0) ||
				(strcmpi(pAttribute, "proxiedObjectName") == 0))
			{
				if ((binData = ldap_get_values_len( pLdapConnection, pLdapMessageEntry, pAttribute)) != NULL )
				{
					//cout << "Processing Attribute"  << pAttribute << " Total Values = " << ldap_count_values_len(binData) << "\n";
					
                    size_t vbinlen;
                    INM_SAFE_ARITHMETIC(vbinlen = InmSafeInt<ULONG>::Type(ldap_count_values_len(binData))+1, INMAGE_EX(ldap_count_values_len(binData)))
					pValuesBin = (berval **) calloc(vbinlen, sizeof(berval *));
					ULONG i;
					for ( i = 0; i < ldap_count_values_len(binData); i++ )
					{
						//cout << "Length = " << binData[i]->bv_len << "\n";
						pValuesBin[i] = ber_bvdup(binData[i]);
					}
					pValuesBin[i] = NULL;

					attrValuePairsBin[string(pAttribute)] = pValuesBin;

					// Free memory allocated by ldap_get_values_len(). 
					ldap_value_free_len( binData );
				}
			}
			// handle non-binary attributes
			else if ((ppValue = ldap_get_values( pLdapConnection, pLdapMessageEntry, pAttribute)) != NULL )
			{
				int valIndex = 0;
				// allocate memory for multivalued attributes. Max of 16 values
                size_t valuelen;
                INM_SAFE_ARITHMETIC(valuelen = InmSafeInt<ULONG>::Type(ldap_count_values(ppValue))+1, INMAGE_EX(ldap_count_values(ppValue)))
				pValues = (char **) calloc(valuelen, sizeof(char *));
				for ( int i = 0; ppValue[i] != NULL; i++, valIndex++ )
				{
					replacementAttrValue.clear();
					// switch CN values that may have the source server name and are separated by SLASH
					if ((strcmpi(pAttribute, "legacyExchangeDN") == 0)) {
						//cout << "\nReplacing Attribute SLASH = " << pAttribute << " Value = " << ppValue[i] << std::endl;
						switchCNValuesSpecial(srcExchServerName, tgtExchServerName, ppValue[i], replacementAttrValue,
									numReplaced, CN_SEPARATOR_SLASH);
						//cout << "\nReplacing Attribute SLASH = " << pAttribute << " Value = " << replacementAttrValue  << std::endl;
					}
					else if (strcmpi(pAttribute, "adminDisplayName") == 0)
					{
						switchCNValuesNoSeparator(srcExchServerName, tgtExchServerName, ppValue[i], replacementAttrValue);
					}
					else if (strcmpi(pAttribute, "msExchHomePublicMDB") == 0) {
						// handle the case wherein the public mailstores are hosted on an exchange server other than the 
						// one hosting the mailbox stores
						if (pfSrcExchServerName.empty() == false && pfTgtExchServerName.empty() == false) {
							switchCNValues(pfSrcExchServerName, pfTgtExchServerName, ppValue[i], replacementAttrValue,
											numReplaced, CN_SEPARATOR_COMMA);
						}
						else {
							switchCNValues(srcExchServerName, tgtExchServerName, ppValue[i], replacementAttrValue,
										numReplaced, CN_SEPARATOR_COMMA);
						}
					}
					else {
						//cout << "\nReplacing Attribute = " << pAttribute << " Value = " << ppValue[i] << std::endl;
						switchCNValues(srcExchServerName, tgtExchServerName, ppValue[i], replacementAttrValue,
										numReplaced, CN_SEPARATOR_COMMA);
					}
					char drive[256];
					// replace the source drive letter/mount point  with target drive letter/mount point
					if (srcTotgtVolMapping.empty() == false) {
						if ((strcmpi(pAttribute, "msExchEDBFile") == 0) || (strcmpi(pAttribute, "msExchSLVFile") == 0)) {
							char *srcDrive = (char *)(replacementAttrValue.c_str());
							cout << "replacementAttrValue.c_str() = "<<srcDrive << endl; 
							// PR#10815: Long Path support
                            SVGetVolumePathName(srcDrive, drive, ARRAYSIZE(drive));
							int len = strlen(drive);
							if(drive[len-1] == '\\')
							drive[len-1] = '\0';							
							const char *tgtDrive = srcTotgtVolMapping[string(drive)].c_str();							
							replacementAttrValue.replace(0, strlen(tgtDrive), tgtDrive);
						}
					}
					// handle the case where the value of "dSCorePropagationData" contains duplicate values
					if( (strcmpi(pAttribute, "dSCorePropagationData") == 0) &&  (valIndex > 0) )
					{							
                            if(strcmpi(pValues[valIndex-1], replacementAttrValue.c_str()) == 0)
							{
								valIndex--;
                                continue;
							}
					}
					// copy the attribute value
                    size_t pvslen;
                    INM_SAFE_ARITHMETIC(pvslen = InmSafeInt<size_t>::Type(replacementAttrValue.length())+1, INMAGE_EX(replacementAttrValue.length()))
					pValues[valIndex] = (char *)calloc(pvslen, sizeof(char));
                    inm_strcpy_s(pValues[valIndex], pvslen, replacementAttrValue.c_str());
					//cout << "Found Attribute = " << pAttribute << " Value = " << pValues[valIndex] << std::endl;
				}
				pValues[valIndex] = NULL;
				attrValuePairs[string(pAttribute)] = pValues;
				// Free memory allocated by ldap_get_values(). 
				ldap_value_free( ppValue );
			}
	
			// Free memory allocated by ldap_first_attribute(). 
			ldap_memfree( pAttribute );
		}

		//cout << "Start to add Mail Store\n\n";
		addMailStoresToADPrivate(pLdapConnection, missingMailStoresDN, objectClass, attrValuePairs, attrValuePairsBin);

		// free memory of non-binary attributes
		map<string, char**>::const_iterator iter = attrValuePairs.begin();
		for (; iter != attrValuePairs.end(); iter++) {
			pValues = iter->second;
			for (int index=0; pValues[index] != NULL; index++) {
				free(pValues[index]);
			}
			free(pValues);
		}

		// free memory of binary attributes
		map<string, berval**>::const_iterator iterBin = attrValuePairsBin.begin();
		for (; iterBin != attrValuePairsBin.end(); iterBin++) {
			pValuesBin = iterBin->second;
			for (int index=0; pValuesBin[index] != NULL; index++) {
				ber_bvfree(pValuesBin[index]);
			}
			free(pValuesBin);
		}
	}

	cout << "\n******Successfully created Private Mailstore " << missingMailStoresDN.c_str() << std::endl;
	if( pLdapConnection )
	{
		ldap_unbind_s(pLdapConnection);
		pLdapConnection = NULL;
	}

	return SVS_OK;
}

SVERROR
ADInterface::updateMailStoresPrivate(const string& srcExchServerName, const string& tgtExchServerName, 
									 const string& matchingMailStoresDN, const string& objectClass,
									 map<string, string>& srcTotgtVolMapping,
									 const string& pfSrcExchServerName, const string& pfTgtExchServerName)
{
	char drive[256];
	// find configuration Naming Context DN
	string configurationDN;
	if (getConfigurationDN(configurationDN) == SVE_FAIL) 
	{
		cout << "Configuration Naming Context not found\n";
		return SVE_FAIL;
	}

	// get an Ldap connection
	LDAP* pLdapConnection = getLdapConnection();
	if (pLdapConnection == NULL)
	{
		cout << "Failed to connection to the ldap server\n";
		return SVE_FAIL;
	}

	// ldap bind
	ULONG errorCode = LDAP_SUCCESS;
	ULONG lRtn = 0;
	lRtn = ldap_bind_s(pLdapConnection,							
				(const PCHAR) configurationDN.c_str(),		// Use the naming context from the input list
				NULL,										// if it is NULL it will use current user or service credentials 
				LDAP_AUTH_NEGOTIATE);						// LDAP authentication mode
	if(lRtn != LDAP_SUCCESS)
	{
		cout << "ldap_bind_s failed with:" << lRtn << std::endl;
		return SVE_FAIL;
	}

	LDAPMessage* pSearchResult;
	list<string>::const_iterator iterCN, iterDN;
	string filter;
	map<string, char ** > attrValuePairs;
	map<string, berval** > attrValuePairsBin;
	char** pValues;

	// search in source: for all the missing storage group on target
	string replacementDN;
	ULONG numReplaced;

	// switch CN values of server to search the source
	string copyDN = string(matchingMailStoresDN.c_str());
	switchCNValues(tgtExchServerName, srcExchServerName, copyDN, replacementDN, numReplaced, CN_SEPARATOR_COMMA);

	filter = "(objectClass=*)";

	ULONG ulSearchSizeLimit = 0;	// Limit number of users fetched at a time
	LDAP_TIMEVAL ltConnectionTimeout;
	ltConnectionTimeout.tv_sec = 0;		//CONNECTION_ESTABLISHMENT_TIMEOUT;	
	bool bSearchErr = false, bConnectErr = false;

	do
	{
		errorCode = ldap_search_s(pLdapConnection,						
					(const PCHAR) replacementDN.c_str(),	// null-terminated string that contains the distinguished name of the entry at which to start the search
					LDAP_SCOPE_BASE,						 // Indicate the search scope
					(const PCHAR) filter.c_str(),			// Search Filter
					NULL,									// Retrieve list of attributes
					0,										//retrieve both attributesand values
					&pSearchResult);						// this is out parameter - Search results
		if( errorCode == LDAP_SUCCESS )
			break;    
	    
		if( errorCode == LDAP_SERVER_DOWN || 
			errorCode == LDAP_UNAVAILABLE ||
			errorCode == LDAP_NO_MEMORY ||
			errorCode == LDAP_BUSY ||
			errorCode == LDAP_LOCAL_ERROR ||
			errorCode == LDAP_TIMEOUT ||
			errorCode == LDAP_OPERATIONS_ERROR ||
			errorCode == LDAP_TIMELIMIT_EXCEEDED)
		{
			cout << "[ERROR][" << errorCode << "]: ldap_search_s failed. " << ldap_err2string(errorCode) << endl;
			if(pLdapConnection)
			{
				if(ldap_unbind_s(pLdapConnection)==ERROR_SUCCESS)
				{
					cout << "Cleaning up current connection...\n";
					pLdapConnection = NULL;
				}
			}
			if(pSearchResult)
			{
				ldap_msgfree(pSearchResult);
				pSearchResult = NULL;
			}

     		if( (pLdapConnection = getLdapConnectionCustom(configurationDN, ulSearchSizeLimit, ltConnectionTimeout)) == NULL )
			{
				bConnectErr = true;
				break;	
			}
		}
		else
		{
			//DebugPrintf(SV_LOG_FATAL, "ldap_search_s failed with: 0x%0lx \n",errorCode);
			cout << "[FATAL ERROR][:" << errorCode << "]: ldap_search_s failed. " << ldap_err2string(errorCode) << endl;
			if(pSearchResult != NULL)
			{
				ldap_msgfree(pSearchResult);
				pSearchResult = NULL;						
			}
			bSearchErr = true;
			break;
		}
	}while(1);

	if( bConnectErr )
	{
		cout << "[INFO]: Maximum connection attempts " << MAX_LDAP_CONNECT_ATTEMPTS << " has been reached...\n";
		cout << "[ERROR]: Unable to connect to directory server." << endl;
		return SVE_FAIL;
	}

	if( bSearchErr )
	{
		cout << "[ERROR] Error encountered while searching directory server.\n";
		return SVE_FAIL;
	}
	
	ULONG numberOfEntries = ldap_count_entries(pLdapConnection, pSearchResult);

	// Check if there are any storage groups configured at all. NOTE: This is a fatal error
	// because the reason whey we are here is because this storage group did exist in source
	if(numberOfEntries == NULL)
	{
		cout << "Failed to find entry " << replacementDN.c_str() << std::endl;
		cout << "ldap_count_entries failed with errorcode " << errorCode << std::endl;
	    if(pSearchResult != NULL) 
	    {
		    ldap_msgfree(pSearchResult);
			pSearchResult = NULL;
	    }
	    return SVS_OK;
	}

	LDAPMessage* pLdapMessageEntry = NULL;
	PCHAR pAttribute = NULL;
	BerElement* pBerEncoding = NULL;
	berval** pValuesBin;
	berval** binData;
	PCHAR* ppValue = NULL;
	ULONG iValue = 0;
	string entryDN;
	string entryDNLowerCase;
	string replacementAttrValue;

	// iterate through all mail stores
	for (int index=0; index < (int)numberOfEntries; index++)
	{
	    if(pLdapMessageEntry == NULL)
	        pLdapMessageEntry = ldap_first_entry(pLdapConnection, pSearchResult);
	    else
	        pLdapMessageEntry = ldap_next_entry(pLdapConnection, pLdapMessageEntry);

	    if(pLdapMessageEntry == NULL)
	        break;

		// iterate through all requested attributes on a storage group. Currently it's just "cn"
		for ( pAttribute = ldap_first_attribute( pLdapConnection, pLdapMessageEntry, &pBerEncoding );
				pAttribute != NULL; pAttribute = ldap_next_attribute( pLdapConnection, pLdapMessageEntry, pBerEncoding ) ) {
	  
			if ((strcmpi(pAttribute, "objectGUID") == 0) ||
				(strcmpi(pAttribute, "distinguishedName") == 0) ||
				(strcmpi(pAttribute, "cn") == 0) ||					
				(strcmpi(pAttribute, "name") == 0) ||
				(strcmpi(pAttribute, "objectGUID") == 0) ||
				(strcmpi(pAttribute, "displayName") == 0) ||
				(findPosIgnoreCase(string(pAttribute), string("homeMDBBL")) == 0))
			{
				ldap_memfree(pAttribute);
				continue;
			}

			// handle binary attributes
			if ((strcmpi(pAttribute, "quotaNotificationSchedule") == 0) ||
				(strcmpi(pAttribute, "activationSchedule") == 0) ||
				(strcmpi(pAttribute, "msExchCIRebuildSchedule") == 0) ||
				(strcmpi(pAttribute, "msExchCIUpdateSchedule") == 0) ||
				(strcmpi(pAttribute, "msExchReplicationSchedule") == 0) ||
				(strcmpi(pAttribute, "msExchPolicyOptionList") == 0) ||
				(strcmpi(pAttribute, "msExchUnmergedAttsPt") == 0) ||
				(strcmpi(pAttribute, "delivExtContTypes") == 0) ||
				(strcmpi(pAttribute, "dSASignature") == 0) ||
				(strcmpi(pAttribute, "extensionData") == 0) ||
				(strcmpi(pAttribute, "formData") == 0) ||
				(strcmpi(pAttribute, "maximumObjectID") == 0) ||
				(strcmpi(pAttribute, "mS-DS-ConsistencyGuid") == 0) ||
				(strcmpi(pAttribute, "msExchCatalog") == 0) ||
				(strcmpi(pAttribute, "securityProtocol") == 0) ||
				(strcmpi(pAttribute, "userCert") == 0) ||
				(strcmpi(pAttribute, "userCertificate") == 0) ||
				(strcmpi(pAttribute, "userSMIMECertificate") == 0) ||
				(strcmpi(pAttribute, "partialAttributeSet") == 0) ||
				(strcmpi(pAttribute, "partialAttributeDeletionList") == 0) ||
				(strcmpi(pAttribute, "replicationSignature") == 0) ||
				(strcmpi(pAttribute, "replPropertyMetaData") == 0) ||
				(strcmpi(pAttribute, "replUpToDateVector") == 0) ||
				(strcmpi(pAttribute, "repsFrom") == 0) ||
				(strcmpi(pAttribute, "repsTo") == 0) ||
				(strcmpi(pAttribute, "unmergedAtts") == 0) ||
				(strcmpi(pAttribute, "wellKnownObjects") == 0) ||
				(strcmpi(pAttribute, "otherWellKnownObjects") == 0) ||
				(strcmpi(pAttribute, "proxiedObjectName") == 0))
			{
				if ((binData = ldap_get_values_len( pLdapConnection, pLdapMessageEntry, pAttribute)) != NULL )
				{
					//cout << "Processing Attribute"  << pAttribute << " Total Values = " << ldap_count_values_len(binData) << "\n";
					
                    size_t vbinlen;
                    INM_SAFE_ARITHMETIC(vbinlen = InmSafeInt<ULONG>::Type(ldap_count_values_len(binData))+1, INMAGE_EX(ldap_count_values_len(binData)))
					pValuesBin = (berval **) calloc(vbinlen, sizeof(berval *));
					ULONG i;
					for ( i = 0; i < ldap_count_values_len(binData); i++ )
					{
						//cout << "Length = " << binData[i]->bv_len << "\n";
						pValuesBin[i] = ber_bvdup(binData[i]);
					}
					pValuesBin[i] = NULL;

					attrValuePairsBin[string(pAttribute)] = pValuesBin;

					// Free memory allocated by ldap_get_values_len(). 
					ldap_value_free_len( binData );
				}
			}
			// handle non-binary attributes
			else if ((strcmpi(pAttribute, "msExchDatabaseCreated") == 0) ||
					(strcmpi(pAttribute, "msExchEDBFile") == 0) ||
					(strcmpi(pAttribute, "msExchEDBOffline") == 0) ||
					(strcmpi(pAttribute, "msExchMailboxRetentionPeriod") == 0) ||
					(strcmpi(pAttribute, "msExchPatchMDB") == 0) ||
					(strcmpi(pAttribute, "msExchSLVFile") == 0) ||
					(strcmpi(pAttribute, "msExchHomePublicMDB") == 0))
					
			{
				if ((ppValue = ldap_get_values( pLdapConnection, pLdapMessageEntry, pAttribute)) != NULL )
				{
					// allocate memory for multivalued attributes. Max of 16 values
                    size_t valuelen;
                    INM_SAFE_ARITHMETIC(valuelen = InmSafeInt<ULONG>::Type(ldap_count_values(ppValue))+1, INMAGE_EX(ldap_count_values(ppValue)))
					pValues = (char **) calloc(valuelen, sizeof(char *));
					ULONG i;
					for (  i = 0; ppValue[i] != NULL; i++ )
					{
						replacementAttrValue.clear();
						// switch CN values that may have the source server name and are separated by SLASH
						if ((strcmpi(pAttribute, "legacyExchangeDN") == 0)) {
							//cout << "\nReplacing Attribute SLASH = " << pAttribute << " Value = " << ppValue[i] << std::endl;
							switchCNValuesSpecial(srcExchServerName, tgtExchServerName, ppValue[i], replacementAttrValue,
										numReplaced, CN_SEPARATOR_SLASH);
						}
						else if (strcmpi(pAttribute, "adminDisplayName") == 0)
						{
							switchCNValuesNoSeparator(srcExchServerName, tgtExchServerName, ppValue[i], replacementAttrValue);
						}
						else if (strcmpi(pAttribute, "msExchHomePublicMDB") == 0) {
							// handle the case wherein the public mailstores are hosted on an exchange server other than the 
							// one hosting the mailbox stores
							if (pfSrcExchServerName.empty() == false && pfTgtExchServerName.empty() == false) {
								switchCNValues(pfSrcExchServerName, pfTgtExchServerName, ppValue[i], replacementAttrValue,
												numReplaced, CN_SEPARATOR_COMMA);
							}
							else {
								switchCNValues(srcExchServerName, tgtExchServerName, ppValue[i], replacementAttrValue,
											numReplaced, CN_SEPARATOR_COMMA);
							}
						}
						else {
							//cout << "\nReplacing Attribute = " << pAttribute << " Value = " << ppValue[i] << std::endl;
							switchCNValues(srcExchServerName, tgtExchServerName, ppValue[i], replacementAttrValue,
											numReplaced, CN_SEPARATOR_COMMA);
						}
						
						if (srcTotgtVolMapping.empty() == false) {
							if ((strcmpi(pAttribute, "msExchEDBFile") == 0) || (strcmpi(pAttribute, "msExchSLVFile") == 0)) {
								char *srcDrive = (char *)(replacementAttrValue.c_str());
								// PR#10815: Long Path support
                                SVGetVolumePathName(srcDrive, drive, ARRAYSIZE(drive));
								int len = strlen(drive);
								if(drive[len-1] == '\\')
								drive[len-1] = '\0';
								const char *tgtDrive = srcTotgtVolMapping[string(drive)].c_str();								
								replacementAttrValue.replace(0, strlen(tgtDrive), string(tgtDrive));											
							}
						}
						// copy the attribute value
                        size_t pvslen;
                        INM_SAFE_ARITHMETIC(pvslen = InmSafeInt<size_t>::Type(replacementAttrValue.length())+1, INMAGE_EX(replacementAttrValue.length()))
						pValues[i] = (char *)calloc(pvslen, sizeof(char));
                        inm_strcpy_s(pValues[i], pvslen, replacementAttrValue.c_str());
						//cout << "Found Attribute = " << pAttribute << " Value = " << pValues[i] << std::endl;
					}
					pValues[i] = NULL;
					attrValuePairs[string(pAttribute)] = pValues;
					// Free memory allocated by ldap_get_values(). 
					ldap_value_free( ppValue );
				}
			}
	
			// Free memory allocated by ldap_first_attribute(). 
			ldap_memfree( pAttribute );
		}

		//cout << "Start to add Mail Store\n\n";
		modifyADEntry(pLdapConnection, matchingMailStoresDN, attrValuePairs, attrValuePairsBin);

		// free memory of non-binary attributes
		map<string, char**>::const_iterator iter = attrValuePairs.begin();
		for (; iter != attrValuePairs.end(); iter++) {
			pValues = iter->second;
			for (int index=0; pValues[index] != NULL; index++) {
				free(pValues[index]);
			}
			free(pValues);
		}

		// free memory of binary attributes
		map<string, berval**>::const_iterator iterBin = attrValuePairsBin.begin();
		for (; iterBin != attrValuePairsBin.end(); iterBin++) {
			pValuesBin = iterBin->second;
			for (int index=0; pValuesBin[index] != NULL; index++) {
				ber_bvfree(pValuesBin[index]);
			}
			free(pValuesBin);
		}
	}

	cout << "\n*****Successfully updated Private Mailstore " << matchingMailStoresDN.c_str() << std::endl;
	if( pLdapConnection )
	{
        ldap_unbind_s(pLdapConnection);
		pLdapConnection = NULL;
	}
	return SVS_OK;
}

SVERROR
ADInterface::createMailStoresPublic(const string& srcExchServerName, const string& tgtExchServerName, 
									const string& missingMailStoresDN, const string& objectClass,
									map<string, string>& srcTotgtVolMapping)
{
	char drive[256];
	// find configuration Naming Context DN
	string configurationDN;
	if (getConfigurationDN(configurationDN) == SVE_FAIL) 
	{
		cout << "Configuration Naming Context not found\n";
		return SVE_FAIL;
	}

	// get an Ldap connection
	LDAP* pLdapConnection = getLdapConnection();
	if (pLdapConnection == NULL)
	{
		cout << "Failed to connection to the ldap server\n";
		return SVE_FAIL;
	}

	// ldap bind
	ULONG errorCode = LDAP_SUCCESS;
	ULONG lRtn = 0;
	lRtn = ldap_bind_s(pLdapConnection,							
				(const PCHAR) configurationDN.c_str(),		// Use the naming context from the input list
				NULL,										// if it is NULL it will use current user or service credentials 
				LDAP_AUTH_NEGOTIATE);						// LDAP authentication mode
	if(lRtn != LDAP_SUCCESS)
	{
		cout << "ldap_bind_s failed with:" << lRtn << std::endl;
		return SVE_FAIL;
	}

	LDAPMessage* pSearchResult;
	list<string>::const_iterator iterCN, iterDN;
	string filter;
	map<string, char ** > attrValuePairs;
	map<string, berval** > attrValuePairsBin;
	char** pValues;

	string replacementDN;
	ULONG numReplaced;

	// switch CN values of server to search the source
	string copyDN = string(missingMailStoresDN.c_str());
	switchCNValues(tgtExchServerName, srcExchServerName, copyDN, replacementDN, numReplaced, CN_SEPARATOR_COMMA);

	filter = "(objectClass=*)";

	ULONG ulSearchSizeLimit = 0;	// Limit number of users fetched at a time
	LDAP_TIMEVAL ltConnectionTimeout;
	ltConnectionTimeout.tv_sec = 0;		//CONNECTION_ESTABLISHMENT_TIMEOUT;	
	bool bSearchErr = false, bConnectErr = false;

	do
	{
		errorCode = ldap_search_s(pLdapConnection,					
					(const PCHAR) replacementDN.c_str(),	// null-terminated string that contains the distinguished name of the entry at which to start the search
					LDAP_SCOPE_BASE,						 // Indicate the search scope
					(const PCHAR) filter.c_str(),			// Search Filter
					NULL,									// Retrieve list of attributes
					0,										//retrieve both attributesand values
					&pSearchResult);						// this is out parameter - Search results
		if( errorCode == LDAP_SUCCESS )
			break;    
	    
		if( errorCode == LDAP_SERVER_DOWN || 
			errorCode == LDAP_UNAVAILABLE ||
			errorCode == LDAP_NO_MEMORY ||
			errorCode == LDAP_BUSY ||
			errorCode == LDAP_LOCAL_ERROR ||
			errorCode == LDAP_TIMEOUT ||
			errorCode == LDAP_OPERATIONS_ERROR ||
			errorCode == LDAP_TIMELIMIT_EXCEEDED)
		{
			cout << "[ERROR][" << errorCode << "]: ldap_search_s failed. " << ldap_err2string(errorCode) << endl;
			if(pLdapConnection)
			{
				if(ldap_unbind_s(pLdapConnection)==ERROR_SUCCESS)
				{
					cout << "Cleaning up current connection...\n";
					pLdapConnection = NULL;
				}
			}
			if(pSearchResult)
			{
				ldap_msgfree(pSearchResult);
			}

     		if( (pLdapConnection = getLdapConnectionCustom(configurationDN, ulSearchSizeLimit, ltConnectionTimeout)) == NULL )
			{
				bConnectErr = true;
				break;	
			}
		}
		else
		{
			//DebugPrintf(SV_LOG_FATAL, "ldap_search_s failed with: 0x%0lx \n",errorCode);
			cout << "[FATAL ERROR][:" << errorCode << "]: ldap_search_s failed. " << ldap_err2string(errorCode) << endl;
			if(pSearchResult != NULL)
			{
				ldap_msgfree(pSearchResult);
				pSearchResult = NULL;						
			}
			bSearchErr = true;
			break;
		}
	}while(1);

	if( bConnectErr )
	{
		cout << "[INFO]: Maximum connection attempts " << MAX_LDAP_CONNECT_ATTEMPTS << " has been reached...\n";
		cout << "[ERROR]: Unable to connect to directory server." << endl;
		return SVE_FAIL;
	}

	if( bSearchErr )
	{
		cout << "[ERROR] Error encountered while searching directory server.\n";
		return SVE_FAIL;
	}

	ULONG numberOfEntries = ldap_count_entries(pLdapConnection, pSearchResult);

	// Check if there are any storage groups configured at all. NOTE: This is a fatal error
	// because the reason whey we are here is because this storage group did exist in source
	if(numberOfEntries == NULL)
	{
		cout << "Failed to find entry " << replacementDN.c_str() << std::endl;
		cout << "ldap_count_entries failed with errorcode " << errorCode << std::endl;
	    if(pSearchResult != NULL) 
	    {
		    ldap_msgfree(pSearchResult);
			pSearchResult = NULL;
	    }
		
		return SVE_FAIL;
	}

	//Get appropriate MTA servers
	string srcMTA, tgtMTA;
		
	srcMTA = (this->srcMTAExchServerName.empty()) ? srcExchServerName : this->srcMTAExchServerName;
	tgtMTA = (this->tgtMTAExchServerName.empty()) ? tgtExchServerName : this->tgtMTAExchServerName;

	LDAPMessage* pLdapMessageEntry = NULL;
	PCHAR pAttribute = NULL;
	BerElement* pBerEncoding = NULL;
	berval** pValuesBin;
	berval** binData;
	PCHAR* ppValue = NULL;
	ULONG iValue = 0;
	string entryDN;
	string entryDNLowerCase;
	string replacementAttrValue;

	// iterate through all mail stores
	for (int index=0; index < (int)numberOfEntries; index++)
	{
	    if(pLdapMessageEntry == NULL)
	        pLdapMessageEntry = ldap_first_entry(pLdapConnection, pSearchResult);
	    else
	        pLdapMessageEntry = ldap_next_entry(pLdapConnection, pLdapMessageEntry);

	    if(pLdapMessageEntry == NULL)
	        break;

		// iterate through all requested attributes on a storage group. Currently it's just "cn"
		for ( pAttribute = ldap_first_attribute( pLdapConnection, pLdapMessageEntry, &pBerEncoding );
				pAttribute != NULL; pAttribute = ldap_next_attribute( pLdapConnection, pLdapMessageEntry, pBerEncoding ) ) 
		{
			if ((strcmpi(pAttribute, "distinguishedName") == 0) ||
				(strcmpi(pAttribute, "cn") == 0) ||
				(strcmpi(pAttribute, "name") == 0) ||
				(findPosIgnoreCase(string(pAttribute), string("homeMDBBL")) == 0) ||
				(strcmpi(pAttribute, "displayName") == 0) ||
				(strcmpi(pAttribute, "adminDisplayName") == 0) ||
				(strcmpi(pAttribute, "objectGUID") == 0))
			{
				ldap_memfree(pAttribute);
				continue;
			}

			if ((strcmpi(pAttribute, "quotaNotificationSchedule") == 0) ||
				(strcmpi(pAttribute, "activationSchedule") == 0) ||
				(strcmpi(pAttribute, "msExchCIRebuildSchedule") == 0) ||
				(strcmpi(pAttribute, "msExchCIUpdateSchedule") == 0) ||
				(strcmpi(pAttribute, "msExchCatalog") == 0) ||
				(strcmpi(pAttribute, "msExchReplicationSchedule") == 0) ||
				(strcmpi(pAttribute, "msExchPolicyOptionList") == 0) ||
				(strcmpi(pAttribute, "msExchUnmergedAttsPt") == 0) ||
				(strcmpi(pAttribute, "partialAttributeSet") == 0) ||
				(strcmpi(pAttribute, "partialAttributeDeletionList") == 0) ||
				(strcmpi(pAttribute, "proxiedObjectName") == 0) ||
				(strcmpi(pAttribute, "dSASignature") == 0) ||
				(strcmpi(pAttribute, "maximumObjectID") == 0) ||
				(strcmpi(pAttribute, "mS-DS-ConsistencyGuid") == 0) ||
				(strcmpi(pAttribute, "replicationSignature") == 0) ||
				(strcmpi(pAttribute, "replPropertyMetaData") == 0) ||
				(strcmpi(pAttribute, "replUpToDateVector") == 0) ||
				(strcmpi(pAttribute, "repsFrom") == 0) ||
				(strcmpi(pAttribute, "repsTo") == 0) ||
				(strcmpi(pAttribute, "unmergedAtts") == 0) ||
				(strcmpi(pAttribute, "wellKnownObjects") == 0) ||
				(strcmpi(pAttribute, "objectGUID") == 0))
			{
				//cout << "Adding binary attribute " << pAttribute << std::endl;
				if ((binData = ldap_get_values_len( pLdapConnection, pLdapMessageEntry, pAttribute)) != NULL )
				{
					//cout << "Processing Attribute"  << pAttribute << " Total Values = " << ldap_count_values_len(binData) << "\n";
					
                    size_t vbinlen;
                    INM_SAFE_ARITHMETIC(vbinlen = InmSafeInt<ULONG>::Type(ldap_count_values_len(binData))+1, INMAGE_EX(ldap_count_values_len(binData)))
					pValuesBin = (berval **) calloc(vbinlen, sizeof(berval *));
					ULONG i;
					for ( i = 0; i < ldap_count_values_len(binData); i++ )
					{
						//cout << "Length = " << binData[i]->bv_len << "\n";
						pValuesBin[i] = ber_bvdup(binData[i]);
					}
					pValuesBin[i] = NULL;

					attrValuePairsBin[string(pAttribute)] = pValuesBin;

					// Free memory allocated by ldap_get_values_len(). 
					ldap_value_free_len( binData );
				}
			}
			else if ((ppValue = ldap_get_values( pLdapConnection, pLdapMessageEntry, pAttribute)) != NULL )
			{
					int valIndex = 0;
					// allocate memory for multivalued attributes. Max of 16 values
                    size_t valuelen;
                    INM_SAFE_ARITHMETIC(valuelen = InmSafeInt<ULONG>::Type(ldap_count_values(ppValue))+1, INMAGE_EX(ldap_count_values(ppValue)))
					pValues = (char **) calloc(valuelen, sizeof(char *));
					for ( int i = 0; ppValue[i] != NULL; i++, valIndex++ )
					{
						replacementAttrValue.clear();
						// switch CN values that may have the source server name
						if ((strcmpi(pAttribute, "legacyExchangeDN") == 0)) 
						{
							//cout << "\nReplacing Attribute SLASH = " << pAttribute << " Value = " << ppValue[i] << std::endl;
							switchCNValuesSpecial(srcExchServerName, tgtExchServerName, ppValue[i], replacementAttrValue,
										numReplaced, CN_SEPARATOR_SLASH);
							//cout << "\nReplaced Attribute SLASH = " << pAttribute << " Value = " << replacementAttrValue.c_str() << std::endl;
						}
						
						else if ((strcmpi(pAttribute, "mail") == 0) ||
								(strcmpi(pAttribute, "mailNickname") == 0) ||
								(strcmpi(pAttribute, "textEncodedORAddress") == 0) ||
								(strcmpi(pAttribute, "proxyAddresses") == 0))
						{
							//cout << "\nReplacing Attribute NO SEP = " << pAttribute << " Value = " << ppValue[i] << std::endl;
							switchCNValuesNoSeparator(srcExchServerName, tgtExchServerName, ppValue[i], replacementAttrValue);
							//cout << "\nReplaced Attribute NO SEP = " << pAttribute << " Value = " << replacementAttrValue.c_str() << std::endl;
						}
						else 
						{
							if ((strcmpi(pAttribute, "homeMTA") == 0) &&
                                (findPosIgnoreCase(string(ppValue[i]), string(string("=") + srcMTA + ",")) != string::npos))
							{
								switchCNValues(srcMTA, tgtMTA, ppValue[i], replacementAttrValue,
											numReplaced, CN_SEPARATOR_COMMA);
							}
							else
							{
                                //cout << "\nReplacing Attribute = " << pAttribute << " Value = " << ppValue[i] << std::endl;
                                switchCNValues(srcExchServerName, tgtExchServerName, ppValue[i], replacementAttrValue,
											numReplaced, CN_SEPARATOR_COMMA);
							}
						}

						if ((srcTotgtVolMapping.empty() == false)) {
							if ((strcmpi(pAttribute, "msExchEDBFile") == 0) || (strcmpi(pAttribute, "msExchSLVFile") == 0)) {
								char *srcDrive = (char *)(replacementAttrValue.c_str());
								// PR#10815: Long Path support
                                SVGetVolumePathName(srcDrive, drive, ARRAYSIZE(drive));
								int len = strlen(drive);
								if(drive[len-1] == '\\')
								drive[len-1] = '\0';
								const char *tgtDrive = srcTotgtVolMapping[drive].c_str();			
								//cout <<"Replacement of:"<<	replacementAttrValue.c_str() <<" with "<<"tgtDrive:"<<tgtDrive;
								replacementAttrValue.replace(0, strlen(tgtDrive), tgtDrive);								
							}
						}
						// handle the case where the value of "dSCorePropagationData" contains duplicate values
						if( (strcmpi(pAttribute, "dSCorePropagationData") == 0) &&  (valIndex > 0) )
						{							
								if(strcmpi(pValues[valIndex-1], replacementAttrValue.c_str()) == 0)
								{
									valIndex--;
									continue;
								}
						}
						// copy the attribute value
                        size_t pvslen;
                        INM_SAFE_ARITHMETIC(pvslen = InmSafeInt<size_t>::Type(replacementAttrValue.length())+1, INMAGE_EX(replacementAttrValue.length()))
						pValues[valIndex] = (char *)calloc(pvslen, sizeof(char));
                        inm_strcpy_s(pValues[valIndex], pvslen, replacementAttrValue.c_str());
						//cout << "Found Attribute = " << pAttribute << " Value = " << pValues[valIndex] << std::endl;
					}
					pValues[valIndex] = NULL;
					attrValuePairs[string(pAttribute)] = pValues;
					// Free memory allocated by ldap_get_values(). 
					ldap_value_free( ppValue );
			}
	
			// Free memory allocated by ldap_first_attribute(). 
			ldap_memfree( pAttribute );
		}

		if (pBerEncoding != NULL)
			ber_free( pBerEncoding, 0);

		//cout << "Start to add Mail Store\n\n";
		SVERROR result = addMailStoresToADPublic(pLdapConnection, missingMailStoresDN, objectClass, attrValuePairs, attrValuePairsBin);
		if (result == SVE_FAIL) {
			cout << "\nFailed to created Public Mailstore " << missingMailStoresDN.c_str() << std::endl;
		}
		else {
			//cout << "\nSuccessfully created Public Mailstore " << missingMailStoresDN.c_str() << std::endl;
		}

		// free memory of non-binary attributes
		map<string, char**>::const_iterator iter = attrValuePairs.begin();
		for (; iter != attrValuePairs.end(); iter++) {
			pValues = iter->second;
			for (int index=0; pValues[index] != NULL; index++) {
				free(pValues[index]);
			}
			free(pValues);
		}

		// free memory of binary attributes
		map<string, berval**>::const_iterator iterBin = attrValuePairsBin.begin();
		for (; iterBin != attrValuePairsBin.end(); iterBin++) {
			pValuesBin = iterBin->second;
			for (int index=0; pValuesBin[index] != NULL; index++) {
				ber_bvfree(pValuesBin[index]);
			}
			free(pValuesBin);
		}
	}

	if( pLdapConnection )
	{
        ldap_unbind_s(pLdapConnection);
		pLdapConnection = NULL;
	}
	return SVS_OK;
}

SVERROR
ADInterface::updateMailStoresPublic(const string& srcExchServerName, const string& tgtExchServerName, 
									const string& matchingMailStoresDN, const string& objectClass,
									map<string, string>& srcTotgtVolMapping)
{
	char drive[256];
	// find configuration Naming Context DN
	string configurationDN;
	if (getConfigurationDN(configurationDN) == SVE_FAIL) 
	{
		cout << "Configuration Naming Context not found\n";
		return SVE_FAIL;
	}

	// get an Ldap connection
	LDAP* pLdapConnection = getLdapConnection();
	if (pLdapConnection == NULL)
	{
		cout << "Failed to connection to the ldap server\n";
		return SVE_FAIL;
	}

	// ldap bind
	ULONG errorCode = LDAP_SUCCESS;
	ULONG lRtn = 0;
	lRtn = ldap_bind_s(pLdapConnection,							
				(const PCHAR) configurationDN.c_str(),		// Use the naming context from the input list
				NULL,										// if it is NULL it will use current user or service credentials 
				LDAP_AUTH_NEGOTIATE);						// LDAP authentication mode
	if(lRtn != LDAP_SUCCESS)
	{
		cout << "ldap_bind_s failed with:" << lRtn << std::endl;
		return SVE_FAIL;
	}

	LDAPMessage* pSearchResult;
	list<string>::const_iterator iterCN, iterDN;
	string filter;
	map<string, char ** > attrValuePairs;
	map<string, berval** > attrValuePairsBin;
	char** pValues;

	// search in source: for all the missing storage group on target
	string replacementDN;
	ULONG numReplaced;

	// switch CN values of server to search the source
	string copyDN = string(matchingMailStoresDN.c_str());
	switchCNValues(tgtExchServerName, srcExchServerName, copyDN, replacementDN, numReplaced, CN_SEPARATOR_COMMA);

	filter = "(objectClass=*)";

	ULONG ulSearchSizeLimit = 0;	// Limit number of users fetched at a time
	LDAP_TIMEVAL ltConnectionTimeout;
	ltConnectionTimeout.tv_sec = 0;		//CONNECTION_ESTABLISHMENT_TIMEOUT;	
	bool bSearchErr = false, bConnectErr = false;

	do
	{
		errorCode = ldap_search_s(pLdapConnection,						
					(const PCHAR) replacementDN.c_str(),	// null-terminated string that contains the distinguished name of the entry at which to start the search
					LDAP_SCOPE_BASE,						 // Indicate the search scope
					(const PCHAR) filter.c_str(),			// Search Filter
					NULL,									// Retrieve list of attributes
					0,										//retrieve both attributesand values
					&pSearchResult);						// this is out parameter - Search results
		if( errorCode == LDAP_SUCCESS )
			break;    
	    
		if( errorCode == LDAP_SERVER_DOWN || 
			errorCode == LDAP_UNAVAILABLE ||
			errorCode == LDAP_NO_MEMORY ||
			errorCode == LDAP_BUSY ||
			errorCode == LDAP_LOCAL_ERROR ||
			errorCode == LDAP_TIMEOUT ||
			errorCode == LDAP_OPERATIONS_ERROR ||
			errorCode == LDAP_TIMELIMIT_EXCEEDED)
		{
			cout << "[ERROR][" << errorCode << "]: ldap_search_s failed. " << ldap_err2string(errorCode) << endl;
			if(pLdapConnection)
			{
				if(ldap_unbind_s(pLdapConnection)==ERROR_SUCCESS)
				{
					cout << "Cleaning up current connection...\n";
					pLdapConnection = NULL;
				}
			}
			if(pSearchResult)
			{
				ldap_msgfree(pSearchResult);
				pSearchResult = NULL;
			}

     		if( (pLdapConnection = getLdapConnectionCustom(configurationDN, ulSearchSizeLimit, ltConnectionTimeout)) == NULL )
			{
				bConnectErr = true;
				break;	
			}
		}
		else
		{
			//DebugPrintf(SV_LOG_FATAL, "ldap_search_s failed with: 0x%0lx \n",errorCode);
			cout << "[FATAL ERROR][:" << errorCode << "]: ldap_search_s failed. " << ldap_err2string(errorCode) << endl;
			if(pSearchResult != NULL)
			{
				ldap_msgfree(pSearchResult);
				pSearchResult = NULL;						
			}
			bSearchErr = true;
			break;
		}
	}while(1);

	if( bConnectErr )
	{
		cout << "[INFO]: Maximum connection attempts " << MAX_LDAP_CONNECT_ATTEMPTS << " has been reached...\n";
		cout << "[ERROR]: Unable to connect to directory server." << endl;
		return SVE_FAIL;
	}

	if( bSearchErr )
	{
		cout << "[ERROR] Error encountered while searching directory server.\n";
		return SVE_FAIL;
	}
	
	
	ULONG numberOfEntries = ldap_count_entries(pLdapConnection, pSearchResult);

	// Check if there are any storage groups configured at all. NOTE: This is a fatal error
	// because the reason whey we are here is because this storage group did exist in source
	if(numberOfEntries == NULL)
	{
		cout << "Failed to find entry " << replacementDN.c_str() << std::endl;
		cout << "ldap_count_entries failed with errorcode " << errorCode << std::endl;
	    if(pSearchResult != NULL) 
	    {
		    ldap_msgfree(pSearchResult);
			pSearchResult = NULL;
	    }
	    return SVE_FAIL;
	}

	//Get appropriate MTA servers
	string srcMTA, tgtMTA;
		
	srcMTA = (this->srcMTAExchServerName.empty()) ? srcExchServerName : this->srcMTAExchServerName;
	tgtMTA = (this->tgtMTAExchServerName.empty()) ? tgtExchServerName : this->tgtMTAExchServerName;

	LDAPMessage* pLdapMessageEntry = NULL;
	PCHAR pAttribute = NULL;
	BerElement* pBerEncoding = NULL;
	berval** pValuesBin;
	berval** binData;
	PCHAR* ppValue = NULL;
	ULONG iValue = 0;
	string entryDN;
	string entryDNLowerCase;
	string replacementAttrValue;

	// iterate through all mail stores
	for (int index=0; index < (int)numberOfEntries; index++)
	{
	    if(pLdapMessageEntry == NULL)
	        pLdapMessageEntry = ldap_first_entry(pLdapConnection, pSearchResult);
	    else
	        pLdapMessageEntry = ldap_next_entry(pLdapConnection, pLdapMessageEntry);

	    if(pLdapMessageEntry == NULL)
	        break;

		// iterate through all requested attributes on a storage group. Currently it's just "cn"
		for ( pAttribute = ldap_first_attribute( pLdapConnection, pLdapMessageEntry, &pBerEncoding );
				pAttribute != NULL; pAttribute = ldap_next_attribute( pLdapConnection, pLdapMessageEntry, pBerEncoding ) ) 
		{
			if ((strcmpi(pAttribute, "distinguishedName") == 0) ||
				(strcmpi(pAttribute, "cn") == 0) ||
				(strcmpi(pAttribute, "name") == 0) ||
				(findPosIgnoreCase(string(pAttribute), string("homeMDBBL")) == 0) ||
				(strcmpi(pAttribute, "displayName") == 0) ||
				(strcmpi(pAttribute, "adminDisplayName") == 0) ||
				(strcmpi(pAttribute, "objectGUID") == 0))
			{
				ldap_memfree(pAttribute);
				continue;
			}

			// handle binary attributes
			if ((strcmpi(pAttribute, "quotaNotificationSchedule") == 0) ||
				(strcmpi(pAttribute, "activationSchedule") == 0) ||
				(strcmpi(pAttribute, "msExchCIRebuildSchedule") == 0) ||
				(strcmpi(pAttribute, "msExchCIUpdateSchedule") == 0) ||
				(strcmpi(pAttribute, "msExchCatalog") == 0) ||
				(strcmpi(pAttribute, "msExchReplicationSchedule") == 0) ||
				(strcmpi(pAttribute, "msExchPolicyOptionList") == 0) ||
				(strcmpi(pAttribute, "msExchUnmergedAttsPt") == 0) ||
				(strcmpi(pAttribute, "partialAttributeSet") == 0) ||
				(strcmpi(pAttribute, "partialAttributeDeletionList") == 0) ||
				(strcmpi(pAttribute, "proxiedObjectName") == 0) ||
				(strcmpi(pAttribute, "dSASignature") == 0) ||
				(strcmpi(pAttribute, "maximumObjectID") == 0) ||
				(strcmpi(pAttribute, "mS-DS-ConsistencyGuid") == 0) ||
				(strcmpi(pAttribute, "replicationSignature") == 0) ||
				(strcmpi(pAttribute, "replPropertyMetaData") == 0) ||
				(strcmpi(pAttribute, "replUpToDateVector") == 0) ||
				(strcmpi(pAttribute, "repsFrom") == 0) ||
				(strcmpi(pAttribute, "repsTo") == 0) ||
				(strcmpi(pAttribute, "unmergedAtts") == 0) ||
				(strcmpi(pAttribute, "wellKnownObjects") == 0))
			{
				if ((binData = ldap_get_values_len( pLdapConnection, pLdapMessageEntry, pAttribute)) != NULL )
				{
					//cout << "Processing Attribute"  << pAttribute << " Total Values = " << ldap_count_values_len(binData) << "\n";
					
                    size_t vbinlen;
                    INM_SAFE_ARITHMETIC(vbinlen = InmSafeInt<ULONG>::Type(ldap_count_values_len(binData))+1, INMAGE_EX(ldap_count_values_len(binData)))
					pValuesBin = (berval **) calloc(vbinlen, sizeof(berval *));
					ULONG i;
					for ( i = 0; i < ldap_count_values_len(binData); i++ )
					{
						//cout << "Length = " << binData[i]->bv_len << "\n";
						pValuesBin[i] = ber_bvdup(binData[i]);
					}
					pValuesBin[i] = NULL;

					attrValuePairsBin[string(pAttribute)] = pValuesBin;

					// Free memory allocated by ldap_get_values_len(). 
					ldap_value_free_len( binData );
				}
			}
			// handle non-binary attributes
			else if ((strcmpi(pAttribute, "msExchDatabaseCreated") == 0) ||
					(strcmpi(pAttribute, "msExchEDBFile") == 0) ||
					(strcmpi(pAttribute, "msExchEDBOffline") == 0) ||
					(strcmpi(pAttribute, "msExchPatchMDB") == 0) ||
					(strcmpi(pAttribute, "msExchSLVFile") == 0))
					
			{
				if ((ppValue = ldap_get_values( pLdapConnection, pLdapMessageEntry, pAttribute)) != NULL )
				{
					// allocate memory for multivalued attributes. Max of 16 values
                    size_t valuelen;
                    INM_SAFE_ARITHMETIC(valuelen = InmSafeInt<ULONG>::Type(ldap_count_values(ppValue))+1, INMAGE_EX(ldap_count_values(ppValue)))
					pValues = (char **) calloc(valuelen, sizeof(char *));
					ULONG i;
					for ( i = 0; ppValue[i] != NULL; i++ )
					{
						replacementAttrValue.clear();
						// switch CN values that may have the source server name
						if ((strcmpi(pAttribute, "legacyExchangeDN") == 0)) 
						{
							//cout << "\nReplacing Attribute SLASH = " << pAttribute << " Value = " << ppValue[i] << std::endl;
							switchCNValuesSpecial(srcExchServerName, tgtExchServerName, ppValue[i], replacementAttrValue,
										numReplaced, CN_SEPARATOR_SLASH);
							//cout << "\nReplaced Attribute SLASH = " << pAttribute << " Value = " << replacementAttrValue.c_str() << std::endl;
						}
						
						else if ((strcmpi(pAttribute, "mail") == 0) ||
								(strcmpi(pAttribute, "mailNickname") == 0) ||
								(strcmpi(pAttribute, "textEncodedORAddress") == 0) ||
								(strcmpi(pAttribute, "proxyAddresses") == 0))
						{
							//cout << "\nReplacing Attribute NO SEP = " << pAttribute << " Value = " << ppValue[i] << std::endl;
							switchCNValuesNoSeparator(srcExchServerName, tgtExchServerName, ppValue[i], replacementAttrValue);
							//cout << "\nReplaced Attribute NO SEP = " << pAttribute << " Value = " << replacementAttrValue.c_str() << std::endl;
						}
						else 
						{
							if ((strcmpi(pAttribute, "homeMTA") == 0) &&
                                (findPosIgnoreCase(string(ppValue[i]), string(string("=") + srcMTA + ",")) != string::npos))
							{
								switchCNValues(srcMTA, tgtMTA, ppValue[i], replacementAttrValue,
											numReplaced, CN_SEPARATOR_COMMA);
							}
							else
							{
                                //cout << "\nReplacing Attribute = " << pAttribute << " Value = " << ppValue[i] << std::endl;
                                switchCNValues(srcExchServerName, tgtExchServerName, ppValue[i], replacementAttrValue,
											numReplaced, CN_SEPARATOR_COMMA);
							}
						}

						if ((srcTotgtVolMapping.empty() == false)) {
							if ((strcmpi(pAttribute, "msExchEDBFile") == 0) || (strcmpi(pAttribute, "msExchSLVFile") == 0)) {
								char *srcDrive = (char *)(replacementAttrValue.c_str());
								// PR#10815: Long Path support
                                SVGetVolumePathName(srcDrive, drive, ARRAYSIZE(drive));
								int len = strlen(drive);
								if(drive[len-1] == '\\')
								drive[len-1] = '\0';
								const char *tgtDrive = srcTotgtVolMapping[string(drive)].c_str();															
								replacementAttrValue.replace(0, strlen(tgtDrive), tgtDrive);								
							}
						}
						// copy the attribute value
                        size_t pvslen;
                        INM_SAFE_ARITHMETIC(pvslen = InmSafeInt<size_t>::Type(replacementAttrValue.length())+1, INMAGE_EX(replacementAttrValue.length()))
						pValues[i] = (char *)calloc(pvslen, sizeof(char));
                        inm_strcpy_s(pValues[i], pvslen, replacementAttrValue.c_str());
						//cout << "Found Attribute = " << pAttribute << " Value = " << pValues[i] << std::endl;
					}
					pValues[i] = NULL;
					attrValuePairs[string(pAttribute)] = pValues;
					// Free memory allocated by ldap_get_values(). 
					ldap_value_free( ppValue );
				}
			}
	
			// Free memory allocated by ldap_first_attribute(). 
			ldap_memfree( pAttribute );
		}

		if (pBerEncoding != NULL)
			ber_free( pBerEncoding, 0);

		//cout << "Start to add Mail Store\n\n";
		modifyADEntry(pLdapConnection, matchingMailStoresDN, attrValuePairs, attrValuePairsBin);

		// free memory of non-binary attributes
		map<string, char**>::const_iterator iter = attrValuePairs.begin();
		for (; iter != attrValuePairs.end(); iter++) {
			pValues = iter->second;
			for (int index=0; pValues[index] != NULL; index++) {
				free(pValues[index]);
			}
			free(pValues);
		}

		// free memory of binary attributes
		map<string, berval**>::const_iterator iterBin = attrValuePairsBin.begin();
		for (; iterBin != attrValuePairsBin.end(); iterBin++) {
			pValuesBin = iterBin->second;
			for (int index=0; pValuesBin[index] != NULL; index++) {
				ber_bvfree(pValuesBin[index]);
			}
			free(pValuesBin);
		}
	}
	cout << "\nSuccessfully updated Public Mailstore " << matchingMailStoresDN.c_str() << std::endl;
	if( pLdapConnection )
	{
        ldap_unbind_s(pLdapConnection);
		pLdapConnection = NULL;
	}
	return SVS_OK;
}

SVERROR
ADInterface::addMailStoresToADPrivate(LDAP *pLdapConnection, const string& entryDN, const string& objectClass, 
				  map<string, char**>& attrValuePairs, map<string, berval**>& attrValuePairsBin)
{
	LDAPMod *NewEntry[1024];
	LDAPMod *entry;
	ULONG errorCode = LDAP_SUCCESS;

	// construct the add request and add to AD

	// now add all the remaining attrbiutes
	map<string, char**>::const_iterator iter = attrValuePairs.begin();
	map<string, berval**>::const_iterator iterBin = attrValuePairsBin.begin();
	string attrValue;
	string replacementAttrValue;
	int i=0;
	size_t size;

	for (; iter != attrValuePairs.end(); iter++) {
		// skip the distinguished name as that should be set as the DN is created
		// only add the mandatory attributes
		entry = (LDAPMod *) malloc(sizeof(LDAPMod));
		entry->mod_op = LDAP_MOD_ADD;
		size = (*iter).first.length();
        size_t typelen;
        INM_SAFE_ARITHMETIC(typelen = InmSafeInt<size_t>::Type(size)+1, INMAGE_EX(size))
		entry->mod_type = (char *)calloc(typelen, sizeof(char));
		inm_strncpy_s(entry->mod_type,(size+1), (*iter).first.c_str(), size);
		entry->mod_values = (*iter).second;
		NewEntry[i++] = entry;
	}

	for (; iterBin != attrValuePairsBin.end(); iterBin++) {
		// skip the distinguished name as that should be set as the DN is created
		// only add the mandatory attributes
		entry = (LDAPMod *) malloc(sizeof(LDAPMod));
		entry->mod_op = LDAP_MOD_ADD  | LDAP_MOD_BVALUES;
		size = (*iterBin).first.length();
        size_t typelen;
        INM_SAFE_ARITHMETIC(typelen = InmSafeInt<size_t>::Type(size)+1, INMAGE_EX(size))
		entry->mod_type = (char *)calloc(typelen, sizeof(char));
		inm_strncpy_s(entry->mod_type,(size+1), (*iterBin).first.c_str(), size);
		entry->mod_bvalues = (*iterBin).second;
		NewEntry[i++] = entry;
	}

	// initialize last entry to NULL
	NewEntry[i] = NULL;

	cout << "\nAdding entry " << entryDN.c_str() << " to AD" << std::endl << std::endl;

	errorCode = ldap_add_s(pLdapConnection, (const PCHAR) entryDN.c_str(), NewEntry);
	if (errorCode != LDAP_SUCCESS)
	{	
		if (errorCode != LDAP_ALREADY_EXISTS) {
			cout << "Failed to add entry " << entryDN.c_str() 
				<< "\n" << "errorCode = " << errorCode << std::endl;
		}
	}
	else {
		cout << "\n***** Successfully Added Mailbox " << entryDN.c_str() << " to AD" << std::endl << std::endl;
	}

	// release memory
	for (int j=0; j < i; j++) {
		entry = NewEntry[j]; 
		free(entry->mod_type);
		free(entry);
	}

	return SVS_OK;
}

SVERROR
ADInterface::addMailStoresToADPublic(LDAP *pLdapConnection, const string& entryDN, const string& objectClass, 
				  map<string, char**>& attrValuePairs, map<string, berval**>& attrValuePairsBin)
{
	LDAPMod *NewEntry[1024];
	LDAPMod *entry;
	ULONG errorCode = LDAP_SUCCESS;

	// construct the add request and add to AD
	// now add all the remaining attrbiutes
	map<string, char**>::const_iterator iter = attrValuePairs.begin();
	map<string, berval**>::const_iterator iterBin = attrValuePairsBin.begin();
	string attrValue;
	string replacementAttrValue;
	int i=0;
	size_t size;

	for (; iter != attrValuePairs.end(); iter++) {
		// skip the distinguished name as that should be set as the DN is created
		// only add the mandatory attributes
			
		// Skip these attributes and let AD derive these attributes from the DN
		entry = (LDAPMod *) malloc(sizeof(LDAPMod));
		entry->mod_op = LDAP_MOD_ADD;
		size = (*iter).first.length();
        size_t typelen;
        INM_SAFE_ARITHMETIC(typelen = InmSafeInt<size_t>::Type(size)+1, INMAGE_EX(size))
		entry->mod_type = (char *)calloc(typelen, sizeof(char));
		inm_strncpy_s(entry->mod_type,(size+1), (*iter).first.c_str(), size);
		entry->mod_values = (*iter).second;
		NewEntry[i++] = entry;
	}

	for (; iterBin != attrValuePairsBin.end(); iterBin++) {
		// skip the distinguished name as that should be set as the DN is created
		// only add the mandatory attributes
		entry = (LDAPMod *) malloc(sizeof(LDAPMod));
		entry->mod_op = LDAP_MOD_ADD  | LDAP_MOD_BVALUES;
		size = (*iterBin).first.length();
        size_t typelen;
        INM_SAFE_ARITHMETIC(typelen = InmSafeInt<size_t>::Type(size)+1, INMAGE_EX(size))
		entry->mod_type = (char *)calloc(typelen, sizeof(char));
		inm_strncpy_s(entry->mod_type,(size+1), (*iterBin).first.c_str(), size);
		entry->mod_bvalues = (*iterBin).second;
		NewEntry[i++] = entry;
	}

	// initialize last entry to NULL
	NewEntry[i] = NULL;

	errorCode = ldap_add_s(pLdapConnection, (const PCHAR) entryDN.c_str(), NewEntry);

	if (errorCode != LDAP_SUCCESS)
	{	
		if (errorCode != LDAP_ALREADY_EXISTS) {
			cout << "Failed to add entry " << entryDN.c_str() << "\n" << "errorCode = " << errorCode << std::endl;
		}
	}
	else {
		cout << "\n***** Successfully Added Public Store " << entryDN.c_str() << " to AD" << std::endl << std::endl;
	}

	// release memory
	for (int j=0; j < i; j++) {
		entry = NewEntry[j]; 
			free(entry->mod_type);
		free(entry);
	}

	return SVS_OK;
}

SVERROR 
ADInterface::switchCNValuesNoSeparator(const string& search, 
								  const string& replace, 
								  const string& sourceDN, 
								  string& replacementDN)
{
	size_t startPos = 0;
	size_t srcSize = search.length();
	size_t tgtSize = replace.length();
	replacementDN = sourceDN;
	string::size_type pos = replacementDN.find(search, startPos);
	while (pos != string::npos) {
		replacementDN.replace(pos, srcSize, replace);
		startPos = pos+tgtSize;
		pos = replacementDN.find(search, startPos); 
		cout << "Partial replacement=" << replacementDN.c_str() << endl;
	}

	return SVS_OK;
}

SVERROR 
ADInterface::switchCNValuesSpecial(const string& search, 
							  const string& replace,
							  const string& sourceDN,
							  string& replacementDN,
							  ULONG& numReplaced,
							  string szToken) 
{
	// @TODO: Fix errant replaces of LHS values. e.g: We will cause an error if a replacement server is called "CN" or "DC" etc.
	// @TODO: Fix case insensitive compares based on locale

    SVERROR rc = SVS_OK;
    ULONG ulSourceLength;
    INM_SAFE_ARITHMETIC(ulSourceLength = InmSafeInt<ULONG>::Type((ULONG) sourceDN.length()) + 1 /* Length + null character */, INMAGE_EX((ULONG) sourceDN.length()))

    char *sourceDNCopy = (char *) calloc(ulSourceLength, sizeof(char));
    inm_memcpy_s(sourceDNCopy,ulSourceLength, sourceDN.c_str(), ulSourceLength * sizeof(char));

	const char *ch = szToken.c_str();
	if (sourceDNCopy[0] = ch[0]) {
		replacementDN += szToken;
	}

    vector<char *> vToken;
    char *pToken = strtok(sourceDNCopy, szToken.c_str());

    while(pToken)
    {
        vToken.push_back(pToken); //Push token on stack
        pToken = strtok(NULL, szToken.c_str());
    }

	string cnToSearch = "CN=";
	cnToSearch += search;

    for(ULONG i = 0; i < vToken.size(); i++)
    {
        string szReplace;

		if(findPosIgnoreCase(vToken[i], cnToSearch.c_str()) == 0)
        {
            searchAndReplaceToken(search, replace, vToken[i] + 3, szReplace, numReplaced);
            replacementDN += vToken[i][0];
            replacementDN += vToken[i][1];
            replacementDN += vToken[i][2];
            replacementDN += szReplace + szToken;
        }
        else
        {
            replacementDN += vToken[i];
            replacementDN += szToken;
        }
    }

    //Erase last comma
    replacementDN.erase(replacementDN.size() - 1);

	return rc;
}

SVERROR
ADInterface::getExchServerInstallLocation(string &path)
{
	// Get resync drives setting
	//
	CRegKey cregkey;
	HRESULT hr;
	DWORD dwResult = cregkey.Open( HKEY_LOCAL_MACHINE, EXCH_SETUP_REG_KEY );
	if( ERROR_SUCCESS != dwResult )
	{
	    hr = HRESULT_FROM_WIN32( dwResult );
		cout << "FAILED cregkey.Open()... hr = " << hr << std::endl;
	    return SVE_FAIL;
	}

	char szValue[ 512 ];
	DWORD dwCount = sizeof( szValue );
	dwResult = cregkey.QueryStringValue( EXCH_INSTALL_PATH_VALUE_NAME, szValue, &dwCount );
	if( ERROR_SUCCESS != dwResult )
	{
	    hr = HRESULT_FROM_WIN32( dwResult );
		cout << "FAILED cregkey.QueryValue()... hr = " << hr << std::endl;
	    return SVE_FAIL;
	}
	path = string(szValue);
	if (path.empty()) {
		cout << "FAILED to locate Exchange server path" << std::endl;
		return SVE_FAIL;
	}

	return SVS_OK;
}

SVERROR 
ADInterface::getSystemAttendantHomeMDBValue(const string& srcExchServerName, const string& tgtExchServerName, const string& configurationDN, 
											LDAP* pLdapConnection, std::string& sysAttHomeMDBValue)
{
	//search for source's system attendant	
	PCHAR pAttributes[2];
	pAttributes[0] = (PCHAR)(homeMDB.c_str());
	pAttributes[1] = NULL;
	LDAPMessage* pSearchResult;

	cout << "Searching for Microsoft System Attendant of source server..\n";
	string filter = "(objectClass=exchangeAdminService)";
	ULONG errorCode;
	
	ULONG ulSearchSizeLimit = 0;	// Limit number of users fetched at a time
	LDAP_TIMEVAL ltConnectionTimeout;
	ltConnectionTimeout.tv_sec = 0;		//CONNECTION_ESTABLISHMENT_TIMEOUT;	
	bool bSearchErr = false, bConnectErr = false;

	do
	{
		errorCode= ldap_search_s(pLdapConnection,
					(const PCHAR) configurationDN.c_str(),	// DN to start search
					LDAP_SCOPE_SUBTREE,						 // Indicate the search scope
					(const PCHAR) filter.c_str(),			// Search Filter
					pAttributes,							// Retrieve list of attributes
					0,										//retrieve both attributesand values
					&pSearchResult);						// this is out parameter - Search results
		if( errorCode == LDAP_SUCCESS )
			break;    
	    
		if( errorCode == LDAP_SERVER_DOWN || 
			errorCode == LDAP_UNAVAILABLE ||
			errorCode == LDAP_NO_MEMORY ||
			errorCode == LDAP_BUSY ||
			errorCode == LDAP_LOCAL_ERROR ||
			errorCode == LDAP_TIMEOUT ||
			errorCode == LDAP_OPERATIONS_ERROR ||
			errorCode == LDAP_TIMELIMIT_EXCEEDED)
		{
			cout << "[ERROR][" << errorCode << "]: ldap_search_s failed. " << ldap_err2string(errorCode) << endl;
			if(pLdapConnection)
			{
				if(ldap_unbind_s(pLdapConnection)==ERROR_SUCCESS)
				{
					cout << "Cleaning up current connection...\n";
					pLdapConnection = NULL;
				}
			}
			if(pSearchResult)
			{
				ldap_msgfree(pSearchResult);
				pSearchResult = NULL;
			}

     		if( (pLdapConnection = getLdapConnectionCustom(configurationDN, ulSearchSizeLimit, ltConnectionTimeout)) == NULL )
			{
				bConnectErr = true;
				break;	
			}
		}
		else
		{
			//DebugPrintf(SV_LOG_FATAL, "ldap_search_s failed with: 0x%0lx \n",errorCode);
			cout << "[FATAL ERROR][:" << errorCode << "]: ldap_search_s failed. " << ldap_err2string(errorCode) << endl;
			if(pSearchResult != NULL)
			{
				ldap_msgfree(pSearchResult);
				pSearchResult = NULL;						
			}
			bSearchErr = true;
			break;
		}
	}while(1);

	if( bConnectErr )
	{
		cout << "[INFO]: Maximum connection attempts " << MAX_LDAP_CONNECT_ATTEMPTS << " has been reached...\n";
		cout << "[ERROR]: Unable to connect to directory server." << endl;
		return SVE_FAIL;
	}

	if( bSearchErr )
	{
		cout << "[ERROR] Error encountered while searching directory server.\n";
		return SVE_FAIL;
	}
	
	ULONG numberOfEntries = ldap_count_entries(pLdapConnection, pSearchResult);

	if(numberOfEntries == NULL)
	{
	    DebugPrintf(SV_LOG_WARNING, "ldap_count_entries failed with 0x%0lx \n", errorCode);
	    if(pSearchResult != NULL) 
	    {
		    ldap_msgfree(pSearchResult);
			pSearchResult = NULL;
	    }
		cout << "No entries found for filter " << filter.c_str() << std::endl;
		return SVS_OK;
	}

	LDAPMessage* pLdapMessageEntry = NULL;
	string entryDN;
	string entryDNLowerCase;
	string exchServerCN = "CN=";
	exchServerCN += srcExchServerName;
	string exchServerCNLowerCase = convertoToLower(exchServerCN, exchServerCN.length());

	string searchFor;
	for (int index=0; index < (int)numberOfEntries; index++)
	{
	    if(pLdapMessageEntry == NULL)
	        pLdapMessageEntry = ldap_first_entry(pLdapConnection, pSearchResult);
	    else
	        pLdapMessageEntry = ldap_next_entry(pLdapConnection, pLdapMessageEntry);

	    if(pLdapMessageEntry == NULL)
	        break;
		
		entryDN = ldap_get_dn(pLdapConnection, pLdapMessageEntry);
		entryDNLowerCase = convertoToLower(entryDN, entryDN.length());
		
		//cout <<"entryDNLowerCase=" << entryDNLowerCase.c_str() << endl;
		//cout <<"exchServerCNLowerCase=" << exchServerCNLowerCase.c_str() << endl;        

		searchFor  = exchServerCNLowerCase;
		searchFor += ",cn=servers";
		
		// is this entry part of the concerned exchange server
		if(strstr(entryDNLowerCase.c_str(), searchFor.c_str()) == NULL)
		{
			searchFor.clear();
			// move to the next entry
			continue;
		}
		
		ULONG numReplaced = 0;
		string replacementAttrValue;
		PCHAR* ppValue;
		cout << "Retrieving homeMDB attribute of Microsoft System Attendant of source server..\n";
		ppValue = ldap_get_values(pLdapConnection, pLdapMessageEntry, pAttributes[0]);

		if( ppValue == NULL )
		{
			cout << "[WARNING]: homeMDB Attribute is <not set> for Microsoft System Attendant of source server.\n"
				<< "           Please verify if Microsoft System Attendant of source server is configured properly.\n"
				<< "           Skipping the processing of homeMDB attribute of System Attendant mailbox.\n";
			return SVE_FAIL;
		}
		else
		{
            //Replace source's CN by target's CN
			//cout << "\nGetSystemAttendantHomeMDBValue::Replacing Attribute = " << pAttributes[0] << " Value = " << ppValue[0] << std::endl;		
		}

		switchCNValues(srcExchServerName, tgtExchServerName, ppValue[0], replacementAttrValue,
							numReplaced, CN_SEPARATOR_COMMA);

		sysAttHomeMDBValue = replacementAttrValue;
		break;
	}

	return SVS_OK;
}

SVERROR 
ADInterface::setHomeMDBForMicrosoftSystemAttendat(const string& configurationDN, 
											 const string& exchangeServerName, 
											 LDAP *pLdapConnection, 
											 const string& value)
{
	// Get the CN of the Storage Groups. NOTE: the object class for storage group is "msExchStorageGroupd"
	PCHAR pAttributes[2];
	pAttributes[0] = (PCHAR)(homeMDB.c_str());
	pAttributes[1] = NULL;
	LDAPMessage* pSearchResult;

	string filter = "(objectClass=exchangeAdminService)";
	ULONG errorCode;
	


	ULONG ulSearchSizeLimit = 0;	// Limit number of users fetched at a time
	LDAP_TIMEVAL ltConnectionTimeout;
	ltConnectionTimeout.tv_sec = 0;		//CONNECTION_ESTABLISHMENT_TIMEOUT;	
	bool bSearchErr = false, bConnectErr = false;

	do
	{
		errorCode= ldap_search_s(pLdapConnection,						
					(const PCHAR) configurationDN.c_str(),	// DN to start search
					LDAP_SCOPE_SUBTREE,						 // Indicate the search scope
					(const PCHAR) filter.c_str(),			// Search Filter
					pAttributes,							// Retrieve list of attributes
					0,										//retrieve both attributesand values
					&pSearchResult);						// this is out parameter - Search results

		if( errorCode == LDAP_SUCCESS )
			break;    
	    
		if( errorCode == LDAP_SERVER_DOWN || 
			errorCode == LDAP_UNAVAILABLE ||
			errorCode == LDAP_NO_MEMORY ||
			errorCode == LDAP_BUSY ||
			errorCode == LDAP_LOCAL_ERROR ||
			errorCode == LDAP_TIMEOUT ||
			errorCode == LDAP_OPERATIONS_ERROR ||
			errorCode == LDAP_TIMELIMIT_EXCEEDED)
		{
			cout << "[ERROR][" << errorCode << "]: ldap_search_s failed. " << ldap_err2string(errorCode) << endl;
			if(pLdapConnection)
			{
				if(ldap_unbind_s(pLdapConnection)==ERROR_SUCCESS)
				{
					cout << "Cleaning up current connection...\n";
					pLdapConnection = NULL;
				}
			}
			if(pSearchResult)
			{
				ldap_msgfree(pSearchResult);
				pSearchResult = NULL;
			}

     		if( (pLdapConnection = getLdapConnectionCustom(configurationDN, ulSearchSizeLimit, ltConnectionTimeout)) == NULL )
			{
				bConnectErr = true;
				break;	
			}
		}
		else
		{
			//DebugPrintf(SV_LOG_FATAL, "ldap_search_s failed with: 0x%0lx \n",errorCode);
			cout << "[FATAL ERROR][:" << errorCode << "]: ldap_search_s failed. " << ldap_err2string(errorCode) << endl;
			if(pSearchResult != NULL)
			{
				ldap_msgfree(pSearchResult);
				pSearchResult = NULL;						
			}
			bSearchErr = true;
			break;
		}
	}while(1);

	if( bConnectErr )
	{
		cout << "[INFO]: Maximum connection attempts " << MAX_LDAP_CONNECT_ATTEMPTS << " has been reached...\n";
		cout << "[ERROR]: Unable to connect to directory server." << endl;
		return SVE_FAIL;
	}

	if( bSearchErr )
	{
		cout << "[ERROR] Error encountered while searching directory server.\n";
		return SVE_FAIL;
	}
	
	ULONG numberOfEntries = ldap_count_entries(pLdapConnection, pSearchResult);

	// Check if there are any storage groups configured at all
	if(numberOfEntries == NULL)
	{
	    DebugPrintf(SV_LOG_WARNING, "ldap_count_entries failed with 0x%0lx \n", errorCode);
	    if(pSearchResult != NULL) 
	    {
		    ldap_msgfree(pSearchResult);
			pSearchResult = NULL;
	    }
		cout << "No entries found for filter " << filter.c_str() << std::endl;
		return SVS_OK;
	}

	LDAPMessage* pLdapMessageEntry = NULL;
	string entryDN;
	string entryDNLowerCase;
	string exchServerCN = "CN=";
	exchServerCN += exchangeServerName;
	string exchServerCNLowerCase = convertoToLower(exchServerCN, exchServerCN.length());

	// iterate through all storage groups
	string searchFor;
	for (int index=0; index < (int)numberOfEntries; index++)
	{
	    if(pLdapMessageEntry == NULL)
	        pLdapMessageEntry = ldap_first_entry(pLdapConnection, pSearchResult);
	    else
	        pLdapMessageEntry = ldap_next_entry(pLdapConnection, pLdapMessageEntry);

	    if(pLdapMessageEntry == NULL)
	        break;

		// get storage group DN
		entryDN = ldap_get_dn(pLdapConnection, pLdapMessageEntry);
		entryDNLowerCase = convertoToLower(entryDN, entryDN.length());
		
		//cout <<"entryDNLowerCase=" << entryDNLowerCase.c_str() << endl;
		//cout <<"exchServerCNLowerCase=" << exchServerCNLowerCase.c_str() << endl;        

		searchFor  = exchServerCNLowerCase;
		searchFor += ",cn=servers";
		
		// is this entry part of the concerned exchange server
		if(strstr(entryDNLowerCase.c_str(), searchFor.c_str()) == NULL)
		{
			searchFor.clear();
			// move to the next entry
			continue;
		}

		// replace the attribute value
		LDAPMod modSourceAttribute;
		LDAPMod* changedAttributes[] = {&modSourceAttribute, NULL};
		char *replacementValues[2];

		// Set up the attributes to be modified
		modSourceAttribute.mod_op = LDAP_MOD_REPLACE;
		modSourceAttribute.mod_type = (PCHAR)homeMDB.c_str();
    
		replacementValues[0] = (PCHAR)(value.c_str());
		replacementValues[1] = NULL;

		modSourceAttribute.mod_vals.modv_strvals = replacementValues;

		//cout << "\nModifying homeMDB attribute for Entry = " << entryDN.c_str()
				//<< " New Value = " << value.c_str() << std::endl;
		errorCode = ldap_modify_s(pLdapConnection, (PCHAR)(entryDN.c_str()), changedAttributes);
		if (errorCode != LDAP_SUCCESS) {
			cout << "ldap_modify_s failed while modifying entry = " << entryDN.c_str() << std::endl;
			return SVE_FAIL;
		}
		else {
			cout << "Successfully set homeMDB attribute of Microsoft System Attendant:\n\t" << value.c_str() << std::endl << std::endl;
		}
	}

	return SVS_OK;
}

// This method creates an SMTP Connection object in AD for each of the mailbox (Public and Private).
// This is required for viewing the public folders and enables users to send and receive emails.
SVERROR
ADInterface::createSMTPConnections(const string& srcExchServerName, const string& tgtExchServerName, const list<string>& missingMailStoresDN)
{
	SVERROR retCode = SVS_OK;
	// Get the object GUID for the mailbox
	if (missingMailStoresDN.empty() == false) {
		list<string>::const_iterator beginIter = missingMailStoresDN.begin();
		list<string>::const_iterator endIter = missingMailStoresDN.end();
		for (;beginIter != endIter; beginIter++) {
			string objectGUID, normObjectGUID;
			// query objectGUID for the mailbox
			if (queryAttrValueForDN(*(beginIter), OBJECT_GUID, ATTRIBUTE_BINARY, objectGUID) == SVS_OK) {
				//cout << "Object GUID is = " << objectGUID.c_str() << std::endl;
				normalizeObjectGUID(objectGUID, normObjectGUID);
				string mtaDN, tgtMTA;
				
				//Initialize the target MTA server
				tgtMTA = (this->tgtMTAExchServerName.empty()) ? tgtExchServerName : this->tgtMTAExchServerName;

				if (getMTADN(tgtMTA, mtaDN) == SVS_OK)
				{
					cout << "MTA DN is = " << mtaDN.c_str() << std::endl;
					if(mtaDN.empty())
					{			
                        cout << "[ERROR] Could not find MTA resource on the server: " << tgtMTA << endl
							<< "Failover attempt failed while creating SMTP connectors for mail stores on server: " << tgtMTA << endl;
						return SVE_FAIL;
					}
					// get the legacy exchange DN for an SMTP connections object for this server
					string legacyExchangeDN;
					if (getLegacyExchangeDN(srcExchServerName, tgtExchServerName, legacyExchangeDN, normObjectGUID) == SVS_OK) {
						//cout << "legacy Exchange DN is = " << legacyExchangeDN.c_str() << std::endl;
						// now create the SMTP connections object
						retCode = createSMTPConnectionObjectInAD(srcExchServerName, tgtExchServerName, normObjectGUID, mtaDN, (*beginIter), legacyExchangeDN);
						if (retCode == SVE_FAIL) {
							cout << "createSMTPConnections. Failed to create SMTP Connection object in AD " << std::endl;
							return SVE_FAIL;
						}
					}
					else {
						cout << "createSMTPConnections. Failed to get LegacyExchangeDN " << std::endl;
						return SVE_FAIL;
					}
				}
				else {
					cout << "createSMTPConnections. Failed to get MTA DN for server " << tgtMTA.c_str() << std::endl;
					return SVE_FAIL;
				}
			}
			else {
				cout << "createSMTPConnections. Failed to query Object GUID for " << (*beginIter).c_str() << std::endl;
				return SVE_FAIL;
			}
		}
	}
	return SVS_OK;
}

SVERROR
ADInterface::createSMTPConnectionObjectInAD(const string& srcExchServerName, const string& tgtExchServerName, const string& objGUID, 
							   const string& mtaDN, const string& storeDN, const string& legacyExchangeDN)
{
	// find configuration Naming Context DN
	string configurationDN;
	if (getConfigurationDN(configurationDN) == SVE_FAIL) 
	{
		cout << "Configuration Naming Context not found\n";
		return SVE_FAIL;
	}

	// get an Ldap connection
	LDAP* pLdapConnection = getLdapConnection();
	if (pLdapConnection == NULL)
	{
		cout << "Failed to connection to the ldap server\n";
		return SVE_FAIL;
	}

	// ldap bind
	ULONG errorCode = LDAP_SUCCESS;
	ULONG lRtn = 0;
	lRtn = ldap_bind_s(pLdapConnection,						
				(const PCHAR) configurationDN.c_str(),		// Use the naming context from the input list
				NULL,										// if it is NULL it will use current user or service credentials 
				LDAP_AUTH_NEGOTIATE);						// LDAP authentication mode
	if(lRtn != LDAP_SUCCESS)
	{
		cout << "ldap_bind_s failed with:" << lRtn << std::endl;
		return SVE_FAIL;
	}

	map<string, char** > attrValuePairs;
	map<string, berval** > attrValuePairsBin;

	// consturct the CN
	string cn = "SMTP (";
	cn += tgtExchServerName;
	cn += "-{";
	cn += objGUID;
	cn += "})";
	char **pValues = (char **) calloc(2, sizeof(char *));
    size_t valuelen;
    INM_SAFE_ARITHMETIC(valuelen = InmSafeInt<size_t>::Type(cn.length())+1, INMAGE_EX(cn.length()))
	pValues[0] = (char *)calloc(valuelen, sizeof(char));
    inm_strcpy_s(pValues[0], valuelen, cn.c_str());
	pValues[1] = NULL;
	attrValuePairs[CN] = pValues;
	// set the homeMTA
	pValues = (char **) calloc(2, sizeof(char *));
    INM_SAFE_ARITHMETIC(valuelen = InmSafeInt<size_t>::Type(mtaDN.length())+1, INMAGE_EX(mtaDN.length()))
	pValues[0] = (char *)calloc(valuelen, sizeof(char));
    inm_strcpy_s(pValues[0], valuelen, mtaDN.c_str());
	pValues[1] = NULL;
	attrValuePairs[homeMTA] = pValues;
	// set the deliver mechanism
	pValues = (char **) calloc(2, sizeof(char *));
	pValues[0] = (char *)calloc(2, sizeof(char));
	inm_strcpy_s(pValues[0],2,"2");
	pValues[1] = NULL;
	attrValuePairs[deliveryMechanism] = pValues;
	// set the homeMDB
	pValues = (char **) calloc(2, sizeof(char *));
    INM_SAFE_ARITHMETIC(valuelen = InmSafeInt<size_t>::Type(storeDN.length())+1, INMAGE_EX(storeDN.length()))
	pValues[0] = (char *)calloc(valuelen, sizeof(char));
    inm_strcpy_s(pValues[0], valuelen, storeDN.c_str());
	pValues[1] = NULL;
	attrValuePairs[homeMDB] = pValues;
	// set the legacy exchange DN
	pValues = (char **) calloc(2, sizeof(char *));
    INM_SAFE_ARITHMETIC(valuelen = InmSafeInt<size_t>::Type(legacyExchangeDN.length())+1, INMAGE_EX(legacyExchangeDN.length()))
	pValues[0] = (char *)calloc(valuelen, sizeof(char));
    inm_strcpy_s(pValues[0], valuelen, legacyExchangeDN.c_str());
	pValues[1] = NULL;
	attrValuePairs[LEGACY_EXCHANGE_DN] = pValues;

	// construct the DN to be added
	// Query any existing connections object for this server and use that as a reference to build the new DN
	string mailGatewayDN;
	list<string> mailGatewayCNs, mailGatewayDNs;
	// find any existing SMTP connection for the target exchange server
	if (searchSmtpEntryinAD(tgtExchServerName, mailGatewayCNs, mailGatewayDNs, string("(objectclass=mailGateway)")) == SVE_FAIL) {
		cout << "WARNING: No connections object exists for exchange server " << tgtExchServerName.c_str() << std::endl;
	}
	// if no connections object exist for the target server then find one for the source server
	if (mailGatewayDNs.empty() == true) {
		if (searchSmtpEntryinAD(srcExchServerName, mailGatewayCNs, mailGatewayDNs, string("(objectclass=mailGateway)")) == SVE_FAIL) {
			cout << "Failed to find a connections object for exchange server " << srcExchServerName.c_str() << std::endl;
			cout << "Please verify that Storage Groups are configured for Source Exchange Server " << srcExchServerName.c_str() << std::endl;
			
			return SVE_FAIL;
		}
		switchCNValuesNoSeparator(srcExchServerName, tgtExchServerName, *(mailGatewayDNs.begin()), mailGatewayDN);
	}
	else {
		mailGatewayDN = *(mailGatewayDNs.begin());
	}

	// initialize the first CN of the DN
	string smtpConnDN = "CN=";
	smtpConnDN += cn;
	// tokenize the queried DN and use everything except the first token
	vector<char *> vToken;

	cout << ">>>mailGatewayDN = " << mailGatewayDN.c_str() << endl;
	//cout << "Procesing Mail Gateway DN = " << (*iter).c_str() << std::endl;
    char *pToken = strtok((char *)(mailGatewayDN.c_str()), CN_SEPARATOR_COMMA.c_str());

    while(pToken)
    {
        vToken.push_back(pToken);
        pToken = strtok(NULL, CN_SEPARATOR_COMMA.c_str());
    }

    for(ULONG i = 1; i < vToken.size(); i++)
    {
		smtpConnDN += CN_SEPARATOR_COMMA;
		smtpConnDN += vToken[i];
    }	

	// add entry to AD
	cout << "Adding mailGateway DN = " << smtpConnDN.c_str() << endl;
	SVERROR retCode = addEntryToAD(pLdapConnection, smtpConnDN, MAIL_GATEWAY, attrValuePairs, attrValuePairsBin);
	if (retCode == SVE_FAIL) {
		cout << "ADInterface::createSMTPConnectionObjectInAD. Failed to add DN " << smtpConnDN.c_str() << " to Active Directory\n";
		return SVE_FAIL;
	}

	if( pLdapConnection )
	{
        ldap_unbind_s(pLdapConnection);
		pLdapConnection = NULL;
	}
	return SVS_OK;
}

SVERROR
ADInterface::queryAttrValueForDN(const string& dn, const string& attr, const string& attrType, string& value)
{
	// find configuration Naming Context DN
	string configurationDN;
	if (getConfigurationDN(configurationDN) == SVE_FAIL) 
	{
		cout << "Configuration Naming Context not found\n";
		return SVE_FAIL;
	}

	// get an Ldap connection
	LDAP* pLdapConnection = getLdapConnection();
	if (pLdapConnection == NULL)
	{
		cout << "Failed to connection to the ldap server\n";
		return SVE_FAIL;
	}

	// ldap bind
	ULONG errorCode = LDAP_SUCCESS;
	ULONG lRtn = 0;
	lRtn = ldap_bind_s(pLdapConnection,						
				(const PCHAR) configurationDN.c_str(),		// Use the naming context from the input list
				NULL,										// if it is NULL it will use current user or service credentials 
				LDAP_AUTH_NEGOTIATE);						// LDAP authentication mode
	if(lRtn != LDAP_SUCCESS)
	{
		cout << "ldap_bind_s failed with:" << lRtn << std::endl;
		return SVE_FAIL;
	}

	PCHAR pAttributes[2];
	pAttributes[0] = (PCHAR)(attr.c_str());
	pAttributes[1] = NULL;
	LDAPMessage* pSearchResult;


	ULONG ulSearchSizeLimit = 0;	// Limit number of users fetched at a time
	LDAP_TIMEVAL ltConnectionTimeout;
	ltConnectionTimeout.tv_sec = 0;		//CONNECTION_ESTABLISHMENT_TIMEOUT;	
	bool bSearchErr = false, bConnectErr = false;

	do
	{
		errorCode = ldap_search_s(pLdapConnection,						// Session handle
					(const PCHAR) dn.c_str(),				// DN to start search
					LDAP_SCOPE_BASE,						 // Indicate the search scope
					(const PCHAR)("ObjectClass=*"),               		// Search Filter
					pAttributes,							// Retrieve list of attributes
					0,										//retrieve both attributesand values
					&pSearchResult);						// this is out parameter - Search results
		if( errorCode == LDAP_SUCCESS )
			break;    
	    
		if( errorCode == LDAP_SERVER_DOWN || 
			errorCode == LDAP_UNAVAILABLE ||
			errorCode == LDAP_NO_MEMORY ||
			errorCode == LDAP_BUSY ||
			errorCode == LDAP_LOCAL_ERROR ||
			errorCode == LDAP_TIMEOUT ||
			errorCode == LDAP_OPERATIONS_ERROR ||
			errorCode == LDAP_TIMELIMIT_EXCEEDED)
		{
			cout << "[ERROR][" << errorCode << "]: ldap_search_s failed. " << ldap_err2string(errorCode) << endl;
			if(pLdapConnection)
			{
				if(ldap_unbind_s(pLdapConnection)==ERROR_SUCCESS)
				{
					cout << "Cleaning up current connection...\n";
					pLdapConnection = NULL;
				}
			}
			if(pSearchResult)
			{
				ldap_msgfree(pSearchResult);
				pSearchResult = NULL;
			}

     		if( (pLdapConnection = getLdapConnectionCustom(configurationDN, ulSearchSizeLimit, ltConnectionTimeout)) == NULL )
			{
				bConnectErr = true;
				break;	
			}
		}
		else
		{
			//DebugPrintf(SV_LOG_FATAL, "ldap_search_s failed with: 0x%0lx \n",errorCode);
			cout << "[FATAL ERROR][:" << errorCode << "]: ldap_search_s failed. " << ldap_err2string(errorCode) << endl;
			if(pSearchResult != NULL)
			{
				ldap_msgfree(pSearchResult);
				pSearchResult = NULL;						
			}
			bSearchErr = true;
			break;
		}
	}while(1);

	if( bConnectErr )
	{
		cout << "[INFO]: Maximum connection attempts " << MAX_LDAP_CONNECT_ATTEMPTS << " has been reached...\n";
		cout << "[ERROR]: Unable to connect to directory server." << endl;
		return SVE_FAIL;
	}

	if( bSearchErr )
	{
		cout << "[ERROR] Error encountered while searching directory server.\n";
		return SVE_FAIL;
	}

	LDAPMessage* pLdapMessageEntry = ldap_first_entry(pLdapConnection, pSearchResult);

	if (attrType == ATTRIBUTE_BINARY) {
		berval** binData;
		
		if ((binData = ldap_get_values_len( pLdapConnection, pLdapMessageEntry, pAttributes[0])) != NULL ) {
			BuildGUIDString((LPBYTE) binData[0]->bv_val, value);
	    	// Free memory allocated by ldap_get_values_len(). 
			ldap_value_free_len( binData );
		}
	}
	else {
		PCHAR* ppValue = NULL;
		ppValue = ldap_get_values(pLdapConnection, pLdapMessageEntry, pAttributes[0]);
		value = ppValue[0];
		// Free memory allocated by ldap_get_values(). 
		ldap_value_free( ppValue );
	}

	//cout << "Querying DN " << dn.c_str() << "\nAttribute " << attr.c_str() << "\nValue " << value.c_str() << std::endl;
	if( pLdapConnection )
	{
        ldap_unbind_s(pLdapConnection);
		pLdapConnection = NULL;
	}
	return SVS_OK;
}

void 
ADInterface::BuildGUIDString(LPBYTE pGUID, string& value)
{
	char szGUID[33];		// string version of the GUID
 
	inm_sprintf_s(szGUID,ARRAYSIZE(szGUID), "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x", 
		pGUID[0], pGUID[1], pGUID[2], pGUID[3],
		pGUID[4], pGUID[5], pGUID[6], pGUID[7], 
		pGUID[8],pGUID[9], pGUID[10], pGUID[11],
		pGUID[12], pGUID[13],pGUID[14], pGUID[15]);

	value = szGUID;
}

SVERROR
ADInterface::getMTADN(const string& exchangeServerName, string& dn)
{
	list<string> mtaCN, mtaDN;
	if (searchEntryinAD(exchangeServerName, mtaCN, mtaDN, string("(objectclass=mTA)")) == SVE_FAIL) {
		cout << "Failed to find MTA DN for exchange server " << exchangeServerName.c_str() << std::endl;
		return SVE_FAIL;
	}

	list<string>::const_iterator iter = mtaDN.begin();
	dn = (*iter);
	return SVS_OK;
}

SVERROR
ADInterface::getLegacyExchangeDN(const string& srcExchServerName, const string& tgtExchServerName, string& legacyExchangeDN, const string& objGUID)
{
	list<string> mailGatewayCN, mailGatewayDN;
	int flag = 0;
	// find any existing SMTP connection for the target exchange server
	if (searchSmtpEntryinAD(tgtExchServerName, mailGatewayCN, mailGatewayDN, string("(objectclass=mailGateway)")) == SVE_FAIL) {
		cout << "Failed to find a connections object for exchange server " << tgtExchServerName.c_str() << std::endl;
		return SVE_FAIL;
	}

	// if no entry is found then search for a SMTP connections object for the source exchange server
	if (mailGatewayDN.empty() == true) {
		if (searchSmtpEntryinAD(srcExchServerName, mailGatewayCN, mailGatewayDN, string("(objectclass=mailGateway)")) == SVE_FAIL) {
			cout << "Failed to find a connections object for exchange server " << srcExchServerName.c_str() << std::endl;
			return SVE_FAIL;
		}
		flag = 1;
	}

	if (mailGatewayDN.empty() == true) {
		cout << "Failed to find a connections object for exchange server " << srcExchServerName.c_str() << std::endl;
		return SVE_FAIL;
	}

	list<string>::const_iterator iter = mailGatewayDN.begin();

	// find the legacy exchange DN attribute value for the above SMTP connection
	string legacyExchDN;
	if (queryAttrValueForDN((*iter), LEGACY_EXCHANGE_DN, ATTRIBUTE_STRING, legacyExchDN) == SVE_FAIL) {
		cout << "Failed to find legacy exchange DN attribute for mailGateway " << (*iter).c_str() << std::endl;
		return SVE_FAIL;
	}

	// replace all occurences of source server with target server name
	string newLegacyExchangeDN;
	//cout << "legacyExchDN= " << legacyExchDN.c_str() << endl;
	string src1 = "(";
	src1 += srcExchServerName;
	src1 += ")";
	string tgt1 = "(";
	tgt1 += tgtExchServerName;
	tgt1 += ")";

	if( flag )
	{
        switchCNValuesNoSeparator(src1, tgt1, legacyExchDN, newLegacyExchangeDN);
	}
	else
		newLegacyExchangeDN = legacyExchDN;

	// modify the legachExchDN last token to the value of the objGUID
	size_t len = newLegacyExchangeDN.length() - 37;
	char base[512];
	inm_strncpy_s(base, ARRAYSIZE(base),newLegacyExchangeDN.c_str(), len);
	base[len] = '\0';
	inm_strcat_s(base, ARRAYSIZE(base),objGUID.c_str());
	inm_strcat_s(base,ARRAYSIZE(base), "}");

	legacyExchangeDN = base;

	//cout << "New legacy Exchange DN = " << legacyExchangeDN.c_str() << std::endl;

	return SVS_OK;
}

void
ADInterface::normalizeObjectGUID(const string &objectGUID, string& normObjectGUID)
{
	string strCopy = objectGUID;
	char *copy = (char *)(strCopy.c_str());

	char temp[37];
	temp[0] = *(copy+6); temp[1] = *(copy+7); temp[2] = *(copy+4); temp[3] = *(copy+5);
	temp[4] = *(copy+2); temp[5] = *(copy+3); temp[6] = *(copy+0); temp[7] = *(copy+1);
	temp[8] = '-'; 
	temp[9] = *(copy+10); temp[10] = *(copy+11); temp[11] = *(copy+8); temp[12] = *(copy+9);
	temp[13] = '-';
	temp[14] = *(copy+14); temp[15] = *(copy+15); temp[16] = *(copy+12); temp[17] = *(copy+13);
	temp[18] = '-';
	temp[19] = *(copy+16); temp[20] = *(copy+17); temp[21] = *(copy+18); temp[22] = *(copy+19);
	temp[23] = '-';
	temp[24] = *(copy+20); temp[25] = *(copy+21); temp[26] = *(copy+22); temp[27] = *(copy+23);
	temp[28] = *(copy+24); temp[29] = *(copy+25); temp[30] = *(copy+26); temp[31] = *(copy+27);
	temp[32] = *(copy+28); temp[33] = *(copy+29); temp[34] = *(copy+30); temp[35] = *(copy+31);
	temp[36] = '\0';

	normObjectGUID = temp;
}

bool
ADInterface::replaceAttributeDryRun(string attrName, string attrValue, string srcExchangeSource, string srcExchangeDest, LDAP* pLdapConnection, PCHAR pEntryDN)
{
    return true;
}

ULONG 
ADInterface::modifyPatchMDB(LDAP *pLdapConnection,PCHAR pEntryDN)
{
    LDAPMod modSourceAttribute;
	LDAPMod* changedAttributes[] = {&modSourceAttribute, NULL};
	char *replacementValues[2];

	// Set up the attributes to be modified
	modSourceAttribute.mod_op = LDAP_MOD_REPLACE;
    modSourceAttribute.mod_type = (PCHAR)msExchPatchMDB.c_str(); 
    
    replacementValues[0] = (PCHAR)"TRUE";
    replacementValues[1] = NULL;

    modSourceAttribute.mod_vals.modv_strvals = replacementValues;
	ULONG ldrc = ldap_modify_s(pLdapConnection, pEntryDN, changedAttributes);
    return ldrc;
}

ULONG
ADInterface::modifyPatchMDBDryRun(LDAP *pLdapConnection,PCHAR pEntryDN)
{
    return LDAP_SUCCESS;
}

SVERROR
ADInterface::rerouteRUS(const string& srcExchServerName, const string& tgtExchServerName)
{
	if(strcmpi(srcExchServerName.c_str(), tgtExchServerName.c_str()) == 0)
	{
		//same server udpate case
		cout << "[INFO]: Skipping RUS rerouting as source & target server specified are same.\n";
		return SVS_OK;
	}
	// find configuration Naming Context DN
	string configurationDN;
	if (getConfigurationDN(configurationDN) == SVE_FAIL) 
	{
		cout << "Configuration Naming Context not found\n";
		return SVE_FAIL;
	}
	else
	{
		cout << "\nConfiguration Naming Context : " << configurationDN << endl << endl;
	}

	// get an Ldap connection
	LDAP* pLdapConnection = getLdapConnection();
	if (pLdapConnection == NULL)
	{
		cout << "Failed to connect to the ldap server\n";
		return SVE_FAIL;
	}

	// ldap bind
	ULONG errorCode = LDAP_SUCCESS;
	ULONG lRtn = 0;
	lRtn = ldap_bind_s(pLdapConnection,							// Session Handle
				(const PCHAR) configurationDN.c_str(),		// Use the naming context from the input list
				NULL,										// if it is NULL it will use current user or service credentials 
				LDAP_AUTH_NEGOTIATE);						// LDAP authentication mode
	if(lRtn != LDAP_SUCCESS)
	{
		cout << "ldap_bind_s failed with:" << lRtn << std::endl;
		return SVE_FAIL;
	}

	LDAPMessage* pSearchResult;
	// search all the RUS entries
    PCHAR pSearchFilter = "(objectClass=msExchAddressListService)";
	PCHAR pAttributes[2];
    pAttributes[0] = (PCHAR)MS_EXCH_ADDRESS_LIST_SERVICE_LINK.c_str();
	pAttributes[1] = NULL;

	ULONG ulSearchSizeLimit = 0;	// Limit number of users fetched at a time
	LDAP_TIMEVAL ltConnectionTimeout;
	ltConnectionTimeout.tv_sec = 0;		//CONNECTION_ESTABLISHMENT_TIMEOUT;	
	bool bSearchErr = false, bConnectErr = false;

	do
	{
		errorCode = ldap_search_s(pLdapConnection,						// Session handle
						(const PCHAR) configurationDN.c_str(),  // DN to start search
						LDAP_SCOPE_SUBTREE,						 // Indicate the search scope
						pSearchFilter,								// Search Filter
						pAttributes,							// Retrieve list of attributes
						0,										//retrieve both attributesand values
						&pSearchResult);						// this is out parameter - Search results
		if( errorCode == LDAP_SUCCESS )
			break;    
	    
		if( errorCode == LDAP_SERVER_DOWN || 
			errorCode == LDAP_UNAVAILABLE ||
			errorCode == LDAP_NO_MEMORY ||
			errorCode == LDAP_BUSY ||
			errorCode == LDAP_LOCAL_ERROR ||
			errorCode == LDAP_TIMEOUT ||
			errorCode == LDAP_OPERATIONS_ERROR ||
			errorCode == LDAP_TIMELIMIT_EXCEEDED)
		{
			cout << "[ERROR][" << errorCode << "]: ldap_search_s failed. " << ldap_err2string(errorCode) << endl;
			if(pLdapConnection)
			{
				if(ldap_unbind_s(pLdapConnection)==ERROR_SUCCESS)
				{
					cout << "Cleaning up current connection...\n";
					pLdapConnection = NULL;
				}
			}
			if(pSearchResult)
			{
				ldap_msgfree(pSearchResult);
				pSearchResult = NULL;
			}

     		if( (pLdapConnection = getLdapConnectionCustom(configurationDN, ulSearchSizeLimit, ltConnectionTimeout)) == NULL )
			{
				bConnectErr = true;
				break;	
			}
		}
		else
		{
			//DebugPrintf(SV_LOG_FATAL, "ldap_search_s failed with: 0x%0lx \n",errorCode);
			cout << "[FATAL ERROR][:" << errorCode << "]: ldap_search_s failed. " << ldap_err2string(errorCode) << endl;
			if(pSearchResult != NULL)
			{
				ldap_msgfree(pSearchResult);
				pSearchResult = NULL;						
			}
			bSearchErr = true;
			break;
		}
	}while(1);

	if( bConnectErr )
	{
		cout << "[INFO]: Maximum connection attempts " << MAX_LDAP_CONNECT_ATTEMPTS << " has been reached...\n";
		cout << "[ERROR]: Unable to connect to directory server." << endl;
		return SVE_FAIL;
	}

	if( bSearchErr )
	{
		cout << "[ERROR] Error encountered while searching directory server.\n";
		return SVE_FAIL;
	}
    
    ULONG numberOfEntries;
    numberOfEntries = ldap_count_entries(pLdapConnection, pSearchResult);
    if(numberOfEntries == NULL)
    {
	    DebugPrintf(SV_LOG_WARNING, "ldap_count_entries failed with 0x%0lx \n", errorCode);
	    if(pSearchResult != NULL) 
	    {
		    ldap_msgfree(pSearchResult);
			pSearchResult = NULL;
		    return SVE_FAIL;
	    }
    }

    LDAPMessage* pLdapMessageEntry = NULL;

    while(1)
    {
        if(pLdapMessageEntry == NULL)
            pLdapMessageEntry = ldap_first_entry(pLdapConnection, pSearchResult);
        else
            pLdapMessageEntry = ldap_next_entry(pLdapConnection, pLdapMessageEntry);

        if(pLdapMessageEntry == NULL)
		{
            if( pSearchResult != NULL )
			{
				ldap_msgfree(pSearchResult);
				pSearchResult = NULL;
			}
            break;
		}

        PCHAR pEntryDN = ldap_get_dn(pLdapConnection, pLdapMessageEntry);
	    if( pEntryDN == NULL )
	    {
            cout<<" DN is null\n";
			if( pSearchResult != NULL )
			{
				ldap_msgfree(pSearchResult);
				pSearchResult = NULL;
			}			
		    return SVE_FAIL;
		}
		//else
		//	cout << "\nDN = " << pEntryDN << endl << endl;

		string attrValue;
		BerElement* pBerEncoding = NULL;
		PCHAR pAttribute = NULL;
		PCHAR* ppValue = NULL;

		// get the value of the requested attribute
		for ( pAttribute = ldap_first_attribute( pLdapConnection, pLdapMessageEntry, &pBerEncoding );
				pAttribute != NULL; pAttribute = ldap_next_attribute( pLdapConnection, pLdapMessageEntry, pBerEncoding ) ) 
		{
			// Print each value of the attribute.
			if ((ppValue = ldap_get_values( pLdapConnection, pLdapMessageEntry, pAttribute)) != NULL ) 
			{
				for ( int i = 0; ppValue[i] != NULL; i++ ) 
				{
					// save the attribute value
					attrValue = string(ppValue[i]);
				}
				// Free memory allocated by ldap_get_values(). 
				ldap_value_free( ppValue );
			}
    
			// Free memory allocated by ldap_first_attribute(). 
			ldap_memfree( pAttribute );
		}

		if(attrValue.empty())
		{
			continue;
		}

		cout << "*****Processing RUS DN " << pEntryDN << std::endl;
		//cout << "*****Attribute value = " << attrValue.c_str() << std::endl;

		string replacementAttrValue;
		ULONG numReplaced;
		switchCNValues(srcExchServerName, tgtExchServerName, attrValue.c_str(), replacementAttrValue,
											numReplaced, CN_SEPARATOR_COMMA);
		//cout << "*****New Attribute value = " << replacementAttrValue.c_str() << std::endl;
		// modify the RUS attribute to point to target exchange server
		LDAPMod modSourceAttribute;
		LDAPMod* changedAttributes[] = {&modSourceAttribute, NULL};
		char *replacementValues[2];

		// Set up the attributes to be modified
		modSourceAttribute.mod_op = LDAP_MOD_REPLACE;
		modSourceAttribute.mod_type = (PCHAR)MS_EXCH_ADDRESS_LIST_SERVICE_LINK.c_str(); 
    
		replacementValues[0] = (PCHAR)replacementAttrValue.c_str();
		replacementValues[1] = NULL;

	    modSourceAttribute.mod_vals.modv_strvals = replacementValues;
		errorCode = ldap_modify_s(pLdapConnection, pEntryDN, changedAttributes);
		if (errorCode != LDAP_SUCCESS) {
			cout << "\nERROR: ldap_modify_s failed while modifying RUS entry = " << pEntryDN << std::endl;
			return SVE_FAIL;;
		}		
    }
	if( pLdapConnection )
	{
        ldap_unbind_s(pLdapConnection);
		pLdapConnection = NULL;
	}
	return SVS_OK;
}

SVERROR 
ADInterface::modifyServicePrincipalName(const string& srcExchServerName, const string& tgtExchServerName, bool bRetainDuplicateSPN, const string& srcIP, const string& appName)
{
	if(strcmpi(srcExchServerName.c_str(), tgtExchServerName.c_str()) == 0)
	{
		//source server update case
		cout << "[INFO]: Skipping update of SPN entries as source & target servers specified are same.\n";
		return SVS_OK;
	}
	if( bRetainDuplicateSPN )
	{
		cout << "Copying the Exchange SPN entries from " << srcExchServerName.c_str() << " account to " << tgtExchServerName.c_str() << endl;
		
		TCHAR buffer[256] = TEXT("");
		TCHAR szDescription[32] = TEXT("DNS fully-qualified");
		DWORD dwSize = sizeof(buffer);
		
		// COMPUTER_NAME_FORMAT of enum NameType "ComputerNameDnsFullyQualified" 
		// The fully-qualified DNS name that uniquely identifies the local computer or 
		// the cluster associated with the local computer. 

		if (!GetComputerNameEx((COMPUTER_NAME_FORMAT)3, buffer, &dwSize))
		{
			_tprintf(TEXT("GetComputerNameEx failed (%d)\n"), GetLastError());
			return 0;
		}
		//else 
		//	_tprintf(TEXT("%s: %s\n"), szDescription, buffer);
			
		string hostName = string(buffer);
		size_t firstDot = 0;		
		size_t foundDot = 0;		
		string domainName;
		string domainDN;		

		if( (firstDot = hostName.find_first_of('.', 0)) != string::npos )
		{
			domainName.assign(hostName.c_str(), firstDot+1, hostName.length()-1-firstDot);			
			domainDN = "DC=" + domainName;
			while ( (foundDot = domainDN.find_first_of('.', 0)) != string::npos )			
				domainDN.replace(foundDot, 1, ",DC=");
			hostName.assign(hostName.c_str(), firstDot);				
		//	cout << "hostName=" << hostName.c_str() << endl;
		//	cout << "domainName =" << domainName.c_str() << endl;
		//	cout << "domainDN =" << domainDN.c_str() << endl;
		}			
		dwSize = sizeof(buffer);

		// get an Ldap connection
		LDAP* pLdapConnection = getLdapConnection();
		if (pLdapConnection == NULL)
		{
			cout << "Failed to connection to the ldap server\n";
			return SVE_FAIL;
		}

		// ldap bind to the domain DN
		ULONG errorCode = LDAP_SUCCESS;
		ULONG lRtn = 0;
		lRtn = ldap_bind_s(pLdapConnection,
					(const PCHAR) domainDN.c_str(),				// Use the naming context from the input list
					NULL,										// if it is NULL it will use current user or service credentials 
					LDAP_AUTH_NEGOTIATE);						// LDAP authentication mode
		if(lRtn != LDAP_SUCCESS)
		{
			cout << "ldap_bind_s failed with:" << lRtn << std::endl;
			return SVE_FAIL;
		}

		// search source and target computers
		LDAPMessage* pSearchResult;
		PCHAR pSearchFilter = "(objectClass=Computer)";
		PCHAR pAttributes[2];
		pAttributes[0] = (PCHAR)EXCHANGE_SERVICE_PRINCIPAL_NAME;
		pAttributes[1] = NULL;

		ULONG ulSearchSizeLimit = 0;	// Limit number of users fetched at a time
		LDAP_TIMEVAL ltConnectionTimeout;
		ltConnectionTimeout.tv_sec = 0;		//CONNECTION_ESTABLISHMENT_TIMEOUT;	
		bool bSearchErr = false, bConnectErr = false;

		do
		{
			errorCode = ldap_search_s(pLdapConnection,
							(const PCHAR) domainDN.c_str(),			// DN to start search
							LDAP_SCOPE_SUBTREE,						 // Indicate the search scope
							pSearchFilter,								// Search Filter
							pAttributes,							// Retrieve list of attributes
							0,										//retrieve both attributesand values
							&pSearchResult);						// this is out parameter - Search results
			if( errorCode == LDAP_SUCCESS )
				break;    
		    
			if( errorCode == LDAP_SERVER_DOWN || 
				errorCode == LDAP_UNAVAILABLE ||
				errorCode == LDAP_NO_MEMORY ||
				errorCode == LDAP_BUSY ||
				errorCode == LDAP_LOCAL_ERROR ||
				errorCode == LDAP_TIMEOUT ||
				errorCode == LDAP_OPERATIONS_ERROR ||
				errorCode == LDAP_TIMELIMIT_EXCEEDED)
			{
				cout << "[ERROR][" << errorCode << "]: ldap_search_s failed. " << ldap_err2string(errorCode) << endl;
				if(pLdapConnection)
				{
					if(ldap_unbind_s(pLdapConnection)==ERROR_SUCCESS)
					{
						cout << "Cleaning up current connection...\n";
						pLdapConnection = NULL;
					}
				}
				if(pSearchResult)
				{
					ldap_msgfree(pSearchResult);
					pSearchResult = NULL;
				}

     			if( (pLdapConnection = getLdapConnectionCustom(domainDN, ulSearchSizeLimit, ltConnectionTimeout)) == NULL )
				{
					bConnectErr = true;
					break;	
				}
			}
			else
			{
				//DebugPrintf(SV_LOG_FATAL, "ldap_search_s failed with: 0x%0lx \n",errorCode);
				cout << "[FATAL ERROR][:" << errorCode << "]: ldap_search_s failed. " << ldap_err2string(errorCode) << endl;
				if(pSearchResult != NULL)
				{
					ldap_msgfree(pSearchResult);
					pSearchResult = NULL;						
				}
				bSearchErr = true;
				break;
			}
		}while(1);

		if( bConnectErr )
		{
			cout << "[INFO]: Maximum connection attempts " << MAX_LDAP_CONNECT_ATTEMPTS << " has been reached...\n";
			cout << "[ERROR]: Unable to connect to directory server." << endl;
			return SVE_FAIL;
		}

		if( bSearchErr )
		{
			cout << "[ERROR] Error encountered while searching directory server.\n";
			return SVE_FAIL;
		}
	    
		ULONG numberOfEntries;
		numberOfEntries = ldap_count_entries(pLdapConnection, pSearchResult);
		if(numberOfEntries == NULL)
		{
			DebugPrintf(SV_LOG_WARNING, "ldap_count_entries failed with 0x%0lx \n", errorCode);
			if(pSearchResult != NULL) 
			{
				ldap_msgfree(pSearchResult);
				pSearchResult = NULL;
				return SVE_FAIL;
			}
		}

		LDAPMessage* pLdapMessageEntry = NULL;
		string searchFor;

		while(1)
		{
			if(pLdapMessageEntry == NULL)
				pLdapMessageEntry = ldap_first_entry(pLdapConnection, pSearchResult);
			else
				pLdapMessageEntry = ldap_next_entry(pLdapConnection, pLdapMessageEntry);

			if(pLdapMessageEntry == NULL)
				break;

			string entryDN = ldap_get_dn(pLdapConnection, pLdapMessageEntry);
			string exchServerCNLowerCase = convertoToLower(tgtExchServerName, tgtExchServerName.length());
			string entryDNLowerCase = convertoToLower(entryDN, entryDN.length());
			//cout <<"entryDNLowerCase=" << entryDNLowerCase.c_str() << endl;
			//cout <<"exchServerCNLowerCase=" << exchServerCNLowerCase.c_str() << endl;
			
			searchFor  = string("cn=") + exchServerCNLowerCase;
			searchFor += ",";

			if(strstr(entryDNLowerCase.c_str(), searchFor.c_str()) == NULL)
			{
				// move to the next entry
				searchFor.clear();
				continue;
			}

			string attrValue;
			BerElement* pBerEncoding = NULL;
			PCHAR pAttribute = NULL;
			PCHAR* ppValue = NULL;

			cout << "\n\n*****Processing Computer DN " << entryDN.c_str() << std::endl;

			string fqn_tgt_hostName = tgtExchServerName + string(".") + domainName;
			string fqn_src_hostName = srcExchServerName + string(".") + domainName;
			LDAPMod modSourceAttribute;
			LDAPMod* conf[] = {&modSourceAttribute, NULL};
			char *data[19];
			for (int k=0; k < 19; k++)
			{
				data[k] = NULL;
			}
			string value;
			int j=0;
			list<string> attrValues;

			// find out all the values for servicePrincipalAttribute
			for ( pAttribute = ldap_first_attribute( pLdapConnection, pLdapMessageEntry, &pBerEncoding );
					pAttribute != NULL; pAttribute = ldap_next_attribute( pLdapConnection, pLdapMessageEntry, pBerEncoding ) ) 
			{
	      
				// Print each value of the attribute.
				if ((ppValue = ldap_get_values( pLdapConnection, pLdapMessageEntry, pAttribute)) != NULL ) 
				{
					for ( int i = 0; ppValue[i] != NULL; i++ ) 
					{
						// save the values
						attrValues.push_back(string(ppValue[i]));
					}
					// Free memory allocated by ldap_get_values(). 
					ldap_value_free( ppValue );
				}
	    
				// Free memory allocated by ldap_first_attribute(). 
				ldap_memfree( pAttribute );
			}

			// Set up the values to be added
			modSourceAttribute.mod_op = LDAP_MOD_ADD;
			modSourceAttribute.mod_type = (PCHAR)EXCHANGE_SERVICE_PRINCIPAL_NAME;
			int maxServices = 3;
			if(0 == appName.compare(("Exchange2010")))
			{
				maxServices = 5;
			}
			
            size_t datalen;
			for (int i=0; i < maxServices; i++)
			{
				value = string(exchValues[i]) + "/" + tgtExchServerName;
				if (exists(value, attrValues) == false) {
                    INM_SAFE_ARITHMETIC(datalen = InmSafeInt<size_t>::Type(sizeof(char)) * value.length() + 1, INMAGE_EX(value.length()))
					data[j] = (char *)malloc(datalen);
                    inm_strcpy_s(data[j++], datalen, value.c_str());
				}
				value = string(exchValues[i]) + "/" + fqn_tgt_hostName;
				if (exists(value, attrValues) == false) {
                    INM_SAFE_ARITHMETIC(datalen = InmSafeInt<size_t>::Type(sizeof(char)) * value.length() + 1, INMAGE_EX(value.length()))
					data[j] = (char *)malloc(datalen);
                    inm_strcpy_s(data[j++], datalen, value.c_str());
				}
				value = string(exchValues[i]) + "/" + srcExchServerName;
				if (exists(value, attrValues) == false) {
                    INM_SAFE_ARITHMETIC(datalen = InmSafeInt<size_t>::Type(sizeof(char)) * value.length() + 1, INMAGE_EX(value.length()))
					data[j] = (char *)malloc(datalen);
                    inm_strcpy_s(data[j++], datalen, value.c_str());
				}
				value = string(exchValues[i]) + "/" + fqn_src_hostName;
				if (exists(value, attrValues) == false) {
                    INM_SAFE_ARITHMETIC(datalen = InmSafeInt<size_t>::Type(sizeof(char)) * value.length() + 1, INMAGE_EX(value.length()))
					data[j] = (char *)malloc(datalen);
                    inm_strcpy_s(data[j++], datalen, value.c_str());
				}
			}
			if(0 != appName.compare(("Exchange2010")))
			{
				value = string(EXCHANGE_HTTP) + "/" + fqn_src_hostName;
				if (exists(value, attrValues) == false) {
                        INM_SAFE_ARITHMETIC(datalen = InmSafeInt<size_t>::Type(sizeof(char)) * value.length() + 1, INMAGE_EX(value.length()))
						data[j] = (char *)malloc(datalen);
                        inm_strcpy_s(data[j++], datalen, value.c_str());
				}
				value = string(EXCHANGE_HTTP) + "/" + fqn_tgt_hostName;
				if (exists(value, attrValues) == false) {
                        INM_SAFE_ARITHMETIC(datalen = InmSafeInt<size_t>::Type(sizeof(char)) * value.length() + 1, INMAGE_EX(value.length()))
						data[j] = (char *)malloc(datalen);
                        inm_strcpy_s(data[j++], datalen, value.c_str());
				}
			}
			data[j] = NULL;

			// verify there is data to be added
			if (data[0] != NULL) {
				modSourceAttribute.mod_vals.modv_strvals = data;
				//cout << "Modifying Entry DN " << entryDN.c_str() << std::endl;

				errorCode = ldap_modify_s(pLdapConnection, (PCHAR)(entryDN.c_str()), conf);
				if (errorCode != LDAP_SUCCESS) {
					cout << "ldap_modify_s failed while modifying entry = " << entryDN.c_str() << " errorCode = " << errorCode << std::endl;
					cout << "Failed to set " << EXCHANGE_SERVICE_PRINCIPAL_NAME << " attribute" << std::endl;
				}
				else {
					cout << "Successfully set " << EXCHANGE_SERVICE_PRINCIPAL_NAME << " attribute" << std::endl;
				}
			}

			// release the memory
			for (int k=0; k < 19; k++)
			{
				if (data[k] != NULL)
					free(data[k]);
			}
		}
		if( pLdapConnection )
		{
			ldap_unbind_s(pLdapConnection);
			pLdapConnection = NULL;
		}
		return SVS_OK;
	}
	else
	{
        list<string> srcServer;
		srcServer.push_back(srcExchServerName);		
					
		list<string> tgtServer;
		tgtServer.push_back(tgtExchServerName);

		map< string, list<string> > tgtAccSrcServer;
		tgtAccSrcServer[tgtExchServerName] = srcServer;

		map< string, list<string> > srcAccSrcServer;			
		srcAccSrcServer[srcExchServerName] = srcServer;			

		map< string, list<string> > tgtAccTgtServer;
		tgtAccTgtServer[tgtExchServerName] = tgtServer;

		map< string, list<string> > srcAccTgtServer;
		srcAccTgtServer[srcExchServerName] = tgtServer; 

		string domainName, domainDN, serverDomainName;
        
		if( srcIP.empty() )
		{
			//Failover            

			if( AddSpnEntries(tgtAccSrcServer, domainName, domainDN, serverDomainName) )
			{
				cout << "\nSuccessfully added Exchange SPN entries of " << srcExchServerName.c_str() << " to computer account " << tgtExchServerName.c_str() << endl;
				if( DeleteSpnEntries(srcAccSrcServer, domainName, domainDN, serverDomainName) ) 
					cout << "\nSuccessfully deleted Exchange SPN entries of " << srcExchServerName.c_str() << " from computer account " << srcExchServerName.c_str() << endl;							
				else
					cout << "\nFailed to delete Exchange SPN entries of " << srcExchServerName.c_str() << " from computer account " << srcExchServerName.c_str() << endl;							
			}			
			else
			{
				cout << "\nFailed to add Exchange SPN entries of " << srcExchServerName.c_str() << " to computer account " << tgtExchServerName.c_str() << endl;
			}
		}
		else
		{
			//Failback
				
			if ( AddSpnEntries(tgtAccTgtServer, domainName, domainDN, serverDomainName) )
			{
				cout << "\nSuccessfully added Exchange SPN entries of " << tgtExchServerName.c_str() << " to computer account " << tgtExchServerName.c_str() << endl;
				if( DeleteSpnEntries(srcAccTgtServer, domainName, domainDN, serverDomainName) )
					cout << "\nSuccessfully deleted Exchange SPN entries of " << tgtExchServerName.c_str() << " from computer account " << srcExchServerName.c_str() << endl;
				else
					cout << "\nWARNING: Failed to delete Exchange SPN entries of " << tgtExchServerName.c_str() << " from computer account " << srcExchServerName.c_str() << endl;
			}
			else
			{
				cout << "\nWARNING: Failed to add Exchange SPN entries of " << tgtExchServerName.c_str() << " to computer account " << tgtExchServerName.c_str() << endl;			
			}
		}
	}

	return SVS_OK;
}

bool
ADInterface::exists(const string& data, const list<string>& dataList)
{
	list<string>::const_iterator beginIter = dataList.begin();
	list<string>::const_iterator endIter = dataList.end();

	for (;beginIter != endIter; beginIter++)
	{
		if (stricmp((*beginIter).c_str(), data.c_str()) == 0)
		{
			//cout << "****Value already exists " << data.c_str() << std::endl;
			return true;
		}
	}
	return false;

}

string
ADInterface::getAgentInstallPath(void)
{
	if(m_szAgentInstallPath.empty())
	{
		try
		{
			LocalConfigurator config;
			m_szAgentInstallPath = config.getInstallPath();
		}
		catch(...)
		{
			cout << "[ERROR]:Caught exception during instantiation of LocalConfiguration. Failed to retrieve agent install path.\n";
		}
	}
	return m_szAgentInstallPath;
}

SVERROR
ADInterface::getAttrValueList(const string& exchangeServerName,
									list<string>& attrValueList,
									const string& attrName,
									const string& filter,
									const string& baseDN)
{
	// find configuration Naming Context DN
	string configurationDN;
	if (getConfigurationDN(configurationDN) == SVE_FAIL) 
	{
		cout << "Configuration Naming Context not found\n";
		return SVE_FAIL;
	}

	// get an Ldap connection
	LDAP* pLdapConnection = getLdapConnection();
	if (pLdapConnection == NULL) 
	{
		cout << "Failed to connection to the ldap server\n";
		return SVE_FAIL;
	}

	// ldap bind
	ULONG errorCode = LDAP_SUCCESS;
	ULONG lRtn = 0;
	lRtn = ldap_bind_s(
				pLdapConnection,							// Session Handle
				(const PCHAR) configurationDN.c_str(),		// Use the naming context from the input list
				NULL,										// if it is NULL it will use current user or service credentials 
				LDAP_AUTH_NEGOTIATE);						// LDAP authentication mode
	if(lRtn != LDAP_SUCCESS)
	{
		cout << "ldap_bind_s failed with:" << lRtn << std::endl;
		return SVE_FAIL;
	}

	// Get the CN of the Storage Groups. NOTE: the object class for storage group is "msExchStorageGroupd"
	PCHAR pAttributes[2];
	pAttributes[0] = (PCHAR)(attrName.c_str());
	pAttributes[1] = NULL;
	LDAPMessage* pSearchResult;

	if (baseDN.empty() == false)
		configurationDN = baseDN;


	ULONG ulSearchSizeLimit = 0;	// Limit number of users fetched at a time
	LDAP_TIMEVAL ltConnectionTimeout;
	ltConnectionTimeout.tv_sec = 0;		//CONNECTION_ESTABLISHMENT_TIMEOUT;	
	bool bSearchErr = false, bConnectErr = false;

	do
	{
		errorCode = ldap_search_s(pLdapConnection,	
					(const PCHAR) configurationDN.c_str(),	// DN to start search
					LDAP_SCOPE_SUBTREE,						 // Indicate the search scope
					(const PCHAR) filter.c_str(),			// Search Filter
					pAttributes,							// Retrieve list of attributes
					0,										//retrieve both attributesand values
					&pSearchResult);						// this is out parameter - Search results

		if( errorCode == LDAP_SUCCESS )
			break;    
	    
		if( errorCode == LDAP_SERVER_DOWN || 
			errorCode == LDAP_UNAVAILABLE ||
			errorCode == LDAP_NO_MEMORY ||
			errorCode == LDAP_BUSY ||
			errorCode == LDAP_LOCAL_ERROR ||
			errorCode == LDAP_TIMEOUT ||
			errorCode == LDAP_OPERATIONS_ERROR ||
			errorCode == LDAP_TIMELIMIT_EXCEEDED)
		{
			cout << "[ERROR][" << errorCode << "]: ldap_search_s failed. " << ldap_err2string(errorCode) << endl;
			if(pLdapConnection)
			{
				if(ldap_unbind_s(pLdapConnection)==ERROR_SUCCESS)
				{
					cout << "Cleaning up current connection...\n";
					pLdapConnection = NULL;
				}
			}
			if(pSearchResult)
			{
				ldap_msgfree(pSearchResult);
				pSearchResult = NULL;
			}

     		if( (pLdapConnection = getLdapConnectionCustom(configurationDN, ulSearchSizeLimit, ltConnectionTimeout)) == NULL )
			{
				bConnectErr = true;
				break;	
			}
		}
		else
		{
			//DebugPrintf(SV_LOG_FATAL, "ldap_search_s failed with: 0x%0lx \n",errorCode);
			cout << "[FATAL ERROR][:" << errorCode << "]: ldap_search_s failed. " << ldap_err2string(errorCode) << endl;
			if(pSearchResult != NULL)
			{
				ldap_msgfree(pSearchResult);
				pSearchResult = NULL;						
			}
			bSearchErr = true;
			break;
		}
	}while(1);

	if( bConnectErr )
	{
		cout << "[INFO]: Maximum connection attempts " << MAX_LDAP_CONNECT_ATTEMPTS << " has been reached...\n";
		cout << "[ERROR]: Unable to connect to directory server." << endl;
		return SVE_FAIL;
	}

	if( bSearchErr )
	{
		cout << "[ERROR] Error encountered while searching directory server.\n";
		return SVE_FAIL;
	}

	
	ULONG numberOfEntries = ldap_count_entries(pLdapConnection, pSearchResult);

	// Check if there are any storage groups configured at all
	if(numberOfEntries == NULL)
	{
		//cout << "No Entries Found\n";
	    if(pSearchResult != NULL) 
	    {
		    ldap_msgfree(pSearchResult);
			pSearchResult = NULL;
	    }
		return SVS_OK;
	}
	else
	{
		//cout << numberOfEntries << " found\n";
	}


	LDAPMessage* pLdapMessageEntry = NULL;
	PCHAR pAttribute = NULL;
	BerElement* pBerEncoding = NULL;
	PCHAR* ppValue = NULL;
	ULONG iValue = 0;
	string entryDN;
	string entryDNLowerCase;

	string exchServerCN = "CN=";
	exchServerCN += exchangeServerName;
	string exchServerCNLowerCase = convertoToLower(exchServerCN, exchServerCN.length());

	string searchFor;
	// iterate through all storage groups
	for (int index=0; index < (int)numberOfEntries; index++)
	{
	    if(pLdapMessageEntry == NULL)
	        pLdapMessageEntry = ldap_first_entry(pLdapConnection, pSearchResult);
	    else
	        pLdapMessageEntry = ldap_next_entry(pLdapConnection, pLdapMessageEntry);

	    if(pLdapMessageEntry == NULL)
	        break;

		// get DN
		entryDN = ldap_get_dn(pLdapConnection, pLdapMessageEntry);
		entryDNLowerCase = convertoToLower(entryDN, entryDN.length());

		//cout <<"entryDNLowerCase=" << entryDNLowerCase.c_str() << endl;
		//cout <<"exchServerCNLowerCase=" << exchServerCNLowerCase.c_str() << endl;
		// is this entry part of the concerned exchange server
		
		searchFor  = exchServerCNLowerCase;
		searchFor += ",cn=servers";

        if(strstr(entryDNLowerCase.c_str(), searchFor.c_str()) == NULL)
		{
			searchFor.clear();
			// move to the next entry
			continue;
		}

		// iterate through all requested attributes on a storage group. Currently it's just "cn"
		for ( pAttribute = ldap_first_attribute( pLdapConnection, pLdapMessageEntry, &pBerEncoding );
				pAttribute != NULL; pAttribute = ldap_next_attribute( pLdapConnection, pLdapMessageEntry, pBerEncoding ) ) 
		{
      
			// Print each value of the attribute.
			if ((ppValue = ldap_get_values( pLdapConnection, pLdapMessageEntry, pAttribute)) != NULL ) 
			{
				for ( int i = 0; ppValue[i] != NULL; i++ ) 
				{
					// save the CN and the DN
					attrValueList.push_back(string(ppValue[i]));
					//cout << "Exchange Server Name = " << exchangeServerName << " Log File Path/Database Path = " 
						//<< ppValue[i] << std::endl;
				}
				// Free memory allocated by ldap_get_values(). 
				ldap_value_free( ppValue );
			}
    
			// Free memory allocated by ldap_first_attribute(). 
			ldap_memfree( pAttribute );
		}
	}

	// Free memory allocated by ldap_search_ext_s().
	if( pSearchResult != NULL )
	{
        ldap_msgfree( pSearchResult );
		pSearchResult = NULL;
	}
	if( pLdapConnection )
	{
        ldap_unbind_s(pLdapConnection);
		pLdapConnection = NULL;
	}
	return SVS_OK;
}

SVERROR
ADInterface::getAttrValue(string& attrValue, const string& attrName, const string& filter,
							const string& baseDN)
{
	// find configuration Naming Context DN
	string configurationDN;
	if (getConfigurationDN(configurationDN) == SVE_FAIL) 
	{
		cout << "Configuration Naming Context not found\n";
		return SVE_FAIL;
	}

	// get an Ldap connection
	LDAP* pLdapConnection = getLdapConnection();
	if (pLdapConnection == NULL) 
	{
		cout << "Failed to connection to the ldap server\n";
		return SVE_FAIL;
	}

	// ldap bind
	ULONG errorCode = LDAP_SUCCESS;
	ULONG lRtn = 0;
	lRtn = ldap_bind_s(pLdapConnection,							
				(const PCHAR) configurationDN.c_str(),		// Use the naming context from the input list
				NULL,										// if it is NULL it will use current user or service credentials 
				LDAP_AUTH_NEGOTIATE);						// LDAP authentication mode
	if(lRtn != LDAP_SUCCESS)
	{
		cout << "ldap_bind_s failed with:" << lRtn << std::endl;
		return SVE_FAIL;
	}

	// Get the CN of the Storage Groups. NOTE: the object class for storage group is "msExchStorageGroupd"
	PCHAR pAttributes[2];
	pAttributes[0] = (PCHAR)(attrName.c_str());
	pAttributes[1] = NULL;
	LDAPMessage* pSearchResult;

	if (baseDN.empty() == false)
		configurationDN = baseDN;
	
	ULONG ulSearchSizeLimit = 0;	// Limit number of users fetched at a time
	LDAP_TIMEVAL ltConnectionTimeout;
	ltConnectionTimeout.tv_sec = 0;		//CONNECTION_ESTABLISHMENT_TIMEOUT;	
	bool bSearchErr = false, bConnectErr = false;

	do
	{
		errorCode = ldap_search_s(pLdapConnection,						
				(const PCHAR) configurationDN.c_str(),	// DN to start search
				LDAP_SCOPE_SUBTREE,						 // Indicate the search scope
				(const PCHAR) filter.c_str(),			// Search Filter
				pAttributes,							// Retrieve list of attributes
				0,										//retrieve both attributesand values
				&pSearchResult);						// this is out parameter - Search results
		if( errorCode == LDAP_SUCCESS )
			break;    
	    
		if( errorCode == LDAP_SERVER_DOWN || 
			errorCode == LDAP_UNAVAILABLE ||
			errorCode == LDAP_NO_MEMORY ||
			errorCode == LDAP_BUSY ||
			errorCode == LDAP_LOCAL_ERROR ||
			errorCode == LDAP_TIMEOUT ||
			errorCode == LDAP_OPERATIONS_ERROR ||
			errorCode == LDAP_TIMELIMIT_EXCEEDED)
		{
			cout << "[ERROR][" << errorCode << "]: ldap_search_s failed. " << ldap_err2string(errorCode) << endl;
			if(pLdapConnection)
			{
				if(ldap_unbind_s(pLdapConnection)==ERROR_SUCCESS)
				{
					cout << "Cleaning up current connection...\n";
					pLdapConnection = NULL;
				}
			}
			if(pSearchResult)
			{
				ldap_msgfree(pSearchResult);
				pSearchResult = NULL;
			}

     		if( (pLdapConnection = getLdapConnectionCustom(configurationDN, ulSearchSizeLimit, ltConnectionTimeout)) == NULL )
			{
				bConnectErr = true;
				break;	
			}
		}
		else
		{
			//DebugPrintf(SV_LOG_FATAL, "ldap_search_s failed with: 0x%0lx \n",errorCode);
			cout << "[FATAL ERROR][:" << errorCode << "]: ldap_search_s failed. " << ldap_err2string(errorCode) << endl;
			if(pSearchResult != NULL)
			{
				ldap_msgfree(pSearchResult);
				pSearchResult = NULL;						
			}
			bSearchErr = true;
			break;
		}
	}while(1);

	if( bConnectErr )
	{
		cout << "[INFO]: Maximum connection attempts " << MAX_LDAP_CONNECT_ATTEMPTS << " has been reached...\n";
		cout << "[ERROR]: Unable to connect to directory server." << endl;
		return SVE_FAIL;
	}

	if( bSearchErr )
	{
		cout << "[ERROR] Error encountered while searching directory server.\n";
		return SVE_FAIL;
	}

	
	ULONG numberOfEntries = ldap_count_entries(pLdapConnection, pSearchResult);

	// Check if there are any storage groups configured at all
	if(numberOfEntries == NULL)
	{
		//cout << "No Entries Found\n";
	    if(pSearchResult != NULL) 
	    {
		    ldap_msgfree(pSearchResult);
			pSearchResult = NULL;
	    }
		return SVS_OK;
	}

	LDAPMessage* pLdapMessageEntry = NULL;
	PCHAR pAttribute = NULL;
	BerElement* pBerEncoding = NULL;
	PCHAR* ppValue = NULL;
	ULONG iValue = 0;
	string entryDN;

	// iterate through all storage groups
	for (int index=0; index < (int)numberOfEntries; index++)
	{
	    if(pLdapMessageEntry == NULL)
	        pLdapMessageEntry = ldap_first_entry(pLdapConnection, pSearchResult);
	    else
	        pLdapMessageEntry = ldap_next_entry(pLdapConnection, pLdapMessageEntry);

	    if(pLdapMessageEntry == NULL)
	        break;

		// get DN
		entryDN = ldap_get_dn(pLdapConnection, pLdapMessageEntry);

		// iterate through all requested attributes on a storage group. Currently it's just "cn"
		for ( pAttribute = ldap_first_attribute( pLdapConnection, pLdapMessageEntry, &pBerEncoding );
				pAttribute != NULL; pAttribute = ldap_next_attribute( pLdapConnection, pLdapMessageEntry, pBerEncoding ) ) 
		{
      
			// Print each value of the attribute.
			if ((ppValue = ldap_get_values( pLdapConnection, pLdapMessageEntry, pAttribute)) != NULL ) 
			{
				for ( int i = 0; ppValue[i] != NULL; i++ ) 
				{
					// save the CN and the DN
					attrValue = string(ppValue[i]);
					//cout << "Exchange Server Name = " << exchangeServerName << " Log File Path/Database Path = " 
						//<< ppValue[i] << std::endl;
				}
				// Free memory allocated by ldap_get_values(). 
				ldap_value_free( ppValue );
			}
    
			// Free memory allocated by ldap_first_attribute(). 
			ldap_memfree( pAttribute );
		}
	}

	// Free memory allocated by ldap_search_ext_s().
	if( pSearchResult != NULL)
	{
        ldap_msgfree( pSearchResult );
		pSearchResult = NULL;
	}
	if( pLdapConnection )
	{
        ldap_unbind_s(pLdapConnection);
		pLdapConnection = NULL;
	}
	return SVS_OK;
}
void
ADInterface::findProtectedStorageGroups(const string& srcServer, const string& tgtServer ,
										const list<string>& sgs, const list<string>& proVolumes, list<string>& proSGs,
										const string &sgDN)
{
	list<string>::const_iterator iter = sgs.begin();
	for (; iter != sgs.end(); iter++) {
		string replacementDN;
		ULONG numReplaced;
		switchCNValues(tgtServer, srcServer, (*iter), replacementDN, numReplaced, CN_SEPARATOR_COMMA);
		cout << "Processing Storage Group : " << replacementDN.c_str() << std::endl;
		if (proVolumes.empty() == false) {
			if (sgDN.empty() == false) {
				if (strcmpi(replacementDN.c_str(), sgDN.c_str()) == 0) {
					if (isStorageGroupProtected(srcServer, replacementDN, proVolumes)) {
						proSGs.push_back(*iter);
					}
				}
				else {
					cout << "Storage group : " << (*iter).c_str() << " is not protected. Skipping\n";
				}
			}
			else {
				if (isStorageGroupProtected(srcServer, replacementDN, proVolumes)) {
					proSGs.push_back(*iter);
				}
			}
		}
		else {	// empty list of protected volumes => everything is protected
			proSGs.push_back(*iter);
		}
	}
}

bool
ADInterface::isStorageGroupProtected(const string& srcHost, const string& sgDN, const list<string>& proVolumes)
{
	string logDirPath;
	char tmpVol[MAX_PATH+1];
//	char drive[MAX_PATH];
	SVERROR rc = getAttrValue(logDirPath, "msExchESEParamLogFilePath", "(objectclass=msExchStorageGroup)", sgDN);
	if (rc == SVE_FAIL) {
		cout << "Problem retrieving Attr Values for Log File Path For storage group : " << sgDN.c_str() << "\n";
		return false;
	}
	// find the Log directory drive letter
	if (logDirPath.empty() == false) 
	{
		list< pair<string, string> > logPathLogVolumeList;
		list< pair<string, string> >::const_iterator logPathVolIter;

		list<string> logPaths;
		string volmnt;
		//string tmpstr = logDirPath.c_str();
		//string abspath;				
		//tmpstr.replace(1, 1, "$");
		//abspath += "\\\\" + srcHost + "\\" + tmpstr ;
		//cout << "abspath=" <<abspath.c_str()<<endl;
		//if(	GetVolumePathName(abspath.c_str(),drive, MAX_PATH) )
		//{
		//	cout << "Unique Volume/MountPoint with data/log file " << abspath.c_str() <<" is " << drive << endl;
		//  volmnt = string(drive);

		//	size_t pos = volmnt.find_first_of("$",0) - 1;
		//	volmnt = volmnt.substr(pos);
		//	volmnt.replace(1, 1, ":");	
		//}
		//else
		//{
		//	cout << "GetVolumePathName failed Error= "<< GetLastError()
		//	//     << "\n\t[Logfile path=]" << drive 
		//		 << "\n\t source Host=" << srcHost.c_str()<< endl;			
		
		logPaths.push_back(logDirPath);            
		loadLogConfiguration(srcHost, logPaths, logPathLogVolumeList);
		if( !logPathLogVolumeList.empty() )
		{
            logPathVolIter = logPathLogVolumeList.begin();
			volmnt = logPathVolIter->second;
		}		
		try
		{			
			inm_strcpy_s(tmpVol,ARRAYSIZE(tmpVol),volmnt.c_str());
			int len = strlen(tmpVol);
			if(tmpVol[len-1] == '\\')
				tmpVol[len-1] = '\0';
		}
		catch(...)
		{
			cout << "Could not retrieve volume/mountpoint corresponding to the log path " << logDirPath.c_str() << endl;
		}
	}

	// determine if the log drive is protected
	list<string>::const_iterator iter = proVolumes.begin();
	for (; iter != proVolumes.end(); iter++) {
		if (strcmpi((*iter).c_str(), tmpVol) == 0) {
			cout << "\nStorage Group : " << sgDN.c_str() << " is Protected\n";
			return true;
		}
	}

	cout << "Storage Group : " << sgDN.c_str() << " is NOT Protected\n";
	return false;
}

SVERROR 
ADInterface::loadLogConfiguration(const string& hostName, list<string>& logPaths, list< pair<string,string> >& logPathVolumePairs)
{
	SVERROR result = SVS_OK;
	// Get Vx agent install location
	ADInterface ad;
	string installLocation = ad.getAgentInstallPath();
	if (installLocation.empty()) {
		cout << "Could not determine the install location for the VX agent. Failover/Failback cannot continue\n";
		return SVE_FAIL;
	}
	// open the file for reading the Exchange information for the host
	FILE* pFile = NULL;
	string fileName, alternateFileName;
	alternateFileName = installLocation + string("\\") + FAILOVER_DIRECTORY + string("\\");
	fileName = alternateFileName + DATA_DIRECTORY + string("\\");	

	// replace / with _ since clustered Exchange server names are in the form "<CLUSTERNAME\SERVERINSTANCENAME>".
	// This is required because the discoverd configuration for a Exchange server is saved in a file named as the Exchange server name	
	char normServerName[512];
	memset(normServerName, 0, 511);
	inm_strncpy_s(normServerName,ARRAYSIZE(normServerName), hostName.c_str(), hostName.length());
	size_t index = 0;
	while (normServerName[index] != NULL) {
		if (normServerName[index] == '\\')
			normServerName[index] = '_';
		index++;
	}
	//cout << "Normalized server name is : " << normServerName << std::endl;
	fileName += string(normServerName) + EXCHANGE_LOG_CONFIG_FILE;
	alternateFileName += string(normServerName) + EXCHANGE_LOG_CONFIG_FILE;


	if( discoverFrom == READ_TRANSMITTED_FILE )
	{
        pFile = fopen(fileName.c_str(), "r");

		cout << "\n***** Attempting to read the configuration file " << fileName.c_str() << " for Exchange log configuration\n";

		if (pFile == NULL) {
			cout << "\n>>>>>> Error Opening file " << fileName.c_str() << " for reading Exchange log configuration\n\n";
			cout << "\n***** Attempting to read configuration from alternate path " << alternateFileName.c_str() << "\n";

			pFile = fopen(alternateFileName.c_str(), "r");
			if (pFile == NULL)
			{
				cout << "\n>>>>>> Error Opening file " << alternateFileName.c_str() << " for reading Exchange log configuration\n\n";
				return SVE_FAIL;
			}
		}
	}
	else
	{
			cout << "\n***** Attempting to read the configuration file " << alternateFileName.c_str() << "\n";
			pFile = fopen(alternateFileName.c_str(), "r");
			if (pFile == NULL)
			{
				cout << "\n>>>>>> Error Opening file " << alternateFileName.c_str() << " for reading Exchange log configuration\n\n";
				return SVE_FAIL;
			}
	}

	char buffer[512];
	char temp[512];
	list<string>::const_iterator logPathIter;
	// read the contents of the file and initialize the in-memory database structure
	while (!feof(pFile)) {
		if (fgets(buffer, 511, pFile)) {
			// tokenize the data read
			char *pToken = strtok(buffer, "\t\n");
			logPathIter = logPaths.begin();
			for(;(logPathIter != logPaths.end()) && (NULL != pToken); logPathIter++)
			{
				if (strcmpi(pToken, (*logPathIter).c_str()) == 0) 
					if ( (pToken = strtok(NULL, "\t")) != NULL)
					{
						inm_strcpy_s(temp,ARRAYSIZE(temp), pToken);	
						logPathVolumePairs.push_back(make_pair(*logPathIter,string(temp)));						
						cout << logPathIter->c_str() << "\t" << temp << endl;
					}					
			}
		}
	}
	// close file handle
	if (pFile != NULL)
		fclose(pFile);

	return result;
}

SVERROR 
ADInterface::loadExchConfiguration(const string& hostName, map<string,string>& pathVolumeMap)
{
	SVERROR result = SVS_OK;
	// Get Vx agent install location
	ADInterface ad;
	string installLocation = ad.getAgentInstallPath();
	if (installLocation.empty()) {
		cout << "Could not determine the install location for the VX agent. Failover/Failback cannot continue\n";
		return SVE_FAIL;
	}
	// open the file for reading the Exchange information for the host
	FILE* pFile = NULL;
	string fileName;
	fileName = installLocation + string("\\") + FAILOVER_DIRECTORY + string("\\");// + DATA_DIRECTORY + string("\\");

	// replace / with _ since clustered Exchange server names are in the form "<CLUSTERNAME\SERVERINSTANCENAME>".
	// This is required because the discoverd configuration for a Exchange server is saved in a file named as the Exchange server name	
	char normServerName[512];
	memset(normServerName, 0, 511);
	inm_strncpy_s(normServerName, ARRAYSIZE(normServerName),hostName.c_str(), hostName.length());
	size_t index = 0;
	while (normServerName[index] != NULL) {
		if (normServerName[index] == '\\')
			normServerName[index] = '_';
		index++;
	}
	//cout << "Normalized server name is : " << normServerName << std::endl;
	fileName += string(normServerName) + EXCHANGE_LOG_CONFIG_FILE;

	pFile = fopen(fileName.c_str(), "r");

	cout << "\n***** Attempting to read the configuration file " << fileName.c_str() << " for Exchange log configuration\n";

	if (pFile == NULL) {
		cout << "\n>>>>>> Error Opening file " << fileName.c_str() << " for reading Exchange log configuration\n\n";
		return SVE_FAIL;
	}

	char buffer[512];
	char path[512];
	
	// read the contents of the file and initialize the in-memory database structure
	while (!feof(pFile)) {
		if (fgets(buffer, 511, pFile)) {
			// tokenize the data read
			char *pToken;
            if( (pToken = strtok(buffer, "\t\n")) != NULL) 
			{
				inm_strcpy_s(path,ARRAYSIZE(path), pToken);	
				if( (pToken = strtok(NULL, "\t")) != NULL)
				{				
                    pathVolumeMap[std::string(path)] = std::string(pToken);
				}					
			}
		}
	}
	// close file handle
	if (pFile != NULL)
		fclose(pFile);

	return result;
}

void
ADInterface::findProtectedMailstores(const list<string>& mailstores, const list<string>& proSGs, list<string>& proMailstores)
{
	list<string>::const_iterator iter = mailstores.begin();
	for (; iter != mailstores.end(); iter++) {
		bool isProtected = false;
		list<string>::const_iterator sgIter = proSGs.begin();
		for (; sgIter != proSGs.end(); sgIter++) {
			string msLower = convertoToLower((*iter), (*iter).length());			
			//cout << "Processing Mailstore : " << msLower.c_str() << std::endl;
			string sgLower = convertoToLower((*sgIter), (*sgIter).length());
			//cout << "****** Searching For Storage Group : " << sgLower.c_str() << std::endl;
			if (strstr(msLower.c_str(), sgLower.c_str()) != 0) {
				cout << "\nMailstore : " << (*iter).c_str() << " is Protected\n";
				isProtected = true;
				proMailstores.push_back(*iter);
				break;
			}
		}
		if (isProtected == false) {
			cout << "Mailstore : " << (*iter).c_str() << " is NOT Protected\n";
		}
	}
}

SVERROR
ADInterface::getBaseStorageGroupDN(const string& host, const string &sg, string &baseSGDN)
{
	// get the dn of the requested storage group
	if (sg.empty() == false) {
		list<string> sgDN;
		SVERROR rc = getAttrValueList(host, sgDN, "distinguishedName", "(objectclass=msExchStorageGroup)", "");
		if (rc == SVE_FAIL) {
			cout << "Problem retrieving Distinguished Name for Storage Group\n";
			return rc;
		}
		list<string>::const_iterator sgIter = sgDN.begin();
		bool found = false;
		for (; sgIter != sgDN.end(); sgIter++) {
			string cn;
			getAttrValue(cn, "cn", "(objectclass=msExchStorageGroup)", (*sgIter).c_str());
			//cout << "Searching For Storage Group : " << sg.c_str() << " In CN : " << cn.c_str() << std::endl;
			if (strcmpi(cn.c_str(), sg.c_str()) == 0) {
				found = true;
				baseSGDN = (*sgIter);
				//cout << "Base Search DN : " << baseSGDN.c_str() << std::endl;
				break;
			}
		}

		if (found == false) {
			cout << "Storage Group : " << sg.c_str() << " Not found on Exchange Server : " << host.c_str() << std::endl;
			return SVE_FAIL;
		}
	}
	return SVS_OK;
}

//
// This function returns the virtual server name for a given host
// Input:		hostName	-> Name of the host for which virtual server name is to be found
//              Application: ApplicationName - > Name of the application for which virtual server is to be determined
//
// Output:		virtualServerName	-> Name of the virtual server if the input host name is clustered
// Comments:	virutalServerName is empty for NON clustered hosts
//
SVERROR
ADInterface::queryCXforVirtualServerName(const string& host, const string& application, string &virServerName)
{
    char *pszGetBuffer = NULL;
    char *pszGetURL = NULL;
	const size_t URL_SIZE = 1024;
	LocalConfigurator config;

	pszGetURL = new char[ 1024 ];
	if( NULL == pszGetURL )
	{
	    cout << "Not enough memory\n";
		return SVE_FAIL;
	}

	string appLowerCase = convertoToLower(application, application.length());
	string queryField = appLowerCase + "VirtualServerName";
	// Get 
	inm_strcpy_s( pszGetURL,URL_SIZE, "/get_virtual_server_name.php" );
	inm_strcat_s( pszGetURL,URL_SIZE, "?host=" );
	inm_strcat_s( pszGetURL, URL_SIZE,host.c_str());
	inm_strcat_s( pszGetURL, URL_SIZE,"&fieldName=");
	inm_strcat_s( pszGetURL, URL_SIZE,queryField.c_str());

	if( postToCx(GetCxIpAddress().c_str(), config.getHttp().port, pszGetURL, NULL,&pszGetBuffer,config.IsHttps()).failed() )
	{
		return SVE_FAIL;
	}
		    
	if (pszGetBuffer != NULL) {
		virServerName = pszGetBuffer;
	}

	// free memory
	delete[] pszGetBuffer;
    delete[] pszGetURL;
	return SVS_OK;
}

//This function compare the IP address of source and Target and in return true in bIPSame else false
SVERROR
ADInterface::CompareIPs(string szSource, string szTarget, bool &bSameIP )
{
    DNS_RECORD* pSource = NULL, *pTarget = NULL, *pRedirectedSource = NULL;
	recordType recType = InvalidRecord;
	IP4_ARRAY* ptrIpArray = NULL;
    ULONG ulRet = 0;
	string strIP;
	bool bIsSourceCname = false;
	bSameIP = false;
	WORD type = DNS_TYPE_ANY;
	in_addr inaddr;
	std::string domainName;

    getDomainName(domainName);
	getFqdnName(szSource,domainName);

	ulRet = DnsQuery(
            szSource.c_str(),
            type,
            DNS_QUERY_BYPASS_CACHE,
            NULL,
            &pSource,
            NULL
            );

	if(ulRet != 0)
	{
		cout << "Could not lookup DNS entry for:" << szSource.c_str() << " ErrorCode: " << ulRet<<" [Hexadecimal: "<<std::hex<<ulRet << "]\n";
		goto cleanup_and_exit;
	}
	if(pSource->wType == CNameRecord)
	{
		std::cout<<" Source: "<<szSource <<" cannot be an cname record "<<std::endl;
		bIsSourceCname = true;
	}
	else
	{
		in_addr inaddr;
		inaddr.S_un.S_addr = pSource->Data.A.IpAddress;
		strIP = inet_ntoa(inaddr);
		cout << szSource.c_str() << " IP address = " << strIP.c_str() << "\n";
	}
	//If Source is CNAME,something wrong had happened. Source cant be a CNAME even during failover and failback
	if(!bIsSourceCname)
	{
		getFqdnName(szTarget,domainName);

#ifdef SCOUT_WIN2K_SUPPORT
		ulRet = DnsQuery(szTarget.c_str(),
						type,
						DNS_QUERY_USE_TCP_ONLY,
						NULL,
						&pTarget,
						NULL);

#else
		ulRet = DnsQuery(szTarget.c_str(),
						type,
						DNS_QUERY_WIRE_ONLY,
						NULL,
						&pTarget,
						NULL);
#endif

		if(ulRet != 0)
		{
			cout<<"\nCould not lookup DNS entry for: " << szTarget.c_str() << " \nErrorCode:" << ulRet<<" [Hexadecimal: "<<std::hex<<ulRet <<"]\n";
			goto cleanup_and_exit;
		}
		
		if(pTarget->wType == CNameRecord)
		{
			std::string cnameHost = std::string(pTarget->Data.CNAME.pNameHost);
			std::cout<<"cnameHost = "<<cnameHost<<" szSource = "<<szSource<<std::endl;
			if(strcmpi(cnameHost.c_str() , szSource.c_str()) == 0)
			{
				std::cout<<"Records are pointing to same IP address"<<std::endl;
				bSameIP = true;
			}
			else
			{
				std::cout<<" Although the target machine :"<<cnameHost<<" is a cname record but its not pointing to "<<szSource<<std::endl;
				bSameIP = false;
			}
		}
		else
		{
			inaddr.S_un.S_addr = pTarget->Data.A.IpAddress;
			strIP = inet_ntoa(inaddr);
			cout << szTarget.c_str() << " IP address = " << strIP.c_str() << "\n";
				
			if(pTarget->Data.A.IpAddress == pSource->Data.A.IpAddress)
			{
				bSameIP = true;
			}
			else
			{
				bSameIP =false;
			}
		}
	}
	else
	{
		std::cout<<" Source is found to be CNAME it is unexpected state of Source Machine."<<std::endl;
		ulRet = SVS_FALSE;
	}
cleanup_and_exit:

    if(pSource)
        DnsRecordListFree(pSource, DnsFreeRecordListDeep);

    if(pTarget)
        DnsRecordListFree(pTarget, DnsFreeRecordListDeep);

	return ulRet;
}

//CNAME change ... Source cannot be cname record.if it is, its abnormal condition.
//This function compare the IP address of source and Target and in return true in bIPSame else false
SVERROR
ADInterface::CompareIPs(string szSource, IP4_ADDRESS IPaddress, bool &bIPSame )
{
    DNS_RECORD* pSource = NULL;
    ULONG ulRet = 0;
	bIPSame = true;
	string strIP;

    ulRet = DnsQuery(szSource.c_str(),
                DNS_TYPE_A,
                DNS_QUERY_BYPASS_CACHE,
                NULL,
                &pSource,
                NULL);

    if(ulRet != 0)
    {
        cout << "Could not lookup DNS entry for:" << szSource.c_str() << " ErrorCode: " << ulRet << "\n";
        goto cleanup_and_exit;
    }

	in_addr inaddr;
	inaddr.S_un.S_addr = pSource->Data.A.IpAddress;
	strIP = inet_ntoa(inaddr);
    if(pSource->Data.A.IpAddress == IPaddress)
	{
		bIPSame = true;
	}
	else
	{
		bIPSame =false;
	}

cleanup_and_exit:

    if(pSource)
        DnsRecordListFree(pSource, DnsFreeRecordListDeep);

    return ulRet;
}

BOOL
ADInterface::FlushDnsCache(void)
{
	BOOL bRet = 0;
	ACE_Process_Options cmd_ipconfig_opt;
    cmd_ipconfig_opt.handle_inheritance(false);
	cmd_ipconfig_opt.command_line(CMD_FLUSH_DNS_CACHE);
	
	do {
		ACE_Process_Manager *inm_ace_pm = ACE_Process_Manager::instance();
		if (NULL == inm_ace_pm)
		{
			DebugPrintf(SV_LOG_ERROR, "Could not get ACE Process Manager reference.\n");
			break;
		}

		//Disable STDIN, STDOUT for the child. But retaining stderr for failure information.
		if (cmd_ipconfig_opt.set_handles(ACE_INVALID_HANDLE, ACE_INVALID_HANDLE, ACE_STDERR) != 0)
		{
			DebugPrintf(SV_LOG_WARNING, "Error in setting standard handles to child process.\n");
			//Ignoring this failure as it is not critical.
		}

		pid_t pid_cmd_flushdns = inm_ace_pm->spawn(cmd_ipconfig_opt);
		if (ACE_INVALID_PID == pid_cmd_flushdns)
		{
			DebugPrintf(SV_LOG_ERROR, "Could not run the command: %s. Error %lu\n", CMD_FLUSH_DNS_CACHE, GetLastError());
			break;
		}

		ACE_exitcode cmd_exitcode = 0;
		ACE_Time_Value cmd_timeout(300);             //Cmd timeout: 5mins
		pid_t ret_wait_status = inm_ace_pm->wait(pid_cmd_flushdns, cmd_timeout, &cmd_exitcode);
		
		if (ret_wait_status == pid_cmd_flushdns)     //Child exited
		{
			if (cmd_exitcode)
			{
				DebugPrintf(SV_LOG_ERROR, "Command: %s. Failed with exit code: %lu\n", CMD_FLUSH_DNS_CACHE, cmd_exitcode);
			}
			else
			{
				bRet = 1;                            //Command successful.
			}
			break;
		}
		else if (0 == ret_wait_status)               //Time-out
		{
			DebugPrintf(SV_LOG_ERROR, "Time-out for the command: %s\n", CMD_FLUSH_DNS_CACHE);
		}
		else                                        //ACE_INVALID_PID: Wait error
		{
			DebugPrintf(SV_LOG_ERROR, "Wait error for the command: %s\n", CMD_FLUSH_DNS_CACHE);
		}

		inm_ace_pm->terminate(pid_cmd_flushdns);   //Terminating child process if still running.
		                                           //Ignore terminate return value.
	} while (false);

	cmd_ipconfig_opt.release_handles();

	return bRet;
}
ULONG 
ADInterface::checkADPrivileges(const string szSourceHost)
{
	TCHAR buffer[256] = TEXT("");
	TCHAR szDescription[32] = TEXT("DNS fully-qualified");
	DWORD dwSize = sizeof(buffer);
	
	// COMPUTER_NAME_FORMAT of enum NameType "ComputerNameDnsFullyQualified" 
	// The fully-qualified DNS name that uniquely identifies the local computer or 
	// the cluster associated with the local computer. 

	if (!GetComputerNameEx((COMPUTER_NAME_FORMAT)3, buffer, &dwSize))
	{
		_tprintf(TEXT("GetComputerNameEx failed (%d)\n"), GetLastError());
		return 0;
	}
	//else 
	//	_tprintf(TEXT("%s: %s\n"), szDescription, buffer);
		
	string hostName = string(buffer);
	size_t firstDot = 0;		
	size_t foundDot = 0;		
	string domainName;
	string domainDN;		

	if( (firstDot = hostName.find_first_of('.', 0)) != string::npos )
	{
		domainName.assign(hostName.c_str(), firstDot+1, hostName.length()-1-firstDot);			
		domainDN = "DC=" + domainName;
		while ( (foundDot = domainDN.find_first_of('.', 0)) != string::npos )			
			domainDN.replace(foundDot, 1, ",DC=");
		hostName.assign(hostName.c_str(), firstDot);				
	//	cout << "hostName=" << hostName.c_str() << endl;
	//	cout << "domainName =" << domainName.c_str() << endl;
	//	cout << "domainDN =" << domainDN.c_str() << endl;
	}			
	dwSize = sizeof(buffer);

	// get an Ldap connection
	LDAP* pLdapConnection = getLdapConnection();
	if (pLdapConnection == NULL)
	{
		cout << "Failed to connect to the ldap server\n";		
		return 52; //LDAP_UNAVAILABLE: Indicates that the LDAP server cannot process the client's bind request, 
				   //usually because it is shutting down.
	}

	// ldap bind to the domain DN
	ULONG errorCode = LDAP_SUCCESS;
	ULONG lRtn = 0;
	lRtn = ldap_bind_s(pLdapConnection,							
				(const PCHAR) domainDN.c_str(),				// Use the naming context from the input list
				NULL,										// if it is NULL it will use current user or service credentials 
				LDAP_AUTH_NEGOTIATE);						// LDAP authentication mode
	if(lRtn != LDAP_SUCCESS)
	{
		cout << "ldap_bind_s failed with ERROR:[" << lRtn << "] " << ldap_err2string(lRtn)<< endl;	
		return lRtn;
	}

	// search source and target computers
	LDAPMessage* pSpnList;
	PCHAR pSearchFilter = "(objectClass=Computer)";
	PCHAR pAttributes[2];
	pAttributes[0] = (PCHAR)EXCHANGE_SERVICE_PRINCIPAL_NAME;
	pAttributes[1] = NULL;

	ULONG ulSearchSizeLimit = 0;	// Limit number of users fetched at a time
	LDAP_TIMEVAL ltConnectionTimeout;
	ltConnectionTimeout.tv_sec = 0;		//CONNECTION_ESTABLISHMENT_TIMEOUT;	
	bool bSearchErr = false, bConnectErr = false;

	do
	{
		errorCode = ldap_search_s(pLdapConnection,						// Session handle
						(const PCHAR) domainDN.c_str(),			// DN to start search
						LDAP_SCOPE_SUBTREE,						 // Indicate the search scope
						pSearchFilter,								// Search Filter
						pAttributes,							// Retrieve list of attributes
						0,										//retrieve both attributesand values
						&pSpnList);						// this is out parameter - Search results
		if( errorCode == LDAP_SUCCESS )
			break;    
	    
		if( errorCode == LDAP_SERVER_DOWN || 
			errorCode == LDAP_UNAVAILABLE ||
			errorCode == LDAP_NO_MEMORY ||
			errorCode == LDAP_BUSY ||
			errorCode == LDAP_LOCAL_ERROR ||
			errorCode == LDAP_TIMEOUT ||
			errorCode == LDAP_OPERATIONS_ERROR ||
			errorCode == LDAP_TIMELIMIT_EXCEEDED)
		{
			cout << "[ERROR][" << errorCode << "]: ldap_search_s failed. " << ldap_err2string(errorCode) << endl;
			if(pLdapConnection)
			{
				if(ldap_unbind_s(pLdapConnection)==ERROR_SUCCESS)
				{
					cout << "Cleaning up current connection...\n";
					pLdapConnection = NULL;
				}
			}
			if(pSpnList)
			{
				ldap_msgfree(pSpnList);
				pSpnList = NULL;
			}

     		if( (pLdapConnection = getLdapConnectionCustom(domainDN, ulSearchSizeLimit, ltConnectionTimeout)) == NULL )
			{
				bConnectErr = true;
				break;	
			}
		}
		else
		{
			//DebugPrintf(SV_LOG_FATAL, "ldap_search_s failed with: 0x%0lx \n",errorCode);
			cout << "[FATAL ERROR][:" << errorCode << "]: ldap_search_s failed. " << ldap_err2string(errorCode) << endl;
			if(pSpnList != NULL)
			{
				ldap_msgfree(pSpnList);
				pSpnList = NULL;						
			}
			if(pLdapConnection)
			{
				if(ldap_unbind_s(pLdapConnection)==ERROR_SUCCESS)
				{
					cout << "Cleaning up ldap connection...\n";
					pLdapConnection = NULL;
				}
			}
			bSearchErr = true;
			break;
		}
	}while(1);

	if( bConnectErr )
	{
		cout << "[INFO]: Maximum connection attempts " << MAX_LDAP_CONNECT_ATTEMPTS << " has been reached...\n";
		cout << "[ERROR]: Unable to connect to directory server." << endl;
		return SVE_FAIL;
	}

	if( bSearchErr )
	{
		cout << "[ERROR] Error encountered while searching directory server.\n";
		return SVE_FAIL;
	}
	
	//char hostname[MAX_PATH] = {0};
	//gethostname(hostname, MAX_PATH);
	string hostCNLowerCase = convertoToLower(szSourceHost.c_str(), szSourceHost.size());//hostname, strlen(hostname));	

	LDAPMessage* pLdapMessageEntry = NULL;
	string searchFor;
	while(1)
	{
		if(pLdapMessageEntry == NULL)
			pLdapMessageEntry = ldap_first_entry(pLdapConnection, pSpnList);
		else
			pLdapMessageEntry = ldap_next_entry(pLdapConnection, pLdapMessageEntry);

		if(pLdapMessageEntry == NULL)
			break;
		PCHAR szDN = NULL; string entryDN;
		szDN = ldap_get_dn(pLdapConnection, pLdapMessageEntry);
		if(szDN)
		{
			entryDN = szDN;
			ldap_memfree(szDN);
			szDN = NULL;
		}
		 
		string entryDNLowerCase = convertoToLower(entryDN, entryDN.length());

		//cout <<"entryDNLowerCase=" << entryDNLowerCase.c_str() << endl;
		//cout <<"hostCNLowerCase=" << hostCNLowerCase.c_str() << endl;
		
		searchFor  = string("cn=") + hostCNLowerCase + ",";		

		if(strstr(entryDNLowerCase.c_str(), searchFor.c_str()) == NULL)
		{
			searchFor.clear();
			//move to the next entry
			continue;
		}

		BerElement* pBerEncoding = NULL;
		PCHAR pAttribute = NULL;
		PCHAR* ppValue = NULL;

		string value;		
		string fqn_hostName = szSourceHost + string(".") + domainName; //hostname + string(".") + domainName;

		LDAPMod modSourceAttribute;
		LDAPMod* conf[] = {&modSourceAttribute, NULL};
		
		char *data[2];
		data[0] = NULL;		
		data[1] = NULL;
				
		// Set up the values to be added		
		modSourceAttribute.mod_type = (PCHAR)EXCHANGE_SERVICE_PRINCIPAL_NAME;
		
				
		value = string("sampleInmageSPN") + "/" + fqn_hostName;
		size_t datalen;
        INM_SAFE_ARITHMETIC(datalen = InmSafeInt<size_t>::Type(sizeof(char)) * value.length() + 1, INMAGE_EX(value.length()))
		data[0] = (char *)malloc(datalen);
        inm_strcpy_s(data[0], datalen, value.c_str());

		modSourceAttribute.mod_op = LDAP_MOD_DELETE;	
		// verify there is data to be deleted

		if (data[0] != NULL) {
			modSourceAttribute.mod_vals.modv_strvals = data;		
		    errorCode = ldap_modify_s(pLdapConnection, (PCHAR)(entryDN.c_str()), conf);
			free(data[0]);
		}	
		break;
	}

	if( pSpnList )
	{
		ldap_msgfree(pSpnList);
		pSpnList = NULL;						
	}

	if( pLdapConnection )
	{
        ldap_unbind_s(pLdapConnection);
        pLdapConnection = NULL;
	}

	if (errorCode == LDAP_INSUFFICIENT_RIGHTS) 
	{
		cout << "LDAP ERROR:[" << errorCode << "] " << ldap_err2string(errorCode)<< endl;
		return errorCode;
	}

	return 0; // Sufficient Privileges 
}
///************************************************************************************************
//This function checks if the current user is having enough priviliges to Access and modify the DNS
///************************************************************************************************
ULONG ADInterface::CheckDNSPermissions(string szSource)
{
	DNS_RECORD* pSource = NULL, *pTarget = NULL, *pRedirectedSource = NULL;
    ULONG ulRet = 0;
	PIP4_ARRAY pSrvIPList = NULL;      //Pointer to IP4_ARRAY structure.
	HANDLE hToken = NULL;
	BOOL ccode = 0;
	BOOL impOutput = 0;
	std::string domainName ;
	HANDLE hContextHandle = NULL;
	DNS_STATUS dnsStatus;
	SEC_WINNT_AUTH_IDENTITY_A *customCredentials = NULL;
	  //Init Credentials Struct
	  customCredentials = (SEC_WINNT_AUTH_IDENTITY_A *)malloc(sizeof(SEC_WINNT_AUTH_IDENTITY_A));
	  if(customCredentials == NULL)
	  {
		 DebugPrintf(SV_LOG_ERROR,"Failed to allocate memory .\n");
		// return retValue;
	  }
	  memset(customCredentials,'\0',sizeof(SEC_WINNT_AUTH_IDENTITY_A));
	  customCredentials->Flags = SEC_WINNT_AUTH_IDENTITY_ANSI;

	 if (customCredentials->UserLength == 0) 
	 {
		 dnsStatus = DnsAcquireContextHandle(FALSE,NULL,&hContextHandle); //Context with default Credentials
	 } 
	 else 
	 {
	 	 dnsStatus = DnsAcquireContextHandle(FALSE,customCredentials,&hContextHandle); //Context with Custom Credentials
	 }
	//cout<<endl<<"Checking if the user has sufficient priviliges to perform a DNS Failover...";
	
	getDomainName(domainName);
	getFqdnName(szSource,domainName);
	//std::cout<<" in checkDns Permissions "<<szSource<<std::endl;

    ulRet = DnsQuery(szSource.c_str(),
                DNS_TYPE_ANY,
                DNS_QUERY_BYPASS_CACHE,
                pSrvIPList,
                &pSource,
                NULL);

    if(ulRet != 0)
    {
			cout <<"Could not lookup DNS entry for:" << szSource.c_str() << " ErrorCode: " << ulRet<<" [Hexadecimal: "<<std::hex<<ulRet<< "]\n";
		cout<<endl<<"Logon User does not have permission to lookup the DNS."<<endl;
        goto cleanup_and_exit;
    }
	
	//cout << szSource.c_str() << " FQDN = " << pSource->pName << "\n";

#ifdef SCOUT_WIN2K_SUPPORT
	ulRet = DnsQuery(szSource.c_str(),
                DNS_TYPE_ANY,
				DNS_QUERY_USE_TCP_ONLY,
                pSrvIPList,
                &pRedirectedSource,
                NULL);

#else
    ulRet = DnsQuery(szSource.c_str(),
                DNS_TYPE_ANY,
                DNS_QUERY_WIRE_ONLY,
                pSrvIPList,
                &pRedirectedSource,
                NULL);
#endif

    if(ulRet != 0)
    {
			cout<<"Could not lookup DNS entry for:" << szSource.c_str() << " ErrorCode: " << ulRet <<" [Hexadecimal: "<<std::hex<<ulRet<< "]\n";
		cout<<endl<<"Logon User does not have permission to lookup the DNS."<<endl;
        goto cleanup_and_exit;
    }
	//For Bug#10514:Check the logon credentials	
	
	ccode = LogonUser(".",".",".",LOGON32_LOGON_NEW_CREDENTIALS,LOGON32_PROVIDER_WINNT50,&hToken);
	if(ccode == 0)
	{
		ulRet = GetLastError();
		cout<<"Could not Logon the current User into the host. ErrorCode: "<<ulRet<<" [Hexadecimal: "<<std::hex<<ulRet<<"]\n";
		goto cleanup_and_exit;
	}
	else
	{
		impOutput = ImpersonateLoggedOnUser(&hToken);
	}
	if(pRedirectedSource->wType == CNameRecord)
	{
		if(CheckDNSPermissionsForCnameRecord(szSource) == SVS_OK)
		{
			ulRet = 0;
		}
		else
			ulRet = SVS_FALSE;
	}
	else
	{
		
		pRedirectedSource->dwTtl++;

		//Replace the DNS record with new TTL
		ulRet = DnsReplaceRecordSetA(pRedirectedSource,
					DNS_UPDATE_SECURITY_ON,
					hContextHandle,
					pSrvIPList,
					NULL);
		
		if(ulRet == 0 )
		{		
			pRedirectedSource->dwTtl--;
			DWORD dwReset = 0;

			//Replace the DNS record with original TTL
			dwReset = DnsReplaceRecordSet(pRedirectedSource,
							DNS_UPDATE_SECURITY_ON,
							NULL,
							pSrvIPList,
							NULL);
		
			if( dwReset != 0)
			{
				cout<<"inner value"<<std::endl;
				cout<<"\n.Failed to reset DNS record for "<<szSource.c_str()<< " ErrorCode:" << dwReset <<" [Hexadecimal: "<<std::hex<<dwReset<<"]\n";
				cout<<"\nUser does not have enough permissions to modify DNS records\n";
				goto cleanup_and_exit;
			}
			//else
				//cout<<"Successfully reset DNS record's TTL\n";
		}
		else
		{
			cout<<"\nFailed to update DNS record" << " ErrorCode: " << ulRet<<" [Hexadecimal: "<<std::hex<<ulRet <<"]\n";
			cout<<endl<<"User does not have enough permissions to modify DNS records\n";
			goto cleanup_and_exit;
		}
	}

cleanup_and_exit:

    if(pRedirectedSource)
        DnsRecordListFree(pRedirectedSource, DnsFreeRecordListDeep);

    if(pSource)
        DnsRecordListFree(pSource, DnsFreeRecordListDeep);
 	
	if(0 == ulRet)
	{
		//cout<<endl<<" Success: The logon user has sufficient priviliges to perform a DNS Failover."<<endl;
	}

	if( impOutput )
	{
		RevertToSelf();
	}

	if( hToken )
	{
		if( CloseHandle(hToken) == 0 )
		{
			cout << "[ERROR]: Failed to close handle to the security tocken.\n";
		}
	}

	
	DnsReleaseContextHandle(hContextHandle);

	//Release the memory allocated for "customCredentials"
	if(customCredentials)
		free(customCredentials);

	return ulRet;
}
void 
ADInterface::setAuditFlag()
{
	m_bAudit = true;
}

SVERROR 
ADInterface::getResponsibleMTAServer(const string& exchServer, string& mtaServer)
{
	if(exchServer.empty())
	{
		cout << "[ERROR]: The specified Exchange server name is empty" << endl;
		return SVE_FAIL;
	}
	
	string configurationDN;
	if (getConfigurationDN(configurationDN) == SVE_FAIL) 
	{
		cout << "Configuration Naming Context not found\n";
		return SVE_FAIL;
	}
	
	LDAP* pLdapConn = NULL;
	LDAPMessage* pResult = NULL;

	PCHAR pDN, pCred, pBase, pFilter, pAttrMTAServer;	
	ULONG ulBindErr, ulUnbindErr, ulLdapErrCode, ulSearchErr, ulScope, ulSearchSizeLimit;
	string szLdapMethod, szFilter, szBase;
	LDAP_TIMEVAL ltConnectionTimeout;

	PCHAR pAttributes[2] = { (PCHAR)ResponsibleMTAServer.c_str(), NULL };
	pAttrMTAServer = (PCHAR)ResponsibleMTAServer.c_str();
	szFilter = "(&(cn=" + exchServer + ")(objectClass=msExchExchangeServer))";	
	pFilter = PCHAR(szFilter.c_str());
	szBase = EXCHANGE_CONFIGURATION_BASE_PREFIX + configurationDN;
	pBase = PCHAR(szBase.c_str());
	pDN = NULL;
	pCred = NULL;
	ulScope = LDAP_SCOPE_SUBTREE;
	ulLdapErrCode = LDAP_SUCCESS;
	ulUnbindErr = 0;
	ulBindErr = 0;
	ulSearchSizeLimit = 0;
	ltConnectionTimeout.tv_sec = 0;
	
	pLdapConn = getLdapConnection();
	if (pLdapConn == NULL)
	{
		cout << "Failed to connect to the ldap server\n";
		return SVE_FAIL;
	}

	ulBindErr = ldap_bind_s(pLdapConn, pDN/*Entry to bind*/, pCred/*customCredentials*/, LDAP_AUTH_NEGOTIATE);		
	if(ulBindErr != LDAP_SUCCESS)
	{
		ulLdapErrCode = ulBindErr;
		szLdapMethod = "ldap_bind";
		goto Error_Exit;
	}

	do
	{
        ulSearchErr = ldap_search_s(pLdapConn, pBase, ulScope, pFilter, pAttributes, 0, &pResult);

		if(ulSearchErr == LDAP_SUCCESS)
			break;
	
		if( ulSearchErr != LDAP_SUCCESS )
		{
			if(pResult != NULL)
				ldap_msgfree(pResult);
			if(pLdapConn != NULL)
				ldap_unbind_s(pLdapConn);
			ulLdapErrCode = ulSearchErr;
			szLdapMethod = "ldap_search_s";
			if( (pLdapConn = getLdapConnectionCustom("", ulSearchSizeLimit, ltConnectionTimeout)) == NULL )
			{
				cout << "\n[FATAL ERROR]: Unable to connect to directory server. \n";
				goto Error_Exit;
			}
		}
	}while(1);
	
	if(pResult != NULL)
	{
		ULONG ulEntryCount = 0;
		ulEntryCount = ldap_count_entries(pLdapConn, pResult);
        if( ulEntryCount > 0 )
		{
            PCHAR *value = ldap_get_values(pLdapConn, pResult, pAttrMTAServer);		
			string temp = *value;
			size_t start, end;
			start = temp.find_first_of(',',0) + strlen(",CN=");
			end = temp.find_first_of(',',start) - 1;
			mtaServer.assign(temp, start, end - start + 1);				
		}		
		ldap_msgfree(pResult);		
	}	

Error_Exit:
	if(pLdapConn != NULL)
	{		
		ulUnbindErr = ldap_unbind(pLdapConn);
		if(ulUnbindErr != LDAP_SUCCESS)
		{ 
			cout << "[LDAP ERROR]:[" << hex << ulUnbindErr << "]\tldap_unbind: " << ldap_err2string(ulUnbindErr) << endl;
		}
	}
	
	if(ulLdapErrCode != LDAP_SUCCESS)
	{
		cout << "[LDAP ERROR]:[" << hex << ulLdapErrCode << "]\t" << szLdapMethod << ": " << ldap_err2string(ulLdapErrCode) << endl;
		return SVE_FAIL;
	}
	return SVS_OK;
}	

SVERROR ADInterface::createHostRecord(const std::string& sourceHost,const std::string& sourceHostIP,IP4_ARRAY* servers)
{
	  DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	  SVERROR retValue = SVS_FALSE;
	  DNS_RECORDA *dnsRecordResult = NULL;
	  DNS_STATUS dnsStatus;
  	  HANDLE hContextHandle;
	  DWORD dwOptions = DNS_UPDATE_SECURITY_ON;
	  SEC_WINNT_AUTH_IDENTITY_A *customCredentials = NULL;

	  //Init Credentials Struct
	  customCredentials = (SEC_WINNT_AUTH_IDENTITY_A *)malloc(sizeof(SEC_WINNT_AUTH_IDENTITY_A));
	  if(customCredentials == NULL)
	  {
		 DebugPrintf(SV_LOG_ERROR,"Failed to allocate memory .\n");
		 return retValue;
	  }
	  memset(customCredentials,'\0',sizeof(SEC_WINNT_AUTH_IDENTITY_A));
	  customCredentials->Flags = SEC_WINNT_AUTH_IDENTITY_ANSI;

	 if (customCredentials->UserLength == 0) 
	 {
		 dnsStatus = DnsAcquireContextHandle(FALSE,NULL,&hContextHandle); //Context with default Credentials
	 } 
	 else 
	 {
	 	 dnsStatus = DnsAcquireContextHandle(FALSE,customCredentials,&hContextHandle); //Context with Custom customCredentials
	 }

	 if (dnsStatus == ERROR_SUCCESS) 
	 {
		dnsRecordResult =(PDNS_RECORDA)malloc(sizeof(DNS_RECORDA));
		memset(dnsRecordResult,'\0',sizeof(DNS_RECORDA));
		dnsRecordResult->wType = DNS_TYPE_A; //DNS_TYPE_A by default
		
		dnsRecordResult->Data.A.IpAddress = inet_addr(sourceHostIP.c_str());
		dnsRecordResult->pName = const_cast<char*>(sourceHost.c_str());
		dnsRecordResult->wDataLength = sizeof(DNS_A_DATA);
		dnsRecordResult->Flags.S.Section = DNSREC_ANSWER;
		dnsRecordResult->Flags.S.CharSet = DnsCharSetAnsi;
		dnsRecordResult->pNext = NULL;
    //dnsRecordResult->dwTtl = m_hostRecordTtl;
		//Assigning the original DNS TTL value while creating the host record
		dnsRecordResult->dwTtl=m_DnsTtlvalue[sourceHost];

		dnsStatus = DnsModifyRecordsInSet_A(dnsRecordResult, //add record
			NULL, //delete record
			dwOptions,
			hContextHandle,
			servers,
			NULL);
		
		if (dnsStatus == ERROR_SUCCESS) 
		{
			std::cout<<"Created DNS host record for the host "<<sourceHost<<std::endl;
			//DnsRecordListFree(dnsRecordResult,DnsFreeRecordList);
			free(dnsRecordResult);
     	retValue = SVS_OK;
		} 
		else 
		{
			std::cout<<"Unable to create "<<sourceHost<<std::endl;
			cout<<"ErrorCode: "<<dnsStatus<<" [Hexadecimal: "<<std::hex<<dnsStatus<<"]\n";
		}
	 }
	 else
	 {
		 std::cout<<"Error Calling DnsAcquireContextHandle.Error Code "<<GetLastError()<<std::endl;
	 }
 	 DnsReleaseContextHandle(hContextHandle);
	 DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
     return retValue;
}

SVERROR ADInterface::createCnameRecord(const std::string& cname, std::string TgtHost,IP4_ARRAY* servers)
{
	  DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	  SVERROR retValue = SVS_FALSE;
	  DNS_RECORDA *dnsRecordResult = NULL;
	  DNS_STATUS dnsStatus;
  	  HANDLE hContextHandle;
	  DWORD dwOptions = DNS_UPDATE_SECURITY_ON;
	  SEC_WINNT_AUTH_IDENTITY_A *customCredentials = NULL;

	  //Init customCredentials Struct
	  customCredentials = (SEC_WINNT_AUTH_IDENTITY_A *)malloc(sizeof(SEC_WINNT_AUTH_IDENTITY_A));
	  if(customCredentials == NULL)
	  {
		 DebugPrintf(SV_LOG_ERROR,"Failed to allocate memory .\n");
		 return retValue;
	  }
	  memset(customCredentials,'\0',sizeof(SEC_WINNT_AUTH_IDENTITY_A));
	  customCredentials->Flags = SEC_WINNT_AUTH_IDENTITY_ANSI;

	 if (customCredentials->UserLength == 0) 
	 {
		 dnsStatus = DnsAcquireContextHandle(FALSE,NULL,&hContextHandle); //Context with default Credentials
	 } 
	 else 
	 {
	 	 dnsStatus = DnsAcquireContextHandle(FALSE,customCredentials,&hContextHandle); //Context with Custom Credentials
	 }
	 if (dnsStatus == ERROR_SUCCESS) 
	 {
		dnsRecordResult =(PDNS_RECORDA)malloc(sizeof(DNS_RECORDA));
		memset(dnsRecordResult,'\0',sizeof(DNS_RECORDA));
		dnsRecordResult->wType = DNS_TYPE_CNAME; //DNS_TYPE_A by default
		//dnsRecordResult->dwTtl = m_hostRecordTtl;
		//Assigning the original DNS TTL value while creating the CName record
		dnsRecordResult->dwTtl=m_DnsTtlvalue[cname];

		dnsRecordResult->Flags.S.Delete = 1;
		
		//dnsRecordResult->Data.Cname.pNameHost = address;
		dnsRecordResult->Data.CNAME.pNameHost = const_cast<char*>(TgtHost.c_str());
		dnsRecordResult->pName = const_cast<char*>(cname.c_str());
		dnsRecordResult->wDataLength = sizeof(DNS_PTR_DATA);
		dnsRecordResult->Flags.S.Section = DNSREC_ANSWER;
		dnsRecordResult->Flags.S.CharSet = DnsCharSetAnsi;
		dnsRecordResult->pNext = NULL;

		dnsStatus = DnsModifyRecordsInSet_A(dnsRecordResult, NULL, 	dwOptions,hContextHandle,servers,NULL);
		
		if (dnsStatus == ERROR_SUCCESS) 
		{
			std::cout<<"Created DNS CName Record for "<<TgtHost << " as " <<cname << std::endl;
			//DnsRecordListFree(dnsRecordResult,DnsFreeRecordList);
			free(dnsRecordResult);
			retValue = SVS_OK;
		} 
		else 
		{
			std::cout<<"Unable to create "<<cname<<".\n";
			std::cout<<"ErrorCode: "<<dnsStatus<<" [Hexadecimal: "<<std::hex<<dnsStatus<<"]\n";
		}
	 }
	 else
	 {
		 std::cout<<"Error Calling DnsAcquireContextHandle.Error Code "<<GetLastError()<<std::endl;
	 }
 	 DnsReleaseContextHandle(hContextHandle);
	 DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
     return retValue;
}
SVERROR ADInterface::deleteDNSRecordOfHost(const std::string& srcHost,IP4_ARRAY* servers, bool bDnsFailback)
{
	  DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	  SVERROR retValue = SVS_FALSE;
	  DNS_RECORDA *dnsRecordResult = NULL;
	  DNS_STATUS dnsStatus;
  	  HANDLE hContextHandle;
	  DWORD dwOptions = DNS_UPDATE_SECURITY_ON;
	  SEC_WINNT_AUTH_IDENTITY_A *customCredentials = NULL;

	  //Init Credentials Struct
	  customCredentials = (SEC_WINNT_AUTH_IDENTITY_A *)malloc(sizeof(SEC_WINNT_AUTH_IDENTITY_A));
	  if(customCredentials == NULL)
	  {
		 DebugPrintf(SV_LOG_ERROR," Failed to allocate memory .\n");
		 return retValue;
	  }
	  memset(customCredentials,'\0',sizeof(SEC_WINNT_AUTH_IDENTITY_A));
	  customCredentials->Flags = SEC_WINNT_AUTH_IDENTITY_ANSI;

	 if (customCredentials->UserLength == 0) 
	 {
		 dnsStatus = DnsAcquireContextHandle(FALSE,NULL,&hContextHandle); //Context with default Credentials
	 } 
	 else 
	 {
	 	 dnsStatus = DnsAcquireContextHandle(FALSE,customCredentials,&hContextHandle); //Context with Custom Credentials
	 }
	 if (dnsStatus == ERROR_SUCCESS) 
	 {
		dnsRecordResult = queryHostRecordFromDns(const_cast<char*>(srcHost.c_str()),servers,dnsStatus,bDnsFailback);
		
		if (dnsRecordResult != NULL) 
		{
			//storing DNS TTL value before deleting the record
			m_DnsTtlvalue[srcHost]=dnsRecordResult->dwTtl;

			//We are trying to delete only the CNAMe records here So the result what we got from "queryHostRecordFromDns" contains
			//two records(CNAME record as well as the target host record)
			//So below we are filtering it to modify only the CNAME record(by doing this we are able to remove the 87 error)
			if(dnsRecordResult->wType == CNameRecord)
			{
				dnsRecordResult->pNext = NULL;
			}
			dnsStatus = DnsModifyRecordsInSet_A(NULL, dnsRecordResult, dwOptions,hContextHandle,servers,NULL);
				
			if (dnsStatus == ERROR_SUCCESS) 
			{
				//std::cout<<"Host "<<srcHost<<" Deleted. "<<std::endl;
				std::cout<<"Host record "<<srcHost<<" Deleted. "<<std::endl;
				DebugPrintf(SV_LOG_DEBUG,"Host Deleted. Rechecking Record ..");
				DnsRecordListFree(dnsRecordResult,DnsFreeRecordList);
				////again querying to make sure that Record is not added in mean time
				//dnsRecordResult = DnsQueryA(const_cast<char*>(srcHost.c_str()),servers);

				//if(dnsRecordResult != NULL)
				//{
				//	DnsRecordListFree(dnsRecordResult,DnsFreeRecordList);
				//}
				retValue = SVS_OK;		
			} 
			else 
			{
				std::cout<<"Unable to delete the DNS Record. "<<srcHost<<std::endl;
				std::cout<<"Error Code: "<<dnsStatus<<" [Hexadecimal: "<<std::hex<<dnsStatus<<"]"<<std::endl;
			}
		} 
		else 
		{
			std::cout<<"Host "<<srcHost<<" record not found in DNS"<<std::endl;
			retValue = SVS_OK; //considering it success
		}
	 }
	 else
	 {
		 std::cout<<"Error Calling DnsAcquireContextHandle.Error Code "<<GetLastError()<<std::endl;
	 }

	 DnsReleaseContextHandle(hContextHandle);
     if(customCredentials)
	 {
		free(customCredentials);
	 }
    
	  DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
      return retValue;
}

DNS_RECORDA* ADInterface::queryHostRecordFromDns(char* name,IP4_ARRAY* servers,DNS_STATUS& dnsStatus, bool bDnsFailback)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	//DNS_STATUS dnsStatus;
	WORD type;
	if(false == bDnsFailback)
		type = DNS_TYPE_ANY;
	else
		type = DNS_TYPE_CNAME;
#ifdef SCOUT_WIN2K_SUPPORT
	DWORD fOptions = DNS_QUERY_BYPASS_CACHE | DNS_QUERY_USE_TCP_ONLY;
#else
	DWORD fOptions = DNS_QUERY_BYPASS_CACHE | DNS_QUERY_NO_LOCAL_NAME |DNS_QUERY_NO_HOSTS_FILE | DNS_QUERY_NO_NETBT | 	DNS_QUERY_TREAT_AS_FQDN;
#endif
	PVOID* reserved = NULL;
	DNS_RECORDA *dnsRecords = (PDNS_RECORDA)malloc(sizeof(DNS_RECORDA));
	DNS_RECORDA *dnsRecordResult=NULL;
	IN_ADDR ipaddr;
	std::stringstream ostr;
	int count=0;

	if (!name) 
	{
		return (NULL);
	} 
	else 
	{
		memset(dnsRecords,'\0',sizeof(DNS_RECORDA));
		dnsStatus = DnsQuery_A( name, //PCWSTR pszName,
				type, //WORD wType,
				fOptions, //DWORD fOptions,
				servers, //PIP4_ARRAY aipServers,
				&dnsRecords, //PDNS_RECORD* ppQueryResultsSet,
				reserved ); //PVOID* pReserved

		if (dnsStatus == ERROR_SUCCESS)
		{
			fflush(stdout);
			dnsRecordResult=dnsRecords;
			do 
			{
				switch (dnsRecordResult->wType) 
				{
					case DNS_TYPE_A:
						ipaddr.S_un.S_addr = (dnsRecordResult->Data.A.IpAddress);
						ostr<<" Host "<<dnsRecordResult->pName <<" resolved as "<<inet_ntoa(ipaddr)<<std::endl;
					break;
					
					case DNS_TYPE_NS:
						ostr<<"Domain "<<dnsRecordResult->pName<<" Dns Servers "<<dnsRecordResult->Data.Ns.pNameHost<<std::endl;	
					break;
					
					case DNS_TYPE_CNAME:
						ostr<<"Host "<<dnsRecordResult->pName<<" resolved as CNAME "<<dnsRecordResult->Data.CNAME.pNameHost<<std::endl;			
						//queryHostRecordFromDns(dnsRecordResult->Data.Cname.pNameHost,servers);
					break;

					case DNS_TYPE_SOA:
						ostr<<" SOA Information: PrimaryServer: "<<dnsRecordResult->Data.Soa.pNamePrimaryServer<<std::endl;
						ostr<<" SOA Information: Administrator: "<<dnsRecordResult->Data.Soa.pNameAdministrator;
						printf(" SOA Information: SerialNo %x - Refresh %i - retry %i - Expire %i - DefaultTld 	%i\n",
						dnsRecordResult->Data.Soa.dwSerialNo,
						dnsRecordResult->Data.Soa.dwRefresh,
						dnsRecordResult->Data.Soa.dwRetry,
						dnsRecordResult->Data.Soa.dwExpire,
						dnsRecordResult->Data.Soa.dwDefaultTtl);
					break;

					case DNS_TYPE_MX:
						ostr<<dnsRecordResult->pName<<" MX Server resolved as "<<dnsRecordResult->Data.Mx.pNameExchange<<" Preference "<<dnsRecordResult->Data.Mx.wPreference<<std::endl;
					break;

					case DNS_TYPE_TEXT:
						ostr<<" Text "<<dnsRecordResult->Data.Txt.dwStringCount<<std::endl;
					break;

					case DNS_TYPE_SRV:
						ostr<<" SRV Record. NameTarget "<<dnsRecordResult->Data.Srv.pNameTarget<<std::endl;
						ostr<<" Priority "<<dnsRecordResult->Data.Srv.wPriority<<" Port "<<dnsRecordResult->Data.Srv.wPort<<" Weigth "<<dnsRecordResult->Data.Srv.wWeight<<std::endl;
					break;

					default:
						ostr<<" DnsQuery returned unknown type "<<dnsRecordResult->wType<<std::endl;
					break;
				}
				dnsRecordResult=dnsRecordResult->pNext;
			} while (dnsRecordResult!=NULL);
		} 
		else 
		{
			if (dnsStatus == 9003) 
			{
				//even if record is not found in DNS (for Source Machine),we will go and create the CNAME
				ostr<<"Record is not found in DNS "<<std::endl;
				cout<<"Record is not found in DNS "<<std::endl;
			}
			else
			{
				ostr<<" Query failed with status "<<dnsStatus<<" Error Code: "<<GetLastError();
				cout<<" Query failed with status "<<dnsStatus<<" [Hexadecimal: "<<std::hex<<dnsStatus<<"] Error Code: "<<GetLastError()<<std::endl;
			}
		}
	}
	DebugPrintf(SV_LOG_DEBUG,"%s",ostr.str().c_str());
  DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return dnsRecords;
}

std::string ADInterface::getRemoteHostIpAddress(const std::string& srcHost)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	std::string strIPAddress;
	std::string hostNameWithoutFQDN ;
	size_t index = srcHost.find_first_of(".");
	hostNameWithoutFQDN = srcHost.substr(0,index);
	char *ptr = NULL;
	struct hostent *ptrHostent = NULL;
	if((ptrHostent = gethostbyname(hostNameWithoutFQDN.c_str()))!= NULL)
	{
		struct in_addr addr;
		addr.s_addr = *(reinterpret_cast<u_long*>(ptrHostent->h_addr_list[0]));
	//	addr.s_addr =  *(u_long *) ptrHostent->h_addr_list[0];
		ptr = inet_ntoa(addr);
		strIPAddress = std::string(ptr);
	}
	else
	{
		DebugPrintf(SV_LOG_ERROR," GetHostByName Failed");
		DebugPrintf(SV_LOG_ERROR,"\nError [%d] ",WSAGetLastError ());
	}
   DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
   return strIPAddress;
}

/*****************************************************************
During Failover,steps are
	i)  Delete Source host Record
	ii) Create a CName with Source Host Name to Target

During Failback
    i) Delete CNAME
	ii) Create a New Source Host Record with original IP

*****************************************************************/
SVERROR ADInterface::dnsFailoverUsingCname( std::string srcHost,  std::string tgtHost, const std::string& srcIP ,std::string dnsServIp,std::string dnsDomain, std::string user, std::string password)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	SVERROR result = SVS_FALSE;
	PIP4_ARRAY pSrvIPList = NULL;      //Pointer to IP4_ARRAY structure.
	HANDLE hToken = NULL;
	BOOL ccode = 0;
	BOOL impOutput = 0;
	ADInterface ad;
	DNS_STATUS dnsStatus;
	std::string domainName;
	bool bDnsFailback = false;
  
 /* getDomainName(domainName);
	
	getFqdnName(srcHost,domainName);
	getFqdnName(tgtHost,domainName);

	*/
	if(!dnsServIp.empty())
	{
		pSrvIPList = (PIP4_ARRAY) LocalAlloc(LPTR,sizeof(IP4_ARRAY));
		if(!pSrvIPList)
		{
			std::cout<<"Memory allocation failed \n"<<std::endl;
		    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
			return result;
		}
		pSrvIPList->AddrCount = 1;
		pSrvIPList->AddrArray[0] = inet_addr(dnsServIp.c_str()); //DNS server IP address

		if(!dnsServIp.empty())
		{
			ccode = LogonUser(user.c_str(),
						dnsDomain.c_str(),
						password.c_str(),
						LOGON32_LOGON_NEW_CREDENTIALS,
						LOGON32_PROVIDER_WINNT50,
						&hToken);		

			if(ccode == 0)
				cout << "Error in LogonUser(). Error Code: " << GetLastError() << endl;

			if(hToken==NULL)
			{
				cout << "Couldn't get handle in LogonUser()." << endl;
  			    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
				return result;
			}
			impOutput = ImpersonateLoggedOnUser(hToken);
			//Clean up password 
			SecureZeroMemory(&password[0],password.size());		
		}
	}
	getDomainName(domainName);
	getFqdnName(tgtHost,domainName);

	//display DNS Server Name
	std::string dnsServerName;
	getDnsServerName(dnsServerName);
	if(!dnsServerName.empty())
		cout << endl << " DNS Server : " << dnsServerName << endl;

	//Source host (i.e actual target) will be empty for DNS failback as the command for DNSi failback is
	//dns.exe -failback -host <actual src> -ip <src IP>
	//When source host is found empty get the source host name from the canonical name of its CNAME record.
	if(!srcIP.empty() && srcHost.empty())
	{
		 DNS_RECORDA *recordSet = NULL;
		 recordSet=queryHostRecordFromDns(const_cast<char*>(tgtHost.c_str()),pSrvIPList,dnsStatus);
		 if(recordSet!=NULL)
		 {
			 srcHost=recordSet->Data.CNAME.pNameHost;
			 DnsRecordListFree(recordSet,DnsFreeRecordList);	
			 if(srcHost.empty())
			 {
				 cout<<"The dependent name of CNAME record : "<<tgtHost<<" is found empty in the DNS."<<endl;
				 return SVE_FAIL;
			 }
		 }
	}
	
		getFqdnName(srcHost,domainName);
	

	if (srcIP.empty()) 
	{
				
		//result = ad.DNSChangeIP(srcHost, tgtHost, "", "", "", "", "", true);
		//std::cout<<"\n Deleting DNS host record of actual source : "<<srcHost<<std::endl;
		result = deleteDNSRecordOfHost(srcHost,pSrvIPList);
	
		//Even if source name is not present ,Create CNAME even
		result = createCnameRecord(srcHost,tgtHost,pSrvIPList);

	}
	else 
	{
		/*1. Try to delete ths source CNAME
		  2. If not able to delete,delete the original Target Host
		  3. Now,delete the CNAME of original source
		  */
		//trying to delete CNAME
		bDnsFailback = true;
		std::string hostIp = getRemoteHostIpAddress(srcHost);
		std::cout<<std::endl<<"Deleting CName record "<<tgtHost<< " ---> "<< srcHost << std::endl;
		result = deleteDNSRecordOfHost(tgtHost,pSrvIPList,bDnsFailback);
		if(result != SVS_OK)
		{
			bDnsFailback = false;
			std::cout<<std::endl<<"Ignoring the previous error ."<<std::endl;
			//std::cout<<" Again trying to delete CNAME by removing base dependent Name "<<std::endl;
			std::cout<<" Again trying to delete CNAME record for Secondary Server" << std::endl;
			if(!hostIp.empty())
			{
				//delete hostname
				//std::cout<<std::endl<<"Deleting DNS host record of actual target "<<srcHost<<std::endl;
				result = deleteDNSRecordOfHost(srcHost,pSrvIPList);
				if(result == SVS_OK)
				{
					//Now again try to delete CNAME
					//std::cout<<std::endl<<"Deleting CName record for actual target host "<<srcHost<<" as "<<tgtHost<<std::endl;
					result = deleteDNSRecordOfHost(tgtHost,pSrvIPList);
					if(result == SVS_OK)
					{
						result = createHostRecord(srcHost,hostIp,pSrvIPList);
					}
					else
					{
						std::cout<<"Failed to  delete DnsRecord for Host "<<tgtHost<<std::endl; 
						result = SVS_FALSE;
					}
				}
				else
				{
					std::cout<<"Failed to find deleteDnsRecord for Host "<<srcHost<<std::endl; 
					result = SVS_FALSE;
				}
			}
		}
		if(result == SVS_OK)
		{
			result = createHostRecord(tgtHost,srcIP,pSrvIPList);
			if(result == SVS_OK)
			{
				cout << "\n***** Successfully completed DNS Failback" << std::endl << std::endl;

			}
			else
			{
				std::cout<<"Failed to create hostname: "<<tgtHost<<" with IP address "<<srcIP<<endl;
				cout << "\n***** Failed DNS Failback from server " << srcHost.c_str() << " to " << tgtHost.c_str() << std::endl << std::endl;
			}
		}
		else
		{
			std::cout<<"Failed to delete the CNAME record for host:"<<tgtHost<<std::endl;
			cout << "\n***** Failed DNS Failback from server " << srcHost.c_str() << " to " << tgtHost.c_str() << std::endl << std::endl;
		}
	}
	if( impOutput )
		RevertToSelf();

	if( hToken )
	{
		if( CloseHandle(hToken) == 0 )
		{
			cout << "[ERROR]: Failed to close handle to the security tocken.\n";
		}
	}

	if( pSrvIPList )
	{
		LocalFree(pSrvIPList);
	}

	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return result;
}


void ADInterface::getDomainName(std::string& strHostname)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	DWORD size = 260;
	char szHostName[260] = {0};
	if(!GetComputerNameEx(ComputerNameDnsDomain,szHostName,&size))
	{
		std::cout<<"Failed to delete the hostname "<<std::endl;
		std::cout<<"Error Code :"<<GetLastError()<<std::endl;
		strHostname = ""; //this will make the subsequent calls to fail
	}
	else
	{
		strHostname = std::string(szHostName);
	}
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}

void ADInterface::getFqdnName(std::string& hostname,const std::string& domainName)
{
	size_t index = hostname.find_first_of(".");
	if(index == std::string::npos)
	{
		hostname += std::string(".");
		hostname += domainName;
	}
}

ULONG ADInterface::CheckDNSPermissionsForCnameRecord(const std::string& sourceHost)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	ULONG retError = 1;
	IP4_ARRAY *servers = NULL;
	DNS_RECORDA *dnsRecordResult = NULL;
	DNS_STATUS dnsStatus;
	//std::cout<<" in check dns permission for cname "<<std::endl;
	
	dnsRecordResult = queryHostRecordFromDns(const_cast<char*>(sourceHost.c_str()),servers,dnsStatus);
	if(dnsRecordResult->wType == CNameRecord)
	{
		//dnsRecordResult->dwTtl--;
		/*
		if(dnsRecordResult->pNext->Next != NULL)
		{
		   dnsRecordResult->pNext = dnsRecordResult->pNext->Next;
		 }*/
		dnsRecordResult->pNext = NULL;
		dnsStatus = DnsReplaceRecordSetA(dnsRecordResult,
					DNS_UPDATE_SECURITY_ON, //Attempts nonsecure dynamic update. If refused, then attempts secure dynamic 		update.
					NULL,
					servers,//pServerList,
					NULL);//pReserved
		if (dnsStatus == ERROR_SUCCESS)
		{
			DnsRecordListFree(dnsRecordResult,DnsFreeRecordList);
			retError  = GetLastError();
		}
		else
		{
			retError  = GetLastError();
			cout<<"\n Failed in dnrePlaeReCORDSET"<<std::endl;
			cout<<"Error code : "<<GetLastError()<<std::endl;
		}
	}
	else
	{
		std::cout<<" Record is not CNAME type "<<std::endl;
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return retError;
}


SVERROR ADInterface::getDNSRecordType(std::string tgtHost,bool &bCNameDNSRecord)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	DNS_RECORDA *recordSet = NULL;
	//DWORD dwOptions = DNS_UPDATE_SECURITY_ON;
	SVERROR retValue=SVS_OK;
	IP4_ARRAY *serverIPs=NULL;
	std::string domainName;
	DNS_STATUS dnsStatus;

	getDomainName(domainName);
	getFqdnName(tgtHost,domainName);	
	recordSet = queryHostRecordFromDns(const_cast<char*>(tgtHost.c_str()),serverIPs,dnsStatus);
	if (recordSet != NULL) 
	{
		if(dnsStatus != 9003)
		{
			if(recordSet->wType==DNS_TYPE_CNAME)
				bCNameDNSRecord=true;
			else if(recordSet->wType==DNS_TYPE_A)
				bCNameDNSRecord=false;
			else
			{
				std::cout<<"DNS record type of host "<<tgtHost<<" is neither Host record nor CNAME record."<<endl;
				retValue=SVE_FAIL;
			}
		}
		else
		{
			std::cout<<"Unable to find the Type of DNS Record for the host: "<<tgtHost<<endl;
			retValue=SVE_FAIL;
		}
		DnsRecordListFree(recordSet,DnsFreeRecordList);
	}
	else
	{
		std::cout<<"Failed to find the Type of DNS Record for the host: "<<tgtHost<<endl;
		std::cout<<"[ERROR CODE]: "<<GetLastError()<<std::endl;
    retValue=SVE_FAIL;
	}
	   
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
  return retValue;
}

void ADInterface::getDnsServerName(std::string &dnsServer)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	dnsServer = "";
	DWORD DnLen = 512;
	char domainName[512] = {0};
	if(!GetComputerNameEx(ComputerNameDnsDomain,domainName,&DnLen))
	{
		DebugPrintf(SV_LOG_ERROR, "GetComputerNameEx failed. Error %d\n", GetLastError()) ;
		DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
		return;
	}
	DebugPrintf(SV_LOG_DEBUG, "Domain Name: %s\n", domainName) ;
	PDNS_RECORD pDnsRR /*= (PDNS_RECORD)malloc(sizeof(DNS_RECORD))*/;
#ifdef SCOUT_WIN2K_SUPPORT
	DWORD dwOptins = DNS_QUERY_BYPASS_CACHE | DNS_QUERY_USE_TCP_ONLY;
#else
	DWORD dwOptins = DNS_QUERY_BYPASS_CACHE | DNS_QUERY_NO_LOCAL_NAME |DNS_QUERY_NO_HOSTS_FILE | DNS_QUERY_NO_NETBT | 	DNS_QUERY_TREAT_AS_FQDN;
#endif
	DNS_STATUS dnsStatus = DnsQuery(domainName,DNS_TYPE_SOA,dwOptins,NULL,&pDnsRR,NULL);
	if(dnsStatus == ERROR_SUCCESS)
	{
		if(pDnsRR->wType == DNS_TYPE_SOA)
		{
			dnsServer.assign(pDnsRR->Data.Soa.pNamePrimaryServer);
			DebugPrintf(SV_LOG_DEBUG, "DNS Server Name: %s\n", dnsServer.c_str()) ;
		}
		else
		{
			DebugPrintf(SV_LOG_ERROR, "got non SOA record, can not find dns server name\n");
		}
		DnsRecordListFree(pDnsRR,DnsFreeRecordList);
	}
	else
	{
		DebugPrintf(SV_LOG_ERROR, "DnsQuery failed. Ret val %d\n",dnsStatus);
	}

	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}
string ADInterface::getLDAPServerName(LDAP* const _pLdapConnection)
{
	LDAP *pLdapConnection = NULL;
	string strLdapServerName;
	ULONG ulSearchlimit = 0;
	LDAP_TIMEVAL time;
	time.tv_sec = 0;

	if(_pLdapConnection)
		pLdapConnection = _pLdapConnection;
	else
		pLdapConnection = getLdapConnectionCustom("",ulSearchlimit,time);

	if(NULL == pLdapConnection)
	{
		cout << "[ERROR]: Can not find domain name." << endl;
	}
	else
	{
		PCHAR strLDAPServer = NULL;
		ULONG ulReturn = LDAP_SUCCESS;
		do
		{
			ulReturn = ldap_get_option(pLdapConnection,LDAP_OPT_HOST_NAME,&strLDAPServer);
			if(LDAP_SUCCESS == ulReturn)
			{
				break;
			}
			else if(ulReturn == LDAP_SERVER_DOWN		|| 
					ulReturn == LDAP_UNAVAILABLE		||
					ulReturn == LDAP_NO_MEMORY			||
					ulReturn == LDAP_BUSY				||
					ulReturn == LDAP_LOCAL_ERROR		||
					ulReturn == LDAP_TIMEOUT			||
					ulReturn == LDAP_OPERATIONS_ERROR	||
					ulReturn == LDAP_TIMELIMIT_EXCEEDED )
			{
				if(pLdapConnection)
				{
					ldap_unbind(pLdapConnection);
					pLdapConnection = NULL;
				}
				pLdapConnection = getLdapConnectionCustom("",ulSearchlimit,time);
				if(NULL == pLdapConnection)
				{
					cout << "[ERROR]: Can not find domain name." << endl;
					return strLdapServerName;
				}
			}
			else
			{
				cout << "Can not find domain name. Error: " << ldap_err2string(ulReturn) << endl;
				return strLdapServerName;
			}
		}while(1);
		if(strLDAPServer)
		{
			strLdapServerName = strLDAPServer;
			DebugPrintf(SV_LOG_DEBUG,"LDAP Server: %s\n",strLdapServerName.c_str());
		}
		if(!_pLdapConnection)
			ulReturn = ldap_unbind(pLdapConnection);
	}
	return strLdapServerName;
}
