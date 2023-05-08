#include "ADInterface.h"
#include "Ex2010Failover.h"
#include "inmsafecapis.h"
#include "inmsafeint.h"
#include "inmageex.h"

using namespace std;

extern bool bSldRoot;

Ex2010Failover::Ex2010Failover(void)
{
	m_casSource = "";
	m_casTarget = "";
	m_htSource  = "";
	m_htTarget  = "";

	//Establish the LDAP connection
	if(LDAP_SUCCESS != m_LdapConnection.EstablishConnection() )
	{
		cout<<"Unable to connect to the directory server."<<endl;
		m_LdapConnection.CloseConnection();// Clear the connection object
	}
	else if(LDAP_SUCCESS != m_LdapConnection.Bind(""))
	{
		cout<<"Unable to connect to the directory server."<<endl;
		m_LdapConnection.CloseConnection();// Clear the connection object
	}
}

Ex2010Failover::~Ex2010Failover(void)
{
	//Performs LDAP connection clean up.
	if(!m_LdapConnection.isClosed())
		m_LdapConnection.CloseConnection();
}
//Modifies the Exchange configuration in AD(MDB-attributes,MDBCopy-Attributes)
SVERROR Ex2010Failover::processExchangeTree(const string& sourceExchangeServerName,
										 const string& destinationExchangeServerName)
{
	cout<<"\n****** Processing MailStores "<<endl;
	if (processPrivateAndPublicMailboxStores(sourceExchangeServerName, destinationExchangeServerName) == SVE_FAIL)
	{
		cout << "Searching and Compare failed for Public and Private MailStores. Failover cannot continue\n";
		return SVE_FAIL;
	}
	cout<<"\n****** successfully Processed MailStores."<<endl;
	//Modify OAB Generating Server "sourceExchangeServerName" with "destinationExchangeServerName"
	cout<<"\n****** Processsing Offline Address Books "<<endl;
	if(processOABs(sourceExchangeServerName,destinationExchangeServerName) == SVE_FAIL)
	{
		cout<<"Processsing Offline Address Books. Failover cannot continue"<<endl;
		return SVE_FAIL;
	}
	cout<<"\n****** Successfully Processed Offline Address Books"<<endl;

	cout<<"\n****** Processing the SMTP Send Connectors"<<endl;
	if(processSMTPSendConnectors(sourceExchangeServerName,destinationExchangeServerName) == SVE_FAIL)
	{
		cout<<"[WARNING]: Failed in processing SMTP Send Connectors."<<endl
			<<"[INFO]: Please add the Target Hub Trasport Server to the SMTP send connectors manually such that the send connectors \
			  will work properly after the Failover/Failback."<<endl;
	}
	cout<<"\n****** Successfully Processed SMTP Send Connrctors"<<endl;

	return SVS_OK;
}
SVERROR Ex2010Failover::processPrivateAndPublicMailboxStores(const string& srcExchServerNameIn,
																   const string& tgtExchServerNameIn)
{
	string srcExchServerName = ToUpper(srcExchServerNameIn,srcExchServerNameIn.length());
	string tgtExchServerName = ToUpper(tgtExchServerNameIn,tgtExchServerNameIn.length());

	std::map<std::string,MDBCopyInfo> srcMDBs;

	//Get the targetserver DN
	std::string tgtServerDN;
	m_LdapConnection.GetServerDN(tgtExchServerName,tgtServerDN);

	//------------------------------------------------------------------------------------------------------------
	//If the target server already has Public folder Database, then its not possible to failover the public folder
	//Database of source servers. In this case we are Deleting the public folder Database configuration on target.
	//------------------------------------------------------------------------------------------------------------
	std::string tgtMTA;
	std::list<std::string> vals;
	//Here we are sending serverDN, undes this hierarchy in AD we have only one mTA object.This finction the DN of the mTA
	m_LdapConnection.GetAttributeValues(vals,"distinguishedName","(objectClass=mTA)",tgtServerDN);
	std::list<std::string>::iterator ValsIter;
	for(ValsIter = vals.begin(); ValsIter != vals.end(); ValsIter++)
		tgtMTA = *ValsIter;

	//Get the msExchMasterServerOrAvailabilityGroup attribute value for the MDBs to failover to TargetServer
	std::string msExchMasterServerOrAvailabilityGroupVal;
	m_LdapConnection.GetMasterServerOravailabilityGroupDN(tgtExchServerName,msExchMasterServerOrAvailabilityGroupVal);

	//Get the MDBs of source exchange server to failover to target exchange server
	if(m_LdapConnection.GetMDBCopiesAtServer(srcExchServerName,srcMDBs) != LDAP_SUCCESS)
	{
		cout<<"[ERROR]: Failed to retrieve MDBs at the server: "<<srcExchServerName<<endl;
		cout<<"ERROR IN: "<<__FUNCTION__<<endl;
		return SVE_FAIL;
	}

	//Failover each MDB gathered...
	std::map<std::string,MDBCopyInfo>::iterator srcMDBIter = srcMDBs.begin();
	std::map<std::string, char**> attrValuePairs;
	std::map<std::string, berval**> attrValuePairsBin;
	while(srcMDBIter != srcMDBs.end())
	{
		attrValuePairs.clear();
		attrValuePairsBin.clear();
		
		std::string strAttrValue;
		ULONG numreplaced;

		//get legacyExchangeDN attribute value;
		//NOTE:This will be changed for next level of support
		m_LdapConnection.switchCNValuesSpecial(srcExchServerName,tgtExchServerName,srcMDBIter->second.legacyExchangeDN,strAttrValue,numreplaced,"/");
		char **pAttr1Values = (char**)calloc(2,sizeof(char*));
        size_t valuelen;
        INM_SAFE_ARITHMETIC(valuelen = InmSafeInt<size_t>::Type(strAttrValue.length())+1, INMAGE_EX(strAttrValue.length()))
		pAttr1Values[0] = (char*)malloc(valuelen);
        inm_strcpy_s(pAttr1Values[0], valuelen, strAttrValue.c_str());
		pAttr1Values[1] = NULL;
		attrValuePairs[std::string(LEGACY_EXCHANGE_DN)]= pAttr1Values;

		//msExchMasterServerOrAvailabilityGroup attribute value
		pAttr1Values = (char**)calloc(2,sizeof(char*));
        INM_SAFE_ARITHMETIC(valuelen = InmSafeInt<size_t>::Type(msExchMasterServerOrAvailabilityGroupVal.length())+1, INMAGE_EX(msExchMasterServerOrAvailabilityGroupVal.length()))
		pAttr1Values[0] = (char*)malloc(valuelen);
        inm_strcpy_s(pAttr1Values[0], valuelen, msExchMasterServerOrAvailabilityGroupVal.c_str());
		pAttr1Values[1] = NULL;
		attrValuePairs[std::string(MS_EXCH_MASTERSERVER_OR_AVAILABILITY_GROUP)]= pAttr1Values;

		//msExchOwningServer attribute value
		pAttr1Values = (char**)calloc(2,sizeof(char*));
        INM_SAFE_ARITHMETIC(valuelen = InmSafeInt<size_t>::Type(tgtServerDN.length())+1, INMAGE_EX(tgtServerDN.length()))
		pAttr1Values[0] = (char*)malloc(valuelen);
        inm_strcpy_s(pAttr1Values[0], valuelen, tgtServerDN.c_str());
		pAttr1Values[1] = NULL;
		attrValuePairs[std::string(MS_EXCH_OWNING_SERVER)]= pAttr1Values;

		//msExchDatabaseBeingRestored attribute value is set to TRUE
		pAttr1Values = (char**)calloc(2,sizeof(char*));
        INM_SAFE_ARITHMETIC(valuelen = InmSafeInt<size_t>::Type(tgtServerDN.length())+1, INMAGE_EX(tgtServerDN.length()))
		pAttr1Values[0] = (char*)malloc(valuelen);
        inm_strcpy_s(pAttr1Values[0], valuelen, "TRUE");
		pAttr1Values[1] = NULL;
		attrValuePairs[std::string(MS_EXCH_DATABASE_BEEING_RESTORED)]= pAttr1Values;

		//msExchPatchMDB attribute value is set to TRUE
		pAttr1Values = (char**)calloc(2,sizeof(char*));
        INM_SAFE_ARITHMETIC(valuelen = InmSafeInt<size_t>::Type(tgtServerDN.length())+1, INMAGE_EX(tgtServerDN.length()))
		pAttr1Values[0] = (char*)malloc(valuelen);
        inm_strcpy_s(pAttr1Values[0], valuelen, "TRUE");
		pAttr1Values[1] = NULL;
		attrValuePairs[std::string(MS_EXCH_PATCHMDB)]= pAttr1Values;

		//If the current MDB is a public MDB then modify the homeMTA atrribute
		if(srcMDBIter->second.bPublicMDBCopy)
		{
			pAttr1Values = (char**)calloc(2,sizeof(char*));
            INM_SAFE_ARITHMETIC(valuelen = InmSafeInt<size_t>::Type(tgtMTA.length())+1, INMAGE_EX(tgtMTA.length()))
			pAttr1Values[0] = (char*)malloc(valuelen);
            inm_strcpy_s(pAttr1Values[0], valuelen, tgtMTA.c_str());
			pAttr1Values[1] = NULL;
			attrValuePairs[std::string(HOME_MTA)]= pAttr1Values;
		}

		//Modify these atrributes for the current MDB
		if(m_LdapConnection.modifyADEntry(srcMDBIter->second.mdbDN,attrValuePairs,attrValuePairsBin) == SVS_OK)
		{
			//Modify the MDBCopies of the current MDB.
			if(processMDBCopy(tgtExchServerName,srcExchServerName,srcMDBIter->second.mdbDN,tgtServerDN) == SVS_OK)
				cout<<"\n\t"<<srcMDBIter->first<<"\t[SUCCEEDED]"<<endl;
			else
				cout<<"\n\t"<<srcMDBIter->first<<"\t[FAILED]"<<endl;
		}else
			cout<<"\n\t"<<srcMDBIter->first<<"\t[FAILED]"<<endl;
		//----------------------------------------------------------------------------------
		//Note: Here i am not stopping the failover process even  the above functions failed,
		//becouse even one MDB failover has disturbed the others will continue failover to
		//target, but with some warnig message...
		//-----------------------------------------------------------------------------------

		//Free the Memory allocated dynamically for attrValuePairs
		std::map<std::string, char**>::iterator attrValuePairsIter = attrValuePairs.begin();
		while(attrValuePairsIter != attrValuePairs.end())
		{
			char** tempPtr = attrValuePairsIter->second;
			for(int i=0; tempPtr[i] != NULL; i++)
				free(tempPtr[i]);
			free(tempPtr);
			attrValuePairsIter++;
		}

		srcMDBIter++;
	}
	return SVS_OK;
}

// If the specified server has PublicFolder MDB then returns TRUE, otherwise returns FALSE
bool Ex2010Failover::isServerHasPfMDBCopy(const std::string &svrName)
{
	std::map<std::string,MDBCopyInfo> MDBs;
	//--------------------------------------------------------------------
	//Gathering all the MDBCopies at the server. If any one of these MDBs are
	//of type publicMDBCopy then return TRUE, otherwise FALSE
	//--------------------------------------------------------------------
	if(m_LdapConnection.GetMDBCopiesAtServer(svrName,MDBs) != LDAP_SUCCESS)
	{
		cout<<"[ERROR]: Failed to retrieve MDBs at the server: "<<svrName<<endl;
		cout<<"ERROR IN: "<<__FUNCTION__<<endl;
		return false;
	}
	std::map<std::string,MDBCopyInfo>::iterator MDBIer = MDBs.begin();
	while(MDBIer != MDBs.end())
	{
		if(MDBIer->second.bPublicMDBCopy)
			return true;
		MDBIer++;
	}
	return false;
}
//---------------------------------------------------------------------------------------------
//Modifies the MDBCopy atrributes of the MDB. If the MDB has more than one MDBcopy,
//then all the exctra copies will be removed,finally lefts only one MDBCopy which has modified.
//---------------------------------------------------------------------------------------------
SVERROR Ex2010Failover::processMDBCopy(const std::string &tgtExchServer,
											 const std::string &srcExchServer,
											 const std::string &mdbDN,
											 const std::string &tgtExchSvrDN)
{
	//The DN format of public/privateMDBCopy is CN=ExchSvrName,CN=MDBXXX,CN=Databases,CN=Exchange Administrative Groups,...
	std::string newRDN = "CN="+tgtExchServer+","+mdbDN;
	std::string oldRDN = "CN="+srcExchServer+","+mdbDN;

	//-----------------------------------------------------------------------------------------
	//In this function we will do the following two things
	// 1-> Change the RDN of MDBCopy i.e oldRDN to newRDN
	// 2-> Modify the msExchHostServerLink atrribute value to DN of the Target Exchange Server
	//-----------------------------------------------------------------------------------------

	if(m_LdapConnection.modifyRDN(oldRDN,newRDN) != LDAP_SUCCESS)
	{
		cout<<"[WARNING]: Failed to modify DN of the MDBCopy: "<<oldRDN<<endl;
		return SVE_FAIL;
	}//Successfully changed the DN

	std::map<std::string, char**> attrValuePairs;
	std::map<std::string, berval**> attrValuePairsBin;

	attrValuePairs.clear();
	attrValuePairsBin.clear();

	char **pAttr1Values = (char**)calloc(2,sizeof(char*));
    size_t valuelen;
    INM_SAFE_ARITHMETIC(valuelen = InmSafeInt<size_t>::Type(tgtExchSvrDN.length())+1, INMAGE_EX(tgtExchSvrDN.length()))
	pAttr1Values[0] = (char*)malloc(valuelen);
	inm_strcpy_s(pAttr1Values[0],(tgtExchSvrDN.length()+1),tgtExchSvrDN.c_str());
	pAttr1Values[1] = NULL;
	attrValuePairs[std::string(MS_EXCH_HOST_SERVER_LINK)]= pAttr1Values;

	//We should use newRDN. Because, just before we have modified it...
	SVERROR ulReturn = m_LdapConnection.modifyADEntry(newRDN,attrValuePairs,attrValuePairsBin);

	/*------------------------------------------------------------------------------->>>
		I--> Removing the extra MDBCopy Objects part will be implemented here ...
	<<<-------------------------------------------------------------------------------*/

	//Free the Memory allocated dynamically for attrValuePairs
	std::map<std::string, char**>::iterator attrValuePairsIter = attrValuePairs.begin();
	while(attrValuePairsIter != attrValuePairs.end())
	{
		char** tempPtr = attrValuePairsIter->second;
		for(int i=0; tempPtr[i] != NULL; i++)
			free(tempPtr[i]);
		free(tempPtr);
		attrValuePairsIter++;
	}

	if(ulReturn != SVS_OK)
	{
		cout<<"[WARNING]: Failed to modify the -msExchHostServerLink- attribute of "<<newRDN<<endl;
		cout<<"HINT: To correct this issue,Please change this attribute's value to the following\n"
			<<newRDN<<endl;
		return SVE_FAIL;
	}
	return SVS_OK;
}
//Performs the Fast Failover/failback
SVERROR Ex2010Failover::FastFailover(const string& sourceHost,
									const string& tgtHost,
									const string& szFilter,
									const string& appName,
									bool bFilter,
									bool bFailover, 
									bool bFailback,
									bool bRetainDuplicateSPN,
									bool bDryRun)
{
	//connect to LDAP Server with current logon user credentials
	if(m_LdapConnection.isClosed())
	{
		cout<<"[ERROR]: Failed to contact the Domain controller."<<endl
			<<"[INFO]: Please verify that the domain controller is reachable form the server: "<<sourceHost<<", and retry the Job."<<endl;
	}

	string srcMTA, tgtMTA;
	if( m_LdapConnection.GetResponsibleMTAServer(sourceHost, srcMTA) != LDAP_SUCCESS )
	{
		cout << "Error while detecting responsible MTA server responsible for source host: " << sourceHost << endl;
		return SVE_FAIL;
	}
	else
	{
		cout << "The MTA server responsible for " << sourceHost << " is " << srcMTA << endl;
	}
	if( m_LdapConnection.GetResponsibleMTAServer(tgtHost, tgtMTA) != LDAP_SUCCESS )
	{
		cout << "Error while detecting responsible MTA server responsible for target host: " << tgtHost << endl;
		return SVE_FAIL;
	}
	else
	{
			cout << "The MTA server responsible for " << tgtHost << " is " << tgtMTA << endl;
	}

	//Validate the MTA associations w.r.t. source & target servers
	if( (strcmpi(sourceHost.c_str(), srcMTA.c_str()) == 0) ^ (strcmpi(tgtHost.c_str(), tgtMTA.c_str()) == 0))
	{
		cout << "[WARNING]: Either source virtual server or target virtual server is not associated with MTA resource...\n";
	}

	m_LdapConnection.srcMTAExchServerName = srcMTA;
	m_LdapConnection.tgtMTAExchServerName = tgtMTA;

	//If user do n't specify the -dryrun option then only change the Exchange Configuration in AD
	if(!bDryRun)
	{
		//Changes the exchneg configuration form source to target in AD
		if(processExchangeTree(sourceHost,tgtHost) != SVS_OK)
		{
			cout<<__FUNCTION__<<": "<<__LINE__<<endl;
			return SVE_FAIL;
		}
	}
	//User should specify any one of the filter option then only we will migrate the users form source to target Exchange server
	if(bFilter)
	{
		//setting the filter
		if(szFilter.empty())
			m_LdapConnection.szDefaultFilter = "(objectCategory=person)(objectClass=user)" "(" + msExchHomeServerName + "=*cn=" + sourceHost.c_str() + ")";
		else
			m_LdapConnection.szFltUserDefinedFilter = szFilter;

		ULONG nUsersMigrated = 0;//Migrated Users count
		cout<<"\n****** Start migrating Users"<<endl;

		//Migrating the users form source to target
		if(m_LdapConnection.failoverExchangeServerUsers(sourceHost,		//Source Exchange Server
													tgtHost,			//Target Exchange Server
													szFilter.empty(),	//if no user defined filter is psecified, then the default filter will be used, other wise it follows the specified filter
													bDryRun,			//DryRun option
													bFailover,			//Failover option
													nUsersMigrated,	//Out param, gives the total number of users migrated form source to target
													bSldRoot)		// If this is true then the user search will happen only in local domain
													!= SVS_OK )
		{
			cout << "Failed to migrate users from server " << sourceHost.c_str() << " to " << tgtHost.c_str() << std::endl;
			return SVE_FAIL;
		}
		cout << "\n****** Successfully migrated a total of " <<nUsersMigrated << " users from server " << sourceHost.c_str() << " to " << tgtHost.c_str() << std::endl;

		//If user do n't specify the -dryrun option then only change the SPN entries
		if(!bDryRun)
		{
			//Failover CAS server: SPN changes
			if (!m_casSource.empty() && !m_casTarget.empty() && strcmpi(m_casSource.c_str(),sourceHost.c_str()) && strcmpi(m_casTarget.c_str(),tgtHost.c_str()))
			{
				if (m_LdapConnection.modifySPN(m_casSource, m_casTarget, bRetainDuplicateSPN, bFailback) == SVE_FAIL)
				{
					cout << "[WARNING]: Failed to update servicePrincipalName in AD. This will cause Authentication failures between outlook clients and Exchange\n";
					DebugPrintf(SV_LOG_DEBUG,"Line(%u): Error %s: ad.modifyServicePrincipalName() has failed.\n",__LINE__,__FUNCTION__);
				}
			}
			// Remove entries from servicePrincipalName attribute from the source computer corresponding to source exchange server
			// and Add entries to servicePrincipalName attribute to the destination computer corresponding to target exchange server
			if (m_LdapConnection.modifySPN(sourceHost, tgtHost, bRetainDuplicateSPN, bFailback) == SVE_FAIL)
				cout << "[WARNING]: Failed to update servicePrincipalName AD. This will cause Authentication failures between outlook clients and Exchange\n"
				<< "[INFO]: To avoid these authentication failures, please change those entries manually."<<endl;

			cout << "\n*******Successfully Updated Computer " <<tgtHost<< " with necessary exchange entries" << std::endl;
		}
	}

	return SVS_OK;
}
/*--------------------------------------------------------------------------------------------------------------
This function failovers the OABs Generation form source to target Exchange server. The OABs we are modifying are
currently generating by source Exchange Server.
---------------------------------------------------------------------------------------------------------------*/
SVERROR Ex2010Failover::processOABs(const std::string SourceExchServer, const std::string targetExchServer)
{
	DebugPrintf(SV_LOG_DEBUG,"Line(%u): BEGIN %s\n",__LINE__,__FUNCTION__);
	std::string targetExchServerDN;
	std::list<std::string> OabDNs;
	std::map<std::string, char**> attrValuePairs;
	std::map<std::string, berval**> attrValuePairsBin;
	ULONG ulReturn = 0;

	if(m_LdapConnection.GetServerDN(targetExchServer,targetExchServerDN) != LDAP_SUCCESS)
	{
		cout<<"[ERROR]: Failed to retrieve the DN of "<<targetExchServer<<endl;
		DebugPrintf(SV_LOG_DEBUG,"Line(%u): TERMINATED %s: Failed to retrieve the Exchange Server DN. m_LdapConnection.GetServerDN() has failed.\n",__LINE__,__FUNCTION__);
		return SVE_FAIL;
	}
	if(m_LdapConnection.GetOABsOfServer(SourceExchServer,OabDNs) != LDAP_SUCCESS)
	{
		cout<<"[ERROR]: Failed to retrieve the OABs generating by the Exchange Server: "<<SourceExchServer<<endl;
		DebugPrintf(SV_LOG_DEBUG,"Line(%u): TERMINATED %s: Failed to retrieve the OABs information. m_LdapConnection.GetOABsOfServer() has failed.\n",__LINE__,__FUNCTION__);
		return SVE_FAIL;
	}
	if(OabDNs.empty())
	{
		cout<<"\t[INFO]: No Offline Address Book(OAB) is available to process."<<endl
			<<"\t\t"<<SourceExchServer<<" is not responsible for generating any OAB."<<endl;
		DebugPrintf(SV_LOG_DEBUG,"Line(%u): END %s\n",__LINE__,__FUNCTION__);
		return SVS_OK;
	}

	//Retrieve the Virtual Directory information of Target CAS Server
	string oabVirDir,casSvrDN;
	list<string> virDirs;
	if(!m_casTarget.empty())
	{
		//Note: Here the validation of the below function is not requred, bcoz we have already validated the m_casSource& m_casTarget
		//at the time of initialization.
		m_LdapConnection.GetServerDN(m_casTarget,casSvrDN);
	}//If the CAS servers are not specified then look for the target server for CAS Role installation,
	// if CAS Role is installed on the target then proceed with the target server as targetCAS.
	else if(isExchangeServer(targetExchServer,CAS_ROLE))
	{
		casSvrDN = targetExchServerDN;
	}
	if(!casSvrDN.empty())
	{
		//In this case,nither targetCAS specified nor terget has CAS Role installed, source and target have the common CAS server then no need to modify Virtual Dir info of OABs.
		//If the user do not specify the target CAS then it is upto the user responsibility to handle the mail access.
		if(LDAP_SUCCESS == m_LdapConnection.GetAttributeValues(virDirs,"distinguishedName","(objectClass=msExchOABVirtualDirectory)",casSvrDN))
		{
			list<string>::const_iterator iterVirDirs = virDirs.begin();
			while(iterVirDirs != virDirs.end())
			{
				oabVirDir = *iterVirDirs;
				iterVirDirs++;
			}
		}
		else
		{
			cout<<"[ERROR]: Failed to retrieve the Virtual Directories information."<<endl;
			DebugPrintf(SV_LOG_DEBUG,"Line(%u): TERMINATED %s: Failed to retrieve the Virtual Directories information. GetAttributeValues() has failed.\n",__LINE__,__FUNCTION__);
			return SVE_FAIL;
		}
	}

	//Update the OABs
	std::list<std::string>::iterator OABsIter = OabDNs.begin();
	while(OABsIter != OabDNs.end())
	{
		attrValuePairs.clear();
		attrValuePairsBin.clear();

		char **pAttrValues = (char**)calloc(2,sizeof(char*));
        size_t valuelen;
        INM_SAFE_ARITHMETIC(valuelen = InmSafeInt<size_t>::Type(targetExchServerDN.length())+1, INMAGE_EX(targetExchServerDN.length()))
		pAttrValues[0] = (char*)malloc(valuelen);
        inm_strcpy_s(pAttrValues[0], valuelen, targetExchServerDN.c_str());
		pAttrValues[1] = NULL;
		attrValuePairs[std::string(OFFLINE_AB_SERVER)]= pAttrValues;

		//Modify the atrributes for the current OAB
		if(m_LdapConnection.modifyADEntry((*OABsIter),attrValuePairs,attrValuePairsBin) == SVS_OK)
		{
			//Modify the OAB's Distribution Virtual directories.
			if(!oabVirDir.empty())
			{
				//If the target CAS virtual directory is available then only add it. 
				//If the oabVirDir is empty means the target CAS information is not available.
				//In this case we are skipping the Vitual Directory modification
				ulReturn = m_LdapConnection.addAttrValue((*OABsIter),MS_EXCH_OAB_VIRTUAL_DIR_LINK,oabVirDir);
				if((ulReturn == LDAP_SUCCESS) || (ulReturn == LDAP_ATTRIBUTE_OR_VALUE_EXISTS))
				{
					cout<<"\n\t[SUCCEEDED]\t"<<(*OABsIter)<<endl;
				}
				else
				{
					cout<<"\n\t[FAILED]\t"<<(*OABsIter)<<endl;
					cout<<"\t[INFO]: Please add the target Client Access Server Virtual Directory to this OAB manually."<<endl;
				}
			}
			else
				cout<<"\n\t[SUCCEEDED]\t"<<(*OABsIter)<<endl;
		}else
			cout<<"\n\t[FAILED]\t"<<(*OABsIter)<<endl;

		//Free the Memory allocated dynamically for attrValuePairs
		std::map<std::string, char**>::iterator attrValuePairsIter = attrValuePairs.begin();
		while(attrValuePairsIter != attrValuePairs.end())
		{
			char** tempPtr = attrValuePairsIter->second;
			for(int i=0; tempPtr[i] != NULL; i++)
				free(tempPtr[i]);
			free(tempPtr);
			attrValuePairsIter++;
		}

		OABsIter++;
	}
	DebugPrintf(SV_LOG_DEBUG,"Line(%u): END %s\n",__LINE__,__FUNCTION__);
	return SVS_OK;
}
SVERROR Ex2010Failover::processSMTPSendConnectors(const std::string &SourceExchServer, const std::string &targetExchServer)
{
	DebugPrintf(SV_LOG_DEBUG,"Line(%u): BEGIN %s\n",__LINE__,__FUNCTION__);
	//Retrieve the protocolCfgSMTPServer DN of target HT server
	ULONG ulReturn = 0;
	string protocolCfgSmtpServerDN,htTargetDN,htSourceSvr;
	if(!m_htTarget.empty())
	{
		ulReturn = m_LdapConnection.GetServerDN(m_htTarget,htTargetDN);
		//No need to validate the m_htSource is empty or not, coz its already validated.
		htSourceSvr = m_htSource;
	}
	else if (isExchangeServer(targetExchServer,HT_ROLE))
	{
		ulReturn = m_LdapConnection.GetServerDN(targetExchServer,htTargetDN);
		htSourceSvr = SourceExchServer;
	}
	if(LDAP_SUCCESS != ulReturn)
	{
		cout<<"[ERROR]: Failed to retrieve the Target HT serverDN."<<endl;
		DebugPrintf(SV_LOG_DEBUG,"Line(%u): TERMINATED %s: Failed to retrieve the Target HT serverDN. GetServerDN() has failed.\n",__LINE__,__FUNCTION__);
		return SVE_FAIL;
	}
	if(htTargetDN.empty())
	{
		//If the user didn't provide the target HT server name in case of non-consolidated exchnage installation at target, 
		// skip the processing of send connector. In this case user is responsible to add the taregt HT server to the Send Connectors.
		DebugPrintf(SV_LOG_DEBUG,"Line(%u): END %s\n",__LINE__,__FUNCTION__);
		return SVS_OK;
	}
	list<string> listProtocolSMTPServer;
	string filter;
	filter.append("(");
	filter.append(OBJECT_CLASS);
	filter.append("=");
	filter.append(PROTOCOL_CFG_SMTP_SERVER);
	filter.append(")");
	ulReturn = m_LdapConnection.GetAttributeValues(listProtocolSMTPServer,OBJECT_DN,filter,htTargetDN);
	if(LDAP_NO_SUCH_OBJECT == ulReturn)
	{
		cout<<"\tNo protocolCfgSmtpServer found under the target HT server."<<endl;
		DebugPrintf(SV_LOG_DEBUG,"Line(%u): END %s: No protocolCfgSmtpServer found under the target HT server.\n",__LINE__,__FUNCTION__);
		return SVS_OK;
	}
	else if(LDAP_SUCCESS != ulReturn)
	{
		cout<<"[ERROR]: Failed to retrieve the protocolCfgSmtpServer information. "<<endl;
		DebugPrintf(SV_LOG_DEBUG,"Line(%u): TERMINATED %s: Failed to retrieve the protocolCfgSmtpServer information.\n",__LINE__,__FUNCTION__);
		return SVE_FAIL;
	}
	list<string>::const_iterator iterProtocolSMTPServer = listProtocolSMTPServer.begin();
	while(iterProtocolSMTPServer != listProtocolSMTPServer.end())
	{
		protocolCfgSmtpServerDN = *iterProtocolSMTPServer;
		iterProtocolSMTPServer++;
	}

	// Now add the protocolCfgSMTPServer DN of target HT server to the send connectors
	list<string> smtpSendConnectors;
	filter.clear();
	filter.append("(");
	filter.append(OBJECT_CLASS);
	filter.append("=");
	filter.append(MS_EXCH_ROUTING_SMTP_CONNECTOR);
	filter.append(")");
	ulReturn = m_LdapConnection.GetAttributeValues(smtpSendConnectors,OBJECT_DN,filter);
	if(LDAP_NO_SUCH_OBJECT == ulReturn)
	{
		cout<<"\tNo send connectors found"<<endl;
		DebugPrintf(SV_LOG_DEBUG,"Line(%u): END %s: No Send Connector found.\n",__LINE__,__FUNCTION__);
		return SVS_OK;
	}
	else if(LDAP_SUCCESS != ulReturn)
	{
		cout<<"[ERROR]: Failed to retrieve the SMTP Send Connectors information. "<<endl;
		DebugPrintf(SV_LOG_DEBUG,"Line(%u): TERMINATED %s: Failed to retrieve the SMTP Send Connectors information.\n",__LINE__,__FUNCTION__);
		return SVE_FAIL;
	}
	//Process each send connector one by one
	list<string>::const_iterator iterSmtpSendConnector = smtpSendConnectors.begin();
	while(iterSmtpSendConnector != smtpSendConnectors.end())
	{
		//Validate that, the send connector is associate with the source HT server. If it is not, then skip processing that connector.
		if(isValiedSendConnecotr(htSourceSvr,*iterSmtpSendConnector))
		{
			ulReturn = m_LdapConnection.addAttrValue(*iterSmtpSendConnector,MS_EXCH_SOURCE_BRIDGEHEAD_SERVERS_DN,protocolCfgSmtpServerDN);
			if(LDAP_SUCCESS == ulReturn || LDAP_ATTRIBUTE_OR_VALUE_EXISTS == ulReturn)
				cout<<"\n\t[SUCCEEDED]\t"<<*iterSmtpSendConnector<<endl;
			else
				cout<<"\n\t[FAILED]\t"<<*iterSmtpSendConnector<<endl;
		}
		iterSmtpSendConnector++;
	}
	DebugPrintf(SV_LOG_DEBUG,"Line(%u): END %s\n",__LINE__,__FUNCTION__);
	return SVS_OK;
}
bool Ex2010Failover::isValiedSendConnecotr(const std::string &ExchSvrName, const std::string &SendConnectorDN)
{
	DebugPrintf(SV_LOG_DEBUG,"Line(%u): BEGIN %s\n",__LINE__,__FUNCTION__);
	bool bReturn = false;
	list<string> protocolCfgSmtpServerDNs;
	string filter;
	filter.append("(");
	filter.append(OBJECT_CLASS);
	filter.append("=");
	filter.append(MS_EXCH_ROUTING_SMTP_CONNECTOR);
	filter.append(")");
	if(LDAP_SUCCESS == m_LdapConnection.GetAttributeValues(protocolCfgSmtpServerDNs,MS_EXCH_SOURCE_BRIDGEHEAD_SERVERS_DN,filter,SendConnectorDN))
	{
		string strSearchPattern = "CN="+ExchSvrName+",";
		//If the above serarch string is part of the send connector's protocolCfgSmtpServerDN attribute value,
		//then this send connector is associated with the specified exchange server i.e valied send connector.
		list<string>::const_iterator iterProtocolCfgSmtpServerDNs = protocolCfgSmtpServerDNs.begin();
		while(iterProtocolCfgSmtpServerDNs != protocolCfgSmtpServerDNs.end())
		{
			if(string::npos != m_LdapConnection.findPosIgnoreCase(*iterProtocolCfgSmtpServerDNs,strSearchPattern))
			{
				bReturn = true;
				break;
			}
			iterProtocolCfgSmtpServerDNs++;
		}
		
	}
	DebugPrintf(SV_LOG_DEBUG,"Line(%u): END %s\n",__LINE__,__FUNCTION__);
	return bReturn;
}
/*-------------------------------------------------------------------------
/Returns true if the specified svrname is a valied exchange server name.
/If you specify the server role it checks the exchnage server role also.
/If role specified is not installed on the server it returns false.
--------------------------------------------------------------------------*/
bool Ex2010Failover::isExchangeServer(const string& svrName, const string& role)
{
	DebugPrintf(SV_LOG_DEBUG,"Line(%u): END %s\n",__LINE__,__FUNCTION__);
	// <ExchServerName , <ServerRoles-List> >  //
	std::map<std::string,std::list<std::string> > Ex2010ServerList;
	if(LDAP_SUCCESS != m_LdapConnection.GetExchangeServerList(Ex2010ServerList))
	{
		cout<<"[ERROR]: Failed to retrieve Exchange Servers in the Domain...\t"<<endl;
		DebugPrintf(SV_LOG_DEBUG,"Line(%u): Error %s: Failed to retrieve Exchange Servers in the Domain...\n",__LINE__,__FUNCTION__);
		return false;// can not continue, simply terminate the function and return failure status.
	}
	std::map<std::string, std::list<std::string> >::iterator SvrIter = Ex2010ServerList.begin();
	while(SvrIter != Ex2010ServerList.end())
	{
		if(0 == strcmpi(SvrIter->first.c_str(),svrName.c_str()))
		{
			// If role is not specified, no need to check the roles of the server
			if(role.empty())
			{
				DebugPrintf(SV_LOG_DEBUG,"Line(%u): END %s\n",__LINE__,__FUNCTION__);
				return true;
			}
			//Check the server role specified
			list<string>::iterator roleIter = SvrIter->second.begin();
			while(roleIter != SvrIter->second.end())
			{
				if(0 == roleIter->compare(role))
				{
					DebugPrintf(SV_LOG_DEBUG,"Line(%u): END %s\n",__LINE__,__FUNCTION__);
					return true;
				}
				roleIter++;
			}
			//server specifed is matched, but role did not match. So return the failure status
			DebugPrintf(SV_LOG_DEBUG,"Line(%u): Warning %s: Could not find server role.\n",__LINE__,__FUNCTION__);
			return false;
		}
		SvrIter++;
	}
	DebugPrintf(SV_LOG_DEBUG,"Line(%u): Warning %s: Could not find server.\n",__LINE__,__FUNCTION__);
	return false;
}
SVERROR Ex2010Failover::SetCASServer(const std::string &casSource,const std::string &casTarget)
{
	DebugPrintf(SV_LOG_DEBUG,"Line(%u): BEGIN %s\n",__LINE__,__FUNCTION__);
	if(!casSource.empty() && !casTarget.empty())
	{
		if(isExchangeServer(casSource,CAS_ROLE))
			m_casSource = casSource;
		else
		{
			cout<<"[WARNING]: "<<casSource<<" is not a valied Exchange Server with CAS Role."<<endl;
			DebugPrintf(SV_LOG_DEBUG,"Line(%u): TERMINATED: specicied source CAS name is not an Exchnage (CAS)server. %s\n",__LINE__,__FUNCTION__);
			return SVE_FAIL;
		}
		if(isExchangeServer(casTarget,CAS_ROLE))
			m_casTarget = casTarget;
		else
		{
			cout<<"[WARNING]: "<<casTarget<<" is not a valied Exchange Server with CAS Role."<<endl;
			DebugPrintf(SV_LOG_DEBUG,"Line(%u): TERMINATED: specicied target CAS name is not an Exchnage (CAS)server. %s\n",__LINE__,__FUNCTION__);
			return SVE_FAIL;
		}
	}
	else if(!casSource.empty() || !casTarget.empty())
	{
		DebugPrintf(SV_LOG_DEBUG,"Line(%u): WARNING: Either the source CAS or target CAS name is empty. %s\n",__LINE__,__FUNCTION__);
		cout<<"[WARNING]: Missing Client Access Server(CAS) Information."<<endl;
	}
	DebugPrintf(SV_LOG_DEBUG,"Line(%u): END %s\n",__LINE__,__FUNCTION__);
	return SVS_OK;
}
SVERROR Ex2010Failover::SetHTServer(const std::string &htSource,const std::string &htTarget)
{
	DebugPrintf(SV_LOG_DEBUG,"Line(%u): BEGIN %s\n",__LINE__,__FUNCTION__);
	if(!htSource.empty() && !htTarget.empty())
	{
		if(isExchangeServer(htSource,HT_ROLE))
			m_htSource = htSource;
		else
		{
			cout<<"[WARNING]: "<<htSource<<" is not a valied Exchange Server with HT Role."<<endl;
			DebugPrintf(SV_LOG_DEBUG,"Line(%u): TERMINATED: specicied source HT name is not an Exchnage (HT)server. %s\n",__LINE__,__FUNCTION__);
			return SVE_FAIL;
		}
		if(isExchangeServer(htTarget,HT_ROLE))
			m_htTarget = htTarget;
		else
		{
			cout<<"[WARNING]: "<<htTarget<<" is not a valied Exchange Server with HT Role."<<endl;
			DebugPrintf(SV_LOG_DEBUG,"Line(%u): TERMINATED: specicied target HT name is not an Exchnage (HT)server. %s\n",__LINE__,__FUNCTION__);
			return SVE_FAIL;
		}
	}
	else if(!htSource.empty() || !htTarget.empty())
	{
		DebugPrintf(SV_LOG_DEBUG,"Line(%u): WARNING: Either the source HT or target HT server name is empty. %s\n",__LINE__,__FUNCTION__);
		cout<<"[WARNING]: Missing Hub Transport(HT) Server Information."<<endl;
	}
	DebugPrintf(SV_LOG_DEBUG,"Line(%u): END %s\n",__LINE__,__FUNCTION__);
	return SVS_OK;
}
