#include <list>
#include "Db2discoveryandvalidator.h"
#include "config/appconfigurator.h"
#include <boost/lexical_cast.hpp>
#include "ruleengine/unix/ruleengine.h"
#include "util/common/util.h"
#include "discovery/discoverycontroller.h"
#include <boost/lexical_cast.hpp>
#include "appscheduler.h"

Db2DiscoveryAndValidator::Db2DiscoveryAndValidator(const ApplicationPolicy& policy)
:DiscoveryAndValidator(policy)
{
}

void Db2DiscoveryAndValidator::convertToDb2DiscoveryInfo(const Db2AppDiscoveryInfo& db2AppDiscoveryInfo, 
        Db2DatabaseDiscoveryInfo& db2discoveryInfo )
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    DiscoveryControllerPtr discCtrlPtr = DiscoveryController::getInstance() ;

    db2discoveryInfo.m_db2InstanceInfoMap.clear();
    db2discoveryInfo.m_db2UnregisterInfoMap.clear();   
    
	std::string dbname;
    std::string instance;
    std::string dbinstance;
	Db2UnregisterDiscInfo db2UnregisterDiscInfo;
	std::map<std::string, Db2UnregisterDiscInfo> db2UnregisterDiscInfoMap = db2AppDiscoveryInfo.m_db2UnregisterDiscInfo;
    std::map<std::string, Db2UnregisterDiscInfo>::const_iterator unregisterIter = db2UnregisterDiscInfoMap.begin();	
	
    for(; unregisterIter != db2UnregisterDiscInfoMap.end(); unregisterIter++)
    {
        Db2UnregisterInfo db2UnregisterInfo;

        //db2UnregisterInfo.m_dbName = unregisterIter->second;
		/*std::pair <std::map<std::string,Db2UnregisterDiscInfo>::iterator, std::map<std::string,Db2UnregisterDiscInfo>::iterator> unregRet;
		unregRet = (db2UnregisterDiscInfoMap).equal_range((*unregisterIter).first);
		
		for (std::map<std::string,Db2UnregisterDiscInfo>::iterator itr=unregRet.first; itr!=unregRet.second; ++itr)
        {
            db2UnregisterDiscInfo=itr->second;
			dblist.push_back(db2UnregisterDiscInfo.m_dbName);
			
			DebugPrintf(SV_LOG_DEBUG, "DB NAME TO UNREGISTER --> %s under DB Instance --> %s\n",(itr->second).m_dbName.c_str(),unregisterIter->first.c_str());
        }*/
		
        for(std::map<std::string, Db2UnregisterDiscInfo>::iterator itr=db2UnregisterDiscInfoMap.begin();itr!=db2UnregisterDiscInfoMap.end();itr++)
        {
           instance=itr->first;           
           for(std::list<std::string>::iterator dbitr=((itr->second).m_dbNames).begin();dbitr!=((itr->second).m_dbNames).end();dbitr++)
           {
               dbinstance=instance+":"+(*dbitr);
               db2UnregisterInfo.m_dbName=dbinstance;
               DebugPrintf(SV_LOG_DEBUG, "DB NAME TO UNREGISTER --> %s \n",dbinstance.c_str());
               db2discoveryInfo.m_db2UnregisterInfoMap.insert(std::make_pair(dbinstance, db2UnregisterInfo));
           }
        }
    }
	
	std::map<std::string, Db2AppInstanceDiscInfo> db2AppInstanceDiscInfoMap = db2AppDiscoveryInfo.m_db2InstDiscInfo;
    std::map<std::string, Db2AppInstanceDiscInfo>::const_iterator discoveryIter = db2AppInstanceDiscInfoMap.begin();
		
    for(; discoveryIter != db2AppInstanceDiscInfoMap.end(); discoveryIter++)
    {
        DebugPrintf(SV_LOG_DEBUG, "DB Instance TO REPORT --> %s\n",discoveryIter->first.c_str());
		//DebugPrintf(SV_LOG_DEBUG, "DB NAME TO REPORT --> %s\n",discoveryIter->first.c_str());
		
		//Db2DBInstanceInfo db2DBInstanceInfo;        		
		Db2AppInstanceDiscInfo db2appInstanceDiscInfo;
	
		//std::pair <std::multimap<std::string,Db2AppInstanceDiscInfo>::iterator, std::multimap<std::string,Db2AppInstanceDiscInfo>::iterator> InstRet;	
		//InstRet=(db2AppInstanceDiscInfoMap).equal_range((*discoveryIter).first);
		
		//for (std::multimap<std::string,Db2AppInstanceDiscInfo>::iterator it=InstRet.first; it!=InstRet.second; ++it)
       // {
				//db2appInstanceDiscInfo=it->second;			
				db2appInstanceDiscInfo=discoveryIter->second;			
				
				std::map<std::string, Db2AppDatabaseDiscInfo> db2AppDatabaseDiscInfo;
				db2AppDatabaseDiscInfo=db2appInstanceDiscInfo.m_db2DiscInfo;
				Db2AppDatabaseDiscInfo db2AppDbDiscInfo;
				Db2DatabaseInfo db2DatabaseInfo;
				for(std::map<std::string, Db2AppDatabaseDiscInfo>::iterator iter=db2AppDatabaseDiscInfo.begin();iter!=db2AppDatabaseDiscInfo.end();iter++)
				{
					dbname=discoveryIter->first+":"+(*iter).first;					
					DebugPrintf(SV_LOG_DEBUG, "DB NAME TO REPORT --> %s\n",((*iter).first).c_str());
					db2AppDbDiscInfo=(*iter).second;
										
					db2DatabaseInfo.m_dbProperties.insert(std::make_pair("DBName",db2AppDbDiscInfo.m_dbName));
					db2DatabaseInfo.m_dbProperties.insert(std::make_pair("DBInstallPath",db2appInstanceDiscInfo.m_dbInstallPath));
					db2DatabaseInfo.m_dbProperties.insert(std::make_pair("DBLocation",db2AppDbDiscInfo.m_dbLocation));
					if(db2AppDbDiscInfo.m_recoveryLogEnabled)
						db2DatabaseInfo.m_dbProperties.insert(std::make_pair("RecoveryLogEnabled", "1"));
					else
						db2DatabaseInfo.m_dbProperties.insert(std::make_pair("RecoveryLogEnabled", "0"));
					db2DatabaseInfo.m_dbProperties.insert(std::make_pair("NodeName",db2appInstanceDiscInfo.m_nodeName));
					db2DatabaseInfo.m_dbProperties.insert(std::make_pair("DatabaseVersion",db2appInstanceDiscInfo.m_dbVersion));
					db2DatabaseInfo.m_dbProperties.insert(std::make_pair("HostId",db2appInstanceDiscInfo.m_hostId));						
					db2DatabaseInfo.m_volumeInfo=db2AppDbDiscInfo.m_diskView;
					db2DatabaseInfo.m_filterDevices=db2AppDbDiscInfo.m_filterDeviceList;
					db2DatabaseInfo.m_files=db2AppDbDiscInfo.m_filesMap;		
					
					//db2DBInstanceInfo.m_databaseInfo.insert(std::make_pair((*iter).first,db2DatabaseInfo));					
                    db2discoveryInfo.m_db2InstanceInfoMap.insert(std::make_pair(dbname.c_str(),db2DatabaseInfo));
				}				
		//}
		//db2discoveryInfo.m_db2InstanceInfoMap.insert(std::make_pair(discoveryIter->first.c_str()+":"+dbname,db2DBInstanceInfo));
	}	 

	//Print the Db2DatabaseDiscoveryInfo values for debugging purpose
	std::map<std::string, Db2DatabaseInfo> db2InstInfoMap;
	std::map<std::string, Db2DatabaseInfo>::iterator instIter;
	std::map<std::string, Db2UnregisterInfo> db2UnregInfoMap;
	std::map<std::string, Db2UnregisterInfo>::iterator unregIter;
	
	db2InstInfoMap=db2discoveryInfo.m_db2InstanceInfoMap;
    db2UnregInfoMap=db2discoveryInfo.m_db2UnregisterInfoMap;
	
	for(unregIter=db2UnregInfoMap.begin();unregIter!=db2UnregInfoMap.end();unregIter++)
	{
        std::string unregDb=((*unregIter).second).m_dbName;
		DebugPrintf(SV_LOG_DEBUG, "DB Name to Unregister --> %s \n",unregDb.c_str());
	}
	for(instIter=db2InstInfoMap.begin();instIter!=db2InstInfoMap.end();instIter++)
	{
		DebugPrintf(SV_LOG_DEBUG, "DB Instance Discoverd --> %s\n",((*instIter).first).c_str());
		
		/*std::map<std::string,Db2DatabaseInfo> db2dbInfoMap;
		db2dbInfoMap=((*instIter).second).m_databaseInfo;*/
		Db2DatabaseInfo db2dbInfo=((*instIter).second);

/*        for(std::map<std::string,Db2DatabaseInfo>::iterator it=db2dbInfoMap.begin();it!=db2dbInfoMap.end();it++)
		{
			DebugPrintf(SV_LOG_DEBUG, "DB Name Discoverd --> %s\n",((*it).first).c_str());
			Db2DatabaseInfo db2dbInfo;
			db2dbInfo=(*it).second;*/
			std::map<std::string, std::string> dbProperties;
			dbProperties=db2dbInfo.m_dbProperties;
			for(std::map<std::string, std::string>::iterator dbinfoitr=dbProperties.begin();dbinfoitr!=dbProperties.end();dbinfoitr++)
			{
				DebugPrintf(SV_LOG_DEBUG, " %s --> %s\n",((*dbinfoitr).first).c_str(),((*dbinfoitr).second).c_str());
			}
			std::list<std::string> volumeInfo;
			volumeInfo=db2dbInfo.m_volumeInfo;
			DebugPrintf(SV_LOG_DEBUG, " VolumeInfo --> \n");
			for(std::list<std::string>::iterator volInfoitr=volumeInfo.begin();volInfoitr!=volumeInfo.end();volInfoitr++)
			{
				DebugPrintf(SV_LOG_DEBUG, " %s\n",(*volInfoitr).c_str());
			}
			std::list<std::string> filterDevices;
			filterDevices=db2dbInfo.m_filterDevices;
			DebugPrintf(SV_LOG_DEBUG, " FilterDevices --> \n");
			for(std::list<std::string>::iterator filDevIter=filterDevices.begin();filDevIter!=filterDevices.end();filDevIter++)
			{
				DebugPrintf(SV_LOG_DEBUG, " %s\n",(*filDevIter).c_str());
			}
			std::map<std::string, std::string> files;
			files=db2dbInfo.m_files;
			for(std::map<std::string, std::string>::iterator fileItr=files.begin();fileItr!=files.end();fileItr++)
			{
				DebugPrintf(SV_LOG_DEBUG, " FileName --> %s\n",((*fileItr).first).c_str());
				DebugPrintf(SV_LOG_DEBUG, " FileContents --> %s\n",((*fileItr).second).c_str());
			}			
		//}
	}
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}

SVERROR Db2DiscoveryAndValidator::PostDiscoveryInfoToCx()
{
    SVERROR bRet = SVS_FALSE ;

    DebugPrintf(SV_LOG_DEBUG, "ENTERED Db2DiscoveryAndValidator::%s\n", FUNCTION_NAME);
    DiscoveryControllerPtr discCtrlPtr = DiscoveryController::getInstance() ;
    DiscoveryInfo discoverInfo ;
    Db2DatabaseDiscoveryInfo db2DiscInfo;
    Db2AppDiscoveryInfo discoveryInfo;

    fillPolicyInfoInfo(db2DiscInfo.m_policyInfo);	
    m_Db2AppPtr->getDb2AppDiscoveryInfo(discoveryInfo);
    convertToDb2DiscoveryInfo(discoveryInfo, db2DiscInfo) ;

    db2DiscInfo.m_policyInfo.insert(std::make_pair("ErrorCode", "0")) ;
    db2DiscInfo.m_policyInfo.insert(std::make_pair("ErrorString", "Db2 application discovery complete"));
    discCtrlPtr->m_disCoveryInfo->db2discoveryInfo.push_back(db2DiscInfo);

    bRet = SVS_OK ;
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ; 
}

SVERROR Db2DiscoveryAndValidator::Discover(bool shouldUpdateCx)
{
	SVERROR bRet = SVS_FALSE ;
    DebugPrintf(SV_LOG_DEBUG, "ENTERED Db2DiscoveryAndValidator::%s\n", FUNCTION_NAME) ;
    AppAgentConfiguratorPtr configuratorPtr = AppAgentConfigurator::GetAppAgentConfigurator() ;
    AppLocalConfigurator localconfigurator;
    do
    {
		if (!localconfigurator.shouldDiscoverDb2())
        {
            DebugPrintf(SV_LOG_DEBUG, "Db2Discovery is disabled in drscout configuration.\n");
            bRet = SVS_OK;
            break;
        }
        m_Db2AppPtr.reset( new Db2Application()) ;

        if( m_Db2AppPtr->init() != SVS_OK)
        {
            DebugPrintf(SV_LOG_DEBUG, "Nothing to discover. \n") ;
            bRet = SVS_OK ;
        }
        else
        {
            if(m_policyId != 0)
            {
                SV_ULONGLONG instanceId = AppScheduler::getInstance()->getInstanceId(m_policyId);
                configuratorPtr->PolicyUpdate(m_policyId,instanceId,"InProgress","");
                
                //This is to clean up the db2appwizard.conf file which contains the unregistered db info
                DebugPrintf(SV_LOG_DEBUG,"Received PolicyId :%d\n",m_policyId);
                std::string policyData =  m_policy.m_policyData ;
                DebugPrintf(SV_LOG_DEBUG,"Received PolicyData: %s\n",policyData.c_str());
                if( 0 == strlen(policyData.c_str()) )
                {
                   std::string appwizardfile = localconfigurator.getInstallPath() + "/etc/db2appwizard.conf";
                   std::ofstream out;
                   out.open(appwizardfile.c_str(), std::ios::out);
                   if (!out)
                   {
                     DebugPrintf(SV_LOG_ERROR,"Failed to open %s file\n", appwizardfile.c_str());
                     DebugPrintf(SV_LOG_ERROR,"db2appwizard.conf file doesnot exist\n");
                   }
                   else
                   {
                     DebugPrintf(SV_LOG_DEBUG,"Emptying the contents of the file %s\n",appwizardfile.c_str());
                     out << " ";
                     out.close();
                   }
                }
            }

            if (m_Db2AppPtr->discoverApplication() == SVS_OK)
            {
                if( shouldUpdateCx )
                {
                    PostDiscoveryInfoToCx();
                }

                bRet = SVS_OK;
            }
            else
            {
				Db2DatabaseDiscoveryInfo db2DiscInfo;
                fillPolicyInfoInfo(db2DiscInfo.m_policyInfo);
                db2DiscInfo.m_policyInfo.insert(std::make_pair("ErrorCode", "1")) ;
                db2DiscInfo.m_policyInfo.insert(std::make_pair("ErrorString", "Db2 application discovery failed"));

                DiscoveryControllerPtr discCtrlPtr = DiscoveryController::getInstance() ;
                discCtrlPtr->m_disCoveryInfo->db2discoveryInfo.push_back(db2DiscInfo);
            }
        }
    }while (0);
    DebugPrintf(SV_LOG_DEBUG, "EXITED Db2DiscoveryAndValidator::%s\n", FUNCTION_NAME) ;
    return bRet ;
}

SVERROR Db2DiscoveryAndValidator::PerformReadinessCheckAtSource()
{
    SVERROR bRet = SVS_FALSE ;
    DebugPrintf(SV_LOG_DEBUG, "ENTERED Db2DiscoveryAndValidator : %s\n", FUNCTION_NAME) ;

    ReadinessCheck readyCheck;
	std::list<ValidatorPtr> validations ;
    std::list<ReadinessCheck> readinessChecksList;
    RuleEnginePtr enginePtr = RuleEngine::getInstance() ;
    ReadinessCheckInfo readinessCheckInfo ;
    DiscoveryControllerPtr discCtrlPtr = DiscoveryController::getInstance() ;
    
    m_Db2AppPtr.reset( new Db2Application() ) ;
    m_Db2AppPtr->discoverApplication();

    SV_ULONGLONG instanceId = AppScheduler::getInstance()->getInstanceId(m_policyId);

    AppAgentConfiguratorPtr configuratorPtr = AppAgentConfigurator::GetAppAgentConfigurator() ;
    configuratorPtr->PolicyUpdate(m_policyId,instanceId,"InProgress","");

    std::list<InmRuleIds> failedRules ;
    bool tagVerifyRule = false;

    DebugPrintf(SV_LOG_DEBUG, "App instanceName : '%s' \n", m_appInstanceName.c_str()) ;
    
    std::string instance = m_appInstanceName.substr(0,m_appInstanceName.find_first_of(":"));
    std::string dbname = m_appInstanceName.substr(m_appInstanceName.find_first_of(":")+1);

    Db2AppInstanceDiscInfo appVerInfo;
    if( m_Db2AppPtr->m_db2DiscoveryInfoMap.find(instance) != m_Db2AppPtr->m_db2DiscoveryInfoMap.end() )
    {
        appVerInfo = m_Db2AppPtr->m_db2DiscoveryInfoMap[instance] ;
        DebugPrintf(SV_LOG_DEBUG, "App instanceName '%s' found.\n", instance.c_str()) ;
    }
	
    DebugPrintf(SV_LOG_DEBUG, "Database Name : '%s' \n", dbname.c_str()) ;
	
    validations = enginePtr->getDb2ProtectionSourceRules(appVerInfo,instance,dbname, m_appVacpOptions);

    std::list<ValidatorPtr>::iterator validateIter = validations.begin() ;
    while(validateIter != validations.end() )
    {
		(* validateIter)->setFailedRules(failedRules) ;
        bRet = enginePtr->RunRule(*validateIter) ;
        if( (*validateIter)->getStatus() != INM_RULESTAT_PASSED )
        {
            failedRules.push_back((*validateIter)->getRuleId()) ;
        }
        ReadinessCheck readyCheck ;
        (*validateIter)->convertToRedinessCheckStruct(readyCheck);
        readinessChecksList.push_back(readyCheck) ;
        validateIter ++ ;
    }

    if ((m_solutionType.compare("PRISM") == 0) && (m_scenarioType == SCENARIO_PROTECTION) && (m_scheduleType != 1))
    {
        std::string marshalledSrcReadinessCheckString = m_policy.m_policyData ;

        DebugPrintf( SV_LOG_DEBUG, "Policy Data : '%s' \n", marshalledSrcReadinessCheckString.c_str()) ;

        try
        {
            SourceReadinessCheckInfo srcReadinessCheckInfo;
            std::map<std::string, std::string> readinessData = unmarshal<std::map<std::string, std::string> >(marshalledSrcReadinessCheckString);

            if( readinessData.find( "dummyLunCheck" ) !=  readinessData.end() )
            {
               ApplianceTargetLunCheckInfo atLunzCheckInfo;

               try
               {
                  std::string marshalledDummyLunzCheckInfo = readinessData.find( "dummyLunCheck" )->second;
                  DebugPrintf( SV_LOG_DEBUG, "Marshalled Dummy Lunz data : '%s'\n", marshalledDummyLunzCheckInfo.c_str()) ;

                  atLunzCheckInfo = unmarshal<ApplianceTargetLunCheckInfo>(marshalledDummyLunzCheckInfo );
                  srcReadinessCheckInfo.m_atLunCheckInfo = atLunzCheckInfo;
               }
               catch(std::exception& ex)
               {
                 DebugPrintf(SV_LOG_ERROR, "Error while unmarshalling dummyLunz %s\n", ex.what()) ;
                 return SVS_FALSE ;
               }
               catch(...)
               {
                 DebugPrintf(SV_LOG_ERROR, "Error while unmarshalling dummyLunz\n") ;
                 return SVS_FALSE ;
               }

               validations = enginePtr->getPrismProtectionSourceRules(srcReadinessCheckInfo) ;
               validateIter = validations.begin() ;
               while(validateIter != validations.end() )
               {
                  enginePtr->RunRule(*validateIter) ;
                  if( (*validateIter)->getStatus() != INM_RULESTAT_PASSED )
                  {
                    failedRules.push_back((*validateIter)->getRuleId()) ;
                  }
                  ReadinessCheck readyCheck ;
                 (*validateIter)->convertToRedinessCheckStruct(readyCheck);
                  readinessChecksList.push_back(readyCheck) ;
                  validateIter ++ ;
               }
           }
           else
           {
                DebugPrintf(SV_LOG_ERROR, "Dummy lunz information is not found in oracle source readiness check data.\n") ;
                return SVS_FALSE ;
           }
        }
        catch(std::exception& ex)
        {
           DebugPrintf(SV_LOG_ERROR, "Error while unmarshalling readiness check%s\n", ex.what()) ;
           return SVS_FALSE ;
        }
        catch(...)
        {
           DebugPrintf(SV_LOG_ERROR, "Error while unmarshalling readiness check\n") ;
           return SVS_FALSE ;
        }
    }


	if(!readinessChecksList.empty())
    {
        readinessCheckInfo.propertiesList.insert(std::make_pair("PolicyId", boost::lexical_cast<std::string>(m_policyId))) ;
        readinessCheckInfo.propertiesList.insert(std::make_pair("InstanceId",boost::lexical_cast<std::string>(instanceId)));
        readinessCheckInfo.validationsList = readinessChecksList ;
        discCtrlPtr->m_ReadinesscheckInfoList.push_back(readinessCheckInfo);
    }

    bRet = SVS_OK ;
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}

SVERROR Db2DiscoveryAndValidator::PerformReadinessCheckAtTarget()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    SVERROR bRet = SVS_OK;
    bool isLocalContext = true;    
    
    AppSchedulerPtr schedulerPtr = AppScheduler::getInstance() ;
    DiscoveryControllerPtr discCtrlPtr = DiscoveryController::getInstance() ;
    std::list<ReadinessCheck> readinessChecksList;
    std::list<ReadinessCheckInfo> readinesscheckInfos; 

    Db2TargetReadinessCheckInfo db2TgtReadinessCheckInfo;
    RuleEnginePtr enginePtr = RuleEngine::getInstance() ;
    ReadinessCheckInfo readinessCheckInfo ;
    std::list<ValidatorPtr> validations ;

    m_Db2AppPtr.reset( new Db2Application() ) ;
    m_Db2AppPtr->discoverApplication();

    readinessChecksList.clear() ;
    std::string marshalledTgtReadinessCheckString = m_policy.m_policyData ;

    AppAgentConfiguratorPtr appConfiguratorPtr = AppAgentConfigurator::GetAppAgentConfigurator() ;
    try
    {
        DebugPrintf( SV_LOG_DEBUG, "Target Readiness Info: '%s' \n", marshalledTgtReadinessCheckString.c_str()) ;

        std::map<std::string, std::string> targetData = unmarshal<std::map<std::string, std::string> >(marshalledTgtReadinessCheckString);

        if( targetData.find( "isTargetServer" ) !=  targetData.end() )
        {
            std::string targetServer = targetData.find("isTargetServer")->second;

            DebugPrintf( SV_LOG_DEBUG, "Target Server : '%s' \n", targetServer.c_str()) ;

            if (targetServer.compare("NO") == 0)
            { 
                isLocalContext = false;
            }
        }

        if ((m_scenarioType == SCENARIO_PROTECTION) && (m_scheduleType != 1))
        {
            if (m_context == CONTEXT_HADR)
            {
                if( targetData.find( "srcDbInfo" ) !=  targetData.end() )
                {
                    try
                    {
                        std::string marshalledDbInfoString = targetData.find("srcDbInfo")->second;

                        DebugPrintf( SV_LOG_DEBUG, "Source DB Info: '%s' \n", marshalledDbInfoString.c_str()) ;

                        std::list<std::map<std::string, std::string> > dbInfo = unmarshal<std::list<std::map<std::string, std::string> > >(marshalledDbInfoString);
                        db2TgtReadinessCheckInfo.m_db2SrcInstances = dbInfo;
                    }
                    catch(std::exception& ex)
                    {
                        DebugPrintf(SV_LOG_ERROR, "Error while unmarshalling src dbinfo : %s\n", ex.what()) ;
                        return SVS_FALSE ;
                    }
                    catch(...)
                    {
                        DebugPrintf(SV_LOG_ERROR, "Error while unmarshalling src dbinfo\n") ;
                        return SVS_FALSE ;
                    }
                }
                else
                {
                    DebugPrintf(SV_LOG_ERROR, "src dbinfo is missing target readiness data\n") ;
                    return SVS_FALSE ;
                }
            }

            if( targetData.find( "tgtDbInfo" ) !=  targetData.end() )
            {
                try
                {
                    std::string marshalledDbInfoString = targetData.find("tgtDbInfo")->second;

                    DebugPrintf( SV_LOG_DEBUG, "Target DB Info: '%s' \n", marshalledDbInfoString.c_str()) ;

                    std::list<std::map<std::string, std::string> > dbInfo = unmarshal<std::list<std::map<std::string, std::string> > >(marshalledDbInfoString);
                    std::list<std::map<std::string, std::string> >::iterator dbInfoIter = dbInfo.begin();
                    std::list<std::map<std::string, std::string> > tgtDbInfo;
                    while(dbInfoIter != dbInfo.end())
                    {
                        std::string dbInstanceName = dbInfoIter->find("InstanceName")->second;
                        std::string InstanceName = dbInstanceName.substr(0,dbInstanceName.find_first_of(":"));
                        std::string dbName = dbInstanceName.substr(dbInstanceName.find_first_of(":")+1);
                        std::map<std::string, std::string> dbInstanceInfo;
						std::string instance;
						Db2AppInstanceDiscInfo db2AppInstDiscInfo;
						std::map<std::string, Db2AppDatabaseDiscInfo> db2AppDbDiscInfoMap;
                        dbInstanceInfo.insert(std::make_pair("dbInstance", InstanceName));
						dbInstanceInfo.insert(std::make_pair("dbName", dbName));
						
                        if( m_Db2AppPtr->m_db2DiscoveryInfoMap.find(InstanceName) != m_Db2AppPtr->m_db2DiscoveryInfoMap.end() )
                        {
							DebugPrintf(SV_LOG_DEBUG, "App instance '%s' online.\n", InstanceName.c_str()) ;
						    std::map<std::string, Db2AppInstanceDiscInfo>::iterator it;
							for(it = m_Db2AppPtr->m_db2DiscoveryInfoMap.begin();it != m_Db2AppPtr->m_db2DiscoveryInfoMap.end();it++)
							{
								instance = (it->first).c_str();
								if( strcmpi(instance.c_str(),InstanceName.c_str()) == 0)
								{
									db2AppInstDiscInfo = (it->second);
									db2AppDbDiscInfoMap = db2AppInstDiscInfo.m_db2DiscInfo;
									if(db2AppDbDiscInfoMap.find(dbName) != db2AppDbDiscInfoMap.end())
									{
										DebugPrintf(SV_LOG_DEBUG, "Database '%s' online.\n", dbName.c_str()) ;
										dbInstanceInfo.insert(std::make_pair("Status", "Online"));
									}
									else
									{
										DebugPrintf(SV_LOG_DEBUG, "Database '%s' is not online.\n", dbName.c_str()) ;
										dbInstanceInfo.insert(std::make_pair("Status", "Offline"));
									}
								}
							}                            
                        }
                        else
                        {
                            DebugPrintf(SV_LOG_DEBUG, "App instance '%s' is not online.\n", InstanceName.c_str()) ;
                            dbInstanceInfo.insert(std::make_pair("Status", "Offline"));
                        }

                        tgtDbInfo.push_back(dbInstanceInfo);

                        dbInfoIter++;
                    }

                    db2TgtReadinessCheckInfo.m_db2TgtInstances = tgtDbInfo;
                }
                catch(std::exception& ex)
                {
                    DebugPrintf(SV_LOG_ERROR, "Error while unmarshalling dbinfo : %s\n", ex.what()) ;
                    return SVS_FALSE ;
                }
                catch(...)
                {
                    DebugPrintf(SV_LOG_ERROR, "Error while unmarshalling dbinfo\n") ;
                    return SVS_FALSE ;
                }
            }

            if( targetData.find( "tgtVolInfo" ) !=  targetData.end() )
            {
                try
                {
                    std::string marshalledVolInfoString = targetData.find("tgtVolInfo")->second;

                    DebugPrintf( SV_LOG_DEBUG, "Volume Info: '%s' \n", marshalledVolInfoString.c_str()) ;
                    std::list<std::map<std::string, std::string> > volInfo = unmarshal<std::list<std::map<std::string, std::string> > >(marshalledVolInfoString);
                    db2TgtReadinessCheckInfo.m_tgtVolumeInfo = volInfo;
                }
                catch(std::exception& ex)
                {
                    DebugPrintf(SV_LOG_ERROR, "Error while unmarshalling volinfo : %s\n", ex.what()) ;
                    return SVS_FALSE ;
                }
                catch(...)
                {
                    DebugPrintf(SV_LOG_ERROR, "Error while unmarshalling volinfo\n") ;
                    return SVS_FALSE ;
                }
            }
            
            if( targetData.find( "volumeCapacities" ) !=  targetData.end() )
            {
                try
                {
                    std::string marshalledVolCapString = targetData.find("volumeCapacities")->second;

                    DebugPrintf( SV_LOG_DEBUG, "Capacities Info: '%s' \n", marshalledVolCapString.c_str()) ;
                    std::list<std::map<std::string, std::string> > capInfo = unmarshal<std::list<std::map<std::string, std::string> > >(marshalledVolCapString);
                    db2TgtReadinessCheckInfo.m_volCapacities= capInfo;
                }
                catch(std::exception& ex)
                {
                    DebugPrintf(SV_LOG_ERROR, "Error while unmarshalling capacities info : %s\n", ex.what()) ;
                    return SVS_FALSE ;
                }
                catch(...)
                {
                    DebugPrintf(SV_LOG_ERROR, "Error while unmarshalling capacities info\n") ;
                    return SVS_FALSE ;
                }
            }
        }
    }
    catch(std::exception& ex)
    {
        DebugPrintf(SV_LOG_ERROR, "Error while unmarshalling target readiness info: %s\n", ex.what()) ;
        return SVS_FALSE ;
    }
    catch(...)
    {
        DebugPrintf(SV_LOG_ERROR, "Error while unmarshalling target readiness info\n") ;
        return SVS_FALSE ;
    }

    SV_ULONGLONG instanceId = schedulerPtr->getInstanceId(m_policyId);
    AppAgentConfiguratorPtr configuratorPtr = AppAgentConfigurator::GetAppAgentConfigurator() ;
    configuratorPtr->PolicyUpdate(m_policyId,instanceId,"InProgress","");
	
    validations = enginePtr->getDb2ProtectionTargetRules(db2TgtReadinessCheckInfo, m_totalNumberOfPairs, isLocalContext);

    std::list<ValidatorPtr>::iterator validateIter = validations.begin() ;
    std::list<InmRuleIds> failedRules ;
    while(validateIter != validations.end() )
    {
        (*validateIter)->setFailedRules(failedRules) ;
        bRet = enginePtr->RunRule(*validateIter) ;
        if( (*validateIter)->getStatus() != INM_RULESTAT_PASSED )
        {
            failedRules.push_back((*validateIter)->getRuleId()) ;
        }
        ReadinessCheck readyCheck ;
        (*validateIter)->convertToRedinessCheckStruct(readyCheck);
        readinessChecksList.push_back(readyCheck) ;
        validateIter ++ ;
    }

    readinessCheckInfo.propertiesList.insert(std::make_pair("PolicyId", boost::lexical_cast<std::string>(m_policyId))) ;
    readinessCheckInfo.propertiesList.insert(std::make_pair("InstanceId",boost::lexical_cast<std::string>(instanceId)));
    readinessCheckInfo.validationsList = readinessChecksList ;

    discCtrlPtr->m_ReadinesscheckInfoList.push_back(readinessCheckInfo) ;

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}

SVERROR Db2DiscoveryAndValidator::UnregisterDB()
{
  SVERROR bRet = SVS_OK ;
  DebugPrintf(SV_LOG_DEBUG, "ENTERED Db2DiscoveryAndValidator::%s\n", FUNCTION_NAME) ;
  std::string marshalledUnregisterDBInfo = m_policy.m_policyData ;

  AppAgentConfiguratorPtr appConfiguratorPtr = AppAgentConfigurator::GetAppAgentConfigurator();
  AppSchedulerPtr schedulerPtr = AppScheduler::getInstance() ;
  SV_ULONG policyId = boost::lexical_cast<SV_ULONG>(m_policy.m_policyProperties.find("PolicyId")->second);
  DebugPrintf( SV_LOG_DEBUG, "Policy Id received : %d\n",policyId);
  SV_ULONGLONG instanceId = schedulerPtr->getInstanceId(policyId);
  std::string outputFileName = getOutputFileName(policyId);
  schedulerPtr->UpdateTaskStatus(policyId, TASK_STATE_STARTED) ;
  AppLocalConfigurator localconfigurator ;
  std::string installPath = localconfigurator.getInstallPath();
  appConfiguratorPtr->PolicyUpdate(policyId,instanceId,"InProgress","InProgress");    
  std::stringstream stream;
  try
  {
      DebugPrintf( SV_LOG_DEBUG, "Unregistered DB Info :%s\n",marshalledUnregisterDBInfo.c_str());
      UnregisterInfo unregInfo = unmarshal<UnregisterInfo>(marshalledUnregisterDBInfo);
      std::string db2appwizardfile = installPath + "/etc/db2appwizard.conf"; 
       
      DebugPrintf( SV_LOG_DEBUG, "Unregistered DBs are : \n");
      std::ofstream out;

      out.open(db2appwizardfile.c_str(), std::ios::app);
      if (!out)
      {
         DebugPrintf(SV_LOG_ERROR,"Failed to open %s file\n", db2appwizardfile.c_str());
         bRet = SVS_FALSE;
      }
      else
      {
          std::list<std::string>::iterator dbitr = unregInfo.m_InstancesList.begin();
          while(dbitr != unregInfo.m_InstancesList.end())
          {
            DebugPrintf( SV_LOG_DEBUG," %s \n",(*dbitr).c_str());
            std::string line = "DB2Database=NO:" + (*dbitr);
            out << line << std::endl;
            dbitr++;
          }
         out.close();
      }
  }
  catch ( ContextualException& ce )
  {
      DebugPrintf(SV_LOG_ERROR,"\n%s encountered exception %s.\n", FUNCTION_NAME, ce.what());
      DebugPrintf(SV_LOG_ERROR,"Got unmarshal exception for %s\n",marshalledUnregisterDBInfo.c_str());
  }
  catch(std::exception ex)
  {
      DebugPrintf(SV_LOG_ERROR, "Failed to unmarshal UnregisterApplication Data %s\n for the policy %ld", ex.what(),m_policyId) ;
      DebugPrintf(SV_LOG_ERROR,"Got unmarshal exception for %s\n",marshalledUnregisterDBInfo.c_str());
  }
  /*schedulerPtr->UpdateTaskStatus(policyId, TASK_STATE_COMPLETED) ;
  if(bRet == SVS_OK)
  {
      stream << "Db2 Database Unregistration is successful." << std::endl;
      WriteStringIntoFile(outputFileName, stream.str());

      appConfiguratorPtr->PolicyUpdate(policyId,instanceId,"Success",std::string("Unregistration of DB2 database is Completed"));
  }
  else
  {
      stream << "Db2 Database Unregistration Failed." << std::endl;
      WriteStringIntoFile(outputFileName, stream.str());
      appConfiguratorPtr->PolicyUpdate(policyId,instanceId,"Failed",std::string("Unregistration of DB2 database is not successful"));
  }*/
  if(bRet == SVS_OK)
  {
      stream << "Db2 Database Unregistration is successful." << std::endl;
      WriteStringIntoFile(outputFileName, stream.str());
      appConfiguratorPtr->UpdateUnregisterApplicationInfo(policyId,instanceId,"Success",std::string("Unregistration of DB2 database is Completed"));
  }
  else
  {
      stream << "Db2 Database Unregistration Failed." << std::endl;
      WriteStringIntoFile(outputFileName, stream.str());
      appConfiguratorPtr->UpdateUnregisterApplicationInfo(policyId,instanceId,"Failed",std::string("Unregistration of DB2 database is not successful"));
  }
  schedulerPtr->UpdateTaskStatus(policyId, TASK_STATE_COMPLETED) ;
  DebugPrintf(SV_LOG_DEBUG, "EXITED Db2DiscoveryAndValidator::%s\n", FUNCTION_NAME) ;
  return bRet ;
}
