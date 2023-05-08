#include "util/common/util.h"
#include "Db2validators.h"
#include "config/applocalconfigurator.h"
#include "appcommand.h"
#include "controller.h"
#include <sstream>

Db2DatabaseRunValidator::Db2DatabaseRunValidator(const std::string& name, const std::string& description, const Db2AppInstanceDiscInfo& appInfo, std::string instanceName, std::string appDatabaseName,  InmRuleIds ruleId)
:Validator(name, description, ruleId), m_db2AppInfo(appInfo), m_instanceName(instanceName), m_dbName(appDatabaseName)
{
}

Db2DatabaseRunValidator:: Db2DatabaseRunValidator(const Db2AppInstanceDiscInfo &appInfo, std::string instanceName, std::string appDatabaseName, InmRuleIds ruleId)
:Validator(ruleId), m_db2AppInfo(appInfo), m_instanceName(instanceName), m_dbName(appDatabaseName)
{

}

SVERROR Db2DatabaseRunValidator::evaluate()
{
    SVERROR bRet = SVS_OK;
    DebugPrintf(SV_LOG_DEBUG, "ENTERED Db2DatabaseRunValidator:%s\n", FUNCTION_NAME) ;
    InmRuleStatus ruleStatus = INM_RULESTAT_NONE ;
    InmRuleErrorCode errorCode = INM_ERROR_NONE;
    std::stringstream stream ;

    stream.clear();    
    DebugPrintf(SV_LOG_DEBUG, "m_db2AppInfo.m_dbInstance :%s\n", (m_db2AppInfo.m_dbInstance).c_str());
    std::string dbname;
	std::list<std::string> dbNameList;
	bool isDbActive=0;
    std::map<std::string, Db2AppDatabaseDiscInfo> db2AppDbDiscInfo;   
	db2AppDbDiscInfo=m_db2AppInfo.m_db2DiscInfo;
	
    for(std::map<std::string, Db2AppDatabaseDiscInfo>::iterator db2DiscItr=db2AppDbDiscInfo.begin();db2DiscItr!=db2AppDbDiscInfo.end();db2DiscItr++)
    {
        dbname=((*db2DiscItr).first);
        DebugPrintf(SV_LOG_DEBUG, "dbname discovered :%s\n", dbname.c_str());
		
		if(strcmpi(m_dbName.c_str(), dbname.c_str()) == 0)
		{
			stream << "Db2 DatabaseRun Validator evaluate done" << std::endl;
			isDbActive=1;
			ruleStatus = INM_RULESTAT_PASSED;
			errorCode = RULE_PASSED;
		}
		dbNameList.push_back(dbname);
        DebugPrintf(SV_LOG_DEBUG, "DBName :%s\n", dbname.c_str()) ;
		dbname.clear();
    }   
    
    if(!isDbActive)
    {
       stream << "Db2 Database Not Discovered" << std::endl;
       ruleStatus = INM_RULESTAT_FAILED;
       errorCode = DB2_DATABASE_NOT_ACTIVE;
	   if(!(dbNameList.empty()))
	   {
         stream << "Discovered Databases are" <<std::endl;
		 for(std::list<std::string>::iterator it=dbNameList.begin();it!=dbNameList.end();it++)
		 {
			stream << (*it).c_str() <<std::endl;
		 }        
	   }
    }
       setRuleExitCode(errorCode);
       setStatus( ruleStatus )  ;
       std::string ruleMessage = stream.str();
       setRuleMessage(ruleMessage);
                                                               
       DebugPrintf(SV_LOG_DEBUG, "EXITED Db2DatabaseRunValidator: %s\n", FUNCTION_NAME) ;
       return bRet ;
}

SVERROR Db2DatabaseRunValidator::fix()
{
    SVERROR bRet = SVS_FALSE ;
    return bRet ;
}

bool Db2DatabaseRunValidator::canfix()
{
    bool bRet = false ;
    return bRet ;
}

Db2ConsistencyValidator::Db2ConsistencyValidator(const std::string& name, const std::string& description, const Db2AppInstanceDiscInfo& appInfo, InmRuleIds ruleId)
:Validator(name, description, ruleId), m_db2AppInfo(appInfo)
{
}

Db2ConsistencyValidator::Db2ConsistencyValidator(const Db2AppInstanceDiscInfo &appInfo, InmRuleIds ruleId)
    :Validator(ruleId), m_db2AppInfo(appInfo)
{

}

SVERROR Db2ConsistencyValidator::evaluate()
{
   DebugPrintf(SV_LOG_DEBUG, "ENTERED Db2ConsistencyValidator:%s\n", FUNCTION_NAME) ;

   InmRuleStatus ruleStatus = INM_RULESTAT_NONE ;
   InmRuleErrorCode errorCode = INM_ERROR_NONE;
   std::stringstream stream ;
   std::string srcConfigFile;

   SVERROR bRet = SVS_FALSE;
   ruleStatus = INM_RULESTAT_FAILED;
   errorCode = DB2_CONSISTENCY_CHECK_ERROR;

   stream.clear();
   
   AppLocalConfigurator localconfigurator ;

   std::string dbInstance=m_db2AppInfo.m_dbInstance;
   std::string dbname;
   std::map<std::string, Db2AppDatabaseDiscInfo> db2AppDbDiscInfoMap;
   db2AppDbDiscInfoMap=m_db2AppInfo.m_db2DiscInfo;
   Db2AppDatabaseDiscInfo db2AppDbDiscInfo;
   if(!db2AppDbDiscInfoMap.empty())
   {
	for(std::map<std::string, Db2AppDatabaseDiscInfo>::iterator it=db2AppDbDiscInfoMap.begin();it!=db2AppDbDiscInfoMap.end();it++)
	{
       dbname=(*it).first;
       DebugPrintf(SV_LOG_DEBUG,"Instance Namei : %s and Database Name: %s", dbInstance.c_str(),dbname.c_str());
       db2AppDbDiscInfo=(*it).second;
        if(!db2AppDbDiscInfo.m_filesMap.empty())
        {
           std::map<std::string, std::string>::const_iterator fileMapIter = db2AppDbDiscInfo.m_filesMap.begin();
		   //TODO: Check till now we have only one config file for each DB which will contain all the info related to the DB.
		   //And the file extension of DB config file is .xml only.
           while(fileMapIter != db2AppDbDiscInfo.m_filesMap.end())
           {					
			   srcConfigFile=(fileMapIter)->first;
               std::ofstream out;
               
               out.open(srcConfigFile.c_str(), std::ios::trunc);
               if (!out)
               {
                   DebugPrintf(SV_LOG_ERROR,"Failed to open %s file\n", srcConfigFile.c_str());
                   break;
               }

               out << (fileMapIter)->second;
               out.close();
               fileMapIter++;
			   
			    if (!srcConfigFile.empty())
				{
					SV_ULONG exitCode = 1 ;
					std::string strCommnadToExecute ;
																																																		AppLocalConfigurator localconfigurator ;
																																																		strCommnadToExecute += localconfigurator.getInstallPath();
																																																		strCommnadToExecute += "/scripts/db2_consistency.sh ";
																																																		strCommnadToExecute += "validate ";
																																																		strCommnadToExecute += dbInstance;
																																																		strCommnadToExecute += " ";
																																																		strCommnadToExecute += dbname;
																																																		strCommnadToExecute += " readinesstag ";
																																																		strCommnadToExecute += srcConfigFile;
																																																		strCommnadToExecute += " ";

					DebugPrintf(SV_LOG_INFO,"\n The command to execute : %s\n",strCommnadToExecute.c_str());

					stream << strCommnadToExecute;

					std::string outputFileName = "/tmp/tagreadiness.log";
																																																		ACE_OS::unlink(outputFileName.c_str());

					AppCommand objAppCommand(strCommnadToExecute, 0, outputFileName);

					std::string output;

																																																		if(objAppCommand.Run(exitCode, output, Controller::getInstance()->m_bActive) != SVS_OK)
					{
						DebugPrintf(SV_LOG_ERROR,"Failed to run script.\n");
						stream << "Failed readiness check. Output - " << output << std::endl ;
					}
					else
					{
						DebugPrintf(SV_LOG_INFO,"Successfully spawned the readiness script.\n");

						if (exitCode != 0)
						{
							DebugPrintf(SV_LOG_ERROR,"script returned error.\n");
							stream << "Failed readiness check. Output - " << output << std::endl ;
						}
						else
						{
							stream << "Readiness successful for database " << dbname << std::endl ;
																																																				ruleStatus = INM_RULESTAT_PASSED;
																																																				errorCode = RULE_PASSED;
																																																				bRet = SVS_OK;
						}	
					}
				}
			}
		}
		else
        {
            DebugPrintf(SV_LOG_ERROR,"Database config file is not found\n", srcConfigFile.c_str());
																																																stream << "Database discovery config file not found." << std::endl;
        }	  
    }
  }

   setRuleExitCode(errorCode);
   setStatus( ruleStatus )  ;
   std::string ruleMessage = stream.str();
   setRuleMessage(ruleMessage);
                                       
   DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
   return bRet ;
}


SVERROR Db2ConsistencyValidator::fix()
{
   SVERROR bRet = SVS_FALSE ;
   return bRet ;
}

bool Db2ConsistencyValidator::canfix()
{
   bool bRet = false ;
   return bRet ;
}

Db2DatabaseShutdownValidator::Db2DatabaseShutdownValidator(const std::string& name, 
                                                                 const std::string& description,
                                                                 const std::list<std::map<std::string,std::string> > &dbInfo,
                                                                 InmRuleIds ruleId)
:Validator(name, description, ruleId), m_db2AppInfo(dbInfo)
{
}

Db2DatabaseShutdownValidator::Db2DatabaseShutdownValidator(const std::list<std::map<std::string,std::string> > &dbInfo, InmRuleIds ruleId)
    :Validator(ruleId), m_db2AppInfo(dbInfo)
{

}
SVERROR Db2DatabaseShutdownValidator::evaluate()
{

    SVERROR bRet = SVS_OK;
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    InmRuleStatus ruleStatus = INM_RULESTAT_PASSED;
    InmRuleErrorCode errorCode = RULE_PASSED;
    std::stringstream stream ;

    stream.clear();

    std::list<std::map<std::string, std::string> >::iterator dbInfoIter = m_db2AppInfo.begin();
    while(dbInfoIter != m_db2AppInfo.end())
    {
        std::string dbInstanceName = dbInfoIter->find("dbInstance")->second;
		std::string dbName = dbInfoIter->find("dbName")->second;
        std::string status = dbInfoIter->find("Status")->second;		

        if (status.compare("Offline") == 0)
        {
          stream << "Db2 Database " << dbName << " of instances " << dbInstanceName << " is not active. " << std::endl;
        }
        else
        {
          stream << "Db2 Database " << dbName << " of instance " << dbInstanceName << " is active. " << std::endl;
          bRet = SVS_FALSE;
        }

        dbInfoIter++;
    }

     if (bRet == SVS_FALSE)
     {
        ruleStatus = INM_RULESTAT_FAILED;
        errorCode = DB2_INSTANCE_NOT_SHUTDOWN;
     }
     else
     {
        stream << "Done." << std::endl ;
     }

     setRuleExitCode(errorCode);
     setStatus( ruleStatus )  ;
     std::string ruleMessage = stream.str();
     setRuleMessage(ruleMessage);
                                                               
     DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
     return bRet ;

}

SVERROR Db2DatabaseShutdownValidator::fix()
{
     SVERROR bRet = SVS_FALSE ;
     return bRet ;
}

bool Db2DatabaseShutdownValidator::canfix()
{
    bool bRet = false ;
    return bRet ;
}

Db2DatabaseConfigValidator::Db2DatabaseConfigValidator(const std::string& name, const std::string& description,
                                                        const std::list<std::map<std::string,std::string> > &dbInfo,
                                                        const std::list<std::map<std::string,std::string> > &volInfo, InmRuleIds ruleId)
:Validator(name, description, ruleId), m_db2AppInfo(dbInfo), m_tgtVolInfo(volInfo)
{
}

Db2DatabaseConfigValidator::Db2DatabaseConfigValidator(const std::list<std::map<std::string,std::string> > &dbInfo,
                                                              const std::list<std::map<std::string,std::string> > &volInfo, InmRuleIds ruleId)
:Validator(ruleId), m_db2AppInfo(dbInfo), m_tgtVolInfo(volInfo)
{

}
SVERROR Db2DatabaseConfigValidator::evaluate()
{
   SVERROR bRet = SVS_OK;
   DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
   InmRuleStatus ruleStatus = INM_RULESTAT_PASSED;
   InmRuleErrorCode errorCode = RULE_PASSED;
   std::stringstream stream ;

   stream.clear();

   AppLocalConfigurator localconfigurator;
   std::string tgtVolInfoFile = localconfigurator.getInstallPath() + "/etc/Db2TgtVolInfoFile.dat";
   std::ofstream out;

   out.open(tgtVolInfoFile.c_str(), std::ios::trunc);
   if (!out) 
   {
      DebugPrintf(SV_LOG_ERROR,"Failed to open %s file\n", tgtVolInfoFile.c_str());
      bRet = SVS_FALSE;
   }
   else
   { 
      std::list<std::map<std::string, std::string> >::iterator volInfoIter = m_tgtVolInfo.begin();

      while (volInfoIter != m_tgtVolInfo.end())
      {
         std::string deviceName = volInfoIter->find("DeviceName")->second;
         std::string fsType = volInfoIter->find("FileSystemType")->second;
         std::string volType = volInfoIter->find("VolumeType")->second;
         //std::string vendor = volInfoIter->find("VendorOrigin")->second;
         std::string mountPoint = volInfoIter->find("MountPoint")->second;
         std::string diskGroup = volInfoIter->find("DiskGroup")->second;
         //std::string shouldDestroy = volInfoIter->find("shouldDestroy")->second;

         if (fsType.empty()) fsType = "0";
         if (mountPoint.empty()) mountPoint = "0";
         if (diskGroup.empty()) diskGroup = "0";

         DebugPrintf( SV_LOG_DEBUG, "deviceName: '%s' fsType: '%s' volType: '%s' mountPoint: '%s' diskGroup: '%s' \n", deviceName.c_str(), fsType.c_str(), volType.c_str(), mountPoint.c_str(), diskGroup.c_str()) ;

         std::string line = "DeviceName=" + deviceName + ":";
         line += "FileSystemType=" + fsType + ":";
         line += "VolumeType=" + volType + ":";
         //line += "VendorOrigin=" + vendor + ":";
         line += "MountPoint=" + mountPoint + ":";
         line += "DiskGroup=" + diskGroup + ":";
         //line += "shouldDestroy=" + shouldDestroy + " ";

         DebugPrintf( SV_LOG_DEBUG, "tgtVolInfo %s\n", line.c_str());

         out << line << std::endl;

         volInfoIter++;
      }

    out.close();
   }

   std::list<std::map<std::string, std::string> >::iterator dbInfoIter = m_db2AppInfo.begin();
   while(dbInfoIter != m_db2AppInfo.end())
   {
      //Here in dbName will represent instanceName:dbname for DB2
	  std::string dbInstanceName = dbInfoIter->find("dbName")->second;
      std::string instanceName = dbInstanceName.substr(0,dbInstanceName.find_first_of(":"));
      std::string dbName = dbInstanceName.substr(dbInstanceName.find_first_of(":")+1);
      std::string fileName = dbInfoIter->find("configFileName")->second;
      std::string config = dbInfoIter->find("configFileContents")->second;

      AppLocalConfigurator localconfigurator ;
      std::string srcConfigFile = "/tmp/" + fileName;
 
      std::ofstream out;
      out.open(srcConfigFile.c_str(), std::ios::trunc);
      if (!out)
      {
          DebugPrintf(SV_LOG_ERROR,"Failed to open %s file\n", srcConfigFile.c_str());
          stream << "Failed to open config file " << srcConfigFile << std::endl ;
          bRet = SVS_FALSE;
          break;
      }
      out << config;
      out.close();

      DebugPrintf( SV_LOG_DEBUG, "Src DB Instance Name: '%s' DBName : '%s' and Conf filename : '%s' \n", instanceName.c_str(), dbName.c_str(), srcConfigFile.c_str()) ;

      SV_ULONG exitCode = 1 ;
      std::string strCommnadToExecute ;
      strCommnadToExecute += localconfigurator.getInstallPath();
      strCommnadToExecute += "/scripts/db2appagent.sh ";
      strCommnadToExecute += "targetreadiness ";
      strCommnadToExecute += srcConfigFile;
      strCommnadToExecute += std::string(" ");

      DebugPrintf(SV_LOG_INFO,"\n The targetreadiness command to execute : %s\n",strCommnadToExecute.c_str());

      stream << strCommnadToExecute;

      std::string outputFileName = "/tmp/db2tgtreadiness.log";
      ACE_OS::unlink(outputFileName.c_str());

      AppCommand objAppCommand(strCommnadToExecute, 0, outputFileName);

      std::string output;
      
      if(objAppCommand.Run(exitCode, output, Controller::getInstance()->m_bActive ) != SVS_OK)
      {
         DebugPrintf(SV_LOG_ERROR,"Failed target readiness script.\n");
         stream << "Failed target readiness. Output - " << output << std::endl ;
         bRet = SVS_FALSE;
         break;
      }
     else
     {
         DebugPrintf(SV_LOG_INFO,"Successfully spawned the target readiness script.\n");

         if (exitCode != 0)
         {
             DebugPrintf(SV_LOG_ERROR,"Failed db2 prepare target script.\n");
             stream << "Failed target readiness. Output - " << output << std::endl ;
             bRet = SVS_FALSE;
             break;
         }

         stream << "Target readiness successful for db Instance " << instanceName << "and db " << dbName << std::endl ;
      }

      dbInfoIter++;
   }
   if (bRet == SVS_FALSE)
   {
      ruleStatus = INM_RULESTAT_FAILED;
      errorCode = DATABASE_CONFIG_ERROR;
   }
   else
   {
       stream << "Target Db2 database configuration is in sync with the source." << std::endl ;
   }

   setRuleExitCode(errorCode);
   setStatus( ruleStatus )  ;
   std::string ruleMessage = stream.str();
   setRuleMessage(ruleMessage);
                        
   DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
   return bRet ;
}
SVERROR Db2DatabaseConfigValidator::fix()
{
   SVERROR bRet = SVS_FALSE ;
   return bRet ;
}

bool Db2DatabaseConfigValidator::canfix()
{
   bool bRet = false ;
   return bRet ;
}

