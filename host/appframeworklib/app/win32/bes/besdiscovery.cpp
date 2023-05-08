#include "besdiscovery.h"
#include "registry.h"
#include "service.h"
#include "mssql/sqldiscovery.h"

std::list<std::pair<std::string, std::string> > BESDiscoveryInfo::getProperties()
{
    std::list<std::pair<std::string, std::string> > properties ;
    properties.push_back(std::make_pair("BES Server Name: ", this->m_configServerName));
    properties.push_back(std::make_pair("Version: ", this->m_installVersion));
    properties.push_back(std::make_pair("Install Path: ", this->m_installPath));
    properties.push_back(std::make_pair("Key Store User Name: ", this->m_configKeyStoreUserName));
    //properties.push_back(std::make_pair("No of Agents: ", this->m_no_of_agents) ); 
    //properties.push_back(std::make_pair("No of Active Agents: ", this->m_no_of_Acitveagents) );
    
    return properties;
}   

std::list<std::pair<std::string, std::string> > BESMetaData::getProperties()
{
    std::list<std::pair<std::string, std::string> > properties ;
    std::list<std::string>::iterator listIter = this->m_exchangeServerNameList.begin();
    if(listIter == this->m_exchangeServerNameList.end())
    {
        properties.push_back(std::make_pair("MailBox Server: ", "NOT SPECIFIED"));
    }
    while(listIter != this->m_exchangeServerNameList.end())
    {
        properties.push_back(std::make_pair("MailBox Server: ", *listIter));
        listIter++;
    }
    properties.push_back(std::make_pair(" ", " "));
    properties.push_back(std::make_pair("Data Base Name: ", this->m_dataBaseName));
    properties.push_back(std::make_pair("DB Server FQN: ", this->m_dataBseServerMachineName));

    return properties;
}

SVERROR BESAplicationImpl::discoverBESData( BESDiscoveryInfo& discoveredData )
{
    DebugPrintf( SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME ) ;
    SVERROR retStatus = SVS_FALSE ;

    if( getStringValue("SOFTWARE\\Research In Motion\\BlackBerry Enterprise Server\\Setup", "ConfigInstallType", discoveredData.m_configInstallType) != SVS_OK )
    {
        DebugPrintf(SV_LOG_WARNING, "getStringValue failed for the key ConfigInstallType.");
    }
    if( getStringValue("SOFTWARE\\Research In Motion\\BlackBerry Enterprise Server\\Setup", "ConfigKeystoreUserName", discoveredData.m_configKeyStoreUserName) != SVS_OK )
    {    
        DebugPrintf(SV_LOG_WARNING, "getStringValue failed for the key ConfigKeystoreUserName.");
    }
    if( getStringValue("SOFTWARE\\Research In Motion\\BlackBerry Enterprise Server\\Setup", "ConfigServerName", discoveredData.m_configServerName) != SVS_OK )
    {
        DebugPrintf(SV_LOG_WARNING, "getStringValue failed for the key ConfigServerName.");
    }
    if( getStringValue("SOFTWARE\\Research In Motion\\BlackBerry Enterprise Server\\Setup", "BasePath", discoveredData.m_installPath) != SVS_OK )
    {
        DebugPrintf(SV_LOG_WARNING, "getStringValue failed for the key BasePath.");
    }
    if( getStringValue("SOFTWARE\\Research In Motion\\BlackBerry Enterprise Server\\Setup", "InstallUser", discoveredData.m_installUser) != SVS_OK )
    {
        DebugPrintf(SV_LOG_WARNING, "getStringValue failed for the key InstallUser.");
    }
    if( getStringValue("SOFTWARE\\Research In Motion\\BlackBerry Enterprise Server\\Setup", "InstallVersion", discoveredData.m_installVersion) != SVS_OK )
    {
        DebugPrintf(SV_LOG_WARNING, "getStringValue failed for the key InstallVersion.");
    }
    if( getStringValue("SOFTWARE\\Research In Motion\\BlackBerry Enterprise Server\\Agents", "ActiveAgents", discoveredData.m_no_of_Acitveagents) != SVS_OK )
    {
        DebugPrintf(SV_LOG_WARNING, "getStringValue failed for the key ActiveAgents.");
    }
    if( getDwordValue("SOFTWARE\\Research In Motion\\BlackBerry Enterprise Server\\Agents", "NumAgents", discoveredData.m_no_of_agents) != SVS_OK )
    {
        DebugPrintf(SV_LOG_WARNING, "getStringValue failed for the key NumAgents.");
    }
	
	if(discoveredData.m_installVersion.find("4.1") != std::string::npos)
	{
		discoverServices(discoveredData.m_services, BES_4_1);
	}
	else if(discoveredData.m_installVersion.find("5.0") != std::string::npos)
	{
		discoverServices(discoveredData.m_services, BES_5_0);
	}

    DebugPrintf( SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME ) ;
    return retStatus;
}

SVERROR BESAplicationImpl::discoverBESMetaData( BESMetaData& discoveredMetaData )
{
    DebugPrintf( SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME ) ;
    SVERROR retStatus = SVS_OK ;
   
    if( getStringValue("SOFTWARE\\Research In Motion\\BlackBerry Enterprise Server\\Database", "DatabaseName", discoveredMetaData.m_dataBaseName) != SVS_OK)
    {
        DebugPrintf(SV_LOG_ERROR, "getStringValue failed for the key DatabaseName.");
    }
    if( getStringValue("SOFTWARE\\Research In Motion\\BlackBerry Enterprise Server\\Database", "DatabaseServerMachineName", discoveredMetaData.m_dataBseServerMachineName) != SVS_OK)
    {
        DebugPrintf(SV_LOG_ERROR, "getStringValue failed for the key DatabaseServerMachineName.");
    }

// getting the MailBox Server Names from dataBase.

    std::list<InmProtectedAppType> supportedSqlVersionsList ;
    MSSQLDiscoveryInfoImpl::prepareSupportedSQLVersionsList(supportedSqlVersionsList) ;
    
    std::string sqlInstancefqn = discoveredMetaData.m_dataBseServerMachineName;
    std::string dbName = discoveredMetaData.m_dataBaseName;
    
    std::string queryString = "SELECT ServerDN FROM \"" + dbName + "\".dbo.UserConfig";

    std::list<InmProtectedAppType>::const_iterator supportedSqlVersionIter = supportedSqlVersionsList.begin() ;
    
    SQLAdo *SQLAdoImplPtr;
    
    while( supportedSqlVersionIter != supportedSqlVersionsList.end() )
    {
		SQLAdoImplPtr = NULL;
        SQLAdoImplPtr = SQLAdo::getSQLAdoImpl(*supportedSqlVersionIter); 
        SQLAdoImplPtr->init();
        if(SQLAdoImplPtr->connect(sqlInstancefqn, dbName) == SVS_OK)
        {
            getMailBoxServerNameProperty(SQLAdoImplPtr, queryString, discoveredMetaData.m_exchangeServerNameList);
            break;
        }
        supportedSqlVersionIter++ ;
		if(SQLAdoImplPtr != NULL)
		{
			delete SQLAdoImplPtr;
		}
	}
    DebugPrintf( SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME ) ;
    
    return retStatus;
}

SVERROR BESAplicationImpl::discoverServices(std::list<InmService>& serviceList, BES_SUPPORTED_VERSIONS bes_vers)
{
    DebugPrintf( SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME ) ;
    SVERROR retStatus = SVS_OK ;
	
	InmService alertService("BlackBerry Server Alert");
	QuerySvcConfig(alertService);
	serviceList.push_back(alertService);

	InmService attachMentService("BBAttachServer");
	QuerySvcConfig(attachMentService);
	serviceList.push_back(attachMentService);

	InmService controllerService("BlackBerry Controller");
	QuerySvcConfig(controllerService);
	serviceList.push_back(controllerService);

	InmService dataBaseConsistencyService("BlackBerry DataBase Consistency Service");
	QuerySvcConfig(dataBaseConsistencyService);
	serviceList.push_back(dataBaseConsistencyService);

	InmService dispatcherService("BlackBerry Dispatcher");
	QuerySvcConfig(dispatcherService);
	serviceList.push_back(dispatcherService);

	InmService mdsConnectionService("BlackBerry MDS Connection Service");
	QuerySvcConfig(mdsConnectionService);
	serviceList.push_back(mdsConnectionService);

	InmService polocyService("BlackBerry Policy Service");
	QuerySvcConfig(polocyService);
	serviceList.push_back(polocyService);

	InmService routerService("BlackBerry Router");
	QuerySvcConfig(routerService);
	serviceList.push_back(routerService);
    
	InmService synchronizationService("BlackBerry SyncServer");
	QuerySvcConfig(synchronizationService);
	serviceList.push_back(synchronizationService);
	
	if( bes_vers == BES_5_0 )
	{
		InmService BAS_AS("BAS-AS");
		QuerySvcConfig(BAS_AS);
		serviceList.push_back(BAS_AS);

		InmService BAS_NCC("BAS-NCC");
		QuerySvcConfig(BAS_NCC);
		serviceList.push_back(BAS_NCC);

		InmService bbCollabServ("BlackBerry Collaboration Service");
		QuerySvcConfig(bbCollabServ);
		serviceList.push_back(bbCollabServ);

		InmService bbMailStoreServ("BlackBerry Mailstore Service");
		QuerySvcConfig(bbMailStoreServ);
		serviceList.push_back(bbMailStoreServ);

		InmService MDSIS("MDSIS");
		QuerySvcConfig(MDSIS);
		serviceList.push_back(MDSIS);

		InmService bbMonConsole("BBMonitoring Console");
		QuerySvcConfig(bbMonConsole);
		serviceList.push_back(bbMonConsole);

		InmService bbMonServ_APP("BBMonitoringService_APP");
		QuerySvcConfig(bbMonServ_APP);
		serviceList.push_back(bbMonServ_APP);	

		InmService bbMonServ_DCS("BBMonitoringService_DCS");
		QuerySvcConfig(bbMonServ_DCS);
		serviceList.push_back(bbMonServ_DCS);

		InmService bbMonServ_ENG("BBMonitoringService_ENG");
		QuerySvcConfig(bbMonServ_ENG);
		serviceList.push_back(bbMonServ_ENG);

	}
    DebugPrintf( SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME ) ;
    return retStatus;
}

std::string BESDiscoveryInfo::getSummary()
{
    DebugPrintf( SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME ) ;
    std::stringstream summaryStream;
    summaryStream << "\n\tBES installation Details " << std::endl ;
    summaryStream << "\t----------------------------" << std::endl;
    
    summaryStream << "\t\tInstall Version: \t" << m_installVersion << std::endl;
    summaryStream << "\t\tConfigInstallType: \t" << m_configInstallType << std::endl ;
    summaryStream << "\t\tInstall Path: \t" << m_installPath << std::endl ;
    summaryStream << "\t\tKey Store User Name: \t" << m_configKeyStoreUserName << std::endl;
    
    
    return summaryStream.str();
    DebugPrintf( SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME ) ;
}

std::string BESMetaData::getSummary()
{
    DebugPrintf( SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME ) ;
    std::stringstream summaryStream;
    summaryStream << "\n\tBES Configuration Details " << std::endl ;
    summaryStream << "\t------------------------------" << std::endl;
    summaryStream << "\t\t DataBase Details:" << std::endl;
    summaryStream << "\t\t\tDataBase Name: " << this->m_dataBaseName <<std::endl;
    summaryStream << "\t\t\tDataBase Server Machine Name: " << this->m_dataBseServerMachineName << std::endl;
    summaryStream << "\t\t MailServer Details "<<std::endl;
    std::list<std::string>::iterator nameListIter = this->m_exchangeServerNameList.begin();
    while( nameListIter != this->m_exchangeServerNameList.end() )
    {
        summaryStream << "\t\t\t MailServer Name: " << *nameListIter<<std::endl;
        nameListIter++;
    }
    DebugPrintf( SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME ) ;
    return summaryStream.str();
}