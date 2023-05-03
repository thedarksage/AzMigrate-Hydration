#include <sstream>
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string.hpp>
#include "host.h"
#include "Db2Discovery.h"
#include "util/common/util.h"
#include "inmcommand.h"
#include "cdpsnapshotrequest.h"
#include <string>
#include "portablehelpers.h"
#include "config/appconfigurator.h"
#include "appcommand.h"
#include "controller.h"

ACE_Recursive_Thread_Mutex Db2DiscoveryImpl::m_mutex;

boost::shared_ptr<Db2DiscoveryImpl> Db2DiscoveryImpl::m_instancePtr ;

boost::shared_ptr<Db2DiscoveryImpl> Db2DiscoveryImpl::getInstance()
{
    if( m_instancePtr.get() == NULL )
    {
        m_instancePtr.reset(new Db2DiscoveryImpl()) ;
    }
    return m_instancePtr ;
}

SVERROR Db2DiscoveryImpl::fillDb2UnregisterInfo( std::map<std::string, std::string> &unregisterDetailsMap, 
    Db2AppDiscoveryInfo &db2AppDiscInfo)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED Db2DiscoveryImpl::%s\n", FUNCTION_NAME) ;
	SVERROR bRet = SVS_FALSE ;

	if(unregisterDetailsMap.empty())
    {
        DebugPrintf(SV_LOG_DEBUG, " Unregister Database map is empty\n");
        return SVS_FALSE;
    }

    Db2UnregisterDiscInfo db2UnregisterDiscInfo;
	std::string value;
	std::string dbinstance,dbname;
	std::map<std::string, std::string>::iterator it;
    std::list<std::string> dbnames;
	for(it=unregisterDetailsMap.begin();it!=unregisterDetailsMap.end();it++)
	{
	    value=(*it).second;
		dbinstance=value.substr(0,value.find_first_of(":"));
		dbname=value.substr(value.find_first_of(":")+1);
        if(db2AppDiscInfo.m_db2UnregisterDiscInfo.find(dbinstance)!=db2AppDiscInfo.m_db2UnregisterDiscInfo.end())
        {
            DebugPrintf(SV_LOG_DEBUG, " Instance is already mapped for unregistered databases \n");
            ((db2AppDiscInfo.m_db2UnregisterDiscInfo.find(dbinstance))->second).m_dbNames.push_back(dbname);
        }
        else
        {
          dbnames.push_back(dbname);
          db2UnregisterDiscInfo.m_dbNames=dbnames;
          db2AppDiscInfo.m_db2UnregisterDiscInfo.insert(std::make_pair(dbinstance, db2UnregisterDiscInfo));
        }
	}        

    bRet = SVS_OK;

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;

    return bRet;
}

SVERROR Db2DiscoveryImpl::fillDb2AppInstanceDiscInfo(std::map<std::string, std::string>& discoveryOutputMap,Db2AppDiscoveryInfo &db2AppDiscInfo)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED Db2DiscoveryImpl::%s\n", FUNCTION_NAME) ;
	
	Db2AppInstanceDiscInfo db2InstDiscInfo;
	Db2AppDatabaseDiscInfo db2DatabaseDiscInfo;
	SVERROR bRet = SVS_FALSE ;
	
	if(discoveryOutputMap.empty())
    {
        DebugPrintf(SV_LOG_ERROR, " Discovery Data not in Correct Format\n");
        return SVS_FALSE;
    }

    LocalConfigurator localConfigurator;
    std::string installPath = localConfigurator.getInstallPath();
    std::string instance;
    int updatedbInfo=0;
		
    if (discoveryOutputMap.find("HostID") != discoveryOutputMap.end())
    {
        db2InstDiscInfo.m_hostId = discoveryOutputMap.find("HostID")->second;
    }
	
	if (discoveryOutputMap.find("MachineName") != discoveryOutputMap.end())
    {
        db2InstDiscInfo.m_nodeName = discoveryOutputMap.find("MachineName")->second;
    }
	
	if (discoveryOutputMap.find("DBInstance") != discoveryOutputMap.end())
    {
        instance = discoveryOutputMap.find("DBInstance")->second;
        DebugPrintf(SV_LOG_ERROR, "DB Instance found in discoveryOutputMap :%s\n",instance.c_str());
        if(db2AppDiscInfo.m_db2InstDiscInfo.find(instance) != db2AppDiscInfo.m_db2InstDiscInfo.end())
        {
         DebugPrintf(SV_LOG_DEBUG,"Instance %s is already present in the Db2AppDiscoveryInfo, update only DB Info",instance.c_str());
         updatedbInfo=1;
        }
        db2InstDiscInfo.m_dbInstance = discoveryOutputMap.find("DBInstance")->second;
    }
    else
    {
        DebugPrintf(SV_LOG_ERROR, "DB Instance not found in discovery\n");
        return SVS_FALSE;
    }
	
	if (discoveryOutputMap.find("DBInstallationPath") != discoveryOutputMap.end())
    {
        db2InstDiscInfo.m_dbInstallPath = discoveryOutputMap.find("DBInstallationPath")->second;
    }
    
	if (discoveryOutputMap.find("DBVersion") != discoveryOutputMap.end())
    {
        db2InstDiscInfo.m_dbVersion = discoveryOutputMap.find("DBVersion") ->second;
    }
    else
    {
        DebugPrintf(SV_LOG_ERROR, "DB Version not found in discovery\n");
        return SVS_FALSE;
    }
	
	if (discoveryOutputMap.find("DBName") != discoveryOutputMap.end())
    {
        db2DatabaseDiscInfo.m_dbName = discoveryOutputMap.find("DBName")->second;
    }
    else
    {
        DebugPrintf(SV_LOG_ERROR, "DB Name not found in discovery\n");
        return SVS_FALSE;
    }
	
	if (discoveryOutputMap.find("DBLocation") != discoveryOutputMap.end())
    {
        db2DatabaseDiscInfo.m_dbLocation = discoveryOutputMap.find("DBLocation")->second;
    }
    else
    {
        DebugPrintf(SV_LOG_ERROR, "DBLocation not found in discovery\n");
        return SVS_FALSE;
    }
	
	if (discoveryOutputMap.find("ConfigFile") != discoveryOutputMap.end())
    {
        std::string dbConfFileLocation = discoveryOutputMap.find("ConfigFile")->second;
        std::string dbConfFileName = basename_r(dbConfFileLocation.c_str(), '/');

        std::string dbConfFileContents = getFileContents(dbConfFileLocation);
        DebugPrintf(SV_LOG_ERROR, "dbConfFileContents Length : %d \n",strlen(dbConfFileContents.c_str()));
        if(dbConfFileContents.empty())
        {
            DebugPrintf(SV_LOG_ERROR, "ConfigFile is Empty \n");
            //return SVS_FALSE; TODO: Need to check if config file is not found whether to return or not.
        }
		db2DatabaseDiscInfo.m_filesMap.insert(std::make_pair(dbConfFileName, dbConfFileContents));
    }
    else
    {
        DebugPrintf(SV_LOG_ERROR, "Config file is not found in discovery\n");
        return SVS_FALSE;
    }	

	if (discoveryOutputMap.find("RecoveryLogEnabled") != discoveryOutputMap.end())
    {
        if("RECOVERY" == discoveryOutputMap.find("RecoveryLogEnabled")->second)
        {
            db2DatabaseDiscInfo.m_recoveryLogEnabled = true;    
        }
        else
        {
            db2DatabaseDiscInfo.m_recoveryLogEnabled = false;
        }
    }

	if (discoveryOutputMap.find("ViewLevels") != discoveryOutputMap.end())
    {
        db2DatabaseDiscInfo.m_diskView = tokenizeString(discoveryOutputMap.find("ViewLevels")->second, ";");
    }
    else
    {
        DebugPrintf(SV_LOG_ERROR, "ViewLevels not found in discovery\n");
        return SVS_FALSE;
    }
    
    if (discoveryOutputMap.find("FilterDevice") != discoveryOutputMap.end())
    {
        db2DatabaseDiscInfo.m_filterDeviceList = tokenizeString(discoveryOutputMap.find("FilterDevice")->second.c_str(), ",");
    }
    else
    {
        DebugPrintf(SV_LOG_ERROR, "FilterDevices not found in discovery\n");
        return SVS_FALSE;
    }
 
	
	//Print the db2InstDiscInfo map 
	DebugPrintf(SV_LOG_DEBUG,"db2InstDiscInfo.m_hostId = %s\n",(db2InstDiscInfo.m_hostId).c_str()) ;
	DebugPrintf(SV_LOG_DEBUG,"db2InstDiscInfo.m_nodeName = %s\n",(db2InstDiscInfo.m_nodeName).c_str()) ;		
	DebugPrintf(SV_LOG_DEBUG,"db2InstDiscInfo.m_dbInstance = %s\n",(db2InstDiscInfo.m_dbInstance).c_str()) ;	
	DebugPrintf(SV_LOG_DEBUG,"db2InstDiscInfo.m_dbVersion = %s\n",(db2InstDiscInfo.m_dbVersion).c_str()) ;
	DebugPrintf(SV_LOG_DEBUG,"db2InstDiscInfo.m_dbInstallPath = %s\n",(db2InstDiscInfo.m_dbInstallPath).c_str()) ;
	
	DebugPrintf(SV_LOG_DEBUG,"db2DatabaseDiscInfo.m_dbName = %s\n",(db2DatabaseDiscInfo.m_dbName).c_str()) ;
	DebugPrintf(SV_LOG_DEBUG,"db2DatabaseDiscInfo.m_dbLocation = %s\n",(db2DatabaseDiscInfo.m_dbLocation).c_str()) ;
	DebugPrintf(SV_LOG_DEBUG,"db2DatabaseDiscInfo.m_recoveryLogEnabled = %d\n",db2DatabaseDiscInfo.m_recoveryLogEnabled) ;
	
	std::map<std::string, std::string>::iterator it;
	for(it=(db2DatabaseDiscInfo.m_filesMap).begin();it!=(db2DatabaseDiscInfo.m_filesMap).end();it++)
	{
		DebugPrintf(SV_LOG_DEBUG,"db2DatabaseDiscInfo.m_filesMap first= %s\n",((*it).first).c_str()) ;
		DebugPrintf(SV_LOG_DEBUG,"db2DatabaseDiscInfo.m_filesMap second = %s\n",((*it).second).c_str()) ;		
	}
	std::list<std::string>::iterator itDiskView;
	for(itDiskView=(db2DatabaseDiscInfo.m_diskView).begin(); itDiskView!=(db2DatabaseDiscInfo.m_diskView).end(); itDiskView++)
	{
		DebugPrintf(SV_LOG_DEBUG,"db2DatabaseDiscInfo.m_diskView = %s\n",(*itDiskView).c_str()) ;		
	}	
	std::list<std::string>::iterator itFilterDevice;
	for(itFilterDevice=(db2DatabaseDiscInfo.m_filterDeviceList).begin(); itFilterDevice!=(db2DatabaseDiscInfo.m_filterDeviceList).end(); itFilterDevice++)
	{
		DebugPrintf(SV_LOG_DEBUG," db2DatabaseDiscInfo.m_filterDeviceList = %s\n",(*itFilterDevice).c_str()) ;		
	}	
	
    if(updatedbInfo == 1)
    {
        if(db2AppDiscInfo.m_db2InstDiscInfo.find(instance) != db2AppDiscInfo.m_db2InstDiscInfo.end())
        {
            (db2AppDiscInfo.m_db2InstDiscInfo.find(instance)->second).m_db2DiscInfo.insert(std::make_pair(db2DatabaseDiscInfo.m_dbName, db2DatabaseDiscInfo));
        }
    }
    else
    {
    	db2InstDiscInfo.m_db2DiscInfo.insert(std::make_pair(db2DatabaseDiscInfo.m_dbName, db2DatabaseDiscInfo));
        db2AppDiscInfo.m_db2InstDiscInfo.insert(std::make_pair(db2InstDiscInfo.m_dbInstance, db2InstDiscInfo));
    }

    bRet = SVS_OK;

    DebugPrintf(SV_LOG_DEBUG, "\nEXITED %s\n", FUNCTION_NAME) ;
    return bRet ;	

}

SVERROR Db2DiscoveryImpl::getDb2DiscoveryData(Db2AppDiscoveryInfo & Db2AppDiscInfo)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED Db2DiscoveryImpl::%s\n", FUNCTION_NAME) ;
    SVERROR bRet = SVS_OK;
   
    AppLocalConfigurator localconfigurator;
    std::string installPath = localconfigurator.getInstallPath();
    std::string outputFile = installPath + "etc/db2discoveryoutput.dat";

    std::string discoveryOutput;

    ACE_Guard<ACE_Recursive_Thread_Mutex> guard(m_mutex);
    SV_ULONG exitCode = 1 ;
    std::string strCommandToExecute ;
    strCommandToExecute += installPath;
    strCommandToExecute += "scripts/db2appagent.sh discover ";
    strCommandToExecute += outputFile;
    strCommandToExecute += std::string(" ");

    DebugPrintf(SV_LOG_INFO,"\n The discovery command to execute : %s\n",strCommandToExecute.c_str());

    AppCommand objAppCommand(strCommandToExecute, 0, outputFile);

    std::string output;

    if(objAppCommand.Run(exitCode, output, Controller::getInstance()->m_bActive) != SVS_OK)
    {
        DebugPrintf(SV_LOG_ERROR,"Failed to spawn db2 discovery script %s.\n", strCommandToExecute.c_str());
        bRet = SVS_FALSE;
    }
    else
    {
        DebugPrintf(SV_LOG_INFO,"Successfully spawned the db2 discover script.\n");
        DebugPrintf(SV_LOG_INFO,"output. %s\n", output.c_str());

        if (exitCode != 0)
        {
            DebugPrintf(SV_LOG_ERROR,"Failed db2 discover script %s.\n", strCommandToExecute.c_str());
            bRet = SVS_FALSE;
        }
    }

    if ( bRet == SVS_FALSE )
    {
        return bRet;
    }
                  
    discoveryOutput = getFileContents(outputFile);

    if(discoveryOutput.empty())
    {
        DebugPrintf(SV_LOG_DEBUG, "Discovery Output File is Empty \n");
        return SVS_FALSE;
    }

	std::list<std::string> discoveryScriptOutputTokensList, keyValueTokens;
	std::map<std::string, std::string> discoveryDataMap;
	std::map<std::string, std::string> unregisterMap;
	std::string machineName,hostId,dbInstance,InstallPath,viewLevel,dbVersion;	
	int isDbFound=0;

	std::string viewLevels,filterDevices, outputTokenIterValue,levels;    

	discoveryScriptOutputTokensList = tokenizeString(discoveryOutput, "\n");
	
	std::list<std::string>::iterator outputTokenIter = discoveryScriptOutputTokensList.begin();
	
	while(outputTokenIter != discoveryScriptOutputTokensList.end())
    {
        outputTokenIterValue = *outputTokenIter;	
		keyValueTokens = tokenizeString((*outputTokenIter), "=");		
		
		DebugPrintf(SV_LOG_DEBUG,"Key - %s --> value - %s\n",(keyValueTokens.front()).c_str(),(keyValueTokens.back()).c_str());
		
		if(keyValueTokens.size() == 2)
		{
		    if(keyValueTokens.front() == "Unregister" )
			{
				unregisterMap.insert(std::make_pair(keyValueTokens.front(), keyValueTokens.back()));
				bRet = fillDb2UnregisterInfo(unregisterMap, Db2AppDiscInfo);
				if (bRet == SVS_FALSE)
				{
					DebugPrintf(SV_LOG_ERROR, "Failed to fill Db2 unregister info\n");
				}
				unregisterMap.clear();	
			}
			
			else if(keyValueTokens.front() == "MachineName")
			{
				machineName.clear();
                machineName=keyValueTokens.back();	
				//discoveryDataMap.insert(std::make_pair("MachineName",machineName));
                DebugPrintf(SV_LOG_DEBUG,"Machine Name received : %s\n",machineName.c_str());
			}
			else if(keyValueTokens.front() == "HostID")
			{
                hostId.clear();
				hostId=keyValueTokens.back();	
				//discoveryDataMap.insert(std::make_pair("HostID",hostId));
                DebugPrintf(SV_LOG_DEBUG,"HostId received :%s\n",hostId.c_str());
			}
			else if(keyValueTokens.front() == "DBInstance")
			{
                dbInstance.clear();
				dbInstance=keyValueTokens.back();	
				//discoveryDataMap.insert(std::make_pair("DBInstance",dbInstance));
                DebugPrintf(SV_LOG_DEBUG,"DBInstance Received :%s\n",dbInstance.c_str());
			}
			else if(keyValueTokens.front() == "DBInstallationPath")
			{
                InstallPath.clear();
				InstallPath=keyValueTokens.back();	
				//discoveryDataMap.insert(std::make_pair("DBInstallationPath",InstallPath));
                DebugPrintf(SV_LOG_DEBUG,"DBInstallation Path received :%s\n",InstallPath.c_str());
			}	
			else if(keyValueTokens.front() == "DBVersion")
			{
                dbVersion.clear();
				dbVersion=keyValueTokens.back();	
				//discoveryDataMap.insert(std::make_pair("DBVersion",dbVersion));
                DebugPrintf(SV_LOG_DEBUG,"DBVersion received :%s\n",dbVersion.c_str());
			}	
			else if(keyValueTokens.front() == "DBName")
			{
                viewLevels.clear();
				viewLevels=dbInstance+":"+keyValueTokens.back()+",0;";
                DebugPrintf(SV_LOG_ERROR, "viewLevel %s\n",viewLevels.c_str());
                DebugPrintf(SV_LOG_DEBUG,"DBName received : %s\n",(keyValueTokens.back()).c_str());

                /*if(isDbFound!=1)
				{
					isDbFound=1;
					discoveryDataMap.insert(std::make_pair(keyValueTokens.front(), keyValueTokens.back()));
				}
				else
				{*/
                    //discoveryDataMap.clear();
					discoveryDataMap.insert(std::make_pair("MachineName",machineName));
					discoveryDataMap.insert(std::make_pair("HostID",hostId));
					discoveryDataMap.insert(std::make_pair("DBInstance",dbInstance));
					discoveryDataMap.insert(std::make_pair("DBInstallationPath",InstallPath));
					discoveryDataMap.insert(std::make_pair("DBVersion",dbVersion));
					discoveryDataMap.insert(std::make_pair(keyValueTokens.front(), keyValueTokens.back()));
				//}
			}
			else if(keyValueTokens.front() != "DBName")
            {
                if((keyValueTokens.front() == "MountPointWithLevel") || (keyValueTokens.front() == "VolumeNameWithLevel") || (keyValueTokens.front() == "DiskGroupNameWithLevel") || (keyValueTokens.front() == "DiskNameWithLevel"))
               {
					std::string value=keyValueTokens.back();
					//viewLevel=(value).substr((value).find_last_of(",")+1);
					viewLevel=value;
					DebugPrintf(SV_LOG_ERROR, "viewLevel %s\n",viewLevel.c_str());
					viewLevels=viewLevels+viewLevel+";";
												
              }
              discoveryDataMap.insert(std::make_pair(keyValueTokens.front(), keyValueTokens.back()));
			}				
			
			if((keyValueTokens.front()) == "FilterDevice")
			{
				discoveryDataMap.insert(std::make_pair("ViewLevels",viewLevels));				
				
				bRet = fillDb2AppInstanceDiscInfo(discoveryDataMap, Db2AppDiscInfo);

				if (bRet == SVS_FALSE)
				{
					DebugPrintf(SV_LOG_ERROR, "Failed to fill db2 database discovery info\n");
					return SVS_FALSE;
				}	
				discoveryDataMap.clear();	
			}				
		}
		else
        {
            DebugPrintf(SV_LOG_ERROR, "Unexpected number of token values in Database\n");
           // return SVS_FALSE;
  	    }		
			
		outputTokenIter++;			  
    }

    bRet = SVS_OK;
    DebugPrintf(SV_LOG_DEBUG, "EXITED Db2DiscoveryImpl::%s\n", FUNCTION_NAME) ;
	return bRet;
}

bool Db2DiscoveryImpl::isInstalled(const std::string& db2Host)
{
    if(m_isInstalled)
    {
        DebugPrintf(SV_LOG_DEBUG, "Db2 is installed on %s\n", db2Host.c_str()) ;
        return true ;
    }
    else
    {
        DebugPrintf(SV_LOG_DEBUG, "Db2 is not installed on %s\n", db2Host.c_str()) ;
        return false ;
    }
}

SVERROR Db2DiscoveryImpl::discoverDb2Application(Db2AppDiscoveryInfo& db2AppDiscInfo)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    SVERROR bRet = SVS_FALSE ;
    
    bRet = getDb2DiscoveryData(db2AppDiscInfo);
    DebugPrintf(SV_LOG_DEBUG,"Printing the discovered DB info\n");
    std::map<std::string, Db2AppInstanceDiscInfo>::iterator instDiscItr=db2AppDiscInfo.m_db2InstDiscInfo.begin();
    Db2AppInstanceDiscInfo db2AppInstDiscInfo;
    Db2AppDatabaseDiscInfo db2AppDbDiscInfo;
    Db2UnregisterDiscInfo db2UnregDiscInfo;
    for(;instDiscItr!=db2AppDiscInfo.m_db2InstDiscInfo.end();instDiscItr++)
    {
        DebugPrintf(SV_LOG_DEBUG,"Instance Discovered : %s\n",(instDiscItr->first).c_str());
        db2AppInstDiscInfo = instDiscItr->second;
        DebugPrintf(SV_LOG_DEBUG,"HostId : %s\n",db2AppInstDiscInfo.m_hostId.c_str());
        DebugPrintf(SV_LOG_DEBUG,"NodeName : %s\n",db2AppInstDiscInfo.m_nodeName.c_str());
        DebugPrintf(SV_LOG_DEBUG,"DB Instance : %s\n",db2AppInstDiscInfo.m_dbInstance.c_str());
        DebugPrintf(SV_LOG_DEBUG,"DB Installation Path : %s\n",db2AppInstDiscInfo.m_dbInstallPath.c_str());
        DebugPrintf(SV_LOG_DEBUG,"DB Version :%s\n",db2AppInstDiscInfo.m_dbVersion.c_str());
        std::map<std::string, Db2AppDatabaseDiscInfo>::iterator dbDiscItr=db2AppInstDiscInfo.m_db2DiscInfo.begin();
        for(;dbDiscItr!=db2AppInstDiscInfo.m_db2DiscInfo.end();dbDiscItr++)
        {
            DebugPrintf(SV_LOG_DEBUG,"Database Discovered : %s\n",(dbDiscItr->first).c_str());
            db2AppDbDiscInfo = dbDiscItr->second;
            DebugPrintf(SV_LOG_DEBUG,"DBName : %s\n",db2AppDbDiscInfo.m_dbName.c_str());
            DebugPrintf(SV_LOG_DEBUG,"DBLocation : %s\n",db2AppDbDiscInfo.m_dbLocation.c_str());
            std::map<std::string, std::string>::iterator fileitr=db2AppDbDiscInfo.m_filesMap.begin();
            for(;fileitr!=db2AppDbDiscInfo.m_filesMap.end();fileitr++)
            {
              DebugPrintf(SV_LOG_DEBUG,"File Name : %s\n",(fileitr->first).c_str());
              DebugPrintf(SV_LOG_DEBUG,"File Contents : %s\n",(fileitr->second).c_str());
            }
            std::list <std::string>::iterator viewitr=db2AppDbDiscInfo.m_diskView.begin();
            for(;viewitr!=db2AppDbDiscInfo.m_diskView.end();viewitr++)
            {
                DebugPrintf(SV_LOG_DEBUG,"Disk View : %s\n",(*viewitr).c_str());
            }
            std::list<std::string>::iterator fildevitr=db2AppDbDiscInfo.m_filterDeviceList.begin();
            for(;fildevitr!=db2AppDbDiscInfo.m_filterDeviceList.end();fildevitr++)
            {
                DebugPrintf(SV_LOG_DEBUG,"Filter Device : %s\n",(*fildevitr).c_str());
            }
        }
    }
    std::map<std::string, Db2UnregisterDiscInfo>::iterator unregDiscItr=db2AppDiscInfo.m_db2UnregisterDiscInfo.begin();
    for(;unregDiscItr!=db2AppDiscInfo.m_db2UnregisterDiscInfo.end();unregDiscItr++)
    {
      DebugPrintf(SV_LOG_DEBUG,"Instance of Unregistered Database : %s\n",(unregDiscItr->first).c_str());
      db2UnregDiscInfo = unregDiscItr->second;
      std::list<std::string>::iterator unregDbItr=db2UnregDiscInfo.m_dbNames.begin();
      for(;unregDbItr!=db2UnregDiscInfo.m_dbNames.end();unregDbItr++)
      {
        DebugPrintf(SV_LOG_DEBUG,"Unregisted Database : %s\n",(*unregDbItr).c_str());
      }
    }
    
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}

void Db2DiscoveryImpl::Display(const Db2AppInstanceDiscInfo& db2AppInfo)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    DebugPrintf(SV_LOG_DEBUG, "Is Database Installed - %s\n", m_isInstalled);
    DebugPrintf(SV_LOG_DEBUG, "Is DiskGroup Mounted - %s\n", m_isMounted);
    DebugPrintf(SV_LOG_DEBUG, "Database Version - %s\n",db2AppInfo.m_dbVersion.c_str());        
    DebugPrintf(SV_LOG_DEBUG, "Node Name - %s\n", db2AppInfo.m_nodeName.c_str());
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}

void Db2DiscoveryImpl::setHostName(const std::string& hostname)
{
    m_hostName = hostname ;
}



