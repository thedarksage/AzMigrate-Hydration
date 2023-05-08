#include "StdAfx.h"
#include <windows.h>
#include ".\appdata.h"
#include "Resource.h"
#include "host.h"
#include "ruleengine.h"
#include "appglobals.h"
#include "configvalueobj.h"
#include "system.h"

using namespace std;

AppData* AppData::m_pAppData = NULL;
boost::mutex AppData::m_AppDataMutex;

extern std::list<std::string> installedApps ;
AppData::AppData(void)
:m_ErrorMsg(""),
m_bCreateTxtFile(true),
m_strSummary(""),
m_error(RWIZARD_CX_UPATE_SUCCESS),
m_retryCount(0)
{
	m_discoveyInfo = NULL;
	m_discoveyInfo = new DiscoveryInfo();
	if(!m_discoveyInfo)
	{
		AfxMessageBox("Failed to allocate memmory for discovery data");
	}
}

AppData::~AppData(void)
{
	if(m_discoveyInfo)
		delete m_discoveyInfo;
}

void AppData::getAppData(void)
{
}

CString AppData::appType(int choice)
{
    DebugPrintf(SV_LOG_DEBUG, "Selected CHoice %d\n", choice) ;
	switch(choice)
	{
        case 0:
            return NULL;
        case INM_APP_EXCHNAGE:
            return CString(_T("EXCHANGE"));
        case INM_APP_MSSQL:
            return CString(_T("MS SQL Server"));
        case INM_APP_MSSQL2005:
            return CString(_T("MS SQL Server 2005"));
        case INM_APP_MSSQL2000:
            return CString(_T("MS SQL Server 2000"));
        case INM_APP_MSSQL2008:
            return CString(_T("MS SQL Server 2008"));
		case INM_APP_MSSQL2012:
            return CString(_T("MS SQL Server 2012"));
        case INM_APP_EXCH2003:
            return CString(_T("EXCHANGE 2003"));
        case INM_APP_EXCH2007:
            return CString(_T("EXCHANGE 2007"));
        case INM_APP_EXCH2010:
            return CString(_T("EXCHANGE 2010"));
        case INM_APP_FILESERVER:
            return CString(_T("File Server"));
        case INM_APP_BES:
            return CString(_T("BlackBerry Server"));
        default:
            DebugPrintf(SV_LOG_ERROR, "Returning NULL %d\n", choice) ;
            return NULL;
	}
    DebugPrintf(SV_LOG_ERROR, "Returning NULL %d\n", choice) ;
	return NULL;
}

void AppData::initAppTreeVector(void)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",__FUNCTION__);
	vector_appTree.clear();// Need to clear while re-discovering...
	std::list<MSSQLVersionDiscoveryInfo>::const_iterator& sqlVersionDiscoveryInfoIter = m_sqlApp.m_mssqlDiscoveryInfo.m_sqlVersionDiscoveryInfo.begin() ;
    while( sqlVersionDiscoveryInfoIter != m_sqlApp.m_mssqlDiscoveryInfo.m_sqlVersionDiscoveryInfo.end() )
    {
		if( sqlVersionDiscoveryInfoIter->errCode == 0 )
		{
			AppTreeFormat atf;
			atf.m_appName.Format(appType(sqlVersionDiscoveryInfoIter->m_type));//Version is added
			std::list<MSSQLInstanceDiscoveryInfo>::const_iterator instanceDiscoveryInfoIter = sqlVersionDiscoveryInfoIter->m_sqlInstanceDiscoveryInfo.begin() ;
			while( instanceDiscoveryInfoIter != sqlVersionDiscoveryInfoIter->m_sqlInstanceDiscoveryInfo.end() )
			{
				const MSSQLInstanceDiscoveryInfo& discInfo = *instanceDiscoveryInfoIter;
				if( discInfo.errCode == 0 )
				{
					atf.m_instancevector.push_back(CString(discInfo.m_instanceName.c_str()));//Adding instances

					vector<CString> tempDBNames;
					tempDBNames.clear();
					const MSSQLInstanceMetaData& instanceMetaData = m_sqlApp.m_sqlmetaData.m_serverInstanceMap[discInfo.m_instanceName] ;
					std::map<std::string, MSSQLDBMetaData>::const_iterator dbMapIter = instanceMetaData.m_dbsMap.begin() ;

					while( dbMapIter != instanceMetaData.m_dbsMap.end() )
					{
						tempDBNames.push_back(CString(dbMapIter->first.c_str()));//DataBase names for instance
						dbMapIter++;
					}
					atf.m_DBmap.insert(make_pair((discInfo.m_instanceName.c_str()),tempDBNames));
				}
				instanceDiscoveryInfoIter++;
			}
			vector_appTree.push_back(atf);
		}
		sqlVersionDiscoveryInfoIter++;
	}

	//Exchange discovery initialization
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",__FUNCTION__);
}

bool AppData::isVersion(CString versionName)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",__FUNCTION__);
	vector<AppTreeFormat>::const_iterator iterator = vector_appTree.begin();
	while(iterator != vector_appTree.end())
	{
		const AppTreeFormat& tempAppInfo = *iterator;
		if(versionName.Compare(tempAppInfo.m_appName) == 0)
			return true;
		iterator++;
	}
	//Exchange versions/hostname verification
	std::map<std::string, ExchAppVersionDiscInfo>::iterator exchDiscIter = m_pExchangeApp.m_exchDiscoveryInfoMap.begin() ;
    while( exchDiscIter != m_pExchangeApp.m_exchDiscoveryInfoMap.end() )
    {
		if(versionName.Compare(exchDiscIter->first.c_str()) == 0 )
			return true;
		exchDiscIter++;
	}
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",__FUNCTION__);
    return false;
}

bool AppData::isInstance(CString instanceName)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",__FUNCTION__);
	vector<AppTreeFormat>::const_iterator iterator = vector_appTree.begin();
	while(iterator != vector_appTree.end())
	{
		const AppTreeFormat& tempAppInfo = *iterator;
		vector<CString>::const_iterator versnIter = tempAppInfo.m_instancevector.begin();
		while(versnIter != tempAppInfo.m_instancevector.end())
		{
			const CString& tempInstance = *versnIter;
			if(instanceName.Compare(tempInstance) == 0)
				return true;
			versnIter++;
		}
		iterator++;
	}
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",__FUNCTION__);
    return false;
}

bool AppData::isDatabase(CString dbName,CString instanceName)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",__FUNCTION__);
    vector<AppTreeFormat>::const_iterator iterator = vector_appTree.begin();
	while(iterator != vector_appTree.end())
	{
		const AppTreeFormat& tempAppInfo = *iterator;
		vector<CString>::const_iterator versnIter = tempAppInfo.m_instancevector.begin();
		while(versnIter != tempAppInfo.m_instancevector.end())
		{
			const CString& tempInstance = *versnIter;
            if(instanceName.Compare(tempInstance) == 0)
			{
				map<CString,vector<CString> >::const_iterator dbMapIter = tempAppInfo.m_DBmap.find(tempInstance);
				vector<CString>::const_iterator dbIter = dbMapIter->second.begin();
				while(dbIter != dbMapIter->second.end())
				{
					const CString& tempDBName = *dbIter;
					if(dbName.Compare(tempDBName) == 0)
						return true;
					dbIter++;
				}
			}
			versnIter++;
		}
		iterator++;
	}
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",__FUNCTION__);
    return false;
}

bool AppData::isStorageGroup(CString strSGName,CString strHostName)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED isStorageGroup(CString strSGName)\n") ;
	std::map<std::string,ExchAppVersionDiscInfo>::const_iterator exchIter = m_pExchangeApp.m_exchDiscoveryInfoMap.begin();
	if(exchIter != m_pExchangeApp.m_exchDiscoveryInfoMap.end())
		{
			while(exchIter != m_pExchangeApp.m_exchDiscoveryInfoMap.end())
			{
				if(strHostName.Compare(exchIter->first.c_str()) == 0)
				{
					const ExchAppVersionDiscInfo &appVerInfo = exchIter->second;
					ExchangeMetaData& tempMetadata = m_pExchangeApp.m_exchMetaDataMap[exchIter->first];
					std::list<ExchangeStorageGroupMetaData>::const_iterator SGMetaDaraIter = tempMetadata.m_storageGrps.begin();
					while(SGMetaDaraIter != tempMetadata.m_storageGrps.end())
					{
						const ExchangeStorageGroupMetaData &SGMetatData = *SGMetaDaraIter;
						if(strSGName.Compare(SGMetatData.m_storageGrpName.c_str()) == 0)
						{
							DebugPrintf(SV_LOG_DEBUG, "SUCCESS: EXITED isStorageGroup(CString strSGName)\n") ;
							return true;
						}
						SGMetaDaraIter++;
					}
				}
				exchIter++;
			}
		}
	DebugPrintf(SV_LOG_DEBUG, "FAILED: EXITED isStorageGroup(CString strSGName)\n") ;
	return false;
}

bool AppData::isMailBox(CString strMailBoxName, CString strSGName,CString strHostName)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED isMailBox(CString strMailBoxName, CString strSGName)\n") ;
	std::map<std::string,ExchAppVersionDiscInfo>::const_iterator exchIter = m_pExchangeApp.m_exchDiscoveryInfoMap.begin();
	if(exchIter != m_pExchangeApp.m_exchDiscoveryInfoMap.end())
		{
			while(exchIter != m_pExchangeApp.m_exchDiscoveryInfoMap.end())
			{
				const ExchAppVersionDiscInfo &appVerInfo = exchIter->second;
				if( appVerInfo.m_appType == INM_APP_EXCH2003 || appVerInfo.m_appType == INM_APP_EXCH2007 || appVerInfo.m_appType == INM_APP_EXCHNAGE )
				{
				ExchangeMetaData& tempMetadata = m_pExchangeApp.m_exchMetaDataMap[exchIter->first];
				std::list<ExchangeStorageGroupMetaData>::const_iterator SGMetaDaraIter = tempMetadata.m_storageGrps.begin();
				while(SGMetaDaraIter != tempMetadata.m_storageGrps.end())
				{
					const ExchangeStorageGroupMetaData &SGMetatData = *SGMetaDaraIter;
					if(strSGName.Compare(SGMetatData.m_storageGrpName.c_str()) == 0)
					{
						std::list<ExchangeMDBMetaData>::const_iterator DBMetaDataIter = SGMetatData.m_mdbMetaDataList.begin();
						while(DBMetaDataIter != SGMetatData.m_mdbMetaDataList.end())
						{
							const ExchangeMDBMetaData &DBMetaData = *DBMetaDataIter;
							if(strMailBoxName.Compare(DBMetaData.m_mdbName.c_str()) == 0)
							{
								DebugPrintf(SV_LOG_DEBUG, "SUCCESS: EXITED isMailBox(CString strMailBoxName, CString strSGName)\n") ;
								return true;
							}
							DBMetaDataIter++;
						}
					}
					SGMetaDaraIter++;
				}
				}
				else if( appVerInfo.m_appType == INM_APP_EXCH2010 )
				{
					Exchange2k10MetaData& tempMetadata = m_pExchangeApp.m_exch2k10MetaDataMap[exchIter->first];
					std::list<ExchangeMDBMetaData>::const_iterator DBMetaDataIter = tempMetadata.m_mdbMetaDataList.begin();
					while(DBMetaDataIter != tempMetadata.m_mdbMetaDataList.end())
					{
						const ExchangeMDBMetaData &DBMetaData = *DBMetaDataIter;
						if(strMailBoxName.Compare(DBMetaData.m_mdbName.c_str()) == 0)
						{
							DebugPrintf(SV_LOG_DEBUG, "SUCCESS: EXITED isMailBox(CString strMailBoxName, CString strSGName)\n") ;
							return true;
						}
						DBMetaDataIter++;
					}
				}
				exchIter++;
			}
		}
		DebugPrintf(SV_LOG_DEBUG, "FAILED: EXITED isMailBox(CString strMailBoxName, CString strSGName)\n") ;
	return false;
}
void AppData::ValidateRules(void)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",__FUNCTION__);
    //Clearing all rule's containers
	m_appLevelRules.clear();
	m_versionLevelRules.clear();
	m_sgLevelRules.clear();
	m_instanceRulesMap.clear();
	RuleEnginePtr enginePtr = RuleEngine::getInstance() ;
	std::list<ValidatorPtr>::iterator ruleIter ;
	std::list<ValidatorPtr> rules ;
	MSSQLInstanceMetaData metaData ;
	FileServerInstanceMetaData fsMetatData ;
	USES_CONVERSION;
    std::string systemDrive ;
    getEnvironmentVariable("SystemDrive", systemDrive) ;

	m_sysLevelRules = enginePtr->getSystemRules(SCENARIO_PROTECTION, CONTEXT_HADR) ;
	ruleIter = m_sysLevelRules.begin() ;
	while(ruleIter != m_sysLevelRules.end() )
	{
		enginePtr->RunRule(*ruleIter) ;
		ruleIter ++ ;
	}
	ConfigValueObj configObj = ConfigValueObj::getInstance() ;
    std::set<InmProtectedAppType> selectedApps  = configObj.getApplications() ;
    std::set<InmProtectedAppType>::const_iterator appIter = selectedApps.begin() ;
    std::list<MSSQLVersionDiscoveryInfo>::const_iterator sqlVersionDiscoveryInfoIter ;
	std::map<std::string, FileServerInstanceMetaData>::iterator fileServerInstanceMetaDataIter ;
    while( appIter != selectedApps.end() )
    {
        switch(*appIter)
        {
        case INM_APP_MSSQL :
			rules.clear();
			// As part of Application Usability code refactoring be low function is removed as there is no rule for mssql application
	        /*rules = enginePtr->getMSSQLAppLevelRules(SCENARIO_PROTECTION, CONTEXT_HADR, *appIter) ;
	        ruleIter = rules.begin() ;
	        while(ruleIter != rules.end() )
	        {
		        enginePtr->RunRule(*ruleIter) ;
		        ruleIter ++ ;
	        }*/
	        m_appLevelRules.insert(std::make_pair(appType(INM_APP_MSSQL),rules));
            sqlVersionDiscoveryInfoIter = m_sqlApp.m_mssqlDiscoveryInfo.m_sqlVersionDiscoveryInfo.begin() ;
            while( sqlVersionDiscoveryInfoIter != m_sqlApp.m_mssqlDiscoveryInfo.m_sqlVersionDiscoveryInfo.end() )
            {
				
				rules = enginePtr->getMSSQLVersionLevelRules(sqlVersionDiscoveryInfoIter->m_type, SCENARIO_PROTECTION, CONTEXT_HADR);
				m_versionLevelRules.insert(std::make_pair(appType(sqlVersionDiscoveryInfoIter->m_type), rules)) ;
	            ruleIter = rules.begin() ;
	            while(ruleIter != rules.end() )
	            {
		            enginePtr->RunRule(*ruleIter) ;
		            ruleIter ++ ;
	            }
                std::list<MSSQLInstanceDiscoveryInfo>::const_iterator instanceDiscoveryInfoIter = sqlVersionDiscoveryInfoIter->m_sqlInstanceDiscoveryInfo.begin() ;
                while( instanceDiscoveryInfoIter != sqlVersionDiscoveryInfoIter->m_sqlInstanceDiscoveryInfo.end() )
                {
                    const MSSQLInstanceDiscoveryInfo& discInfo = *instanceDiscoveryInfoIter;
		            metaData = m_sqlApp.m_sqlmetaData.m_serverInstanceMap[discInfo.m_instanceName];
					//Passing empty string for vacp options
		            rules = enginePtr->getMSSQLInstanceRules(metaData, SCENARIO_PROTECTION, CONTEXT_HADR, systemDrive,"");
		            m_instanceRulesMap.insert(std::make_pair(discInfo.m_instanceName.c_str(), rules)) ;
		            ruleIter = rules.begin() ;
		            while(ruleIter != rules.end() )
		            {
			            enginePtr->RunRule(*ruleIter) ;
			            ruleIter ++ ;
		            }
		            instanceDiscoveryInfoIter++;
	            }
	            sqlVersionDiscoveryInfoIter++;
            }
            break ;
        case INM_APP_FILESERVER:
	        rules = enginePtr->getFileServerRules(SCENARIO_PROTECTION, CONTEXT_HADR);
	        ruleIter = rules.begin();
	        while(ruleIter != rules.end() )
	        {
		        enginePtr->RunRule(*ruleIter) ;
		        ruleIter ++ ;
	        }
	        m_appLevelRules.insert(std::make_pair(appType(INM_APP_FILESERVER),rules));
			fileServerInstanceMetaDataIter = m_fileservApp.m_fileShareInstanceMetaDataMap.begin();
            while( fileServerInstanceMetaDataIter != m_fileservApp.m_fileShareInstanceMetaDataMap.end() )
            {
				rules = enginePtr->getFileServerInstanceLevelRules( fileServerInstanceMetaDataIter->second, SCENARIO_PROTECTION, CONTEXT_HADR, systemDrive, "-a FS" );
				ruleIter = rules.begin() ;
				while(ruleIter != rules.end() )
				{
					enginePtr->RunRule(*ruleIter) ;
					ruleIter ++ ;
				}
				m_instanceRulesMap.insert( std::make_pair( fileServerInstanceMetaDataIter->first.c_str(), rules ) ) ;
	            fileServerInstanceMetaDataIter++;
            }
            break ;
        case INM_APP_EXCHNAGE:
        {
	        std::map<std::string,ExchAppVersionDiscInfo>::const_iterator exchIter = m_pExchangeApp.m_exchDiscoveryInfoMap.begin();
	        std::string vacpOptions = ""; //Passing Empty dtring for vacpOptions
			while(exchIter != m_pExchangeApp.m_exchDiscoveryInfoMap.end())
	        {
		        const ExchAppVersionDiscInfo &appVerInfo = exchIter->second;
				if(appVerInfo.m_appType == INM_APP_EXCH2003 || appVerInfo.m_appType == INM_APP_EXCH2007 || appVerInfo.m_appType == INM_APP_EXCHNAGE )
				{
					ExchangeMetaData &tempMetadata = m_pExchangeApp.m_exchMetaDataMap[exchIter->first];
					
					if(appVerInfo.m_appType == INM_APP_EXCH2007)
					{
						vacpOptions +=  std::string(" -w ExchangeIS ");
					}
					else if(appVerInfo.m_appType == INM_APP_EXCH2003)
					{
						vacpOptions += std::string(" -a EXCHANGE ");
					}

					rules = enginePtr->getExchangeRules(appVerInfo,tempMetadata, SCENARIO_PROTECTION, CONTEXT_HADR, systemDrive, vacpOptions, Host::GetInstance().GetHostName());
				}
				else if(appVerInfo.m_appType == INM_APP_EXCH2010)
				{
					Exchange2k10MetaData &tempMetadata = m_pExchangeApp.m_exch2k10MetaDataMap[exchIter->first];
					vacpOptions +=  std::string(" -w ExchangeIS ");
					rules = enginePtr->getExchange2010Rules(appVerInfo,tempMetadata, SCENARIO_PROTECTION, CONTEXT_HADR, systemDrive, vacpOptions, Host::GetInstance().GetHostName());
				}
		        ruleIter = rules.begin();
		        while(ruleIter != rules.end())
		        {
			        enginePtr->RunRule(*ruleIter) ;
			        ruleIter ++ ;
		        }
		        m_versionLevelRules.insert(std::make_pair(exchIter->first.c_str(),rules));
		        exchIter++;
	        }
            break ;
         }
         case INM_APP_BES:
	        rules = enginePtr->getBESRules();
	        ruleIter = rules.begin();
	        while(ruleIter != rules.end() )
	        {
		        enginePtr->RunRule(*ruleIter) ;
		        ruleIter ++ ;
	        }
	        m_appLevelRules.insert(std::make_pair(appType(INM_APP_BES),rules));
            break ;
		}
        appIter++ ;
	}
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",__FUNCTION__);
}
void AppData::GenerateSummary()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",__FUNCTION__);
    stringstream str ;
	USES_CONVERSION;
	str <<  m_hldiscInfo.summary(m_sysLevelRules) ;
	std::list<MSSQLVersionDiscoveryInfo>::iterator iterMSSQLVersion ;
    std::map<std::string,ExchAppVersionDiscInfo>::iterator exchIter ;
    std::set<InmProtectedAppType> selectedApps = ConfigValueObj::getInstance().getApplications() ;
    std::set<InmProtectedAppType>::iterator appIter = selectedApps.begin() ;

    while( appIter != selectedApps.end() )
    {
        switch(*appIter)
        {
            case INM_APP_FILESERVER:
                str << m_fileservApp.getSummary(m_appLevelRules[appType(INM_APP_FILESERVER)] );
                break ;
            case INM_APP_MSSQL:
                str <<  m_sqlApp.m_mssqlDiscoveryInfo.summary() ;
                iterMSSQLVersion =  m_sqlApp.m_mssqlDiscoveryInfo.m_sqlVersionDiscoveryInfo.begin();
                if(iterMSSQLVersion !=  m_sqlApp.m_mssqlDiscoveryInfo.m_sqlVersionDiscoveryInfo.end())
                {
	                while(iterMSSQLVersion !=  m_sqlApp.m_mssqlDiscoveryInfo.m_sqlVersionDiscoveryInfo.end())
	                {
		                str<<iterMSSQLVersion->summary();//generates versions discovery info summary
		                iterMSSQLVersion++;
	                }
	                iterMSSQLVersion =  m_sqlApp.m_mssqlDiscoveryInfo.m_sqlVersionDiscoveryInfo.begin();
	                while(iterMSSQLVersion !=  m_sqlApp.m_mssqlDiscoveryInfo.m_sqlVersionDiscoveryInfo.end())
	                {
		                std::list<MSSQLInstanceDiscoveryInfo>::iterator iterMSSQLInstance = iterMSSQLVersion->m_sqlInstanceDiscoveryInfo.begin();        
		                while(iterMSSQLInstance != iterMSSQLVersion->m_sqlInstanceDiscoveryInfo.end())
		                {
			                str << iterMSSQLInstance->summary() ;
			                CString instanceName = iterMSSQLInstance->m_instanceName.c_str();
			                std::list<ValidatorPtr>& ruleList =  m_instanceRulesMap[instanceName] ;
			                DebugPrintf(SV_LOG_DEBUG, "InstanceRuleMapSize %d\n",  m_instanceRulesMap.size()) ;
			                DebugPrintf(SV_LOG_DEBUG, "instance name requested %s\n", instanceName) ;
			                DebugPrintf(SV_LOG_DEBUG, "Rule List size %d\n", ruleList.size()) ;
			                str <<  m_sqlApp.m_sqlmetaData.m_serverInstanceMap[iterMSSQLInstance->m_instanceName].summary(ruleList) ; 
			                iterMSSQLInstance++ ;
		                }
		                iterMSSQLVersion++;
	                }
                }
                break ;
            case INM_APP_EXCHNAGE:
	            exchIter =  m_pExchangeApp.m_exchDiscoveryInfoMap.begin();
	            if(exchIter !=  m_pExchangeApp.m_exchDiscoveryInfoMap.end())
	            {
		            while(exchIter != m_pExchangeApp.m_exchDiscoveryInfoMap.end())
		            {
			            ExchAppVersionDiscInfo &appVerInfo = exchIter->second;
						str<<  appVerInfo.summary();
						if(appVerInfo.m_appType == INM_APP_EXCH2003 || appVerInfo.m_appType == INM_APP_EXCH2007 || appVerInfo.m_appType == INM_APP_EXCHNAGE)
						{
			            ExchangeMetaData &tempmetadata = m_pExchangeApp.m_exchMetaDataMap[exchIter->first];
			            str<<  tempmetadata.summary();

			            std::list<ValidatorPtr>& ruleList =  m_versionLevelRules[exchIter->first.c_str()];
			            str<<  m_pExchangeApp.summary(ruleList);
						}
						else if(appVerInfo.m_appType == INM_APP_EXCH2010)
						{
							Exchange2k10MetaData &tempmetadata = m_pExchangeApp.m_exch2k10MetaDataMap[exchIter->first];
							str<<  tempmetadata.summary();
							std::list<ValidatorPtr>& ruleList =  m_versionLevelRules[exchIter->first.c_str()];
							str<<  m_pExchangeApp.summary(ruleList);
						}
						else
						{
						
						}
			            exchIter++;
		            }
	            }
                break ;
            case INM_APP_BES:
                if( m_besApp.isInstalled() )
                {
                    str << m_besApp.getSummary();
                }
        }
        appIter++ ;
    }
	m_strSummary.Format(str.str().c_str());
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",__FUNCTION__);
}

void AppData::Create(void)
{
}

void AppData::CreateSummaryTextFile(bool bCreate)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",__FUNCTION__);
	m_bCreateTxtFile = bCreate;
	if(m_bCreateTxtFile)
	{
		//Writing summary report to ValidatioSummary.txt file in the local folder.
		CStdioFile summaryFile;
		if(summaryFile.Open("Summary.txt",CFile::modeWrite | CFile::modeCreate))
		{
			summaryFile.WriteString(m_strSummary);
			summaryFile.Close();
		}
		else
			AfxMessageBox(_T("Unable to Create/Overwrite Summary.txt \n\nAccess is denied"));
	}
}

void AppData::Clear(void)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",__FUNCTION__);
    m_hldiscInfo.clean();
    m_pExchangeApp.clean();
    m_sqlApp.clean();
    m_fileservApp.clean();
    m_besApp.clean();

    vector_appTree.clear(); 

	m_appLevelRules.clear();
	m_instanceRulesMap.clear();
	m_sgLevelRules.clear();
	m_sysLevelRules.clear();
	m_versionLevelRules.clear();
    installedApps.clear() ;
    ConfigValueObj::getInstance().clearApplications() ;
	//clean old data
	if(m_discoveyInfo)
		delete m_discoveyInfo;
	m_discoveyInfo = new DiscoveryInfo();
	//memset(&m_discoveyInfo,0,sizeof(m_discoveyInfo));
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",__FUNCTION__);       
}

bool AppData::isServNTName(CString strSelItem)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",__FUNCTION__);    
    std::map<std::string, FileServerInstanceDiscoveryData>::iterator iter = m_fileservApp.m_fileShareInstanceDiscoveryMap.begin() ;
    while( iter != m_fileservApp.m_fileShareInstanceDiscoveryMap.end() )
	{
        if(strSelItem.Compare(iter->first.c_str()) == 0)
			return true;
        iter++ ;
	}
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",__FUNCTION__);
	return false;
}
bool AppData::prepareDiscoveryUpdate()
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",__FUNCTION__); 
	bool bRet = true;
	if(m_discoveyInfo)
	{
		delete m_discoveyInfo;
		m_discoveyInfo = new DiscoveryInfo();
	}
	else
	{
		m_discoveyInfo = new DiscoveryInfo();
	}
	if( !fillSystemDiscoveryInfo() ||
		!fillExchangeDiscoveryInfo() ||
		!fillMsSQLDiscoveryInfo() ||
		!fillFileServerDiscoveryInfo()
		)
	{
		bRet = false;
	}
	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",__FUNCTION__);
	return bRet;
}
bool AppData::fillSystemDiscoveryInfo()
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",__FUNCTION__); 
	bool filled = true;
	//Updating n/w adatpters information
	m_discoveyInfo->m_nwInfo.clear();
	std::list<NetworkAdapterConfig>::const_iterator nwAdapterIter = m_hldiscInfo.m_sysInfo.m_nwAdapterList.begin();
	while ( nwAdapterIter != m_hldiscInfo.m_sysInfo.m_nwAdapterList.end() )
	{
		DebugPrintf(SV_LOG_DEBUG, "adding nic information\n") ;
		NICInformation nicInfo ;
		std::string macAddress ;
		nicInfo.m_nicProperties.insert(std::make_pair("MACAddress", nwAdapterIter->m_macAddress)) ;
		for( unsigned long i = 0 ; i < nwAdapterIter->m_no_ipsConfigured ; i++ )
		{
			nicInfo.m_IpAddresses.push_back(nwAdapterIter->m_ipaddress[i]) ;
		}
        nicInfo.m_nicProperties.insert(std::make_pair("ErrorCode",boost::lexical_cast<std::string>(nwAdapterIter->m_ErrorCode))) ;
        nicInfo.m_nicProperties.insert(std::make_pair("ErrorString",nwAdapterIter->m_ErrorString)) ;
        m_discoveyInfo->m_nwInfo.push_back(nicInfo) ;
		nwAdapterIter++ ;
	}

	//Updating cluster infomration
	ClusInformation clusInfo ;
	std::string clusterName = m_hldiscInfo.m_clusterInfo.m_clusterName ;
    std::string clusterId = m_hldiscInfo.m_clusterInfo.m_clusterID ;
	clusInfo.m_clusProperties.insert(std::make_pair("ClusterName", clusterName )) ;
	clusInfo.m_clusProperties.insert(std::make_pair("ClusterId", clusterId)) ; 
    std::string ErrorCode = boost::lexical_cast<std::string>(m_hldiscInfo.m_clusterInfo.m_ErrorCode);
    std::string ErrorStr = m_hldiscInfo.m_clusterInfo.m_ErrorString ;
    if( clusterName.compare("") == 0 || 
        clusterId.compare("") == 0 ||
        m_hldiscInfo.m_clusterInfo.m_networkInfoMap.size() == 0 )
    {
        ErrorCode = "-1" ;
        ErrorStr = "The Cluster Information is not complete/unavailable" ;
    }
    clusInfo.m_clusProperties.insert(std::make_pair("ErrorCode", ErrorCode)) ;     
    clusInfo.m_clusProperties.insert(std::make_pair("ErrorString", ErrorStr)) ;

	std::map<std::string, NetworkResourceInfo> clusNwResourceMap = m_hldiscInfo.m_clusterInfo.m_networkInfoMap ;
	std::map<std::string, NetworkResourceInfo>::const_iterator clusNwResourceIter = clusNwResourceMap.begin() ;
	while( clusNwResourceIter != clusNwResourceMap.end() )
	{
		ClusterNwResourceInfo nwResourceInfo ;
		nwResourceInfo.m_resourceProperties.insert(std::make_pair("VirtualServerName", clusNwResourceIter->first)) ;
		nwResourceInfo.m_resourceProperties.insert(std::make_pair("OwnerNode", clusNwResourceIter->second.m_resInfo.m_ownerNodeName)) ;
		nwResourceInfo.m_resourceProperties.insert(std::make_pair("OwnerNode", clusNwResourceIter->second.m_resInfo.m_ownerNodeName)) ;
		std::string resourceStatus = boost::lexical_cast<std::string>(clusNwResourceIter->second.m_resInfo.m_resourceStatus) ;
		nwResourceInfo.m_resourceProperties.insert(std::make_pair("ResourceCurrentState", clusNwResourceIter->second.m_resInfo.m_ownerNodeName)) ;
		std::list<std::string> dependentIpsList = clusNwResourceIter->second.m_dependendentIpList ;
		std::list<std::string>::const_iterator ipIter = dependentIpsList.begin() ;
		while( ipIter != dependentIpsList.end() )
		{
			nwResourceInfo.m_ipsList.push_back(*ipIter) ;
			ipIter ++ ;
		}
		clusInfo.m_nwNameInfoList.push_back( nwResourceInfo ) ;
		clusNwResourceIter++ ;
	}
	m_discoveyInfo->ClusterInformation = clusInfo ;

	//Update VSS Provider information
	m_discoveyInfo->m_vssProviders.clear();
	std::list<VssProviderProperties> vssProvidersList = m_hldiscInfo.m_vssProviderList ;
	std::list<VssProviderProperties>::iterator vssIter = vssProvidersList.begin() ;
	while( vssIter != vssProvidersList.end() )
	{
		VSSProviderDetails vssProviderDetails ;
		std::list<std::pair<std::string, std::string> > propertiesList = vssIter->getPropList() ;
		std::list<std::pair<std::string, std::string> >::const_iterator propertiesIter = propertiesList.begin() ;
		while( propertiesIter != propertiesList.end() )
		{
			if(propertiesIter->first.compare("Name") == 0 )
			{
				vssProviderDetails.m_providerProperties.insert(std::make_pair("ProviderName", propertiesIter->second)) ;
			}
			else if(propertiesIter->first.compare("GUID") == 0 )
			{
				vssProviderDetails.m_providerProperties.insert(std::make_pair("ProviderGuid", propertiesIter->second)) ;
			}
			else if(propertiesIter->first.compare("Type") == 0 )
			{
				vssProviderDetails.m_providerProperties.insert(std::make_pair("ProviderType", propertiesIter->second)) ;
			}
			propertiesIter++ ;
		}
		m_discoveyInfo->m_vssProviders.push_back(vssProviderDetails) ;
		vssIter++ ;
    }
    m_discoveyInfo->m_properties.insert(std::make_pair("DomainName", m_hldiscInfo.m_sysInfo.m_hostInfo.m_domainName));
	//TODO: Write logic here.
	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",__FUNCTION__);
	return filled;
}

bool AppData::fillExchangeDiscoveryInfo()
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",__FUNCTION__); 
	bool filled = true;

	std::map<std::string, ExchangeMetaData> exchMetadataMap ;
	std::map<std::string, Exchange2k10MetaData> exch2k10MetaDataMap ;
    std::map<std::string, ExchAppVersionDiscInfo> exchDiscoveryInfoMap ;
    bool metadataAvailable = false ;

	std::list<std::string> instanceNameList = m_pExchangeApp.getInstances();
	std::list<std::string>::iterator iterInstanceName = instanceNameList.begin();
	while( iterInstanceName != instanceNameList.end() )
	{
		DebugPrintf(SV_LOG_DEBUG, "instance name %s\n", iterInstanceName->c_str()) ;
        if( m_pExchangeApp.m_exchDiscoveryInfoMap.find(*iterInstanceName) != m_pExchangeApp.m_exchDiscoveryInfoMap.end() )
		{        
			exchDiscoveryInfoMap.insert(*(m_pExchangeApp.m_exchDiscoveryInfoMap.find(*iterInstanceName))) ;
            bool metadataFound = false ;
			if( m_pExchangeApp.m_exchDiscoveryInfoMap.find(*iterInstanceName)->second.m_appType != INM_APP_EXCH2010 && 
				m_pExchangeApp.m_exchMetaDataMap.find(*iterInstanceName) != m_pExchangeApp.m_exchMetaDataMap.end() )
			{
				exchMetadataMap.insert(*(m_pExchangeApp.m_exchMetaDataMap.find(*iterInstanceName))) ;
			}
            else if( m_pExchangeApp.m_exchDiscoveryInfoMap.find(*iterInstanceName)->second.m_appType == INM_APP_EXCH2010 && 
					 m_pExchangeApp.m_exch2k10MetaDataMap.find(*iterInstanceName) != m_pExchangeApp.m_exch2k10MetaDataMap.end() )
			{
				exch2k10MetaDataMap.insert(*(m_pExchangeApp.m_exch2k10MetaDataMap.find(*iterInstanceName)));	
			}
		}
		iterInstanceName++;
	}

	std::map<std::string, ExchangeMetaData>::const_iterator exchMetadataMapIter  = exchMetadataMap.begin() ;
    while( exchMetadataMapIter != exchMetadataMap.end() )
    {
        ExchangeDiscoveryInfo exchdiscoveryInfo ;
		exchdiscoveryInfo.m_policyInfo.insert(std::make_pair("policyId","0"));
		exchdiscoveryInfo.m_policyInfo.insert(std::make_pair("InstanceId","ApplicationDiscoveryWizardUpdate"));
        const ExchangeMetaData& exchMetadata = exchMetadataMapIter->second ;;
        const ExchAppVersionDiscInfo& exchDiscoveryInfo = exchDiscoveryInfoMap.find(exchMetadataMapIter->first)->second ;
		convertToExchangeDiscoveryInfo(exchDiscoveryInfo, exchMetadata, exchdiscoveryInfo) ;
		m_discoveyInfo->exchInfo.push_back(exchdiscoveryInfo);
        exchMetadataMapIter++ ;
    }

    std::map<std::string, Exchange2k10MetaData>::const_iterator exch2010MetadataMapIter  = exch2k10MetaDataMap.begin() ;
    while( exch2010MetadataMapIter != exch2k10MetaDataMap.end() )
    {
        ExchangeDiscoveryInfo exchdiscoveryInfo ;
		exchdiscoveryInfo.m_policyInfo.insert(std::make_pair("policyId","0"));
		exchdiscoveryInfo.m_policyInfo.insert(std::make_pair("InstanceId","ApplicationDiscoveryWizardUpdate"));
        const Exchange2k10MetaData& exch2k10Metadata = exch2010MetadataMapIter->second ;
        const ExchAppVersionDiscInfo& exchDiscoveryInfo = exchDiscoveryInfoMap.find(exch2010MetadataMapIter->first)->second ;
		convertToExchange2010DiscoveryInfo(exchDiscoveryInfo, exch2k10Metadata, exchdiscoveryInfo) ;
		m_discoveyInfo->exchInfo.push_back(exchdiscoveryInfo);
        exch2010MetadataMapIter++ ;
    }
	//TODO: Write logic here.
	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",__FUNCTION__);
	return filled;
}

void AppData::convertToExchangeDiscoveryInfo(const ExchAppVersionDiscInfo& exchDiscoveryInfo, 
                                                                const ExchangeMetaData& exchMetadata, 
                                                                ExchangeDiscoveryInfo& exchdiscoveryInfo
																)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    exchdiscoveryInfo.m_InstanceInformation.insert(std::make_pair("ApplicationVersion", exchDiscoveryInfo.m_version)) ;
    exchdiscoveryInfo.m_InstanceInformation.insert(std::make_pair("ApplicationEdition", "No Edition available")) ;
    exchdiscoveryInfo.m_InstanceInformation.insert(std::make_pair("ApplicationCompatibiltiNo", "10000")) ;
    exchdiscoveryInfo.m_InstanceInformation.insert(std::make_pair("InstallPath", exchDiscoveryInfo.m_installPath )) ;
    exchdiscoveryInfo.m_InstanceInformation.insert(std::make_pair("ApplicationInstanceName", exchDiscoveryInfo.m_cn)) ;
    if( exchDiscoveryInfo.m_isClustered )
    {
        exchdiscoveryInfo.m_InstanceInformation.insert(std::make_pair("IsClustered", "1")) ;
        exchdiscoveryInfo.m_InstanceInformation.insert(std::make_pair("VirtualServerName", exchDiscoveryInfo.m_cn));
    }
    else
    {
        exchdiscoveryInfo.m_InstanceInformation.insert(std::make_pair("IsClustered", "0")) ; 
        exchdiscoveryInfo.m_InstanceInformation.insert(std::make_pair("VirtualServerName", exchDiscoveryInfo.m_cn));
    }

    exchdiscoveryInfo.m_InstanceInformation.insert(std::make_pair("ServerRole", exchDiscoveryInfo.ServerCurrentRoleToString())) ;
    exchdiscoveryInfo.m_InstanceInformation.insert(std::make_pair("ServerType", exchDiscoveryInfo.ServerRoleToString())) ;
    exchdiscoveryInfo.m_InstanceInformation.insert(std::make_pair("DistinguishedName", exchDiscoveryInfo.m_dn)) ; 
	exchdiscoveryInfo.m_InstanceInformation.insert(std::make_pair("ErrorCode",boost::lexical_cast<std::string>(exchDiscoveryInfo.m_errCode))) ;            
	exchdiscoveryInfo.m_InstanceInformation.insert(std::make_pair("ErrorString",exchDiscoveryInfo.m_errString)) ;

    
	std::list<InmService>::const_iterator svcIter = exchDiscoveryInfo.m_services.begin() ;
    while( svcIter != exchDiscoveryInfo.m_services.end() )
    {
        InmServiceProperties svcProperties ;
        svcProperties.m_serviceProperties.insert(std::make_pair("ServiceName", svcIter->m_serviceName)) ;
        svcProperties.m_serviceProperties.insert(std::make_pair("StartupType", svcIter->typeAsStr())) ;
        svcProperties.m_serviceProperties.insert(std::make_pair("ServiceStatus", svcIter->statusAsStr())) ;
        svcProperties.m_serviceProperties.insert(std::make_pair("LogonUser", svcIter->m_logonUser)) ;
        svcProperties.m_serviceProperties.insert(std::make_pair("Dependencies", svcIter->m_dependencies)) ;
        exchdiscoveryInfo.m_services.insert(std::make_pair(svcIter->m_serviceName, svcProperties)) ;
        svcIter ++ ;
    }
    if( exchMetadata.errCode != DISCOVERY_SUCCESS )
    {
        std::map<std::string, std::string>::iterator discErrStrIter,discErrorCodeIter ;
        discErrStrIter = exchdiscoveryInfo.m_InstanceInformation.find("ErrorString") ;
        discErrorCodeIter = exchdiscoveryInfo.m_InstanceInformation.find("ErrorCode") ;
        if( exchDiscoveryInfo.m_isClustered && m_hldiscInfo.m_clusterInfo.m_clusterID.compare("") == 0 )
        {
            discErrorCodeIter->second = boost::lexical_cast<std::string>(DISCOVERY_FAIL) ;
        }
        else
        {
            if( boost::lexical_cast<int>(discErrorCodeIter->second) == DISCOVERY_SUCCESS )
            {
                discErrorCodeIter->second = boost::lexical_cast<std::string>(DISCOVERY_METADATANOTAVAILABLE) ;
            }
            else
            {
                discErrorCodeIter->second = boost::lexical_cast<std::string>(exchMetadata.errCode) ;
            }
        }
        discErrStrIter->second += exchMetadata.errString ;
    }
    std::list<ExchangeStorageGroupMetaData> storageGrpList = exchMetadata.m_storageGrps ;
    std::list<ExchangeStorageGroupMetaData>::iterator storageGrpIter = storageGrpList.begin() ;
    while( storageGrpIter != storageGrpList.end() )
    {
        StorageGroup sg ;
		std::string sgSystemVolume;
        sg.m_StorageGrpProperties.insert(std::make_pair("StorageGroupName", storageGrpIter->m_storageGrpName)) ;
        sg.m_StorageGrpProperties.insert(std::make_pair("TransactionLogPath", storageGrpIter->m_logFilePath)) ;
		sg.m_StorageGrpProperties.insert(std::make_pair("StorageGroupSystemPath", storageGrpIter->m_systemPath)) ;
		if(getVolumePath(storageGrpIter->m_systemPath,sgSystemVolume))
		{
			sanitizeVolumePathName(sgSystemVolume);
			sg.m_StorageGrpProperties.insert(std::make_pair("StorageGroupSystemVolume", sgSystemVolume)) ;
		}
        sg.m_StorageGrpProperties.insert(std::make_pair("OnlineStatus", "ONLINE")) ;
            
        if( storageGrpIter->m_lcrEnabled )
        {
            sg.m_StorageGrpProperties.insert(std::make_pair("LCRStatus", "ENABLED")) ;
        }
        else
        {
            sg.m_StorageGrpProperties.insert(std::make_pair("LCRStatus", "DISABLED")) ;
        }
        if( storageGrpIter->m_msExchStandbyCopyMachines.compare("") == 0 )
        {
            sg.m_StorageGrpProperties.insert(std::make_pair("SCRStatus", "DISABLED")) ;
        }
        else
        {
            sg.m_StorageGrpProperties.insert(std::make_pair("SCRStatus", "ENABLED")) ;
        }
        const std::string& clusterStorageType = exchDiscoveryInfo.ClusterStorageTypeToStr() ;
        if( clusterStorageType.compare("Disabled")== 0 )
        {
            sg.m_StorageGrpProperties.insert(std::make_pair("CCRSTATUS", "DISABLED")  ) ;
            sg.m_StorageGrpProperties.insert(std::make_pair("ClusterStorageType", clusterStorageType)) ;
        }
        else
        {
            sg.m_StorageGrpProperties.insert(std::make_pair("CCRSTATUS", "DISABLED")) ;
            sg.m_StorageGrpProperties.insert(std::make_pair("ClusterStorageType", "N.A.")) ;
        }
	    std::string tempString1;
	    tempString1 = storageGrpIter->m_logVolumePath;
	    sanitizeVolumePathName(tempString1);
        sg.m_volumes.push_back( tempString1 ) ;
        std::list<ExchangeMDBMetaData>::iterator mdbIter = storageGrpIter->m_mdbMetaDataList.begin()  ;
        while( mdbIter != storageGrpIter->m_mdbMetaDataList.end() )
        {
            MailBox mb ;
            mb.m_mailBoxProperties.insert(std::make_pair("MailBoxName", mdbIter->m_mdbName)) ;
            mb.m_mailBoxProperties.insert(std::make_pair("OnlineStatus", "ONLINE")) ;
            mb.m_mailBoxProperties.insert(std::make_pair("EdbFilePath", mdbIter->m_edbFilePath)) ;
            if( mdbIter->m_type == EXCHANGE_PRIV_MDB )
            {
                mb.m_mailBoxProperties.insert(std::make_pair("MailStoreType", "PRIVATE")) ;
                mb.m_mailBoxProperties.insert(std::make_pair("PublicFolderDatabaseMailServerName", mdbIter->m_defaultPublicFolderDBs_MailBoxServerName)) ;
                mb.m_mailBoxProperties.insert(std::make_pair("PublicFolderDatabaseStorageGroup", mdbIter->m_defaultPublicFolderDBs_StorageGRPName)) ;
                mb.m_mailBoxProperties.insert(std::make_pair("PublicFolderDatabaseName", mdbIter->m_defaultPublicFolderDBName)) ;
            }
            else if( mdbIter->m_type == EXCHANGE_PUB_MDB )
            {
                mb.m_mailBoxProperties.insert(std::make_pair("MailStoreType", "PUBLIC")) ;
            }

		    std::string tempString;
            tempString = mdbIter->m_edbVol ;
		    sanitizeVolumePathName(tempString);
            mb.mailstoreFiles.insert( std::make_pair(mdbIter->m_edbFilePath,  tempString)) ;
            if( mdbIter->m_streamingDB.compare("") != 0 )
            {
                tempString = mdbIter->m_streamingVol ;
		        sanitizeVolumePathName(tempString);
                mb.mailstoreFiles.insert( std::make_pair(mdbIter->m_streamingDB,  tempString)) ;
            }
            mb.m_mailBoxProperties.insert(std::make_pair("ErrorCode", boost::lexical_cast<std::string>(mdbIter->errCode))) ;
            mb.m_mailBoxProperties.insert(std::make_pair("ErrorString", mdbIter->errString)) ;
            sg.m_mailBoxes.insert(std::make_pair(mdbIter->m_mdbName, mb)) ;
            mdbIter++ ;
        }
        sg.m_StorageGrpProperties.insert(std::make_pair("ErrorCode",boost::lexical_cast<std::string> (storageGrpIter->errCode))) ;
        sg.m_StorageGrpProperties.insert(std::make_pair("ErrorString",storageGrpIter->errString)) ;
        exchdiscoveryInfo.m_storageGrpMap.insert(std::make_pair(storageGrpIter->m_storageGrpName, sg)) ;
        storageGrpIter ++ ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}

void AppData::convertToExchange2010DiscoveryInfo(const ExchAppVersionDiscInfo& exchVerDiscoveryInfo, 
                                                                const Exchange2k10MetaData& exch2010Metadata, 
                                                                ExchangeDiscoveryInfo& exchdiscoveryInfo)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    exchdiscoveryInfo.m_InstanceInformation.insert(std::make_pair("ApplicationVersion", exchVerDiscoveryInfo.m_version)) ;
    exchdiscoveryInfo.m_InstanceInformation.insert(std::make_pair("ApplicationEdition", "No Edition available")) ;
    exchdiscoveryInfo.m_InstanceInformation.insert(std::make_pair("ApplicationCompatibiltiNo", "10000")) ;
    exchdiscoveryInfo.m_InstanceInformation.insert(std::make_pair("InstallPath", exchVerDiscoveryInfo.m_installPath )) ;
    exchdiscoveryInfo.m_InstanceInformation.insert(std::make_pair("ApplicationInstanceName", exchVerDiscoveryInfo.m_cn)) ;
    exchdiscoveryInfo.m_InstanceInformation.insert(std::make_pair("ServerRole", exchVerDiscoveryInfo.ServerCurrentRoleToString())) ;
    exchdiscoveryInfo.m_InstanceInformation.insert(std::make_pair("ServerType", exchVerDiscoveryInfo.ServerRoleToString())) ;
    exchdiscoveryInfo.m_InstanceInformation.insert(std::make_pair("DistinguishedName", exchVerDiscoveryInfo.m_dn)) ; 

	exchdiscoveryInfo.m_InstanceInformation.insert(std::make_pair("ErrorCode", boost::lexical_cast<std::string>(exchVerDiscoveryInfo.m_errCode))) ;            
	exchdiscoveryInfo.m_InstanceInformation.insert(std::make_pair("ErrorString", exchVerDiscoveryInfo.m_errString)) ;

	std::string DAGName = "";
	std::string hostName = Host::GetInstance().GetHostName();
	if( !exch2010Metadata.m_mdbMetaDataList.empty() && 
		strcmpi(exch2010Metadata.m_mdbMetaDataList.begin()->m_exch2k10MDBmetaData.m_dagName.c_str(), hostName.c_str()))
	{
		DAGName = exch2010Metadata.m_mdbMetaDataList.begin()->m_exch2k10MDBmetaData.m_dagName;
		exchdiscoveryInfo.m_InstanceInformation.insert(std::make_pair("DataAvailabilityGroup", DAGName)) ; 
	}	
	std::list<ExchangeMDBMetaData>::const_iterator exch2k10Iter = exch2010Metadata.m_mdbMetaDataList.begin();
	if( exch2k10Iter != exch2010Metadata.m_mdbMetaDataList.end() )
	{
		exchdiscoveryInfo.m_exchange2010Data.m_dagParticipants = exch2k10Iter->m_exch2k10MDBmetaData.m_participantsServersList;
	}
    if( exch2010Metadata.errCode != DISCOVERY_SUCCESS )
    {
        std::map<std::string, std::string>::iterator discErrStrIter,discErrorCodeIter ;
        discErrStrIter = exchdiscoveryInfo.m_InstanceInformation.find("ErrorString") ;
        discErrorCodeIter = exchdiscoveryInfo.m_InstanceInformation.find("ErrorCode") ;
        if(  boost::lexical_cast<int>(discErrorCodeIter->second) == DISCOVERY_SUCCESS )
        {
            discErrorCodeIter->second = boost::lexical_cast<std::string>(DISCOVERY_METADATANOTAVAILABLE) ;
        }
        else
        {
            discErrorCodeIter->second = boost::lexical_cast<std::string>(exch2010Metadata.errCode) ;
        }
        discErrStrIter->second += exch2010Metadata.errString ;
    }
	while( exch2k10Iter != exch2010Metadata.m_mdbMetaDataList.end() )
	{
		MailBox mb;
		mb.m_mailBoxProperties.insert(std::make_pair("MailBoxName", exch2k10Iter->m_mdbName ));
		mb.m_mailBoxProperties.insert(std::make_pair("OnlineStatus","ONLINE" ));
		mb.m_mailBoxProperties.insert(std::make_pair("TransactionLogPath", exch2k10Iter->m_exch2k10MDBmetaData.m_msExchESEParamLogFilePath ));
		mb.m_mailBoxProperties.insert(std::make_pair("EdbFilePath", exch2k10Iter->m_edbFilePath ));
		mb.m_mailBoxProperties.insert(std::make_pair("GUID", exch2k10Iter->m_exch2k10MDBmetaData.m_guid));
		mb.m_mailBoxProperties.insert(std::make_pair("MountInfo", exch2k10Iter->m_exch2k10MDBmetaData.m_mountInfo));
		if( exch2k10Iter->m_type == EXCHANGE_PRIV_MDB )
		{
			mb.m_mailBoxProperties.insert(std::make_pair("MailStoreType", "PRIVATE"));
			mb.m_mailBoxProperties.insert(std::make_pair("PublicFolderDatabaseMailServerName", exch2k10Iter->m_defaultPublicFolderDBs_MailBoxServerName ));
			mb.m_mailBoxProperties.insert(std::make_pair("PublicFolderDatabaseName", exch2k10Iter->m_defaultPublicFolderDBName));
		}
		else if( exch2k10Iter->m_type == EXCHANGE_PUB_MDB )
		{
			mb.m_mailBoxProperties.insert( std::make_pair("MailStoreType", "PUBLIC"));					
		}
		std::string tempString;
        tempString = exch2k10Iter->m_edbVol ;
        sanitizeVolumePathName(tempString);
        mb.mailstoreFiles.insert( std::make_pair(exch2k10Iter->m_edbFilePath,  tempString)) ;

        tempString = exch2k10Iter->m_exch2k10MDBmetaData.m_msExchESEParamLogFileVol ;
        sanitizeVolumePathName(tempString);
        mb.mailstoreFiles.insert( std::make_pair(exch2k10Iter->m_exch2k10MDBmetaData.m_msExchESEParamLogFilePath,  tempString)) ;

		std::string hostName = Host::GetInstance().GetHostName();
		if( strcmpi(DAGName.c_str(), hostName.c_str() ))
		{
			mb.m_copyHosts = exch2k10Iter->m_exch2k10MDBmetaData.m_exchangeMDBCopysList;
		}
        mb.m_mailBoxProperties.insert(std::make_pair("ErrorCode", boost::lexical_cast<std::string>(exch2k10Iter->errCode))) ;
        mb.m_mailBoxProperties.insert(std::make_pair("ErrorString", exch2k10Iter->errString)) ;
		exchdiscoveryInfo.m_exchange2010Data.m_mailBoxes.insert(std::make_pair(exch2k10Iter->m_mdbName, mb));
		exch2k10Iter++;
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}

bool AppData::fillFileServerDiscoveryInfo()
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",__FUNCTION__); 
	bool filled = true;
	std::list<std::string> networkNameList;
	m_fileservApp.getNetworkNameList(networkNameList);
	std::list<std::string>::iterator networkNameIter = networkNameList.begin();
	while ( networkNameIter != networkNameList.end() )
	{
		FileServerInfo fsInfo;

		fsInfo.m_policyInfo.insert(std::make_pair("policyId","0"));
		fsInfo.m_policyInfo.insert(std::make_pair("InstanceId","ApplicationDiscoveryWizardUpdate"));//Temp logic

		fillFileServerDiscoveryData(*networkNameIter,fsInfo);
		m_discoveyInfo->fileserverInfo.push_back(fsInfo);

		networkNameIter++;
	}

	//TODO: Write logic here.
	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",__FUNCTION__);
	return filled;
}
SVERROR AppData::fillFileServerDiscoveryData( std::string& networkName, FileServerInfo& fsInfo)
{
	SVERROR bRet = SVS_FALSE ;
    DebugPrintf( SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME ) ;

	std::map<std::string, FileServerInstanceDiscoveryData>::iterator FileServerInstanceDiscoveryDataIter;
	FileServerInstanceDiscoveryDataIter = m_fileservApp.m_fileShareInstanceDiscoveryMap.find(networkName);
	if( FileServerInstanceDiscoveryDataIter != m_fileservApp.m_fileShareInstanceDiscoveryMap.end() )	
	{
		fsInfo.m_ipAddressList = FileServerInstanceDiscoveryDataIter->second.m_ips;
	}
	std::map<std::string, FileServerInstanceMetaData>::iterator FileServerInstanceMetaDataIter;
	FileServerInstanceMetaDataIter = m_fileservApp.m_fileShareInstanceMetaDataMap.find(networkName);
	if( FileServerInstanceMetaDataIter != m_fileservApp.m_fileShareInstanceMetaDataMap.end() )
	{
		fsInfo.m_InstanceInformation.insert( std::make_pair("ApplicationInstanceName", networkName) );
		if( FileServerInstanceMetaDataIter->second.m_isClustered )
		{
			fsInfo.m_InstanceInformation.insert(std::make_pair("IsClustered", "1"));
		}
		else
		{
			fsInfo.m_InstanceInformation.insert(std::make_pair("IsClustered", "0"));
		}
		fsInfo.m_InstanceInformation.insert(std::make_pair("Version", m_fileservApp.m_registryVersion));

		fsInfo.m_InstanceInformation.insert(std::make_pair("ErrorCode", boost::lexical_cast<std::string>(FileServerInstanceMetaDataIter->second.m_ErrorCode))); 
		fsInfo.m_InstanceInformation.insert(std::make_pair("ErrorString", FileServerInstanceMetaDataIter->second.m_ErrorString));  
		
		fsInfo.m_volumes.insert(fsInfo.m_volumes.end(), FileServerInstanceMetaDataIter->second.m_volumes.begin(), FileServerInstanceMetaDataIter->second.m_volumes.end() );

		std::map<std::string, FileShareInfo>::iterator fileShareInfoIter;
		fileShareInfoIter = FileServerInstanceMetaDataIter->second.m_shareInfo.begin();
		while( fileShareInfoIter != FileServerInstanceMetaDataIter->second.m_shareInfo.end() )
		{
			ShareProperties shareProps;
			shareProps.m_properties.insert(std::make_pair( "ShareName", fileShareInfoIter->second.m_shareName ));
			std::string tempVolume = fileShareInfoIter->second.m_mountPoint;
			sanitizeVolumePathName(tempVolume);			
			shareProps.m_properties.insert(std::make_pair( "MountPoint", tempVolume ));

			shareProps.m_properties.insert(std::make_pair( "AbsolutePath", fileShareInfoIter->second.m_absolutePath ));
			
			shareProps.m_properties.insert(std::make_pair( "Shares", fileShareInfoIter->second.m_sharesAscii )); 
			
			shareProps.m_properties.insert(std::make_pair( "Security", fileShareInfoIter->second.m_securityAscii ));

			if( isClusterNode() )
			{
				OSVERSIONINFOEX osVersionInfo ;
				getOSVersionInfo(osVersionInfo) ;
				if(osVersionInfo.dwMajorVersion == 6) //making this condition true for windows 2008 sp1,sp2 and R2
				{
					char shareuserbuf[sizeof(unsigned long)*8+1];
					char sharetypebuf[sizeof(unsigned long)*8+1];
					shareProps.m_properties.insert(std::make_pair( "ShareUsers", ultoa(fileShareInfoIter->second.m_shi503_max_uses,shareuserbuf, 16)));

					shareProps.m_properties.insert(std::make_pair( "SecurityPwd", fileShareInfoIter->second.m_shi503_passwd ));

					shareProps.m_properties.insert(std::make_pair( "ShareRemark", fileShareInfoIter->second.m_shi503_remark ));

					shareProps.m_properties.insert(std::make_pair( "ShareType", ultoa(fileShareInfoIter->second.m_shi503_type, sharetypebuf, 16)));
				}
			}

			fsInfo.m_shareProperties.insert(std::make_pair(fileShareInfoIter->second.m_shareName, shareProps) );
			fileShareInfoIter++;
		}
	}
	else
	{
		std::stringstream errorString;
		errorString << "Instance Name " << networkName << " not found";
		fsInfo.m_InstanceInformation.insert(std::make_pair("ErrorCode", "-1")); 
		fsInfo.m_InstanceInformation.insert(std::make_pair("ErrorString", errorString.str()));  
		DebugPrintf(SV_LOG_DEBUG, "The network name %s not found in the discovered info.\n", networkName.c_str());
	}
	bRet = SVS_OK; 
 	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return bRet;	
}

bool AppData::fillMsSQLDiscoveryInfo()
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",__FUNCTION__); 
	bool filled = true;
	std::list<std::string> instanceNameList;
	m_sqlApp.getInstanceNameList(instanceNameList);
	std::list<std::string>::iterator iterInstanceName = instanceNameList.begin();
	while ( iterInstanceName != instanceNameList.end() )
	{
		SQLDiscoveryInfo sqlDiscInfo;
		sqlDiscInfo.m_policyInfo.insert(std::make_pair("policyId","0"));
		sqlDiscInfo.m_policyInfo.insert(std::make_pair("InstanceId","ApplicationDiscoveryWizardUpdate"));

		fillSQLBasicData(*iterInstanceName,sqlDiscInfo.m_InstanceInformation);
		fillSQLMetaData(*iterInstanceName,sqlDiscInfo.m_sqlMetaDatInfo);

		m_discoveyInfo->sqlInfo.push_back(sqlDiscInfo);
		iterInstanceName++;
	}
	//TODO: Write logic here.
	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",__FUNCTION__);
	return filled;
}

void AppData::fillSQLBasicData(const std::string instanceName,
		std::map<std::string,std::string>& instnceInfoMap)
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",__FUNCTION__); 
	MSSQLInstanceDiscoveryInfo instanceDiscInfo;
    if(m_sqlApp.getDiscoveredDataByInstance(instanceName, instanceDiscInfo) == SVS_OK)
    {
        instnceInfoMap.insert(std::make_pair("ApplicationInstanceName", instanceDiscInfo.m_instanceName));
        instnceInfoMap.insert(std::make_pair("ApplicationVersion", instanceDiscInfo.m_version));
        instnceInfoMap.insert(std::make_pair("ApplicationEdition", instanceDiscInfo.m_edition));
        instnceInfoMap.insert(std::make_pair("ApplicationCompatibiltiNo", "1000000"));
        instnceInfoMap.insert(std::make_pair("InstallPath", instanceDiscInfo.m_installPath));
        instnceInfoMap.insert(std::make_pair("DataRoot", instanceDiscInfo.m_dataDir));
        if(instanceDiscInfo.m_isClustered || instanceDiscInfo.m_virtualSrvrName.compare("") != 0)
        {
            instnceInfoMap.insert(std::make_pair("IsClustered", "1"));
            instnceInfoMap.insert(std::make_pair("VirtualServerName", instanceDiscInfo.m_virtualSrvrName));
        }
        else
        {
            instnceInfoMap.insert(std::make_pair("IsClustered", "0"));
            instnceInfoMap.insert(std::make_pair("VirtualServerName", instanceDiscInfo.m_virtualSrvrName));
        }        
        if( instanceDiscInfo.m_version.compare("") == 0 || 
            ( m_hldiscInfo.m_clusterInfo.m_clusterID.compare("") == 0 && instanceDiscInfo.m_isClustered ) )
        {
            instnceInfoMap.insert(std::make_pair("ErrorCode",boost::lexical_cast<std::string>(-1)));
            instnceInfoMap.insert(std::make_pair("ErrorCode","The discovery information is not available"));
        }
        else
        {
            instnceInfoMap.insert(std::make_pair("ErrorString", instanceDiscInfo.errStr));
            instnceInfoMap.insert(std::make_pair("ErrorCode",boost::lexical_cast<std::string>(instanceDiscInfo.errCode)));
		}
    }
	//TODO: Write logic here.
	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",__FUNCTION__);
}

void AppData::fillSQLMetaData(const std::string& instanceName, std::map<std::string, MSSQLDBMetaDataInfo>& sqlMetaDatInfoMap)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);	
    MSSQLInstanceMetaData instanceMetaData;
    if( m_sqlApp.getDiscoveredMetaDataByInstance(instanceName, instanceMetaData) == SVS_OK )
    {
        std::map<std::string, MSSQLDBMetaData>::iterator MSSQLDBMetaDataIter = instanceMetaData.m_dbsMap.begin();
        std::string tempString;

        while(MSSQLDBMetaDataIter != instanceMetaData.m_dbsMap.end())
        {	
            MSSQLDBMetaDataInfo metaDataInfo;

            if(MSSQLDBMetaDataIter->second.m_dbOnline)
            {
                metaDataInfo.m_properties.insert(std::make_pair("Status", "ONLINE"));	
            }
            else
            {
                metaDataInfo.m_properties.insert(std::make_pair("Status", "OFFLINE"));	
            }
            metaDataInfo.m_properties.insert(std::make_pair("DbType", boost::lexical_cast<std::string>(MSSQLDBMetaDataIter->second.m_dbType)));	
            std::list<std::string>::iterator volIter = MSSQLDBMetaDataIter->second.m_volumes.begin();
            while(volIter != MSSQLDBMetaDataIter->second.m_volumes.end())
            {
                tempString = *volIter;
                sanitizeVolumePathName(tempString);
                metaDataInfo.m_dbVolumes.push_back(tempString);
                volIter++;
            }
            metaDataInfo.m_properties.insert(std::make_pair("ErrorCode",boost::lexical_cast<std::string>(MSSQLDBMetaDataIter->second.errCode)));
            metaDataInfo.m_properties.insert(std::make_pair("ErrorString",boost::lexical_cast<std::string>(MSSQLDBMetaDataIter->second.errorString)));

            metaDataInfo.m_LogFilesList = MSSQLDBMetaDataIter->second.m_dbFiles;
            sqlMetaDatInfoMap.insert(std::make_pair(MSSQLDBMetaDataIter->first, metaDataInfo));  
            MSSQLDBMetaDataIter++;
        }
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}
bool getVolumePath(const std::string& filePath, std::string& volPath)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    bool retVal = false ;
    int statRetVal ;
    ACE_stat stat ;

    do
    {
        if( (statRetVal = sv_stat( getLongPathName(filePath.c_str()).c_str(), &stat )) != 0 )
        {
            DebugPrintf(SV_LOG_WARNING, "Unable to stat %s. This can be ignored if the volume that contains the file/folder is not available\n", filePath.c_str()) ;
            break ;
        }
        CHAR driveLetter[1024] ;
        // PR#10815: Long Path support
        if (SVGetVolumePathName(filePath.c_str(), driveLetter, ARRAYSIZE(driveLetter)) == FALSE)
        {
            DebugPrintf(SV_LOG_ERROR, "SVGetVolumePathName for %s is failed with error %ld \n", filePath.c_str(), GetLastError()) ;
            break ;
        }
        if(  (statRetVal = sv_stat( getLongPathName(filePath.c_str()).c_str(), &stat )) != 0 )
        {
            DebugPrintf(SV_LOG_WARNING, "Unable to stat %s. This can be ignored if the volume that contains file/folder is not available\n", filePath.c_str()) ;
            break ;
        }
        retVal = true ;
        volPath = driveLetter ;
    } while( false ) ;
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return retVal ;
}
DiscoveryError AppData::isDiscoverySuccess(std::string & errorMessage)
{
	DiscoveryError DiscError = DISCOVERY_SUCCESSFUL;
	std::map<std::string,ExchAppVersionDiscInfo>::const_iterator exchIter = m_pExchangeApp.m_exchDiscoveryInfoMap.begin();
	while( (exchIter != m_pExchangeApp.m_exchDiscoveryInfoMap.end()) && (DiscError == DISCOVERY_SUCCESSFUL))
	{
		const ExchAppVersionDiscInfo &appVerInfo = exchIter->second;
		if( appVerInfo.m_appType	== INM_APP_EXCH2003 || appVerInfo.m_appType	== INM_APP_EXCH2007 || appVerInfo.m_appType == INM_APP_EXCHNAGE )
		{
			ExchangeMetaData& tempMetadata = m_pExchangeApp.m_exchMetaDataMap[exchIter->first];
			if(tempMetadata.errCode != DISCOVERY_SUCCESS)
			{
				DiscError = DISCOVERY_FAILED_EXCHANGE;
				errorMessage = tempMetadata.errString;
			}
		}
		else if( appVerInfo.m_appType == INM_APP_EXCH2010 )
		{
			Exchange2k10MetaData& tempMetadata = m_pExchangeApp.m_exch2k10MetaDataMap[exchIter->first];
			if(tempMetadata.errCode != DISCOVERY_SUCCESS)
			{
				DiscError = DISCOVERY_FAILED_EXCHANGE;
				errorMessage = tempMetadata.errString;
			}
		}
		else
		{
			DiscError = DISCOCVERY_FAILED;
			errorMessage = "Unknown version of exchange found.";
		}
		exchIter++;
	}
	//TODO: Implement the similar logic for MSSQL and Fileserver
	return DiscError;
}