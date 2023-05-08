#include "exchangeldaphelper.h"
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string.hpp>

SVERROR ExchangeLdapHelper::init(const std::string& dnsname)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    SVERROR bRet = SVS_FALSE ;
    if( m_ldapConnection.get() == NULL )
    {
        m_ldapConnection.reset( new ldapConnection(dnsname) ) ;

    }
    m_ldapConnection->init() ;
	bRet = m_ldapConnection->ConnectToServer() ;
    if(bRet == SVS_OK)
		bRet = bindToDn();
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}



SVERROR ExchangeLdapHelper::bindToDn()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    SVERROR bRet = SVS_FALSE ;
    if( m_ldapConnection->getConfigurationDn(m_configDn) == SVS_OK )
    {
        if( m_ldapConnection->Bind(m_configDn) == SVS_OK )
        {
            bRet = SVS_OK ;
        }
    }
    else
    {
        DebugPrintf(SV_LOG_ERROR, "getConfigurationDn Failed\n") ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}

SVERROR ExchangeLdapHelper::findExchangeServerCN(const std::string& hostname, std::list<std::string> & exchangeServerCnList)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    SVERROR bRet = SVS_FALSE ;
    std::list<std::string> attrsList ;
    attrsList.push_back("cn") ;
    
    std::string filter ;
        
    if( hostname.compare("") != 0 )
    {
        filter = "(&(objectclass=msExchExchangeServer)" ;
        filter += "(CN=";
	    filter += hostname ;
	    filter += "))";        
    }
    else
    {
        filter = "(objectclass=msExchExchangeServer)" ;
    }

    std::list<std::multimap<std::string, std::string > > resultSet ;
    if( m_ldapConnection->Search(m_configDn, LDAP_SCOPE_SUBTREE, filter, attrsList, resultSet) == SVS_OK )
    {
        bRet = SVS_OK ;
        std::list<std::multimap<std::string, std::string> >::const_iterator resultSetIter ;
        resultSetIter = resultSet.begin() ;
        while( resultSetIter != resultSet.end() )
        {
            std::multimap<std::string, std::string>::const_iterator attrMapIter = resultSetIter->begin() ;
            while( attrMapIter != resultSetIter->end() )
            {
                if( attrMapIter->first.compare("cn") == 0 )
                {
                    exchangeServerCnList.push_back(attrMapIter->second) ;
                }
                attrMapIter ++ ;
            }
            resultSetIter++ ;
        }
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}
void ExchangeLdapHelper::prepareExchangeServerAttrList(std::list<std::string>& classattrsList)
{
    classattrsList.clear() ;
    classattrsList.push_back("cn") ;
    classattrsList.push_back("serialNumber") ;
    classattrsList.push_back("distinguishedName") ;
    classattrsList.push_back("serverRole") ;
    classattrsList.push_back("msExchInstallPath") ;
    classattrsList.push_back("msExchDataPath") ;
    classattrsList.push_back("msExchTransportSendProtocolLogPath") ; 
    classattrsList.push_back("msExchTransportReceiveProtocolLogPath") ; 
    classattrsList.push_back("msExchTransportPickupDirectoryPath") ; 
    classattrsList.push_back("msExchTransportMessageTrackingPath") ; 
    classattrsList.push_back("msExchTransportReplayDirectoryPath") ; 
    classattrsList.push_back("msExchTransportConnectivityLogPath") ; 
    classattrsList.push_back("msExchELCAuditLogPath") ; 
    classattrsList.push_back("msExchTransportPipelineTracingPath") ; 
    classattrsList.push_back("msExchTransportRoutingLogPath") ; 
    classattrsList.push_back("msExchCurrentServerRoles") ;
    classattrsList.push_back("msExchClusterStorageType") ;
    classattrsList.push_back("msExchServerRole") ;
    classattrsList.push_back("msExchServerRedundantMachines") ;
    classattrsList.push_back("msExchResponsibleMTAServer") ;
}
SVERROR ExchangeLdapHelper::getExchangeServerAttributes(const std::string& hostname, std::multimap<std::string, std::string>& attrMap)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    SVERROR bRet = SVS_FALSE ;

    std::string filter ;
    filter = "(&(objectclass=msExchExchangeServer)" ;
    filter += "(CN=";
	filter += hostname ;
	filter += "))";        

    std::list<std::string> classattrsList ;
    prepareExchangeServerAttrList(classattrsList) ;

    std::list<std::multimap<std::string, std::string> > resultSet ;
    if( m_ldapConnection->Search(m_configDn, LDAP_SCOPE_SUBTREE, filter, classattrsList, resultSet) == SVS_OK )
    {
        bRet = SVS_OK ;
        std::list<std::multimap<std::string, std::string> >::iterator resultSetIter ;
        resultSetIter = resultSet.begin() ;
        if(resultSetIter != resultSet.end())
		{
			attrMap = *resultSetIter ;
		}

    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}

SVERROR ExchangeLdapHelper::getStorageGroups(const std::string& hostname, std::list<std::string>& storageGroupList)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    SVERROR bRet = SVS_FALSE ;
    
    std::string filter ;
    filter = "(objectclass=msExchStorageGroup)" ;
    std::list<std::string> classattrsList ;
    classattrsList.push_back("distinguishedName") ;
    classattrsList.push_back("cn") ;
    std::list<std::multimap<std::string, std::string> > resultSet ;
    std::string StorageGrpSearchKeyword = "CN=InformationStore,";
    StorageGrpSearchKeyword += "CN=" ;
    StorageGrpSearchKeyword += hostname ;
    StorageGrpSearchKeyword += ",CN=Servers" ;
    boost::algorithm::to_upper(StorageGrpSearchKeyword) ;
    DebugPrintf(SV_LOG_DEBUG, "Search Word : %s\n", StorageGrpSearchKeyword.c_str()) ;
    if( m_ldapConnection->Search(m_configDn, LDAP_SCOPE_SUBTREE, filter, classattrsList, resultSet) == SVS_OK )
    {
        bRet = SVS_OK ;
        std::list<std::multimap<std::string, std::string> >::iterator resultSetIter ;
        resultSetIter = resultSet.begin() ;
        while( resultSetIter != resultSet.end() )
        {
            DebugPrintf(SV_LOG_DEBUG, "dn : %s\n", resultSetIter->find("distinguishedName")->second.c_str());
            std::string dn = resultSetIter->find("distinguishedName")->second ;
            boost::algorithm::to_upper(dn) ;
            DebugPrintf(SV_LOG_DEBUG, "dn after to_upper: %s\n", dn.c_str());
            if( strstr(dn.c_str(), StorageGrpSearchKeyword.c_str()) != NULL )
            {
                storageGroupList.push_back(resultSetIter->find("cn")->second) ;
                DebugPrintf(SV_LOG_DEBUG, "Storage Group Name: %s\n", resultSetIter->find("cn")->second.c_str()) ;
            }
            resultSetIter++ ;
        }
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}

SVERROR ExchangeLdapHelper::getMDBs(const std::string& hostname, const std::string& storageGroupName, ExchangeMdbType mdbType, std::list<std::string>& Mdbs)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    SVERROR bRet = SVS_FALSE ;
    std::string filter ;
    std::list<std::string> classattrsList ;    
    if( mdbType == EXCHANGE_PRIV_MDB )
    {
        filter = "(objectclass=msExchPrivateMDB)" ;
    }
    else
    {
        filter = "(objectclass=msExchPublicMDB)" ;
    }
    classattrsList.push_back("distinguishedName") ;
    classattrsList.push_back("cn") ;


    std::list<std::multimap<std::string, std::string> > resultSet ;
    
    std::string mdbSearchKeyword ;
    mdbSearchKeyword = "CN=" ;
    mdbSearchKeyword += storageGroupName ;
    mdbSearchKeyword += ",CN=InformationStore,CN=" ;
    mdbSearchKeyword += hostname ;
    mdbSearchKeyword += ",CN=Servers" ;

    DebugPrintf(SV_LOG_DEBUG, "Search Word : %s\n", mdbSearchKeyword.c_str()) ;
    boost::algorithm::to_upper(mdbSearchKeyword) ;
    if( m_ldapConnection->Search(m_configDn, LDAP_SCOPE_SUBTREE, filter, classattrsList, resultSet) == SVS_OK )
    {
        bRet = SVS_OK ;
        std::list<std::multimap<std::string, std::string> >::iterator resultSetIter ;
        resultSetIter = resultSet.begin() ;
        while( resultSetIter != resultSet.end() )
        {
            std::string dn = resultSetIter->find("distinguishedName")->second.c_str() ;
            boost::algorithm::to_upper(dn) ;
            if( strstr(dn.c_str(), mdbSearchKeyword.c_str()) != NULL )
            {
                Mdbs.push_back(resultSetIter->find("cn")->second) ;
                DebugPrintf(SV_LOG_DEBUG, "Mdb Name: %s\n", resultSetIter->find("cn")->second.c_str()) ;
			}
            resultSetIter++ ;
        }
    }


    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}
void ExchangeLdapHelper::prepareStorageGrpAttrList(std::list<std::string>& attrList)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	
    attrList.push_back("cn") ;
    attrList.push_back("distinguishedName") ;
    attrList.push_back("msExchESEParamLogFilePath") ;
    attrList.push_back("msExchESEParamSystemPath") ;
    attrList.push_back("msExchESEParamBaseName") ;
    attrList.push_back("msExchHasLocalCopy") ;
    attrList.push_back("msExchESEParamCopyLogFilePath") ;
    attrList.push_back("msExchESEParamCopySystemPath") ;
	attrList.push_back("msExchStandbyCopyMachines") ;

	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}


SVERROR ExchangeLdapHelper::getStorageGroupAttributes(const std::string& hostname, const std::string& storageGrpName, std::multimap<std::string, std::string>& attrMap)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    SVERROR bRet = SVS_FALSE ;
    attrMap.clear() ;    
    std::string filter ;
    filter = "(objectclass=msExchStorageGroup)" ;
    std::list<std::string> classattrsList ;
    prepareStorageGrpAttrList(classattrsList) ;

    std::string StorageGrpSearchKeyword ;
    StorageGrpSearchKeyword = "CN=" ;
    StorageGrpSearchKeyword += storageGrpName;
    StorageGrpSearchKeyword += ",CN=InformationStore";
    StorageGrpSearchKeyword += ",CN=" ;
    StorageGrpSearchKeyword += hostname ;
    StorageGrpSearchKeyword += ",CN=Servers" ;

    DebugPrintf(SV_LOG_DEBUG, "Search Word : %s\n", StorageGrpSearchKeyword.c_str()) ;
    boost::algorithm::to_upper(StorageGrpSearchKeyword) ;
    std::list<std::multimap<std::string, std::string> > resultSet ;
    if( m_ldapConnection->Search(m_configDn, LDAP_SCOPE_SUBTREE, filter, classattrsList, resultSet) == SVS_OK )
    {
        std::list<std::multimap<std::string, std::string> >::iterator resultSetIter ;
        resultSetIter = resultSet.begin() ;
        while( resultSetIter != resultSet.end() )
        {
            std::string dn = resultSetIter->find("distinguishedName")->second.c_str() ;
            boost::algorithm::to_upper(dn) ;
            if( strstr(dn.c_str(), StorageGrpSearchKeyword.c_str()) != NULL )
            {
                if( resultSetIter->find("cn")->second.compare(storageGrpName) == 0 )
                {
                    attrMap = *resultSetIter ;
                    bRet = SVS_OK ;
                }
            }
            resultSetIter++;
        }
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}

void ExchangeLdapHelper::prepareMdbattrList(std::list<std::string>& attrList)
{
    /*
    attrList.push_back("cn") ; 
    attrList.push_back("distinguishedName") ; 
    attrList.push_back("msExchEDBFile") ; 
    attrList.push_back("msExchCopyEDBFile") ; 
    attrList.push_back("msExchEDBOffline") ; 
    attrList.push_back("msExchHasLocalCopy") ;
    */
}

SVERROR ExchangeLdapHelper::getMDBAttributes(const std::string& hostname, const std::string storageGrpName, ExchangeMdbType mdbType, const std::string mdbName, std::multimap<std::string, std::string>& attrMap)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    SVERROR bRet = SVS_FALSE ;

    std::string configDn ;

    std::string filter ;
    if( mdbType == EXCHANGE_PRIV_MDB )
    {
        filter = "(objectclass=msExchPrivateMDB)" ;
    }
    else
    {
        filter = "(objectclass=msExchPublicMDB)" ;
    }
    std::list<std::string> classattrsList ;
    prepareMdbattrList(classattrsList) ;

    std::list<std::multimap<std::string, std::string> > resultSet ;

    std::string mdbSearchKeyword ;
    mdbSearchKeyword = "CN=" ;
    mdbSearchKeyword += mdbName ; 
    mdbSearchKeyword += ",CN=" ; 
    mdbSearchKeyword += storageGrpName ;
    mdbSearchKeyword += ",CN=InformationStore,CN=" ;
    mdbSearchKeyword += hostname ;
    mdbSearchKeyword += ",CN=Servers" ;
    boost::algorithm::to_upper(mdbSearchKeyword) ;
    DebugPrintf(SV_LOG_DEBUG, "Search Word : %s\n", mdbSearchKeyword.c_str()) ;
    if( m_ldapConnection->Search(m_configDn, LDAP_SCOPE_SUBTREE, filter, classattrsList, resultSet) == SVS_OK )
    {
        std::list<std::multimap<std::string, std::string> >::iterator resultSetIter ;
        resultSetIter = resultSet.begin() ;
        while( resultSetIter != resultSet.end() )
        {
            DebugPrintf(SV_LOG_DEBUG, "distinguished name : %s\n", resultSetIter->find("distinguishedName")->second.c_str());
            std::string dn = resultSetIter->find("distinguishedName")->second.c_str() ;
            boost::algorithm::to_upper(dn) ;
            if( strstr(dn.c_str(), mdbSearchKeyword.c_str()) != NULL )
            {
                bRet = SVS_OK ;
                attrMap = *resultSetIter ;
            }
            resultSetIter++ ;
        }
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}

bool ExchangeLdapHelper::isExchangeInstalled(const std::string& hostName)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    bool installed = false ;
    std::list<std::string> exchangeServerCnList ;
    if( findExchangeServerCN(hostName, exchangeServerCnList) == SVS_OK )
    {
        if( exchangeServerCnList.size() != 0 )
        {
            installed = true ;
        }
        else
        {
            DebugPrintf(SV_LOG_DEBUG, "exchange server cn is not found in AD for %s\n", hostName.c_str()) ;
        }
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return installed ;
}

bool ExchangeLdapHelper::isExchangeInstalled(std::list<std::string>& exchHosts)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    bool installed = false ;
    if( findExchangeServerCN("", exchHosts) == SVS_OK )
    {
        if( exchHosts.size() != 0 )
        {
            installed = true ;
        }
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return installed ;
}

SVERROR ExchangeLdapHelper::get2010MDBs(const std::string& hostname, std::list<std::string>& Mdbs)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	SVERROR retStatus = SVS_FALSE;
    std::string filter ;
    std::list<std::string> classattrsList ;    
	filter = "(objectclass=msExchMDBCopy)" ;
    classattrsList.push_back("distinguishedName") ;
    classattrsList.push_back("cn") ;


    std::list<std::multimap<std::string, std::string> > resultSet ;
    
    std::string mdbSearchKeyword ;
    mdbSearchKeyword = "CN=" ;
    mdbSearchKeyword += hostname ;
    mdbSearchKeyword += ",CN=" ;

    DebugPrintf(SV_LOG_DEBUG, "Search Word : %s\n", mdbSearchKeyword.c_str()) ;
    boost::algorithm::to_upper(mdbSearchKeyword) ;
    if( m_ldapConnection->Search(m_configDn, LDAP_SCOPE_SUBTREE, filter, classattrsList, resultSet) == SVS_OK )
    {
        retStatus = SVS_OK ;
        std::list<std::multimap<std::string, std::string> >::iterator resultSetIter ;
        resultSetIter = resultSet.begin() ;
        while( resultSetIter != resultSet.end() )
        {
            std::string dn = resultSetIter->find("distinguishedName")->second.c_str() ;
            boost::algorithm::to_upper(dn) ;
            if( strstr(dn.c_str(), mdbSearchKeyword.c_str()) != NULL )
            {
				std::string MDBName = "";
				dn = dn.substr(mdbSearchKeyword.size());
				MDBName = dn.substr(0, dn.find(","));
                Mdbs.push_back(MDBName) ;
                DebugPrintf(SV_LOG_DEBUG, "Mdb Name: %s\n", MDBName.c_str()) ;
			}
            resultSetIter++ ;
        }
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;	
	return retStatus;
}

SVERROR ExchangeLdapHelper::get2010MDBAttributes(const std::string& hostname, ExchangeMdbType mdbType, const std::string mdbName, std::multimap<std::string, std::string>& attrMap)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    SVERROR bRet = SVS_FALSE ;

    std::string configDn ;

    std::string filter ;
    if( mdbType == EXCHANGE_PRIV_MDB )
    {
        filter = "(objectclass=msExchPrivateMDB)" ;
    }
    else
    {
        filter = "(objectclass=msExchPublicMDB)" ;
    }
    std::list<std::string> classattrsList ;
    prepareMdbattrList(classattrsList) ;

    std::list<std::multimap<std::string, std::string> > resultSet ;

	std::string mdbSearchKeyword2;
	mdbSearchKeyword2 = "CN=" ;
    mdbSearchKeyword2 += mdbName ;
	mdbSearchKeyword2 += ",CN=Databases" ;
	boost::algorithm::to_upper(mdbSearchKeyword2) ;
    if( m_ldapConnection->Search(m_configDn, LDAP_SCOPE_SUBTREE, filter, classattrsList, resultSet) == SVS_OK )
    {
        std::list<std::multimap<std::string, std::string> >::iterator resultSetIter ;
        resultSetIter = resultSet.begin() ;
		DebugPrintf(SV_LOG_DEBUG, "Search Word2 : %s\n", mdbSearchKeyword2.c_str()) ;            
        while( resultSetIter != resultSet.end() )
        {
			DebugPrintf(SV_LOG_DEBUG, "Distinguished Name : %s\n", resultSetIter->find("distinguishedName")->second.c_str());
			std::string dn = resultSetIter->find("distinguishedName")->second.c_str() ;
            boost::algorithm::to_upper(dn) ;
            if( strstr(dn.c_str(), mdbSearchKeyword2.c_str()) != NULL )
            {
                bRet = SVS_OK ;
                attrMap = *resultSetIter ;
				break;
			}
            resultSetIter++ ;
        }
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}

SVERROR ExchangeLdapHelper::get2010MDBCopyHosts( std::string& mdbName, std::list<std::string>& copyHostNameList )
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	SVERROR retStatus = SVS_FALSE;
    std::string filter ;
    std::list<std::string> classattrsList ;    
	filter = "(objectclass=msExchMDBCopy)" ;
    classattrsList.push_back("distinguishedName") ;
    classattrsList.push_back("cn") ;

    std::list<std::multimap<std::string, std::string> > resultSet ;
    
    std::string mdbSearchKeyword ;
    mdbSearchKeyword = "CN=" ;
    mdbSearchKeyword += mdbName ;
    mdbSearchKeyword += ",CN=Databases" ;

    DebugPrintf(SV_LOG_DEBUG, "Search Word : %s\n", mdbSearchKeyword.c_str()) ;
    boost::algorithm::to_upper(mdbSearchKeyword) ;
    if( m_ldapConnection->Search(m_configDn, LDAP_SCOPE_SUBTREE, filter, classattrsList, resultSet) == SVS_OK )
    {
        retStatus = SVS_OK ;
        std::list<std::multimap<std::string, std::string> >::iterator resultSetIter ;
        resultSetIter = resultSet.begin() ;
        while( resultSetIter != resultSet.end() )
        {
            std::string dn = resultSetIter->find("distinguishedName")->second.c_str() ;
			std::string cn = resultSetIter->find("cn")->second.c_str() ;
            boost::algorithm::to_upper(dn) ;
            if( strstr(dn.c_str(), mdbSearchKeyword.c_str()) != NULL )
            {
				DebugPrintf(SV_LOG_DEBUG, "DN: %s\n", dn.c_str()) ;
				DebugPrintf(SV_LOG_DEBUG, "CN: %s\n", cn.c_str()) ;
				copyHostNameList.push_back(cn);
			}
            resultSetIter++ ;
        }
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;	
	return retStatus;	


	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}

SVERROR ExchangeLdapHelper::getDAGParticipantsSereversList( std::string& DAGName, std::list<std::string>& participantsServersList )
{
	DebugPrintf( SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME );
	SVERROR retStatus = SVS_FALSE;
    std::string filter ;
    std::list<std::string> classattrsList ;    
	filter = "(objectclass=msExchExchangeServer)" ;
    classattrsList.push_back("msExchMDBAvailabilityGroupLink") ;
    classattrsList.push_back("cn") ;


    std::list<std::multimap<std::string, std::string> > resultSet ;
    
    std::string mdbSearchKeyword ;
    mdbSearchKeyword = "CN=" ;
    mdbSearchKeyword += DAGName ;
    mdbSearchKeyword += ",CN=Database Availability Groups" ;

    DebugPrintf(SV_LOG_DEBUG, "Search Word : %s\n", mdbSearchKeyword.c_str()) ;
    boost::algorithm::to_upper(mdbSearchKeyword) ;
    if( m_ldapConnection->Search(m_configDn, LDAP_SCOPE_SUBTREE, filter, classattrsList, resultSet) == SVS_OK )
    {
        retStatus = SVS_OK ;
        std::list<std::multimap<std::string, std::string> >::iterator resultSetIter ;
        resultSetIter = resultSet.begin() ;
        while( resultSetIter != resultSet.end() )
        {
			std::string dagGroupLink;
			if( resultSetIter->find("msExchMDBAvailabilityGroupLink") != resultSetIter->end())
			{
				dagGroupLink = resultSetIter->find("msExchMDBAvailabilityGroupLink")->second.c_str() ;
				std::string cn = resultSetIter->find("cn")->second.c_str() ;
				boost::algorithm::to_upper(dagGroupLink) ;
				boost::algorithm::to_upper(cn) ;
				if( strstr(dagGroupLink.c_str(), mdbSearchKeyword.c_str()) != NULL )
				{
					DebugPrintf(SV_LOG_DEBUG, "msExchMDBAvailabilityGroupLink: %s\n", dagGroupLink.c_str()) ;
					DebugPrintf(SV_LOG_DEBUG, "CN: %s\n", cn.c_str()) ;
					participantsServersList.push_back(cn);
				}
			}
            resultSetIter++ ;
        }
    }

	DebugPrintf( SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME );
	return retStatus;
}

SVERROR ExchangeLdapHelper::getServerMaxMailstores(const std::string &serverDN, std::string &nMaxMailstores)
{
	DebugPrintf( SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME );
	SVERROR retStatus = SVS_FALSE;
	do
	{
		if(serverDN.empty())
		{
			DebugPrintf( SV_LOG_WARNING, "Server DN is empty. Can not get Mailsoteres capacity.\n");
			break;
		}
		std::string filter = "(objectclass=msExchInformationStore)";
		std::list<std::string> attributes;
		std::list<std::multimap<std::string,std::string> > result;
		//Requested attribute
		attributes.push_back("msExchMaxStoresTotal");
		attributes.push_back("msExchMaxStorageGroups");
		attributes.push_back("msExchMaxStoresPerGroup");
		if( m_ldapConnection->Search(serverDN, LDAP_SCOPE_SUBTREE, filter, attributes, result) == SVS_OK)
		{
			std::list<std::multimap<std::string,std::string> >::iterator iterResult = result.begin();
			while(iterResult != result.end())
			{
				if(iterResult->find("msExchMaxStoresTotal") != iterResult->end())
				{
					//Exchange 2007/2010 versions maintain valied value for "msExchMaxStoresTotal" attribute
					nMaxMailstores = iterResult->find("msExchMaxStoresTotal")->second;
					DebugPrintf( SV_LOG_INFO, "Max Mailstores: %s.\n",nMaxMailstores.c_str());
					retStatus = SVS_OK;
				}
				else if( (iterResult->find("msExchMaxStorageGroups") != iterResult->end()) &&
					(iterResult->find("msExchMaxStoresPerGroup") != iterResult->end()) )
				{
					//Need to calculate Max-MailStores-Total for Exchange 2003
					int maxMailStores = atoi(iterResult->find("msExchMaxStorageGroups")->second.c_str());
					maxMailStores *= atoi(iterResult->find("msExchMaxStoresPerGroup")->second.c_str());
					try
					{
						nMaxMailstores = boost::lexical_cast<std::string>(maxMailStores);
						retStatus = SVS_OK;
					}catch( boost::bad_lexical_cast & blc )
					{
						DebugPrintf(SV_LOG_ERROR, "Got Exception. MailStores value %d. Details: %s\n",maxMailStores,blc.what());
					}
				}
				iterResult++;
			}
		}
		else
		{
			DebugPrintf( SV_LOG_ERROR, "Can not get Mailstores capacity.\n");
		}
	}while(false);
	DebugPrintf( SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME );
	return retStatus;
}
std::string ExchangeLdapHelper::getEdition(const std::string& maxMailstores, InmProtectedAppType appType)
{
	DebugPrintf( SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME );
	std::string edition = "No Edition available";
	
	int nMaxMailStores = atoi(maxMailstores.c_str());
	switch(appType)
	{
	case INM_APP_EXCH2003:
		if(20 == nMaxMailStores)
			edition = EXCHANGE_EDITION_ENTERPRISE;
		else
			edition = EXCHANGE_EDITION_STANDARD;
		break;
	case INM_APP_EXCH2007:
		if(5 == nMaxMailStores)
			edition = EXCHANGE_EDITION_STANDARD;
		else if(50 == nMaxMailStores)
			edition = EXCHANGE_EDITION_ENTERPRISE;
		break;
	case INM_APP_EXCH2010:
		if(5 == nMaxMailStores)
			edition = EXCHANGE_EDITION_STANDARD;
		else if(100 == nMaxMailStores)
			edition = EXCHANGE_EDITION_ENTERPRISE;
		break;
	default:
		DebugPrintf( SV_LOG_WARNING, "Unknown exchange version.\n");
		break;
	}

	DebugPrintf( SV_LOG_DEBUG, "Edition: %s.\n",edition.c_str());
	DebugPrintf( SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME );
	return edition;
}