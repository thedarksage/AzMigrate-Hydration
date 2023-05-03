#include "Stdafx.h"
#include "LDAPConnection.h"
#include "localconfigurator.h"
#include "rpc.h"
#include "ADInterface.h"
#include "inmsafecapis.h"
#include "inmsafeint.h"
#include "inmageex.h"

using namespace std;
extern const char *nonUserNamingContexts[];
extern const char *exchValues[];
#define VALID_CHAR_IN_CN_SIZE   69
extern char Token[];

// A Global function which returns the upper case letters string of the given string.
string ToUpper(const string szStringToConvert,const size_t length)
{
	DebugPrintf(SV_LOG_DEBUG,"Line(%u): BEGIN %s\n",__LINE__,__FUNCTION__);
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
	DebugPrintf(SV_LOG_DEBUG,"Line(%u): END %s\n",__LINE__,__FUNCTION__);
    return szConvertedString;
}
// A Global function which returns the lower case letters string of the given string.
string ToLower(const string szStringToConvert,const size_t length)
{
	DebugPrintf(SV_LOG_DEBUG,"Line(%u): BEGIN %s\n",__LINE__,__FUNCTION__);
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
	DebugPrintf(SV_LOG_DEBUG,"Line(%u): END %s\n",__LINE__,__FUNCTION__);
    return szConvertedString;
}

LDAPConnection::LDAPConnection()
{
	DebugPrintf(SV_LOG_DEBUG,"Line(%u): BEGIN %s\n",__LINE__,__FUNCTION__);
	m_pLDAPConnection = NULL;
	m_searchSizeLimit = 0;
	m_tv_sec = 0;
	m_baseDN = "";
	m_hostName = "";
	m_configDN = "";
	DebugPrintf(SV_LOG_DEBUG,"Line(%u): END %s\n",__LINE__,__FUNCTION__);
}

LDAPConnection::~LDAPConnection(void)
{
	DebugPrintf(SV_LOG_DEBUG,"Line(%u): BEGIN %s\n",__LINE__,__FUNCTION__);
	ULONG ulReturn = 0;
	if(!isClosed())
	{
		ulReturn = ldap_unbind_s(m_pLDAPConnection);
		if(LDAP_SUCCESS != ulReturn)
		{
			cout<<"[INFO]: LDAP Connection might not be cleaned properly."<<endl;
			DebugPrintf(SV_LOG_DEBUG,"Line(%u): Info %s: LDAP Connection might not be cleaned properly.\n",__LINE__,__FUNCTION__);
		}
	}
	DebugPrintf(SV_LOG_DEBUG,"Line(%u): END %s\n",__LINE__,__FUNCTION__);
}

ULONG LDAPConnection::EstablishConnection(const std::string &domain, const ULONG searchSizeLimit, const LONG tv_sec)
{
	DebugPrintf(SV_LOG_DEBUG,"Line(%u): BEGIN %s\n",__LINE__,__FUNCTION__);
	ULONG ulVersion = LDAP_VERSION3;
	ULONG ulRturn = 0;

	m_searchSizeLimit = searchSizeLimit;
	m_tv_sec = tv_sec;

	if(!domain.empty())
		m_hostName = domain;

	l_timeval searchTimeLimit;
	searchTimeLimit.tv_sec = m_tv_sec;
	int connectAttempts = 0;

	//Try for the connetion for MAX_LDAP_CONNECT_ATTEMPTS times, in case of failing to connect to server
	do
	{
		if(m_hostName.empty())
			m_pLDAPConnection = ldap_init(NULL,LDAP_PORT);
		else
			m_pLDAPConnection = ldap_init((PCHAR)m_hostName.c_str(),LDAP_PORT);
		if(!m_pLDAPConnection)
		{
			if(connectAttempts >= MAX_LDAP_CONNECT_ATTEMPTS)
			{
				ulRturn = LdapGetLastError();
				cout<<"Failed in establishing connectio to LDAP Server..."<<endl
					<<"[ERROR] : "<<ldap_err2string(ulRturn)<<endl;
				DebugPrintf(SV_LOG_DEBUG,"Line(%u): Error %s: ldap_init() Failed. %s\n",__LINE__,__FUNCTION__,ldap_err2string(ulRturn));
				return ulRturn;
			}
			connectAttempts++;
			continue;
		}

		ulRturn = ldap_set_option(m_pLDAPConnection,LDAP_OPT_VERSION,(void*)ulVersion);
		if(LDAP_SUCCESS != ulRturn)
		{
			if(connectAttempts >= MAX_LDAP_CONNECT_ATTEMPTS)
			{
				cout<<"Failed to setting the LDAP Version ..."<<endl
					<<"[ERROR] : "<<ldap_err2string(ulRturn)<<endl;
				DebugPrintf(SV_LOG_DEBUG,"Line(%u): Error %s: ldap_set_option() Failed. %s\n",__LINE__,__FUNCTION__,ldap_err2string(ulRturn));
				return ulRturn;
			}
			connectAttempts++;
			continue;
		}

		ulRturn = ldap_set_option(m_pLDAPConnection,LDAP_OPT_SIZELIMIT,(void*)&m_searchSizeLimit);
		if(LDAP_SUCCESS != ulRturn)
		{
			if(connectAttempts >= MAX_LDAP_CONNECT_ATTEMPTS)
			{
				cout<<"Failed to setting the search limit ..."<<endl
					<<"[ERROR] : "<<ldap_err2string(ulRturn)<<endl;
				DebugPrintf(SV_LOG_DEBUG,"Line(%u): Error %s: ldap_set_option() Failed. %s\n",__LINE__,__FUNCTION__,ldap_err2string(ulRturn));
				return ulRturn;
			}
			connectAttempts++;
			continue;
		}

		ulRturn = ldap_connect(m_pLDAPConnection,NULL);
		if(LDAP_SUCCESS != ulRturn)
		{
			if(connectAttempts >= MAX_LDAP_CONNECT_ATTEMPTS)
			{
				cout<<"ldap_connect() Failed ..."<<endl
					<<"[ERROR] : "<<ldap_err2string(ulRturn)<<endl;
				DebugPrintf(SV_LOG_DEBUG,"Line(%u): Error %s: ldap_connect() Failed. %s\n",__LINE__,__FUNCTION__,ldap_err2string(ulRturn));
				return ulRturn;
			}
			connectAttempts++;
			continue;
		}

		//reaching this point means every thing is fine.Exiting the loop 
		break;

	}while(1);
	DebugPrintf(SV_LOG_DEBUG,"Line(%u): END %s\n",__LINE__,__FUNCTION__);
	return LDAP_SUCCESS;
}

ULONG LDAPConnection::Bind(const char* pDn)
{
	DebugPrintf(SV_LOG_DEBUG,"Line(%u): BEGIN %s\n",__LINE__,__FUNCTION__);
	ULONG ulReturn = 0;
	int connectAttempts = 0;

	m_baseDN = pDn;
	
	do
	{
		ulReturn = ldap_bind_s(m_pLDAPConnection,(PCHAR)pDn,NULL,LDAP_AUTH_NEGOTIATE);

		if(LDAP_SUCCESS != ulReturn)
		{
			if(connectAttempts >= MAX_LDAP_CONNECT_ATTEMPTS)
			{
				cout<<"Bind() Failed.\n"
					<<"[ERROR]: "<<ldap_err2string(ulReturn)<<endl;
				DebugPrintf(SV_LOG_DEBUG,"Line(%u): Error %s: ldap_bind_s() failed.\n",__LINE__,__FUNCTION__);
				return ulReturn;
			}
			connectAttempts++;
			continue;
		}
		//Display LDAP Server details.
		PCHAR ldapServer = NULL;
		ldap_get_option(m_pLDAPConnection,LDAP_OPT_HOST_NAME,(void*)&ldapServer);
		if(ldapServer)
		{
			cout << endl << "LDAP Connection Details: " << endl
			     << "Directory Server: " << ldapServer << endl;
		}
		string domainName = getDNSDomainName();
		if(!domainName.empty())
			cout << "Domain : " << domainName << endl;
		//reaching this point means every thing is fine. Exiting the loop ...
		break;

	}while(1);
	DebugPrintf(SV_LOG_DEBUG,"Line(%u): END %s\n",__LINE__,__FUNCTION__);
	return LDAP_SUCCESS;
}

ULONG LDAPConnection::CloseConnection(void)
{
	DebugPrintf(SV_LOG_DEBUG,"Line(%u): BEGIN %s\n",__LINE__,__FUNCTION__);
	ULONG ulReturn = 0;

	if(!m_pLDAPConnection)
		return LDAP_SUCCESS;

	ulReturn = ldap_unbind_s(m_pLDAPConnection);
	if(LDAP_SUCCESS != ulReturn)
	{
		cout<<"Failed in closing the LDAP Connection..."<<endl
			<<"[ERROR] : "<<ldap_err2string(ulReturn)<<endl;
		DebugPrintf(SV_LOG_DEBUG,"Line(%u): Error %s: ldap_unbind_s() Failed. %s\n",__LINE__,__FUNCTION__,ldap_err2string(ulReturn));
		return ulReturn;
	}
	m_pLDAPConnection = NULL;
	DebugPrintf(SV_LOG_DEBUG,"Line(%u): END %s\n",__LINE__,__FUNCTION__);
	return LDAP_SUCCESS;
}

BOOL LDAPConnection::isClosed(void)
{
	DebugPrintf(SV_LOG_DEBUG,"Line(%u): BEGIN %s\n",__LINE__,__FUNCTION__);
	DebugPrintf(SV_LOG_DEBUG,"Line(%u): END %s\n",__LINE__,__FUNCTION__);
	return (m_pLDAPConnection == NULL) ? TRUE : FALSE ;
}

ULONG LDAPConnection::HandleConnectionError()
{
	DebugPrintf(SV_LOG_DEBUG,"Line(%u): BEGIN %s\n",__LINE__,__FUNCTION__);
	ULONG ulRetrun = LDAP_SUCCESS;

	if(!isClosed())
	{
		cout<<"Cleaning the current connection . . ."<<endl;
		ulRetrun = CloseConnection();
		if(LDAP_SUCCESS != ulRetrun)
		{
			DebugPrintf(SV_LOG_DEBUG,"Line(%u): Error %s: CloseConnection() failed.\n",__LINE__,__FUNCTION__);
			return ulRetrun;
		}
	}
	//Establish new conection
	ulRetrun = EstablishConnection(m_hostName,m_searchSizeLimit,m_tv_sec);
	if(LDAP_SUCCESS != ulRetrun)
	{
		DebugPrintf(SV_LOG_DEBUG,"Line(%u): Error %s: EstablishConnection() failed.\n",__LINE__,__FUNCTION__);
		return ulRetrun;
	}
	//Perform Bind
	ulRetrun = Bind(m_baseDN.c_str());
	if(LDAP_SUCCESS != ulRetrun)
	{
		DebugPrintf(SV_LOG_DEBUG,"Line(%u): Error %s: Bind() failed.\n",__LINE__,__FUNCTION__);
		return ulRetrun;
	}
	DebugPrintf(SV_LOG_DEBUG,"Line(%u): END %s\n",__LINE__,__FUNCTION__);
	return ulRetrun;
}
ULONG LDAPConnection::GetNamingcontexts(list<string>& namingContexts)
{
	DebugPrintf(SV_LOG_DEBUG,"Line(%u): BEGIN %s\n",__LINE__,__FUNCTION__);
	if(m_pLDAPConnection == NULL)
	{
		cout<<"[ERROR]: Conection is not established."<<endl;
		DebugPrintf(SV_LOG_DEBUG,"Line(%u): Error %s: Conection is not established.\n",__LINE__,__FUNCTION__);
		return LDAP_CONNECT_ERROR;
	}

	ULONG ulReturn = 0;
	LDAPMessage *pInmSearchResult,*pInmEntry;
	BerElement *pInmBerEle;
	char* pInmAttribute;
	char** inm_val;
	char* inm_req_attribute[2];
	inm_req_attribute[0] = "namingContexts";
	inm_req_attribute[1] = NULL;

	ulReturn = ldap_set_option( m_pLDAPConnection, LDAP_OPT_REFERRALS, LDAP_OPT_OFF);
	if(LDAP_SUCCESS != ulReturn)
	{
		cout<<"Failed to set options LDAP_OPT_REFERELS..."<<endl
			<<"[ERROR] : "<<ldap_err2string(ulReturn)<<endl;
		DebugPrintf(SV_LOG_DEBUG,"Line(%u): Error %s: Failed to set options LDAP_OPT_REFERELS..\n",__LINE__,__FUNCTION__);
		return ulReturn;
	}

	//int searchCount = 0;
	do{
		ulReturn = ldap_search_s(m_pLDAPConnection,"",LDAP_SCOPE_BASE,"(objectclass=*)",inm_req_attribute,0,&pInmSearchResult);

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
			cout << "[ERROR]: ldap_search_s failed. "<< ldap_err2string(ulReturn) << endl;
			DebugPrintf(SV_LOG_DEBUG,"Line(%u): Error %s: ldap_search_s failed().\n",__LINE__,__FUNCTION__);
			if(pInmSearchResult)
			{
				ldap_msgfree(pInmSearchResult);
				pInmSearchResult = NULL;
			}

			////If same error hass repeated for MAX_SEARCH_ATTEMPTS times then stop the searach process
			//if(searchCount >= MAX_SEARCH_ATTEMPTS)
			//{
			//	cout<<"[INFO]: Maximum search attempts has reached."<<endl
			//		<<"Failed to search the directory server."<<endl;
			//	return ulReturn;
			//}
			ulReturn = HandleConnectionError();
			if(LDAP_SUCCESS != ulReturn)
			{
				cout << "[INFO]: Maximum connection attempts " << MAX_LDAP_CONNECT_ATTEMPTS << " has been reached."<<endl;
				DebugPrintf(SV_LOG_DEBUG,"Line(%u): Info %s: Maximum connection attempts has been reached.\n",__LINE__,__FUNCTION__);
				cout << "[ERROR]: Unable to connect to directory server." << endl;
				DebugPrintf(SV_LOG_DEBUG,"Line(%u): Error %s: Unable to connect to directory server.\n",__LINE__,__FUNCTION__);
				return ulReturn;
			}
		}
		else
		{
			if(pInmSearchResult)
			{
				ldap_msgfree(pInmSearchResult);
				pInmSearchResult = NULL;
			}

			cout<<"Error encountered while searching directory server."<<endl;
			cout<<"ldap_search_s() failed ..."<<endl
				<<"[ERROR] : "<<ldap_err2string(ulReturn)<<endl;
			DebugPrintf(SV_LOG_DEBUG,"Line(%u): Error %s: Error encountered while searching directory server. ldap_search_s() failed. %s.\n",__LINE__,__FUNCTION__,ldap_err2string(ulReturn));
			return ulReturn;
		}

		//searchCount++;
	}while(1);

	pInmEntry = ldap_first_entry(m_pLDAPConnection,pInmSearchResult);
	if(NULL == pInmEntry)
	{
		ldap_msgfree(pInmSearchResult);
		cout<<"No entries found or ldap_first_entry() failed ..."<<endl;
		DebugPrintf(SV_LOG_DEBUG,"Line(%u): Error %s: No entries found or ldap_first_entry() failed .\n",__LINE__,__FUNCTION__);
		return (ULONG)GetLastError();
	}
	while(NULL != pInmEntry)
	{
		pInmAttribute = ldap_first_attribute(m_pLDAPConnection,pInmEntry,&pInmBerEle);
		while(NULL != pInmAttribute)
		{
			inm_val = ldap_get_values(m_pLDAPConnection,pInmEntry,pInmAttribute);
			for(int i=0;inm_val[i] != NULL;i++)
				namingContexts.push_back((string)inm_val[i]);
			ldap_value_free(inm_val);

			ldap_memfree(pInmAttribute);
			pInmAttribute = ldap_next_attribute(m_pLDAPConnection,pInmEntry,pInmBerEle);
		}
		ber_free(pInmBerEle,0);
		pInmEntry = ldap_next_entry(m_pLDAPConnection,pInmEntry);
	}
	ldap_msgfree(pInmSearchResult);
	DebugPrintf(SV_LOG_DEBUG,"Line(%u): END %s\n",__LINE__,__FUNCTION__);
	return LDAP_SUCCESS;
}
ULONG LDAPConnection::GetconfigurationDN()
{
	DebugPrintf(SV_LOG_DEBUG,"Line(%u): BEGIN %s\n",__LINE__,__FUNCTION__);
	list<string> lstNamingContexts;
	if(isClosed())//Make sure that the LDAP connection is established
	{
		cout<<"LDAP connection is not established ..."<<endl;
		DebugPrintf(SV_LOG_DEBUG,"Line(%u): Error %s: Conection is not established.\n",__LINE__,__FUNCTION__);
		return LDAP_CONNECT_ERROR;
	}
	if(GetNamingcontexts(lstNamingContexts) != LDAP_SUCCESS)//Gets all the naming contexts
	{
		cout<<"Failed to retrieve namingContexts..."<<endl;
		DebugPrintf(SV_LOG_DEBUG,"Line(%u): Error %s: Failed to retrieve namingContexts.\n",__LINE__,__FUNCTION__);
		CloseConnection();
	}
	if(lstNamingContexts.empty())
	{
		cout<<"Operaton Failed.No Namingcontexts found.\n"<<endl;
		DebugPrintf(SV_LOG_DEBUG,"Line(%u): Error %s: Operaton Failed.No Namingcontexts found.\n",__LINE__,__FUNCTION__);
		return LDAP_OPERATIONS_ERROR;
	}

	// Filtering configurationDN from the list of namingcontexts
	list<string>::iterator lstIter = lstNamingContexts.begin();
	while(lstIter != lstNamingContexts.end())
	{
		if(lstIter->find("CN=Configuration") != string::npos)
		{
			m_configDN = (*lstIter);
			break;
		}
		lstIter++;
	}
	if(m_configDN.empty())
	{
		cout<<"Failed to get the configurationDN"<<endl;
		DebugPrintf(SV_LOG_DEBUG,"Line(%u): Error %s: Failed to get the configurationDN.\n",__LINE__,__FUNCTION__);
		return LDAP_NO_SUCH_OBJECT;
	}
	DebugPrintf(SV_LOG_DEBUG,"Line(%u): END %s\n",__LINE__,__FUNCTION__);
	return LDAP_SUCCESS;
}
string LDAPConnection::getDNSDomainName()
{
	string dnsDomainName;

	PCHAR strDomain = NULL;
	ULONG ulReturn = LDAP_SUCCESS;
	do
	{
		ulReturn = ldap_get_option(m_pLDAPConnection,LDAP_OPT_DNSDOMAIN_NAME,&strDomain);
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
			ulReturn = HandleConnectionError();
			if(LDAP_SUCCESS != ulReturn)
			{
				cout << "[Error]: Can not find domain name." << endl;
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
		DebugPrintf(SV_LOG_DEBUG,"LDAP: connected to the domain %s\n",dnsDomainName.c_str());
	}

	return dnsDomainName;
}
string LDAPConnection::getRootDomainName()
{
	string rootDomainName;
	if(m_configDN.empty())
	{
		if(LDAP_SUCCESS != GetconfigurationDN())
			return rootDomainName;
	}
	string configDn = m_configDN;
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

	string domainName = getDNSDomainName();
	DebugPrintf(SV_LOG_DEBUG,"\n Domain : %s\n Root Domain: %s\n",domainName.c_str(),rootDomainName.c_str());
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
ULONG LDAPConnection::GetServerDN(const string svrName,string& svrDN)
{
	DebugPrintf(SV_LOG_DEBUG,"Line(%u): BEGIN %s\n",__LINE__,__FUNCTION__);
	if(NULL == m_pLDAPConnection)
	{
		cout<<"[ERROR]: Conection is not established..."<<endl;
		DebugPrintf(SV_LOG_DEBUG,"Line(%u): Error %s: Conection is not established.\n",__LINE__,__FUNCTION__);
		return LDAP_CONNECT_ERROR;
	}

	ULONG ulReturn = 0;
	LDAPMessage *pInmSearchResult,*pInmEntry;
	char* inm_req_attribute[2];
	inm_req_attribute[0] = "cn";
	inm_req_attribute[1] = NULL;

	if(m_configDN.empty())
	{
		ulReturn = GetconfigurationDN();
		if(LDAP_SUCCESS != ulReturn)
			return ulReturn;
	}

	string filter = "(&(objectclass=msExchExchangeServer)(cn="+svrName+"))";

	//int searchCount = 0;
	do{
		ulReturn = ldap_search_s(m_pLDAPConnection,(PCHAR)m_configDN.c_str(),LDAP_SCOPE_SUBTREE,(PCHAR)filter.c_str(),inm_req_attribute,0,&pInmSearchResult);
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
			cout << "[ERROR]: ldap_search_s failed. "<< ldap_err2string(ulReturn) << endl;
			DebugPrintf(SV_LOG_DEBUG,"Line(%u): Error %s: ldap_search_s failed().\n",__LINE__,__FUNCTION__);
			if(pInmSearchResult)
			{
				ldap_msgfree(pInmSearchResult);
				pInmSearchResult = NULL;
			}

			////If same error hass repeated for MAX_SEARCH_ATTEMPTS times then stop the searach process
			//if(searchCount >= MAX_SEARCH_ATTEMPTS)
			//{
			//	cout<<"[INFO]: Maximum search attempts has reached."<<endl
			//		<<"Failed to search the directory server."<<endl;
			//	return ulReturn;
			//}
			ulReturn = HandleConnectionError();
			if(LDAP_SUCCESS != ulReturn)
			{
				cout << "[INFO]: Maximum connection attempts " << MAX_LDAP_CONNECT_ATTEMPTS << " has been reached."<<endl;
				DebugPrintf(SV_LOG_DEBUG,"Line(%u): Info %s: Maximum connection attempts has been reached.\n",__LINE__,__FUNCTION__);
				cout << "[ERROR]: Unable to connect to directory server." << endl;
				DebugPrintf(SV_LOG_DEBUG,"Line(%u): Error %s: Unable to connect to directory server.\n",__LINE__,__FUNCTION__);
				return ulReturn;
			}
		}
		else
		{
			if(pInmSearchResult)
			{
				ldap_msgfree(pInmSearchResult);
				pInmSearchResult = NULL;
			}

			cout<<"Error encountered while searching directory server."<<endl;
			cout<<"ldap_search_s() failed ..."<<endl
				<<"[ERROR] : "<<ldap_err2string(ulReturn)<<endl;
			DebugPrintf(SV_LOG_DEBUG,"Line(%u): Error %s: Error encountered while searching directory server. ldap_search_s() failed. %s.\n",__LINE__,__FUNCTION__,ldap_err2string(ulReturn));
			return ulReturn;
		}

		//searchCount++;
	}while(1);

	pInmEntry = ldap_first_entry(m_pLDAPConnection,pInmSearchResult);
	if(NULL == pInmEntry)
	{
		ldap_msgfree(pInmSearchResult);
		cout<<"No entries found or ldap_first_entry() failed ..."<<endl;
		DebugPrintf(SV_LOG_DEBUG,"Line(%u): Error %s: No entries found or ldap_first_entry() failed .\n",__LINE__,__FUNCTION__);
		return (ULONG)GetLastError();
	}
	while(NULL != pInmEntry)
	{
		svrDN = string(ldap_get_dn(m_pLDAPConnection,pInmEntry));
		pInmEntry = ldap_next_entry(m_pLDAPConnection,pInmEntry);
	}
	ldap_msgfree(pInmSearchResult);
	DebugPrintf(SV_LOG_DEBUG,"Line(%u): END %s\n",__LINE__,__FUNCTION__);
	return LDAP_SUCCESS;

}
ULONG LDAPConnection::GetExchangeServerList(map<string,list<string> >& lstExServerCN)
{
	DebugPrintf(SV_LOG_DEBUG,"Line(%u): BEGIN %s\n",__LINE__,__FUNCTION__);
	if(NULL == m_pLDAPConnection)
	{
		cout<<"[ERROR]: Conection is not established..."<<endl;
		DebugPrintf(SV_LOG_DEBUG,"Line(%u): Error %s: Conection is not established.\n",__LINE__,__FUNCTION__);
		return LDAP_CONNECT_ERROR;
	}

	//Clear any old entries from the lstExServerCN map.
	lstExServerCN.clear();

	ULONG ulReturn = 0;
	LDAPMessage *pInmSearchResult,*pInmEntry;
	BerElement *pInmBerEle;
	char* pInmAttribute;
	char** inm_val;

	char* inm_req_attribute[3];	
	inm_req_attribute[0] = "cn";
	inm_req_attribute[1] = "msExchCurrentServerRoles";
	inm_req_attribute[2] = NULL;

	string svrCN,Roles;
	list<string> svrRoles;

	if(m_configDN.empty())
	{
		ulReturn = GetconfigurationDN();
		if(LDAP_SUCCESS != ulReturn)
			return ulReturn;
	}

	//int searchCount = 0;
	do
	{
		ulReturn = ldap_search_s(m_pLDAPConnection,
						(PCHAR)m_configDN.c_str(),
						LDAP_SCOPE_SUBTREE,
						"(objectclass=msExchExchangeServer)",
						inm_req_attribute,
						0,
						&pInmSearchResult);
		
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
			cout << "[ERROR]: ldap_search_s failed. "<< ldap_err2string(ulReturn) << endl;
			DebugPrintf(SV_LOG_DEBUG,"Line(%u): Error %s: ldap_search_s failed().\n",__LINE__,__FUNCTION__);
			if(pInmSearchResult)
			{
				ldap_msgfree(pInmSearchResult);
				pInmSearchResult = NULL;
			}

			////If same error hass repeated for MAX_SEARCH_ATTEMPTS times then stop the searach process
			//if(searchCount >= MAX_SEARCH_ATTEMPTS)
			//{
			//	cout<<"[INFO]: Maximum search attempts has reached."<<endl
			//		<<"Failed to search the directory server."<<endl;
			//	return ulReturn;
			//}
			ulReturn = HandleConnectionError();
			if(LDAP_SUCCESS != ulReturn)
			{
				cout << "[INFO]: Maximum connection attempts " << MAX_LDAP_CONNECT_ATTEMPTS << " has been reached."<<endl;
				DebugPrintf(SV_LOG_DEBUG,"Line(%u): Info %s: Maximum connection attempts has been reached.\n",__LINE__,__FUNCTION__);
				cout << "[ERROR]: Unable to connect to directory server." << endl;
				DebugPrintf(SV_LOG_DEBUG,"Line(%u): Error %s: Unable to connect to directory server.\n",__LINE__,__FUNCTION__);
				return ulReturn;
			}
		}
		else
		{
			if(pInmSearchResult)
			{
				ldap_msgfree(pInmSearchResult);
				pInmSearchResult = NULL;
			}

			cout<<"Error encountered while searching directory server."<<endl;
			cout<<"ldap_search_s() failed ..."<<endl
				<<"[ERROR] : "<<ldap_err2string(ulReturn)<<endl;
			DebugPrintf(SV_LOG_DEBUG,"Line(%u): Error %s: Error encountered while searching directory server. ldap_search_s() failed. %s.\n",__LINE__,__FUNCTION__,ldap_err2string(ulReturn));
			return ulReturn;
		}

		//searchCount++;
	}while(1);		

	ULONG entryCount = ldap_count_entries(m_pLDAPConnection,pInmSearchResult);
	if(entryCount <= 0)
	{
		cout<<"[WARNING]: No exchange servers installed in the domain"<<endl;
		DebugPrintf(SV_LOG_DEBUG,"Line(%u): Error %s: No Exchange Servers installed in the domain.\n",__LINE__,__FUNCTION__);
		return LDAP_SUCCESS;
	}

	pInmEntry = ldap_first_entry(m_pLDAPConnection,pInmSearchResult);
	if(NULL == pInmEntry)
	{
		ldap_msgfree(pInmSearchResult);
		cout<<"ldap_first_entry() failed ..."<<endl
			<<"[ERROR] : "<<ldap_err2string(ulReturn)<<endl;
		DebugPrintf(SV_LOG_DEBUG,"Line(%u): Error %s: ldap_first_entry() failed .\n",__LINE__,__FUNCTION__);
		return (ULONG)GetLastError();
	}
	while(NULL != pInmEntry)
	{
		svrCN.clear();

		pInmAttribute = ldap_first_attribute(m_pLDAPConnection,pInmEntry,&pInmBerEle);
		while(NULL != pInmAttribute)
		{
			inm_val = ldap_get_values(m_pLDAPConnection,pInmEntry,pInmAttribute);
			if(0 == strcmp(pInmAttribute,"cn"))
				svrCN = (string)*inm_val;
			else if(0 == strcmp(pInmAttribute,"msExchCurrentServerRoles"))
			{
				Roles = (string)*inm_val;
				ulReturn = GetServerRoles(Roles.c_str(),svrRoles);// If no exchange-roles installed, then GetServerRoles() return LDAP_OPERATIONS_ERROR.
			}
			ldap_value_free(inm_val);

			ldap_memfree(pInmAttribute);
			pInmAttribute = ldap_next_attribute(m_pLDAPConnection,pInmEntry,pInmBerEle);
		}
		ber_free(pInmBerEle,0);
		if(LDAP_SUCCESS == ulReturn)		//If no roles installed then not need to perform discovery on that server.(Even exchange management tool installed)
			lstExServerCN.insert(make_pair(svrCN,svrRoles));
		pInmEntry = ldap_next_entry(m_pLDAPConnection,pInmEntry);
	}
	ldap_msgfree(pInmSearchResult);
	DebugPrintf(SV_LOG_DEBUG,"Line(%u): END %s\n",__LINE__,__FUNCTION__);
	return LDAP_SUCCESS;
}
ULONG LDAPConnection::GetDAGorMDBInfo(map<string, map<string,string> >& MDBs,char **inm_req_attribute,string objClass)
{
	DebugPrintf(SV_LOG_DEBUG,"Line(%u): BEGIN %s\n",__LINE__,__FUNCTION__);
	if(NULL == m_pLDAPConnection)
	{
		cout<<"[ERROR]: Conection is not established..."<<endl;
		DebugPrintf(SV_LOG_DEBUG,"Line(%u): Error %s: Conection is not established.\n",__LINE__,__FUNCTION__);
		return LDAP_CONNECT_ERROR;
	}
	string MDBcn;
	map<string,string> attribiteValues;

	ULONG ulReturn = 0;
	LDAPMessage *pInmSearchResult,*pInmEntry;
	BerElement *pInmBerEle;
	char* pInmAttribute;
	char** inm_val;
	string filter;
	filter = "(objectclass="+objClass+")";

	if(m_configDN.empty())
	{
		ulReturn = GetconfigurationDN();
		if(LDAP_SUCCESS != ulReturn)
			return ulReturn;
	}
	//int searchCount = 0;
	do{	
		ulReturn = ldap_search_s(m_pLDAPConnection,(PCHAR)m_configDN.c_str(),LDAP_SCOPE_SUBTREE,(PCHAR)filter.c_str(),inm_req_attribute,0,&pInmSearchResult);
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
			cout << "[ERROR]: ldap_search_s failed. "<< ldap_err2string(ulReturn) << endl;
			DebugPrintf(SV_LOG_DEBUG,"Line(%u): Error %s: ldap_search_s failed().\n",__LINE__,__FUNCTION__);
			if(pInmSearchResult)
			{
				ldap_msgfree(pInmSearchResult);
				pInmSearchResult = NULL;
			}

			//If same error hass repeated for MAX_SEARCH_ATTEMPTS times then stop the searach process
			/*if(searchCount >= MAX_SEARCH_ATTEMPTS)
			{
				cout<<"[INFO]: Maximum search attempts has reached."<<endl
					<<"Failed to search the directory server."<<endl;
				return ulReturn;
			}*/
			ulReturn = HandleConnectionError();
			if(LDAP_SUCCESS != ulReturn)
			{
				cout << "[INFO]: Maximum connection attempts " << MAX_LDAP_CONNECT_ATTEMPTS << " has been reached."<<endl;
				DebugPrintf(SV_LOG_DEBUG,"Line(%u): Info %s: Maximum connection attempts has been reached.\n",__LINE__,__FUNCTION__);
				cout << "[ERROR]: Unable to connect to directory server." << endl;
				DebugPrintf(SV_LOG_DEBUG,"Line(%u): Error %s: Unable to connect to directory server.\n",__LINE__,__FUNCTION__);
				return ulReturn;
			}
		}
		else
		{
			if(pInmSearchResult)
			{
				ldap_msgfree(pInmSearchResult);
				pInmSearchResult = NULL;
			}

			cout<<"Error encountered while searching directory server."<<endl;
			cout<<"ldap_search_s() failed ..."<<endl
				<<"[ERROR] : "<<ldap_err2string(ulReturn)<<endl;
			DebugPrintf(SV_LOG_DEBUG,"Line(%u): Error %s: Error encountered while searching directory server. ldap_search_s() failed. %s.\n",__LINE__,__FUNCTION__,ldap_err2string(ulReturn));
			return ulReturn;
		}

		//searchCount++;
	}while(1);

	ulReturn = ldap_count_entries(m_pLDAPConnection,pInmSearchResult);
	if(0 == ulReturn)
	{
		//No entries found
		ldap_msgfree(pInmSearchResult);
		return LDAP_SUCCESS;
	}

	pInmEntry = ldap_first_entry(m_pLDAPConnection,pInmSearchResult);
	if(NULL == pInmEntry)
	{
		ldap_msgfree(pInmSearchResult);
		cout<<"ldap_first_entry() failed ."<<endl;
		DebugPrintf(SV_LOG_DEBUG,"Line(%u): Error %s: No entries found or ldap_first_entry() failed .\n",__LINE__,__FUNCTION__);
		return (ULONG)GetLastError();
	}
	while(NULL != pInmEntry)
	{
		MDBcn.clear();
		attribiteValues.clear();

		pInmAttribute = ldap_first_attribute(m_pLDAPConnection,pInmEntry,&pInmBerEle);
		while(NULL != pInmAttribute)
		{
			string value;
			value.clear();
			inm_val = ldap_get_values(m_pLDAPConnection,pInmEntry,pInmAttribute);
			
			for(int i=0;inm_val[i] != NULL;i++)//some times an attribute may have multiple values.
			{
				if(i)
					value += ";"; // in case of multivalued atrribute, the values are seperated by ';'
				value += (string)inm_val[i];
			}

			if(0 == strcmp(pInmAttribute,"cn"))
				MDBcn = value;
			attribiteValues.insert(make_pair(pInmAttribute,value));

			ldap_value_free(inm_val);

			ldap_memfree(pInmAttribute); 
			pInmAttribute = ldap_next_attribute(m_pLDAPConnection,pInmEntry,pInmBerEle);
		}
		ber_free(pInmBerEle,0);

		MDBs.insert(make_pair(MDBcn,attribiteValues));

		pInmEntry = ldap_next_entry(m_pLDAPConnection,pInmEntry);
	}
	ldap_msgfree(pInmSearchResult);
	DebugPrintf(SV_LOG_DEBUG,"Line(%u): END %s\n",__LINE__,__FUNCTION__);
	return LDAP_SUCCESS;
}
ULONG LDAPConnection::GetServerRoles(const char* role,list<string>& serverRoles)
{
	DebugPrintf(SV_LOG_DEBUG,"Line(%u): BEGIN %s\n",__LINE__,__FUNCTION__);
	serverRoles.clear();
	int RoleVal = atoi(role);
	if(RoleVal == 0)
	{
		serverRoles.clear();// Mean no values in the roles-list
		return LDAP_OPERATIONS_ERROR;
	}
	if(RoleVal == 64)
	{
		serverRoles.push_back(EDGE_ROLE);
		return LDAP_SUCCESS;
	}
	if(RoleVal >= 32)
	{
		serverRoles.push_back(HT_ROLE);
		RoleVal -= 32;											  //============================== SERVER ROLE'S LOGIC======================================//
	}															 //----------------------------------------------------------------------------------------//
	if(RoleVal >= 16)											// Each server role is indicated by some integer value.									  //
	{														   //  -> Mainbox Role			= 2												             //
		serverRoles.push_back(UMSG_ROLE);					  //   -> ClientAccess Role		= 4													        //
		RoleVal -= 16;										 //	   -> unified Messaging Role= 16                                                       //
	}														//     -> Hub Transport Role	= 32											          //
	if(RoleVal >= 4)									   //	   -> Edge Transport Role	= 64													 //
	{													  // "msExchCurrentServerRoles" atrribute contains the sum of these role's value installed. //
		serverRoles.push_back(CAS_ROLE);	             //========================================================================================//
		RoleVal -= 4;
	}
	if(RoleVal >= 2)
	{
		serverRoles.push_back(MBX_ROLE);
		RoleVal -= 2;
	}

	if(RoleVal != 0)//Validating the msExchCurrentServerRoles value...
	{
		cout<<"Unable to decide Exchange server roles on the Exchange server"<<endl
			<<"[ERROR]: Invalied msExchCurrentServerRoles value..."<<endl;
		serverRoles.clear();
		return LDAP_OPERATIONS_ERROR;
	}
	DebugPrintf(SV_LOG_DEBUG,"Line(%u): END %s\n",__LINE__,__FUNCTION__);
	return LDAP_SUCCESS;
}
ULONG LDAPConnection::GetMDBCopyInfo(const char* base,char **inm_req_attribute,list<string>& svrList)
{
	DebugPrintf(SV_LOG_DEBUG,"Line(%u): BEGIN %s\n",__LINE__,__FUNCTION__);
	if(NULL == m_pLDAPConnection)
	{
		cout<<"[ERROR]: Conection is not established..."<<endl;
		DebugPrintf(SV_LOG_DEBUG,"Line(%u): Error %s: Conection is not established.\n",__LINE__,__FUNCTION__);
		return LDAP_CONNECT_ERROR;
	}

	ULONG ulReturn = 0;
	LDAPMessage *pInmSearchResult,*pInmEntry;
	BerElement *pInmBerEle;
	char* pInmAttribute;
	char** inm_val;
	string filter;
	filter = "(objectclass=msExchMDBCopy)";
	//int searchCount = 0;
	do{
		ulReturn = ldap_search_s(m_pLDAPConnection,(PCHAR)base,LDAP_SCOPE_ONELEVEL,(PCHAR)filter.c_str(),inm_req_attribute,0,&pInmSearchResult);
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
			cout << "[ERROR]: ldap_search_s failed. "<< ldap_err2string(ulReturn) << endl;
			DebugPrintf(SV_LOG_DEBUG,"Line(%u): Error %s: ldap_search_s failed().\n",__LINE__,__FUNCTION__);
			if(pInmSearchResult)
			{
				ldap_msgfree(pInmSearchResult);
				pInmSearchResult = NULL;
			}

			//If same error hass repeated for MAX_SEARCH_ATTEMPTS times then stop the searach process
			/*if(searchCount >= MAX_SEARCH_ATTEMPTS)
			{
				cout<<"[INFO]: Maximum search attempts has reached."<<endl
					<<"Failed to search the directory server."<<endl;
				return ulReturn;
			}*/
			ulReturn = HandleConnectionError();
			if(LDAP_SUCCESS != ulReturn)
			{
				cout << "[INFO]: Maximum connection attempts " << MAX_LDAP_CONNECT_ATTEMPTS << " has been reached."<<endl;
				DebugPrintf(SV_LOG_DEBUG,"Line(%u): Info %s: Maximum connection attempts has been reached.\n",__LINE__,__FUNCTION__);
				cout << "[ERROR]: Unable to connect to directory server." << endl;
				DebugPrintf(SV_LOG_DEBUG,"Line(%u): Error %s: Unable to connect to directory server.\n",__LINE__,__FUNCTION__);
				return ulReturn;
			}
		}
		else
		{
			if(pInmSearchResult)
			{
				ldap_msgfree(pInmSearchResult);
				pInmSearchResult = NULL;
			}

			cout<<"Error encountered while searching directory server."<<endl;
			cout<<"ldap_search_s() failed ..."<<endl
				<<"[ERROR] : "<<ldap_err2string(ulReturn)<<endl;
			DebugPrintf(SV_LOG_DEBUG,"Line(%u): Error %s: Error encountered while searching directory server. ldap_search_s() failed. %s.\n",__LINE__,__FUNCTION__,ldap_err2string(ulReturn));
			return ulReturn;
		}

		//searchCount++;
	}while(1);
	ulReturn = ldap_count_entries(m_pLDAPConnection,pInmSearchResult);
	if(0 == ulReturn)
	{
		//No entries found
		ldap_msgfree(pInmSearchResult);
		DebugPrintf(SV_LOG_DEBUG,"Line(%u): Error %s: No entries found.\n",__LINE__,__FUNCTION__);
		return LDAP_SUCCESS;
	}

	pInmEntry = ldap_first_entry(m_pLDAPConnection,pInmSearchResult);
	if(NULL == pInmEntry)
	{
		ldap_msgfree(pInmSearchResult);
		cout<<"ldap_first_entry() failed ..."<<endl;
		DebugPrintf(SV_LOG_DEBUG,"Line(%u): Error %s: ldap_first_entry() failed .\n",__LINE__,__FUNCTION__);
		return (ULONG)GetLastError();
	}
	while(NULL != pInmEntry)
	{
		pInmAttribute = ldap_first_attribute(m_pLDAPConnection,pInmEntry,&pInmBerEle);
		while(NULL != pInmAttribute)
		{
			inm_val = ldap_get_values(m_pLDAPConnection,pInmEntry,pInmAttribute);		
			for(int i=0;inm_val[i] != NULL;i++)
				svrList.push_back((string)inm_val[i]);
			ldap_value_free(inm_val);
			ldap_memfree(pInmAttribute);
			pInmAttribute = ldap_next_attribute(m_pLDAPConnection,pInmEntry,pInmBerEle);
		}
		ber_free(pInmBerEle,0);
		pInmEntry = ldap_next_entry(m_pLDAPConnection,pInmEntry);
	}
	ldap_msgfree(pInmSearchResult);
	DebugPrintf(SV_LOG_DEBUG,"Line(%u): END %s\n",__LINE__,__FUNCTION__);
	return LDAP_SUCCESS;
}
ULONG LDAPConnection::GetMDBCopiesAtServer(const string svrName,map<string,MDBCopyInfo>& MDBs)
{
	DebugPrintf(SV_LOG_DEBUG,"Line(%u): BEGIN %s\n",__LINE__,__FUNCTION__);
	USES_CONVERSION;
	if(svrName.empty())
	{
		cout<<"[WAARNING]: No Excgange server is specified."<<endl;
		DebugPrintf(SV_LOG_DEBUG,"Line(%u): Warning %s: Conection is not established.\n",__LINE__,__FUNCTION__);
		return LDAP_OPERATIONS_ERROR;
	}
	if(NULL == m_pLDAPConnection)
	{
		cout<<"[ERROR]: Conection is not established..."<<endl;
		DebugPrintf(SV_LOG_DEBUG,"Line(%u): Error %s: Conection is not established.\n",__LINE__,__FUNCTION__);
		return LDAP_CONNECT_ERROR;
	}

	MDBs.clear();// Cleaning MAP in any old values are there in it.

	//map< MDB_DN, HostServerLink>
	map<string, string> MDBCopySearchResult;

	ULONG ulReturn = 0;
	LDAPMessage *pInmSearchResult,*pInmEntry;
	BerElement *pInmBer1;
	char* pInmAttribute1;
	char** inm_val1;
	PCHAR MDBCopyattrs[2];
	MDBCopyattrs[0] = "msExchHostServerLink";
	MDBCopyattrs[1] = NULL;
	string filter;
	filter = "(&(objectclass=msExchMDBCopy)(cn="+svrName+"))";
	string MDBCopyDN,MDBDN,HostServerLink,MDBName;

	if(m_configDN.empty())
	{
		ulReturn = GetconfigurationDN();
		if(LDAP_SUCCESS != ulReturn)
			return ulReturn;
	}
						//'MDBCopy at Server' search started...
	//int searchCount = 0;
	do{
		ulReturn = ldap_search_s(m_pLDAPConnection,(char*)m_configDN.c_str(),LDAP_SCOPE_SUBTREE,(char*)filter.c_str(),MDBCopyattrs,0,&pInmSearchResult);
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
			cout << "[ERROR]: ldap_search_s failed. "<< ldap_err2string(ulReturn) << endl;
			DebugPrintf(SV_LOG_DEBUG,"Line(%u): Error %s: ldap_search_s failed().\n",__LINE__,__FUNCTION__);
			if(pInmSearchResult)
			{
				ldap_msgfree(pInmSearchResult);
				pInmSearchResult = NULL;
			}

			//If same error hass repeated for MAX_SEARCH_ATTEMPTS times then stop the searach process
			/*if(searchCount >= MAX_SEARCH_ATTEMPTS)
			{
				cout<<"[INFO]: Maximum search attempts has reached."<<endl
					<<"Failed to search the directory server."<<endl;
				return ulReturn;
			}*/
			ulReturn = HandleConnectionError();
			if(LDAP_SUCCESS != ulReturn)
			{
				cout << "[INFO]: Maximum connection attempts " << MAX_LDAP_CONNECT_ATTEMPTS << " has been reached."<<endl;
				DebugPrintf(SV_LOG_DEBUG,"Line(%u): Info %s: Maximum connection attempts has been reached.\n",__LINE__,__FUNCTION__);
				cout << "[ERROR]: Unable to connect to directory server." << endl;
				DebugPrintf(SV_LOG_DEBUG,"Line(%u): Error %s: Unable to connect to directory server.\n",__LINE__,__FUNCTION__);
				return ulReturn;
			}
		}
		else
		{
			if(pInmSearchResult)
			{
				ldap_msgfree(pInmSearchResult);
				pInmSearchResult = NULL;
			}

			cout<<"Error encountered while searching directory server."<<endl;
			cout<<"ldap_search_s() failed ..."<<endl
				<<"[ERROR] : "<<ldap_err2string(ulReturn)<<endl;
			DebugPrintf(SV_LOG_DEBUG,"Line(%u): Error %s: Error encountered while searching directory server. ldap_search_s() failed. %s.\n",__LINE__,__FUNCTION__,ldap_err2string(ulReturn));
			return ulReturn;
		}

		//searchCount++;
	}while(1);
	pInmEntry = ldap_first_entry(m_pLDAPConnection,pInmSearchResult);
	if(NULL == pInmEntry)
	{
		DebugPrintf(SV_LOG_DEBUG,"Line(%u): Error %s: No entries found or ldap_first_entry() failed .\n",__LINE__,__FUNCTION__);
		return LDAP_SUCCESS;
	}
	while(NULL != pInmEntry)
	{
		MDBCopyDN = ldap_get_dn(m_pLDAPConnection,pInmEntry);
		MDBDN = MDBCopyDN.substr(MDBCopyDN.find(",")+1);
		pInmAttribute1 = ldap_first_attribute(m_pLDAPConnection,pInmEntry,&pInmBer1);
		while(NULL != pInmAttribute1)
		{
			if(0 == strcmp(pInmAttribute1,"msExchHostServerLink"))
			{
				inm_val1 = ldap_get_values(m_pLDAPConnection,pInmEntry,pInmAttribute1);
				HostServerLink = (string)inm_val1[0];//Here the value returned by the above function is multi-valued string.
				ldap_value_free(inm_val1);
			}
			ldap_memfree(pInmAttribute1);
			pInmAttribute1 = ldap_next_attribute(m_pLDAPConnection,pInmEntry,pInmBer1);
		}
		MDBCopySearchResult.insert(make_pair(MDBDN,HostServerLink));
		ber_free(pInmBer1,0);
		pInmEntry = ldap_next_entry(m_pLDAPConnection,pInmEntry);
	}
	ldap_msgfree(pInmSearchResult);
	//'MDBCopy at Server' search ended...

	// Compare 'hostServerLink' value of MDBCopy and 'msExchOwningServer' value of MDB to determine active database copy.
	map<string, string>::iterator MDBCopyIter = MDBCopySearchResult.begin();
	while(MDBCopyIter != MDBCopySearchResult.end())
	{
		MDBDN = MDBCopyIter->first;
		HostServerLink = MDBCopyIter->second;

		MDBName.clear();
		                        // MDB entry search started...
		LDAPMessage *inm_res,*inm_entry;
		BerElement *pInmBer2;
		char* pInmAttribute2;
		berval** bInmVal;
		char** inm_val2;
		PCHAR MDBattrs[9];
		MDBattrs[0] = "cn";
		MDBattrs[1] = "msExchESEParamLogFilePath";
		MDBattrs[2] = "msExchEDBFile";
		MDBattrs[3] = "msExchOwningServer";
		MDBattrs[4] = "objectClass";
		MDBattrs[5] = "msExchESEParamBaseName";
		MDBattrs[6] = "legacyExchangeDN";
		MDBattrs[7] = "objectGUID";
		MDBattrs[8] = NULL;

		//searchCount = 0;
		do{
			ulReturn = ldap_search_s(m_pLDAPConnection,(char*)MDBDN.c_str(),LDAP_SCOPE_BASE,"(|(objectClass=msExchPrivateMDB)(objectClass=msExchPublicMDB))",MDBattrs,0,&inm_res);
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
				cout << "[ERROR]: ldap_search_s failed. "<< ldap_err2string(ulReturn) << endl;
				DebugPrintf(SV_LOG_DEBUG,"Line(%u): Error %s: ldap_search_s failed().\n",__LINE__,__FUNCTION__);
				if(inm_res)
				{
					ldap_msgfree(inm_res);
					inm_res = NULL;
				}

				//If same error hass repeated for MAX_SEARCH_ATTEMPTS times then stop the searach process
				/*if(searchCount >= MAX_SEARCH_ATTEMPTS)
				{
					cout<<"[INFO]: Maximum search attempts has reached."<<endl
						<<"Failed to search the directory server."<<endl;
					return ulReturn;
				}*/

				ulReturn = HandleConnectionError();
				if(LDAP_SUCCESS != ulReturn)
				{
					cout << "[INFO]: Maximum connection attempts " << MAX_LDAP_CONNECT_ATTEMPTS << " has been reached."<<endl;
					DebugPrintf(SV_LOG_DEBUG,"Line(%u): Info %s: Maximum connection attempts has been reached.\n",__LINE__,__FUNCTION__);
					cout << "[ERROR]: Unable to connect to directory server." << endl;
					DebugPrintf(SV_LOG_DEBUG,"Line(%u): Error %s: Unable to connect to directory server.\n",__LINE__,__FUNCTION__);
					return ulReturn;
				}
			}
			else
			{
				if(inm_res)
				{
					ldap_msgfree(inm_res);
					inm_res = NULL;
				}

				cout<<"Error encountered while searching directory server."<<endl;
				cout<<"ldap_search_s() failed ..."<<endl
					<<"[ERROR] : "<<ldap_err2string(ulReturn)<<endl;
				DebugPrintf(SV_LOG_DEBUG,"Line(%u): Error %s: Error encountered while searching directory server. ldap_search_s() failed. %s.\n",__LINE__,__FUNCTION__,ldap_err2string(ulReturn));
				return ulReturn;
			}

			//searchCount++;
		}while(1);

		inm_entry = ldap_first_entry(m_pLDAPConnection,inm_res);
		if(NULL == inm_entry)
		{
			cout<<"[ERROR] : Failed to get MDB info"<<endl;
			DebugPrintf(SV_LOG_DEBUG,"Line(%u): Error %s: No entries found or ldap_first_entry() failed .\n",__LINE__,__FUNCTION__);
		}
		else
		{
			MDBCopyInfo info;
			BOOL bActive = FALSE;
			BOOL bPublicMDBCopy = FALSE;
			string mdbPath,logPath,OwningServerName,objectClass,logBaseName,legacyExchangeDN,objectGUID;
			pInmAttribute2 = ldap_first_attribute(m_pLDAPConnection,inm_entry,&pInmBer2);
			while(pInmAttribute2 != NULL)
			{
				if(strcmp(pInmAttribute2,MDBattrs[0]) == 0)
				{
					inm_val2 = ldap_get_values(m_pLDAPConnection,inm_entry,pInmAttribute2);
					for(int i=0;inm_val2[i] != NULL;i++)
						MDBName = (string)inm_val2[i];
					ldap_value_free(inm_val2);
				}
				else if(strcmp(pInmAttribute2,MDBattrs[1]) == 0)
				{
					inm_val2 = ldap_get_values(m_pLDAPConnection,inm_entry,pInmAttribute2);
					for(int i=0;inm_val2[i] != NULL;i++)
						logPath = (string)inm_val2[i];
					ldap_value_free(inm_val2);
				}
				else if(strcmp(pInmAttribute2,MDBattrs[2]) == 0)
				{
					inm_val2 = ldap_get_values(m_pLDAPConnection,inm_entry,pInmAttribute2);
					for(int i=0;inm_val2[i] != NULL;i++)
						mdbPath = (string)inm_val2[i];
					ldap_value_free(inm_val2);
				}
				else if(strcmp(pInmAttribute2,MDBattrs[3]) == 0)
				{
					OwningServerName.clear();
					inm_val2 = ldap_get_values(m_pLDAPConnection,inm_entry,pInmAttribute2);
					for(int i=0;inm_val2[i] != NULL;i++)
						OwningServerName = (string)inm_val2[i];
					if(HostServerLink.compare(OwningServerName) == 0)
						bActive = TRUE;
					ldap_value_free(inm_val2);
				}
				else if(strcmp(pInmAttribute2,MDBattrs[4]) == 0)
				{
					bPublicMDBCopy = FALSE; // default-value (i.e FALSE) is private MDB
					inm_val2 = ldap_get_values(m_pLDAPConnection,inm_entry,pInmAttribute2);
					for(int i=0;inm_val2[i] != NULL;i++)// objectClass is a multi-valued attribute
					{
						objectClass.clear();
						objectClass = string(inm_val2[i]);
						if(objectClass.compare("msExchPublicMDB") == 0)//if it is a publicMDBCopy then change the value to TRUE
						{
							bPublicMDBCopy = TRUE;
							break;
						}
					}							
					ldap_value_free(inm_val2);
				}
				else if(strcmp(pInmAttribute2,MDBattrs[5]) == 0)
				{
					inm_val2 = ldap_get_values(m_pLDAPConnection,inm_entry,pInmAttribute2);
					for(int i=0;inm_val2[i] != NULL;i++)
						logBaseName = (string)inm_val2[i];
					ldap_value_free(inm_val2);
				}
				else if(strcmp(pInmAttribute2,MDBattrs[6]) == 0)
				{
					inm_val2 = ldap_get_values(m_pLDAPConnection,inm_entry,pInmAttribute2);
					for(int i=0;inm_val2[i] != NULL;i++)
						legacyExchangeDN = (string)inm_val2[i];
					ldap_value_free(inm_val2);
				}
				else if(strcmp(pInmAttribute2,MDBattrs[7]) == 0)
				{
					bInmVal = ldap_get_values_len(m_pLDAPConnection,inm_entry,pInmAttribute2);
					for(int i=0; bInmVal[i] != NULL; i++)
					{
						LPWSTR szwGUID = new WCHAR[39];
						LPGUID pGUID = (LPGUID)bInmVal[i]->bv_val;
						::StringFromGUID2(*pGUID,szwGUID,39);
						string strGUID(W2T(szwGUID));
						//Remove the first '{' and last '}' characters from GUID
						objectGUID = ToLower(strGUID.substr(1,strGUID.length()-2));
						delete szwGUID;
					}
					ldap_value_free_len(bInmVal);
				}
				ldap_memfree(pInmAttribute2);
				pInmAttribute2 = ldap_next_attribute(m_pLDAPConnection,inm_entry,pInmBer2);
			}
			
			info.bActive = bActive;
			info.mdbDrive = mdbPath;
			info.logDrive = logPath;
			info.objectGUID = objectGUID;
			info.logBaseName = logBaseName;
			info.bPublicMDBCopy = bPublicMDBCopy;
			info.legacyExchangeDN = legacyExchangeDN;
			info.mdbDN = string(ldap_get_dn(m_pLDAPConnection,inm_entry));

			MDBs.insert(make_pair(MDBName,info));
			bActive = FALSE;mdbPath.clear();logPath.clear();ber_free(pInmBer2,0);
		}
		ldap_msgfree(inm_res);
		// 'MDB inm_entry' search ended...
		MDBCopyIter++;
	}
	DebugPrintf(SV_LOG_DEBUG,"Line(%u): END %s\n",__LINE__,__FUNCTION__);
	return LDAP_SUCCESS;
}
string LDAPConnection::getAgentInstallationPath()
{
	DebugPrintf(SV_LOG_DEBUG,"Line(%u): BEGIN %s\n",__LINE__,__FUNCTION__);
	LocalConfigurator VXconfig;
	DebugPrintf(SV_LOG_DEBUG,"Line(%u): END %s\n",__LINE__,__FUNCTION__);
	return VXconfig.getInstallPath();
}
// Returns Exchange Version
string LDAPConnection::getExchangeVersion(const string svrName)
{
	DebugPrintf(SV_LOG_DEBUG,"Line(%u): BEGIN %s\n",__LINE__,__FUNCTION__);
	if(NULL == m_pLDAPConnection)
	{
		cout<<"[ERROR]: Conection is not established..."<<endl;
		DebugPrintf(SV_LOG_DEBUG,"Line(%u): Error %s: Conection is not established.\n",__LINE__,__FUNCTION__);
		return string("");
	}

	ULONG ulReturn = 0;
	LDAPMessage *pInmSearchResult,*pInmEntry;
	BerElement *pInmBerEle;
	char* pInmAttribute;
	char** inm_val;

	char* inm_req_attribute[3];	
	inm_req_attribute[0] = "serialNumber";
	inm_req_attribute[1] = NULL;

	string configDN;
	string filter,serialNumber;
	filter = "(&(objectclass=msExchExchangeServer)(cn="+ToUpper(svrName,svrName.length())+"))";
	if(m_configDN.empty())
	{
		ulReturn = GetconfigurationDN();
		if(LDAP_SUCCESS != ulReturn)
			return string("");
	}
	//int searchCount = 0;
	do{
		ulReturn = ldap_search_s(m_pLDAPConnection,(PCHAR)m_configDN.c_str(),LDAP_SCOPE_SUBTREE,(PCHAR)filter.c_str(),inm_req_attribute,0,&pInmSearchResult);
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
			cout << "[ERROR]: ldap_search_s failed. "<< ldap_err2string(ulReturn) << endl;
			DebugPrintf(SV_LOG_DEBUG,"Line(%u): Error %s: ldap_search_s failed().\n",__LINE__,__FUNCTION__);
			if(pInmSearchResult)
			{
				ldap_msgfree(pInmSearchResult);
				pInmSearchResult = NULL;
			}

			//If same error hass repeated for MAX_SEARCH_ATTEMPTS times then stop the searach process
			/*if(searchCount >= MAX_SEARCH_ATTEMPTS)
			{
				cout<<"[INFO]: Maximum search attempts has reached."<<endl
					<<"Failed to search the directory server."<<endl;
				return string("");
			}*/
			ulReturn = HandleConnectionError();
			if(LDAP_SUCCESS != ulReturn)
			{
				cout << "[INFO]: Maximum connection attempts " << MAX_LDAP_CONNECT_ATTEMPTS << " has been reached."<<endl;
				DebugPrintf(SV_LOG_DEBUG,"Line(%u): Info %s: Maximum connection attempts has been reached.\n",__LINE__,__FUNCTION__);
				cout << "[ERROR]: Unable to connect to directory server." << endl;
				DebugPrintf(SV_LOG_DEBUG,"Line(%u): Error %s: Unable to connect to directory server.\n",__LINE__,__FUNCTION__);
				return string("");
			}
		}
		else
		{
			if(pInmSearchResult)
			{
				ldap_msgfree(pInmSearchResult);
				pInmSearchResult = NULL;
			}

			cout<<"Error encountered while searching directory server."<<endl;
			cout<<"ldap_search_s() failed ..."<<endl
				<<"[ERROR] : "<<ldap_err2string(ulReturn)<<endl;
			DebugPrintf(SV_LOG_DEBUG,"Line(%u): Error %s: Error encountered while searching directory server. ldap_search_s() failed. %s.\n",__LINE__,__FUNCTION__,ldap_err2string(ulReturn));
			return string("");
		}

		//searchCount++;
	}while(1);

	ULONG entryCount = ldap_count_entries(m_pLDAPConnection,pInmSearchResult);
	if(entryCount <= 0)
	{
		DebugPrintf(SV_LOG_DEBUG,"Line(%u): Error %s: No entries found.\n",__LINE__,__FUNCTION__);
		return string("");
	}

	pInmEntry = ldap_first_entry(m_pLDAPConnection,pInmSearchResult);
	if(NULL == pInmEntry)
	{
		ldap_msgfree(pInmSearchResult);
		cout<<"ldap_first_entry() failed ..."<<endl
			<<"[ERROR] : "<<ldap_err2string(ulReturn)<<endl;
		DebugPrintf(SV_LOG_DEBUG,"Line(%u): Error %s: No entries found or ldap_first_entry() failed .\n",__LINE__,__FUNCTION__);
		return string("");
	}
	pInmAttribute = ldap_first_attribute(m_pLDAPConnection,pInmEntry,&pInmBerEle);
	while(NULL != pInmAttribute)
	{
		inm_val = ldap_get_values(m_pLDAPConnection,pInmEntry,pInmAttribute);
		if(strcmp(pInmAttribute,"serialNumber") == 0)
			for(int i=0;inm_val[i] != NULL;i++)
				serialNumber = string(inm_val[i]);
		ldap_value_free(inm_val);
		ldap_memfree(pInmAttribute);
		pInmAttribute = ldap_next_attribute(m_pLDAPConnection,pInmEntry,pInmBerEle);
	}
	ber_free(pInmBerEle,0);

	ldap_msgfree(pInmSearchResult);

	if(serialNumber.find("Version 14.") != string::npos)
		return string("Exchange2010");
	else if(serialNumber.find("Version 8.") != string::npos)
		return string("Exchange2007");
	else if(serialNumber.find("Version 6.5") != string::npos)
		return string("Exchange2003");

	//Our product supports Exchange 2003,Exchange 2007 & Exchange 2010 only. Other versions are out of our product scope.
	//So all other version of exchange are treating as "Exchange Unknown"
	DebugPrintf(SV_LOG_DEBUG,"Line(%u): END %s\n",__LINE__,__FUNCTION__);
	return string("Exchange Unknown");
}

// Retrieves the resopnsible MTA server of the specified server
ULONG LDAPConnection::GetResponsibleMTAServer(const string& svrName, string& mtaServer)
{
	DebugPrintf(SV_LOG_DEBUG,"Line(%u): BEGIN %s\n",__LINE__,__FUNCTION__);
	if(NULL == m_pLDAPConnection)
	{
		cout<<"[ERROR]: Conection is not established..."<<endl;
		DebugPrintf(SV_LOG_DEBUG,"Line(%u): Error %s: Conection is not established.\n",__LINE__,__FUNCTION__);
		return SVE_FAIL;
	}
	if(svrName.empty())
	{
		cout << "[ERROR]: The specified Exchange server name is empty" << endl;
		DebugPrintf(SV_LOG_DEBUG,"Line(%u): Warning %s: No Exchange server name specified.\n",__LINE__,__FUNCTION__);
		return SVE_FAIL;
	}
	ULONG ulReturn;
	if(m_configDN.empty())
	{
		ulReturn = GetconfigurationDN();
		if(LDAP_SUCCESS != ulReturn)
			return ulReturn;
	}

	LDAPMessage* pInmSearchResult = NULL;
	
	string szFilter;

	PCHAR pAttributes[2] = { "msExchResponsibleMTAServer", NULL };
	PCHAR pAttrMTAServer = "msExchResponsibleMTAServer";
	szFilter = "(&(cn=" + svrName + ")(objectClass=msExchExchangeServer))";	

	//int searchCount = 0;
	do{
		ulReturn = ldap_search_s(m_pLDAPConnection,(PCHAR)m_configDN.c_str(),LDAP_SCOPE_SUBTREE,(PCHAR)szFilter.c_str(), pAttributes, 0, &pInmSearchResult);
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
			cout << "[ERROR]: ldap_search_s failed. "<< ldap_err2string(ulReturn) << endl;
			DebugPrintf(SV_LOG_DEBUG,"Line(%u): Error %s: ldap_search_s failed().\n",__LINE__,__FUNCTION__);
			if(pInmSearchResult)
			{
				ldap_msgfree(pInmSearchResult);
				pInmSearchResult = NULL;
			}

			//If same error hass repeated for MAX_SEARCH_ATTEMPTS times then stop the searach process
			/*if(searchCount >= MAX_SEARCH_ATTEMPTS)
			{
				cout<<"[INFO]: Maximum search attempts has reached."<<endl
					<<"Failed to search the directory server."<<endl;
				return ulReturn;
			}*/
			ulReturn = HandleConnectionError();
			if(LDAP_SUCCESS != ulReturn)
			{
				cout << "[INFO]: Maximum connection attempts " << MAX_LDAP_CONNECT_ATTEMPTS << " has been reached."<<endl;
				DebugPrintf(SV_LOG_DEBUG,"Line(%u): Info %s: Maximum connection attempts has been reached.\n",__LINE__,__FUNCTION__);
				cout << "[ERROR]: Unable to connect to directory server." << endl;
				DebugPrintf(SV_LOG_DEBUG,"Line(%u): Error %s: Unable to connect to directory server.\n",__LINE__,__FUNCTION__);
				return ulReturn;
			}
		}
		else
		{
			if(pInmSearchResult)
			{
				ldap_msgfree(pInmSearchResult);
				pInmSearchResult = NULL;
			}

			cout<<"Error encountered while searching directory server."<<endl;
			cout<<"ldap_search_s() failed ..."<<endl
				<<"[ERROR] : "<<ldap_err2string(ulReturn)<<endl;
			DebugPrintf(SV_LOG_DEBUG,"Line(%u): Error %s: Error encountered while searching directory server. ldap_search_s() failed. %s.\n",__LINE__,__FUNCTION__,ldap_err2string(ulReturn));
			return ulReturn;
		}

		//searchCount++;
	}while(1);

	if(NULL != pInmSearchResult)
	{
		ULONG ulEntryCount = 0;
		ulEntryCount = ldap_count_entries(m_pLDAPConnection, pInmSearchResult);
        if( ulEntryCount > 0 )
		{
            PCHAR *value_inm = ldap_get_values(m_pLDAPConnection, pInmSearchResult, pAttrMTAServer);		
			string inm_temp = *value_inm;
			size_t start, end;
			start = inm_temp.find_first_of(',',0) + strlen(",CN=");
			end = inm_temp.find_first_of(',',start) - 1;
			mtaServer.assign(inm_temp, start, end - start + 1);				
		}		
		ldap_msgfree(pInmSearchResult);		
	}	
	DebugPrintf(SV_LOG_DEBUG,"Line(%u): END %s\n",__LINE__,__FUNCTION__);
	return LDAP_SUCCESS;
}

// Returns the list of values for a particular attribute of entry, which matches the specified filter
ULONG LDAPConnection::GetAttributeValues(list<string>& values,
										 string Attribute,
										 string Filter,
										 string baseDN)
{
	DebugPrintf(SV_LOG_DEBUG,"Line(%u): BEGIN %s\n",__LINE__,__FUNCTION__);
	USES_CONVERSION; // Required here to conver wstring to string
	values.clear();
	if(NULL == m_pLDAPConnection)
	{
		cout<<"[ERROR]: Conection is not established..."<<endl;
		DebugPrintf(SV_LOG_DEBUG,"Line(%u): Error %s: Conection is not established.\n",__LINE__,__FUNCTION__);
		return LDAP_CONNECT_ERROR;
	}

	ULONG ulReturn = 0;
	LDAPMessage *pInmSearchResult,*pInmEntry;
	BerElement *pInmBerEle;
	char* pInmAttribute;
	berval** bInmVal;
	char** inm_val;

	PCHAR req_attrs[2];
	req_attrs[0] = (PCHAR)Attribute.c_str();
	req_attrs[1] = NULL;

	string base;
	if(baseDN.empty())
	{
		if(m_configDN.empty())
		{
			ulReturn = GetconfigurationDN();
			if(LDAP_SUCCESS != ulReturn)
				return ulReturn;
		}
		base = m_configDN;
	}
	else
		base = baseDN;
	//int searchCount = 0;
	do{
		ulReturn = ldap_search_s(m_pLDAPConnection,(PCHAR)base.c_str(),LDAP_SCOPE_SUBTREE,(PCHAR)Filter.c_str(),req_attrs,0,&pInmSearchResult);
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
			cout << "[ERROR]: ldap_search_s failed. "<< ldap_err2string(ulReturn) << endl;
			DebugPrintf(SV_LOG_DEBUG,"Line(%u): Error %s: ldap_search_s failed().\n",__LINE__,__FUNCTION__);
			if(pInmSearchResult)
			{
				ldap_msgfree(pInmSearchResult);
				pInmSearchResult = NULL;
			}

			//If same error hass repeated for MAX_SEARCH_ATTEMPTS times then stop the searach process
			/*if(searchCount >= MAX_SEARCH_ATTEMPTS)
			{
				cout<<"[INFO]: Maximum search attempts has reached."<<endl
					<<"Failed to search the directory server."<<endl;
				return ulReturn;
			}*/
			ulReturn = HandleConnectionError();
			if(LDAP_SUCCESS != ulReturn)
			{
				cout << "[INFO]: Maximum connection attempts " << MAX_LDAP_CONNECT_ATTEMPTS << " has been reached."<<endl;
				DebugPrintf(SV_LOG_DEBUG,"Line(%u): Info %s: Maximum connection attempts has been reached.\n",__LINE__,__FUNCTION__);
				cout << "[ERROR]: Unable to connect to directory server." << endl;
				DebugPrintf(SV_LOG_DEBUG,"Line(%u): Error %s: Unable to connect to directory server.\n",__LINE__,__FUNCTION__);
				return ulReturn;
			}
		}
		else
		{
			if(pInmSearchResult)
			{
				ldap_msgfree(pInmSearchResult);
				pInmSearchResult = NULL;
			}

			cout<<"Error encountered while searching directory server."<<endl;
			cout<<"ldap_search_s() failed ..."<<endl
				<<"[ERROR] : "<<ldap_err2string(ulReturn)<<endl;
			DebugPrintf(SV_LOG_DEBUG,"Line(%u): Error %s: Error encountered while searching directory server. ldap_search_s() failed. %s.\n",__LINE__,__FUNCTION__,ldap_err2string(ulReturn));
			return ulReturn;
		}

		//searchCount++;
	}while(1);

	ULONG entryCount = ldap_count_entries(m_pLDAPConnection,pInmSearchResult);
	if(entryCount <= 0)
	{
		DebugPrintf(SV_LOG_DEBUG,"Line(%u): Error %s: No entries found.\n",__LINE__,__FUNCTION__);
		return LDAP_NO_SUCH_OBJECT;
	}

	pInmEntry = ldap_first_entry(m_pLDAPConnection,pInmSearchResult);
	if(NULL == pInmEntry)
	{
		ulReturn = GetLastError();
		ldap_msgfree(pInmSearchResult);
		cout<<"ldap_first_entry() failed ..."<<endl
			<<"[ERROR] : "<<ldap_err2string(ulReturn)<<endl;
		DebugPrintf(SV_LOG_DEBUG,"Line(%u): Error %s: No entries found or ldap_first_entry() failed .\n",__LINE__,__FUNCTION__);
		return ulReturn;
	}
	while(pInmEntry != NULL)
	{
		pInmAttribute = ldap_first_attribute(m_pLDAPConnection,pInmEntry,&pInmBerEle);
		while(NULL != pInmAttribute)
		{
			if(strcmpi("objectGUID",pInmAttribute) == 0)
			{
				bInmVal = ldap_get_values_len(m_pLDAPConnection,pInmEntry,pInmAttribute);
				for(int i=0; bInmVal[i] != NULL; i++)
				{
					LPWSTR szwInmGUID = new WCHAR[39];
					LPGUID pGUID = (LPGUID)bInmVal[i]->bv_val;
					::StringFromGUID2(*pGUID,szwInmGUID,39);
					string strGUID(W2T(szwInmGUID));
					//Remove the first '{' and last '}' characters from GUID
					strGUID = ToLower(strGUID.substr(1,strGUID.length()-2));
					values.push_back(strGUID);
					delete szwInmGUID;
				}
				ldap_value_free_len(bInmVal);
			}
			else
			{
				inm_val = ldap_get_values(m_pLDAPConnection,pInmEntry,pInmAttribute);
					for(int i=0;inm_val[i] != NULL;i++)
						values.push_back(string(inm_val[i]));
				ldap_value_free(inm_val);
			}
			ldap_memfree(pInmAttribute);
			pInmAttribute = ldap_next_attribute(m_pLDAPConnection,pInmEntry,pInmBerEle);
		}
		ber_free(pInmBerEle,0);
		pInmEntry = ldap_next_entry(m_pLDAPConnection,pInmEntry);
	}
	ldap_msgfree(pInmSearchResult);
	DebugPrintf(SV_LOG_DEBUG,"Line(%u): END %s\n",__LINE__,__FUNCTION__);
	return LDAP_SUCCESS;
}
//User Migration to Target the server
SVERROR LDAPConnection::failoverExchangeServerUsers(const string& sourceExchangeServerName,
													const string& destinationExchangeServerName, 
												    bool bAutoRefineFilters,
													bool bDryRun,
													bool bFailover,
													ULONG& nUsersMigrated,
													bool bSldRoot)
{
	DebugPrintf(SV_LOG_DEBUG,"Line(%u): BEGIN %s\n",__LINE__,__FUNCTION__);
	if(strcmpi(sourceExchangeServerName.c_str(), destinationExchangeServerName.c_str()) == 0)
	{
		cout << "[INFO]: Skipping user migration as source & target servers specified are same.\n";
		return SVS_OK;
	}
	
	//Close the existing connection and connect to the rootdomain. bug#14371
	LDAP *pOldConnection= m_pLDAPConnection;	//Save existing connection.
	string oldBaseDN	= m_baseDN;				//Maintain the existing connection variables to restore.it.
	string oldHostName	= getHost();
	string rootDomainName = bSldRoot ? "" : getRootDomainName();
	bool isNewConnection = false;
	if(!rootDomainName.empty())
	{
		m_pLDAPConnection	= NULL;					//Initialize the pointer to null.
		isNewConnection = true;
		if(EstablishConnection(rootDomainName) != LDAP_SUCCESS)//go for new connection to root domain.
		{
			//Restore old connection
			m_pLDAPConnection = pOldConnection;
			m_baseDN = oldBaseDN;
			setHost(oldHostName);
			//return SVE_FAIL;
			isNewConnection = false;
		}
		else if(LDAP_SUCCESS != Bind(""))
		{
			cout<<"Unable to connect to the directory server."<<endl;
			CloseConnection();// Clear the connection object
			//Restore old connection
			m_pLDAPConnection = pOldConnection;
			m_baseDN = oldBaseDN;
			setHost(oldHostName);
			DebugPrintf(SV_LOG_DEBUG,"Line(%u): Error %s: Unable to connect to the directory server.\n",__LINE__,__FUNCTION__);
			//return SVE_FAIL;
			isNewConnection = false;
		}
		else if(NULL == m_pLDAPConnection)
		{
			cout<<"[ERROR]: Conection is not established..."<<endl;
			DebugPrintf(SV_LOG_DEBUG,"Line(%u): Error %s: Conection is not established.\n",__LINE__,__FUNCTION__);
			//Restore old connection
			m_pLDAPConnection = pOldConnection;
			m_baseDN = oldBaseDN;
			setHost(oldHostName);
			//return LDAP_CONNECT_ERROR;
			isNewConnection = false;
		}

		if(!isNewConnection)
		{
			cout<< "[WARNING]: Could not connect to the domain " << rootDomainName << endl
				<< "If the exchange server have mailboxes for remote domain users then those users will not migrate."<< endl;
		}
	}
	
	list <string> namingContexts;
	ULONG ulReturn = 0, option = 1;

    if (LDAP_SUCCESS != GetNamingcontexts(namingContexts)) 
	{
		cout<< "Could not get naming contexts" << endl;
		DebugPrintf(SV_LOG_DEBUG,"Line(%u): Error %s: Could not get naming conexts. GetNamingcontexts() failed.\n",__LINE__,__FUNCTION__);
		if(isNewConnection)
		{
			//Clear new connection
			CloseConnection();
			//Restore old connection
			m_pLDAPConnection = pOldConnection;
			m_baseDN = oldBaseDN;
			setHost(oldHostName);
		}
		return SVE_FAIL;
	}

    list <string>::const_iterator iter;
	ulReturn = ldap_set_option(m_pLDAPConnection,LDAP_OPT_REFERRALS,&option);
	if(LDAP_SUCCESS != ulReturn)
	{
		DebugPrintf( SV_LOG_ERROR, "ldap_set_option: %s\n", ldap_err2string( ulReturn ) );
		cout << "Could not set the option of LDAP_OPT_REFERRALS to ON." << endl
				<< "[" << ulReturn << "] " << ldap_err2string( ulReturn ) << endl;
	}

	 /////////////////////////////////////////////////////////////////////////////////////////////////////
	 ////Not required for Exchnage 2010. Bcz, we are not creating any new mailstore objects.			//
	 ////Basically msExchPatchMDB attribute is related to the GUID changes of Mailstore object.			//
	 //   /*for (iter=namingContexts.begin(); iter!=namingContexts.end(); ++iter)						//
	 //   {																								//
	 //       if(findPosIgnoreCase((*iter), "CN=Configuration") == 0)									//
	 //       {																							//
	 //           setMsExchPatchMdbFlag(destinationExchangeServerName, bDryRun);						//
	 //       }																							//
	 //   }*/																							//	
	 /////////////////////////////////////////////////////////////////////////////////////////////////////
	
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
			//cout << "Skipping the context for user search : " << *iter << endl;
			continue;
		}        
		
		//cout << "\nSearching for users in the context : " << *iter << endl;		
		
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
		    ULONG ulInmRet = LDAP_SUCCESS;
            PCHAR pInmFilter = (PCHAR) szFilter.c_str();

            PCHAR pInmAttributes[3];
		    pInmAttributes[0] = "homeMTA";
		    pInmAttributes[1] = "msExchHomeServerName";
		    pInmAttributes[2] = NULL;
		    /*pInmAttributes[3] = "homeMDB";*/

			int refinefilterflag = 0;
			bool bSearchErr = false, bConnectErr = false;
			LDAPSearch *pSearch = NULL;
			do
			{
				pSearch = ldap_search_init_page(m_pLDAPConnection,(const PCHAR) (*iter).c_str(),
							LDAP_SCOPE_SUBTREE,pInmFilter,pInmAttributes,0,0,0,0,0,0);
				if(NULL == pSearch)
					ulInmRet = LdapGetLastError();
				else
					ulInmRet = LDAP_SUCCESS;

				if( LDAP_SUCCESS ==  ulInmRet)
					break;
				if(LDAP_PARTIAL_RESULTS == ulInmRet || LDAP_SIZELIMIT_EXCEEDED == ulInmRet)
				{
					//cout << "partial/sizelimit exceeded...\n";
					refinefilterflag = 1;
					break;
				}
				else if( ulInmRet == LDAP_SERVER_DOWN || 
					ulInmRet == LDAP_UNAVAILABLE ||
					ulInmRet == LDAP_NO_MEMORY ||
					ulInmRet == LDAP_BUSY ||
					ulInmRet == LDAP_LOCAL_ERROR ||
					ulInmRet == LDAP_TIMEOUT ||
					ulInmRet == LDAP_OPERATIONS_ERROR ||
					ulInmRet == LDAP_TIMELIMIT_EXCEEDED)
				{
					cout << "[ERROR][" << ulInmRet << "]: ldap_search_init_page failed. " << ldap_err2string(ulInmRet) << endl;
					DebugPrintf(SV_LOG_DEBUG,"Line(%u): Error %s: ldap_search_s failed().\n",__LINE__,__FUNCTION__);

					if( LDAP_SUCCESS != HandleConnectionError() )
					{
						bConnectErr = true;
						break;	
					}
					else
					{
						cout << "Re-connected successfully...\n";
						DebugPrintf(SV_LOG_DEBUG,"Line(%u): Info %s: Re-connected successfully...\n",__LINE__,__FUNCTION__);
					}
				}				
				else
				{
					//DebugPrintf(SV_LOG_FATAL, "ldap_search_s failed with: 0x%0lx \n",ulInmRet);
					cout << "[FATAL ERROR][:" << ulInmRet << "]: ldap_search_init_page failed. " << ldap_err2string(ulInmRet) << endl;
					DebugPrintf(SV_LOG_DEBUG,"Line(%u): Error %s: ldap_search_init_page() failed..\n",__LINE__,__FUNCTION__);
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
				DebugPrintf(SV_LOG_DEBUG,"Line(%u): Info %s: Maximum connection attempts has been reached. Moving to next context, if any...\n",__LINE__,__FUNCTION__);
				cout << "[INFO]: Maximum connection attempts " << MAX_LDAP_CONNECT_ATTEMPTS << " has been reached...\n";
				cout << "[ERROR]: Unable to connect to directory server for context: " << *iter << endl;
                cout << "[INFO]: Moving to next context, if any\n";
				DebugPrintf(SV_LOG_DEBUG,"Line(%u): Error %s: Unable to connect to directory server for contex '%s'.\n",__LINE__,__FUNCTION__,*iter);
				break;	//search next context
			}
		
			if( bSearchErr )
			{
				cout << "[ERROR] Error encountered while searching directory server, moving to next context, if any\n";
				DebugPrintf(SV_LOG_DEBUG,"Line(%u): Error %s: Error encountered while searching directory server, moving to next context, if any.\n",__LINE__,__FUNCTION__);
				break;	//search next context
			}
			
			ULONG ulSearchPageErr = LDAP_SUCCESS;
			while(LDAP_SUCCESS == ulSearchPageErr)
			{
				l_timeval timeout;
				timeout.tv_sec = INM_LDAP_MAX_SEARCH_TIME_LIMT_SEC;
				ULONG ulTotalCount = 0;
				LDAPMessage* pInmSearchResult = NULL;
				ulSearchPageErr = ldap_get_next_page_s(m_pLDAPConnection,pSearch,
					&timeout,SEARCH_PAGE_SIZE,&ulTotalCount,&pInmSearchResult);
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
				numberOfEntries = ldap_count_entries(m_pLDAPConnection, pInmSearchResult);
				if(NULL == numberOfEntries)
				{
					DebugPrintf(SV_LOG_DEBUG,"Line(%u): Warning %s: No entries found or ldap_count_entries() failed .\n",__LINE__,__FUNCTION__);
					if(NULL != pInmSearchResult) 
					{
						ldap_msgfree(pInmSearchResult);
						pInmSearchResult = NULL;
					}
					continue;
				}
				//cout << "Found " << numberOfEntries << " users " << endl;

				// Loop through the search entries, get, and output the requested list of attributes and values.
				LDAPMessage* pInmEntry = NULL;
				char* sMsg;
				BerElement* pInmBerEle = NULL;
				PCHAR pInmAttribute = NULL;
				PCHAR* ppValue = NULL;
				ULONG iValue = 0;

				for(ULONG iCnt=0; iCnt < numberOfEntries; iCnt++ )
				{
					bool failoverUser = false;
					if( !iCnt ) 
					{
						pInmEntry = ldap_first_entry(m_pLDAPConnection, pInmSearchResult);
					}
					else
					{
						pInmEntry = ldap_next_entry(m_pLDAPConnection, pInmEntry);
					}

					// Output a status message.
					sMsg = (!iCnt ? "ldap_first_entry" : "ldap_next_entry");
					if( NULL == pInmEntry )
					{
						DebugPrintf(SV_LOG_DEBUG, "Line(%u): %s: %s failed with 0x%0lx \n",__LINE__,__FUNCTION__, sMsg, LdapGetLastError());
						cout << sMsg << "failed with:" << LdapGetLastError() << endl;
						if( NULL != pInmSearchResult)
						{
							ldap_msgfree(pInmSearchResult);
							pInmSearchResult = NULL;
						}
						continue;
					}

					PCHAR pEntryDN = ldap_get_dn(m_pLDAPConnection, pInmEntry);
					if( NULL == pEntryDN )
					{
						DebugPrintf(SV_LOG_FATAL, "Line(%u): %s: %s failed with 0x%0lx \n",__LINE__,__FUNCTION__, sMsg, LdapGetLastError());
						cout << sMsg << "failed with:" << LdapGetLastError() << endl;
						if( NULL != pInmSearchResult)
						{
							ldap_msgfree(pInmSearchResult);
							pInmSearchResult = NULL;
						}
						continue;
					}

					//cout << "\n*****Processing user: " << pEntryDN << endl;
					map <string, char**> attributes;
					map<string, berval** > attributesBin;
					char **pValues = NULL;
					string srcMTA, tgtMTA;

					//Initialize with appropriate MTA servers
					srcMTA = (this->srcMTAExchServerName.empty()) ? sourceExchangeServerName : this->srcMTAExchServerName;
					tgtMTA = (this->tgtMTAExchServerName.empty()) ? destinationExchangeServerName : this->tgtMTAExchServerName;

					// Get the first attribute name
					pInmAttribute = ldap_first_attribute(m_pLDAPConnection, pInmEntry, &pInmBerEle);

					while( NULL != pInmAttribute)
					{
						string szSearchString = "";
						ppValue = NULL;
						pValues = NULL;

						ppValue = ldap_get_values(m_pLDAPConnection, pInmEntry, pInmAttribute);
						if(NULL == ppValue)
						{
							cout << ": [NO ATTRIBUTE VALUE RETURNED]" << endl;
							DebugPrintf(SV_LOG_DEBUG,"Line(%u): Warning %s: No attribute value returned.\n",__LINE__,__FUNCTION__);
						}
						// Output the attribute values
						else
						{
							iValue = ldap_count_values(ppValue);
							if(!iValue)
							{
								cout << ": [BAD VALUE LIST]" << endl;
								DebugPrintf(SV_LOG_DEBUG,"Line(%u): Error %s: Bad Value-list.\n",__LINE__,__FUNCTION__);
							}
							else
							{
								// The attributes are not guaranteed to return in the same order as specified
								// attributes with no values are not returned
								if (0 == strcmp(pInmAttributes[0],pInmAttribute)) {//homeMTA
									szSearchString += *ppValue;
								}
								else if (0 == strcmp(pInmAttributes[1],pInmAttribute)) {//msExchhomeserverName
									szSearchString += *ppValue;
								}
								else {
									DebugPrintf(SV_LOG_DEBUG,"Line(%u): Warning %s: Unexpected attribute.\n",__LINE__,__FUNCTION__);
								}

								// if this user belongs to the source server, modify entry and save it as it needs to be migrated
								if(szSearchString.empty() == false &&
									((findPosIgnoreCase(szSearchString, sourceExchangeServerName) != string::npos) || 
									(findPosIgnoreCase(szSearchString, srcMTA) != string::npos)) ) 

								{
									//Now we can conform that the user-mailbox is in the MDB which we are failovering to target server
									failoverUser = true;

									pValues = (char **) calloc(2, sizeof(char *));
									string replacementDN;
									ULONG numReplaced;
									int pos;
									string serverNameFromEntry;
									string searchPpValue;
									if (findPosIgnoreCase(pInmAttribute, pInmAttributes[1]) == 0)//If msExchHomeServerName ...
									{	
										searchPpValue = ToLower(*ppValue, strlen(*ppValue));
										pos = searchPpValue.find(string("servers/cn="));
										serverNameFromEntry.assign(searchPpValue.c_str(), pos + strlen("servers/cn="), szSearchString.length());

										if(serverNameFromEntry.length() == sourceExchangeServerName.length())
										{
											if (switchCNValuesSpecial(sourceExchangeServerName, destinationExchangeServerName, *ppValue, replacementDN, numReplaced, "/").failed())									{
												DebugPrintf(SV_LOG_DEBUG,"Line(%u): Error %s: Could not failover to the new server.\n",__LINE__,__FUNCTION__);
												cout << "Could not switch over to the new server \n";
											}
											else {
												//cout << "Modified msExchHomeServerName Attribute Value to " << replacementDN.c_str() << endl;											
											}
										}
									}
									else // If homeMTA ...
									{
										if( findPosIgnoreCase(szSearchString, string(string("=") + srcMTA + ",")) != string::npos)
										{
											if (switchCNValues(srcMTA, tgtMTA, *ppValue, replacementDN, numReplaced, ",").failed())
											{
												DebugPrintf(SV_LOG_DEBUG,"Line(%u): Error %s: Could not failover to the new server.\n",__LINE__,__FUNCTION__);
												cout << "Could not switch over to the new server \n";
											}		
											else
											{
												//cout << "Modified homeMTA Value to " << replacementDN.c_str() << endl;
											}
										}
									}
                                    size_t valuelen;
                                    INM_SAFE_ARITHMETIC(valuelen = InmSafeInt<size_t>::Type(replacementDN.length())+1, INMAGE_EX(replacementDN.length()))
									pValues[0] = (char *)calloc(valuelen, sizeof(char));
									inm_strcpy_s(pValues[0],(replacementDN.length()+1),replacementDN.c_str());
									pValues[1] = NULL;
									attributes[pInmAttribute] = pValues;
								}
								ULONG z;
								for(z=1; z<iValue; z++)
								{
									DebugPrintf(SV_LOG_DEBUG,"Line(%u): Warning %s: Multiple attribute values detected. '%s'\n",__LINE__,__FUNCTION__,ppValue[z]);
									cerr << "Multiple attribute values detected:" << ppValue[z] << endl;
								}
							}
						}

						// Free memory.
						if(NULL != ppValue)  {
							ldap_value_free(ppValue);
						}
						ppValue = NULL;
						ldap_memfree(pInmAttribute);
	    		        
						// Get next attribute name.
						pInmAttribute = ldap_next_attribute(m_pLDAPConnection, pInmEntry, pInmBerEle);
					}

					// If any of the queried attributes were returned, replace the servername with the destination servername
					if (failoverUser)
					{
						if( bDryRun )
						{
							cout << "User marked for migration : " << pEntryDN << endl;
							uiDryrunMarkedUsers++;
						}
						else if (modifyADEntry(pEntryDN, attributes, attributesBin) == SVE_FAIL)
						{
							cout << "\n****** Failed to update user entry " << pEntryDN << endl;
							DebugPrintf(SV_LOG_DEBUG,"Line(%u): Error %s: Failed to update user entry '%s'.\n",__LINE__,__FUNCTION__,pEntryDN);
							uiNotMigrated++;
						}
						else
						{
							cout << "\n\t[MIGRATED]\t" << pEntryDN <<endl;
							uiMigrated++;
						}
					}
					else {
						cout << "User NOT marked for migration : " << pEntryDN << endl;
						DebugPrintf(SV_LOG_DEBUG,"Line(%u): Info %s: User '%s' not migrated for migration.\n",__LINE__,__FUNCTION__,pEntryDN);
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

					if( NULL != pInmBerEle ) {
						ber_free(pInmBerEle,0);
					}
					pInmBerEle = NULL;

					if (NULL != pEntryDN) {
						ldap_memfree(pEntryDN);
					}
					pEntryDN = NULL;

					if (NULL != ppValue)
						ldap_value_free(ppValue);
					ppValue = NULL;
				}

				if(NULL != pInmSearchResult) 
				{
					ldap_msgfree(pInmSearchResult);
					pInmSearchResult = NULL;
				}
			}
			
			ldap_search_abandon_page(m_pLDAPConnection,pSearch);
	    }
	}

	if( !bDryRun )
	{
		if( uiNotMigrated )
		{
			cout << "\n***** Failed to migrate a total of " << uiNotMigrated << " users\n\n";
			DebugPrintf(SV_LOG_DEBUG,"Line(%u): Error %s: Migration failed for %u the users.\n",__LINE__,__FUNCTION__,uiNotMigrated);
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
	//Assign nUsersMigrated to Total number of users migrated
	nUsersMigrated = uiMigrated;

	if(isNewConnection)
	{
		//Clear new connection
		CloseConnection();
		//Restore old connection
		m_pLDAPConnection = pOldConnection;
		m_baseDN = oldBaseDN;
		setHost(oldHostName);
	}

	//_strtime( tmpbuf2 );
	//cout << "Start Time: " << tmpbuf1 << " End Time: " << tmpbuf2 << endl;
	DebugPrintf(SV_LOG_DEBUG,"Line(%u): END %s\n",__LINE__,__FUNCTION__);
	return SVS_OK;
}
//
SVERROR LDAPConnection::switchCNValues(const string& search,
									   const string& replace,
									   const string& sourceDN,
									   string& replacementDN,
									   ULONG& numReplaced, string szToken) 
{
	DebugPrintf(SV_LOG_DEBUG,"Line(%u): BEGIN %s\n",__LINE__,__FUNCTION__);
	// @TODO: Fix errant replaces of LHS values. e.g: We will cause an error if a replacement server is called "CN" or "DC" etc.
	// @TODO: Fix case insensitive compares based on locale

    SVERROR rc = SVS_OK;
    ULONG ulSourceLength = (ULONG) sourceDN.length() + 1; //Length + null character

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
	DebugPrintf(SV_LOG_DEBUG,"Line(%u): END %s\n",__LINE__,__FUNCTION__);
	return rc;
}
//
SVERROR LDAPConnection::switchCNValuesNoSeparator(const string& search, 
								  const string& replace, 
								  const string& sourceDN, 
								  string& replacementDN)
{
	DebugPrintf(SV_LOG_DEBUG,"Line(%u): BEGIN %s\n",__LINE__,__FUNCTION__);
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
	DebugPrintf(SV_LOG_DEBUG,"Line(%u): END %s\n",__LINE__,__FUNCTION__);
	return SVS_OK;
}
//
SVERROR LDAPConnection::switchCNValuesSpecial(const string& search, 
							  const string& replace,
							  const string& sourceDN,
							  string& replacementDN,
							  ULONG& numReplaced,
							  string szToken) 
{
	DebugPrintf(SV_LOG_DEBUG,"Line(%u): BEGIN %s\n",__LINE__,__FUNCTION__);
	// @TODO: Fix errant replaces of LHS values. e.g: We will cause an error if a replacement server is called "CN" or "DC" etc.
	// @TODO: Fix case insensitive compares based on locale

    SVERROR rc = SVS_OK;
    ULONG ulSourceLength = (ULONG) sourceDN.length() + 1; //Length + null character

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
	DebugPrintf(SV_LOG_DEBUG,"Line(%u): END %s\n",__LINE__,__FUNCTION__);
	return rc;
}
//--------------------------------------------------------------------------------------------------
//This function sets the msExchPatchMDB attribute of a mailstore under the specified exchange server
//--------------------------------------------------------------------------------------------------
ULONG LDAPConnection::setMsExchPatchMdbFlag(const string szTargetServerName, bool bDryRun)
{
	DebugPrintf(SV_LOG_DEBUG,"Line(%u): BEGIN %s\n",__LINE__,__FUNCTION__);
	if(m_pLDAPConnection == NULL)
	{
		cout<<"[ERROR]: Conection is not established..."<<endl;
		DebugPrintf(SV_LOG_DEBUG,"Line(%u): Error %s: Conection is not established.\n",__LINE__,__FUNCTION__);
		return LDAP_CONNECT_ERROR;
	}
	ULONG ulReturn = LDAP_SUCCESS;
	LDAPMessage* pInmSearchResult;

    PCHAR pInmFilter = "(|(objectClass=msExchPrivateMDB)(objectClass=msExchPublicMDB))";
	PCHAR pInmAttributes[3];
    pInmAttributes[0] = "msExchPatchMDB";//reading of this attribute is not requred
	pInmAttributes[1] = "msExchOwningServer";
	pInmAttributes[2] = NULL;

	if(m_configDN.empty())
	{
		ulReturn = GetconfigurationDN();
		if(LDAP_SUCCESS != ulReturn)
			return ulReturn;
	}

	BerElement *pInmBerEle;
	char* pInmAttribute;
	char** inm_val;
	LDAPMessage* pInmEntry = NULL;

	//int searchCount = 0;
	do{
		ulReturn = ldap_search_s(m_pLDAPConnection,(PCHAR)m_configDN.c_str(),
					LDAP_SCOPE_SUBTREE,pInmFilter,pInmAttributes,0,&pInmSearchResult);
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
			cout << "[ERROR]: ldap_search_s failed. "<< ldap_err2string(ulReturn) << endl;
			DebugPrintf(SV_LOG_DEBUG,"Line(%u): Error %s: ldap_search_s failed().\n",__LINE__,__FUNCTION__);
			if(pInmSearchResult)
			{
				ldap_msgfree(pInmSearchResult);
				pInmSearchResult = NULL;
			}

			//If same error hass repeated for MAX_SEARCH_ATTEMPTS times then stop the searach process
			/*if(searchCount >= MAX_SEARCH_ATTEMPTS)
			{
				cout<<"[INFO]: Maximum search attempts has reached."<<endl
					<<"Failed to search the directory server."<<endl;
				return ulReturn;
			}*/
			ulReturn = HandleConnectionError();
			if(LDAP_SUCCESS != ulReturn)
			{
				cout << "[INFO]: Maximum connection attempts " << MAX_LDAP_CONNECT_ATTEMPTS << " has been reached."<<endl;
				DebugPrintf(SV_LOG_DEBUG,"Line(%u): Info %s: Maximum connection attempts has been reached.\n",__LINE__,__FUNCTION__);
				cout << "[ERROR]: Unable to connect to directory server." << endl;
				DebugPrintf(SV_LOG_DEBUG,"Line(%u): Error %s: Unable to connect to directory server.\n",__LINE__,__FUNCTION__);
				return ulReturn;
			}
		}
		else
		{
			if(pInmSearchResult)
			{
				ldap_msgfree(pInmSearchResult);
				pInmSearchResult = NULL;
			}

			cout<<"Error encountered while searching directory server."<<endl;
			cout<<"ldap_search_s() failed ..."<<endl
				<<"[ERROR] : "<<ldap_err2string(ulReturn)<<endl;
			DebugPrintf(SV_LOG_DEBUG,"Line(%u): Error %s: Error encountered while searching directory server. ldap_search_s() failed. %s.\n",__LINE__,__FUNCTION__,ldap_err2string(ulReturn));
			return ulReturn;
		}

		//searchCount++;
	}while(1);

	pInmEntry = ldap_first_entry(m_pLDAPConnection,pInmSearchResult);
	if(pInmEntry == NULL)
	{
		ldap_msgfree(pInmSearchResult);
		cout<<"ldap_first_entry() failed ..."<<endl
			<<"[ERROR] : "<<ldap_err2string(ulReturn)<<endl;
		DebugPrintf(SV_LOG_DEBUG,"Line(%u): Error %s: No entries found or ldap_first_entry() failed .\n",__LINE__,__FUNCTION__);
		return ulReturn;
	}
	while(pInmEntry != NULL)
	{
		PCHAR pMdbDN = ldap_get_dn(m_pLDAPConnection,pInmEntry);
		string owningSvr;
		pInmAttribute = ldap_first_attribute(m_pLDAPConnection,pInmEntry,&pInmBerEle);
		while(pInmAttribute != NULL)
		{
			inm_val = ldap_get_values(m_pLDAPConnection,pInmEntry,pInmAttribute);
			if(strcmp(pInmAttribute,pInmAttributes[1]) == 0)
				for(int i=0;inm_val[i] != NULL;i++)
					owningSvr = string(inm_val[i]);
			ldap_value_free(inm_val);
			ldap_memfree(pInmAttribute);
			pInmAttribute = ldap_next_attribute(m_pLDAPConnection,pInmEntry,pInmBerEle);
		}
		ber_free(pInmBerEle,0);
		pInmEntry = ldap_next_entry(m_pLDAPConnection,pInmEntry);
	

        string szTargetNameToMatch ="CN=";
        szTargetNameToMatch += szTargetServerName;
		szTargetNameToMatch += ",CN=Servers";

        //Search whether this is a target MDB that needs to be modified.
		if(findPosIgnoreCase(owningSvr, szTargetNameToMatch) == string::npos)
            continue;
		//if  dryrun specified, nothing will be modified.
        if (!bDryRun) 
			if (LDAP_SUCCESS == modifyPatchMDB(pMdbDN))
			{
				cout << "\n*****Changed PatchMDB settings for entry:"<< pMdbDN << endl;
			}
			else
			{
				cout << "\tFailed changing  PatchMDB settings for entry:"<< owningSvr << " Error:"<< ldap_err2string( LdapGetLastError())<<endl;
				DebugPrintf(SV_LOG_DEBUG,"Line(%u): Error %s: Failed to change the PatchMDB settings. modifyPatchMDB() failed.\n",__LINE__,__FUNCTION__);
			}
	}
	ldap_msgfree(pInmSearchResult);
	DebugPrintf(SV_LOG_DEBUG,"Line(%u): END %s\n",__LINE__,__FUNCTION__);
	return LDAP_SUCCESS;
}
//sets the  "msExchPatchMDB" atrribute to TRUE
ULONG LDAPConnection::modifyPatchMDB(PCHAR pMdbDN)
{
	DebugPrintf(SV_LOG_DEBUG,"Line(%u): BEGIN %s\n",__LINE__,__FUNCTION__);
	if(m_pLDAPConnection == NULL)
	{
		cout<<"[ERROR]: Conection is not established...\t"<<__FUNCTION__<<endl;
		DebugPrintf(SV_LOG_DEBUG,"Line(%u): Error %s: Conection is not established.\n",__LINE__,__FUNCTION__);
		return LDAP_CONNECT_ERROR;
	}
	ULONG ulReturn;
    LDAPMod modSourceAttribute;
	LDAPMod* changedAttributes[] = {&modSourceAttribute, NULL};
	char *replacementValues[2];

	// Set up the attributes to be modified
	modSourceAttribute.mod_op = LDAP_MOD_REPLACE;
    modSourceAttribute.mod_type = "msExchPatchMDB"; 
    
    replacementValues[0] = (PCHAR)"TRUE";
    replacementValues[1] = NULL;

    modSourceAttribute.mod_vals.modv_strvals = replacementValues;
	ulReturn = ldap_modify_s(m_pLDAPConnection, pMdbDN, changedAttributes);
	if(LDAP_SUCCESS != ulReturn)
	{
		DebugPrintf(SV_LOG_DEBUG,"Line(%u): Error %s: ldap_modify_s() failed . %s.\n",__LINE__,__FUNCTION__,ldap_err2string(ulReturn));
		return ulReturn;
	}
	DebugPrintf(SV_LOG_DEBUG,"Line(%u): END %s\n",__LINE__,__FUNCTION__);
    return LDAP_SUCCESS;
}
//
SVERROR LDAPConnection::searchAndReplaceToken(const string& search, 
						 const string& replace, 
						 const string& source, 
						 string& replacement,
						 ULONG& numReplaced)
{
	DebugPrintf(SV_LOG_DEBUG,"Line(%u): BEGIN %s\n",__LINE__,__FUNCTION__);
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
    inm_memcpy_s(szTmpToken,(source.length() + 1), source.c_str(), source.length() + 1);

    vector<char *> vToken;
    char *pToken = strtok(szTmpToken, " ()");

    string szSearchLower = ToLower(search, search.length());
    while(pToken) {

        string szTokenLower = ToLower(pToken, strlen(pToken));
        if(szTokenLower.compare(szSearchLower) == 0)
        {
            ulOffset = pToken - szTmpToken + lAdjustmentForLength; //Get the relative offset of this token inside target string
		    replacement.replace(ulOffset, search.length(), replace);
            lAdjustmentForLength += (long)(replace.length() - search.length()); //Adjust the length difference between replace and search strings
		    numReplaced++;
        }

        pToken = strtok(NULL, " ()");
	}
	DebugPrintf(SV_LOG_DEBUG,"Line(%u): END %s\n",__LINE__,__FUNCTION__);
	return rc;
}
ULONG LDAPConnection::addAttrValue(const string& entryDN,const string& attrName,const string& value)
{
	DebugPrintf(SV_LOG_DEBUG,"Line(%u): BEGIN %s\n",__LINE__,__FUNCTION__);
	LDAPMod newEntry;
	LDAPMod *Entry[] = {&newEntry,NULL};
	newEntry.mod_op = LDAP_MOD_ADD;
	newEntry.mod_type = (PCHAR)attrName.c_str();
	char *Mod_Vals[2];
    size_t valuelen;
    INM_SAFE_ARITHMETIC(valuelen = InmSafeInt<size_t>::Type(value.length())+1, INMAGE_EX(value.length()))
	Mod_Vals[0] = new char[valuelen];
	inm_strcpy_s(Mod_Vals[0],(value.length()+1),value.c_str());
	Mod_Vals[1] = NULL;
	newEntry.mod_vals.modv_strvals = Mod_Vals;

	ULONG ulReturn = ldap_modify_s(m_pLDAPConnection,(PCHAR)entryDN.c_str(),Entry);

	//free the memory
	delete Mod_Vals[0];

	if(LDAP_SUCCESS != ulReturn && LDAP_ATTRIBUTE_OR_VALUE_EXISTS != ulReturn)
	{
		cout<<"Error: ("<<ulReturn<<") "<<ldap_err2string(ulReturn)<<endl
			<<"Failed to update the attribute :"<< attrName <<endl;
		DebugPrintf(SV_LOG_DEBUG,"Line(%u): TERMINATED: ldap_modify_s() failed. %u: %s. %s\n",__LINE__,ulReturn,ldap_err2string(ulReturn),__FUNCTION__);
		return ulReturn;
	}

	DebugPrintf(SV_LOG_DEBUG,"Line(%u): END %s\n",__LINE__,__FUNCTION__);
	return ulReturn;
}
//Modifies the AD Entry specified...
SVERROR LDAPConnection::modifyADEntry(const string& entryDN,
								map<string, char**>& attrValuePairs,
								map<string, PBERVAL*>& attrValuePairsBin)
{
	DebugPrintf(SV_LOG_DEBUG,"Line(%u): BEGIN %s\n",__LINE__,__FUNCTION__);
	if(m_pLDAPConnection == NULL)
	{
		cout<<"[ERROR]: Conection is not established...\t"<<__FUNCTION__<<endl;
		DebugPrintf(SV_LOG_DEBUG,"Line(%u): Error %s: Conection is not established.\n",__LINE__,__FUNCTION__);
		return LDAP_CONNECT_ERROR;
	}

	LDAPMod *NewEntry[1024];
	LDAPMod *inm_entry;
	ULONG ulInmRet = LDAP_SUCCESS;
	size_t size;

	map<string, char**>::const_iterator iter = attrValuePairs.begin();
	map<string, berval**>::const_iterator iterBin = attrValuePairsBin.begin();

	int i=0;
	// iterate through non-binary attributes
	for (; iter != attrValuePairs.end(); iter++) {
		// skip the distinguished name as that should be set as the DN is created
	{
			inm_entry = (LDAPMod *) malloc(sizeof(LDAPMod));
			inm_entry->mod_op = LDAP_MOD_REPLACE;
			size = (*iter).first.length();
            size_t typelen;
            INM_SAFE_ARITHMETIC(typelen = InmSafeInt<size_t>::Type(size)+1, INMAGE_EX(size))
			inm_entry->mod_type = (char *)calloc(typelen, sizeof(char));
			inm_strncpy_s(inm_entry->mod_type, (size+1),(*iter).first.c_str(), size);
			inm_entry->mod_values = (*iter).second;
			NewEntry[i++] = inm_entry;
			//cout << "Adding Attribute = " << (char *)((*iter).first.c_str()) << endl;
		}
	}

	// initialize last entry to NULL
	NewEntry[i] = NULL;

	if (NewEntry[0] != NULL)
		ulInmRet = ldap_modify_s(m_pLDAPConnection, (const PCHAR) entryDN.c_str(), NewEntry);

	if (ulInmRet != LDAP_SUCCESS)
	{	
		cout << "Operation failed errorCode = "<< ulInmRet << endl;
		DebugPrintf(SV_LOG_DEBUG,"Line(%u): Error %s: ldap_modify_s() failed.\n",__LINE__,__FUNCTION__);
		return SVE_FAIL;
	}

	//cout << "\n***** Successfully Updated entry " << entryDN.c_str() << " to AD" << endl;

	for (int j=0; j < i; j++) {
		inm_entry = NewEntry[j]; 
		free(inm_entry->mod_type);
		free(inm_entry);
	}
	DebugPrintf(SV_LOG_DEBUG,"Line(%u): END %s\n",__LINE__,__FUNCTION__);
	return SVS_OK;
}
//Deletes the entries from AD, whose DNs are specified in the list
ULONG LDAPConnection::deleteEntryFromAD(const list<string>& DNs,bool bRemoveSubTree)
{
	DebugPrintf(SV_LOG_DEBUG,"Line(%u): BEGIN %s\n",__LINE__,__FUNCTION__);
	if(m_pLDAPConnection == NULL)
	{
		cout<<"[ERROR]: Conection is not established..."<<endl;
		DebugPrintf(SV_LOG_DEBUG,"Line(%u): Error %s: Conection is not established.\n",__LINE__,__FUNCTION__);
		return LDAP_CONNECT_ERROR;
	}
	ULONG ulReturn = LDAP_SUCCESS;

	LDAPControl ldap_control;
	ldap_control.ldctl_oid = LDAP_SERVER_TREE_DELETE_OID;
	ldap_control.ldctl_value.bv_len = 0;
	ldap_control.ldctl_value.bv_val = NULL;
	LDAPControl *pldap_control[] = {&ldap_control,NULL};

	list<string>::const_iterator iter = DNs.begin();
	for (; iter != DNs.end(); iter++) {
		//cout << "Deleting Entry DN = " << iter->c_str() << endl;
		if(bRemoveSubTree)
		{
			ulReturn = ldap_delete_ext_s(m_pLDAPConnection,(PCHAR)iter->c_str(),pldap_control,NULL);
			if (ulReturn != LDAP_SUCCESS)
			{
				cout << "Failed to delete : "<<iter->c_str()<<endl<<" Error: "<<ldap_err2string(ulReturn)<<endl;
				DebugPrintf(SV_LOG_DEBUG,"Line(%u): Error %s: ldap_delete_s() failed.\n",__LINE__,__FUNCTION__);
				return ulReturn;
			}
		}
		else
		{
			ulReturn = ldap_delete_s(m_pLDAPConnection, (PCHAR)iter->c_str());
			if (ulReturn != LDAP_SUCCESS)
			{
				cout << "Failed to delete : "<<iter->c_str()<<endl<<" Error: "<<ldap_err2string(ulReturn)<<endl;
				DebugPrintf(SV_LOG_DEBUG,"Line(%u): Error %s: ldap_delete_s() failed.\n",__LINE__,__FUNCTION__);
				return ulReturn;
			}
		}
	}
	DebugPrintf(SV_LOG_DEBUG,"Line(%u): END %s\n",__LINE__,__FUNCTION__);
	return ulReturn;
}
//
size_t LDAPConnection::findPosIgnoreCase(const string &szSource, const string &szSearch)
{
	DebugPrintf(SV_LOG_DEBUG,"Line(%u): BEGIN %s\n",__LINE__,__FUNCTION__);
    size_t st = ToLower(szSource, szSource.length()).find(ToLower(szSearch, szSearch.length()));
	DebugPrintf(SV_LOG_DEBUG,"Line(%u): END %s\n",__LINE__,__FUNCTION__);
    return st;
}
//
void LDAPConnection::InitiallizeFilter(bool bAutoRefineFilters)
{
	DebugPrintf(SV_LOG_DEBUG,"Line(%u): BEGIN %s\n",__LINE__,__FUNCTION__);
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
	DebugPrintf(SV_LOG_DEBUG,"Line(%u): END %s\n",__LINE__,__FUNCTION__);
}
//
void LDAPConnection::RefineFilter()
{
	DebugPrintf(SV_LOG_DEBUG,"Line(%u): BEGIN %s\n",__LINE__,__FUNCTION__);
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
	DebugPrintf(SV_LOG_DEBUG,"Line(%u): END %s\n",__LINE__,__FUNCTION__);
}
//
string LDAPConnection::getNextFilter()
{
	DebugPrintf(SV_LOG_DEBUG,"Line(%u): BEGIN %s\n",__LINE__,__FUNCTION__);
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
	DebugPrintf(SV_LOG_DEBUG,"Line(%u): END %s\n",__LINE__,__FUNCTION__);
    return szFilter;
}
// Retrieves the DAG(if DAG enabled)/Server DN of specified Exchange server
ULONG LDAPConnection::GetMasterServerOravailabilityGroupDN(const string &tgtSvrName,string& resultDN)
{
	DebugPrintf(SV_LOG_DEBUG,"Line(%u): BEGIN %s\n",__LINE__,__FUNCTION__);
	if(m_pLDAPConnection == NULL)
	{
		cout<<"[ERROR]: Conection is not established..."<<endl;
		DebugPrintf(SV_LOG_DEBUG,"Line(%u): Error %s: Conection is not established.\n",__LINE__,__FUNCTION__);
		return LDAP_CONNECT_ERROR;
	}
	if(tgtSvrName.empty())
	{
		cout<<"[WARNING]: TargetServerName specified is empty ...\n"
			<<"Unblae to retrieve the information form AD.";
		DebugPrintf(SV_LOG_DEBUG,"Line(%u): Warning %s: No Target Server Name specified.\n",__LINE__,__FUNCTION__);
		return LDAP_OPERATIONS_ERROR;
	}

	ULONG ulReturn = 0;

	if(m_configDN.empty())
	{
		ulReturn = GetconfigurationDN();
		if(LDAP_SUCCESS != ulReturn)
			return ulReturn;
	}

    LDAPMessage* pInmSearchResult;
	PCHAR* ppValue;
	PCHAR pInmAttribute;
	BerElement *pInmBerEle;
    string Filter = "(&(objectClass=msExchExchangeServer)(cn="+tgtSvrName+"))";
	PCHAR pInmAttributes[2];

	pInmAttributes[0] = MS_EXCH_MDB_AVAILABILITYGROUP_LINK;
    pInmAttributes[1] = NULL;
    
	//int searchCount = 0;
	do{
		ulReturn = ldap_search_s(m_pLDAPConnection,(PCHAR)m_configDN.c_str(),
						LDAP_SCOPE_SUBTREE,(PCHAR)Filter.c_str(),pInmAttributes,
						0,&pInmSearchResult);	
    
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
			cout << "[ERROR]: ldap_search_s failed. "<< ldap_err2string(ulReturn) << endl;
			DebugPrintf(SV_LOG_DEBUG,"Line(%u): Error %s: ldap_search_s failed().\n",__LINE__,__FUNCTION__);
			if(pInmSearchResult)
			{
				ldap_msgfree(pInmSearchResult);
				pInmSearchResult = NULL;
			}

			//If same error hass repeated for MAX_SEARCH_ATTEMPTS times then stop the searach process
			/*if(searchCount >= MAX_SEARCH_ATTEMPTS)
			{
				cout<<"[INFO]: Maximum search attempts has reached."<<endl
					<<"Failed to search the directory server."<<endl;
				return ulReturn;
			}*/
			ulReturn = HandleConnectionError();
			if(LDAP_SUCCESS != ulReturn)
			{
				cout << "[INFO]: Maximum connection attempts " << MAX_LDAP_CONNECT_ATTEMPTS << " has been reached."<<endl;
				DebugPrintf(SV_LOG_DEBUG,"Line(%u): Info %s: Maximum connection attempts has been reached.\n",__LINE__,__FUNCTION__);
				cout << "[ERROR]: Unable to connect to directory server." << endl;
				DebugPrintf(SV_LOG_DEBUG,"Line(%u): Error %s: Unable to connect to directory server.\n",__LINE__,__FUNCTION__);
				return ulReturn;
			}
		}
		else
		{
			if(pInmSearchResult)
			{
				ldap_msgfree(pInmSearchResult);
				pInmSearchResult = NULL;
			}

			cout<<"Error encountered while searching directory server."<<endl;
			cout<<"ldap_search_s() failed ..."<<endl
				<<"[ERROR] : "<<ldap_err2string(ulReturn)<<endl;
			DebugPrintf(SV_LOG_DEBUG,"Line(%u): Error %s: Error encountered while searching directory server. ldap_search_s() failed. %s.\n",__LINE__,__FUNCTION__,ldap_err2string(ulReturn));
			return ulReturn;
		}

		//searchCount++;
	}while(1);    
    //----------------------------------------------------------
    // Get the entries returned.
    //----------------------------------------------------------
    LDAPMessage *pInmEntry = ldap_first_entry(m_pLDAPConnection,pInmSearchResult);
	if(pInmEntry == NULL)
	{
		cout<<"[WRANING]: No Entries found for the server :"<<tgtSvrName<<endl;
		cout<<ldap_err2string(GetLastError());
		ldap_msgfree(pInmSearchResult);
		DebugPrintf(SV_LOG_DEBUG,"Line(%u): Error %s: No entries found or ldap_first_entry() failed .\n",__LINE__,__FUNCTION__);
		return LDAP_SUCCESS;
	}
	//---------------------------------------------------------------------------------------------
	//msExchMDBAvailabilityGroup attribute is set to the DN of the DAG that server points to.
	//If the attribute is not set, which means the server is not part of any DAG. So, in that case
	//we are sending the DN of the server. If the enabled then we will send the DAG DN.
	while(pInmEntry != NULL)
	{
		resultDN.clear();
		pInmAttribute = ldap_first_attribute(m_pLDAPConnection,pInmEntry,&pInmBerEle);
		while(pInmAttribute != NULL)
		{
			ppValue = ldap_get_values(m_pLDAPConnection,pInmEntry,pInmAttribute);
			if(strcmp(pInmAttribute,pInmAttributes[0]) == 0)
				resultDN = string(*ppValue);
			ldap_value_free(ppValue);
			pInmAttribute = ldap_next_attribute(m_pLDAPConnection,pInmEntry,pInmBerEle);
		}
		if(resultDN.empty())
			resultDN = string(ldap_get_dn(m_pLDAPConnection,pInmEntry));
		pInmEntry = ldap_next_entry(m_pLDAPConnection,pInmEntry);
	}
	ldap_msgfree(pInmSearchResult);
	DebugPrintf(SV_LOG_DEBUG,"Line(%u): END %s\n",__LINE__,__FUNCTION__);
	return ulReturn;
}

// Modifies the DN of specified object(oldRDN) with newRDN
ULONG LDAPConnection::modifyRDN(const string oldRDN, const string newRDN)
{
	DebugPrintf(SV_LOG_DEBUG,"Line(%u): BEGIN %s\n",__LINE__,__FUNCTION__);
	if(m_pLDAPConnection == NULL)
	{
		cout<<"[ERROR]: Conection is not established...\t"<<endl;
		DebugPrintf(SV_LOG_DEBUG,"Line(%u): Error %s: Conection is not established.\n",__LINE__,__FUNCTION__);
		return LDAP_CONNECT_ERROR;
	}

	ULONG ulReturn;
	ulReturn = ldap_modrdn2_s(m_pLDAPConnection,(const PCHAR)oldRDN.c_str(),(const PCHAR)newRDN.c_str(),TRUE);
	if(ulReturn != LDAP_SUCCESS)
	{
		cout<<"Failed to Modify the RDN ..."<<endl;
		cout<<"HINT: "<<ldap_err2string(ulReturn)<<endl;
		DebugPrintf(SV_LOG_DEBUG,"Line(%u): Error %s: Failed to modify the RDN. ldap_modrdn2_s() failed . %s.\n",__LINE__,__FUNCTION__,ldap_err2string(ulReturn));
		return ulReturn;
	}
	DebugPrintf(SV_LOG_DEBUG,"Line(%u): END %s\n",__LINE__,__FUNCTION__);
	return LDAP_SUCCESS;
}

// Gathers the list of OABs generated by the specified Exchange server
ULONG LDAPConnection::GetOABsOfServer(const string ExchServerName, list<string>& OabDNs)
{
	DebugPrintf(SV_LOG_DEBUG,"Line(%u): BEGIN %s\n",__LINE__,__FUNCTION__);
	if(m_pLDAPConnection == NULL)
	{
		cout<<"[ERROR]: Conection is not established..."<<endl;
		DebugPrintf(SV_LOG_DEBUG,"Line(%u): Error %s: Conection is not established.\n",__LINE__,__FUNCTION__);
		return LDAP_CONNECT_ERROR;
	}
	if(ExchServerName.empty())
	{
		cout<<"[WARNING]: Exchange Server Name specified is empty ...\n"
			<<"Unblae to retrieve the information form AD.";
		DebugPrintf(SV_LOG_DEBUG,"Line(%u): Warning %s: No Exchange Server Name specified.\n",__LINE__,__FUNCTION__);
		return LDAP_OPERATIONS_ERROR;
	}

	ULONG ulReturn = 0;
	LDAPMessage *pInmSearchResult,*pInmEntry;
	char* inm_req_attribute[2];
	inm_req_attribute[0] = OFFLINE_AB_SERVER;
	inm_req_attribute[1] = NULL;

	BerElement *pInmBerEle;
	char *pInmAttribute;
	char **inm_val;

	if(m_configDN.empty())
	{
		ulReturn = GetconfigurationDN();
		if(LDAP_SUCCESS != ulReturn)
			return ulReturn;
	}

	string serverDN;
	ulReturn = GetServerDN(ExchServerName,serverDN);
	if(ulReturn != LDAP_SUCCESS)
	{
		cout<<" Failed to get the DN of the server"<<ExchServerName<<endl;
		DebugPrintf(SV_LOG_DEBUG,"Line(%u): Error %s: GetServerDN() failed .\n",__LINE__,__FUNCTION__);
		return ulReturn;
	}

	string filter = "(objectclass=msExchOAB)";
	//int searchCount = 0;
	do{
		ulReturn = ldap_search_s(m_pLDAPConnection,(PCHAR)m_configDN.c_str(),LDAP_SCOPE_SUBTREE,(PCHAR)filter.c_str(),inm_req_attribute,0,&pInmSearchResult);
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
			cout << "[ERROR]: ldap_search_s failed. "<< ldap_err2string(ulReturn) << endl;
			DebugPrintf(SV_LOG_DEBUG,"Line(%u): Error %s: ldap_search_s failed().\n",__LINE__,__FUNCTION__);
			if(pInmSearchResult)
			{
				ldap_msgfree(pInmSearchResult);
				pInmSearchResult = NULL;
			}

			//If same error hass repeated for MAX_SEARCH_ATTEMPTS times then stop the searach process
			/*if(searchCount >= MAX_SEARCH_ATTEMPTS)
			{
				cout<<"[INFO]: Maximum search attempts has reached."<<endl
					<<"Failed to search the directory server."<<endl;
				return ulReturn;
			}*/
			ulReturn = HandleConnectionError();
			if(LDAP_SUCCESS != ulReturn)
			{
				cout << "[INFO]: Maximum connection attempts " << MAX_LDAP_CONNECT_ATTEMPTS << " has been reached."<<endl;
				DebugPrintf(SV_LOG_DEBUG,"Line(%u): Info %s: Maximum connection attempts has been reached.\n",__LINE__,__FUNCTION__);
				cout << "[ERROR]: Unable to connect to directory server." << endl;
				DebugPrintf(SV_LOG_DEBUG,"Line(%u): Error %s: Unable to connect to directory server.\n",__LINE__,__FUNCTION__);
				return ulReturn;
			}
		}
		else
		{
			if(pInmSearchResult)
			{
				ldap_msgfree(pInmSearchResult);
				pInmSearchResult = NULL;
			}

			cout<<"Error encountered while searching directory server."<<endl;
			cout<<"ldap_search_s() failed ..."<<endl
				<<"[ERROR] : "<<ldap_err2string(ulReturn)<<endl;
			DebugPrintf(SV_LOG_DEBUG,"Line(%u): Error %s: Error encountered while searching directory server. ldap_search_s() failed. %s.\n",__LINE__,__FUNCTION__,ldap_err2string(ulReturn));
			return ulReturn;
		}

		//searchCount++;
	}while(1);

	pInmEntry = ldap_first_entry(m_pLDAPConnection,pInmSearchResult);
	if(pInmEntry == NULL)
	{
		ldap_msgfree(pInmSearchResult);
		cout<<"No entries found or ldap_first_entry() failed ..."<<endl;
		DebugPrintf(SV_LOG_DEBUG,"Line(%u): Error %s: No entries found or ldap_first_entry() failed .\n",__LINE__,__FUNCTION__);
		return (ULONG)GetLastError();
	}
	while(pInmEntry != NULL)
	{
		pInmAttribute = ldap_first_attribute(m_pLDAPConnection,pInmEntry,&pInmBerEle);
		while(pInmAttribute != NULL)
		{
			if(strcmp(pInmAttribute,inm_req_attribute[0]) == 0)
			{
				inm_val = ldap_get_values(m_pLDAPConnection,pInmEntry,pInmAttribute);
				if(serverDN.compare(*inm_val) == 0)
					OabDNs.push_back( string(ldap_get_dn(m_pLDAPConnection,pInmEntry)) );
			}
			ldap_value_free(inm_val);
			pInmAttribute = ldap_next_attribute(m_pLDAPConnection,pInmEntry,pInmBerEle);
		}
		ber_free(pInmBerEle,0);
		pInmEntry = ldap_next_entry(m_pLDAPConnection,pInmEntry);
	}
	ldap_msgfree(pInmSearchResult);
	DebugPrintf(SV_LOG_DEBUG,"Line(%u): END %s\n",__LINE__,__FUNCTION__);
	return LDAP_SUCCESS;
}

ULONG LDAPConnection::GetMsExchInstallPath(const string &ServerName, string& installPath)
{
	DebugPrintf(SV_LOG_DEBUG,"Line(%u): BEGIN %s\n",__LINE__,__FUNCTION__);
	ULONG ulReturn = LDAP_SUCCESS;
	if(m_pLDAPConnection == NULL)
	{
		cout<<"[ERROR]: Conection is not established..."<<endl;
		DebugPrintf(SV_LOG_DEBUG,"Line(%u): Error %s: Conection is not established.\n",__LINE__,__FUNCTION__);
		return LDAP_CONNECT_ERROR;
	}
	if(ServerName.empty())
	{
		cout<<"[WARNING]: No Exchange Server Name specified ...\n"
			<<"Unblae to retrieve the information form AD.";
		DebugPrintf(SV_LOG_DEBUG,"Line(%u): Warning %s: No Exchange Server Name specified.\n",__LINE__,__FUNCTION__);
		return LDAP_OPERATIONS_ERROR;
	}
	LDAPMessage *pInmSearchResult,*pInmEntry;
	char* inm_req_attribute[2];
	inm_req_attribute[0] = "msExchInstallPath";
	inm_req_attribute[1] = NULL;

	BerElement *pInmBerEle;
	char *pInmAttribute;
	char **inm_val;

	if(m_configDN.empty())
	{
		ulReturn = GetconfigurationDN();
		if(LDAP_SUCCESS != ulReturn)
			return ulReturn;
	}


	string filter = "(&(objectclass=msExchExchangeServer)(cn="+ServerName+"))";
	//int searchCount = 0;
	do{
		ulReturn = ldap_search_s(m_pLDAPConnection,(PCHAR)m_configDN.c_str(),LDAP_SCOPE_SUBTREE,(PCHAR)filter.c_str(),inm_req_attribute,0,&pInmSearchResult);
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
			cout << "[ERROR]: ldap_search_s failed. "<< ldap_err2string(ulReturn) << endl;
			DebugPrintf(SV_LOG_DEBUG,"Line(%u): Error %s: ldap_search_s failed().\n",__LINE__,__FUNCTION__);

			if(pInmSearchResult)
			{
				ldap_msgfree(pInmSearchResult);
				pInmSearchResult = NULL;
			}

			//If same error hass repeated for MAX_SEARCH_ATTEMPTS times then stop the searach process
			/*if(searchCount >= MAX_SEARCH_ATTEMPTS)
			{
				cout<<"[INFO]: Maximum search attempts has reached."<<endl
					<<"Failed to search the directory server."<<endl;
				return ulReturn;
			}*/
			ulReturn = HandleConnectionError();
			if(LDAP_SUCCESS != ulReturn)
			{
				cout << "[INFO]: Maximum connection attempts " << MAX_LDAP_CONNECT_ATTEMPTS << " has been reached."<<endl;
				DebugPrintf(SV_LOG_DEBUG,"Line(%u): Info %s: Maximum connection attempts has been reached.\n",__LINE__,__FUNCTION__);
				cout << "[ERROR]: Unable to connect to directory server." << endl;
				DebugPrintf(SV_LOG_DEBUG,"Line(%u): Error %s: Unable to connect to directory server.\n",__LINE__,__FUNCTION__);
				return ulReturn;
			}
		}
		else
		{
			if(pInmSearchResult)
			{
				ldap_msgfree(pInmSearchResult);
				pInmSearchResult = NULL;
			}

			cout<<"Error encountered while searching directory server."<<endl;
			cout<<"ldap_search_s() failed ..."<<endl
				<<"[ERROR] : "<<ldap_err2string(ulReturn)<<endl;
			DebugPrintf(SV_LOG_DEBUG,"Line(%u): Error %s: Error encountered while searching directory server. ldap_search_s() failed. %s.\n",__LINE__,__FUNCTION__,ldap_err2string(ulReturn));
			return ulReturn;
		}

		//searchCount++;
	}while(1);

	pInmEntry = ldap_first_entry(m_pLDAPConnection,pInmSearchResult);
	if(pInmEntry == NULL)
	{
		ldap_msgfree(pInmSearchResult);
		cout<<"No entries found or ldap_first_entry() failed ..."<<endl;
		DebugPrintf(SV_LOG_DEBUG,"Line(%u): Error %s: No entries found or ldap_first_entry() failed.\n",__LINE__,__FUNCTION__);
		return (ULONG)GetLastError();
	}
	while(pInmEntry != NULL)
	{
		pInmAttribute = ldap_first_attribute(m_pLDAPConnection,pInmEntry,&pInmBerEle);
		while(pInmAttribute != NULL)
		{
			if(strcmp(pInmAttribute,inm_req_attribute[0]) == 0)
			{
				inm_val = ldap_get_values(m_pLDAPConnection,pInmEntry,pInmAttribute);
				installPath = string(*inm_val);
			}
			ldap_value_free(inm_val);
			pInmAttribute = ldap_next_attribute(m_pLDAPConnection,pInmEntry,pInmBerEle);
		}
		ber_free(pInmBerEle,0);
		pInmEntry = ldap_next_entry(m_pLDAPConnection,pInmEntry);
	}
	ldap_msgfree(pInmSearchResult);
	DebugPrintf(SV_LOG_DEBUG,"Line(%u): END %s\n",__LINE__,__FUNCTION__);
	return ulReturn;
}
ULONG LDAPConnection::RemoveExtraMDBCopies(const string & mdbDN,const string & mdbCopyDN)
{
	DebugPrintf(SV_LOG_DEBUG,"Line(%u): BEGIN %s\n",__LINE__,__FUNCTION__);
	ULONG ulReturn = LDAP_SUCCESS;
	if(m_pLDAPConnection == NULL)
	{
		cout<<"[ERROR]: Conection is not established..."<<endl;
		DebugPrintf(SV_LOG_DEBUG,"Line(%u): Error %s: Conection is not established.\n",__LINE__,__FUNCTION__);
		return LDAP_CONNECT_ERROR;
	}
	if(mdbDN.empty())
	{
		cout<<"[WARNING]: No Mailbox Database Name specified ...\n"
			<<"Unblae to retrieve the information from AD.";
		DebugPrintf(SV_LOG_DEBUG,"Line(%u): Warning %s: No Mailbox Database Name specified.\n",__LINE__,__FUNCTION__);
		return LDAP_OPERATIONS_ERROR;
	}
	if(mdbCopyDN.empty())
	{
		cout<<"[WARNING]: No Mailbox Database Copy Name specified ...\n"
			<<"Unblae to retrieve the information from AD.";
		DebugPrintf(SV_LOG_DEBUG,"Line(%u): Warning %s: No Mailbox Database Copy Name specified.\n",__LINE__,__FUNCTION__);
		return LDAP_OPERATIONS_ERROR;
	}
	LDAPMessage *pInmSearchResult,*pInmEntry;
	PCHAR pDN = NULL;
	list<string> extraMDBCopies;
	bool bMdbCopyFound = false;

	do{
		//search should happen only one level below the specified Mailbox Database, and the objectclass should be 'msExchMDBCopy'
		ulReturn = ldap_search_s(m_pLDAPConnection,(PCHAR)mdbDN.c_str(),LDAP_SCOPE_ONELEVEL,"(objectclass=msExchMDBCopy)",NULL,0,&pInmSearchResult);
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
			cout << "[ERROR]: ldap_search_s failed. "<< ldap_err2string(ulReturn) << endl;
			DebugPrintf(SV_LOG_DEBUG,"Line(%u): Error %s: ldap_search_s failed().\n",__LINE__,__FUNCTION__);

			if(pInmSearchResult)
			{
				ldap_msgfree(pInmSearchResult);
				pInmSearchResult = NULL;
			}

			//If same error hass repeated for MAX_SEARCH_ATTEMPTS times then stop the searach process
			/*if(searchCount >= MAX_SEARCH_ATTEMPTS)
			{
				cout<<"[INFO]: Maximum search attempts has reached."<<endl
					<<"Failed to search the directory server."<<endl;
				return ulReturn;
			}*/
			ulReturn = HandleConnectionError();
			if(LDAP_SUCCESS != ulReturn)
			{
				cout << "[INFO]: Maximum connection attempts " << MAX_LDAP_CONNECT_ATTEMPTS << " has been reached."<<endl;
				DebugPrintf(SV_LOG_DEBUG,"Line(%u): Info %s: Maximum connection attempts has been reached.\n",__LINE__,__FUNCTION__);
				cout << "[ERROR]: Unable to connect to directory server." << endl;
				DebugPrintf(SV_LOG_DEBUG,"Line(%u): Error %s: Unable to connect to directory server.\n",__LINE__,__FUNCTION__);
				return ulReturn;
			}
		}
		else
		{
			if(pInmSearchResult)
			{
				ldap_msgfree(pInmSearchResult);
				pInmSearchResult = NULL;
			}

			cout<<"Error encountered while searching directory server."<<endl;
			cout<<"ldap_search_s() failed ..."<<endl
				<<"[ERROR] : "<<ldap_err2string(ulReturn)<<endl;
			DebugPrintf(SV_LOG_DEBUG,"Line(%u): Error %s: Error encountered while searching directory server. ldap_search_s() failed. %s.\n",__LINE__,__FUNCTION__,ldap_err2string(ulReturn));
			return ulReturn;
		}

		//searchCount++;
	}while(1);

	pInmEntry = ldap_first_entry(m_pLDAPConnection,pInmSearchResult);
	if(pInmEntry == NULL)
	{
		ldap_msgfree(pInmSearchResult);
		cout<<"No entries found or ldap_first_entry() failed ..."<<endl;
		DebugPrintf(SV_LOG_DEBUG,"Line(%u): Error %s: No entries found or ldap_first_entry() failed.\n",__LINE__,__FUNCTION__);
		return (ULONG)GetLastError();
	}
	//Verify that, the mdbCopyDN should be one of the entries returned by the above search fuction. Then only we will remove the copies except that one
	while(pInmEntry != NULL)
	{
		pDN = ldap_get_dn(m_pLDAPConnection,pInmEntry);
		if(NULL == pDN)
		{
			ldap_msgfree(pInmSearchResult);
			cout<<"Failed to get MDB copy DN. ldap_get_dn() failed."<<endl;
			DebugPrintf(SV_LOG_DEBUG,"Line(%u): Error %s: Failed to get MDB copy DN. ldap_get_dn() failed.\n",__LINE__,__FUNCTION__);
			return (ULONG)GetLastError();
		}
		//Gather the list of all extra copies
		if(0 == mdbCopyDN.compare(pDN))
		{
			bMdbCopyFound = true;
		}
		else
		{
			extraMDBCopies.push_back(string(pDN));
		}
		pInmEntry = ldap_next_entry(m_pLDAPConnection,pInmEntry);
	}
	ldap_msgfree(pInmSearchResult);
	//If we found the MDBCopy specified then only Remove the extra copies. Otherwise don't remove the copies, some configuration has mis-matched
	if(bMdbCopyFound)
	{
		ulReturn = deleteEntryFromAD(extraMDBCopies);
		if(LDAP_SUCCESS != ulReturn)
		{
			cout<<"Could not remove the additional MDB Copies from AD."<<endl;
			DebugPrintf(SV_LOG_DEBUG,"Line(%u): Error %s: Could not remove the additional MDB Copies from AD.\n",__LINE__,__FUNCTION__);
			return ulReturn;
		}
	}
	else
	{
		cout<<"Could not remove the additional MDB Copies from AD."<<endl;
		DebugPrintf(SV_LOG_DEBUG,"Line(%u): Error %s: Could not remove the additional MDB Copies from AD. Specified MDBCopy DN is not found.\n",__LINE__,__FUNCTION__);
		return LDAP_OPERATIONS_ERROR;
	}
	DebugPrintf(SV_LOG_DEBUG,"Line(%u): END %s\n",__LINE__,__FUNCTION__);
	return LDAP_SUCCESS;
}
SVERROR LDAPConnection::modifySPN(const  std::string& srcExchServerName, const  std::string& trgExchServerName,bool bRetainDuplicateSPN,bool bFailback, const  std::string& /*appName*/)
{
	DebugPrintf(SV_LOG_DEBUG,"Line(%u): BEGIN %s\n",__LINE__,__FUNCTION__);

	TCHAR bufCompNamr[256] = TEXT("");
	DWORD dwSize = sizeof(bufCompNamr);

	if (!GetComputerNameEx((COMPUTER_NAME_FORMAT)3, bufCompNamr, &dwSize))
	{
		cout<<"GetComputerNameEx failed. Error code:"<<GetLastError()<<endl;
		DebugPrintf(SV_LOG_DEBUG,"Line(%u): TERMINATED %s: GetComputerNameEx failed.\n",__LINE__,__FUNCTION__);
		return SVE_FAIL;
	}
		
	string hostName = string(bufCompNamr);
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
	}

	/*
	/ Prepare FQDN of source & target servers
	*/
	string source_fqdn = srcExchServerName+"."+domainName;
	string target_fqdn = trgExchServerName+"."+domainName;
	/*
	/Get the source and target exchange servers SPN entris.
	*/
	list<string> sourceSPN;
	list<string> targetSPN;
	ULONG ulReturn = GetAttributeValues(sourceSPN,SERVICE_PRINCIPAL_NAME,"(&(objectClass=computer)(cn="+srcExchServerName+"))",domainDN);
	if(LDAP_SUCCESS != ulReturn)
	{
		cout<<"Failed to get the SPN entries of "<<srcExchServerName<<endl
			<<"[Error]["<<ulReturn<<"]: "<<ldap_err2string(ulReturn)<<endl;
		DebugPrintf(SV_LOG_DEBUG,"Line(%u): TERMINATED %s\n",__LINE__,__FUNCTION__);
		return SVE_FAIL;
	}
	ulReturn = GetAttributeValues(targetSPN,SERVICE_PRINCIPAL_NAME,"(&(objectClass=computer)(cn="+trgExchServerName+"))",domainDN);
	if(LDAP_SUCCESS != ulReturn)
	{
		cout<<"Failed to get the SPN entries of "<<trgExchServerName<<endl
			<<"[Error]["<<ulReturn<<"]: "<<ldap_err2string(ulReturn)<<endl;
		DebugPrintf(SV_LOG_DEBUG,"Line(%u): TERMINATED %s\n",__LINE__,__FUNCTION__);
		return SVE_FAIL;
	}
	/*
	/Filter SPN Entries of source and prepare requiredEntries
	*/
	list<string> requiredSPNEntries; //These are the entries need to updated at target
	list<string>::const_iterator iter_SPNs = sourceSPN.begin();
	for(int i=0; i<5 ; i++)
	{
		std::string strEntryVal;
		if(bFailback) //In Failback case the target SPNs are available at source.
		{			 //So we need to gather target SPN entries at source macine. Remember, in failabck case the target means actual source.
			if(isEntryExist(strSPNEntry[i]+trgExchServerName,sourceSPN,strEntryVal))
				requiredSPNEntries.push_back(strEntryVal);
			if(isEntryExist(strSPNEntry[i]+target_fqdn,sourceSPN,strEntryVal))
				requiredSPNEntries.push_back(strEntryVal);
		}
		else //In Failover case the source & target SPN entries are in normal state.
		{	// So we need to gahter required entries of source at source machine itself
			if(isEntryExist(strSPNEntry[i]+srcExchServerName,sourceSPN,strEntryVal))
				requiredSPNEntries.push_back(strEntryVal);
			if(isEntryExist(strSPNEntry[i]+source_fqdn,sourceSPN,strEntryVal))
				requiredSPNEntries.push_back(strEntryVal);
		}
	}
	/*
	/If required entries are empty then nothing is there to update the SPN entries, simply quit the procedure.
	*/
	if(requiredSPNEntries.empty())
	{
		cout<<"<!>WARNING: No Exchange related SPN entry found on "<<srcExchServerName<<". Nothing to update SPN entries."<<endl;
		DebugPrintf(SV_LOG_DEBUG,"Line(%u): END %s\n",__LINE__,__FUNCTION__);
		return SVS_OK;
	}
	/*
	/Prepare newSourceSPN and newTargetSPN
	*/
	list<string> newSourceSPN;
	list<string> newTargetSPN;

	iter_SPNs = sourceSPN.begin();
	while(iter_SPNs != sourceSPN.end())
	{
		if(bRetainDuplicateSPN)
			newSourceSPN.push_back(*iter_SPNs);
		else if(false == isEntryExist(*iter_SPNs,requiredSPNEntries))
		{
			newSourceSPN.push_back(*iter_SPNs);
		}
		iter_SPNs++;
	}

	iter_SPNs = targetSPN.begin();
	while(iter_SPNs != targetSPN.end())
	{
		newTargetSPN.push_back(*iter_SPNs);
		iter_SPNs++;
	}
	iter_SPNs = requiredSPNEntries.begin(); // Add the requiredEntries to the newTragetSPNs
	while(iter_SPNs != requiredSPNEntries.end())
	{
		if(false == isEntryExist(*iter_SPNs,newTargetSPN))
			newTargetSPN.push_back(*iter_SPNs);
		iter_SPNs++;
	}
	/*
	/Get the DNs of source and target to update SPNs in AD
	*/
	string srcComputerDN,trgComputerDN;
	ulReturn = GetDN("(&(objectClass=computer)(cn="+srcExchServerName+"))",srcComputerDN,domainDN);
	if(LDAP_SUCCESS != ulReturn)
	{
		cout<<"Failed to get the DN of "<<srcExchServerName<<endl;
		DebugPrintf(SV_LOG_DEBUG,"Line(%u): TERMINATED %s\n",__LINE__,__FUNCTION__);
		return SVE_FAIL;
	}
	ulReturn = GetDN("(&(objectClass=computer)(cn="+trgExchServerName+"))",trgComputerDN,domainDN);
	if(LDAP_SUCCESS != ulReturn)
	{
		cout<<"Failed to get the DN of "<<trgExchServerName<<endl;
		DebugPrintf(SV_LOG_DEBUG,"Line(%u): TERMINATED %s\n",__LINE__,__FUNCTION__);
		return SVE_FAIL;
	}

	/*
	/Update the newSourceSPN & newTargetSPN to AD
	*/
	ulReturn = ModifyADEntry(trgComputerDN,SERVICE_PRINCIPAL_NAME,newTargetSPN);
	if(LDAP_SUCCESS != ulReturn)
	{
		cout<<"Failed to update the SPN entries for "<<trgExchServerName<<endl;
		DebugPrintf(SV_LOG_DEBUG,"Line(%u): TERMINATED %s\n",__LINE__,__FUNCTION__);
		return SVE_FAIL;
	}
	ulReturn = ModifyADEntry(srcComputerDN,SERVICE_PRINCIPAL_NAME,newSourceSPN);
	if(LDAP_SUCCESS != ulReturn)
	{
		cout<<"Failed to update the SPN entries for "<<srcExchServerName<<endl;
		DebugPrintf(SV_LOG_DEBUG,"Line(%u): TERMINATED %s\n",__LINE__,__FUNCTION__);
		return SVE_FAIL;
	}
	/*
	/Reaching this point means everything was fine, and the SPN entries are updated successfully.
	*/
	if(bRetainDuplicateSPN)
	{
		cout<<"********** Follwing SPN entris are successfully updated at "<< trgExchServerName<<endl;
	}
	else
	{
		cout<<"********** Follwing SPN entries are successfully moved from "<< srcExchServerName \
			<<" to "<<trgExchServerName<<endl;
	}
	iter_SPNs = requiredSPNEntries.begin();
	while(iter_SPNs != requiredSPNEntries.end())
	{
		cout<<*iter_SPNs<<endl;
		iter_SPNs++;
	}
	DebugPrintf(SV_LOG_DEBUG,"Line(%u): END %s\n",__LINE__,__FUNCTION__);
	return SVS_OK;
}
bool LDAPConnection::isEntryExist(std::string strEntry,std::list<string>& listEntry, std::string& entryVal)
{
	DebugPrintf(SV_LOG_DEBUG,"Line(%u): BEGIN %s\n",__LINE__,__FUNCTION__);
	entryVal.clear();//clear any existing data.
	std::list<std::string>::const_iterator iterEntry = listEntry.begin();
	while(iterEntry != listEntry.end())
	{
		if(0 == strcmpi(strEntry.c_str(),iterEntry->c_str()))
		{
			entryVal = string(*iterEntry);
			DebugPrintf(SV_LOG_DEBUG,"Line(%u): END %s\n",__LINE__,__FUNCTION__);
			return true;
		}
		iterEntry++;
	}
	DebugPrintf(SV_LOG_DEBUG,"Line(%u): END %s\n",__LINE__,__FUNCTION__);
	return false;
}
bool LDAPConnection::isEntryExist(std::string strEntry,std::list<string>& listEntry)
{
	DebugPrintf(SV_LOG_DEBUG,"Line(%u): BEGIN %s\n",__LINE__,__FUNCTION__);
	std::list<std::string>::const_iterator iterEntry = listEntry.begin();
	while(iterEntry != listEntry.end())
	{
		if(0 == strcmpi(strEntry.c_str(),iterEntry->c_str()))
		{
			DebugPrintf(SV_LOG_DEBUG,"Line(%u): END %s\n",__LINE__,__FUNCTION__);
			return true;
		}
		iterEntry++;
	}
	DebugPrintf(SV_LOG_DEBUG,"Line(%u): END %s\n",__LINE__,__FUNCTION__);
	return false;
}
ULONG LDAPConnection::GetDN(const std::string &Filter,std::string &valDN,std::string baseDN)
{
	DebugPrintf(SV_LOG_DEBUG,"Line(%u): BEGIN %s\n",__LINE__,__FUNCTION__);
	valDN.clear();
	if(NULL == m_pLDAPConnection)
	{
		cout<<"[ERROR]: Conection is not established..."<<endl;
		DebugPrintf(SV_LOG_DEBUG,"Line(%u): TERMINATED %s: Conection is not established.\n",__LINE__,__FUNCTION__);
		return LDAP_CONNECT_ERROR;
	}

	ULONG ulReturn = 0;
	LDAPMessage *pInmSearchResult,*pInmEntry;

	PCHAR attrs[2];
	attrs[0] = (PCHAR)"distinguishedName";
	attrs[1] = NULL;

	string base;
	if(baseDN.empty())
	{
		ulReturn = GetconfigurationDN();
		if(LDAP_SUCCESS != ulReturn)
		{
			DebugPrintf(SV_LOG_DEBUG,"Line(%u): TERMINATED %s\n",__LINE__,__FUNCTION__);
			return ulReturn;
		}
		base = baseDN;
	}
	else
		base = baseDN;
	//int searchCount = 0;
	do{
		ulReturn = ldap_search_s(m_pLDAPConnection,(PCHAR)base.c_str(),LDAP_SCOPE_SUBTREE,(PCHAR)Filter.c_str(),attrs,0,&pInmSearchResult);
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
			cout << "ldap_search_s failed. "<<endl
				 <<"[Error]["<<ulReturn<<"]: "<<ldap_err2string(ulReturn)<<endl;
			if(pInmSearchResult)
			{
				ldap_msgfree(pInmSearchResult);
				pInmSearchResult = NULL;
			}

			//If same error has repeated for MAX_SEARCH_ATTEMPTS times then stop the searach process
			/*if(searchCount >= MAX_SEARCH_ATTEMPTS)
			{
				cout<<"[INFO]: Maximum search attempts has reached."<<endl
					<<"Failed to search the directory server."<<endl;
				return ulReturn;
			}*/
			ulReturn = HandleConnectionError();
			if(LDAP_SUCCESS != ulReturn)
			{
				cout << "[INFO]: Maximum connection attempts " << MAX_LDAP_CONNECT_ATTEMPTS << " has been reached."<<endl;
				cout << "[ERROR]: Unable to connect to directory server." << endl;
				DebugPrintf(SV_LOG_DEBUG,"Line(%u): TERMINATED %s: Unable to connect to directory server.\n",__LINE__,__FUNCTION__);
				return ulReturn;
			}
		}
		else
		{
			if(pInmSearchResult)
			{
				ldap_msgfree(pInmSearchResult);
				pInmSearchResult = NULL;
			}

			cout<<"Error encountered while searching directory server."<<endl;
			cout<<"Filter :"<<Filter<<endl
				<<"[Error]["<<ulReturn<<"]: "<<ldap_err2string(ulReturn)<<endl;
			return ulReturn;
		}

		//searchCount++;
	}while(1);

	ULONG entryCount = ldap_count_entries(m_pLDAPConnection,pInmSearchResult);
	if(entryCount <= 0)
	{
		ldap_msgfree(pInmSearchResult);
		cout<<"Warning : No entry found."<<endl
			<<"Filter : "<<Filter<<endl;
		DebugPrintf(SV_LOG_DEBUG,"Line(%u): TERMINATED %s: No entry found.\n",__LINE__,__FUNCTION__);
		return LDAP_NO_SUCH_OBJECT;
	}
	pInmEntry = ldap_first_entry(m_pLDAPConnection,pInmSearchResult);
	if(NULL == pInmEntry)
	{
		ulReturn = GetLastError();
		ldap_msgfree(pInmSearchResult);
		cout<<"ldap_first_entry() failed ..."<<endl
			<<"[Error]["<<ulReturn<<"]: "<<ldap_err2string(ulReturn)<<endl;
		DebugPrintf(SV_LOG_DEBUG,"Line(%u): TERMINATED %s: ldap_first_entry() failed.\n",__LINE__,__FUNCTION__);
		return ulReturn;
	}
	while(pInmEntry != NULL)
	{
		valDN = ldap_get_dn(m_pLDAPConnection,pInmEntry);
		pInmEntry = ldap_next_entry(m_pLDAPConnection,pInmEntry);
	}
	ldap_msgfree(pInmSearchResult);
	if(valDN.empty())
	{
		cout<<"Empty DN found."<<endl;
		DebugPrintf(SV_LOG_DEBUG,"Line(%u): TERMINATED %s: Empty DN found.\n",__LINE__,__FUNCTION__);
		return LDAP_NO_SUCH_ATTRIBUTE;
	}
	DebugPrintf(SV_LOG_DEBUG,"Line(%u): END %s\n",__LINE__,__FUNCTION__);
	return LDAP_SUCCESS;
}

ULONG LDAPConnection::ModifyADEntry(const string& objectDN,const string &attribute,list<string>& attrValue)
{
	DebugPrintf(SV_LOG_DEBUG,"Line(%u): BEGIN %s\n",__LINE__,__FUNCTION__);
	if(m_pLDAPConnection == NULL)
	{
		cout<<"[ERROR]: Conection is not established...\t"<<__FUNCTION__<<endl;
		DebugPrintf(SV_LOG_DEBUG,"Line(%u): TERMINATED %s: Conection is not established.\n",__LINE__,__FUNCTION__);
		return LDAP_CONNECT_ERROR;
	}

	ULONG ulInmRet = LDAP_SUCCESS;

	LDAPMod inm_entry;
	inm_entry.mod_op = LDAP_MOD_REPLACE;
	inm_entry.mod_type = (PCHAR)attribute.c_str();
	list<string>::const_iterator iterModVal = attrValue.begin();
	
	int nValue=0;
	int numVlues;
	INM_SAFE_ARITHMETIC(numVlues = InmSafeInt< std::list<std::string>::size_type >::Type(attrValue.size())+1, INMAGE_EX(attrValue.size()))
	PCHAR *values = new PCHAR[numVlues];
	while(iterModVal != attrValue.end())
	{
		if(nValue >= numVlues)
			break; //Validation to avoid out of memory access issues.
        size_t valuelen;
        INM_SAFE_ARITHMETIC(valuelen = InmSafeInt<size_t>::Type(iterModVal->length())+1, INMAGE_EX(iterModVal->length()))
		values[nValue] = new char[valuelen];
		inm_strcpy_s(values[nValue],(iterModVal->length()+1),iterModVal->c_str());
		nValue++;
		iterModVal++;
	}
	values[nValue] = NULL;

	inm_entry.mod_vals.modv_strvals = values;
	LDAPMod *NewEntry[2];
	NewEntry[0] = &inm_entry;
	NewEntry[1] = NULL;

	if (NewEntry[0] != NULL)
		ulInmRet = ldap_modify_s(m_pLDAPConnection, (const PCHAR) objectDN.c_str(), NewEntry);

	if (ulInmRet != LDAP_SUCCESS)
	{	
		cout << "ldap_modify opeation failed."<<endl
			 <<"[Error]["<<ulInmRet<<"]: "<<ldap_err2string(ulInmRet)<<endl;
		DebugPrintf(SV_LOG_DEBUG,"Line(%u): TERMINATED %s: ldap_modify_s() failed.\n",__LINE__,__FUNCTION__);
	}
	// release memory
	for (int i=0; i < nValue; i++) {
		delete[] values[i];
	}
	delete[] values;

	DebugPrintf(SV_LOG_DEBUG,"Line(%u): END %s\n",__LINE__,__FUNCTION__);
	return ulInmRet;
}
