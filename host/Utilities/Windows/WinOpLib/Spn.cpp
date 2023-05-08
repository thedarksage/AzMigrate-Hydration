#include "stdafx.h"
#include <list>
#include <map>
#include <string>
#include <vector>
#include <iostream>
#include <tchar.h>
#include <winsock2.h>
#include <windows.h>
#include <winldap.h>
#include "ADInterface.h"
#include "Spn.h"
#include "inmsafecapis.h"
#include "inmsafeint.h"
#include "inmageex.h"

using namespace std;

const string CN                       = "CN";
const string DN                       = "DN";
const string SEPARATOR_COMMA		  = ",";
const string SEPARATOR_COLON		  = ":";
const string SEPARATOR_SEMICOLON	  = ";";
#define MAX_LDAP_SEARCH_ATTEMPTS	1
#define DELAY_BETWEEN_ATTEMPTS	5
#define VALID_CHAR_IN_CN_SIZE   36
char Tkn[VALID_CHAR_IN_CN_SIZE]={'a','b','c','d','e','f','g','h','i','j','k','l','m','n','o','p','q','r','s','t','u','v','w','x','y','z','1','2','3','4','5','6','7','8','9','0'};

// Exchange Server related parameters
const char *exchEntries[] = {"exchangeMDB", "exchangeRFR", "SMTPSVC"};
const char *hostEntry = "HOST";
ADInterface ad;

ULONG modifyEntriesInComputerAccount(bool operation, const std::string compAccount, const std::string serverNameParam,
									 LDAP* pLdapConnection, LDAPMessage* pSpnList, std::string domainName, const string& serverDomainName);
void trimRight( string& str, const string& trimChars)
{
	string::size_type pos = str.find_last_not_of( trimChars );
	str.erase( pos + 1 );  
} 

void trimLeft( string& str, const string& trimChars)  
{
	string::size_type pos = str.find_first_not_of( trimChars );   
	str.erase( 0, pos ); 
} 

void trim( string& str, const string& trimChars)
{     
	trimRight( str, trimChars );    
	trimLeft( str, trimChars ); 
} 

ULONG AddSpnEntries(map< string, list<string> > accServerMapping, string domainName, string domainDN, const string& serverDomainName)
{	
	if(domainName.empty() && domainDN.empty())
	{
		TCHAR buffer[256] = TEXT("");
		TCHAR szDescription[32] = TEXT("DNS fully-qualified");
		DWORD dwSize = sizeof(buffer);
		
		// "ComputerNameDnsFullyQualified" is of enum NameType COMPUTER_NAME_FORMAT(3).
		// It is the input parameter for output of fully-qualified DNS name that uniquely identifies 
		// the local computer or 
		// the cluster associated with the local computer. 

		if (!GetComputerNameEx((COMPUTER_NAME_FORMAT)3, buffer, &dwSize))
		{
			_tprintf(TEXT("GetComputerNameEx failed (%d)\n"), GetLastError());
			return 0;
		}
		//else			
			//_tprintf(TEXT("%s: %s\n"), szDescription, buffer);
		
		size_t firstDot = 0;
		size_t foundDot = 0;
		string hostName = string(buffer);		

		if( (firstDot = hostName.find_first_of('.', 0)) != string::npos )
		{
			domainName.assign(hostName.c_str(), firstDot+1, hostName.length()-1-firstDot);			
			domainDN = "DC=" + domainName;
			while ( (foundDot = domainDN.find_first_of('.', 0)) != string::npos )			
				domainDN.replace(foundDot, 1, ",DC=");
			hostName.assign(hostName.c_str(), firstDot);					
			//cout << "hostName=" << hostName.c_str() << endl;
			//cout << "domainName=" << domainName.c_str() << endl;
			//cout << "domainDN=" << domainDN.c_str() << endl;
		}			
		dwSize = sizeof(buffer);		
	}

	// get an Ldap connection
	LDAP* pLdapConnection = ad.getLdapConnection();
	if (pLdapConnection == NULL)
	{
		cout << "Failed to connect to the ldap server\n";
		return false;
	}

	// ldap bind to the domain DN
	ULONG errorCode = LDAP_SUCCESS;
	ULONG lRtn = 0;
	lRtn = ldap_bind_s(	pLdapConnection,							
				(const PCHAR) domainDN.c_str(),				//Use the naming context from the input list
				NULL,										//Credentials structure
				LDAP_AUTH_NEGOTIATE);						// Authentication mode
	if(lRtn != LDAP_SUCCESS)
	{
		cout << "ldap_bind_s failed.\nERROR:[" << lRtn << "] " << ldap_err2string(errorCode)<< endl;	
		return false;
	}

	string strDCName = ad.getLDAPServerName(pLdapConnection);
	if(!strDCName.empty())
		cout << endl << "Connected DC: " << strDCName << endl;

	// search source and target computers
	LDAPMessage* pSpnList;
	PCHAR pMyFilter = "(objectClass=Computer)";
	PCHAR pAttributes[2];
	pAttributes[0] = (PCHAR)EXCHANGE_SERVICE_PRINCIPAL_NAME;
	pAttributes[1] = NULL;	
	ULONG numberOfEntries;
	string tempFilter;
	int test = 0;
	ADInterface ad;

	map< string, list<string> >::const_iterator iter_acc = accServerMapping.begin();
	for(; iter_acc != accServerMapping.end(); iter_acc++)
	{
		//Search for the current computer account in current domain
		tempFilter = "(&";
		tempFilter += pMyFilter;
		tempFilter += "(CN=";
		tempFilter += iter_acc->first;
		tempFilter += "))";
		
		// Check if any server name corresponding to a computer account is provided
	
		ULONG ulSearchSizeLimit = 0;	// Limit number of users fetched at a time
		LDAP_TIMEVAL ltConnTimeout;
		ltConnTimeout.tv_sec = 0;		//CONNECTION_ESTABLISHMENT_TIMEOUT;	
		bool bSearchErr = false, bConnectErr = false;		

		do
		{
			errorCode = ldap_search_s(pLdapConnection,						
					(const PCHAR) domainDN.c_str(),			// DN to start search
					LDAP_SCOPE_SUBTREE,						// Scope
					(const PCHAR) tempFilter.c_str(),		// Filter
					pAttributes,							// Retrieve list of attributes
					0,										// Get both attributes and values
					&pSpnList);						// [out] Search results
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
				}

     			if( (pLdapConnection = ad.getLdapConnectionCustom(domainDN, ulSearchSizeLimit, ltConnTimeout)) == NULL )
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
				bSearchErr = true;
				break;
			}
		}while(1);

		if( bConnectErr )
		{
			cout << "[INFO]: Maximum connection attempts " << MAX_LDAP_CONNECT_ATTEMPTS << " has been reached...\n";
			cout << "[ERROR]: Unable to connect to directory server." << endl;
			return 0;
		}

		if( bSearchErr )
		{
			cout << "[ERROR] Error encountered while searching directory server.\n";
			return 0;
		}
				
		numberOfEntries = ldap_count_entries(pLdapConnection, pSpnList);
		if(numberOfEntries == -1)
		{
			cout << "ldap_count_entries failed \n";
			if(pSpnList != NULL) 
			{
				ldap_msgfree(pSpnList);
				return false;
			}
		}
		//else
		//	cout << "The search result size for filter: " << tempFilter.c_str() << " is " << numberOfEntries << endl;

		if( numberOfEntries )
		{				
			if( iter_acc->second.empty())
			{
				if( modifyEntriesInComputerAccount(OPERATION_ADDSPN, iter_acc->first, string(""), pLdapConnection, pSpnList, domainName, serverDomainName) )
					return false;
			}
			else
			{
				list<string>::const_iterator iter_server = iter_acc->second.begin();
				for(; iter_server != iter_acc->second.end(); iter_server++)
				{
					ULONG ulSearchSizeLimit = 0;	// Limit number of users fetched at a time
					LDAP_TIMEVAL ltConnTimeout;
					ltConnTimeout.tv_sec = 0;		//CONNECTION_ESTABLISHMENT_TIMEOUT;	
					bool bSearchErr = false, bConnectErr = false;		

					do
					{
						errorCode = ldap_search_s(pLdapConnection,						// Session handle
							(const PCHAR) domainDN.c_str(),			// DN to start search
							LDAP_SCOPE_SUBTREE,						// Scope
							(const PCHAR) tempFilter.c_str(),		// Filter
							pAttributes,							// Retrieve list of attributes
							0,										// Get both attributes and values
							&pSpnList);						// [out] Search results						if( errorCode == LDAP_SUCCESS )
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
							}

     						if( (pLdapConnection = ad.getLdapConnectionCustom(domainDN, ulSearchSizeLimit, ltConnTimeout)) == NULL )
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
							bSearchErr = true;
							break;
						}
					}while(1);

					if( bConnectErr )
					{
						cout << "[INFO]: Maximum connection attempts " << MAX_LDAP_CONNECT_ATTEMPTS << " has been reached...\n";
						cout << "[ERROR]: Unable to connect to directory server." << endl;
						return 0;
					}

					if( bSearchErr )
					{
						cout << "[ERROR] Error encountered while searching directory server.\n";
						return 0;
					}
					
					numberOfEntries = ldap_count_entries(pLdapConnection, pSpnList);
					if(numberOfEntries == -1)
					{
						cout << "ldap_count_entries failed \n";
						if(pSpnList != NULL) 
						{
							ldap_msgfree(pSpnList);
							return false;
						}
					}
					//else
					//	cout << "The search result size for filter: " << tempFilter.c_str() << " is " << numberOfEntries << endl;

			        if(modifyEntriesInComputerAccount(OPERATION_ADDSPN, iter_acc->first, *iter_server, pLdapConnection, pSpnList, domainName, serverDomainName))
						return false;
				}
			}            
		}
		else
		{
			cout << "\nWARNING: Could not find the computer " << iter_acc->first.c_str() << " in domain " << domainName.c_str() << endl;
			return false;
		}
	}

	if( pLdapConnection )
        ldap_unbind_s(pLdapConnection);

	return true;
}

ULONG DeleteSpnEntries(map< string, list<string> > accServerMapping, string domainName, string domainDN, const string& serverDomainName)
{
	if(domainName.empty() && domainDN.empty())
	{
		TCHAR buffer[256] = TEXT("");
		TCHAR szDescription[32] = TEXT("DNS fully-qualified");
		DWORD dwSize = sizeof(buffer);
		
		// "ComputerNameDnsFullyQualified" is of enum NameType COMPUTER_NAME_FORMAT 
		// is the input parameter for output of fully-qualified DNS name that uniquely identifies 
		// the local computer or 
		// the cluster associated with the local computer. 

		if (!GetComputerNameEx((COMPUTER_NAME_FORMAT)3, buffer, &dwSize))
		{
			_tprintf(TEXT("GetComputerNameEx failed (%d)\n"), GetLastError());
			return 0;
		}
		//else 
		//	_tprintf(TEXT("%s: %s\n"), szDescription, buffer);
		
		size_t firstDot = 0;
		string hostName = string(buffer);		
		size_t foundDot = 0;

		if( (firstDot = hostName.find_first_of('.', 0)) != string::npos )
		{
			domainName.assign(hostName.c_str(), firstDot+1, hostName.length()-1-firstDot);			
			domainDN = "DC=" + domainName;
			while ( (foundDot = domainDN.find_first_of('.', 0)) != string::npos )			
				domainDN.replace(foundDot, 1, ",DC=");
			hostName.assign(hostName.c_str(), firstDot);					
			//cout << "hostName=" << hostName.c_str() << endl;
			//cout << "domainName =" << domainName.c_str() << endl;
			//cout << "domainDN =" << domainDN.c_str() << endl;
		}			
		dwSize = sizeof(buffer);
	}

	// get an Ldap connection
	LDAP* pLdapConnection = ad.getLdapConnection();
	if (pLdapConnection == NULL)
	{
		cout << "Failed to connect to the ldap server\n";
		return false;
	}

	// ldap bind to the domain DN
	ULONG errorCode = LDAP_SUCCESS;
	ULONG lRtn = 0;
	lRtn = ldap_bind_s(	pLdapConnection,
				(const PCHAR) domainDN.c_str(),				// Use the naming context from the input list
				NULL,										// Credentials structure
				LDAP_AUTH_NEGOTIATE);						// Auth mode
	if(lRtn != LDAP_SUCCESS)
	{
		cout << "ldap_bind_s failed.\nERROR:[" << lRtn << "] " << ldap_err2string(errorCode)<< endl;	
		return false;
	}

	string strDCName = ad.getLDAPServerName(pLdapConnection);
	if(!strDCName.empty())
		cout << endl << "Connected DC: " << strDCName << endl;

	// search source and target computers
	LDAPMessage* pSpnList;
	PCHAR pMyFilter = "(objectClass=Computer)";
	PCHAR pAttributes[2];
	pAttributes[0] = (PCHAR)EXCHANGE_SERVICE_PRINCIPAL_NAME;
	pAttributes[1] = NULL;
	ULONG numberOfEntries;
	string tempFilter;
	int test = 0;
		
	map< string, list<string> >::const_iterator iter_acc = accServerMapping.begin();
	for(; iter_acc != accServerMapping.end(); iter_acc++)
	{
		//Search for the current computer account in current domain
		tempFilter = "(&";
		tempFilter += pMyFilter;
		tempFilter += "(CN=";
		tempFilter += iter_acc->first;
		tempFilter += "))";
		
		pSpnList = NULL;
		ULONG ulSearchSizeLimit = 0;	// Limit number of users fetched at a time
		LDAP_TIMEVAL ltConnTimeout;
		ltConnTimeout.tv_sec = 0;		//CONNECTION_ESTABLISHMENT_TIMEOUT;	
		bool bSearchErr = false, bConnectErr = false;		

		do
		{
			errorCode = ldap_search_s(pLdapConnection,						// Session handle
					(const PCHAR) domainDN.c_str(),			// DN to start search
					LDAP_SCOPE_SUBTREE,						// Scope
					(const PCHAR) tempFilter.c_str(),		// Filter
					pAttributes,							// Retrieve list of attributes
					0,										// Get both attributes and values
					&pSpnList);						// [out] Search results
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
				}

     			if( (pLdapConnection = ad.getLdapConnectionCustom(domainDN, ulSearchSizeLimit, ltConnTimeout)) == NULL )
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
				bSearchErr = true;
				break;
			}
		}while(1);

		if( bConnectErr )
		{
			cout << "[INFO]: Maximum connection attempts " << MAX_LDAP_CONNECT_ATTEMPTS << " has been reached...\n";
			cout << "[ERROR]: Unable to connect to directory server." << endl;
			return 0;
		}

		if( bSearchErr )
		{
			cout << "[ERROR] Error encountered while searching directory server.\n";
			return 0;
		}

		numberOfEntries = ldap_count_entries(pLdapConnection, pSpnList);
		if(numberOfEntries == -1)
		{
			cout << "ldap_count_entries failed \n";
			if(pSpnList != NULL) 
			{
				ldap_msgfree(pSpnList);
				return false;
			}
		}
		//else
		//	cout << "The search result size for filter: " << tempFilter.c_str() << " is " << numberOfEntries << endl;

		if( numberOfEntries )
		{				
			// Check if any server name is entered corresponding to a computer account		
			if( iter_acc->second.empty())
			{
				if( modifyEntriesInComputerAccount(OPERATION_DELETESPN, iter_acc->first, string(""), pLdapConnection, pSpnList, domainName, serverDomainName) )
					return false;
			}		
			else
			{
				list<string>::const_iterator iter_server = iter_acc->second.begin();
				for(; iter_server != iter_acc->second.end(); iter_server++)
				{
					pSpnList = NULL;
					ULONG ulSearchSizeLimit = 0;	// Limit number of users fetched at a time
					LDAP_TIMEVAL ltConnTimeout;
					ltConnTimeout.tv_sec = 0;		//CONNECTION_ESTABLISHMENT_TIMEOUT;	
					bool bSearchErr = false, bConnectErr = false;		

					do
					{
						errorCode = ldap_search_s(pLdapConnection,	// Session handle
							(const PCHAR) domainDN.c_str(),			// DN to start search
							LDAP_SCOPE_SUBTREE,						// Scope
							(const PCHAR) tempFilter.c_str(),		// Filter
							pAttributes,							// Retrieve list of attributes
							0,										// Get both attributes and values
							&pSpnList);						// [out] Search results

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
							}

     						if( (pLdapConnection = ad.getLdapConnectionCustom(domainDN, ulSearchSizeLimit, ltConnTimeout)) == NULL )
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
							bSearchErr = true;
							break;
						}
					}while(1);

					if( bConnectErr )
					{
						cout << "[INFO]: Maximum connection attempts " << MAX_LDAP_CONNECT_ATTEMPTS << " has been reached...\n";
						cout << "[ERROR]: Unable to connect to directory server." << endl;
						return 0;
					}

					if( bSearchErr )
					{
						cout << "[ERROR] Error encountered while searching directory server.\n";
						return 0;
					}
							
					numberOfEntries = ldap_count_entries(pLdapConnection, pSpnList);
					if(numberOfEntries == -1)
					{
						cout << "ldap_count_entries failed \n";
						if(pSpnList != NULL) 
						{
							ldap_msgfree(pSpnList);
							return false;
						}
					}
					//else
					//	cout << "The search result size for filter: " << tempFilter.c_str() << " is " << numberOfEntries << endl;

					if(modifyEntriesInComputerAccount(OPERATION_DELETESPN, iter_acc->first, *iter_server, pLdapConnection, pSpnList, domainName, serverDomainName ))
						return false;
				}
			}
		}
		else
		{
			cout << "\nWARNING: Could not find the computer " << iter_acc->first.c_str() << " in domain " << domainName.c_str() << endl;
			return false;
		}
	}

	if( pLdapConnection )
        ldap_unbind_s(pLdapConnection);

	return true;
}

ULONG modifyEntriesInComputerAccount(bool operation, const string compAccount, const string serverNameParam,
									 LDAP* pLdapConnection, LDAPMessage* pSpnList, string domainName, const string& serverDomainName)
{
	ULONG errorCode = LDAP_SUCCESS;
	LDAPMessage* pEntry = NULL;
	string serverName;
	if(serverNameParam.empty())
		serverName = compAccount.c_str();
	else
		serverName = serverNameParam.c_str();

	string searchFor;
	bool accountExists = false;
	while(1)
	{
		if(pEntry == NULL)
			pEntry = ldap_first_entry(pLdapConnection, pSpnList);
		else
			pEntry = ldap_next_entry(pLdapConnection, pEntry);

		if(pEntry == NULL)
			break;
		
		int entriesExists = 1;		
		string entryDN = ldap_get_dn(pLdapConnection, pEntry);
		string exchServerCNLowerCase = ad.convertoToLower(compAccount, compAccount.length());
		string entryDNLowerCase = ad.convertoToLower(entryDN, entryDN.length());
		
        searchFor = string("cn=") + exchServerCNLowerCase + ",";
		if(strstr(entryDNLowerCase.c_str(), searchFor.c_str()) == NULL)//TODO:convert fqdn to serverCN and strstr entryDN for serverCN
		{
			//cout << "entryDN = " << entryDNLowerCase.c_str() << "\n serverCNLowerCase = " << searchFor.c_str() << endl;
			searchFor.clear();
			//move to the next entry			
			continue;
		}

		string attrValue;
		BerElement* pBer = NULL;
		PCHAR pAttribute = NULL;
		PCHAR* ppValue = NULL;

		cout << "\n*****Processing Computer DN " << entryDN.c_str() << "\n\n";

		string fqn_hostName;
		if( serverDomainName.empty() )
			fqn_hostName = serverName + string(".") + domainName;
		else
			fqn_hostName = serverName + string(".") + serverDomainName;

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
		for ( pAttribute = ldap_first_attribute( pLdapConnection, pEntry, &pBer );
				pAttribute != NULL; pAttribute = ldap_next_attribute( pLdapConnection, pEntry, pBer ) ) 
		{
	    
			// Print each value of the attribute.
			if ((ppValue = ldap_get_values( pLdapConnection, pEntry, pAttribute)) != NULL ) 
			{
				for ( int i = 0; ppValue[i] != NULL; i++ ) 
				{
					//save the values
					//cout << ppValue[i] <<endl;
					attrValues.push_back(string(ppValue[i]));
				}
				// Free memory allocated by ldap_get_values(). 
				ldap_value_free( ppValue );
			}
	
			// Free memory allocated by ldap_first_attribute(). 
			ldap_memfree( pAttribute );
		}

		// Set up the values to be added		
		modSourceAttribute.mod_type = (PCHAR)EXCHANGE_SERVICE_PRINCIPAL_NAME;

		switch(operation)
		{
            case OPERATION_ADDSPN:	
					modSourceAttribute.mod_op = LDAP_MOD_ADD;					
					break;

            case OPERATION_DELETESPN:	
					modSourceAttribute.mod_op = LDAP_MOD_DELETE;		
					break;	
		}
		
		for (int i=0; i < 3; i++)
		{
			value = string(exchEntries[i]) + "/" + serverName;
			if (ad.exists(value, attrValues) == operation) {
                size_t datalen;
                INM_SAFE_ARITHMETIC(datalen = (InmSafeInt<size_t>::Type(sizeof(char)) * value.length()) + 1, INMAGE_EX(sizeof(char))(value.length()))
				data[j] = (char *)malloc(datalen);
				inm_strcpy_s(data[j++],(value.length() + 1), value.c_str());
				cout << data[j-1] << endl;
			}	
			else if( !operation )			
				cout << "The SPN attribute value " << value.c_str() << " is already present \n";
			else if( operation )
				cout << "The SPN attribute value " << value.c_str() << " does not exist \n";

			value = string(exchEntries[i]) + "/" + fqn_hostName;
			if (ad.exists(value, attrValues) == operation) {
                size_t datalen;
                INM_SAFE_ARITHMETIC(datalen = (InmSafeInt<size_t>::Type(sizeof(char)) * value.length()) + 1, INMAGE_EX(sizeof(char))(value.length()))
				data[j] = (char *)malloc(datalen);
				inm_strcpy_s(data[j++], (value.length() + 1),value.c_str());
				cout << data[j-1] << endl;
			}						
			else if( !operation )			
				cout << "The SPN attribute value " << value.c_str() << " is already present \n";
			else if( operation )
				cout << "The SPN attribute value " << value.c_str() << " does not exist \n";
		}
		value = string(EXCHANGE_HTTP) + "/" + fqn_hostName;
		if (ad.exists(value, attrValues) == operation) {
                size_t datalen;
                INM_SAFE_ARITHMETIC(datalen = (InmSafeInt<size_t>::Type(sizeof(char)) * value.length()) + 1, INMAGE_EX(sizeof(char))(value.length()))
				data[j] = (char *)malloc(datalen);
				inm_strcpy_s(data[j++],(value.length() + 1), value.c_str());
				cout << data[j-1] << "\n\n";			
		}	      
		else if( !operation )			
			cout << "The SPN attribute value " << value.c_str() << " is already present\n";
		else if( operation )
			cout << "The SPN attribute value " << value.c_str() << " does not exist \n";


		data[j] = NULL;			

		// verify there is data to be added
		if (data[0] != NULL) {
			modSourceAttribute.mod_vals.modv_strvals = data;
			//cout << "Modifying Entry DN " << entryDN.c_str() << std::endl;

			errorCode = ldap_modify_s(pLdapConnection, (PCHAR)(entryDN.c_str()), conf);
			if (errorCode != LDAP_SUCCESS) {				
				cout << "Error: ldap_modify_s failed while modifying entry = " << entryDN.c_str()  << std::endl;
				cout << "ERROR:[" << errorCode << "] " << ldap_err2string(errorCode)<< endl;	
				return errorCode;
			}
			else {
				cout << "Successfully set " << EXCHANGE_SERVICE_PRINCIPAL_NAME << " attribute in computer account: " << compAccount.c_str() << std::endl;
			}
		}
		else			
			cout << "\nSkipping the operation of setting " << EXCHANGE_SERVICE_PRINCIPAL_NAME << " attribute in computer account: " << compAccount.c_str() << std::endl;

		// release the memory
		for (int k=0; k < 19; k++)
		{
			if (data[k] != NULL)
				free(data[k]);
		}
	}
	
	return 0; //0 says LDAP_SUCCESS
}



ULONG AddHostSpn(map< string, list<string> > accServerMapping, string domainName, string domainDN, const string& serverDomainName, ULONG& nEntries)
{	
	if(domainName.empty() && domainDN.empty())
	{
		TCHAR buffer[256] = TEXT("");
		TCHAR szDescription[32] = TEXT("DNS fully-qualified");
		DWORD dwSize = sizeof(buffer);
		
		// "ComputerNameDnsFullyQualified" is of enum NameType COMPUTER_NAME_FORMAT(3).
		// It is the input parameter for output of fully-qualified DNS name that uniquely identifies 
		// the local computer or 
		// the cluster associated with the local computer. 

		if (!GetComputerNameEx((COMPUTER_NAME_FORMAT)3, buffer, &dwSize))
		{
			_tprintf(TEXT("GetComputerNameEx failed (%d)\n"), GetLastError());
			return 0;
		}
		//else			
			//_tprintf(TEXT("%s: %s\n"), szDescription, buffer);
		
		size_t firstDot = 0;
		size_t foundDot = 0;
		string hostName = string(buffer);		

		if( (firstDot = hostName.find_first_of('.', 0)) != string::npos )
		{
			domainName.assign(hostName.c_str(), firstDot+1, hostName.length()-1-firstDot);			
			domainDN = "DC=" + domainName;
			while ( (foundDot = domainDN.find_first_of('.', 0)) != string::npos )			
				domainDN.replace(foundDot, 1, ",DC=");
			hostName.assign(hostName.c_str(), firstDot);					
			//cout << "hostName=" << hostName.c_str() << endl;
			//cout << "domainName=" << domainName.c_str() << endl;
			//cout << "domainDN=" << domainDN.c_str() << endl;
		}			
		dwSize = sizeof(buffer);		
	}

	// get an Ldap connection
	LDAP* pLdapConnection = ad.getLdapConnection();
	if (pLdapConnection == NULL)
	{
		cout << "Failed to connect to the ldap server\n";
		return false;
	}

	// ldap bind to the domain DN
	ULONG errorCode = LDAP_SUCCESS;
	ULONG lRtn = 0;
	lRtn = ldap_bind_s(	pLdapConnection,							// Session Handle
				(const PCHAR) domainDN.c_str(),				// Use the naming context from the input list
				NULL,										// Credentials structure
				LDAP_AUTH_NEGOTIATE);						// Auth mode
	if(lRtn != LDAP_SUCCESS)
	{
		cout << "ldap_bind_s failed.\nERROR:[" << lRtn << "] " << ldap_err2string(errorCode)<< endl;	
		return false;
	}

	string strDCName = ad.getLDAPServerName(pLdapConnection);
	if(!strDCName.empty())
		cout << endl << "Connected DC: " << strDCName << endl;

	// search source and target computers
	LDAPMessage* pSpnList;
	PCHAR pMyFilter = "(objectClass=Computer)";
	PCHAR pAttributes[2];
	pAttributes[0] = (PCHAR)EXCHANGE_SERVICE_PRINCIPAL_NAME;
	pAttributes[1] = NULL;
	ULONG numberOfEntries;
	string tempFilter;
	int test = 0;

	map< string, list<string> >::const_iterator iter_acc = accServerMapping.begin();
	for(; iter_acc != accServerMapping.end(); iter_acc++)
	{
		//Search for the current computer account in current domain
		tempFilter = "(&";
		tempFilter += pMyFilter;
		tempFilter += "(CN=";
		tempFilter += iter_acc->first;
		tempFilter += "))";
		
		// Check if any server name corresponding to a computer account is provided

		ULONG ulSearchSizeLimit = 0;	// Limit number of users fetched at a time
		LDAP_TIMEVAL ltConnTimeout;
		ltConnTimeout.tv_sec = 0;		//CONNECTION_ESTABLISHMENT_TIMEOUT;	
		bool bSearchErr = false, bConnectErr = false;		

		do
		{
			errorCode = ldap_search_s(pLdapConnection,						
					(const PCHAR) domainDN.c_str(),			// DN to start search
					LDAP_SCOPE_SUBTREE,						// Scope
					(const PCHAR) tempFilter.c_str(),		// Filter
					pAttributes,							// Retrieve list of attributes
					0,										// Get both attributes and values
					&pSpnList);						// [out] Search results
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
				}

     			if( (pLdapConnection = ad.getLdapConnectionCustom(domainDN, ulSearchSizeLimit, ltConnTimeout)) == NULL )
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
				bSearchErr = true;
				break;
			}
		}while(1);

		if( bConnectErr )
		{
			cout << "[INFO]: Maximum connection attempts " << MAX_LDAP_CONNECT_ATTEMPTS << " has been reached...\n";
			cout << "[ERROR]: Unable to connect to directory server." << endl;
			return 0;
		}

		if( bSearchErr )
		{
			cout << "[ERROR] Error encountered while searching directory server.\n";
			return 0;
		}

		numberOfEntries = ldap_count_entries(pLdapConnection, pSpnList);
		if(numberOfEntries == -1)
		{
			cout << "ldap_count_entries failed \n";
			if(pSpnList != NULL) 
			{
				ldap_msgfree(pSpnList);
				return false;
			}
		}
		//else
		//	cout << "The search result size for filter: " << tempFilter.c_str() << " is " << numberOfEntries << endl;

		if( numberOfEntries )
		{
			// Check if any server name corresponding to a computer account is provided
			if( iter_acc->second.empty())
			{
				if( modifyHostEntryInComputerAccount(OPERATION_ADDSPN, iter_acc->first, string(""), pLdapConnection, pSpnList, domainName, serverDomainName) )
					return false;
			}
			else
			{
				list<string>::const_iterator iter_server = iter_acc->second.begin();
				for(; iter_server != iter_acc->second.end(); iter_server++)
				{
					ULONG ulSearchSizeLimit = 0;	// Limit number of users fetched at a time
					LDAP_TIMEVAL ltConnTimeout;
					ltConnTimeout.tv_sec = 0;		//CONNECTION_ESTABLISHMENT_TIMEOUT;	
					bool bSearchErr = false, bConnectErr = false;		

					do
					{
						errorCode = ldap_search_s(
								pLdapConnection,						// Session handle
								(const PCHAR) domainDN.c_str(),			// DN to start search
								LDAP_SCOPE_SUBTREE,						// Scope
								(const PCHAR) tempFilter.c_str(),		// Filter
								pAttributes,							// Retrieve list of attributes
								0,										// Get both attributes and values
								&pSpnList);						// [out] Search results

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
							}

     						if( (pLdapConnection = ad.getLdapConnectionCustom(domainDN, ulSearchSizeLimit, ltConnTimeout)) == NULL )
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
							bSearchErr = true;
							break;
						}
					}while(1);

					if( bConnectErr )
					{
						cout << "[INFO]: Maximum connection attempts " << MAX_LDAP_CONNECT_ATTEMPTS << " has been reached...\n";
						cout << "[ERROR]: Unable to connect to directory server." << endl;
						return 0;
					}

					if( bSearchErr )
					{
						cout << "[ERROR] Error encountered while searching directory server.\n";
						return 0;
					}			

					numberOfEntries = ldap_count_entries(pLdapConnection, pSpnList);
					if(numberOfEntries == -1)
					{
						cout << "ldap_count_entries failed \n";
						if(pSpnList != NULL) 
						{
							ldap_msgfree(pSpnList);
							return false;
						}
					}
					//else
					//	cout << "The search result size for filter: " << tempFilter.c_str() << " is " << numberOfEntries << endl;

					if(modifyHostEntryInComputerAccount(OPERATION_ADDSPN, iter_acc->first, *iter_server, pLdapConnection, pSpnList, domainName, serverDomainName))
						return false;
				}
			}            
		}
		else
		{
			nEntries = numberOfEntries;
			cout << "\nWARNING: Could not find the computer " << iter_acc->first.c_str() << " in domain " << domainName.c_str() << endl;
			return false;
		}
	}

	if( pLdapConnection )
        ldap_unbind_s(pLdapConnection);

	return true;
}

ULONG DeleteHostSpn(map< string, list<string> > accServerMapping, string domainName, string domainDN, const string& serverDomainName, ULONG& nEntries)
{
	if(domainName.empty() && domainDN.empty())
	{
		TCHAR buffer[256] = TEXT("");
		TCHAR szDescription[32] = TEXT("DNS fully-qualified");
		DWORD dwSize = sizeof(buffer);
		
		// "ComputerNameDnsFullyQualified" is of enum NameType COMPUTER_NAME_FORMAT 
		// is the input parameter for output of fully-qualified DNS name that uniquely identifies 
		// the local computer or 
		// the cluster associated with the local computer. 

		if (!GetComputerNameEx((COMPUTER_NAME_FORMAT)3, buffer, &dwSize))
		{
			_tprintf(TEXT("GetComputerNameEx failed (%d)\n"), GetLastError());
			return 0;
		}
		//else 
		//	_tprintf(TEXT("%s: %s\n"), szDescription, buffer);
		
		size_t firstDot = 0;
		string hostName = string(buffer);		
		size_t foundDot = 0;

		if( (firstDot = hostName.find_first_of('.', 0)) != string::npos )
		{
			domainName.assign(hostName.c_str(), firstDot+1, hostName.length()-1-firstDot);			
			domainDN = "DC=" + domainName;
			while ( (foundDot = domainDN.find_first_of('.', 0)) != string::npos )			
				domainDN.replace(foundDot, 1, ",DC=");
			hostName.assign(hostName.c_str(), firstDot);					
			//cout << "hostName=" << hostName.c_str() << endl;
			//cout << "domainName =" << domainName.c_str() << endl;
			//cout << "domainDN =" << domainDN.c_str() << endl;
		}			
		dwSize = sizeof(buffer);
	}

	// get an Ldap connection
	LDAP* pLdapConnection = ad.getLdapConnection();
	if (pLdapConnection == NULL)
	{
		cout << "Failed to connect to the ldap server\n";
		return false;
	}

	// ldap bind to the domain DN
	ULONG errorCode = LDAP_SUCCESS;
	ULONG lRtn = 0;
	lRtn = ldap_bind_s(
				pLdapConnection,							// Session Handle
				(const PCHAR) domainDN.c_str(),				// Use the naming context from the input list
				NULL,										// Credentials structure
				LDAP_AUTH_NEGOTIATE);						// Auth mode
	if(lRtn != LDAP_SUCCESS)
	{
		cout << "ldap_bind_s failed.\nERROR:[" << lRtn << "] " << ldap_err2string(errorCode)<< endl;	
		return false;
	}

	string strDCName = ad.getLDAPServerName(pLdapConnection);
	if(!strDCName.empty())
		cout << endl << "Connected DC: " << strDCName << endl;

	// search source and target computers
	LDAPMessage* pSpnList;
	PCHAR pMyFilter = "(objectClass=Computer)";
	PCHAR pAttributes[2];
	pAttributes[0] = (PCHAR)EXCHANGE_SERVICE_PRINCIPAL_NAME;
	pAttributes[1] = NULL;
	ULONG numberOfEntries;
	string tempFilter;
	int test = 0;
		
	map< string, list<string> >::const_iterator iter_acc = accServerMapping.begin();
	for(; iter_acc != accServerMapping.end(); iter_acc++)
	{		
		//Search for the current computer account in current domain
		tempFilter = "(&";
		tempFilter += pMyFilter;
		tempFilter += "(CN=";
		tempFilter += iter_acc->first;
		tempFilter += "))";

		pSpnList = NULL;
		ULONG ulSearchSizeLimit = 0;	// Limit number of users fetched at a time
		LDAP_TIMEVAL ltConnTimeout;
		ltConnTimeout.tv_sec = 0;		//CONNECTION_ESTABLISHMENT_TIMEOUT;	
		bool bSearchErr = false, bConnectErr = false;		

		do
		{
			errorCode = ldap_search_s(pLdapConnection,						// Session handle
					(const PCHAR) domainDN.c_str(),			// DN to start search
					LDAP_SCOPE_SUBTREE,						// Scope
					(const PCHAR) tempFilter.c_str(),		// Filter
					pAttributes,							// Retrieve list of attributes
					0,										// Get both attributes and values
					&pSpnList);						// [out] Search results
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
				}

     			if( (pLdapConnection = ad.getLdapConnectionCustom(domainDN, ulSearchSizeLimit, ltConnTimeout)) == NULL )
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
				bSearchErr = true;
				break;
			}
		}while(1);

		if( bConnectErr )
		{
			cout << "[INFO]: Maximum connection attempts " << MAX_LDAP_CONNECT_ATTEMPTS << " has been reached...\n";
			cout << "[ERROR]: Unable to connect to directory server." << endl;
			return 0;
		}

		if( bSearchErr )
		{
			cout << "[ERROR] Error encountered while searching directory server.\n";
			return 0;
		}

		numberOfEntries = ldap_count_entries(pLdapConnection, pSpnList);
		if(numberOfEntries == -1)
		{
			cout << "ldap_count_entries failed \n";
			if(pSpnList != NULL) 
			{
				ldap_msgfree(pSpnList);
				return false;
			}
		}
		//else
		//	cout << "The search result size for filter: " << tempFilter.c_str() << " is " << numberOfEntries << endl;

		if( numberOfEntries )
		{
			// Check if any server name is entered corresponding to a computer account		
			if( iter_acc->second.empty())
			{
				if( modifyHostEntryInComputerAccount(OPERATION_DELETESPN, iter_acc->first, string(""), pLdapConnection, pSpnList, domainName, serverDomainName) )
					return false;
			}
			else
			{
				list<string>::const_iterator iter_server = iter_acc->second.begin();
				for(; iter_server != iter_acc->second.end(); iter_server++)
				{
					ULONG ulSearchSizeLimit = 0;	// Limit number of users fetched at a time
					LDAP_TIMEVAL ltConnTimeout;
					ltConnTimeout.tv_sec = 0;		//CONNECTION_ESTABLISHMENT_TIMEOUT;	
					bool bSearchErr = false, bConnectErr = false;		

					do
					{
						errorCode = ldap_search_s(pLdapConnection,		// Session handle
								(const PCHAR) domainDN.c_str(),			// DN to start search
								LDAP_SCOPE_SUBTREE,						// Scope
								(const PCHAR) tempFilter.c_str(),		// Filter
								pAttributes,							// Retrieve list of attributes
								0,										// Get both attributes and values
								&pSpnList);						// [out] Search results
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
							}

     						if( (pLdapConnection = ad.getLdapConnectionCustom(domainDN, ulSearchSizeLimit, ltConnTimeout)) == NULL )
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
							bSearchErr = true;
							break;
						}
					}while(1);

					if( bConnectErr )
					{
						cout << "[INFO]: Maximum connection attempts " << MAX_LDAP_CONNECT_ATTEMPTS << " has been reached...\n";
						cout << "[ERROR]: Unable to connect to directory server." << endl;
						return 0;
					}

					if( bSearchErr )
					{
						cout << "[ERROR] Error encountered while searching directory server.\n";
						return 0;
					}							
													
					numberOfEntries = ldap_count_entries(pLdapConnection, pSpnList);
					if(numberOfEntries == -1)
					{
						cout << "ldap_count_entries failed \n";
						if(pSpnList != NULL) 
						{
							ldap_msgfree(pSpnList);
							return false;
						}
					}
					//else
					//	cout << "The search result size for filter: " << tempFilter.c_str() << " is " << numberOfEntries << endl;

					if(modifyHostEntryInComputerAccount(OPERATION_DELETESPN, iter_acc->first, *iter_server, pLdapConnection, pSpnList, domainName, serverDomainName))
						return false;
				}
			}
		}
		else
		{
			nEntries = numberOfEntries;
			cout << "\nWARNING: Could not find the computer " << iter_acc->first.c_str() << " in domain " << domainName.c_str() << endl;
			return false;
		}
	}

	if( pLdapConnection )
        ldap_unbind_s(pLdapConnection);

	return true;
}

ULONG modifyHostEntryInComputerAccount(bool operation, const string compAccount, const string serverNameParam,
									 LDAP* pLdapConnection, LDAPMessage* pSpnList, string domainName, const string& serverDomainName)
{
	ULONG errorCode = LDAP_SUCCESS;
	LDAPMessage* pEntry = NULL;
	string serverName;
	if(serverNameParam.empty())
		serverName = compAccount.c_str();
	else
		serverName = serverNameParam.c_str();

	string searchFor;

	while(1)
	{
		if(pEntry == NULL)
			pEntry = ldap_first_entry(pLdapConnection, pSpnList);
		else
			pEntry = ldap_next_entry(pLdapConnection, pEntry);

		if(pEntry == NULL)
			break;
		
		int entriesExists = 1;		
		string entryDN = ldap_get_dn(pLdapConnection, pEntry);
		string exchServerCNLowerCase = ad.convertoToLower(compAccount, compAccount.length());
		string entryDNLowerCase = ad.convertoToLower(entryDN, entryDN.length());
		
        searchFor = string("cn=") + exchServerCNLowerCase + ",";
		if(strstr(entryDNLowerCase.c_str(), searchFor.c_str()) == NULL)//TODO:convert fqdn to serverCN and strstr entryDN for serverCN
		{
			//cout << "entryDN = " << entryDNLowerCase.c_str() << "\n serverCNLowerCase = " << searchFor.c_str() << endl;
			searchFor.clear();
			//move to the next entry			
			continue;
		}

		string attrValue;
		BerElement* pBer = NULL;
		PCHAR pAttribute = NULL;
		PCHAR* ppValue = NULL;

		cout << "\n*****Processing Computer DN " << entryDN.c_str() << "\n\n";

		string fqn_hostName;
		if( serverDomainName.empty() )
            fqn_hostName = serverName + string(".") + domainName;
		else
			fqn_hostName = serverName + string(".") + serverDomainName;

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
		for ( pAttribute = ldap_first_attribute( pLdapConnection, pEntry, &pBer );
				pAttribute != NULL; pAttribute = ldap_next_attribute( pLdapConnection, pEntry, pBer ) ) 
		{
	    
			// Print each value of the attribute.
			if ((ppValue = ldap_get_values( pLdapConnection, pEntry, pAttribute)) != NULL ) 
			{
				for ( int i = 0; ppValue[i] != NULL; i++ ) 
				{
					//save the values
					//cout << ppValue[i] <<endl;
					attrValues.push_back(string(ppValue[i]));
				}
				// Free memory allocated by ldap_get_values(). 
				ldap_value_free( ppValue );
			}
	
			// Free memory allocated by ldap_first_attribute(). 
			ldap_memfree( pAttribute );
		}

		// Set up the values to be added		
		modSourceAttribute.mod_type = (PCHAR)EXCHANGE_SERVICE_PRINCIPAL_NAME;

		switch(operation)
		{
            case OPERATION_ADDSPN:	
					modSourceAttribute.mod_op = LDAP_MOD_ADD;					
					break;

            case OPERATION_DELETESPN:	
					modSourceAttribute.mod_op = LDAP_MOD_DELETE;		
					break;	
		}
		
		value = string(hostEntry) + "/" + serverName;
		if (ad.exists(value, attrValues) == operation) {
            size_t datalen;
            INM_SAFE_ARITHMETIC(datalen = (InmSafeInt<size_t>::Type(sizeof(char)) * value.length()) + 1, INMAGE_EX(sizeof(char))(value.length()))
			data[j] = (char *)malloc(datalen);
			inm_strcpy_s(data[j++], (value.length() + 1),value.c_str());
			cout << data[j-1] << endl;
		}	
		else if( !operation )			
			cout << "The SPN attribute value " << value.c_str() << " is already present \n";
		else if( operation )
			cout << "The SPN attribute value " << value.c_str() << " does not exist \n";
		value = string(hostEntry) + "/" + fqn_hostName;
		if (ad.exists(value, attrValues) == operation) {
            size_t datalen;
            INM_SAFE_ARITHMETIC(datalen = (InmSafeInt<size_t>::Type(sizeof(char)) * value.length()) + 1, INMAGE_EX(sizeof(char))(value.length()))
			data[j] = (char *)malloc(datalen);
			inm_strcpy_s(data[j++],(value.length() + 1), value.c_str());
			cout << data[j-1] << endl;
		}						
		else if( !operation )			
			cout << "The SPN attribute value " << value.c_str() << " is already present \n";
		else if( operation )
			cout << "The SPN attribute value " << value.c_str() << " does not exist \n";

		data[j] = NULL;			

		// verify there is data to be added
		if (data[0] != NULL) {
			modSourceAttribute.mod_vals.modv_strvals = data;
			//cout << "Modifying Entry DN " << entryDN.c_str() << std::endl;

			errorCode = ldap_modify_s(pLdapConnection, (PCHAR)(entryDN.c_str()), conf);
			if (errorCode != LDAP_SUCCESS) {				
				cout << "Error: ldap_modify_s failed while modifying entry = " << entryDN.c_str()  << std::endl;
				cout << "ERROR:[" << errorCode << "] " << ldap_err2string(errorCode)<< endl;	
				return errorCode;
			}
			else {
				cout << "\nSuccessfully set " << EXCHANGE_SERVICE_PRINCIPAL_NAME << " attribute in computer account: " << compAccount.c_str() << std::endl;
			}
		}
		else
			cout << "\nSkipping the operation of setting " << EXCHANGE_SERVICE_PRINCIPAL_NAME << " attribute in computer account: " << compAccount.c_str() << std::endl;

		// release the memory
		for (int k=0; k < 19; k++)
		{
			if (data[k] != NULL)
				free(data[k]);
		}
	}	
	
	return 0; //0 says LDAP_SUCCESS
}



int SpnMain(int argc, char* argv[])
{
	ULONG ret = 0;
	int iParsedOptions = 2;	                	
	int operationFlag = -1;		
	vector<char *> vAccountVector;
	char *pAccount;
	char *pMachineName;
	string input;
	string operationEntered;
			
	if(argc == 2)
	{
		spn_usage();
		exit(1);
	}

	for (;iParsedOptions < argc; iParsedOptions++)
    {
        if((argv[iParsedOptions][0] == '-') || (argv[iParsedOptions][0] == '/'))
        {
			if(stricmp((argv[iParsedOptions]+1),OPTION_ADD) == 0)
            {
				iParsedOptions++;
				if((iParsedOptions >= argc) || (argv[iParsedOptions][0] == '-') || (argv[iParsedOptions][0] == '/'))
				{
					spn_usage();
					break;
				}
				operationFlag = OPERATION_ADDSPN;
			
				// Accept all the characters after option -add or -delete as a single string to tokenize
				// allowing the user to enter the input with spaces 
				// This helps when user mistakenly misses "" for the account:server string 
				while( iParsedOptions < argc )
					input += argv[iParsedOptions++];

				// Tokenize the input string to prepare the map of computer account & serverNames
				pAccount = strtok((char *)(input.c_str()), SEPARATOR_SEMICOLON.c_str());

				while(pAccount)
				{
					vAccountVector.push_back(pAccount); //Push Token into vector
					pAccount = strtok(NULL, SEPARATOR_SEMICOLON.c_str());
				}				
			}
			else if(stricmp((argv[iParsedOptions]+1),OPTION_DELETE) == 0)
            {
				iParsedOptions++;
				if( (iParsedOptions >= argc) ||(argv[iParsedOptions][0] == '-')|| (argv[iParsedOptions][0] == '/'))
				{
					spn_usage();
					break;
				}
				operationFlag = OPERATION_DELETESPN;
                
				// Accept all the characters after option -add or -delete as a single string to tokenize
				// allowing the user to enter the input with spaces 
				// This helps when user mistakenly misses "" for the account:server string 
				while( iParsedOptions < argc )
					input += argv[iParsedOptions++];
			
				// Tokenize the input string to prepare the map of computer account & serverNames				
				pAccount = strtok((char *)(input.c_str()), SEPARATOR_SEMICOLON.c_str());

				while(pAccount)
				{
					vAccountVector.push_back(pAccount); //Push Token into vector
					pAccount = strtok(NULL, SEPARATOR_SEMICOLON.c_str());
				}								
            }
			else if(stricmp((argv[iParsedOptions]+1),OPTION_ADDHOST) == 0)
            {
				iParsedOptions++;
				if((iParsedOptions >= argc) || (argv[iParsedOptions][0] == '-') || (argv[iParsedOptions][0] == '/'))
				{
					spn_usage();
					break;
				}
				operationFlag = OPERATION_ADDHOST;
			
				// Accept all the characters after option -addhost or -deletehost as a single string to tokenize
				// allowing the user to enter the input with spaces 
				// This helps when user mistakenly misses "" for the account:server string 
				while( iParsedOptions < argc )
					input += argv[iParsedOptions++];

				// Tokenize the input string to prepare the map of computer account & serverNames
				pAccount = strtok((char *)(input.c_str()), SEPARATOR_SEMICOLON.c_str());

				while(pAccount)
				{
					vAccountVector.push_back(pAccount); //Push Token into vector
					pAccount = strtok(NULL, SEPARATOR_SEMICOLON.c_str());
				}				
			}
			else if(stricmp((argv[iParsedOptions]+1),OPTION_DELETEHOST) == 0)
            {
				iParsedOptions++;
				if( (iParsedOptions >= argc) ||(argv[iParsedOptions][0] == '-')|| (argv[iParsedOptions][0] == '/'))
				{
					spn_usage();
					break;
				}
				operationFlag = OPERATION_DELETEHOST;
                
				// Accept all the characters after option -addhost or -deletehost as a single string to tokenize
				// allowing the user to enter the input with spaces 
				// This helps when user mistakenly misses "" for the account:server string 
				while( iParsedOptions < argc )
					input += argv[iParsedOptions++];
			
				// Tokenize the input string to prepare the map of computer account & serverNames				
				pAccount = strtok((char *)(input.c_str()), SEPARATOR_SEMICOLON.c_str());

				while(pAccount)
				{
					vAccountVector.push_back(pAccount); //Push Token into vector
					pAccount = strtok(NULL, SEPARATOR_SEMICOLON.c_str());
				}								
            }
			else
			{
				cout << "Invalid operation : " << operationEntered.c_str() << endl;
				spn_usage();
				break;
			}
		}
	}

	string accountName;
	list<string> serverNames; // Server which runs the services pertaining to SPN entry
	map< string, list<string> > accServerMapping;	
	string inputPerAccount;
	size_t firstDot = 0;
	string domainDN;
	string domainName;
	string serverDomainName;
	size_t foundDot = 0;

	while(vAccountVector.size() > 0)
	{
		inputPerAccount = vAccountVector.back();
		vAccountVector.pop_back();
		accountName =  strtok((char *)(inputPerAccount.c_str()), SEPARATOR_COLON.c_str());		
		trim(accountName);	
		if( (firstDot = accountName.find_first_of('.', 0)) != string::npos )
		{
			domainName.assign(accountName.c_str(), firstDot+1, accountName.length()-1-firstDot);			

			domainDN = "DC=" + domainName;
			while ( (foundDot = domainDN.find_first_of('.', 0)) != string::npos )			
                domainDN.replace(foundDot, 1, ",DC=");
		
			accountName.assign(accountName.c_str(), firstDot);
		}
		
        string strMachineName;
		
		while( pMachineName = strtok(NULL, SEPARATOR_COMMA.c_str()) )
		{
            strMachineName = pMachineName;
            trim(strMachineName);
            if ((firstDot = strMachineName.find_first_of('.', 0)) != string::npos)
			{
                serverDomainName.assign(strMachineName.c_str(), firstDot + 1, strMachineName.length() - 1 - firstDot);
                strMachineName.assign(strMachineName.c_str(), firstDot);
			}
            serverNames.push_back(strMachineName);
		}		
		accServerMapping[accountName] = serverNames;
		serverNames.clear();
	}
	
	ULONG nEntries = 1;
	switch(operationFlag)
	{
		case OPERATION_ADDSPN:			
			ret = AddSpnEntries(accServerMapping, domainName, domainDN, serverDomainName);
			break;
		case OPERATION_DELETESPN:			
			ret = DeleteSpnEntries(accServerMapping, domainName, domainDN, serverDomainName);
			break;
		case OPERATION_ADDHOST:			
			ret = AddHostSpn(accServerMapping, domainName, domainDN, serverDomainName, nEntries);
			break;
		case OPERATION_DELETEHOST:			
			ret = DeleteHostSpn(accServerMapping, domainName, domainDN, serverDomainName, nEntries);
			break;
		default:
			spn_usage();
	}	
	return ret;
}

void spn_usage()
{	
	printf("Usage:\n");
	printf("Winop.exe SPN [-add | -delete | -addhost | -deletehost] <COMPUTER1>[:<SERVER1>[,<SERVER2>,...]] [; <COMPUTER2>[:<SERVER1>[,<SERVER2>,...]] ]\n\n");
	printf("COMPUTER	The computer account name in the AD server where \n");
	printf("		the SPN attribute values have to be added/deleted \n"); 
	printf("SERVER		Name of the Server running the services whose \n");
	printf("		SPN attribute values have to be added/deleted\n");	
}

