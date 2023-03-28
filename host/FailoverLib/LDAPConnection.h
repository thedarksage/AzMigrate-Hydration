#pragma once

#ifndef LDAP_CONNECTION
#define LDAP_CONNECTION

#include <windows.h>
#include <winldap.h>
#include <Ntldap.h>
#include <Windns.h>
#include <winber.h>
#include <atlbase.h>
#include <atlconv.h>

#include <iostream>
#include <string>
#include <list>
#include <map>
#include <vector>


#include "svtypes.h"

//Exchange Attributes 
#define MS_EXCH_MASTERSERVER_OR_AVAILABILITY_GROUP "msExchMasterServerOrAvailabilityGroup"
#define MS_EXCH_MDB_AVAILABILITYGROUP_LINK	       "msExchMDBAvailabilityGroupLink"
#define MS_EXCH_DATABASE_BEEING_RESTORED           "msExchDatabaseBeingRestored"
#define MS_EXCH_OAB_VIRTUAL_DIR_LINK               "msExchOABVirtualDirectoriesLink"
#define MS_EXCH_HOST_SERVER_LINK                   "msExchHostServerLink"
#define MS_EXCH_EXCHANGE_SERVER                    "msExchExchangeServer"
#define MS_EXCH_OWNING_SERVER                      "msExchOwningServer"
#define MS_EXCH_ROUTING_SMTP_CONNECTOR             "msExchRoutingSMTPConnector"
#define MS_EXCH_SOURCE_BRIDGEHEAD_SERVERS_DN       "msExchSourceBridgeheadServersDN"
#define SERVICE_PRINCIPAL_NAME					   "servicePrincipalName"
#define PROTOCOL_CFG_SMTP_SERVER                   "protocolCfgSMTPServer"
#define	LEGACY_EXCHANGE_DN                         "legacyExchangeDN"
#define MS_EXCH_PATCHMDB                           "msExchPatchMDB"
#define HOME_MTA                                   "homeMTA"
#define OFFLINE_AB_SERVER                          "offLineABServer"
#define OBJECT_DN                                  "distinguishedName"
#define OBJECT_CLASS                               "objectClass"

//Exchange Server Roles
#define MBX_ROLE  "Maolbox Role"
#define CAS_ROLE  "Client Access Role"
#define UMSG_ROLE "Unified Messaging Role"
#define HT_ROLE   "Hub Transport Role"
#define EDGE_ROLE "Edge Transport Role"

const std::string strSPNEntry[] = {"exchangeMDB/",
					  "exchangeRFR/",
					  "exchangeAB/",
					  "SmtpSvc/",
					  "SMTP/"};
//Other constants
#define MAX_SEARCH_ATTEMPTS 10

//This structure maintains a variety of informatio related to the MDB and MDBCopy.
//We will use this structure over the Exchange 2010 Failover Solution frequently.
struct MDBCopyInfo
{
	std::string mdbDN;
	std::string mdbDrive;
	std::string logDrive;
	std::string logBaseName;
	std::string legacyExchangeDN;
	std::string objectGUID;
	BOOL bPublicMDBCopy;
	BOOL bActive;
};

//It is a Helper class for LDAP API.
class LDAPConnection
{
private:
    std::vector<std::string> vRemainingFiltersStack;

	LDAP* m_pLDAPConnection;
	ULONG m_searchSizeLimit;	//SearSizeLimit for the connectiom
	LONG m_tv_sec;				//Time limit form client side
	std::string m_baseDN;		//Bind base DN

	std::string m_hostName;		//LDAPServer Name or Doman name or LDAP server IP

	std::string m_configDN;

	//clears the exixting connection, and establishes the fresh one
	ULONG HandleConnectionError();
	//Return the root domain if exists, other wise current domain name will return.
	std::string getRootDomainName();
	//Returns the domain name of the directory server to which the ldap connection has connected.
	std::string getDNSDomainName();
	//It return true if the entry 'strEntry' exist in the list 'listEntry', and the exixting value will be assigned to 'entryVal'.
	bool isEntryExist(std::string strEntry,std::list<std::string>& listEntry, std::string& entryVal);
	//It return true if the entry 'strEntry' exist in the list 'listEntry'.
	bool isEntryExist(std::string strEntry,std::list<std::string>& listEntry);
	
public:
	void setHost(const std::string &hostName) {	m_hostName = hostName;	}
	std::string getHost() {	return m_hostName;		}
	//Member variables
	std::string szFltUserDefinedFilter;
	std::string szFltCurToken;
	std::string szDefaultFilter;
	std::string m_domainName;
	unsigned int discoverFrom;
	std::string tgtMTAExchServerName;
	std::string srcMTAExchServerName;
	bool m_bAudit;
	//Member function
	LDAPConnection();
	~LDAPConnection(void);
	ULONG EstablishConnection(const std::string &domain = "",const ULONG searchSizeLimit=0,const LONG tv_sec=0);
	ULONG Bind(const char* pDn);
	ULONG CloseConnection(void);
	BOOL  isClosed(void);
	ULONG GetNamingcontexts(std::list<std::string>& namingContexts);
	ULONG GetconfigurationDN();
	ULONG GetServerDN(const std::string svrName,std::string& svrDN);

			/*DISCOVERY RELATED HELPER FUNCTIONS*/

	ULONG GetServerRoles(const char* role,std::list<std::string>& serverRoles);
	ULONG GetExchangeServerList(std::map<std::string,std::list<std::string> >& lstExServerCN);
	ULONG GetDAGorMDBInfo(std::map<std::string, std::map<std::string,std::string> >& MDBs,char **attribute,std::string objClass);
	ULONG GetMDBCopyInfo(const char* base,char **attribute,std::list<std::string>& svrList);
	ULONG GetMDBCopiesAtServer(const std::string svrName,std::map<std::string,MDBCopyInfo>& MDBs);
	std::string getAgentInstallationPath();
	// Returns Exchange Version
	std::string getExchangeVersion(std::string svrName);

			/*FAILOVER RELATED HELPER FUNCTION*/

	// Retrieves the resopnsible MTA server of the specified server
	ULONG GetResponsibleMTAServer(const std::string& svrName, std::string& mtaServer);
	// Returns the list of values for a particular attribute of entry, which matches the specified filter
	ULONG GetAttributeValues(std::list<std::string>& values, std::string Attribute, std::string Filter, std::string baseDN = "");
	//Removes the additional MDB copies of the Mailboxdatabase except the the specified one
	ULONG RemoveExtraMDBCopies(const std::string & mdbDN,const std::string & mdbCopyDN);

	//User Migration to Target the server
	SVERROR failoverExchangeServerUsers(const std::string& sourceExchangeServerName,
													const std::string& destinationExchangeServerName, 
												    bool bAutoRefineFilters,
													bool bDryRun,
													bool bFailover,
													ULONG& nUsersMigrated,
													bool bSldRoot=false);
	//
	SVERROR switchCNValues(const std::string& search,
						   const std::string& replace,
						   const std::string& sourceDN,
						   std::string& replacementDN,
						   ULONG& numReplaced, std::string szToken);
	//
	SVERROR searchAndReplaceToken(const std::string& search, 
						 const std::string& replace, 
						 const std::string& source, 
						 std::string& replacement,
						 ULONG& numReplaced);
	//
	SVERROR switchCNValuesNoSeparator(const std::string& search, 
								  const std::string& replace, 
								  const std::string& sourceDN, 
								  std::string& replacementDN);
	//
	SVERROR switchCNValuesSpecial(const std::string& search, 
							  const std::string& replace,
							  const std::string& sourceDN,
							  std::string& replacementDN,
							  ULONG& numReplaced,
							  std::string szToken);
	//Modifies the AD Entry specified...
	SVERROR modifyADEntry(const std::string& entryDN,
								std::map<std::string, char**>& attrValuePairs,
								std::map<std::string, PBERVAL*>& attrValuePairsBin);
	//Deletes the entries from AD, whose DNs are specified in the list
	ULONG deleteEntryFromAD(const std::list<std::string>& dns,bool bRemoveSubTree=false);
	
	//-------------------------------------------------------------------------------------------------------
	//These two function sets the msExchPatchMDB attribute of a mailstore under the specified exchange server
	//------------------------------------------------------------------------------						//
	ULONG setMsExchPatchMdbFlag(const std::string szTargetServerName, bool bDryRun);						//
	ULONG modifyPatchMDB(PCHAR pMdbDN);																		//
	//--------------------------------------------------------------------------------------------------------
	
	//
	size_t findPosIgnoreCase(const std::string &szSource, const std::string &szSearch);
		
	//
	void InitiallizeFilter(bool bAutoRefineFilters);
	//
	void RefineFilter();
	//
	std::string getNextFilter();

	// Retrieves the DAG(if DAG enabled)/Server DN of specified Exchange server
	ULONG GetMasterServerOravailabilityGroupDN(const std::string &tgtSvrName,std::string& resultDN);

	// Modifies the DN of specified object with newRDN
	ULONG modifyRDN(const std::string oldRDN, const std::string newRDN);

	// Adds value to the multivalued attribute
	ULONG addAttrValue(const std::string& entryDN,const std::string& attrName,const std::string& value);

	// Gathers the list of OABs generated by the specified Exchange server
	ULONG GetOABsOfServer(const std::string ExchServerName, std::list<std::string>& OabDNs);

	// Gets the install path of the Exchange Server specified
	ULONG GetMsExchInstallPath(const std::string &ServerName, std::string& installPath);

	//Changes ServicePrincipalName attribute of the specified computer object
	SVERROR modifySPN(const  std::string& srcExchServerName,
					const  std::string& trgExchServerName,
					bool bRetainDuplicateSPN,
					bool bFailback,
					const  std::string& appName="Exchange2010");
	/*
	/Retrieves the DN of the oboject which meets the filter pattern. valDN is the out param.
	/Give the filter such that only one object will match the patters in the AD, ohterwise you may not get expected result.
	*/
	ULONG GetDN(const std::string &Filter,std::string &valDN,std::string baseDN="");
	// This version of ModifyADEntry will change the value of only one attribute at a time.
	ULONG ModifyADEntry(const std::string& objectDN,const std::string &attribute,std::list<std::string>& attrValue);
};

// A Global function which returns the upper case letters string of the given string.
std::string ToUpper(const std::string szStringToConvert, size_t length);
// A Global function which returns the lower case letters string of the given string.
std::string ToLower(const std::string szStringToConvert,const size_t length);

#endif //LDAP_CONNECTION