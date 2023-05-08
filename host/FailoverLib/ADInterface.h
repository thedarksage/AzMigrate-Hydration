#ifndef ADINTERFACE__H
#define ADINTERFACE__H

#include <windows.h>
#include <winldap.h>
#include <winber.h>
#include <iostream>
#include <string>
#include <sstream>
#include <list>
#include <map>
#include <Windns.h>
#include <vector>
#include <atlbase.h>
#include "svtypes.h"
#include "log/logger.h"
#include "portable.h"


const std::string msExchHomeServerName = "msExchHomeServerName";
const std::string MS_EXCH_ADDRESS_LIST_SERVICE = "msExchAddressListService";
const std::string MS_EXCH_ADDRESS_LIST_SERVICE_LINK = "msExchAddressListServiceLink";
#define EXCHANGE_CONFIGURATION_BASE_PREFIX	"CN=Microsoft Exchange,CN=Services,"
// Script Location Directory relative to Agent Install Path
#define FAILOVER_DIRECTORY "Failover"

//Ldap parameters
#define MAX_LDAP_CONNECT_ATTEMPTS	10
#define MAX_LDAP_BIND_ATTEMPTS	10
#define DELAY_BETWEEN_CONNECT_ATTEMPTS		20
#define CONNECTION_ESTABLISHMENT_TIMEOUT	30
#define QUERY_RESULT_SIZE	200
#define SEARCH_PAGE_SIZE	100
#define MAX_RETRY_ATTEMPTS	10
#define INM_LDAP_MAX_SEARCH_TIME_LIMT_SEC	30
enum recordType
{
	InvalidRecord = 0,
	HostRecord ,
	CNameRecord = 5
};
class ADInterface
{
public:
	ADInterface();
	ADInterface(const std::string& domainName);
	virtual ~ADInterface(void);

	SVERROR failoverExchangeServerUsers(const std::string& sourceExchangeServerName, const std::string& destinationExchangeServerName, 
										bool bAutoRefineFilters, bool bDryRun, const std::list<std::string>& proSGs, const bool bFailover, bool bSldRoot=false);
	std::string getRootDomainName();
	std::string getDNSDomainName();
	SVERROR getNamingContexts(LDAP *pLdapConnection, std::list<std::string>& namingContexts);
	SVERROR switchCNValues(const std::string& search, const std::string& replace, const std::string& sourceDN, std::string& replacementDN, ULONG& numReplaced, std::string szToken);
	SVERROR searchAndReplaceToken(const std::string& search, const std::string& replace, const std::string& source, std::string& replacement, ULONG& numReplaced);
	bool    setMsExchPatchMdbFlag(LDAP *pLdapConnection, std::string szNameContext, std::string szTargetServerName, bool bDryRun,const std::string otherDomain = "");
	SVERROR DNSChangeIP(std::string szSource, std::string szTarget, std::string szIPAddress, std::string szDnsServIp, std::string szDnsDomain, std::string szUserName, std::string szPassword, bool failover );
	//ULONG CheckIfDNSIsEditable(std::string szSource, std::string szTarget, bool bDNSFailover);
	ULONG CheckDNSPermissions(std::string szSource);
	std::string  convertoToLower(const std::string szStringToConvert, size_t length);
	std::string  convertToUpper(const std::string szStringToConvert);
	size_t  findPosIgnoreCase(const std::string szSource,const std::string szSearch);
	void    InitiallizeFilter(bool bAutoRefineFilters);
	void    RefineFilter();
	std::string  getNextFilter();

	SVERROR getExchServerInstallLocation(std::string &path);
	LDAP* getLdapConnection();
	SVERROR switchCNValuesSpecial(const std::string& search, const std::string& replace, const std::string& sourceDN, std::string& replacementDN, ULONG& numReplaced, std::string szToken);
	SVERROR switchCNValuesNoSeparator(const std::string& search, const std::string& replace, const std::string& sourceDN, std::string& replacementDN);
	SVERROR getConfigurationDN(std::string& configurationDN);
	SVERROR getDomainDN(std::string& domainDN);
	SVERROR addEntryToAD(LDAP *pLdapConnection, const std::string& dn, const std::string& objectClass, 
					 std::map<std::string, char**>& attrValuePairs, std::map<std::string, PBERVAL*>& attrValuePairsBin);
	SVERROR modifyADEntry(LDAP *pLdapConnection, const std::string& entryDN, std::map<std::string, char**>& attrValuePairs, std::map<std::string, PBERVAL*>& attrValuePairsBin);
	SVERROR addMailStoresToADPrivate(LDAP *pLdapConnection, const std::string& dn, const std::string& objectClass, 
					 std::map<std::string, char**>& attrValuePairs, std::map<std::string, berval**>& attrValuePairsBin);
	SVERROR addMailStoresToADPublic(LDAP *pLdapConnection, const std::string& dn, const std::string& objectClass, 
					 std::map<std::string, char**>& attrValuePairs, std::map<std::string, berval**>& attrValuePairsBin);
	SVERROR searchAndComparePrivatePublicMailboxStores(const std::string& srcExchServerName, const std::string& tgtExchServerName,
														std::list<std::string>& extraPublicMailStores, std::list<std::string>& extraPrivMailStores,
														const std::list<std::string>& proVolumes, const std::string& pfSrcExchServerName,
														const std::string& pfTgtExchServerName, 
														std::map<std::string, std::string>& srcTotgtVolMapping);
	SVERROR searchEntryinAD(const std::string& exchangeServerName, std::list<std::string>& entryCNList, 
						std::list<std::string>& entryDNList, const std::string& filter);
	
	SVERROR ADInterface::searchSmtpEntryinAD(const std::string& exchangeServerName, 
						std::list<std::string>& entryCNList, 
						std::list<std::string>& entryDNList,
						const std::string& filter);
	SVERROR deleteEntryFromAD(const std::list<std::string>& dns);
	void compareDNs(const std::string& srcExchServerName, const std::string& tgtExchServerName, const std::list<std::string>& srcCNs, 
					const std::list<std::string>& tgtCNs, std::list<std::string>& missingCNs, std::list<std::string>& extraCNs, std::list<std::string>& matchingCNs);
	SVERROR createStorageGroupsOnTarget(const std::string& srcExchServerName, const std::string& tgtExchServerName, 
										const std::string& missingStorageGroups,
										std::map<std::string, std::string>& srcTotgtVolMapping); 
	SVERROR updateMatchingStorageGroupsOnTarget(const std::string& srcExchServerName, const std::string& tgtExchServerName, 
												const std::list<std::string>& matchingStorageGroups, 
												std::map<std::string, std::string> &srcTotgtVolMapping); 
	SVERROR deleteInformationStoreSubtreeFromTarget(const std::list<std::string>& informationStores, const std::list<std::string>& storageGroups,
													const std::list<std::string>& publicStores, const std::list<std::string>& privStores);
	SVERROR createInformationStoreOnTarget(const std::string& srcExchServerName, const std::string& tgtExchServerName);
	SVERROR updateMatchingInformationStoresOnTarget(const std::string& srcExchServerName, const std::string& tgtExchServerName,
						   const std::list<std::string>& informationStores);
	SVERROR searchAndCompareStorageGroups(const std::string& srcExchServerName, const std::string& tgtExchServerName,
										  std::list<std::string>& extraStorageGroups, const std::list<std::string>& proVolumes, 
										  std::map<std::string, std::string>& srcTotgtVolMapping, std::list<std::string>& proSGs,
										  const std::string& sgDN);
	SVERROR searchAndCompareInformationStores(const std::string& srcExchServerName, const std::string& tgtExchServerName,
											std::list<std::string>& extraIS);
	SVERROR createInformationStoresOnTarget(const std::string& srcExchServerName, const std::string& tgtExchServerName, const std::list<std::string>& informationStores); 
	SVERROR createMailStoresPrivate(const std::string& srcExchServerName, const std::string& tgtExchServerName, 
									const std::string& missingMailStoresDN, const std::string& objectClass, 
									std::map<std::string, std::string>& srcTotgtVolMapping,
									const std::string &pfSrcExchServerName="", const std::string& pfTgtExchServerName="");
	SVERROR updateMailStoresPrivate(const std::string& srcExchServerName, const std::string& tgtExchServerName, 
				 					const std::string& matchingMailStoresDN, const std::string& objectClass,
									std::map<std::string, std::string>& srcTotgtVolMapping,
									const std::string &pfSrcExchServerName="", const std::string& pfTgtExchServerName="");
	SVERROR createMailStoresPublic(const std::string& srcExchServerName, const std::string& tgtExchServerName, 
								   const std::string& missingMailStoresDN, const std::string& objectClass,
								   std::map<std::string, std::string>& srcTotgtVolMapping);
	SVERROR updateMailStoresPublic(const std::string& srcExchServerName, const std::string& tgtExchServerName, 
								   const std::string& missingMailStoresDN, const std::string& objectClass,
								   std::map<std::string, std::string>& srcTotgtVolMapping);
	SVERROR createSMTPConnections(const std::string& srcExchServerName, const std::string& tgtExchServerName, const std::list<std::string>& missingMailStoresDN);
	SVERROR createSMTPConnectionObjectInAD(const std::string& srcExchServerName, const std::string& tgtExchServerName, const std::string& objGUID, 
							   const std::string& mtaDN, const std::string& storeDN, const std::string& legacyExchangeDN);
	SVERROR queryAttrValueForDN(const std::string& dn, const std::string& objectGUID, const std::string& attrType, std::string& value);
	SVERROR getMTADN(const std::string& exchangeServerName, std::string& value);
	SVERROR getResponsibleMTAServer(const std::string& exchangeServerName, std::string& mtaServer);
	SVERROR getLegacyExchangeDN(const std::string& srcExchServerName, const std::string& tgtExchServerName, std::string& dn, const std::string& objGUID);
	void BuildGUIDString(LPBYTE pGUID, std::string &value);
	void normalizeObjectGUID(const std::string &objectGUID, std::string& normObjectGUID);
	SVERROR setHomeMDBForMicrosoftSystemAttendat(const std::string& configDN, const std::string& exchServerName, LDAP *pLdapConnection, const std::string& value);
	bool replaceAttributeDryRun(std::string attrName, std::string attrValue, std::string srcExchangeSource, std::string srcExchangeDest, LDAP* pLdapConnection, PCHAR pEntryDN);
	ULONG modifyPatchMDB(LDAP *pLdapConnection,PCHAR pEntryDN);
	ULONG modifyPatchMDBDryRun(LDAP *pLdapConnection,PCHAR pEntryDN);
	SVERROR rerouteRUS(const std::string& srcExchServerName, const std::string& tgtExchServerName);
	SVERROR modifyServicePrincipalName(const std::string& srcExchServerName, const std::string& tgtExchServerName, bool bRetainDuplicateSPN, const std::string& srcIP, const std::string& appName);
	bool exists(const std::string& data, const std::list<std::string>& list);
	std::string getAgentInstallPath(void);
	SVERROR processExchangeTree(const std::string& sourceExchangeServerName, const std::string& destinationExchangeServerName,
								const std::list<std::string>& proVolumes, std::map<std::string, std::string>& srcTotgtVolMapping, const std::string& sgDN, 
								std::list<std::string> &proSGs, bool deleteExchConfigOnTarget,
								const std::string& pfSrcExchServerName, const std::string& pfTgtExchServerName);
	SVERROR getAttrValueList(const std::string& exchangeServerName, std::list<std::string>& attrValueList, const std::string& attrName, 
							const std::string& filter, const std::string& baseDN);
	SVERROR getAttrValue(std::string& attrValue, const std::string& attrName, const std::string& filter, const std::string& baseDN);
	void findProtectedStorageGroups(const std::string& srcServer, const std::string& tgtServer,
									const std::list<std::string>& sgs, const std::list<std::string>& proVolumes, std::list<std::string>& proSGs,
									const std::string& sgDN);
	bool isStorageGroupProtected(const std::string& srcHost, const std::string& sgDN, const std::list<std::string>& proVolumes);
	void findProtectedMailstores(const std::list<std::string>& mailstores, const std::list<std::string>& proVolumes, std::list<std::string>& proMailstores);
	SVERROR getBaseStorageGroupDN(const std::string &host, const std::string &sg, std::string &baseSGDN);
	SVERROR queryCXforVirtualServerName(const std::string& srcHost, const std::string& application, std::string &virServerName);
	SVERROR CompareIPs(std::string szSource, std::string szTarget, bool &bSameIP );
	SVERROR CompareIPs(std::string szSource, DWORD dwIP, bool &bSameIP );
	BOOL FlushDnsCache(void);
	ULONG checkADPrivileges(const std::string szSourceHost);
    SVERROR loadLogConfiguration(const std::string& hostName, std::list<std::string>& logPaths, std::list< std::pair<std::string,std::string> >& logPathVolumePairs);
	SVERROR loadExchConfiguration(const std::string& hostName, std::map <std::string, std::string>& pathVolumeMap);
	void setAuditFlag();
	SVERROR getSystemAttendantHomeMDBValue(const std::string& srcExchServerName, const std::string& tgtExchServerName, const std::string& configDN, LDAP* pLdapConnection, std::string& sysAttHomeMDBValue);
	LDAP* getLdapConnectionCustom(const std::string szContext, ULONG ulSearchSizeLimit, LDAP_TIMEVAL ltConnTimeout, bool bBind=true,const std::string& otherDomain="");

	SVERROR dnsFailoverUsingCname( std::string srcHost,  std::string tgtHost, const std::string& srcIP ,std::string DnsIP,std::string dnsDomain, std::string user, std::string password); 
	ULONG CheckDNSPermissionsForCnameRecord(const std::string&);
	SVERROR getDNSRecordType(std::string tgtHost,bool &bCNameDNSRecord);
	void getDnsServerName(std::string & dnsServer);
	std::string getLDAPServerName(LDAP* const _pLdapConnection = NULL);

	std::string szFltUserDefinedFilter;
	std::string szFltCurToken;
	std::string szDefaultFilter;
	std::string m_domainName;
	unsigned int discoverFrom;
	std::string tgtMTAExchServerName;
	std::string srcMTAExchServerName;

	
private:
    std::vector<std::string> vRemainingFiltersStack;
	bool m_bAudit;
	std::string m_szAgentInstallPath;

//For new CNAME DNS failover
private:
	DNS_RECORDA *queryHostRecordFromDns(char *,IP4_ARRAY*,DNS_STATUS& retStatus, bool bDnsFailback = false);
	void getDomainName(std::string&);
	//DWORD m_hostRecordTtl;
	std::map<std::string,DWORD> m_DnsTtlvalue;
	std::string getRemoteHostIpAddress(const std::string&);
	SVERROR deleteDNSRecordOfHost(const std::string& ,IP4_ARRAY* servers = NULL, bool bDnsFailback = false);
	SVERROR createCnameRecord(const std::string&, std::string tgtHost,IP4_ARRAY* servers = NULL );
	SVERROR createHostRecord(const std::string&,const std::string&,IP4_ARRAY* servers = NULL);
	void getFqdnName(std::string&,const std::string& );
};
#endif // ADINTERFACE__H