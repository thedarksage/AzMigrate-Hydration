#include "Db2Controller.h"
#include "controller.h"
#include "Consistency/TagGenerator.h"
#include "host.h"
#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>
#include "appexception.h"
#include "appcommand.h"
#include "util.h"

Db2ControllerPtr Db2Controller::m_instancePtr;
Db2ControllerPtr Db2Controller::getInstance(ACE_Thread_Manager* tm)
{
    if( m_instancePtr.get() == NULL )
    {
		m_instancePtr.reset(new  Db2Controller(tm)) ;
    }
    return m_instancePtr ;
}

int Db2Controller::open(void *args)
{
    return this->activate(THR_BOUND);
}

int Db2Controller::close(u_long flags )
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;    
    //TODO:: controller specific code
    return 0 ;
}

Db2Controller::Db2Controller(ACE_Thread_Manager* thrMgr)
: AppController(thrMgr)
{
  //TODO
}

//It is pure virtual function in appcontroller.h, added for compilation purpose
void Db2Controller::PreClusterReconstruction()
{

}

int Db2Controller::svc()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    while(!Controller::getInstance()->QuitRequested(5))
    {
        ProcessRequests() ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    
    return  0 ;
}

SVERROR Db2Controller::Consistency()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    SVERROR result = SVS_FALSE;
    std::string strCommandLineOptions;
    std::string strAppType;
    AppAgentConfiguratorPtr appConfiguratorPtr = AppAgentConfigurator::GetAppAgentConfigurator();

    AppSchedulerPtr schedulerPtr = AppScheduler::getInstance() ;
    SV_ULONG policyId = boost::lexical_cast<SV_ULONG>(m_policy.m_policyProperties.find("PolicyId")->second);
    //TODO: Check whether dbname woll be sent in policyProperties or not.
    std::string dbName = m_policy.m_policyProperties.find("InstanceName")->second;
    std::string databaseName=dbName.substr(dbName.find_first_of(":")+1);
    std::stringstream dbNames;
    dbNames << databaseName;
    SV_ULONGLONG instanceId = schedulerPtr->getInstanceId(policyId);
    std::string outputFileName = getOutputFileName(policyId);
    schedulerPtr->UpdateTaskStatus(policyId, TASK_STATE_STARTED) ;
    std::string instance;
    std::string db;

    std::string marshalledConsistencyData = m_policy.m_policyData ;
    try
    {
        DebugPrintf(SV_LOG_DEBUG,"Consistency Data : %s \n",marshalledConsistencyData.c_str());
        ConsistencyData db2consistencyData = unmarshal<ConsistencyData>(marshalledConsistencyData);
        //instance = db2consistencyData.m_Instance;
        std::list<std::string>::iterator iter = db2consistencyData.m_InstancesList.begin();
        while(iter != db2consistencyData.m_InstancesList.end())
        {
            instance = (*iter).substr(0,(*iter).find_first_of(":"));
            if (dbName.compare(*iter) != 0)
            {
                //In the instance list received all the databases belong to same instance. As the filesystem is not shared among multiple instances.
                db = (*iter).substr((*iter).find_first_of(":")+1);

                dbNames << ":";  
                dbNames << db;  
            }
            iter++;
        }
        DebugPrintf(SV_LOG_DEBUG,"Instance : %s\n",instance.c_str());
        DebugPrintf(SV_LOG_DEBUG,"DBs : %s\n",(dbNames.str()).c_str());
    }
    catch ( ContextualException& ce )
    {
        DebugPrintf(SV_LOG_ERROR,
            "\n%s encountered exception %s.\n",
            FUNCTION_NAME, ce.what());
        DebugPrintf(SV_LOG_ERROR,"Got unmarshal exception for %s\n",marshalledConsistencyData.c_str());
    }
    catch(std::exception ex)
    {
        DebugPrintf(SV_LOG_ERROR, "Failed to unmarshal ConsistencyData %s\n for the policy %ld", ex.what(),m_policyId) ;
        DebugPrintf(SV_LOG_ERROR,"Got unmarshal exception for %s\n",marshalledConsistencyData.c_str());
    }
 

    std::map<std::string, std::string> propsMap = m_policy.m_policyProperties ;
    std::map<std::string, std::string>::iterator iter = propsMap.find("ScenarioId");
    if(iter != propsMap.end())
    {
        if(checkPairsInDiffsync(iter->second,outputFileName))
        {        
            iter = propsMap.find("ApplicationType");
            if(iter != propsMap.end())
            {
                strAppType = iter->second;
            }
            else
            {
                DebugPrintf(SV_LOG_ERROR, "there is no option with key as ApplicationType\n") ;
            }
            iter = propsMap.find("ConsistencyCmd");
            if(iter != propsMap.end())
            {
                strCommandLineOptions = iter->second;              
            }
            else
            {
                DebugPrintf(SV_LOG_ERROR, "there is no option with key as ConsistencyCmd\n") ;
            }

            iter = propsMap.find("PolicyId");
            if(iter != propsMap.end())
            {
                TagGenerator obj(strCommandLineOptions,300);
                SV_ULONG exitCode = 0x1;
                if(obj.issueConsistenyTagForDb2(outputFileName, instance,dbNames.str(), exitCode))
                {
                    if(exitCode == 0)
                    {
                        appConfiguratorPtr->PolicyUpdate(policyId, instanceId,"Success",obj.stdOut());
                        result = SVS_OK;
                    }
                    else
                    {
                        //Command Spawned but failed due to invalid parameters.
                        appConfiguratorPtr->PolicyUpdate(policyId, instanceId,"Failed",obj.stdOut());
                        result = SVS_OK;
                    }
                }
                else
                {
                    appConfiguratorPtr->PolicyUpdate(policyId,instanceId,"Failed",std::string("Failed to spawn vacp Process.Please check Command"));
                }
            }
            else
            {
                DebugPrintf(SV_LOG_ERROR, "\n Failed to fetch the PolicyId from policy details");
            }
        }
        else
        {
            appConfiguratorPtr->PolicyUpdate(policyId,instanceId,"Skipped","");
            DebugPrintf(SV_LOG_DEBUG,"Pairs not in diff sync\n");
        }
    }
    else
    {
        DebugPrintf(SV_LOG_ERROR, "there is no option with key as ScenraioId\n") ;
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    schedulerPtr->UpdateTaskStatus(policyId, TASK_STATE_COMPLETED) ;
    return result;
}

SVERROR Db2Controller::prepareTarget()
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	SVERROR result = SVS_OK;

    AppAgentConfiguratorPtr appConfiguratorPtr = AppAgentConfigurator::GetAppAgentConfigurator();
    AppSchedulerPtr schedulerPtr = AppScheduler::getInstance() ;
    SV_ULONG policyId = boost::lexical_cast<SV_ULONG>(m_policy.m_policyProperties.find("PolicyId")->second);
    SV_ULONGLONG instanceId = schedulerPtr->getInstanceId(policyId);
    std::string outputFileName = getOutputFileName(policyId);
    ACE_OS::unlink(outputFileName.c_str()) ;
    std::stringstream stream;
    schedulerPtr->UpdateTaskStatus(policyId,TASK_STATE_STARTED) ;
    appConfiguratorPtr->PolicyUpdate(policyId,instanceId,"InProgress", "InProgress");

    std::map<std::string, std::string> propsMap = m_policy.m_policyProperties ;

    std::string marshalledPrepareTargetString = m_policy.m_policyData ;

    DebugPrintf( SV_LOG_DEBUG, "Policy Data : '%s' \n", marshalledPrepareTargetString.c_str()) ;

    try
    {
        std::map<std::string, std::string> targetData = unmarshal<std::map<std::string, std::string> >(marshalledPrepareTargetString);

        stream << "Verifying if the target devices are in use by Db2 database.   " << std::endl ;

        if( targetData.find( "tgtDbInfo" ) !=  targetData.end() )
        {
            try
            {
                std::string marshalledDbInfoString = targetData.find("tgtDbInfo")->second;

                DebugPrintf( SV_LOG_DEBUG, "DB Info: '%s' \n", marshalledDbInfoString.c_str()) ;


                if (marshalledDbInfoString.size() > 0)
                {
                    std::list<std::map<std::string, std::string> > dbInfo = unmarshal<std::list<std::map<std::string, std::string> > >(marshalledDbInfoString);
                    std::list<std::map<std::string, std::string> >::iterator dbInfoIter = dbInfo.begin();
                    std::string dbConfFileNames = "";

                    while(dbInfoIter != dbInfo.end())
                    {
                        std::string dbInstanceName = dbInfoIter->find("InstanceName")->second;

                        std::string instanceName = dbInstanceName.substr(0,dbInstanceName.find_first_of(":"));
                        std::string dbName = dbInstanceName.substr(dbInstanceName.find_first_of(":")+1);
						//Check this ConfFile
                        std::string dbConfFileName = "/tmp/" + dbInfoIter->find("FileName")->second;
                        std::string config = dbInfoIter->find("FileContents")->second;

                        DebugPrintf( SV_LOG_DEBUG, "DB2 Instance Name: '%s' Database Name: '%s' Conf filename : '%s' \n", instanceName.c_str(),dbName.c_str(), dbConfFileName.c_str()) ;
     
                        std::ofstream out;
                        out.open(dbConfFileName.c_str(), std::ios::trunc);
                        if (!out)
                        {
                            DebugPrintf(SV_LOG_ERROR,"Failed to open %s file\n", dbConfFileName.c_str());
                            stream << "Failed to open config file " << dbConfFileName << std::endl ;
                            result = SVS_FALSE;
                            break;
                        }

                        out << config;
                        out.close();

                        if (dbConfFileNames.size() == 0)
                            dbConfFileNames.append(dbConfFileName);
                        else
                        {
                            dbConfFileNames.append(":");
                            dbConfFileNames.append(dbConfFileName);
                        }
 
                        dbInfoIter++;
                    }

                    if (dbConfFileNames.size() > 0 ) 
                    {
                        AppLocalConfigurator localconfigurator ;
                        SV_ULONG exitCode = 1 ;
                        std::string strCommnadToExecute ;
                        strCommnadToExecute += localconfigurator.getInstallPath();
                        strCommnadToExecute += "/scripts/db2appagent.sh ";
                        strCommnadToExecute += "preparetarget ";
                        strCommnadToExecute += dbConfFileNames;
                        strCommnadToExecute += std::string(" ");
                        DebugPrintf(SV_LOG_INFO,"\n The preparetarget command to execute : %s\n",strCommnadToExecute.c_str());

                        stream << strCommnadToExecute << std::endl;

                        AppCommand objAppCommand(strCommnadToExecute, 0, outputFileName);

                        std::string output;

                        if(objAppCommand.Run(exitCode, output, Controller::getInstance()->m_bActive) != SVS_OK)
                        {
                            DebugPrintf(SV_LOG_ERROR,"Failed to spawn db2 prepare target script %s.\n", strCommnadToExecute.c_str());                            
                            result = SVS_FALSE;
                        }
                        else
                        {
                            DebugPrintf(SV_LOG_INFO,"Successfully spawned the db2 prepare target script.\n");
                            DebugPrintf(SV_LOG_INFO,"output. %s\n", output.c_str());

                            stream << output << std::endl;

                            if (exitCode != 0)
                            {
                                DebugPrintf(SV_LOG_ERROR,"Failed db2 prepare target script %s.\n", strCommnadToExecute.c_str());
                                result = SVS_FALSE;
                            }
                        }
                    }
                }
            }
            catch(std::exception& ex)
            {
                DebugPrintf(SV_LOG_ERROR, "Error while unmarshalling dbInfo %s\n", ex.what()) ;                
                result = SVS_FALSE;
            }
            catch(...)
            {
                DebugPrintf(SV_LOG_ERROR, "Error while unmarshalling dbInfo\n") ;                
                result = SVS_FALSE;
            }
        }


        stream << "Verifying if the target devices are busy." << std::endl ;

        if ( (result != SVS_FALSE) &&  (targetData.find( "tgtVolInfo" ) !=  targetData.end() ) )
        {
            try
            {
                std::string marshalledVolInfoString = targetData.find("tgtVolInfo")->second;

                DebugPrintf( SV_LOG_DEBUG, "Volume Info: '%s' \n", marshalledVolInfoString.c_str()) ;

                if (marshalledVolInfoString.size() > 0)
                {
                    std::list<std::map<std::string, std::string> > volInfo = unmarshal<std::list<std::map<std::string, std::string> > >(marshalledVolInfoString);
                    std::list<std::map<std::string, std::string> >::iterator volInfoIter = volInfo.begin();

                    AppLocalConfigurator localconfigurator ;
                    // Db2PrepareTgtVolInfoFile.dat will be generated by appservice, check this once
                    std::string tgtVolInfoFile = localconfigurator.getInstallPath() + "/etc/Db2PrepareTgtVolInfoFile.dat";
                    std::ofstream out;

                    out.open(tgtVolInfoFile.c_str(), std::ios::trunc);
                    if (!out) 
                    {
                        DebugPrintf(SV_LOG_ERROR,"Failed to open %s file\n", tgtVolInfoFile.c_str());
                        result = SVS_FALSE;
                    }
                    else
                    {
                        while (volInfoIter != volInfo.end())
                        {
                            std::string deviceName = volInfoIter->find("DeviceName")->second;
                            std::string fsType = volInfoIter->find("FileSystemType")->second;
                            std::string volType = volInfoIter->find("VolumeType")->second;
                            std::string vendor = volInfoIter->find("VendorOrigin")->second;
                            std::string mountPoint = volInfoIter->find("MountPoint")->second;
                            std::string diskGroup = volInfoIter->find("DiskGroup")->second;
                            std::string shouldDestroy = volInfoIter->find("shouldDestroy")->second;

                            if (fsType.empty()) fsType = "0";
                            if (mountPoint.empty()) mountPoint = "0";
                            if (diskGroup.empty()) diskGroup = "0";

                            DebugPrintf( SV_LOG_DEBUG, "deviceName: '%s' fsType: '%s' volType: '%s' vendor: '%s' mountPoint: '%s' diskGroup: '%s' shouldDestroy : '%s'\n", deviceName.c_str(), fsType.c_str(), volType.c_str(), vendor.c_str(), mountPoint.c_str(), diskGroup.c_str(), shouldDestroy.c_str()) ;

                            if (fsType.empty()) fsType = "0";
                            if (mountPoint.empty()) mountPoint = "0";
                            if (diskGroup.empty()) diskGroup = "0";

                            DebugPrintf( SV_LOG_DEBUG, "deviceName: '%s' fsType: '%s' volType: '%s' vendor: '%s' mountPoint: '%s' diskGroup: '%s' shouldDestroy : '%s'\n", deviceName.c_str(), fsType.c_str(), volType.c_str(), vendor.c_str(), mountPoint.c_str(), diskGroup.c_str(), shouldDestroy.c_str()) ;

                            std::string line = "DeviceName=" + deviceName + ":";
                            line += "FileSystemType=" + fsType + ":";
                            line += "VolumeType=" + volType + ":";
                            line += "VendorOrigin=" + vendor + ":";
                            line += "MountPoint=" + mountPoint + ":";
                            line += "DiskGroup=" + diskGroup + ":";
                            line += "shouldDestroy=" + shouldDestroy + " ";

                            DebugPrintf( SV_LOG_DEBUG, "tgtVolInfo %s\n", line.c_str());

                            out << line << std::endl;

                            volInfoIter++;
                        }

                        out.close();

                        SV_ULONG exitCode = 1 ;
                        std::string strCommnadToExecute ;
                        strCommnadToExecute += localconfigurator.getInstallPath();
                        strCommnadToExecute += "/scripts/inmvalidator.sh ";
                        strCommnadToExecute += "preparetarget ";
                        strCommnadToExecute += tgtVolInfoFile;
                        strCommnadToExecute += std::string(" ");

                        DebugPrintf(SV_LOG_INFO,"\n The preparetarget command to execute : %s\n",strCommnadToExecute.c_str());

                        stream << strCommnadToExecute << std::endl;

                        AppCommand objAppCommand(strCommnadToExecute, 0, outputFileName);

                        std::string output;

                        if(objAppCommand.Run(exitCode, output, Controller::getInstance()->m_bActive ) != SVS_OK)
                        {
                            DebugPrintf(SV_LOG_ERROR,"Failed to spawn db2 prepare target script %s.\n", strCommnadToExecute.c_str());
                            result = SVS_FALSE;
                        }
                        else
                        {
                            DebugPrintf(SV_LOG_INFO,"Successfully spawned the db2 prepare target script.\n");
                            DebugPrintf(SV_LOG_INFO,"output. %s\n", output.c_str());

                            stream << output << std::endl;

                            if (exitCode != 0)
                            {
                                DebugPrintf(SV_LOG_ERROR,"Failed db2 prepare target script %s.\n", strCommnadToExecute.c_str());
                                result = SVS_FALSE;
                            }
                        }
                    }
                }
                else
                {
                    DebugPrintf(SV_LOG_ERROR,"No target volume info received\n");
                    result = SVS_FALSE;
                }
            }
            catch(std::exception& ex)
            {
                DebugPrintf(SV_LOG_ERROR, "Error while unmarshalling volInfo %s\n", ex.what()) ;             
                result = SVS_FALSE;
            }
            catch(...)
            {
                DebugPrintf(SV_LOG_ERROR, "Error while unmarshalling volInfo\n") ;               
                result = SVS_FALSE;
            }
        }
    }
    catch(std::exception& ex)
    {
        DebugPrintf(SV_LOG_ERROR, "Error while unmarshalling prepare target%s\n", ex.what()) ;        
        result = SVS_FALSE;
    }
    catch(...)
    {
        DebugPrintf(SV_LOG_ERROR, "Error while unmarshalling prepare target\n") ;        
        result = SVS_FALSE;
    }

    if (result == SVS_FALSE)
    {
        stream << "Db2 Application PrepareTarget failed." << std::endl;

        WriteStringIntoFile(outputFileName, stream.str());

        appConfiguratorPtr->PolicyUpdate(policyId,instanceId,"Failed",std::string("Prepare target failed."));
    }
    else
    {
        stream << "Db2 Application PrepareTarget succeeded." << std::endl;

        WriteStringIntoFile(outputFileName, stream.str());

        appConfiguratorPtr->PolicyUpdate(policyId,instanceId,"Success",std::string("Db2 Application PrepareTarget Success."));
    }

    schedulerPtr->UpdateTaskStatus(policyId, TASK_STATE_COMPLETED);

	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return result;
}

bool Db2Controller::StartAppServices()
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	bool bResult = false;
	return bResult;
}

bool Db2Controller::IsItActiveNode()
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	bool bResult = false;
	return bResult;
}

void Db2Controller::MakeAppServicesAutoStart()
{
    
}

SVERROR Db2Controller::GetSourceDiscoveryInformation()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    SVERROR bRet = SVS_FALSE;
    std::map<std::string, std::string>& propsMap = m_policy.m_policyProperties ;

    std::string marshalledSrcDiscInfoStr = m_AppProtectionSettings.srcDiscoveryInformation;
    try
    {
        if(m_FailoverInfo.m_AppType.compare("DB2") == 0)
        {
            m_srcDiscInfo = unmarshal<SrcDb2DiscoveryInformation>(marshalledSrcDiscInfoStr) ;
        }
        bRet = SVS_OK;
    }   
    catch ( ContextualException& ce )
    {
        DebugPrintf(SV_LOG_ERROR, 
                "\n%s encountered exception %s.\n",
                FUNCTION_NAME, ce.what());
        DebugPrintf(SV_LOG_ERROR,"Got unmarshal exception for %s\n",marshalledSrcDiscInfoStr.c_str());       
    }
    catch(std::exception ex)
    {
        DebugPrintf(SV_LOG_ERROR, "Failed to unmarshal srcDiscoveryInformation %s\n for the policy %ld", ex.what(),m_policyId) ;
        DebugPrintf(SV_LOG_ERROR,"Got unmarshal exception for %s\n",marshalledSrcDiscInfoStr.c_str());       
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet;
}

void Db2Controller::UpdateConfigFiles()
{

    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    std::ofstream out;
    AppLocalConfigurator localconfigurator ;
    std::string installPath = localconfigurator.getInstallPath();

    if(m_FailoverInfo.m_PolicyType.compare("DRServerPlannedFailoverRollbackVolumes") == 0 ||
              m_FailoverInfo.m_PolicyType.compare("DRServerPlannedFailoverSetupApplicationEnvironment") == 0 ||
              m_FailoverInfo.m_PolicyType.compare("DRServerPlannedFailoverStartApplication") == 0 ||
              m_FailoverInfo.m_PolicyType.compare("DRServerUnPlannedFailoverRollbackVolumes") == 0 ||
              m_FailoverInfo.m_PolicyType.compare("DRServerUnPlannedFailoverSetupApplicationEnvironment") == 0 ||
              m_FailoverInfo.m_PolicyType.compare("DRServerUnPlannedFailoverStartApplication") == 0 ||
              m_FailoverInfo.m_PolicyType.compare("ProductionServerPlannedFailbackRollbackVolumes") == 0 ||
              m_FailoverInfo.m_PolicyType.compare("ProductionServerPlannedFailbackSetupApplicationEnvironment") == 0 ||
              m_FailoverInfo.m_PolicyType.compare("ProductionServerPlannedFailbackStartApplication") == 0 ||
              m_FailoverInfo.m_PolicyType.compare("ProductionServerFastFailbackSetupApplicationEnvironment") == 0 ||
              m_FailoverInfo.m_PolicyType.compare("ProductionServerFastFailbackStartApplication") == 0)
    {
         srcDbConfFilePath.clear();         

         std::map<std::string, std::string> failoverFiles = m_FailoverData.m_RepointInfoMap;

        if(!failoverFiles.empty())
        {
            std::map<std::string, std::string>::const_iterator failoverFilesIter = failoverFiles.begin();
            while(failoverFilesIter != failoverFiles.end())
            {
                DebugPrintf(SV_LOG_DEBUG,"Key value %s \n",((failoverFilesIter)->first).c_str());
                std::string dbname;
                if("PairInfo" != ((failoverFilesIter)->first))
                {
                  std::string filename = (failoverFilesIter)->first;
                  std::string fileExtension = filename.substr(filename.find_last_of(".")+1);
                  DebugPrintf(SV_LOG_DEBUG,"fileExtension %s \n",fileExtension.c_str());
                  if("conf" == fileExtension)
                  {
                    std::string db2FailoverDir = installPath + "/db2_failover_data/";
                    if(opendir(db2FailoverDir.c_str())== NULL)
                    {
                      DebugPrintf(SV_LOG_DEBUG,"Creating the directory : %s\n",db2FailoverDir.c_str());
                      if(mkdir(db2FailoverDir.c_str(),S_IRWXG|S_IRWXO|S_IRWXU)!=0)
                      {
                          DebugPrintf(SV_LOG_ERROR,"Failed to create the directory :%s\n",db2FailoverDir.c_str());
                          return;
                      }
                    }
                    else
                    {
                        DebugPrintf(SV_LOG_DEBUG,"Directory %s already exists",db2FailoverDir.c_str());
                    }
                    std::string srcConfigFile = db2FailoverDir + (failoverFilesIter)->first;
                    DebugPrintf(SV_LOG_DEBUG,"srcConfigFile --  %s \n",srcConfigFile.c_str());
                    dbname = filename.substr(0,filename.find_last_of("."));
                    DebugPrintf(SV_LOG_DEBUG,"DBName --  %s \n",dbname.c_str());
                    srcDbInstance = dbname.substr(dbname.find_first_of("_")+1);
                    DebugPrintf(SV_LOG_DEBUG,"srcDbInstance --  %s \n",srcDbInstance.c_str());
                    std::ofstream out;
                    out.open(srcConfigFile.c_str(), std::ios::trunc);
                    if (!out) 
                    {
                        DebugPrintf(SV_LOG_ERROR,"Failed to open %s file\n", srcConfigFile.c_str());
                        DebugPrintf(SV_LOG_ERROR,"Got error  %s \n", strerror(errno));
                        return;
                    }
                    out << (failoverFilesIter)->second;
                    out.close();

                    srcDbConfFilePath = srcConfigFile;                 

                  }				
                }
                else if("PairInfo" == ((failoverFilesIter)->first))
                {
                   DebugPrintf(SV_LOG_DEBUG,"PairInfo received %s \n",((failoverFilesIter)->second).c_str());
                   std::string marshalledpairInfo = (failoverFilesIter)->second;
                   try
                   {
                     std::map<std::string,std::map<std::string,std::string> > pairInfo = unmarshal<std::map<std::string,std::map<std::string,std::string> > >(marshalledpairInfo);
                     std::map<std::string,std::map<std::string,std::string> >::const_iterator pairInfoIter = pairInfo.begin();

                     //std::string dbname = (m_FailoverData.m_InstancesList).front();
                     //std::string db = dbname.substr(dbname.find_first_of("_")+1);
                     DebugPrintf(SV_LOG_DEBUG,"db2 database protected is %s\n",(srcDbInstance).c_str());
                     //std::string instance = dbname.substr(0,dbname.find_first_of("."));
                     //std::string db = dbname.substr(dbname.find_first_of(".")+1);

                     //std::string outputFile = installPath + "/etc/" + instance + "." + db + ".PairInfo";
                     std::string outputFile = installPath + "/etc/" + srcDbInstance + ".PairInfo";

                     std::ofstream out;

                     out.open(outputFile.c_str(), std::ios::trunc);

                    if (!out)
                    {
                       DebugPrintf(SV_LOG_ERROR,"Failed to open %s file\n", outputFile.c_str());
                    }
                    else
                    {
                       std::string line = "<replication_pair>\n";
                       while(pairInfoIter != pairInfo.end())
                       {
                          line += "<TargetDevice>\n";
                          DebugPrintf(SV_LOG_DEBUG,"Targetdevicename %s \n",(pairInfoIter->first).c_str());
                                                                                                                                                                                                  line += " <target_devicename>" + (pairInfoIter->first) + "</target_devicename>" + "\n";
                                                                                                                                                                                                  std::map<std::string,std::string> srcDeviceInfo = pairInfoIter->second;
                                                                                                                                                                                                  std::map<std::string,std::string>::const_iterator srcDeviceInfoIter = srcDeviceInfo.begin();
                                                                                                                                                                                                  while(srcDeviceInfoIter != srcDeviceInfo.end())
                          {
                             DebugPrintf(SV_LOG_DEBUG,"SourceHostdId %s \n",(srcDeviceInfoIter->first).c_str());
                                                                                                                                                                                                     DebugPrintf(SV_LOG_DEBUG,"Sourcedevicenames %s \n",(srcDeviceInfoIter->second).c_str());
                                                                                                                                                                                                     line += " <source_hostid>" + (srcDeviceInfoIter->first) + "</source_hostid>" + "\n";
                                                                                                                                                                                                     line += " <source_devicename>" + (srcDeviceInfoIter->second) + "</source_devicename>" + "\n";
                                                                                                                                                                                                     srcDeviceInfoIter++;
                          }
                          pairInfoIter++;
                          line += "</TargetDevice>\n";
                         }
                         line += "</replication_pair>\n";
                         out << line << std::endl;
                         out.close();
                       }
                     }
                     catch ( ContextualException& ce )
                     {
                        DebugPrintf(SV_LOG_ERROR,"\n%s encountered exception %s.\n",FUNCTION_NAME, ce.what());
                        DebugPrintf(SV_LOG_ERROR,"Got unmarshal exception for %s\n",marshalledpairInfo.c_str());
                        return;
                     }
                    catch(std::exception ex)
                    {
                        DebugPrintf(SV_LOG_ERROR, "Failed to unmarshal PairInfo %s\n for the policy %ld", ex.what(),m_policyId) ;
                        DebugPrintf(SV_LOG_ERROR,"Got unmarshal exception for %s\n",marshalledpairInfo.c_str());
                        return;
                    }
                }
                failoverFilesIter++;
            }

            if (srcDbConfFilePath.empty())
            {
                DebugPrintf(SV_LOG_ERROR, "UpdateConfigFiles: no failover files for %s\n id %ld\n", m_FailoverInfo.m_PolicyType.c_str(),m_policyId) ;
            }
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR, "UpdateConfigFiles: no Failover files for %s id %ld\n", m_FailoverInfo.m_PolicyType.c_str(),m_policyId) ;
        }
    }
    else
    {
        DebugPrintf(SV_LOG_DEBUG, "UpdateConfigFiles: Received unsupported policy %s for the policy %ld\n", m_FailoverInfo.m_PolicyType.c_str(),m_policyId) ;
    }
 
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}

void Db2Controller::BuildFailoverCommand(std::string& failoverCmd)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    AppLocalConfigurator localconfigurator ;
    std::stringstream cmdStream;
    std::stringstream dbNames;

    const std::string& policyType = m_FailoverInfo.m_PolicyType;

    std::string installPath = localconfigurator.getInstallPath();
    DebugPrintf(SV_LOG_DEBUG, "Policy Type recieved : %s\n",policyType.c_str()) ;
    DebugPrintf(SV_LOG_DEBUG, "installPath %s\n", installPath.c_str()) ;
	
	//Check: Why for these two policy types db2_consistency.sh failover is used?
	if(policyType.compare("ProductionServerPlannedFailover") == 0 ||
            policyType.compare("DRServerPlannedFailback") == 0)
    {
		cmdStream << installPath;
        cmdStream << "/scripts/db2_consistency.sh";
        cmdStream << " failover ";

        //cmdStream << m_FailoverData.m_Instance;
        //cmdStream << " ";
        std::string instance;
        std::string dbname;
 
        std::list<std::string>::iterator iter = m_FailoverData.m_InstancesList.begin();
        while(iter != m_FailoverData.m_InstancesList.end())
        {
          instance = (*iter).substr(0,(*iter).find_first_of(":"));
          dbname = (*iter).substr((*iter).find_first_of(":")+1);

          if (iter != m_FailoverData.m_InstancesList.begin())
           dbNames << ":";
          dbNames << dbname;

         iter++;
       }

        cmdStream << instance;
        cmdStream << " ";
        
        cmdStream << dbNames.str();
        cmdStream << " ";

        cmdStream << " ";
        cmdStream << m_FailoverInfo.m_TagName;

        std::list<std::string> tgtVolumeList = m_AppProtectionSettings.appVolumes;
        std::list<std::string>::iterator volumeListIter = tgtVolumeList.begin();
        if (!tgtVolumeList.empty())
        {
            std::string volumes;
            volumes.clear();
            while(volumeListIter != tgtVolumeList.end())
            {
                volumes += (*volumeListIter) +",";
                volumeListIter++;
            }
            volumes.resize(volumes.size() -1);
            cmdStream << " ";
            cmdStream << volumes;
        }
        else
        {
            DebugPrintf(SV_LOG_DEBUG, "BuildFailoverCommand: no appVolumes");
        }
        failoverCmd = cmdStream.str();
    }
    else if(policyType.compare("DRServerPlannedFailoverRollbackVolumes") == 0 ||
        policyType.compare("DRServerPlannedFailoverSetupApplicationEnvironment") == 0 ||
        policyType.compare("DRServerPlannedFailoverStartApplication") == 0 ||
        policyType.compare("DRServerUnPlannedFailoverRollbackVolumes") == 0 ||
        policyType.compare("DRServerUnPlannedFailoverSetupApplicationEnvironment") == 0 ||
        policyType.compare("DRServerUnPlannedFailoverStartApplication") == 0 ||
        policyType.compare("ProductionServerPlannedFailbackRollbackVolumes") == 0 ||
        policyType.compare("ProductionServerPlannedFailbackSetupApplicationEnvironment") == 0 ||
        policyType.compare("ProductionServerPlannedFailbackStartApplication") == 0 ||
        policyType.compare("ProductionServerFastFailbackSetupApplicationEnvironment") == 0 ||
        policyType.compare("ProductionServerFastFailbackStartApplication") == 0 ||
		policyType.compare("ProductionServerPlannedFailover") == 0 ||
        policyType.compare("DRServerPlannedFailback") == 0)
    {
        
        cmdStream << installPath;
        cmdStream << "/scripts/db2failover.sh";
        cmdStream << " -a ";

        std::string volumes;

        if (policyType.compare("DRServerPlannedFailoverRollbackVolumes") == 0 ||
            policyType.compare("DRServerUnPlannedFailoverRollbackVolumes") == 0 ||
            policyType.compare("ProductionServerPlannedFailbackRollbackVolumes") == 0)
        {
            cmdStream << " rollback ";

            std::list<std::string> tgtVolumeList = m_AppProtectionSettings.appVolumes;
            std::list<std::string>::iterator volumeListIter = tgtVolumeList.begin();
            volumes.clear();
            if (!tgtVolumeList.empty())
            {
                while(volumeListIter != tgtVolumeList.end())
                {
                    volumes += (*volumeListIter) +",";
                    volumeListIter++;
                }
                volumes.resize(volumes.size() -1);
            }
            else
            {
                DebugPrintf(SV_LOG_DEBUG, "BuildFailoverCommand: no appVolumes");
            }
        }
        else if (policyType.compare("DRServerPlannedFailoverSetupApplicationEnvironment") == 0 ||
                   policyType.compare("DRServerUnPlannedFailoverSetupApplicationEnvironment") == 0 ||
                   policyType.compare("ProductionServerPlannedFailbackSetupApplicationEnvironment") == 0 ||
                   policyType.compare("ProductionServerFastFailbackSetupApplicationEnvironment") == 0)
        {
            cmdStream << " dgstartup ";

            std::list<std::string> tgtVolumeList = m_AppProtectionSettings.appVolumes;
            std::list<std::string>::iterator volumeListIter = tgtVolumeList.begin();
            volumes.clear();
            if (!tgtVolumeList.empty())
            {
                while(volumeListIter != tgtVolumeList.end())
                {
                    volumes += (*volumeListIter) +",";
                    volumeListIter++;
                }
                volumes.resize(volumes.size() -1);
            }
            else
            {
                DebugPrintf(SV_LOG_DEBUG, "BuildFailoverCommand: no appVolumes");
            }
        }
        else if (policyType.compare("DRServerPlannedFailoverStartApplication") == 0 ||
                   policyType.compare("DRServerUnPlannedFailoverStartApplication") == 0 ||
                   policyType.compare("ProductionServerPlannedFailbackStartApplication") == 0 ||
                   policyType.compare("ProductionServerFastFailbackStartApplication") == 0)
        {
            cmdStream << " databasestartup ";
            volumes = " none ";
        }

        cmdStream << " -t ";

        if (policyType.compare("DRServerPlannedFailoverRollbackVolumes") == 0 ||
            policyType.compare("DRServerUnPlannedFailoverRollbackVolumes") == 0 ||
            policyType.compare("ProductionServerPlannedFailbackRollbackVolumes") == 0)
        {
            if (policyType.compare("DRServerUnPlannedFailoverRollbackVolumes") == 0 )
            {
                if(m_FailoverInfo.m_RecoveryPointType.compare("CUSTOM") == 0 )
                {
                    cmdStream <<  "CUSTOM " ;
                    cmdStream <<  " -n " ;
                    cmdStream <<  "\"" ;

                    RecoveryPoints recoveryPoints;
                    std::string volumeName;
                    std::string recoveryPointType;
                    std::string tagName;
                    std::string timeStamp;
                    std::string tagType;

                    std::map<std::string, RecoveryPoints>::iterator RecoverypointMapIter = m_FailoverData.m_recoveryPoints.begin();
                    while(RecoverypointMapIter != m_FailoverData.m_recoveryPoints.end())
                    {
                        recoveryPoints = RecoverypointMapIter->second;
                        volumeName = RecoverypointMapIter->first;

                        GetValueFromMap(recoveryPoints.m_properties,"RecoveryPointType",recoveryPointType);
                        GetValueFromMap(recoveryPoints.m_properties,"TagName",tagName);

                        if(!tagName.empty())
                        {   
                            GetValueFromMap(recoveryPoints.m_properties,"TagType",tagType);
                        }

                        GetValueFromMap(recoveryPoints.m_properties,"Timestamp",timeStamp); 

                        if (recoveryPointType.compare("TAG") == 0 )
                        {
                            cmdStream << volumeName + ":" + recoveryPointType + ":" + tagName + ":" + tagType + ",";
                        }
                        else if (recoveryPointType.compare("TIME") == 0 )
                        {
                            cmdStream << volumeName + ":" +  recoveryPointType + ":" + timeStamp + ",";
                        }

                        RecoverypointMapIter++;
                    }

                    cmdStream <<  "\"" ;

                }
                else if(m_FailoverInfo.m_RecoveryPointType.compare("LCCP") == 0)
                {
                    std::string tagName;
                    cmdStream <<  "LATEST " ;                   
                    cmdStream <<  " -n " ;
                    cmdStream <<  " none " ;                   
                }
                else if(m_FailoverInfo.m_RecoveryPointType.compare("LCT") == 0)
                {
                    std::string timeStamp;
                    cmdStream <<  "LATESTTIME " ;                   
                    cmdStream <<  " -n " ;
                    cmdStream <<  " none " ;                  
                }
            }
            else
            {
                cmdStream <<  " TAG " ;
                cmdStream <<  " -n " ;
                cmdStream << m_FailoverInfo.m_TagName;
            }
        }
        else
        {
            cmdStream <<  " none " ;
            cmdStream <<  " -n " ;
            cmdStream << " none ";
        }

        cmdStream << " -v ";

        if (policyType.compare("DRServerPlannedFailoverRollbackVolumes") == 0 ||
            policyType.compare("DRServerUnPlannedFailoverRollbackVolumes") == 0 ||
            policyType.compare("ProductionServerPlannedFailbackRollbackVolumes") == 0 ||
            policyType.compare("DRServerPlannedFailoverSetupApplicationEnvironment") == 0 ||
            policyType.compare("DRServerUnPlannedFailoverSetupApplicationEnvironment") == 0 ||
            policyType.compare("ProductionServerPlannedFailbackSetupApplicationEnvironment") == 0 ||
            policyType.compare("ProductionServerFastFailbackSetupApplicationEnvironment") == 0)
        {
            std::list<std::string> tgtVolumeList = m_AppProtectionSettings.appVolumes;
            std::list<std::string>::iterator volumeListIter = tgtVolumeList.begin();
            std::string volumes;
            volumes.clear();
            if (!tgtVolumeList.empty())
            {
                while(volumeListIter != tgtVolumeList.end())
                {
                    volumes += (*volumeListIter) +",";
                    volumeListIter++;
                }
                volumes.resize(volumes.size() -1);
            }
            else
            {
                DebugPrintf(SV_LOG_DEBUG, "BuildFailoverCommand: no appVolumes");
            }
            cmdStream << volumes;
        }
        else
        {
            cmdStream << " none ";
        }       

        cmdStream << " -s ";

        /*std::string protectedInst=m_FailoverData.m_InstancesList.front();
        DebugPrintf(SV_LOG_DEBUG, "Protected Database Instance is : %s\n",protectedInst.c_str());
        std::string proinst=protectedInst.substr(0,protectedInst.find_first_of(":"));
        std::string prodb=protectedInst.substr(protectedInst.find_first_of(":")+1);
        DebugPrintf(SV_LOG_DEBUG, "Protected Instance : %s\n",proinst.c_str());
        DebugPrintf(SV_LOG_DEBUG, "Protected Database : %s\n",prodb.c_str());*/
        
        cmdStream << srcDbConfFilePath;

        failoverCmd = cmdStream.str();
    }
    else
    {
        DebugPrintf(SV_LOG_ERROR, "BuildFailoverCommand: Received unsupported policy %s", policyType.c_str()) ;
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}

void Db2Controller::PrePareFailoverJobs()
{

    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    
    FailoverJob failoverJob;
    FailoverCommand failoverCmd;
    
    DebugPrintf(SV_LOG_DEBUG, "Db2Controller::PrePareFailoverJobs %s\n", m_FailoverInfo.m_PolicyType.c_str()) ;

    if (m_FailoverInfo.m_PolicyType.compare("DRServerPlannedFailoverRollbackVolumes") == 0 ||
        m_FailoverInfo.m_PolicyType.compare("DRServerPlannedFailoverSetupApplicationEnvironment") == 0 ||
        m_FailoverInfo.m_PolicyType.compare("DRServerPlannedFailoverStartApplication") == 0 ||
        m_FailoverInfo.m_PolicyType.compare("DRServerUnPlannedFailoverRollbackVolumes") == 0 ||
        m_FailoverInfo.m_PolicyType.compare("DRServerUnPlannedFailoverSetupApplicationEnvironment") == 0 ||
        m_FailoverInfo.m_PolicyType.compare("DRServerUnPlannedFailoverStartApplication") == 0 ||
        m_FailoverInfo.m_PolicyType.compare("ProductionServerPlannedFailbackRollbackVolumes") == 0 ||
        m_FailoverInfo.m_PolicyType.compare("ProductionServerPlannedFailbackSetupApplicationEnvironment") == 0 ||
        m_FailoverInfo.m_PolicyType.compare("ProductionServerPlannedFailbackStartApplication") == 0 ||
        m_FailoverInfo.m_PolicyType.compare("ProductionServerFastFailbackSetupApplicationEnvironment") == 0 ||
        m_FailoverInfo.m_PolicyType.compare("ProductionServerFastFailbackStartApplication") == 0)
    {

        if(m_FailoverInfo.m_RecoveryExecutor.compare("Custom") == 0)
        {
            if(m_FailoverInfo.m_Customscript.empty() == false)
            {
                failoverCmd.m_Command = m_FailoverInfo.m_Customscript;
                failoverCmd.m_GroupId = CUSTOM_TARGET_SCRIPT;
                failoverCmd.m_StepId = 1;
                failoverJob.m_FailoverCommands.push_back(failoverCmd);
            }
        }
        else
        {
            if(m_FailoverInfo.m_Prescript.empty() == false)
            {
                failoverCmd.m_Command = m_FailoverInfo.m_Prescript;
                failoverCmd.m_GroupId = PRESCRIPT_TARGET;
                failoverCmd.m_StepId = 1;
                failoverJob.m_FailoverCommands.push_back(failoverCmd);
            }
            std::string failoverCmdStr;
            BuildFailoverCommand(failoverCmdStr);
            failoverCmd.m_Command = failoverCmdStr;

            if (m_FailoverInfo.m_PolicyType.compare("DRServerPlannedFailoverRollbackVolumes") == 0 ||
                m_FailoverInfo.m_PolicyType.compare("DRServerUnPlannedFailoverRollbackVolumes") == 0 ||
                m_FailoverInfo.m_PolicyType.compare("ProductionServerPlannedFailbackRollbackVolumes") == 0)
            {

                failoverCmd.m_GroupId = ROLLBACK_VOLUMES;
                failoverCmd.m_StepId = 1;
            }
            else if (m_FailoverInfo.m_PolicyType.compare("DRServerPlannedFailoverSetupApplicationEnvironment") == 0 ||
                 m_FailoverInfo.m_PolicyType.compare("DRServerUnPlannedFailoverSetupApplicationEnvironment") == 0 ||
                 m_FailoverInfo.m_PolicyType.compare("ProductionServerPlannedFailbackSetupApplicationEnvironment") == 0 ||
                 m_FailoverInfo.m_PolicyType.compare("ProductionServerFastFailbackSetupApplicationEnvironment") == 0)
            {
                failoverCmd.m_GroupId = SETUP_APPLICATION_ENVIRONMENT;
                failoverCmd.m_StepId = 1;
            }
            else if (m_FailoverInfo.m_PolicyType.compare("DRServerPlannedFailoverStartApplication") == 0 ||
                 m_FailoverInfo.m_PolicyType.compare("DRServerUnPlannedFailoverStartApplication") == 0 ||
                 m_FailoverInfo.m_PolicyType.compare("ProductionServerPlannedFailbackStartApplication") == 0 ||
                 m_FailoverInfo.m_PolicyType.compare("ProductionServerFastFailbackStartApplication") == 0)
            {
                failoverCmd.m_GroupId = START_APPLICATION;
                failoverCmd.m_StepId = 1;
            }

            failoverJob.m_FailoverCommands.push_back(failoverCmd);
            if(m_FailoverInfo.m_Postscript.empty() == false)
            {
                failoverCmd.m_Command = m_FailoverInfo.m_Postscript;
                failoverCmd.m_GroupId = POSTSCRIPT_TARGET;
                failoverCmd.m_StepId = 1;
                failoverJob.m_FailoverCommands.push_back(failoverCmd);
            }

        }
    }
    else if (m_FailoverInfo.m_PolicyType.compare("ProductionServerPlannedFailover") == 0 ||
            m_FailoverInfo.m_PolicyType.compare("DRServerPlannedFailback") == 0)
    {
        if(m_FailoverInfo.m_RecoveryExecutor.compare("Custom") == 0)
        {
            if(m_FailoverInfo.m_Customscript.empty() == false)
            {
                failoverCmd.m_Command = m_FailoverInfo.m_Customscript;
                failoverCmd.m_GroupId = CUSTOM_SOURCE_SCRIPT;
                failoverCmd.m_StepId = 1;
                failoverJob.m_FailoverCommands.push_back(failoverCmd);
            }
        }
        else
        {
            if(m_FailoverInfo.m_Prescript.empty() == false)
            {
                failoverCmd.m_Command = m_FailoverInfo.m_Prescript;
                failoverCmd.m_GroupId = PRESCRIPT_SOURCE;
                failoverCmd.m_StepId = 1;
                failoverJob.m_FailoverCommands.push_back(failoverCmd);
            }
                
            std::string command;
            BuildFailoverCommand(command);
            failoverCmd.m_Command = command;
            failoverCmd.m_GroupId = ISSUE_CONSISTENCY;
            failoverCmd.m_StepId = 1;
            failoverJob.m_FailoverCommands.push_back(failoverCmd);
            command.clear();

            if(m_FailoverInfo.m_Postscript.empty() == false)
            {
                failoverCmd.m_Command = m_FailoverInfo.m_Postscript;
                failoverCmd.m_GroupId = POSTSCRIPT_SOURCE;
                failoverCmd.m_StepId = 1;
                failoverJob.m_FailoverCommands.push_back(failoverCmd);
            }
        }
    }
    else
    {
        DebugPrintf(SV_LOG_ERROR, "PrePareFailoverJobs: Received unsupported policy %s.", m_FailoverInfo.m_PolicyType.c_str()) ;
    }

    DebugPrintf(SV_LOG_DEBUG, "Db2Controller::PrePareFailoverJobs Failover command %s\n", failoverCmd.m_Command.c_str()) ;

    AddFailoverJob(m_policyId,failoverJob);

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return;
}


SVERROR Db2Controller::stopDb2Services(const std::string &appName)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	SVERROR result = SVS_FALSE;
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return result;

}

SVERROR Db2Controller::Process()
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED Db2Controller::%s\n", FUNCTION_NAME) ;
	SVERROR result = SVS_FALSE;
    bool process = false ;
	std::map<std::string, std::string> propsMap = m_policy.m_policyProperties ;
	std::map<std::string, std::string>::iterator iter = propsMap.find("ApplicationType");
	std::string strApplicationType;
	if(iter != propsMap.end())
	{
		strApplicationType = iter->second;
	}
	else
	{
		DebugPrintf(SV_LOG_ERROR, "there is no option with key as ApplicationType\n") ;
	}

    if(propsMap.find("PolicyType")->second.compare("Consistency") == 0)
    {
        DebugPrintf(SV_LOG_DEBUG, " Got Policy Type as Consistency \n");

        if(Consistency() == SVS_OK)
        {
            result = SVS_OK;
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR, "Unable to issue bookmark..\n") ;
        }
    }
    else if(propsMap.find("PolicyType")->second.compare("PrepareTarget") == 0)
    {
        DebugPrintf(SV_LOG_DEBUG, " Got Policy Type as PrepareTarget\n");

        if(prepareTarget() == SVS_OK)
        {
            result = SVS_OK ;
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR, "Failed to prepare target\n") ;
        }
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED Db2Controller::%s\n", FUNCTION_NAME) ;
	return result;
}

void Db2Controller::ProcessMsg(SV_ULONG policyId)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED Db2Controller::%s\n", FUNCTION_NAME) ;
	AppAgentConfiguratorPtr configuratorPtr = AppAgentConfigurator::GetAppAgentConfigurator() ;
    AppSchedulerPtr schedulerPtr = AppScheduler::getInstance() ;
    
    if(schedulerPtr->isPolicyEnabled(m_policyId))
    {
        SV_ULONGLONG instanceId = schedulerPtr->getInstanceId(m_policyId);
        configuratorPtr->PolicyUpdate(m_policyId,instanceId,"InProgress","");
    }
    
	//TODO: In AppAgentConfiguratorPtr, need to check for DB2 Application
	//m_policy should contain application type "DB2", no need to add anything in appconfigurator.cpp
    if( configuratorPtr->getApplicationPolicies(policyId, "DB2",m_policy) )
    {
        Process() ;
    }
    else
    {
        DebugPrintf(SV_LOG_INFO, "There are no policies available for policyId %ld\n", policyId) ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED Db2Controller::%s\n", FUNCTION_NAME) ;
}

