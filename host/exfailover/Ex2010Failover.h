#pragma once 

#ifndef EX_FAILOVER_2010
#define EX_FAILOVER_2010

#include "LDAPconnection.h"

class Ex2010Failover
{
protected:
	LDAPConnection m_LdapConnection;

	std::string m_htSource,m_htTarget;
	std::string m_casSource,m_casTarget;

	//Modifies the Exchange configuration in AD(MDB-attributes,MDBCopy-Attributes)
	SVERROR processExchangeTree(const std::string& sourceExchangeServerName,
		const std::string& destinationExchangeServerName);

	//Modifies private and publicMDB configuration
	SVERROR processPrivateAndPublicMailboxStores(const std::string& srcExchServerNameIn,
		const std::string& tgtExchServerNameIn);

	//---------------------------------------------------------------------------------------------
	//Modifies the MDBCopy atrributes of the MDB. If the MDB has more than one MDBcopy,
	//then all the exctra copies will be removed,finally lefts only one MDBCopy which has modified.
	//---------------------------------------------------------------------------------------------
	SVERROR processMDBCopy(const std::string &tgtExchServer,
		const std::string &srcExchServer,
		const std::string &mdbDN,
		const std::string &tgtExchSvrDN);
	/*--------------------------------------------------------------------------------------------------------------
	This function failovers the OABs Generation form source to target Exchange server. The OABs we are modifying are
	currently generating by source Exchange Server.
	---------------------------------------------------------------------------------------------------------------*/
	SVERROR processOABs(const std::string SourceExchServer, const std::string targetExchServer);

	bool isValiedSendConnecotr(const std::string &ExchSvrName, const std::string &SendConnectorDN);

	SVERROR processSMTPSendConnectors(const std::string &SourceExchServer, const std::string &targetExchServer);

	// If the specified server has PublicFolder MDB then returns TRUE, otherwise returns FALSE
	bool isServerHasPfMDBCopy(const std::string &svrName);
	/*-------------------------------------------------------------------------
	/Returns true if the specified svrname is a valied exchange server name.
	/If you specify the server role it checks the exchnage server role also.
	/If role specified is not installed on the server it returns false.
	--------------------------------------------------------------------------*/
	bool isExchangeServer(const std::string& svrName, const std::string& role);
public:
	Ex2010Failover(void);
	~Ex2010Failover(void);

	//Performs the fast failover/failback
	SVERROR FastFailover(const std::string& srcHost,
		const std::string& tgtHost,
		const std::string& szFilter,
		const std::string& appName,
		bool bFilter,
		bool bFailover, 
		bool bFailback,
		bool bRetainDuplicateSPN,
		bool bDryRun);
	//Set CAS Server names
	SVERROR SetCASServer(const std::string &casSource,const std::string &casTarget);
	//Set HT Server names
	SVERROR SetHTServer(const std::string &htSource,const std::string &htTarget);
};
#endif //EX_FAILOVER_2010