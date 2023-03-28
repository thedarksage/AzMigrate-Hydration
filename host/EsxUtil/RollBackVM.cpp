#include "ChangeVmConfig.h"
#include "portablehelpersmajor.h"
#include "svutils.h"
#include "inmsafeint.h"
#include "inmageex.h"
#include "boost/date_time/posix_time/posix_time.hpp"
#include <boost/lexical_cast.hpp>

using namespace std;

#define FS_TAGTYPE				     "FS"
#define TIME_TAGTYPE                 "TIME"
#define LATEST_TAG                   "LATEST"
#define LATEST_TIME                  "LATESTTIME"
#define RECOVERY_PROGRESS            "_recoveryprogress"
#define RECOVERY_PROGRESS_STATUS     "RecoveryProgressStatus"
#define TAG_NAME                     "TagName"
#define TAG_TYPE                     "TagType"

#ifdef WIN32
#define CDPCLI_BINARY                "cdpcli.exe"
#define DIRECTORY_SEPARATOR          "\\"
#else
#define CDPCLI_BINARY                "cdpcli"
#define DIRECTORY_SEPARATOR          "/"
#endif

std::string RollBackVM::GetCurrentLocalTime()
{
	boost::posix_time::ptime currTime = boost::posix_time::second_clock::local_time();
	std::stringstream ss;
	ss << currTime.date().year() << "_" << static_cast<int>(currTime.date().month()) << "_" << currTime.date().day() << "_"
		<< currTime.time_of_day().hours() << "_" << currTime.time_of_day().minutes() << "_" << currTime.time_of_day().seconds();

	return ss.str();
}

//This function does the preparation to rollback, validates the options, do the rollback and extra checks
// 1. Get the map of protected pairs for give VM hostname
// 2. Get the vector of retention data base log paths
// 3. Using above, validate the given rollback options and check if rollback is possible
// 4. Do the rollback
// 5. In case of windows, if extra check option is provided do the chkdsk on each volume
bool RollBackVM::RollBackVMs(std::string HostName, std::string Value, std::string Type, std::string& strCdpCliCmd, StatusInfo& statInfo , RecoveryFlowStatus& RecStatus)
{
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    bool rv = true;
	m_bClus = false;

    do
    {
		//RecoveryFlowStatus RecStatus;
		ChangeVmConfig objVm;
		objVm.m_bDrDrill = m_bPhySnapShot;
		objVm.m_mapOfVmNameToId = m_mapOfVmNameToId;

		statInfo.PlaceHolder.clear();
		string strSrcHostName = "";
		map<string, string>::iterator iterH = m_mapOfVmNameToId.find(HostName);
		if (iterH != m_mapOfVmNameToId.end())
			strSrcHostName = iterH->second;
		statInfo.PlaceHolder.Properties[MT_HOSTNAME] = m_strHostName;

		//First find the List of Protected Volumes
		//As we are getting RecoveryFlowStatus upfront, we could skip some of the actions here.. like collecting the pair information, retention db path and validation of tag. TODO.
		std::map<std::string, std::string> mapProtectedPairs;
		if(m_bCloudMT)
		{
			map<string, map<string, string> >::iterator iterHost = m_RecVmsInfo.m_volumeMapping.find(HostName);
			if(iterHost != m_RecVmsInfo.m_volumeMapping.end())
			{
				map<string, string>::iterator iterVolPair = iterHost->second.begin();
				for(; iterVolPair != iterHost->second.end(); iterVolPair++)
				{
					mapProtectedPairs.insert(make_pair(iterVolPair->first, iterVolPair->second));
				}
			}
			if(mapProtectedPairs.empty())
			{
				DebugPrintf(SV_LOG_ERROR,"Protected Pairs are empty for the VM - %s\n", HostName.c_str());
				statInfo.ErrorMsg = "Master target " + m_strHostName + " unable to identify the target volumes corresponding to the source machine";
				statInfo.ErrorCode = "EA0606";
				rv = false; break;	        
			}
		}
		else
		{			
			if(m_bPhySnapShot)
				mapProtectedPairs = objVm.getProtectedOrSnapshotDrivesMap(HostName, true);
			else
				mapProtectedPairs = objVm.getProtectedOrSnapshotDrivesMap(HostName);
			if(mapProtectedPairs.empty())
			{
				DebugPrintf(SV_LOG_ERROR,"Protected or Snapshot Pairs is empty for the VM - %s\n", HostName.c_str());
				statInfo.ErrorMsg = "Master target " + m_strHostName + " unable to identify the target volumes corresponding to the source machine";
				statInfo.ErrorCode = "EA0606";
				rv = false; break;	        
			}
		}

#ifndef WIN32
		std::map<std::string, std::string>::iterator iterPair = mapProtectedPairs.begin();
		for(; iterPair != mapProtectedPairs.end(); iterPair++)
		{
			blockDevFlushBuffer(iterPair->second);
			if(!m_bV2P && !m_bCloudMT)
			{
				string strStandardDevice;
				getStandardDevice(iterPair->second, strStandardDevice);
				if(strStandardDevice.find("/dev") == string::npos)
					strStandardDevice = string("/dev/") + strStandardDevice;
				blockDevFlushBuffer(strStandardDevice);
			}
		}
#endif

		if (m_mapOfHostToRecoveryProgress.find(HostName) != m_mapOfHostToRecoveryProgress.end())
		{
			RecStatus = m_mapOfHostToRecoveryProgress.find(HostName)->second;
			if (RecStatus.recoverystatus == TAG_VERIFIED)
			{
#ifdef WIN32
				if (!usingDiskBasedProtection())
				{
					//Logic to assign mount points if removed as part previous cdpcli rollback failure
					if (!AssignMountPointToTgtVols(HostName))
					{
						//Ignoring failure case. need to handle later
						DebugPrintf(SV_LOG_WARNING, "Assigning mount points failed for host - %s\n", HostName.c_str());
					}
				}
#endif
				if (GenerateProtectionPairList(HostName, mapProtectedPairs, RecStatus.tagname, RecStatus.tagtype))
				{
					DebugPrintf(SV_LOG_DEBUG, "Successfully generated new protection pairs list for host - %s\n", HostName.c_str());
				}
				Value = RecStatus.tagname;
				Type = RecStatus.tagtype;
			}
		}		

        //Get Vector of Retention DBs for all replicated
        std::vector<std::string> vRetDbPaths;
		std::map<std::string, std::string> mTgtVolToRetDb;
        if(false == GetRetentionDBPaths(mapProtectedPairs, vRetDbPaths, mTgtVolToRetDb))
        {
            DebugPrintf(SV_LOG_ERROR,"Failed to get Retention Database Paths of the protected/Snapshot Pairs for the VM - %s\n", HostName.c_str());
			statInfo.ErrorMsg = "Unable to query the retention history for the protected item " + strSrcHostName;
			statInfo.ErrorCode = "EA0605";
            rv = false; break;
        }        

        //Check for cluster node or not
		std::map<std::string, std::string>::iterator iterClus = m_mapOfVmToClusOpn.find(HostName);
		if(iterClus != m_mapOfVmToClusOpn.end())
		{
			if(iterClus->second.compare("yes") == 0)
			{
				m_bClus = true;
			}
		}

		//Validate rollback is possible and 
        // fetch latest tag/time incase of LATEST/LATESTTIME options
        std::string LatestTagOrTime;
		if(m_bClus && (Value.compare(LATEST_TAG) == 0))
		{
			Type = FS_TAGTYPE;
			Value = "LATEST_TAG";
			//Calling CX API to get latest Tag
			if(false == m_objLin.GetCommonRecoveryPoint(HostName, Value))
			{
				DebugPrintf(SV_LOG_ERROR,"Failed to get the common recovery point for host : %s\n", HostName.c_str());
				statInfo.ErrorMsg = "Unable to find a common consistent recovery point across volumes of the protected item " + strSrcHostName;
				statInfo.ErrorCode = "EA0604";

				for (map<string, string>::iterator iterPair = mapProtectedPairs.begin(); iterPair != mapProtectedPairs.end(); iterPair++)
				{
					CollectCdpCliListEventLogs(iterPair->second);
				}

				rv = false; break;
			}
			RecStatus.tagname = Value;
			RecStatus.tagtype = Type;
		}
		else
		{
			if(false == ValidateAndGetRollbackOptions(vRetDbPaths, Value, Type, LatestTagOrTime))
			{
				DebugPrintf(SV_LOG_ERROR,"Vaildation Failed. Cannot take physical snapshot/Rollback the volumes of VM - %s\n",HostName.c_str());
				statInfo.ErrorMsg = "Unable to find a common consistent recovery point across volumes of the protected item " + strSrcHostName;
				statInfo.ErrorCode = "EA0604";

				for (map<string, string>::iterator iterPair = mapProtectedPairs.begin(); iterPair != mapProtectedPairs.end(); iterPair++)
				{
					CollectCdpCliListEventLogs(iterPair->second);
				}

				rv = false; break;
			}
			if ((Value.compare(LATEST_TAG) == 0) || (Value.compare(LATEST_TIME) == 0))
				RecStatus.tagname = LatestTagOrTime;
			else
				RecStatus.tagname = Value;
			if (Value.compare(LATEST_TIME) == 0)
				RecStatus.tagtype = TIME_TAGTYPE;
			else
				RecStatus.tagtype = Type;
		}

		//Added to avoid adding nanoseconds to the consistent timestamp
		//Bug #4390560
		if ((Type.compare(TIME_TAGTYPE) == 0) && (Value.compare(LATEST_TIME) != 0))
		{
			DebugPrintf(SV_LOG_INFO, "Selected timestamp for recovery - %s\n", Value.c_str());
			CDPUtil Objcdp;
			SV_ULONGLONG timeStamp;
			if (Objcdp.InputTimeToFileTime(Value, timeStamp))
			{
				timeStamp = timeStamp + 1; //Increasing the time by 1 hundrednanoseconds
				if (!Objcdp.ToDisplayTimeOnConsole(timeStamp, Value))
				{
					DebugPrintf(SV_LOG_ERROR, "Failed to construct the new time stamp for VM - %s\n", HostName.c_str());
					DebugPrintf(SV_LOG_ERROR, "Failed to rollback the volumes of VM - %s\n", HostName.c_str());
					statInfo.ErrorMsg = "Unable to find a common consistent recovery point across volumes of the protected item " + strSrcHostName;
					statInfo.ErrorCode = "EA0604";
					rv = false; break;
				}
			}
			DebugPrintf(SV_LOG_INFO, "Newly constructed timestamp for recovery - %s\n", Value.c_str());
		}

		if(m_bPhySnapShot && m_bArraySnapshot)
		{
			std::map<std::string,std::string> mapSnapshotPairs = objVm.getProtectedOrSnapshotDrivesMap(HostName);

			std::map<std::string,std::string> mapProtectedToSnapshotVol;
			if(false == CreateSnapshotPair(mapProtectedPairs, mapSnapshotPairs, mapProtectedToSnapshotVol))
			{
				bool bSucces = false;
#ifdef WIN32
				if(m_bClus)
				{
					mapProtectedToSnapshotVol.clear();
					bSucces = CreateSnapshotPairForClusNode(HostName,mapProtectedPairs, mapSnapshotPairs, mapProtectedToSnapshotVol);
				}
#endif
				if(!bSucces) 
				{
					DebugPrintf(SV_LOG_ERROR,"Failed to get all array based Snapshotted Volumes of VM - %s\n",HostName.c_str());
					DebugPrintf(SV_LOG_ERROR,"Failed to take Array based snapshot for the volumes of VM - %s\n",HostName.c_str());
					statInfo.ErrorMsg = "Master target " + m_strHostName + " unable to identify the target volumes corresponding to the source machine";
					statInfo.ErrorCode = "EA0606";
					rv = false; break;
				}
			}

			//Do the actual rollback
			if(false == ArraySnapshotOnPairsOfVM(mapProtectedToSnapshotVol, Value, Type, LatestTagOrTime, strCdpCliCmd, mTgtVolToRetDb))
			{
				DebugPrintf(SV_LOG_ERROR,"Failed to take Array based snapshot for the volumes of VM - %s\n",HostName.c_str());
				statInfo.ErrorMsg = "Unable to prepare cdpcli recovery command for protected item " + strSrcHostName;
				statInfo.ErrorCode = "EA0607";
				rv = false; break;
			}
		}
		else if(m_bPhySnapShot)
		{
			std::map<std::string,std::string> mapSnapshotPairs;
			if(m_bCloudMT)
			{
				map<string, map<string, string> >::iterator iterHost = m_SnapshotVmsInfo.m_snapshotDisksVolsInfo.m_volumeMapping.find(HostName);
				if(iterHost != m_SnapshotVmsInfo.m_snapshotDisksVolsInfo.m_volumeMapping.end())
				{
					map<string, string>::iterator iterVolPair = iterHost->second.begin();
					for(; iterVolPair != iterHost->second.end(); iterVolPair++)
					{
						mapSnapshotPairs.insert(make_pair(iterVolPair->first, iterVolPair->second));
					}
				}
			}
			else
				mapSnapshotPairs = objVm.getProtectedOrSnapshotDrivesMap(HostName);

			std::map<std::string,std::string> mapProtectedToSnapshotVol;
			if(false == CreateSnapshotPair(mapProtectedPairs, mapSnapshotPairs, mapProtectedToSnapshotVol))
			{
				bool bSucces = false;
#ifdef WIN32
				if(m_bClus)
				{
					mapProtectedToSnapshotVol.clear();
					bSucces = CreateSnapshotPairForClusNode(HostName,mapProtectedPairs, mapSnapshotPairs, mapProtectedToSnapshotVol);
				}
#endif
				if(!bSucces) 
				{
					DebugPrintf(SV_LOG_ERROR,"Failed to get all Snapshotted Volumes of VM - %s\n",HostName.c_str());
					DebugPrintf(SV_LOG_ERROR,"Failed to take physical snapshot for the volumes of VM - %s\n",HostName.c_str());
					statInfo.ErrorMsg = "Master target " + m_strHostName + " unable to identify the target volumes corresponding to the source machine";
					statInfo.ErrorCode = "EA0606";
					rv = false; break;
				}
			}
			//Do the actual snapshot recovery operation.
			if(false == SnapshotOnPairsOfVM(mapProtectedToSnapshotVol, Value, Type, LatestTagOrTime, strCdpCliCmd))
			{
				DebugPrintf(SV_LOG_ERROR,"Failed to take physical snapshot of the volumes of VM - %s\n",HostName.c_str());
				statInfo.ErrorMsg = "Unable to prepare cdpcli recovery command for protected item " + strSrcHostName;
				statInfo.ErrorCode = "EA0607";
				rv = false; break;
			}
		}
		else
		{
			if (mapProtectedPairs.empty())
			{
				DebugPrintf(SV_LOG_ERROR, "Rollback pairs are empty for VM - %s\n", HostName.c_str());
				strCdpCliCmd = "";
			}
			else
			{
				//Do the actual rollback
				if (false == RollbackPairsOfVM(mapProtectedPairs, Value, Type, LatestTagOrTime, strCdpCliCmd))
				{
					DebugPrintf(SV_LOG_ERROR, "Failed to rollback the volumes of VM - %s\n", HostName.c_str());
					statInfo.ErrorMsg = "Unable to prepare cdpcli rollback command for protected item " + strSrcHostName;
					statInfo.ErrorCode = "EA0607";
					rv = false; break;
				}
			}
			
			if (RecStatus.recoverystatus == RECOVERY_STARTED)
			{
				for (map<string, string>::iterator iterPair = mapProtectedPairs.begin(); iterPair != mapProtectedPairs.end(); iterPair++)
				{
					//Locks the tag/time so that it will come in use in case of failure.
					if (!BookMarkTheTag(RecStatus.tagname, iterPair->second, RecStatus.tagtype))
					{
						DebugPrintf(SV_LOG_ERROR, "Failed to lock the tag/time [%s] for VM - %s\n", RecStatus.tagname.c_str(), HostName.c_str());
						//Currently ignoring the failure case. Need to discuss
					}
				}
			}
		}

		//Updating the recovery progress as tag verifiedand updating the tag name also.
		RecStatus.recoverystatus = TAG_VERIFIED;
		if (!UpdateRecoveryProgress(HostName, m_strDatafolderPath, RecStatus))
		{
			DebugPrintf(SV_LOG_ERROR, "Failed to update the recovery progress in the local file for host - %s.\n", HostName.c_str());
			//Currently ignoring the failure case.
		}

#ifdef WIN32
        //Do the chkdsk in case of windows
		if (m_bExtraChecks && !usingDiskBasedProtection())
        {
            if(false == ChkdskVolumes(mapProtectedPairs))
            {
                DebugPrintf(SV_LOG_ERROR,"Failed to do chkdsk on the volumes of VM - %s\n",HostName.c_str());
                rv = false; break;
            }
        }
#endif  /* WIN32 */
    }while(0);

    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return rv;
}

// This function returns the vector of Retention DB paths for the given Protected Pairs.
bool RollBackVM::GetRetentionDBPaths(std::map<std::string,std::string> mapProtectedPairs, 
                                     std::vector<std::string> & vRetDbPaths, std::map<std::string, std::string>& mTgtVolToRetDb)
{
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    bool rv = true;
    std::map<std::string,std::string>::iterator iterPairs = mapProtectedPairs.begin();       
    for(; iterPairs != mapProtectedPairs.end(); iterPairs++)
    {
        std::string TgtVol = iterPairs->second;
        std::string strRetentionLogPath;
        if(false == getRetentionSettings(TgtVol,strRetentionLogPath))
        {
            DebugPrintf(SV_LOG_ERROR,"Failed to get retention log path for %s\n",TgtVol.c_str());
	        rv = false; break;
        }
        if(strRetentionLogPath.empty()) 
        {
            DebugPrintf(SV_LOG_ERROR,"Retention path is not detected for %s\n",TgtVol.c_str());
			rv = false; break;
        }
        DebugPrintf(SV_LOG_INFO,"Volume = %s  : RetentionLogPath = %s", TgtVol.c_str(), strRetentionLogPath.c_str()); 
		mTgtVolToRetDb.insert(make_pair(TgtVol, strRetentionLogPath));
        vRetDbPaths.push_back(strRetentionLogPath);        
    }
    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return rv;
}

//Checks retention db path existance for the input volume
//True - If exist
//False - If doesnot exist
bool RollBackVM::IsRetentionDBPathExist(const std::string& strVolume, const std::string& strValue, const std::string& strType)
{
	DebugPrintf(SV_LOG_DEBUG, "Entering %s\n", __FUNCTION__);
	bool rv = true;

	std::string strRetentionLogPath;
	if (false == getRetentionSettings(strVolume, strRetentionLogPath))
	{
		DebugPrintf(SV_LOG_ERROR, "Failed to get retention log path for %s\n", strVolume.c_str());
		rv = false;
	}
	else
	{
		if (strRetentionLogPath.empty())
		{
			DebugPrintf(SV_LOG_ERROR, "Retention path is not detected for %s\n", strVolume.c_str());
			rv = false;
		}
		else
		{
			std::vector<std::string> vRetDbPaths;
			vRetDbPaths.push_back(strRetentionLogPath);
			if (strType.compare(FS_TAGTYPE) == 0)   // Specified tag (FS tag)
			{
				if (false == ValidateSpecificTag(vRetDbPaths, strType, strValue))
					rv = false;
			}
			else if (strType.compare(TIME_TAGTYPE) == 0) // Specified time (TIME )
			{
				if (false == ValidateSpecificTime(vRetDbPaths, strValue)) //here Value is Time
					rv = false;
			}
			else
				rv = false;
		}
	}

	DebugPrintf(SV_LOG_DEBUG, "Exiting %s\n", __FUNCTION__);
	return rv;
}


// Validate the rollback options and decide if rollback if possible.
// And in case of LATEST and LATESTTIME , get tag and time correspondingly.
// Supported cases for now - LATEST tag , Specified tag (FS type)
//                           LATESTTIME , Any common time
// Irrespective of type, LATEST => FS type...even if anything else is there it will be ignored.
// Irrespective of type, LATESTTIME => independant..can be anything...will be ignored.
// Output - Fill LatestTagOrTime with LATEST tag/LATESTTIME values based on options.
bool RollBackVM::ValidateAndGetRollbackOptions(std::vector<std::string> vRetDbPaths, 
                                               std::string Value, 
                                               std::string Type,
                                               std::string & LatestTagOrTime)
{
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    bool rv = true;

    if(Value.compare(LATEST_TAG) == 0)       // LATEST_TAG
    {
		if(false == ValidateLatestTag(vRetDbPaths, Type, LatestTagOrTime))
		   rv = false;
	}
    else if(Value.compare(LATEST_TIME) == 0) // LATEST_TIME 
    {
        if(false == ValidateLatestTime(vRetDbPaths, LatestTagOrTime))
            rv = false;
    }
    else if(Type.compare(FS_TAGTYPE) == 0)   // Specified tag (FS tag)
    {
        if(false == ValidateSpecificTag(vRetDbPaths, Type, Value))
            rv = false;
    }
    else if(Type.compare(TIME_TAGTYPE) == 0) // Specified time (TIME )
    {
        if(false == ValidateSpecificTime(vRetDbPaths, Value)) //here Value is Time
            rv = false;
    }
    else    //Unsupported config.
    {
        DebugPrintf(SV_LOG_ERROR,"Unsupported Config - %s,%s\n", Value.c_str(), Type.c_str());
        rv = false;
    }

    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return rv;
}

// This function is for doing the Rollback of given volumes/disks(linux) based on given options.
bool RollBackVM::RollbackPairsOfVM(std::map<std::string, std::string> mapProtectedPairs,
                                   std::string Value,
                                   std::string Type,
                                   std::string LatestTagOrTime,
								   std::string& strCdpCliCmd)
{
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    bool rv = true;

    std::string CdpcliCmd = CDPCLI_BINARY + string(" --rollback --rollbackpairs=");

    std::map<std::string, std::string>::iterator iter = mapProtectedPairs.begin();
    for( ; iter != mapProtectedPairs.end(); iter++)
    {
        CdpcliCmd += iter->second + string(",,;"); //empty mount point and retention db path
    }

    if(Value.compare(LATEST_TAG) == 0)       // LATEST_TAG
    {        
		CdpcliCmd += string(" --recentfsconsistentpoint");
    }
    else if(Value.compare(LATEST_TIME) == 0) // LATEST_TIME 
    {
		CdpcliCmd += string(" --recentcrashconsistentpoint");
    }
    else if(Type.compare(FS_TAGTYPE) == 0)   // Specified tag (FS tag)
    {
        CdpcliCmd += string(" --app=") + Type + string(" --event=") + Value + string(" --picklatest");
    }
    else if(Type.compare(TIME_TAGTYPE) == 0) // Specified time (TIME )
    {
		/*Value	+= string(":00:00:01");
		DebugPrintf(SV_LOG_INFO ," Time after padding Milli, Micro and Nano seconds = %s ", Value.c_str() );*/
#ifdef WIN32
        CdpcliCmd += string(" --recentcrashconsistentpoint") + string(" --timerange=\",") + Value + string("\"");
#else
		CdpcliCmd += string(" --recentcrashconsistentpoint") + string(" --timerange=,") + Value;
#endif
    }

	CdpcliCmd = CdpcliCmd + string(" --deleteretentionlog=yes --loglevel=") + boost::lexical_cast<std::string>(SV_LOG_ALWAYS) +
		string(" --logpath=\"") + m_strDatafolderPath + DIRECTORY_SEPARATOR + m_VmName + string("_cdpcli_") + GetCurrentLocalTime() + string(".log\"");

	string installLocation (getVxAgentInstallPath());
	
	if (installLocation.empty()) 
	{
		DebugPrintf(SV_LOG_ERROR,"Could not determine the install location for the VX agent. Could not find FailoverServices.conf file\n");
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;
	}

#ifdef WIN32
	strCdpCliCmd = installLocation + "\\" + CdpcliCmd ;
#else
	strCdpCliCmd = installLocation + string("/bin/") + CdpcliCmd ;//Change for Linux VM
#endif

	DebugPrintf(SV_LOG_INFO,"\n");
    DebugPrintf(SV_LOG_INFO,"CDPCLI command to execute: %s\n", strCdpCliCmd.c_str());
   

    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return rv;
}

// This function is for taking the physical snapshot of given volumes/disks(linux) based on given options.
bool RollBackVM::SnapshotOnPairsOfVM(std::map<std::string, std::string> mapProtectedToSnapshotVol,
                                   std::string Value,
                                   std::string Type,
                                   std::string LatestTagOrTime,
								   std::string& strCdpCliCmd)
{
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    bool rv = true;

	std::string CdpcliCmd = CDPCLI_BINARY + string(" --recover --recoverypairs=");

    std::map<std::string, std::string>::iterator iter = mapProtectedToSnapshotVol.begin();
    for( ; iter != mapProtectedToSnapshotVol.end(); iter++)
    {
		CdpcliCmd += iter->first + string(",") + iter->second + string(";"); //empty mount point and retention db path
    }

    if(Value.compare(LATEST_TAG) == 0)       // LATEST_TAG
    {        
		CdpcliCmd += string(" --recentfsconsistentpoint");
    }
    else if(Value.compare(LATEST_TIME) == 0) // LATEST_TIME 
    {
		CdpcliCmd += string(" --recentcrashconsistentpoint");
    }
    else if(Type.compare(FS_TAGTYPE) == 0)   // Specified tag (FS tag)
    {
        CdpcliCmd += string(" --app=") + Type + string(" --event=") + Value + string(" --picklatest");
    }
    else if(Type.compare(TIME_TAGTYPE) == 0) // Specified time (TIME )
    {
		/*Value	+= string(":00:00:01");
		DebugPrintf(SV_LOG_INFO ," Time after padding Milli, Micro and Nano seconds = %s ", Value.c_str() );*/
#ifdef WIN32
        CdpcliCmd += string(" --recentcrashconsistentpoint") + string(" --timerange=\",") + Value + string("\"");
#else
		CdpcliCmd += string(" --recentcrashconsistentpoint") + string(" --timerange=,") + Value;
#endif
    }

	CdpcliCmd = CdpcliCmd + string(" --loglevel=") + boost::lexical_cast<std::string>(SV_LOG_ALWAYS) +
		string(" --logpath=\"") + m_strDatafolderPath + DIRECTORY_SEPARATOR + m_VmName + string("_cdpcli_") + GetCurrentLocalTime() + string(".log\"");

	string installLocation (getVxAgentInstallPath());
	
	if (installLocation.empty()) 
	{
		DebugPrintf(SV_LOG_ERROR,"Could not determine the install location for the VX agent. Could not find FailoverServices.conf file\n");
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;
	}

#ifdef WIN32
	strCdpCliCmd = installLocation + "\\" + CdpcliCmd ;
#else
	strCdpCliCmd = installLocation + string("/bin/") + CdpcliCmd ;//Change for Linux VM
#endif

	DebugPrintf(SV_LOG_INFO,"\n");
    DebugPrintf(SV_LOG_INFO,"CDPCLI command to execute: %s\n", strCdpCliCmd.c_str());


    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return rv;
}


// This function is for preparing the cdpcli command for the array based snapshot of given volumes/disks(linux) based on given options.
bool RollBackVM::ArraySnapshotOnPairsOfVM(std::map<std::string, std::string> mapProtectedToSnapshotVol,
                                   std::string Value,
                                   std::string Type,
                                   std::string LatestTagOrTime,
								   std::string& strCdpCliCmd,
								   std::map<std::string, std::string> mTgtVolToRetDb)
{
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    bool rv = true;

    std::string CdpcliCmd = CDPCLI_BINARY + string(" --rollback --rollbackpairs=");

    std::map<std::string, std::string>::iterator iter = mapProtectedToSnapshotVol.begin();
    for( ; iter != mapProtectedToSnapshotVol.end(); iter++)
    {
		std::map<std::string, std::string>::iterator iterMap = mTgtVolToRetDb.find(iter->first);
		if(iterMap == mTgtVolToRetDb.end())
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to get the retention db for target volume %s for which corresponding snapshot volume %s\n",iter->first.c_str(), iter->second.c_str());
			continue;
		}
		CdpcliCmd += iter->second + string(",,") + iterMap->second + string(";"); //empty mount point
    }

    if(Value.compare(LATEST_TAG) == 0)       // LATEST_TAG
    {
		CdpcliCmd += string(" --recentfsconsistentpoint");
    }
    else if(Value.compare(LATEST_TIME) == 0) // LATEST_TIME 
    {
		CdpcliCmd += string(" --recentcrashconsistentpoint");
    }
    else if(Type.compare(FS_TAGTYPE) == 0)   // Specified tag (FS tag)
    {
        CdpcliCmd += string(" --app=") + Type + string(" --event=") + Value + string(" --picklatest");        
    }
    else if(Type.compare(TIME_TAGTYPE) == 0) // Specified time (TIME )
    {
		/*Value	+= string(":00:00:01");
		DebugPrintf(SV_LOG_INFO ," Time after padding Milli, Micro and Nano seconds = %s ", Value.c_str() );*/
#ifdef WIN32
        CdpcliCmd += string(" --recentcrashconsistentpoint") + string(" --timerange=\",") + Value + string("\"");
#else
		CdpcliCmd += string(" --recentcrashconsistentpoint") + string(" --timerange=,") + Value;
#endif
    }

	CdpcliCmd = CdpcliCmd + string(" --skipchecks --loglevel=") + boost::lexical_cast<std::string>(SV_LOG_ALWAYS) +
		string(" --logpath=\"") + m_strDatafolderPath + DIRECTORY_SEPARATOR + m_VmName + string("_cdpcli_") + GetCurrentLocalTime() + string(".log\"");

	string installLocation (getVxAgentInstallPath());
	
	if (installLocation.empty()) 
	{
		DebugPrintf(SV_LOG_ERROR,"Could not determine the install location for the VX agent. Could not find FailoverServices.conf file\n");
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;
	}

#ifdef WIN32
	strCdpCliCmd = installLocation + "\\" + CdpcliCmd ;
#else
	strCdpCliCmd = installLocation + string("/bin/") + CdpcliCmd ;//Change for Linux VM
#endif

	DebugPrintf(SV_LOG_INFO,"\n");
    DebugPrintf(SV_LOG_INFO,"CDPCLI command to execute: %s\n", strCdpCliCmd.c_str());
   

    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return rv;
}


// This function is for validating LATEST tag option
// To validate, check if atleast one common tag is found for given retention db paths
bool RollBackVM::ValidateLatestTag(std::vector<std::string> vRetDbPaths, std::string TagType, std::string & LatestTagOrTime)
{
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    bool rv = true;

    CDPEvent commonevent;
   	SV_EVENT_TYPE type;
    VacpUtil::AppNameToTagType(TagType, type);
	//After & before time of the tag are taken as zero, which defaults to any common tag irrespective of time bounds				  
    bool result =  CDPUtil::MostRecentConsistentPoint(vRetDbPaths, commonevent,type, 0, 0);    
    if (result == true) 
	{
        LatestTagOrTime = commonevent.c_eventvalue;
        DebugPrintf(SV_LOG_INFO,"The common latest tag for the protected volumes is : %s\n",LatestTagOrTime.c_str());		
	}
	else 
	{
        DebugPrintf(SV_LOG_ERROR,"Could not find a common tag for the protected volumes with tag type - %s\n", TagType.c_str());
		rv = false;
    }

    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return rv;
}

// This function is for validating LATEST time option
// To validate, check if a common time is found for given retention db paths
bool RollBackVM::ValidateLatestTime(std::vector<std::string> vRetDbPaths, std::string & LatestTagOrTime)
{
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    bool rv = true;

    do
    {
        SV_ULONGLONG ts;
        SV_ULONGLONG seq;
        string usertime;
        bool result = CDPUtil::MostRecentCCP(vRetDbPaths,ts,seq,0,0);
        if (result == true) 
        {
            if(!CDPUtil::ToDisplayTimeOnConsole(ts,usertime))
            {
               DebugPrintf(SV_LOG_ERROR,"Failed to to convert the time to user time.\n");
               rv = false; break;
            }
            DebugPrintf(SV_LOG_INFO,"The Common Latest Time between the protected volumes is : %s\n", usertime.c_str());
			LatestTagOrTime = usertime;
        }
        else 
        {
            DebugPrintf(SV_LOG_ERROR,"Could not find a Common Time for the protected volumes.\n");
            rv = false; break;
        }
    }while(0);

    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return rv;
}

// This function is for validating SPECIFIC tag option
// To validate, check if the specified common tag is found
bool RollBackVM::ValidateSpecificTag(std::vector<std::string> vRetDbPaths, std::string TagType, std::string TagValue)
{
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    bool rv = true;
    
    std::string identifier;
    SV_ULONGLONG ts;
   	SV_EVENT_TYPE type;    
    VacpUtil::AppNameToTagType(TagType, type);    
	
    bool result = CDPUtil::FindCommonEvent(vRetDbPaths, type, TagValue, identifier, ts);    
    if (result == true) 
	{        
        DebugPrintf(SV_LOG_INFO,"The specified tag for the protected volumes exists. Specified Tag -  %s\n", TagValue.c_str());		
	}
	else 
	{
        DebugPrintf(SV_LOG_ERROR,"The specified tag for the protected volumes does not exist. Specified Tag -  %s\n", TagValue.c_str());
		rv = false;
    }

    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return rv;
}

// This function is for validating SPECIFIC time option
// To validate, check if the specified common time is consistent or not.
bool RollBackVM::ValidateSpecificTime(std::vector<std::string> vRetDbPaths, std::string TimeValue)
{
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    bool rv = true;

    do
    {
        SV_ULONGLONG ts;
        if(false == CDPUtil::InputTimeToFileTime(TimeValue, ts))
        {
            DebugPrintf(SV_LOG_ERROR,"Failed to convert the Specified Time : %s to required format.\n", TimeValue.c_str());
            rv = false; break;
        }
        bool isConsistent = false;
		bool isavailable = false;
        if(!CDPUtil::isCCP(vRetDbPaths,ts,isConsistent,isavailable))
        {
            DebugPrintf(SV_LOG_ERROR,"Failed to check whether the Specified Time : %s is Consistent or Not.\n", TimeValue.c_str());
            rv = false; break;
        }
        if(isConsistent)
        {
            DebugPrintf(SV_LOG_INFO,"The Specified Time : %s is Consistent.\n", TimeValue.c_str());             
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR,"The Specified Time : %s is NOT Consistent.\n", TimeValue.c_str());                     
            rv = false; break;
        }
    }while(0);

    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return rv;
}

vector<string> RollBackVM::parseCsvFile(const string& strLineRead)
{
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    typedef string::const_iterator iter;
    vector<string> vectorOfStringTokens;
    iter i =  strLineRead.begin();
    while (i != strLineRead.end())
    {
        // ignore leading commas
        i = std::find_if(i, strLineRead.end(), not_comma);
        // find end of next word
        iter j = std::find_if(i, strLineRead.end(),isComma);
        // copy the characters in [i, j)
        if (i != strLineRead.end()) 
            vectorOfStringTokens.push_back(string(i, j));
        i = j;
    }
    DebugPrintf(SV_LOG_INFO,"size of token Vector =%d\n ",vectorOfStringTokens.size());
    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return vectorOfStringTokens;
}

bool RollBackVM::GetHostNameHostIdFromRecoveryInfo(std::map<std::string,std::map<std::string,std::string> > &recoveryInfo)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool bReturn = true;
	std::map<std::string,std::map<std::string,std::string> >::iterator iterHosRcvrInfo = recoveryInfo.begin();
	while(iterHosRcvrInfo != recoveryInfo.end())
	{
		std::map<std::string,std::string>::iterator iterHostName = iterHosRcvrInfo->second.find("HostName"),
			iterHostId = iterHosRcvrInfo->second.find("HostId");
		if(iterHostId != iterHosRcvrInfo->second.end() && iterHostName != iterHosRcvrInfo->second.end())
		{
			m_mapOfVmNameToId[iterHostId->second] = iterHostName->second;
		}
		else
		{
			bReturn = false;
			DebugPrintf(SV_LOG_ERROR,"Cloud not find the HostId and HostName properties in recovery information of %s\n",iterHosRcvrInfo->first.c_str());
		}
		iterHosRcvrInfo++;
	}
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bReturn;
}


/*================================================================================================================
This will read the rollback details from config file and unserializes that info and fills the required structures.
==================================================================================================================*/
bool RollBackVM::GetRollbackDetailsFromConfigFile(StatusInfo& statInfoOveral)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool bReturn = true;

	do
	{
		string strUnserializeData = "";
		std::stringstream strbuf;
		std::ifstream inFile(m_strConfigFile.c_str());
		if (!inFile.is_open())
		{
			DebugPrintf(SV_LOG_ERROR, "Failed to read the file - %s\n", m_strConfigFile.c_str());
			statInfoOveral.Status = "2";
			statInfoOveral.ErrorCode = "EA0612";
			statInfoOveral.ErrorMsg = "Failover failed with an internal error on Master Target " +  m_strHostName;
			statInfoOveral.PlaceHolder.Properties[MT_HOSTNAME] = m_strHostName;

			DebugPrintf(SV_LOG_DEBUG, "Exiting %s\n", __FUNCTION__);
			bReturn = false;
			break;
		}
		strbuf << inFile.rdbuf();
		strUnserializeData = strbuf.str();
		DebugPrintf(SV_LOG_INFO, "Serialized Data : %s.\n", strUnserializeData.c_str());
		try
		{
			if (m_bPhySnapShot)
			{
				m_SnapshotVmsInfo = unmarshal<VMsCloneInfo>(strUnserializeData);
				m_RecVmsInfo.m_diskSignatureMapping.insert(m_SnapshotVmsInfo.m_protectedDisksVolsInfo.m_diskSignatureMapping.begin(), m_SnapshotVmsInfo.m_protectedDisksVolsInfo.m_diskSignatureMapping.end());
				m_RecVmsInfo.m_volumeMapping.insert(m_SnapshotVmsInfo.m_protectedDisksVolsInfo.m_volumeMapping.begin(), m_SnapshotVmsInfo.m_protectedDisksVolsInfo.m_volumeMapping.end());
				m_RecVmsInfo.m_mapVMRecoveryInfo.insert(m_SnapshotVmsInfo.m_mapVMRecoveryInfo.begin(), m_SnapshotVmsInfo.m_mapVMRecoveryInfo.end());
			}
			else
			{
				m_RecVmsInfo = unmarshal<VMsRecoveryInfo>(strUnserializeData);
			}
			GetHostNameHostIdFromRecoveryInfo(m_RecVmsInfo.m_mapVMRecoveryInfo);
		}
		catch (std::exception & e)
		{
			DebugPrintf(SV_LOG_ERROR, "Failed while unserializing string = %s\n ", strUnserializeData.c_str());
			DebugPrintf(SV_LOG_ERROR, "Error : %s\n", e.what());
			statInfoOveral.Status = "2";
			statInfoOveral.ErrorCode = "EA0601";
			statInfoOveral.ErrorMsg = "Internal error occurred on Master target " + m_strHostName + " failed with Internal error.";
			statInfoOveral.PlaceHolder.Properties[MT_HOSTNAME] = m_strHostName;
			bReturn = false;
			break;
		}
		catch (...)
		{
			DebugPrintf(SV_LOG_ERROR, "Failed while unserializing string = %s\n ", strUnserializeData.c_str());
			statInfoOveral.Status = "2";
			statInfoOveral.ErrorCode = "EA0601";
			statInfoOveral.ErrorMsg = "Internal error occurred on Master target " + m_strHostName + " failed with Internal error.";
			statInfoOveral.PlaceHolder.Properties[MT_HOSTNAME] = m_strHostName;
			bReturn = false;
			break;
		}

		map<string, string>::iterator iterMap;
		map<std::string, map<string, string> >::iterator iter = m_RecVmsInfo.m_mapVMRecoveryInfo.begin();
		for (; iter != m_RecVmsInfo.m_mapVMRecoveryInfo.end(); iter++)
		{
			m_mapOfVmToClusOpn.insert(make_pair(iter->first, string("no")));
			map<string, string>::iterator iterTag;
			string strTagName, strTagType;

			iterTag = iter->second.find("TagName");
			if (iterTag != iter->second.end())
			{
				strTagName = iterTag->second;
				iterTag = iter->second.find("TagType");
				if (iterTag != iter->second.end())
				{
					strTagType = iterTag->second;
				}
				else
				{
					//Need to take proper action in this case
					continue;
				}
			}
			else
			{
				//Need to take proper action in this case
				continue;
			}
			DebugPrintf(SV_LOG_INFO, "Policy Info :: Host ID : %s , TagName : %s , TagType : %s.\n", iter->first.c_str(), strTagName.c_str(), strTagType.c_str());
			rollbackDetails.insert(make_pair(iter->first, make_pair(strTagName, strTagType)));
		}

		string strErrorMsg;
#ifdef WIN32
		if (!usingDiskBasedProtection())
		{
			if (!generateMapOfVmToSrcAndTgtDiskNmbrs(strErrorMsg))
			{
				DebugPrintf(SV_LOG_ERROR, "Failed to prepare source and target disk number mapping.\n");
				statInfoOveral.Status = "2";
				statInfoOveral.ErrorCode = "EA0602";
				statInfoOveral.ErrorMsg = strErrorMsg;
				statInfoOveral.PlaceHolder.Properties[MT_HOSTNAME] = m_strHostName;
				bReturn = false;
				break;
			}
		}
#else
		if (!createMapOfVmToSrcAndTgtDisk(strErrorMsg))
		{
			DebugPrintf(SV_LOG_ERROR, "Failed to prepare source and target disk number mapping.\n");
			statInfoOveral.Status = "2";
			statInfoOveral.ErrorCode = "EA0602";
			statInfoOveral.ErrorMsg = strErrorMsg;
			statInfoOveral.PlaceHolder.Properties[MT_HOSTNAME] = m_strHostName;
			bReturn = false;
			break;
		}
#endif
	} while (0);

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bReturn;
}


bool RollBackVM::GenerateSrcDiskToTgtScsiIDFromXml(map<string, map<string, string> >& mapHostTomapOfDiskDevIdToNum)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool bRet = true;
	std::string XmlFileName;

	if((m_strOperation.compare("volumeremove") == 0) || (m_strOperation.compare("diskremove") == 0))
		XmlFileName = m_strDatafolderPath + std::string(VOL_DISK_REMOVE_FILE_NAME);
	else if(m_bPhySnapShot)
		XmlFileName = m_strDatafolderPath + std::string(SNAPSHOT_FILE_NAME);
	else
		XmlFileName = m_strDatafolderPath + std::string(RECOVERY_FILE_NAME);

    DebugPrintf(SV_LOG_INFO,"XML File - %s\n", XmlFileName.c_str());

	string strPhysicalDeviceName = "\\\\.\\PHYSICALDRIVE";
	map<string, string> mapOfSrcScsiIdAndSrcdevFromXml;

	xmlDocPtr xDoc;	
	xmlNodePtr xCur;

#ifdef WIN32
    ChangeVmConfig obj;
#else
    LinuxReplication obj;
#endif

	xDoc = xmlParseFile(XmlFileName.c_str());
	if (xDoc == NULL)
	{
		DebugPrintf(SV_LOG_ERROR,"XML document is not Parsed successfully. %s\n", XmlFileName.c_str());
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;
	}

	xCur = xmlDocGetRootElement(xDoc);
	if (xCur == NULL)
	{
		DebugPrintf(SV_LOG_ERROR,"%s is empty. Cannot Proceed further.\n", XmlFileName.c_str());
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		xmlFreeDoc(xDoc);
		return false;
	}

	if (xmlStrcmp(xCur->name,(const xmlChar*)"root")) 
	{
		DebugPrintf(SV_LOG_ERROR,"The %s document is not in expected format : <root> entry not found\n", XmlFileName.c_str());
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		xmlFreeDoc(xDoc);
		return false;
	}

	xCur = obj.xGetChildNode(xCur,(const xmlChar*)"SRC_ESX");
	if (xCur == NULL)
	{
		DebugPrintf(SV_LOG_ERROR,"The %s document is not in expected format : <SRC_ESX> entry not found\n", XmlFileName.c_str());
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		xmlFreeDoc(xDoc);
		return false;			
	}

	//Parse through child nodes and if they are "host" , fetch rollback details.
    for(xmlNodePtr xChild = xCur->children; xChild != NULL; xChild = xChild->next)
	{
        if (!xmlStrcasecmp(xChild->name, (const xmlChar*)"host"))
		{
			std::string strVmId;
            std::string strVmName;

            xmlChar *attr_value_temp;

			attr_value_temp = xmlGetProp(xChild,(const xmlChar*)"hostname");
			if (attr_value_temp == NULL)
			{
				DebugPrintf(SV_LOG_ERROR,"The XML document is not in expected format : <host hostname> entry not found\n");
				DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
				xmlFreeDoc(xDoc); return false;
			}
			strVmName = std::string((char *)attr_value_temp);

			attr_value_temp = xmlGetProp(xChild,(const xmlChar*)"inmage_hostid");
			if (attr_value_temp == NULL)
			{
				DebugPrintf(SV_LOG_ERROR,"The XML document is not in expected format : <host inmage_hostid> entry not found\n");
				strVmId = strVmName;
			}
			else
			{
				strVmId = std::string((char *)attr_value_temp);
				if(strVmId.empty())
					strVmId = strVmName;
			}

			mapOfSrcScsiIdAndSrcdevFromXml.clear();
			//processing each disk to get the scsi id to disk number mapping
			for(xmlNodePtr xChild1 = xChild->children; xChild1 != NULL; xChild1 = xChild1->next)
			{
				if (!xmlStrcasecmp(xChild1->name, (const xmlChar*)"disk"))
				{
					string strSrcDevice, strSrcDeviceId;
					
					attr_value_temp = xmlGetProp(xChild1,(const xmlChar*)"src_devicename");
					if (attr_value_temp == NULL)
					{
						DebugPrintf(SV_LOG_ERROR,"The XML document is not in expected format : <disk src_devicename> entry not found\n");
						bRet = false;
						break;
					}

					strSrcDevice = string((char *)attr_value_temp);
					strSrcDevice = strSrcDevice.substr(strPhysicalDeviceName.size());

					attr_value_temp = xmlGetProp(xChild1,(const xmlChar*)"scsi_mapping_host");
					if (attr_value_temp == NULL)
					{
						DebugPrintf(SV_LOG_ERROR,"The XML document is not in expected format : <disk scsi_mapping_host> entry not found\n");
						bRet = false;
						break;
					}
					strSrcDeviceId = string((char *)attr_value_temp);

					DebugPrintf(SV_LOG_INFO,"From Xml file got the Source Device : %s <==> Source ScsiId : %s\n", strSrcDevice.c_str(), strSrcDeviceId.c_str());

					mapOfSrcScsiIdAndSrcdevFromXml.insert(make_pair(strSrcDeviceId, strSrcDevice));
				}
			}
			if(!mapOfSrcScsiIdAndSrcdevFromXml.empty())
				mapHostTomapOfDiskDevIdToNum.insert(make_pair(strVmId, mapOfSrcScsiIdAndSrcdevFromXml));
		}
	}
	xmlFreeDoc(xDoc);

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bRet;
}


//***************************************************************************************
// Fetch rollback details from XML provided by UI.
/*
	Sample Hierarchy of XML file
	<root CX_IP="10.0.15.153">
		<SRC_ESX>
			<host hostname="WIN2k8-64" tag="LATEST" tagType="FS" ...>					
                .
				.
			</host>
            <host hostname="CWIN2k8-32" tag="LATESTTIME" tagType="TIME" ...>
                .
                .
            </host>
		</SRC_ESX>
		.
		.
	</root>
*/
//	Output - rollbackDetails ==> map[strVmName, pair(strTagValue,strTagType)];
//	Return Value - true on success and false on failure
//***************************************************************************************
bool RollBackVM::xGetHostDetailsFromXML()
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);

	std::string XmlFileName;
	m_mapClusterNameToNodes.clear();
	m_mapOfVmToClusOpn.clear();

	if((m_strOperation.compare("volumeremove") == 0) || (m_strOperation.compare("diskremove") == 0))
		XmlFileName = m_strDatafolderPath + std::string(VOL_DISK_REMOVE_FILE_NAME);
	else if(m_bPhySnapShot)
		XmlFileName = m_strDatafolderPath + std::string(SNAPSHOT_FILE_NAME);
	else
		XmlFileName = m_strDatafolderPath + std::string(RECOVERY_FILE_NAME);

    DebugPrintf(SV_LOG_INFO,"XML File - %s\n", XmlFileName.c_str());

	xmlDocPtr xDoc;	
	xmlNodePtr xCur;	

	xDoc = xmlParseFile(XmlFileName.c_str());
	if (xDoc == NULL)
	{
		DebugPrintf(SV_LOG_ERROR,"XML document is not Parsed successfully.\n");
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;
	}

	//Found the doc. Now process it.
	xCur = xmlDocGetRootElement(xDoc);
	if (xCur == NULL)
	{
        DebugPrintf(SV_LOG_ERROR,"The XML document is empty. Cannot Proceed further.\n");
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
        xmlFreeDoc(xDoc); return false;
	}

	if (xmlStrcasecmp(xCur->name,(const xmlChar*)"root")) 
	{
		//root is not found
		DebugPrintf(SV_LOG_ERROR,"The XML document is not in expected format : <root> entry not found\n");
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		xmlFreeDoc(xDoc); return false;
	}

	//If you are here root entry is found. Go for SRC_ESX entry.
#ifdef WIN32
    ChangeVmConfig obj;
#else
    LinuxReplication obj;
#endif /* WIN32 */
	xCur = obj.xGetChildNode(xCur,(const xmlChar*)"SRC_ESX");
	if (xCur == NULL)
	{
		DebugPrintf(SV_LOG_ERROR,"The XML document is not in expected format : <SRC_ESX> entry not found\n");
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		xmlFreeDoc(xDoc); return false;			
	}

    //Parse through child nodes and if they are "host" , fetch rollback details.
    for(xmlNodePtr xChild = xCur->children; xChild != NULL; xChild = xChild->next)
	{
        if (!xmlStrcasecmp(xChild->name, (const xmlChar*)"host"))
		{
			std::string strVmId;
            std::string strVmName;
            std::string strTagValue;
            std::string strTagType;
			std::string strCluster;

            xmlChar *attr_value_temp;

			attr_value_temp = xmlGetProp(xChild,(const xmlChar*)"hostname");
			if (attr_value_temp == NULL)
			{
				DebugPrintf(SV_LOG_ERROR,"The XML document is not in expected format : <host hostname> entry not found\n");
				DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
				xmlFreeDoc(xDoc); return false;
			}
			strVmName = std::string((char *)attr_value_temp);

			attr_value_temp = xmlGetProp(xChild,(const xmlChar*)"inmage_hostid");
			if (attr_value_temp == NULL)
			{
				DebugPrintf(SV_LOG_ERROR,"The XML document is not in expected format : <host inmage_hostid> entry not found\n");
				strVmId = strVmName;
			}
			else
			{
				strVmId = std::string((char *)attr_value_temp);
				if(strVmId.empty())
					strVmId = strVmName;
			}

            attr_value_temp = xmlGetProp(xChild,(const xmlChar*)"tag");
            if (attr_value_temp == NULL)
            {
                DebugPrintf(SV_LOG_ERROR,"The XML document is not in expected format : <host tag> entry not found\n");
	            DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		        xmlFreeDoc(xDoc); return false;
            }
            strTagValue = std::string((char *)attr_value_temp);

            attr_value_temp = xmlGetProp(xChild,(const xmlChar*)"tagType");
            if (attr_value_temp == NULL)
            {
                DebugPrintf(SV_LOG_ERROR,"The XML document is not in expected format : <host tagType> entry not found\n");
	            DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		        xmlFreeDoc(xDoc); return false;
            }
            strTagType = std::string((char *)attr_value_temp);

			attr_value_temp = xmlGetProp(xChild,(const xmlChar*)"cluster");
            if (attr_value_temp == NULL)
            {
                DebugPrintf(SV_LOG_ERROR,"The XML document is not in expected format : <host cluster> entry not found\n");
	            DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		        xmlFreeDoc(xDoc); return false;
            }
            strCluster = std::string((char *)attr_value_temp);
			if(strCluster.compare("yes") == 0)
			{
				string strClusName;
				attr_value_temp = xmlGetProp(xChild,(const xmlChar*)"cluster_name");
				if (attr_value_temp == NULL)
				{
					DebugPrintf(SV_LOG_ERROR,"The XML document is not in expected format : <host cluster_name> entry not found\n");
				}
				else
				{
					strClusName = (char *)attr_value_temp;
					DebugPrintf(SV_LOG_DEBUG,"Cluster Name from XML file is : %s\n", strClusName.c_str());
				}
				m_mapClusterNameToNodes.insert(make_pair(strClusName,strVmId));

				string strOs;
				attr_value_temp = xmlGetProp(xChild,(const xmlChar*)"operatingsystem");
				if (attr_value_temp == NULL)
				{
					DebugPrintf(SV_LOG_ERROR,"The XML document is not in expected format : <host operatingsystem> entry not found\n");
				}
				else
				{
					strOs = (char *)attr_value_temp;
				}
				m_mapOfClusNodeOS.insert(make_pair(strVmId,strOs));

				std::string Separator = std::string(",");
				size_t pos = 0;
				size_t found;

				string clusnodes;
				attr_value_temp = xmlGetProp(xChild,(const xmlChar*)"clusternodes_inmageguids");
				if(NULL == attr_value_temp)
				{
					DebugPrintf(SV_LOG_ERROR,"Document %s is not in expected format. <host clusternodes_inmageguids=\"XXX\" \\> property is missing.\n",m_strXmlFile.c_str());
				}
				else
				{
					clusnodes = (const char*)attr_value_temp;
					xmlFree(attr_value_temp);

					found = clusnodes.find(Separator, pos);
					if(found == std::string::npos)
					{
						DebugPrintf(SV_LOG_INFO,"Only single cluster node found.\n");
						if(strVmId.compare(clusnodes) != 0)
						{
							m_mapOfVmToClusOpn.insert(make_pair(clusnodes, strCluster));
							m_mapClusterNameToNodes.insert(make_pair(strClusName,clusnodes));
						}
					}
					else
					{
						string strTemp;
						while(found != std::string::npos)
						{
							strTemp = clusnodes.substr(pos,found-pos);
							DebugPrintf(SV_LOG_DEBUG,"Element - [%s]\n",strTemp.c_str());
							if(strVmId.compare(strTemp) != 0)
							{
								DebugPrintf(SV_LOG_DEBUG,"Cluster Node - [%s]\n", strTemp.c_str());
								m_mapOfVmToClusOpn.insert(make_pair(strTemp, strCluster));
								m_mapClusterNameToNodes.insert(make_pair(strClusName,strTemp));
							}
							pos = found + 1; 
							found = clusnodes.find(Separator, pos);            
						}
						DebugPrintf(SV_LOG_DEBUG,"Element - [%s]\n",clusnodes.substr(pos).c_str());
						strTemp = clusnodes.substr(pos);
						if(strVmId.compare(strTemp) != 0)
						{
							DebugPrintf(SV_LOG_DEBUG,"Cluster Node - [%s]\n", strTemp.c_str());
							m_mapOfVmToClusOpn.insert(make_pair(strTemp, strCluster));
							m_mapClusterNameToNodes.insert(make_pair(strClusName,strTemp));
						}
					}
				}
			}

            DebugPrintf(SV_LOG_INFO,"%s,%s,%s\n", strVmName.c_str(), strTagValue.c_str(), strTagType.c_str());
            rollbackDetails.insert(make_pair(strVmId, make_pair(strTagValue,strTagType)));
			m_mapOfVmNameToId.insert(make_pair(strVmId, strVmName));
			DebugPrintf(SV_LOG_INFO,"Cluster ==> %s\n", strCluster.c_str());
			m_mapOfVmToClusOpn.insert(make_pair(strVmId, strCluster));
        }
    }
    
    //Check if "rollbackDetails" is empty.
	if(rollbackDetails.empty())
    {
        DebugPrintf(SV_LOG_ERROR,"No VMs are present for recovery[File - %s].\n", XmlFileName.c_str());			
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;
    }

    xmlFreeDoc(xDoc);
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return true;
}

//bool RollBackVM::readIPChangeCsvFile()
//{
//    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
//	bool bReturn = true;
//	/*bool bIgnoreTagType = false;
//	vector<string> strTokenVector;
//	vector<string> vecIpDetails;
//	string strLineRead;
//	string strVmName;
//	
//	ifstream inFile(m_strIpChangeFileName.c_str());
//	if(!inFile.is_open())
//	{
//		DebugPrintf(SV_LOG_ERROR,"Failed to open file %s for reading.\n ",m_strIpChangeFileName.c_str());
//		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
//		return false;
//	}
//	while(getline(inFile,strLineRead))
//	{
//		if(strLineRead.length() <= 1)
//                        continue;
//
//		vecIpDetails = parseCsvFile(strLineRead);
//		if(vecIpDetails.empty())
//		{
//			DebugPrintf(SV_LOG_ERROR,"Failed to parse the CSV file : %s\n",m_strIpChangeFileName.c_str());
//			bReturn = false;
//			break;
//		}
//		if(vecIpDetails.size()!= 5)
//		{
//            DebugPrintf(SV_LOG_ERROR,"Invalid arguments supplied in the file : %s\n ",m_strIpChangeFileName.c_str());            
//			bReturn = false;
//			break;
//		}
//		m_mapIPChangeDetails.insert(make_pair(vecIpDetails[0],vecIpDetails));
//	}*/
//    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
//	return bReturn;
//}

//// This function will parse csv file with info of VMs to be rollbacked
////  and generates the map rollbackDetails
//// File format : 
////    Each line in file will be - VMHostName,TagValue,TagType
////      TagValue - LATEST,LATESTTIME,<SPECIFIED TAG>,<SPECIFIED TIME>
////      TagType  - FS,TIME
//bool RollBackVM::readRollbackCsvFile()
//{
//    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
//	bool bReturn = true;
//	vector<string> strTokenVector;
//	string strFileName = m_strRollBackInfoFile;
//	string strLineRead;
//	string strVmName;
//	string strTagValue;
//	string strTagType;
//	
//    if(false == checkIfFileExists(strFileName))
//	{
//        DebugPrintf(SV_LOG_ERROR,"File does not exist - %s\n", strFileName.c_str());
//        DebugPrintf(SV_LOG_ERROR,"The format of file should be <vmname>,<tag value>,<tag type>\n");
//        DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);	
//        return false;
//	}
//
//	ifstream inFile(strFileName.c_str());
//	if(!inFile.is_open())
//	{
//        DebugPrintf(SV_LOG_ERROR,"Failed to open file %s for reading.\n",m_strRollBackInfoFile.c_str());
//        DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
//        return false;
//    }
//	while(getline(inFile,strLineRead))    
//	{
//		if(strLineRead.length() <= 1)
//            continue;
//        if(strLineRead[strLineRead.size()-1] == '\r') //dos file is being read in linux.
//            strLineRead.resize(strLineRead.size()-1);
//		strTokenVector = parseCsvFile(strLineRead);
//		if(strTokenVector.empty())
//		{
//			DebugPrintf(SV_LOG_ERROR,"Failed to parse the CSV file: %s\n",strFileName.c_str());
//			bReturn = false;
//			break;
//		}
//		if(strTokenVector.size()!= 3)
//		{
//			DebugPrintf(SV_LOG_ERROR," Invalid arguments supplied in the file - %s\n",strFileName.c_str());			
//			bReturn = false;
//			break;
//		}
//		
//		//No need of local variables,but still for debugging purpose keeping it
//		strVmName   = strTokenVector[0];
//		strTagValue = strTokenVector[1];
//		strTagType  = strTokenVector[2];
//
//		//Check for Valid Tag type
//		if( (stricmp(strTagType.c_str(),FS_TAGTYPE)!=0) && (stricmp(strTagType.c_str(),TIME_TAGTYPE)!=0) )
//		{
//			DebugPrintf(SV_LOG_ERROR,"Invalid Tag type found - %s\n",strTagType.c_str());			
//			DebugPrintf(SV_LOG_INFO,"Supported values for Tag type are: FS,TIME\n");
//			bReturn = false;
//			break;
//		}		
//        rollbackDetails.insert(make_pair(strVmName,make_pair(strTagValue,strTagType)));
//	}
//
//    //Check if "rollbackDetails" is empty.
//	if(rollbackDetails.empty())
//    {
//        DebugPrintf(SV_LOG_ERROR,"Empty file - %s\n", strFileName.c_str());			
//		bReturn = false;
//    }
//
//    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
//	return bReturn;
//}

bool RollBackVM::writeRecoveredVmsName(std::vector<std::string> VecRecoveredVms)
{
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool bRetuenValue = true;
	std::string strRecoveredVmFileName;

	if(m_bPhySnapShot)
		strRecoveredVmFileName = m_strDatafolderPath + SNAPSHOT_VM_FILE_NAME;
	else
		strRecoveredVmFileName = m_strDatafolderPath + ROLLBACK_VM_FILE_NAME;	

	DebugPrintf(SV_LOG_INFO, "Opening the file to write: %s\n", strRecoveredVmFileName.c_str());
	ofstream outFile(strRecoveredVmFileName.c_str(),ios::trunc);
	if(!outFile.is_open())
	{
		DebugPrintf(SV_LOG_ERROR,"Failed to open file %s for writing\n",strRecoveredVmFileName.c_str());
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		bRetuenValue = false;
		return bRetuenValue;
	}

	std::vector<std::string>::iterator iterVec;
	for (iterVec = VecRecoveredVms.begin(); iterVec != VecRecoveredVms.end(); iterVec++)
	{
		map<string, string>::iterator iter = m_mapOfVmNameToId.find(*iterVec);
		if (iterVec->compare(iter->second) != 0)
			outFile << "InMageHostId=\"" << *iterVec << "\";";
		else
			outFile << "InMageHostId=\"\";";
		outFile << "HostName=\"" << iter->second << "\";";
		outFile << endl;
	}
	outFile.close();
	DebugPrintf(SV_LOG_INFO, "Successfully written the recovered VM entry in the file: %s\n", strRecoveredVmFileName.c_str());

	m_objLin.AssignSecureFilePermission(strRecoveredVmFileName);

#ifdef WIN32
	DebugPrintf(SV_LOG_INFO, "Copying the file to windows temp directory.\n");
	std::string systemDrive;
	if(true == GetSystemDrive(systemDrive))
	{
		string strSysTemp;
		if(m_bPhySnapShot)
			strSysTemp = systemDrive + std::string("\\WINDOWS") + std::string("\\Temp\\") + SNAPSHOT_VM_FILE_NAME;
		else
			strSysTemp = systemDrive + std::string("\\WINDOWS") + std::string("\\Temp\\") + ROLLBACK_VM_FILE_NAME;

		if(!SVCopyFile(strRecoveredVmFileName.c_str(), strSysTemp.c_str(), false))
		{
			ULONG dwCopied = 0;
			LPVOID lpMsgBuf;

			dwCopied = GetLastError();

			::FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER |
						FORMAT_MESSAGE_FROM_SYSTEM,
						NULL,
						dwCopied,
						0,
						(LPTSTR)&lpMsgBuf,
						0,
						NULL);
			if(dwCopied)
			{				
				if( dwCopied == ERROR_FILE_NOT_FOUND )
				{
					DebugPrintf(SV_LOG_ERROR, "[ERROR] File Not Found: %s \n", strRecoveredVmFileName.c_str());
					DebugPrintf(SV_LOG_ERROR, "Failed to copy the file: %s \n", strSysTemp.c_str());
				} 
				else
					DebugPrintf(SV_LOG_ERROR, "\n[ERROR] [%d] %s\n",dwCopied,(LPCTSTR)lpMsgBuf);
			}		

			// Free the buffer
			LocalFree(lpMsgBuf);
			DebugPrintf(SV_LOG_ERROR,"copy the file %s to %s manually.\n", strRecoveredVmFileName.c_str(), strSysTemp.c_str());			
			bRetuenValue = false;
		}
		else
		{
			DebugPrintf(SV_LOG_INFO,"Successfully copied the file %s to %s \n", strRecoveredVmFileName.c_str(), strSysTemp.c_str());
			bRetuenValue = true;
		}
	}
	else
	{
		DebugPrintf(SV_LOG_ERROR,"Failed to get the system drive. \n\n");
		DebugPrintf(SV_LOG_ERROR,"copy the file %s to \"<SystemDrive>\\WINDOWS\\TEMP \" manually.\n", strRecoveredVmFileName.c_str());			
		bRetuenValue = false;
	}
#endif
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bRetuenValue;
}

bool RollBackVM::removeRollbackFile()
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool bRetuenValue = true;
	std::string strRecoveredVmFileName;

	if(m_bPhySnapShot)
		strRecoveredVmFileName = m_strDatafolderPath + SNAPSHOT_VM_FILE_NAME;
	else
		strRecoveredVmFileName = m_strDatafolderPath + ROLLBACK_VM_FILE_NAME;

	bRetuenValue = checkIfFileExists(strRecoveredVmFileName);
	if(bRetuenValue)
	{
		if(-1 == remove(strRecoveredVmFileName.c_str()))
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to delete the file - %s\n",strRecoveredVmFileName.c_str());			
            DebugPrintf(SV_LOG_ERROR,"Delete the file manually and rerun the job.\n");			
			bRetuenValue = false;
		}
		else
		{
			DebugPrintf(SV_LOG_INFO,"SuccessFully deleted stale file - %s\n",strRecoveredVmFileName.c_str());			
		}
	}
	else
	{
		DebugPrintf(SV_LOG_INFO,"File(%s) does not exist. Skipping the delete.\n\n",strRecoveredVmFileName.c_str());
		bRetuenValue  = true;
	}

#ifndef WIN32
	strRecoveredVmFileName = "/var/log/inmage_mount.log";
	bRetuenValue = checkIfFileExists(strRecoveredVmFileName);
	if(bRetuenValue)
	{
		if(-1 == remove(strRecoveredVmFileName.c_str()))
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to delete the file - %s\n",strRecoveredVmFileName.c_str());
		}
		else
		{
			DebugPrintf(SV_LOG_INFO,"SuccessFully deleted stale file - %s\n",strRecoveredVmFileName.c_str());			
		}
	}
	else
	{
		DebugPrintf(SV_LOG_INFO,"File(%s) does not exist. Skipping the delete.\n\n",strRecoveredVmFileName.c_str());
		bRetuenValue  = true;
	}

#else
	std::string systemDrive;
	if(true == GetSystemDrive(systemDrive))
	{
		if(m_bPhySnapShot)
			strRecoveredVmFileName = systemDrive + std::string("\\WINDOWS") + std::string("\\Temp\\") + SNAPSHOT_VM_FILE_NAME;
		else
			strRecoveredVmFileName = systemDrive + std::string("\\WINDOWS") + std::string("\\Temp\\") + ROLLBACK_VM_FILE_NAME;

		bRetuenValue = checkIfFileExists(strRecoveredVmFileName);
		if(bRetuenValue)
		{
			if(-1 == remove(strRecoveredVmFileName.c_str()))
			{
				DebugPrintf(SV_LOG_ERROR,"Failed to delete the file - %s\n",strRecoveredVmFileName.c_str());			
				DebugPrintf(SV_LOG_ERROR,"Delete the file manually and rerun the job.\n");			
				bRetuenValue = false;
			}
			else
			{
				DebugPrintf(SV_LOG_INFO,"SuccessFully deleted stale file - %s\n",strRecoveredVmFileName.c_str());			
			}
		}
		else
		{
			DebugPrintf(SV_LOG_INFO,"File(%s) does not exist. Skipping the delete.\n\n",strRecoveredVmFileName.c_str());
			bRetuenValue  = true;
		}
	}
	else
	{
		DebugPrintf(SV_LOG_INFO,"Failed to get the system drive. \n\n");
		if(m_bPhySnapShot)
			DebugPrintf(SV_LOG_ERROR,"Delete the file \"<SystemDrive>\\WINDOWS\\TEMP\\InMage_Recovered_Vms.snapshot \" manually and rerun the job.\n");
		else
			DebugPrintf(SV_LOG_ERROR,"Delete the file \"<SystemDrive>\\WINDOWS\\TEMP\\InMage_Recovered_Vms.rollback \" manually and rerun the job.\n");			
		bRetuenValue = false;
	}
#endif

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bRetuenValue;
}


//Initialises the recovery task update structure with initial values
void RollBackVM::InitRecoveryTaskUpdate()
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);

	stepInfo_t objStepInfo;
	taskInfo_t objTaskInfo;
	list<taskInfo_t> taskInfoList;
	list<stepInfo_t> stepInfoList;

	objTaskInfo.TaskNum			= INM_VCON_TASK_1;
	objTaskInfo.TaskName		= INM_VCON_RECOVERY_TASK_1;
	objTaskInfo.TaskDesc		= INM_VCON_RECOVERY_TASK_1_DESC;
	objTaskInfo.TaskStatus		= INM_VCON_TASK_INPROGRESS;
	objTaskInfo.TaskErrMsg		= "";
	objTaskInfo.TaskFixSteps	= "";
	objTaskInfo.TaskLogPath     = m_strCxPath + "/EsxUtil.log";
	taskInfoList.push_back(objTaskInfo);

	objTaskInfo.TaskNum			= INM_VCON_TASK_2;
	objTaskInfo.TaskName		= INM_VCON_RECOVERY_TASK_2;
	objTaskInfo.TaskDesc		= INM_VCON_RECOVERY_TASK_2_DESC;
	objTaskInfo.TaskStatus		= INM_VCON_TASK_QUEUED;
	objTaskInfo.TaskErrMsg		= "";
	objTaskInfo.TaskFixSteps	= "";
	objTaskInfo.TaskLogPath     = m_strCxPath + "/EsxUtil.log";
	taskInfoList.push_back(objTaskInfo);

	objTaskInfo.TaskNum			= INM_VCON_TASK_3;
	objTaskInfo.TaskName		= INM_VCON_RECOVERY_TASK_3;
	objTaskInfo.TaskDesc		= INM_VCON_RECOVERY_TASK_3_DESC;
	objTaskInfo.TaskStatus		= INM_VCON_TASK_QUEUED;
	objTaskInfo.TaskErrMsg		= "";
	objTaskInfo.TaskFixSteps	= "";
	objTaskInfo.TaskLogPath     = m_strCxPath + "/EsxUtil.log";
	taskInfoList.push_back(objTaskInfo);

	objTaskInfo.TaskNum			= INM_VCON_TASK_4;
	objTaskInfo.TaskName		= INM_VCON_RECOVERY_TASK_4;
	objTaskInfo.TaskDesc		= INM_VCON_RECOVERY_TASK_4_DESC;
	objTaskInfo.TaskStatus		= INM_VCON_TASK_QUEUED;
	objTaskInfo.TaskErrMsg		= "";
	objTaskInfo.TaskFixSteps	= "";
	objTaskInfo.TaskLogPath     = m_strCxPath + "/EsxUtil.log";
	taskInfoList.push_back(objTaskInfo);

	objStepInfo.StepNum			= "Step1";
	objStepInfo.ExecutionStep	= m_strOperation;
	objStepInfo.StepStatus		= INM_VCON_TASK_INPROGRESS;
	objStepInfo.TaskInfoList	= taskInfoList;
	stepInfoList.push_back(objStepInfo);

	m_objLin.m_TaskUpdateInfo.FunctionName	= "UpdateESXProtectionDetails";
	m_objLin.m_TaskUpdateInfo.HostId		= m_objLin.getInMageHostId();
	m_objLin.m_TaskUpdateInfo.PlanId		= m_strPlanId;
	m_objLin.m_TaskUpdateInfo.StepInfoList	= stepInfoList;

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
}


//Initialises the drdrill task update structure with initial values while oerforming actual snapshot
void RollBackVM::InitDrdrillTaskUpdate()
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);

	stepInfo_t objStepInfo;
	taskInfo_t objTaskInfo;
	list<taskInfo_t> taskInfoList;
	list<stepInfo_t> stepInfoList;

	objTaskInfo.TaskNum			= INM_VCON_TASK_1;
	objTaskInfo.TaskName		= INM_VCON_SNAPSHOT_TASK_1;
	objTaskInfo.TaskDesc		= INM_VCON_SNAPSHOT_TASK_1_DESC;
	objTaskInfo.TaskStatus		= INM_VCON_TASK_COMPLETED;
	objTaskInfo.TaskErrMsg		= "";
	objTaskInfo.TaskFixSteps	= "";
	objTaskInfo.TaskLogPath     = m_strCxPath + "/EsxUtil.log";
	taskInfoList.push_back(objTaskInfo);

	objTaskInfo.TaskNum			= INM_VCON_TASK_2;
	objTaskInfo.TaskName		= INM_VCON_SNAPSHOT_TASK_2;
	objTaskInfo.TaskDesc		= INM_VCON_SNAPSHOT_TASK_2_DESC;
	objTaskInfo.TaskStatus		= INM_VCON_TASK_COMPLETED;
	objTaskInfo.TaskErrMsg		= "";
	objTaskInfo.TaskFixSteps	= "";
	objTaskInfo.TaskLogPath     = m_strCxPath + "/EsxUtil.log";
	taskInfoList.push_back(objTaskInfo);

	objTaskInfo.TaskNum			= INM_VCON_TASK_3;
	objTaskInfo.TaskName		= INM_VCON_SNAPSHOT_TASK_3;
	objTaskInfo.TaskDesc		= INM_VCON_SNAPSHOT_TASK_3_DESC;
	objTaskInfo.TaskStatus		= INM_VCON_TASK_COMPLETED;
	objTaskInfo.TaskErrMsg		= "";
	objTaskInfo.TaskFixSteps	= "";
	objTaskInfo.TaskLogPath     = m_strCxPath + "/EsxUtil.log";
	taskInfoList.push_back(objTaskInfo);

#ifdef WIN32
	objTaskInfo.TaskNum			= INM_VCON_TASK_4;
	objTaskInfo.TaskName		= INM_VCON_SNAPSHOT_TASK_4;
	objTaskInfo.TaskDesc		= INM_VCON_SNAPSHOT_TASK_4_DESC;
	objTaskInfo.TaskStatus		= INM_VCON_TASK_COMPLETED;
	objTaskInfo.TaskErrMsg		= "";
	objTaskInfo.TaskFixSteps	= "";
	objTaskInfo.TaskLogPath     = m_strCxPath + "/EsxUtil.log";
	taskInfoList.push_back(objTaskInfo);

	objTaskInfo.TaskNum			= INM_VCON_TASK_5;
	objTaskInfo.TaskName		= INM_VCON_SNAPSHOT_TASK_5;
	objTaskInfo.TaskDesc		= INM_VCON_SNAPSHOT_TASK_5_DESC;
	objTaskInfo.TaskStatus		= INM_VCON_TASK_INPROGRESS;
	objTaskInfo.TaskErrMsg		= "";
	objTaskInfo.TaskFixSteps	= "";
	objTaskInfo.TaskLogPath     = m_strCxPath + "/EsxUtil.log";
	taskInfoList.push_back(objTaskInfo);

	objTaskInfo.TaskNum			= INM_VCON_TASK_6;
	objTaskInfo.TaskName		= INM_VCON_SNAPSHOT_TASK_6;
	objTaskInfo.TaskDesc		= INM_VCON_SNAPSHOT_TASK_6_DESC;
	objTaskInfo.TaskStatus		= INM_VCON_TASK_QUEUED;
	objTaskInfo.TaskErrMsg		= "";
	objTaskInfo.TaskFixSteps	= "";
	objTaskInfo.TaskLogPath     = m_strCxPath + "/EsxUtil.log";
	taskInfoList.push_back(objTaskInfo);

	objTaskInfo.TaskNum			= INM_VCON_TASK_7;
	objTaskInfo.TaskName		= INM_VCON_SNAPSHOT_TASK_7;
	objTaskInfo.TaskDesc		= INM_VCON_SNAPSHOT_TASK_7_DESC;
	objTaskInfo.TaskStatus		= INM_VCON_TASK_QUEUED;
	objTaskInfo.TaskErrMsg		= "";
	objTaskInfo.TaskFixSteps	= "";
	objTaskInfo.TaskLogPath     = m_strCxPath + "/EsxUtil.log";
	taskInfoList.push_back(objTaskInfo);
#else
	objTaskInfo.TaskNum			= INM_VCON_TASK_4;
	objTaskInfo.TaskName		= INM_VCON_SNAPSHOT_TASK_5;
	objTaskInfo.TaskDesc		= INM_VCON_SNAPSHOT_TASK_5_DESC;
	objTaskInfo.TaskStatus		= INM_VCON_TASK_INPROGRESS;
	objTaskInfo.TaskErrMsg		= "";
	objTaskInfo.TaskFixSteps	= "";
	objTaskInfo.TaskLogPath     = m_strCxPath + "/EsxUtil.log";
	taskInfoList.push_back(objTaskInfo);

	objTaskInfo.TaskNum			= INM_VCON_TASK_5;
	objTaskInfo.TaskName		= INM_VCON_SNAPSHOT_TASK_6;
	objTaskInfo.TaskDesc		= INM_VCON_SNAPSHOT_TASK_6_DESC;
	objTaskInfo.TaskStatus		= INM_VCON_TASK_QUEUED;
	objTaskInfo.TaskErrMsg		= "";
	objTaskInfo.TaskFixSteps	= "";
	objTaskInfo.TaskLogPath     = m_strCxPath + "/EsxUtil.log";
	taskInfoList.push_back(objTaskInfo);

	objTaskInfo.TaskNum			= INM_VCON_TASK_6;
	objTaskInfo.TaskName		= INM_VCON_SNAPSHOT_TASK_7;
	objTaskInfo.TaskDesc		= INM_VCON_SNAPSHOT_TASK_7_DESC;
	objTaskInfo.TaskStatus		= INM_VCON_TASK_QUEUED;
	objTaskInfo.TaskErrMsg		= "";
	objTaskInfo.TaskFixSteps	= "";
	objTaskInfo.TaskLogPath     = m_strCxPath + "/EsxUtil.log";
	taskInfoList.push_back(objTaskInfo);
#endif

	objStepInfo.StepNum			= "Step1";
	objStepInfo.ExecutionStep	= m_strOperation;
	objStepInfo.StepStatus		= INM_VCON_TASK_INPROGRESS;
	objStepInfo.TaskInfoList	= taskInfoList;
	stepInfoList.push_back(objStepInfo);

	m_objLin.m_TaskUpdateInfo.FunctionName	= "UpdateESXProtectionDetails";
	m_objLin.m_TaskUpdateInfo.HostId		= m_objLin.getInMageHostId();
	m_objLin.m_TaskUpdateInfo.PlanId		= m_strPlanId;
	m_objLin.m_TaskUpdateInfo.StepInfoList	= stepInfoList;

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
}

void RollBackVM::InitRemovePairTaskUpdate()
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);

	stepInfo_t objStepInfo;
	taskInfo_t objTaskInfo;
	list<taskInfo_t> taskInfoList;
	list<stepInfo_t> stepInfoList;

	objTaskInfo.TaskNum			= INM_VCON_TASK_1;
	objTaskInfo.TaskName		= INM_VCON_REMOVE_PAIR_TASK_1;
	objTaskInfo.TaskDesc		= INM_VCON_REMOVE_PAIR_TASK_1_DESC;
	objTaskInfo.TaskStatus		= INM_VCON_TASK_INPROGRESS;
	objTaskInfo.TaskErrMsg		= "";
	objTaskInfo.TaskFixSteps	= "";
	objTaskInfo.TaskLogPath     = m_strCxPath + "/EsxUtil.log";
	taskInfoList.push_back(objTaskInfo);

	objTaskInfo.TaskNum			= INM_VCON_TASK_2;
	objTaskInfo.TaskName		= INM_VCON_REMOVE_PAIR_TASK_2;
	objTaskInfo.TaskDesc		= INM_VCON_REMOVE_PAIR_TASK_2_DESC;
	objTaskInfo.TaskStatus		= INM_VCON_TASK_QUEUED;
	objTaskInfo.TaskErrMsg		= "";
	objTaskInfo.TaskFixSteps	= "";
	objTaskInfo.TaskLogPath     = m_strCxPath + "/EsxUtil.log";
	taskInfoList.push_back(objTaskInfo);

	objTaskInfo.TaskNum			= INM_VCON_TASK_3;
	objTaskInfo.TaskName		= INM_VCON_REMOVE_PAIR_TASK_3;
	objTaskInfo.TaskDesc		= INM_VCON_REMOVE_PAIR_TASK_3_DESC;
	objTaskInfo.TaskStatus		= INM_VCON_TASK_QUEUED;
	objTaskInfo.TaskErrMsg		= "";
	objTaskInfo.TaskFixSteps	= "";
	objTaskInfo.TaskLogPath     = m_strCxPath + "/EsxUtil.log";
	taskInfoList.push_back(objTaskInfo);

#ifndef WIN32
	objTaskInfo.TaskNum			= INM_VCON_TASK_4;
	objTaskInfo.TaskName		= INM_VCON_REMOVE_PAIR_TASK_4;
	objTaskInfo.TaskDesc		= INM_VCON_REMOVE_PAIR_TASK_4_DESC;
	objTaskInfo.TaskStatus		= INM_VCON_TASK_QUEUED;
	objTaskInfo.TaskErrMsg		= "";
	objTaskInfo.TaskFixSteps	= "";
	objTaskInfo.TaskLogPath     = m_strCxPath + "/EsxUtil.log";
	taskInfoList.push_back(objTaskInfo);
#endif

	if(m_strOperation.compare("diskremove") == 0)
	{
		objTaskInfo.TaskNum			= INM_VCON_TASK_4;
		objTaskInfo.TaskName		= INM_VCON_REMOVE_PAIR_TASK_4;
		objTaskInfo.TaskDesc		= INM_VCON_REMOVE_PAIR_TASK_4_DESC;
		objTaskInfo.TaskStatus		= INM_VCON_TASK_QUEUED;
		objTaskInfo.TaskErrMsg		= "";
		objTaskInfo.TaskFixSteps	= "";
		objTaskInfo.TaskLogPath     = m_strCxPath + "/EsxUtil.log";
		taskInfoList.push_back(objTaskInfo);
	}

	objStepInfo.StepNum			= "Step1";
#ifndef WIN32
	objStepInfo.ExecutionStep	= "diskremove";
#else
	objStepInfo.ExecutionStep	= m_strOperation;
#endif
	objStepInfo.StepStatus		= INM_VCON_TASK_INPROGRESS;
	objStepInfo.TaskInfoList	= taskInfoList;
	stepInfoList.push_back(objStepInfo);

	m_objLin.m_TaskUpdateInfo.FunctionName	= "UpdateESXProtectionDetails";
	m_objLin.m_TaskUpdateInfo.HostId		= m_objLin.getInMageHostId();
	m_objLin.m_TaskUpdateInfo.PlanId		= m_strPlanId;
	m_objLin.m_TaskUpdateInfo.StepInfoList	= stepInfoList;

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
}

#ifdef WIN32
bool RollBackVM::PrepareDinfoFile(map<string, set<string> >& mapOfVmIdToSrcDiskList)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool bReturn = true;

	map<string, map<string, string> > mapHostToNewDiskInfo;
	map<string, map<string, dInfo_t> > mapHostToDiskInfofromDinfo;
	map<string, set<string> >::iterator iterPair = mapOfVmIdToSrcDiskList.begin();
	for(; iterPair != mapOfVmIdToSrcDiskList.end(); iterPair++)
	{
		string strDInfoFileWithHostId, strInfoFile;
		strDInfoFileWithHostId = string(getVxAgentInstallPath()) + string("\\Failover") + string("\\data\\") + std::string(iterPair->first) + string(".dpinfo");
		DebugPrintf(SV_LOG_INFO,"The replicated VM dInfo file = %s\n",strDInfoFileWithHostId.c_str());
		map<string, string>::iterator iterVm = m_objLin.m_mapVmIdToVmName.find(iterPair->first);
		if(iterVm != m_objLin.m_mapVmIdToVmName.end())
		{
			if(iterVm->first.compare(iterVm->second) != 0)
			{
				strInfoFile = string(getVxAgentInstallPath()) + string("\\Failover") + string("\\data\\") + std::string(iterVm->second) + string(".dpinfo");
			}
		}

		map<string, string> mapSrcToTgtDiskSig;
		set<string> missingSrcDisk;
		map<string, dInfo_t>::iterator iterMap;
		map<string, string>::iterator iterNewPair;

		if(false == checkIfFileExists(strDInfoFileWithHostId))
		{
			DebugPrintf(SV_LOG_INFO, "File %s dose not exist, Upgrade case", strDInfoFileWithHostId.c_str());
			if(true == checkIfFileExists(strInfoFile))
			{
				strDInfoFileWithHostId = strInfoFile;
			}
			else
			{
				DebugPrintf(SV_LOG_ERROR, "File %s (or) %s not found. Considering this as upgrade case", strDInfoFileWithHostId.c_str(), strInfoFile.c_str());
				continue;
			}
		}

		map<string, dInfo_t> mapTgtScsiIdToSrcDiskInfo;

		if(mapHostToNewDiskInfo.find(iterPair->first) == mapHostToNewDiskInfo.end())
		{
			if(false == GetMapOfSrcDiskToTgtDiskNumFromDinfo(iterPair->first, strDInfoFileWithHostId, mapTgtScsiIdToSrcDiskInfo))
			{
				DebugPrintf(SV_LOG_ERROR,"Failed to get the map of source to target disk numbers for VM - %s\n", iterPair->first.c_str());
				bReturn = false; continue;
			}
			mapHostToDiskInfofromDinfo.insert(make_pair(iterPair->first, mapTgtScsiIdToSrcDiskInfo));

			for(iterMap = mapTgtScsiIdToSrcDiskInfo.begin(); iterMap != mapTgtScsiIdToSrcDiskInfo.end(); iterMap++)
			{
				mapSrcToTgtDiskSig.insert(make_pair(iterMap->second.DiskSignature, iterMap->first));
			}
		}
		else
			mapSrcToTgtDiskSig = mapHostToNewDiskInfo.find(iterPair->first)->second;		

		set<string>::iterator iterSet = iterPair->second.begin();
		for(; iterSet != iterPair->second.end(); iterSet++)
		{
			iterNewPair = mapSrcToTgtDiskSig.find((*iterSet));
			if(iterNewPair != mapSrcToTgtDiskSig.end())
			{
				DebugPrintf(SV_LOG_INFO, "Removing Disk : %s\n", iterNewPair->first.c_str());
				mapSrcToTgtDiskSig.erase(iterNewPair);
			}
			else
			{
				missingSrcDisk.insert(*iterSet);
			}
		}
		mapHostToNewDiskInfo.insert(make_pair(iterPair->first, mapSrcToTgtDiskSig));

		if(!missingSrcDisk.empty() && IsClusterNode(iterPair->first))
		{
			list<string> lstClusNodes;
			GetClusterNodes(GetNodeClusterName(iterPair->first), lstClusNodes);
			
			list<string>::iterator iterList = lstClusNodes.begin();
			for(; iterList != lstClusNodes.end(); iterList++)
			{
				if(iterList->compare(iterPair->first) != 0)
				{
					mapTgtScsiIdToSrcDiskInfo.clear();
					mapSrcToTgtDiskSig.clear();

					if(mapHostToNewDiskInfo.find(*iterList) == mapHostToNewDiskInfo.end())
					{
						strDInfoFileWithHostId = string(getVxAgentInstallPath()) + string("\\Failover") + string("\\data\\") + (*iterList) + string(".dpinfo");
						if(checkIfFileExists(strDInfoFileWithHostId))
						{
							mapTgtScsiIdToSrcDiskInfo.clear();
							if(false == GetMapOfSrcDiskToTgtDiskNumFromDinfo(*iterList, strDInfoFileWithHostId, mapTgtScsiIdToSrcDiskInfo))
							{
								DebugPrintf(SV_LOG_ERROR,"Failed to get the map of source to target disk numbers for node - %s\n", iterList->c_str());
								bReturn = false; continue;
							}
							mapHostToDiskInfofromDinfo.insert(make_pair(*iterList, mapTgtScsiIdToSrcDiskInfo));

							for(iterMap = mapTgtScsiIdToSrcDiskInfo.begin(); iterMap != mapTgtScsiIdToSrcDiskInfo.end(); iterMap++)
							{
								mapSrcToTgtDiskSig.insert(make_pair(iterMap->second.DiskSignature, iterMap->first));
							}
						}
						else
							DebugPrintf(SV_LOG_WARNING,"Dinfo file does not exist for node - %s\n", iterList->c_str());
					}
					else
						mapSrcToTgtDiskSig = mapHostToNewDiskInfo.find(*iterList)->second;

					for(iterSet = missingSrcDisk.begin(); iterSet != missingSrcDisk.end(); iterSet++)
					{
						iterNewPair = mapSrcToTgtDiskSig.find((*iterSet));
						if(iterNewPair != mapSrcToTgtDiskSig.end())
						{
							DebugPrintf(SV_LOG_INFO, "Removing Disk : %s\n", iterNewPair->first.c_str());
							mapSrcToTgtDiskSig.erase(iterNewPair);
						}
					}
					mapHostToNewDiskInfo.insert(make_pair(*iterList, mapSrcToTgtDiskSig));
				}
			}
		}
	}

	for(map<string, map<string, string> >::iterator iter_map  = mapHostToNewDiskInfo.begin(); iter_map != mapHostToNewDiskInfo.end(); iter_map++)
	{
		string strDInfoFileWithHostId = string(getVxAgentInstallPath()) + string("\\Failover") + string("\\data\\") + std::string(iter_map->first) + string(".dpinfo");
		string strDInfoFileWithHostId_old = strDInfoFileWithHostId + string("_RemovePair");
		if(false==SVCopyFile(strDInfoFileWithHostId.c_str(),strDInfoFileWithHostId_old.c_str(),false))
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to take the back up of file %s with Error [%lu].\n",strDInfoFileWithHostId.c_str(),GetLastError());
		}

		bool bFileCreate = true;
		std::ofstream replicatedPairInfoFile;
		replicatedPairInfoFile.open(strDInfoFileWithHostId.c_str(), std::ios::trunc);
		if(!replicatedPairInfoFile.is_open())
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to open %s for writing\n",strDInfoFileWithHostId.c_str());
			break;
		}

		DebugPrintf(SV_LOG_INFO,"Updating dpinfo file for Host - %s\n", iter_map->first.c_str());
		for(map<string, string>::iterator iterDiskInfo = iter_map->second.begin(); iterDiskInfo != iter_map->second.end(); iterDiskInfo++)
		{
			map<string, map<string, dInfo_t> >::iterator iterDiskFrmDinfo = mapHostToDiskInfofromDinfo.find(iter_map->first);

			if(iterDiskFrmDinfo != mapHostToDiskInfofromDinfo.end())
			{
				map<string, dInfo_t>::iterator iterDisk = iterDiskFrmDinfo->second.find(iterDiskInfo->second);

				if(iterDisk != iterDiskFrmDinfo->second.end())
				{
					DebugPrintf(SV_LOG_INFO,"SrcDiskNum=%s SrcDiskSig=%s(DWORD) SrcDeviceId=%s  TgtDiskSig=%s\n",
										iterDisk->second.DiskNum.c_str(),
										iterDisk->second.DiskSignature.c_str(), 
										iterDisk->second.DiskDeviceId.c_str(), iterDiskInfo->second.c_str());
					replicatedPairInfoFile << iterDisk->second.DiskNum;
					replicatedPairInfoFile << "!@!@!";
					replicatedPairInfoFile << iterDisk->second.DiskSignature;
					replicatedPairInfoFile << "!@!@!";
					replicatedPairInfoFile << iterDisk->second.DiskDeviceId;
					replicatedPairInfoFile << "!@!@!";
					replicatedPairInfoFile << iterDiskInfo->second << endl;
				}
			}
			else
			{
				DebugPrintf(SV_LOG_INFO,"Disk information from dinfo file does not exist for host: %s\n", iter_map->first.c_str());
				bFileCreate = false;
				replicatedPairInfoFile.close();
				SVCopyFile(strDInfoFileWithHostId_old.c_str(),strDInfoFileWithHostId.c_str(),false);
			}
		}
		if(bFileCreate)
			replicatedPairInfoFile.close();
	}

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bReturn;
}
#endif


bool RollBackVM::PreparePinfoFile(map<string, map<string, string> >& mapOfVmIdToTgtNewPairList)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool bReturn = true;

	map<string, map<string, string> >::iterator iterPair = mapOfVmIdToTgtNewPairList.begin();
	for(; iterPair != mapOfVmIdToTgtNewPairList.end(); iterPair++)
	{
		string strReplicatedVmsInfoFle, strInfoFile;
#ifdef WIN32
		strReplicatedVmsInfoFle = string(getVxAgentInstallPath()) + string("\\Failover") + string("\\data\\") + std::string(iterPair->first) + string(".pInfo");
#else
		strReplicatedVmsInfoFle = string(getVxAgentInstallPath()) +  std::string("failover_data/") + std::string(iterPair->first) + string(".pInfo");
#endif
		DebugPrintf(SV_LOG_INFO,"The replicated VM pInfo file = %s\n",strReplicatedVmsInfoFle.c_str());
		map<string, string>::iterator iterVm = m_objLin.m_mapVmIdToVmName.find(iterPair->first);
		if(iterVm != m_objLin.m_mapVmIdToVmName.end())
		{
			if(iterVm->first.compare(iterVm->second) != 0)
			{
#ifdef WIN32
				strInfoFile = string(getVxAgentInstallPath()) + string("\\Failover") + string("\\data\\") + std::string(iterVm->second) + string(".pInfo");
#else
				strInfoFile = string(getVxAgentInstallPath()) +  std::string("failover_data/") + std::string(iterVm->second) + string(".pInfo");
#endif
			}

			if(false == checkIfFileExists(strReplicatedVmsInfoFle))
			{
				DebugPrintf(SV_LOG_INFO, "File %s dose not exist, Upgrade case", strReplicatedVmsInfoFle.c_str());
				if(true == checkIfFileExists(strInfoFile))
				{
					strReplicatedVmsInfoFle = strInfoFile;
				}
				else
				{
					DebugPrintf(SV_LOG_ERROR, "File %s (or) %s not found. Some issue with the files", strReplicatedVmsInfoFle.c_str(), strInfoFile.c_str());
				}
			}
		}

		string strReplicatedVmsInfoFle_old = strReplicatedVmsInfoFle + string("_RemovePair");
#ifdef WIN32
		if(false==SVCopyFile(strReplicatedVmsInfoFle.c_str(),strReplicatedVmsInfoFle_old.c_str(),false))
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to take the back up of file %s with Error [%lu].\n",strReplicatedVmsInfoFle.c_str(),GetLastError());
		}
#else
		int result = rename( strReplicatedVmsInfoFle.c_str() , strReplicatedVmsInfoFle_old.c_str());
		if(result != 0)
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to rename file %s into %s \n ",strReplicatedVmsInfoFle.c_str(), strReplicatedVmsInfoFle_old.c_str());
		}
		else
		{
			DebugPrintf(SV_LOG_INFO,"Successfully renamed the file %s into %s \n ",strReplicatedVmsInfoFle.c_str(), strReplicatedVmsInfoFle_old.c_str());
		}
#endif

		DebugPrintf(SV_LOG_INFO, "After removing the pair information, Updating file %s\n", strReplicatedVmsInfoFle.c_str());
		std::ofstream replicatedPairInfoFile;
		replicatedPairInfoFile.open(strReplicatedVmsInfoFle.c_str(), std::ios::trunc);
		if(!replicatedPairInfoFile.is_open())
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to open %s for writing\n",strReplicatedVmsInfoFle.c_str());
			break;
		}

		map<string, string>::iterator iterNewPair = iterPair->second.begin();
		for(; iterNewPair != iterPair->second.end(); iterNewPair++)
		{
			DebugPrintf(SV_LOG_INFO, "New Pair : %s  --> %s\n", iterNewPair->first.c_str(), iterNewPair->second.c_str());
			//update pInfo file
			replicatedPairInfoFile<<iterNewPair->first;
			replicatedPairInfoFile<<"!@!@!";
			replicatedPairInfoFile<<iterNewPair->second<<endl;
		}
		replicatedPairInfoFile.close();

#ifdef WIN32
		SVCopyFile(strReplicatedVmsInfoFle.c_str(),strInfoFile.c_str(),false);
#else
		char cmdline[256];
		inm_sprintf_s(cmdline,ARRAYSIZE(cmdline), "cp %s %s", strReplicatedVmsInfoFle.c_str(), strInfoFile.c_str());
		system(cmdline);
#endif
	}

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bReturn;
}


bool RollBackVM::RemoveReplicationPairs()
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool bReturn = true;
	taskInfo_t currentTask, nextTask;

	do
	{
		m_objLin.m_strDataDirPath = m_strDatafolderPath;
		m_objLin.m_strCxPath = m_strCxPath;
		InitRemovePairTaskUpdate();
		m_objLin.UpdateTaskInfoToCX();

		//Updating Task1 as completed and Task2 as inprogress
		currentTask.TaskNum = INM_VCON_TASK_1;
		currentTask.TaskStatus = INM_VCON_TASK_COMPLETED;
		nextTask.TaskNum = INM_VCON_TASK_2;
		nextTask.TaskStatus = INM_VCON_TASK_INPROGRESS;
		m_objLin.ModifyTaskInfoForCxApi(currentTask, nextTask);
		m_objLin.UpdateTaskInfoToCX();

		if(false == m_objLin.ProcessCxPath()) //This will download all the required files from the given CX Path.
		{
			currentTask.TaskNum = INM_VCON_TASK_2;
			currentTask.TaskStatus = INM_VCON_TASK_FAILED;
			currentTask.TaskErrMsg = "Failed to download the required files from the given CX path. For extended Error information download EsxUtil.log in vCon Wiz";
			currentTask.TaskFixSteps = "Check the existance CX path and the required files inside that one. Rerun the job again after specifying the correct details.";
			nextTask.TaskNum = INM_VCON_TASK_3;
			nextTask.TaskStatus = INM_VCON_TASK_QUEUED;

			DebugPrintf(SV_LOG_ERROR,"[ERROR]Failed to download the required files from the given CX path %s\n", m_strCxPath.c_str());
			DebugPrintf(SV_LOG_ERROR,"Can't Proceed further, Stopping the Remove replication pairs operation.\n");
			bReturn = false;
			break;
		}

		currentTask.TaskNum = INM_VCON_TASK_2;
		nextTask.TaskNum = INM_VCON_TASK_3;
		currentTask.TaskStatus = INM_VCON_TASK_COMPLETED;
		nextTask.TaskStatus = INM_VCON_TASK_INPROGRESS;
		m_objLin.ModifyTaskInfoForCxApi(currentTask, nextTask);
		m_objLin.UpdateTaskInfoToCX();

		currentTask.TaskNum = INM_VCON_TASK_3;
		nextTask.TaskNum = INM_VCON_TASK_4;

		m_objLin.m_strOperation = m_strOperation;
		m_objLin.m_vConVersion = 4.1;
		m_objLin.m_strDataDirPath = m_strDatafolderPath;
		if(false == m_objLin.readScsiFile())
		{
			currentTask.TaskStatus = INM_VCON_TASK_FAILED;
			currentTask.TaskErrMsg = "Failed to read the scsi file. Cannot proceed further...";
			currentTask.TaskFixSteps = "";
			nextTask.TaskStatus = INM_VCON_TASK_QUEUED;
#ifdef WIN32
			if(m_strOperation.compare("volumeremove") == 0)
			{
				nextTask.TaskNum = m_strOperation;
				nextTask.TaskStatus = INM_VCON_TASK_FAILED;
			}
#endif
			DebugPrintf(SV_LOG_ERROR,"[ERROR]Failed to read the scsi file. Cannot proceed further...\n");
			bReturn = false;
			break;
		}

		if(false == xGetHostDetailsFromXML())
		{
			currentTask.TaskStatus = INM_VCON_TASK_FAILED;
			currentTask.TaskErrMsg = string("Failed to read Host details from Remove XML file. For extended Error information download EsxUtil.log in vCon Wiz");
			currentTask.TaskFixSteps = "Check the File Existance, rerun the job again if issue persists contact inmage customer support.";
			nextTask.TaskStatus = INM_VCON_TASK_QUEUED;
#ifdef WIN32
			if(m_strOperation.compare("volumeremove") == 0)
			{
				nextTask.TaskNum = m_strOperation;
				nextTask.TaskStatus = INM_VCON_TASK_FAILED;
			}
#endif
			DebugPrintf(SV_LOG_ERROR,"\nFailed to read Host details from Remove XML file\n\n");
			bReturn = false;
			break;
		}

		ChangeVmConfig objVm;
		objVm.m_bDrDrill = false;
		objVm.m_mapOfVmNameToId = m_objLin.m_mapVmIdToVmName;

		map<string, map<string, string> > mapHostToDiskIdAndNumFrmXml;
		map<string, set<string> > mapOfVmIdToSrcDiskList;
		map<string, map<string, string> > mapHostToProtectedPairs;
		map<string, set<string> > mapOfVmIdToTgtRemovePairList;
		string strPhysicalDeviceName = "\\\\.\\PHYSICALDRIVE";
		map<string, map<string, string> >::iterator iter;

#ifdef WIN32
		//Collecting the protected volumes information from .pinfo file for all nodes and hosts.
		for(iter = m_objLin.m_mapVmToSrcandTgtVolOrDiskPair.begin(); iter != m_objLin.m_mapVmToSrcandTgtVolOrDiskPair.end(); iter++)
		{
			string HostName = iter->first;
			map<string, string> mapProtectedPairs;
			if(mapHostToProtectedPairs.find(HostName) == mapHostToProtectedPairs.end())
			{
				mapProtectedPairs = objVm.getProtectedOrSnapshotDrivesMap(HostName);
				if(mapProtectedPairs.empty())
				{
					DebugPrintf(SV_LOG_WARNING,"Protected Pairs is empty for the VM - %s .. Proceeding further with other hosts\n", HostName.c_str());
				}

				mapHostToProtectedPairs.insert(make_pair(iter->first, mapProtectedPairs));
			}

			if(IsClusterNode(HostName))
			{
				list<string> lstClusNodes;
				GetClusterNodes(GetNodeClusterName(HostName), lstClusNodes);
				
				list<string>::iterator iterList = lstClusNodes.begin();
				for(; iterList != lstClusNodes.end(); iterList++)
				{
					if(mapHostToProtectedPairs.find(*iterList) == mapHostToProtectedPairs.end())
					{
						HostName = *iterList;
						mapProtectedPairs.clear();
						mapProtectedPairs = objVm.getProtectedOrSnapshotDrivesMap(HostName);
						if(mapProtectedPairs.empty())
						{
							DebugPrintf(SV_LOG_ERROR,"Protected Pairs is empty for the VM - %s.. Proceeding further with other nodes\n", iterList->c_str());
							continue;
						}
						mapHostToProtectedPairs.insert(make_pair(HostName, mapProtectedPairs));
					}
				}
			}
		}
#endif
		
		if(m_strOperation.compare("diskremove") == 0)
		{
#ifdef WIN32
			if(false == GenerateSrcDiskToTgtScsiIDFromXml(mapHostToDiskIdAndNumFrmXml))
			{
				currentTask.TaskStatus = INM_VCON_TASK_FAILED;
				currentTask.TaskErrMsg = string("Failed to read Host to disk mapping details from Remove XML file. For extended Error information download EsxUtil.log in vCon Wiz");
				currentTask.TaskFixSteps = "Check the File Existance, rerun the job again if issue persists contact inmage customer support.";
				nextTask.TaskStatus = INM_VCON_TASK_QUEUED;
				if(m_strOperation.compare("volumeremove") == 0)
				{
					nextTask.TaskNum = m_strOperation;
					nextTask.TaskStatus = INM_VCON_TASK_FAILED;
				}
				DebugPrintf(SV_LOG_ERROR,"\nFailed to read Host details from Remove XML file\n\n");
				bReturn = false;
				break;
			}

			map<string, DisksInfoMap_t> mapHostToDiskInfo;
			map<string, map<string, string> > mapHostToDiskScsiandSig;
			iter = m_objLin.m_mapVmToSrcandTgtVolOrDiskPair.begin();
			for(; iter != m_objLin.m_mapVmToSrcandTgtVolOrDiskPair.end(); iter++)
			{
				string HostName = iter->first;
				set<string> SrcVolList;
				set<string> SrcDiskList;
				string strMbrFile = string(getVxAgentInstallPath()) + string("\\Failover") + string("\\data\\") + HostName + string("_mbrdata");
				if(true == checkIfFileExists(strMbrFile))
				{
					DisksInfoMap_t srcMapOfDiskNamesToDiskInfo;
					DLM_ERROR_CODE RetVal;
					map<string, string> mapDiskScsiIdAndSig;
					DisksInfoMapIter_t iterDiskInfo;

					if(mapHostToDiskInfo.find(HostName) == mapHostToDiskInfo.end())
					{
						srcMapOfDiskNamesToDiskInfo.clear();
						mapDiskScsiIdAndSig.clear();
						RetVal = ReadDiskInfoMapFromFile(strMbrFile.c_str(), srcMapOfDiskNamesToDiskInfo);
						if(DLM_ERR_SUCCESS != RetVal)
						{
							DebugPrintf(SV_LOG_ERROR,"Failed to collect the Source disks info map from the BLOB file(%s). Error Code - %u\n", strMbrFile, RetVal);
							bReturn = false;
							continue;
						}
						mapHostToDiskInfo.insert(make_pair(HostName, srcMapOfDiskNamesToDiskInfo));

						for(iterDiskInfo = srcMapOfDiskNamesToDiskInfo.begin(); iterDiskInfo != srcMapOfDiskNamesToDiskInfo.end(); iterDiskInfo++)
						{
							//string strDiskSig = string(iterDisk->second.DiskSignature);
							stringstream temp;
							temp << iterDiskInfo->second.DiskInfoSub.ScsiId.Channel;
							temp << ":";
							temp << iterDiskInfo->second.DiskInfoSub.ScsiId.Target;
							string strScsiId;
							if(m_bP2V)
							{
								temp << ":";
								temp << iterDiskInfo->second.DiskInfoSub.ScsiId.Lun;
								temp << ":";
								temp << iterDiskInfo->second.DiskInfoSub.ScsiId.Host;
							}
							strScsiId = temp.str();
							DebugPrintf(SV_LOG_INFO,"strScsiId: %s strDiskSignature: %s (DWORD)\n",strScsiId.c_str(), iterDiskInfo->second.DiskSignature);
							mapDiskScsiIdAndSig.insert(make_pair(strScsiId, iterDiskInfo->first));
						}

						mapHostToDiskScsiandSig.insert(make_pair(HostName, mapDiskScsiIdAndSig));
					}

					if(IsClusterNode(HostName))
					{
						list<string> lstClusNodes;
						GetClusterNodes(GetNodeClusterName(HostName), lstClusNodes);
						
						list<string>::iterator iterList = lstClusNodes.begin();
						for(; iterList != lstClusNodes.end(); iterList++)
						{
							if(mapHostToDiskInfo.find(*iterList) == mapHostToDiskInfo.end())
							{
								strMbrFile = string(getVxAgentInstallPath()) + string("\\Failover") + string("\\data\\") + (*iterList) + string("_mbrdata");
								srcMapOfDiskNamesToDiskInfo.clear();
								RetVal = ReadDiskInfoMapFromFile(strMbrFile.c_str(), srcMapOfDiskNamesToDiskInfo);
								if(DLM_ERR_SUCCESS != RetVal)
								{
									DebugPrintf(SV_LOG_ERROR,"Failed to collect the Source disks info map from the BLOB file(%s). Error Code - %u\n", strMbrFile, RetVal);
									bReturn = false;
									continue;
								}
								mapHostToDiskInfo.insert(make_pair(*iterList, srcMapOfDiskNamesToDiskInfo));
								
								mapDiskScsiIdAndSig.clear();
								for(iterDiskInfo = srcMapOfDiskNamesToDiskInfo.begin();iterDiskInfo != srcMapOfDiskNamesToDiskInfo.end();iterDiskInfo++)
								{
									//string strDiskSig = string(iterDisk->second.DiskSignature);
									stringstream temp;
									temp << iterDiskInfo->second.DiskInfoSub.ScsiId.Channel;
									temp << ":";
									temp << iterDiskInfo->second.DiskInfoSub.ScsiId.Target;
									string strScsiId;
									if(m_bP2V)
									{
										temp << ":";
										temp << iterDiskInfo->second.DiskInfoSub.ScsiId.Lun;
										temp << ":";
										temp << iterDiskInfo->second.DiskInfoSub.ScsiId.Host;
									}
									strScsiId = temp.str();
									DebugPrintf(SV_LOG_INFO,"strScsiId: %s strDiskSignature: %s (DWORD)\n",strScsiId.c_str(), iterDiskInfo->second.DiskSignature);
									mapDiskScsiIdAndSig.insert(make_pair(strScsiId, iterDiskInfo->first));
								}

								mapHostToDiskScsiandSig.insert(make_pair(*iterList, mapDiskScsiIdAndSig));
							}
						}
					}
				}
			}

			for(iter = m_objLin.m_mapVmToSrcandTgtVolOrDiskPair.begin(); iter != m_objLin.m_mapVmToSrcandTgtVolOrDiskPair.end(); iter++)
			{
				map<string, DisksInfoMap_t>::iterator iterHost = mapHostToDiskInfo.find(iter->first);
				set<string> SrcVolList, SrcDiskList;
				if(iterHost != mapHostToDiskInfo.end())
				{
					map<string, string> mapMissingScsiDisk;

					for(map<string, string>::iterator iterDiskScsi = iter->second.begin(); iterDiskScsi != iter->second.end(); iterDiskScsi++)
					{
						map<string, string>::iterator  iterDisk = mapHostToDiskScsiandSig.find(iter->first)->second.find(iterDiskScsi->first);
						if(iterDisk != mapHostToDiskScsiandSig.find(iter->first)->second.end())
						{
							DisksInfoMapIter_t iterDiskMetadata = iterHost->second.find(iterDisk->second);
							if(iterDiskMetadata != iterHost->second.end())
							{
								DebugPrintf(SV_LOG_INFO,"Getting Volumes information from disk %s\n",iterDiskMetadata->first.c_str());
								if(!iterDiskMetadata->second.VolumesInfo.empty())
								{
									VolumesInfoVecIter_t iterVol = iterDiskMetadata->second.VolumesInfo.begin();
									for(; iterVol != iterDiskMetadata->second.VolumesInfo.end(); iterVol++)
									{
										SrcVolList.insert(string(iterVol->VolumeName));
									}
								}
								else
								{
									//This processing is specific to cluster where disk information is found but volume infomration does not
									if(IsClusterNode(iter->first))
									{
										list<string> lstClusNodes;
										GetClusterNodes(GetNodeClusterName(iter->first), lstClusNodes);
										
										list<string>::iterator iterListNode = lstClusNodes.begin();
										for(; iterListNode != lstClusNodes.end(); iterListNode++)
										{
											if(iterListNode->compare(iter->first) != 0)
											{
												map<string, DisksInfoMap_t>::iterator iterNode = mapHostToDiskInfo.find(*iterListNode);
												if(iterNode != mapHostToDiskInfo.end())
												{
													DisksInfoMapIter_t iterDiskFrmNode = iterNode->second.find(iterDisk->second);
													if(iterDiskFrmNode != iterNode->second.end())
													{
														if(!iterDiskFrmNode->second.VolumesInfo.empty())
														{
															DebugPrintf(SV_LOG_INFO,"Got the Volumes information for disk [%s] from node : %s\n",iterDiskFrmNode->first.c_str(), iterListNode->c_str());
															VolumesInfoVecIter_t iterVol = iterDiskFrmNode->second.VolumesInfo.begin();
															for(; iterVol != iterDiskFrmNode->second.VolumesInfo.end(); iterVol++)
															{
																SrcVolList.insert(string(iterVol->VolumeName));
															}
															break;
														}
													}
												}
												else
												{
													DebugPrintf(SV_LOG_INFO,"Disk metadata information does not exist for node : %s\n", iterListNode->c_str());
												}
											}
										}
									}
								}
								SrcDiskList.insert(iterDiskMetadata->second.DiskSignature);
							}
						}
						else
						{
							// when the disk information itself is not found from the host. This can happen in case of w2k3 cluster
							if(IsClusterNode(iter->first))
								mapMissingScsiDisk.insert(make_pair(iterDiskScsi->first, iterDiskScsi->second));
							else
							{
								DebugPrintf(SV_LOG_ERROR,"Failed on getting the given scsi id[%s] in source information of host : %s\n",iterDiskScsi->first.c_str(), iter->first.c_str());
								iterDisk = mapHostToDiskIdAndNumFrmXml.find(iter->first)->second.find(iterDiskScsi->first);
								if(iterDisk != mapHostToDiskIdAndNumFrmXml.find(iter->first)->second.end())
								{
									DisksInfoMapIter_t iterDiskMetadata = iterHost->second.find(iterDisk->second);
									if(iterDiskMetadata != iterHost->second.end())
									{
										DebugPrintf(SV_LOG_INFO,"Getting Volumes information from disk %s\n",iterDiskMetadata->first.c_str());
										if(!iterDiskMetadata->second.VolumesInfo.empty())
										{
											VolumesInfoVecIter_t iterVol = iterDiskMetadata->second.VolumesInfo.begin();
											for(; iterVol != iterDiskMetadata->second.VolumesInfo.end(); iterVol++)
											{
												SrcVolList.insert(string(iterVol->VolumeName));
											}
										}
										SrcDiskList.insert(iterDiskMetadata->second.DiskSignature);
									}
								}
								else
								{
									DebugPrintf(SV_LOG_ERROR,"Failed on getting the given scsi id[%s] in source information of host : %s\n",iterDiskScsi->first.c_str(), iter->first.c_str());
									bReturn = false;
									break;
								}
							}
								
						}
					}

					if(!SrcDiskList.empty())
					{
						if(mapOfVmIdToSrcDiskList.find(iter->first) == mapOfVmIdToSrcDiskList.end())
							mapOfVmIdToSrcDiskList.insert(make_pair(iter->first, SrcDiskList));
						else
							mapOfVmIdToSrcDiskList.find(iter->first)->second.insert(SrcDiskList.begin(), SrcDiskList.end());
					}

					//Processing the cluster missing disks.
					if(!mapMissingScsiDisk.empty())
					{
						list<string> lstClusNodes;
						GetClusterNodes(GetNodeClusterName(iter->first), lstClusNodes);
						
						for(map<string, string>::iterator iterMissDisk = mapMissingScsiDisk.begin(); iterMissDisk != mapMissingScsiDisk.end(); iterMissDisk++)
						{
							list<string>::iterator iterListNode = lstClusNodes.begin();
							for(; iterListNode != lstClusNodes.end(); iterListNode++)
							{
								bool bGotVolumeInfo = false;
								if(iterListNode->compare(iter->first) != 0)
								{
									map<string, DisksInfoMap_t>::iterator iterHostNode = mapHostToDiskInfo.find(*iterListNode);
									DebugPrintf(SV_LOG_INFO,"Checking for the disk [%s] under node : %s\n",iterMissDisk->first.c_str(), iterListNode->c_str());
									map<string, string>::iterator iterDisk = mapHostToDiskScsiandSig.find(*iterListNode)->second.find(iterMissDisk->first);
									if(iterDisk != mapHostToDiskScsiandSig.find(*iterListNode)->second.end())
									{
										DisksInfoMapIter_t iterDiskFrmNode = iterHostNode->second.find(iterDisk->second);
										if(iterDiskFrmNode != iterHostNode->second.end())
										{
											set<string> newSrcDisklist; 
											newSrcDisklist.insert(iterDiskFrmNode->second.DiskSignature);  //Found the disk
											if(mapOfVmIdToSrcDiskList.find(*iterListNode) == mapOfVmIdToSrcDiskList.end())
												mapOfVmIdToSrcDiskList.insert(make_pair(*iterListNode, newSrcDisklist));
											else
												mapOfVmIdToSrcDiskList.find(*iterListNode)->second.insert(iterDiskFrmNode->second.DiskSignature);
											
											DebugPrintf(SV_LOG_INFO,"Getting Volumes information from disk %s\n",iterDiskFrmNode->first.c_str());
											if(!iterDiskFrmNode->second.VolumesInfo.empty())
											{
												DebugPrintf(SV_LOG_INFO,"Got the Volumes information for disk [%s] from node : %s\n",iterDiskFrmNode->first.c_str(), iterListNode->c_str());
												VolumesInfoVecIter_t iterVol = iterDiskFrmNode->second.VolumesInfo.begin();
												for(; iterVol != iterDiskFrmNode->second.VolumesInfo.end(); iterVol++)
												{
													SrcVolList.insert(string(iterVol->VolumeName));
												}
												bGotVolumeInfo = true;
											}
											else
											{
												list<string>::iterator iterListNode2 = lstClusNodes.begin();
												for(; iterListNode2 != lstClusNodes.end(); iterListNode2++)
												{
													if((iterListNode2->compare(iter->first) != 0) && (iterListNode2->compare(*iterListNode) != 0))
													{
														map<string, DisksInfoMap_t>::iterator iterNode2 = mapHostToDiskInfo.find(*iterListNode2);
														if(iterNode2 != mapHostToDiskInfo.end())
														{
															DisksInfoMapIter_t iterDiskFrmNode2 = iterNode2->second.find(iterDisk->second);
															if(iterDiskFrmNode2 != iterNode2->second.end())
															{
																if(!iterDiskFrmNode2->second.VolumesInfo.empty())
																{
																	DebugPrintf(SV_LOG_INFO,"Got the Volumes information for disk [%s] from node : %s\n",iterDiskFrmNode2->first.c_str(), iterListNode2->c_str());
																	VolumesInfoVecIter_t iterVol = iterDiskFrmNode2->second.VolumesInfo.begin();
																	for(; iterVol != iterDiskFrmNode2->second.VolumesInfo.end(); iterVol++)
																	{
																		SrcVolList.insert(string(iterVol->VolumeName));
																	}
																	bGotVolumeInfo = true;
																	break;
																}
															}
														}
														else
														{
															DebugPrintf(SV_LOG_INFO,"Disk metadata information does not exist for node : %s\n", iterListNode2->c_str());
														}
													}
												}
											}
											if(bGotVolumeInfo)
												break;
										}
										//else disk details does not found moving to next node
									}
									//else node details does not found moving to next node
								}
								//else same node so not processing and continueing for other cluster nodes
							}
						}
					}
				}
				else
				{
					map<string, unsigned int> SrcMapVolNameToDiskNumber;
					if(false == GetMapVolNameToDiskNumberFromDiskBin(iter->first, SrcMapVolNameToDiskNumber))
					{
						DebugPrintf(SV_LOG_ERROR,"Failed to get the map of volumes to disk numbers from Disk Binary file.\n");
					}

					string strDSFileName = std::string(getVxAgentInstallPath()) + std::string("\\Failover\\data\\") + iter->first + string(SCSI_DISK_FILE_NAME);
					map<string, string> mapOfSrcScsiIdsAndSrcDiskNmbrs;
					if(false == ReadFileWith2Values(strDSFileName,mapOfSrcScsiIdsAndSrcDiskNmbrs))
					{
						DebugPrintf(SV_LOG_ERROR,"Failed to fetch the Source Disk Numbers and SCSI IDs for this VM : %s\n",iter->first.c_str());
					}

					map<string, unsigned int>::iterator iterVol = SrcMapVolNameToDiskNumber.begin();
					for(; iterVol != SrcMapVolNameToDiskNumber.end(); iterVol++)
					{
						SrcVolList.insert(iterVol->first);
					}
				}

				set<string> tgtVolList;
				set<string> missingVolList;
				set<string>::iterator iterSetVol = SrcVolList.begin();
				for(; iterSetVol != SrcVolList.end(); iterSetVol++)
				{
					if(mapHostToProtectedPairs.find(iter->first) != mapHostToProtectedPairs.end())
					{
						map<string, string>::iterator iterVolRemove = mapHostToProtectedPairs.find(iter->first)->second.find(*iterSetVol);
						if(iterVolRemove != mapHostToProtectedPairs.find(iter->first)->second.end())
						{
							tgtVolList.insert((iterVolRemove->second));
							mapHostToProtectedPairs.find(iter->first)->second.erase(iterVolRemove);
						}
						else
							missingVolList.insert(*iterSetVol);

					}
				}

				mapOfVmIdToTgtRemovePairList.insert(make_pair(iter->first, tgtVolList));

				if(!missingVolList.empty() && IsClusterNode(iter->first))
				{
					list<string> lstClusNodes;
					GetClusterNodes(GetNodeClusterName(iter->first), lstClusNodes);
						
					list<string>::iterator iterListNode = lstClusNodes.begin();
					for(; iterListNode != lstClusNodes.end(); iterListNode++)
					{
						if(iterListNode->compare(iter->first) != 0)
						{
							set<string> tgtVolListOfNode;
							if(mapHostToProtectedPairs.find(*iterListNode) != mapHostToProtectedPairs.end())
							{
								for(set<string>::iterator iterMissVol = missingVolList.begin(); iterMissVol != missingVolList.end(); iterMissVol++)
								{
									map<string, string>::iterator iterVolRemove = mapHostToProtectedPairs.find(*iterListNode)->second.find(*iterMissVol);
									if(iterVolRemove != mapHostToProtectedPairs.find(*iterListNode)->second.end())
									{
										tgtVolListOfNode.insert((iterVolRemove->second));
										mapHostToProtectedPairs.find(*iterListNode)->second.erase(iterVolRemove);
									}
								}

								if(mapOfVmIdToTgtRemovePairList.find(*iterListNode) != mapOfVmIdToTgtRemovePairList.end())
									mapOfVmIdToTgtRemovePairList.find(*iterListNode)->second.insert(tgtVolListOfNode.begin(), tgtVolListOfNode.end());
								else
									mapOfVmIdToTgtRemovePairList.insert(make_pair(*iterListNode, tgtVolListOfNode));
							}
						}
					}
				}
			}
#endif
		}
		else
		{
			map<string, list<string> > mapMissingVolList;
			iter = m_objLin.m_mapVmToSrcandTgtVolOrDiskPair.begin();
			for(; iter != m_objLin.m_mapVmToSrcandTgtVolOrDiskPair.end(); iter++)
			{
				list<string> missingVolList;
				set<string> tgtDiskList;
				string HostName = iter->first;
				map<string, string> mapSrcToTgtDisks;
				tgtDiskList.clear();
				mapSrcToTgtDisks.clear();
#ifdef WIN32
				if(mapHostToProtectedPairs.find(HostName) != mapHostToProtectedPairs.end())
					mapSrcToTgtDisks = mapHostToProtectedPairs.find(HostName)->second;

				if(mapSrcToTgtDisks.empty())
				{
					DebugPrintf(SV_LOG_WARNING,"Protected Pairs is empty for the VM - %s\n", HostName.c_str());
				}
#else

				if(false == generateDisksMap(HostName, mapSrcToTgtDisks))
				{
					DebugPrintf(SV_LOG_ERROR,"Failed to get the map of source to target disks for VM - %s\n", HostName.c_str());
					bReturn = false;
					continue;
				}
#endif
				DebugPrintf(SV_LOG_INFO,"Host for which pairs need to remove : %s\n", HostName.c_str());
				map<string, string>::iterator iterDisk = iter->second.begin();
				for(; iterDisk != iter->second.end(); iterDisk++)
				{
					string strTemp = iterDisk->first;
#ifdef WIN32
					if(strTemp.length() <= 3)
					{
						if(strTemp.find(":\\") != string::npos)
							strTemp = iterDisk->first;
						else if(strTemp.find(":") != string::npos)
							strTemp = strTemp + "\\";
						else if(strTemp.length() == 1)
							strTemp = strTemp + ":\\";

					}
					else
					{
						strTemp = iterDisk->first.at(iterDisk->first.length()-1);
						if(strTemp.compare("\\") != 0)
							strTemp = iterDisk->first + "\\";
						else
							strTemp = iterDisk->first;
					}
#endif
					map<string, string>::iterator iterDiskRemove = mapSrcToTgtDisks.find(strTemp);
					if(iterDiskRemove != mapSrcToTgtDisks.end())
					{
						DebugPrintf(SV_LOG_INFO,"Pair which need to remove : [%s]-->[%s]\n", strTemp.c_str(), iterDiskRemove->second.c_str());
						tgtDiskList.insert(iterDiskRemove->second);
						mapSrcToTgtDisks.erase(iterDiskRemove);
					}
#ifdef WIN32
					else
					{
						if(IsClusterNode(HostName))
						{
							DebugPrintf(SV_LOG_INFO,"Volume : [%s] is missing from .pinfo file. Will search it in other cluster nodes.\n", strTemp.c_str());
							missingVolList.push_back(strTemp);
						}
						else
						{
							DebugPrintf(SV_LOG_ERROR,"Failed to get Volume : %s for the VM - %s to remove\n", strTemp.c_str(), HostName.c_str());
							bReturn = false;
						}
					}
#endif
				}
				mapOfVmIdToTgtRemovePairList.insert(make_pair(HostName, tgtDiskList));
				mapHostToProtectedPairs[HostName] = mapSrcToTgtDisks;
#ifdef WIN32
				if(IsClusterNode(HostName))
					mapMissingVolList.insert(make_pair(HostName, missingVolList));
#endif
			}
#ifdef WIN32
			for(map<string, list<string> >::iterator iterMissing = mapMissingVolList.begin(); iterMissing != mapMissingVolList.end(); iterMissing++)
			{
				list<string> lstClusNodes;
				GetClusterNodes(GetNodeClusterName(iterMissing->first), lstClusNodes);
				
				list<string>::iterator iterList = lstClusNodes.begin();
				for(; iterList != lstClusNodes.end(); iterList++)
				{
					if(iterList->compare(iterMissing->first) != 0)
					{
						set<string> tgtVolList;
						map<string, string> mapSrcToTgtDisks; 
						
						if(mapHostToProtectedPairs.find(*iterList) != mapHostToProtectedPairs.end())
							mapSrcToTgtDisks = mapHostToProtectedPairs.find(*iterList)->second;

						if(mapSrcToTgtDisks.empty())
						{
							DebugPrintf(SV_LOG_INFO,"Protected Pairs is empty for the Node - %s\n", iterList->c_str());
							continue;
						}

						DebugPrintf(SV_LOG_INFO,"Node for which pairs need to check for remove : %s\n", iterList->c_str());
						for(list<string>::iterator iterVolList = iterMissing->second.begin(); iterVolList != iterMissing->second.end(); iterVolList++)
						{
							map<string, string>::iterator iterVolRemove = mapHostToProtectedPairs.find(*iterList)->second.find(*iterVolList);
							if(iterVolRemove != mapHostToProtectedPairs.find(*iterList)->second.end())
							{
								DebugPrintf(SV_LOG_INFO,"Pair which need to remove : [%s]-->[%s]\n", iterVolList->c_str(), iterVolRemove->second.c_str());
								tgtVolList.insert(iterVolRemove->second);
								mapHostToProtectedPairs.find(*iterList)->second.erase(iterVolRemove);
							}
						}
						if(mapOfVmIdToTgtRemovePairList.find(*iterList) != mapOfVmIdToTgtRemovePairList.end())
							mapOfVmIdToTgtRemovePairList.find(*iterList)->second.insert(tgtVolList.begin(), tgtVolList.end());
						else
							mapOfVmIdToTgtRemovePairList.insert(make_pair(*iterList, tgtVolList));
					}
				}
			}
#endif
		}

		if(!bReturn)
		{
			currentTask.TaskStatus = INM_VCON_TASK_FAILED;
			currentTask.TaskErrMsg = "Failed while trying to get the hosts required information to delete the pairs. For extended error information check EsxUtil.log...";
			currentTask.TaskFixSteps = "";
			nextTask.TaskStatus = INM_VCON_TASK_QUEUED;
#ifdef WIN32
			if(m_strOperation.compare("volumeremove") == 0)
			{
				nextTask.TaskNum = m_strOperation;
				nextTask.TaskStatus = INM_VCON_TASK_FAILED;
			}
#endif
			DebugPrintf(SV_LOG_ERROR,"[ERROR]Failed while trying to get the hosts required information to delete the pairs\n");
			DebugPrintf(SV_LOG_ERROR,"Can't Proceed further, Stopping the Remove replication pairs operation.\n");
			break;
		}

		if(false == RemoveReplicationPairs(mapOfVmIdToTgtRemovePairList))
		{
			currentTask.TaskStatus = INM_VCON_TASK_FAILED;
			currentTask.TaskErrMsg = "Failed to remove the pairs using CX API. For extended error information check EsxUtil.log...";
			currentTask.TaskFixSteps = "";
			nextTask.TaskStatus = INM_VCON_TASK_QUEUED;
#ifdef WIN32
			if(m_strOperation.compare("volumeremove") == 0)
			{
				nextTask.TaskNum = m_strOperation;
				nextTask.TaskStatus = INM_VCON_TASK_FAILED;
			}
#endif
			DebugPrintf(SV_LOG_ERROR,"[ERROR]Failed to remove the pairs using CX API\n");
			DebugPrintf(SV_LOG_ERROR,"Can't Proceed further, Stopping the Remove replication pairs operation.\n");
			bReturn = false;
			break;
		}

		if(false == PreparePinfoFile(mapHostToProtectedPairs))
		{
			currentTask.TaskStatus = INM_VCON_TASK_WARNING;
			currentTask.TaskErrMsg = "Pairs got removed successfully, failed on removing the pair entries from pinfo file. For extended error information check EsxUtil.log...";
			currentTask.TaskFixSteps = "Manually remove the pair entries from Pinfo file at MT";
			nextTask.TaskStatus = INM_VCON_TASK_INPROGRESS;
#ifdef WIN32
			if(m_strOperation.compare("volumeremove") == 0)
			{
				nextTask.TaskNum = m_strOperation;
				nextTask.TaskStatus = INM_VCON_TASK_WARNING;
			}
#endif
			DebugPrintf(SV_LOG_ERROR,"Pairs got removed successfully, but failed on removing the pair entries from pinfo file\n");
			bReturn = false;
			break;
		}

#ifdef WIN32
		nextTask.TaskNum = m_strOperation;
		nextTask.TaskStatus = INM_VCON_TASK_WARNING;
#else
		nextTask.TaskStatus = INM_VCON_TASK_INPROGRESS;
#endif

#ifdef WIN32
		if(m_strOperation.compare("diskremove") == 0)
		{
			if(false == PrepareDinfoFile(mapOfVmIdToSrcDiskList))
			{
				currentTask.TaskStatus = INM_VCON_TASK_WARNING;
				currentTask.TaskErrMsg = "Pairs got removed successfully, failed on removing the pair entries from dpinfo file. For extended error information check EsxUtil.log...";
				currentTask.TaskFixSteps = "Manually remove the pair entries from Pinfo file at MT";
				nextTask.TaskStatus = INM_VCON_TASK_INPROGRESS;

				DebugPrintf(SV_LOG_ERROR,"Pairs got removed successfully, but failed on removing the pair entries from dpinfo file\n");
				bReturn = false;
				break;
			}
			nextTask.TaskStatus = INM_VCON_TASK_INPROGRESS;
		}
#endif
		currentTask.TaskStatus = INM_VCON_TASK_COMPLETED;
		
	}while(0);

	m_objLin.ModifyTaskInfoForCxApi(currentTask, nextTask);
	m_objLin.UpdateTaskInfoToCX();

	//uploads the Log file to CX
	if(false == m_objLin.UploadFileToCx(m_strLogFile, m_strCxPath))
		DebugPrintf(SV_LOG_ERROR, "Failed to upload the %s file to CX", m_strLogFile.c_str());
	else
		DebugPrintf(SV_LOG_INFO, "Successfully upload file %s file to CX", m_strLogFile.c_str());

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bReturn;
}

bool RollBackVM::RemoveReplicationPairs(map<string, set<string> >& mapOfVmIdToTgtRemovePairList)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool bReturn = true;

	string strMtHostID = m_objLin.getInMageHostId();
	if(strMtHostID.empty())
	{
		DebugPrintf(SV_LOG_ERROR,"Failed to fetch InMage Host id of Master target.\n");
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;
	}

	DebugPrintf(SV_LOG_INFO,"Master target host Id : %s.\n", strMtHostID.c_str());
	list<string> listTgtDevices;
	map<string, set<string> >::iterator iter = mapOfVmIdToTgtRemovePairList.begin();
	for(;iter !=  mapOfVmIdToTgtRemovePairList.end(); iter++)
	{
		DebugPrintf(SV_LOG_INFO,"Removing below pairs for host %s\n", iter->first.c_str());
		if(iter->second.empty())
		{
			DebugPrintf(SV_LOG_WARNING,"No Replication pairs information found to remove for host : %s\n", iter->first.c_str());
			continue;
		}
		set<string>::iterator iterSet = iter->second.begin();
		for(; iterSet != iter->second.end(); iterSet++)
		{
			string strVolume = (*iterSet);
			if(strVolume.length() <= 3)
			{
				strVolume = strVolume.substr(0, 1);
			}
			else
			{
				string strTemp = strVolume.substr(strVolume.length()-1);
				if(strTemp.compare("\\") == 0)
					strVolume = strVolume.substr(0, strVolume.length()-1);
			}
			DebugPrintf(SV_LOG_INFO,"\tDevice : %s\n", strVolume.c_str());
			listTgtDevices.push_back((strVolume));
		}
	}

	if(!listTgtDevices.empty())
	{
		if(false == m_objLin.RemoveReplicationPair(strMtHostID, listTgtDevices))
		{
			DebugPrintf(SV_LOG_INFO,"Failed to remove the replication pairs...\n");
			bReturn = false;
		}
	}
	else
	{
		DebugPrintf(SV_LOG_WARNING,"[WARNING] No Replication pairs information found to remove\n");
	}

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bReturn;
}


//**************************************************************************
//
//GetProtectedPairInformation()
//
//Read m_RecVmsInfo.m_volumeMapping to find Protected pair information per
//Host. This is a common function for both windows/linux when rollback
//policy is read from ConfigFile.
//
//Input		:: Host ID.
//Output	:: Map containing SRC and TGT volumes.
//
//**************************************************************************
std::map< std::string, std::string > RollBackVM::GetProtectedPairsForHostID( const std::string & str_hostID )
{
	DebugPrintf(SV_LOG_DEBUG, "Entering %s\n", __FUNCTION__);

	std::map < std::string, std::string > map_protectedPairInfo;
	map_protectedPairInfo.clear();
	DebugPrintf(SV_LOG_INFO, "Inserting protected volume information for host ID : %s.\n", str_hostID.c_str());
	map<string, map<string, string> >::iterator iterHost = m_RecVmsInfo.m_volumeMapping.find(str_hostID);
	if (iterHost != m_RecVmsInfo.m_volumeMapping.end())
	{
		map<string, string>::iterator iterVolPair = iterHost->second.begin();
		for (; iterVolPair != iterHost->second.end(); iterVolPair++)
		{
			DebugPrintf(SV_LOG_INFO, "Protected Pair information: insert : %s -> %s.\n", iterVolPair->first.c_str(), iterVolPair->second.c_str());
			map_protectedPairInfo.insert(make_pair(iterVolPair->first, iterVolPair->second));
		}
	}
	DebugPrintf(SV_LOG_DEBUG, "Exiting %s\n", __FUNCTION__);
	return map_protectedPairInfo;
}

//**************************************************************************
//
//PreProcessRecoveryPlan()
//
//Use this function to pre-process a recovery plan. Use this function if
//1. A decision has to be made across multiple machines.
//2. To check whether user provided data can be used to recover?
//3. To validate combinational inputs //this is more of a generic.
//4. To set a recovery flow per target environment -- In future.
//
//Input		: NONE.It fetches details from member variable "rollbackDetails"
//Output	: Returns TRUE/FALSE.
//
//**************************************************************************
bool RollBackVM::PreProcessRecoveryPlan( std::map<std::string , StatusInfo >& map_hostIdToStatusInfo )
{
	DebugPrintf( SV_LOG_DEBUG, "Entering %s\n", __FUNCTION__ );
	bool bReturnValue = true; //Make explicit true for every successful case.

	do
	{
		if (rollbackDetails.empty())
		{
			DebugPrintf(SV_LOG_DEBUG, "No machines are sent for recovery.\n");
			bReturnValue = false;
			break;
		}
		
		//Collect all the protected pairs.
		std::map<std::string, std::string> map_combinedProtectedPairs;
		std::vector<std::string> vec_combinedRetDbPaths;
		std::string str_TagName = ""; //LATEST or LATESTTIME
		std::string str_TagType = ""; //FS or TIME
		
		//At CX itself Tag information is validated. It checks whether atleast a Common Distributed Tag exists or not.
		map< string, pair<string, string> >::iterator iterRollbackDetails = rollbackDetails.begin();
		for (; iterRollbackDetails != rollbackDetails.end(); iterRollbackDetails++)
		{
			RecoveryFlowStatus recoveryFlowInput;
			StatusInfo statInfo;

			string strSrcHostName = "";
			map<string, string>::iterator iterH = m_mapOfVmNameToId.find(iterRollbackDetails->first);
			if (iterH != m_mapOfVmNameToId.end())
				strSrcHostName = iterH->second;

			DebugPrintf(SV_LOG_INFO, "Preprocessing host ID = %s, hostname : %s.\n", iterRollbackDetails->first.c_str(), strSrcHostName.c_str());
			recoveryFlowInput.tagtype = iterRollbackDetails->second.second;
			recoveryFlowInput.tagname = iterRollbackDetails->second.first;
			statInfo.HostId = iterRollbackDetails->first;

			std::map<std::string, std::string> map_protectedPairsPerHost;
			map_protectedPairsPerHost.clear();
			map_protectedPairsPerHost = GetProtectedPairsForHostID(iterRollbackDetails->first);
			DebugPrintf(SV_LOG_INFO, "Got the protected pair information.\n");
			DebugPrintf(SV_LOG_INFO, "Size of Map = %d.\n", map_protectedPairsPerHost.size());
			if (map_protectedPairsPerHost.empty())
			{
				DebugPrintf(SV_LOG_ERROR, "Protected pairs list is empty for the VM : %s, Host Name : %s.\n", iterRollbackDetails->first.c_str(), strSrcHostName.c_str());
				recoveryFlowInput.recoverystatus = CDPCLI_ROLLBACK;
			}
			else
			{
				//may be we should not send same list we have to make copy of it and then send it. if protectedpaircount > available pairs?
				std::map<std::string, std::string> map_availablePairs = map_protectedPairsPerHost;
				std::string strValue, strType;//Just for Place Holder
				if (!GenerateProtectionPairList(iterRollbackDetails->first, map_availablePairs, strValue, strType))
				{
					DebugPrintf(SV_LOG_ERROR, "Failed to collect protected pairs for host : %s, hostname : %s.\n", iterRollbackDetails->first.c_str(), strSrcHostName.c_str());
					//This is an error, should have to fail here. TODO.//This is another case which has to be cross checked.
				}

				if (map_availablePairs.empty())
				{
					DebugPrintf(SV_LOG_INFO, "Recovery is completed for all pairs belonging to host : %s, hostname : %s.\n", iterRollbackDetails->first.c_str(), strSrcHostName.c_str());
					recoveryFlowInput.recoverystatus = CDPCLI_ROLLBACK;
				}
				else if (map_protectedPairsPerHost.size() != map_availablePairs.size())
				{
					DebugPrintf(SV_LOG_INFO, "Rollback for some of the pairs for this host is completed.\n");
					//Here we have to use the Tag/Time which is sent in policy.//Do not club this list for common consistent point.
					recoveryFlowInput.recoverystatus = TAG_VERIFIED;
					//Do we need to update only the available pairs? for rollback to happen only on them.
				}
				else if ((iterRollbackDetails->second.first.compare(LATEST_TAG) == 0) || (iterRollbackDetails->second.first.compare(LATEST_TIME) == 0))
				{
					map_combinedProtectedPairs.insert(map_protectedPairsPerHost.begin(), map_protectedPairsPerHost.end());
					
					str_TagName = iterRollbackDetails->second.first;
					str_TagType = iterRollbackDetails->second.second;

					//Now send all the mapped protection pairs to get DB paths.
					std::vector<std::string> vRetDbPaths;
					std::map<std::string, std::string> mTgtVolToRetDb;
					//Retention DB paths are generated for only protected pairs per Host ID.
					if (false == GetRetentionDBPaths(map_protectedPairsPerHost, vRetDbPaths, mTgtVolToRetDb))
					{
						DebugPrintf(SV_LOG_ERROR, "Failed to get Retention Database Paths of the protected/Snapshot Pairs for the VM - %s\n", iterRollbackDetails->first.c_str());
						statInfo.ErrorMsg = "Unable to query the retention history for the protected item " + strSrcHostName;
						statInfo.ErrorCode = "EA0605";
						statInfo.Status = "2";
						statInfo.PlaceHolder.Properties[MT_HOSTNAME] = m_strHostName;
						bReturnValue = false;
					}
					vec_combinedRetDbPaths.insert(vec_combinedRetDbPaths.end(), vRetDbPaths.begin(), vRetDbPaths.end());
				}
				else
				{
					//This is the case where even all the pairs exist but a specific tag information is sent.
					DebugPrintf(SV_LOG_INFO, "Specific tag information is sent for host : %s, HostName = %s.\n", iterRollbackDetails->first.c_str(), strSrcHostName.c_str());
				}
			}

			map_hostIDRecoveryFlowStatus[iterRollbackDetails->first] = recoveryFlowInput;
			map_hostIdToStatusInfo[iterRollbackDetails->first] = statInfo;
		}

		if (str_TagName.empty() || str_TagType.empty())
		{
			DebugPrintf( SV_LOG_INFO, "All the machines sent under recovery policy are specified with particular tag/time.\n" );
			bReturnValue = true;
			break;
		}

		if (!bReturnValue)
			break;

		map< std::string, RecoveryFlowStatus >::iterator iter_maphostIDRecoveryFlowStatus = map_hostIDRecoveryFlowStatus.begin();
		//Validate the presence of tag. Value and Type are equivalent factors for TagValue and TagType : either FS Tag or TIME.
		std::string LatestTagOrTime;
		DebugPrintf(SV_LOG_INFO , "TagName = %s, TagType = %s.\n", str_TagName.c_str(), str_TagType.c_str());
		if (false == ValidateAndGetRollbackOptions(vec_combinedRetDbPaths, str_TagName, str_TagType, LatestTagOrTime))
		{
			DebugPrintf(SV_LOG_ERROR, "Could not find common consistent Tag. Cannot perform rollback.\n");
			for (; iter_maphostIDRecoveryFlowStatus != map_hostIDRecoveryFlowStatus.end(); ++iter_maphostIDRecoveryFlowStatus)
			{
				if ((iter_maphostIDRecoveryFlowStatus->second.recoverystatus != TAG_VERIFIED) || (iter_maphostIDRecoveryFlowStatus->second.recoverystatus != CDPCLI_ROLLBACK))
				{
					DebugPrintf(SV_LOG_INFO, "Failed to get the common recovery point for host : %s\n", iter_maphostIDRecoveryFlowStatus->first.c_str());
					
					StatusInfo statInfo;
					map<std::string, StatusInfo>::iterator iter_maphostIdToStatusInfo = map_hostIdToStatusInfo.find(iter_maphostIDRecoveryFlowStatus->first);
					if (iter_maphostIdToStatusInfo != map_hostIdToStatusInfo.end())
						statInfo = iter_maphostIdToStatusInfo->second;

					string strSrcHostName = "";
					map<string, string>::iterator iterH = m_mapOfVmNameToId.find(iter_maphostIDRecoveryFlowStatus->first);
					if (iterH != m_mapOfVmNameToId.end())
						strSrcHostName = iterH->second;

					statInfo.ErrorMsg = "Unable to find a common consistent recovery point across volumes of the protected item " + strSrcHostName;
					statInfo.ErrorCode = "EA0604";
					statInfo.Status = "2";
					statInfo.PlaceHolder.Properties[MT_HOSTNAME] = m_strHostName;
					map_hostIdToStatusInfo[iter_maphostIDRecoveryFlowStatus->first] = statInfo;

					for (map<string, string>::iterator iterPair = map_combinedProtectedPairs.begin(); iterPair != map_combinedProtectedPairs.end(); iterPair++)
					{
						CollectCdpCliListEventLogs(iterPair->second);
					}
				}
			}
			bReturnValue = false;
			break;
		}
		else
		{
			for (std::map< std::string, std::string >::iterator iter_pairInfo = map_combinedProtectedPairs.begin(); iter_pairInfo != map_combinedProtectedPairs.end(); ++iter_pairInfo)
			{
				if (!BookMarkTheTag(LatestTagOrTime, iter_pairInfo->second , str_TagType))
				{
					DebugPrintf(SV_LOG_ERROR, "Failed to lock the tag/time [%s] for target volume %s.\n", LatestTagOrTime.c_str(), iter_pairInfo->second.c_str());
					//Currently ignoring the failure case. Need to discuss
				}
			}
		}

		if (!bReturnValue)
			break;

		DebugPrintf(SV_LOG_INFO , "Common consistent point = %s.\n", LatestTagOrTime.c_str());
		//Upadate the TAG Name in map_hostIDRecoveryFlowStatus.
		for (iter_maphostIDRecoveryFlowStatus = map_hostIDRecoveryFlowStatus.begin(); iter_maphostIDRecoveryFlowStatus != map_hostIDRecoveryFlowStatus.end(); ++iter_maphostIDRecoveryFlowStatus)
		{
			//Assign Tag Name. If TagName is not LATEST_TAG or LATEST_TIME, then use the TAG NAME And TAG TYPE specified in policy.
			if ((iter_maphostIDRecoveryFlowStatus->second.tagname.compare(LATEST_TAG) == 0)
				|| (iter_maphostIDRecoveryFlowStatus->second.tagname.compare(LATEST_TIME) == 0))
			{

				if (CDPCLI_ROLLBACK == iter_maphostIDRecoveryFlowStatus->second.recoverystatus)
				{
					DebugPrintf(SV_LOG_INFO, "Rollback Completed for Host ID : %s.\n", iter_maphostIDRecoveryFlowStatus->first.c_str());
					continue;
				}

				DebugPrintf(SV_LOG_INFO, "Host ID = %s, Tag Name = %s.\n", iter_maphostIDRecoveryFlowStatus->first.c_str(), LatestTagOrTime.c_str());
				//Assign Tag Type
				if (iter_maphostIDRecoveryFlowStatus->second.tagname.compare(LATEST_TIME) == 0)
					iter_maphostIDRecoveryFlowStatus->second.tagtype = TIME_TAGTYPE;
				else
					iter_maphostIDRecoveryFlowStatus->second.tagtype = FS_TAGTYPE;

				iter_maphostIDRecoveryFlowStatus->second.tagname = LatestTagOrTime;
				iter_maphostIDRecoveryFlowStatus->second.recoverystatus = TAG_VERIFIED;
			}
		}
	} while (0);
	
	DebugPrintf( SV_LOG_DEBUG, "Exiting %s\n", __FUNCTION__ );
	return bReturnValue;
}


//**************************************************************************
//
//MapPolicySettingsToFMA()
//Use this function to map recovery policy settings sent to FMA files 
//written
//
//Input		: NONE.
//Output	: Return structure of type RecoveryFlowStatus
//
//**************************************************************************
bool RollBackVM::MapPolicySettingsToFMA( RecoveryFlowStatus& recoveryFlowStatusPolicy,
	 RecoveryFlowStatus& recoveryFlowStatusFMA , StatusInfo& statInfo)
{
	DebugPrintf(SV_LOG_DEBUG, "Entering %s\n", __FUNCTION__);
	bool bReturnValue = true;

	if (recoveryFlowStatusPolicy.recoverystatus == CDPCLI_ROLLBACK)
	{
		if (recoveryFlowStatusFMA.recoverystatus == RECOVERY_STARTED)
			recoveryFlowStatusPolicy.recoverystatus = CDPCLI_ROLLBACK;
		else
			recoveryFlowStatusPolicy.recoverystatus = recoveryFlowStatusFMA.recoverystatus;
	}
	else if (recoveryFlowStatusPolicy.recoverystatus == TAG_VERIFIED)
	{
		if ( 0 == recoveryFlowStatusPolicy.tagname.compare(recoveryFlowStatusFMA.tagname))
		{
			recoveryFlowStatusFMA.tagtype = recoveryFlowStatusPolicy.tagtype;
			DebugPrintf(SV_LOG_INFO, "Same tag which is used in earlier rollback is sent.\n");
		}
		else if (recoveryFlowStatusFMA.tagname.empty())
		{
			DebugPrintf( SV_LOG_INFO, "First time rollback for this machine.\n");
			recoveryFlowStatusFMA.tagname = recoveryFlowStatusPolicy.tagname;
			recoveryFlowStatusFMA.tagtype = recoveryFlowStatusPolicy.tagtype;
			recoveryFlowStatusFMA.recoverystatus = recoveryFlowStatusPolicy.recoverystatus;
		}
		else if ( 0 != recoveryFlowStatusPolicy.tagname.compare(recoveryFlowStatusFMA.tagname) )
		{
			DebugPrintf(SV_LOG_ERROR, "Different Tag Name is sent for rollback.\n");
			DebugPrintf(SV_LOG_ERROR, "Previous rollback tag = %s, Current rollback tag = %s.\n", recoveryFlowStatusFMA.tagname.c_str(), recoveryFlowStatusPolicy.tagname.c_str());
			// we have to send fail case here.
			statInfo.ErrorMsg = "Possible recovery point is \"" + recoveryFlowStatusFMA.tagname + "\". Please select \"" + recoveryFlowStatusFMA.tagname+ "\" as recovery point and perform unplanned failover";
			//statInfo.ErrorMsg = "Previous failover for this machine used tag \"" + recoveryFlowStatusFMA.tagname + "\", please use same tag/corresponding time to retry failover once again.";
			statInfo.ErrorCode = "EA0615";
			statInfo.Tag = recoveryFlowStatusFMA.tagname;
			bReturnValue = false;
		}
	}
	else if (recoveryFlowStatusPolicy.recoverystatus == RECOVERY_STARTED )
	{
		//expect this in case of Linux.
		DebugPrintf(SV_LOG_INFO, "Linux Machines where recovery status = RECOVERY_STARTED.\n");
		recoveryFlowStatusFMA.tagname = recoveryFlowStatusPolicy.tagname;
		recoveryFlowStatusFMA.tagtype = recoveryFlowStatusPolicy.tagtype;
		recoveryFlowStatusFMA.recoverystatus = recoveryFlowStatusPolicy.recoverystatus;
	}

	DebugPrintf(SV_LOG_DEBUG, "Exiting %s\n", __FUNCTION__);
	return bReturnValue;
}

//**************************************************************************
//
//ListRecoveryStatusOfPolicy()
//Use this function to list recovery status for each of the machine in a 
//recovery policy.This helps for debugging.
//
//Input		: member variable of class RollBackVM()
//				map_hostIdToRecoveryFlowStatus;
//Output	: None.
//
//**************************************************************************
void RollBackVM::ListRecoveryStatusOfPolicy(std::map< const std::string, RecoveryFlowStatus >& map_hostIdToRecoveryFlowStatus)
{
	DebugPrintf(SV_LOG_DEBUG, "Entering %s\n", __FUNCTION__);

	map< std::string, RecoveryFlowStatus >::iterator iter_maphostIDRecoveryFlowStatus = map_hostIdToRecoveryFlowStatus.begin();
	for (; iter_maphostIDRecoveryFlowStatus != map_hostIdToRecoveryFlowStatus.end(); ++iter_maphostIDRecoveryFlowStatus)
	{
		DebugPrintf( SV_LOG_INFO, "Recovery Flow Details for Host Id %s.\n", iter_maphostIDRecoveryFlowStatus->first.c_str() );
		DebugPrintf( SV_LOG_INFO, "Tag Name = %s.\n", iter_maphostIDRecoveryFlowStatus->second.tagname.c_str() );
		DebugPrintf( SV_LOG_INFO, "Tag Type = %s.\n", iter_maphostIDRecoveryFlowStatus->second.tagtype.c_str());
		DebugPrintf( SV_LOG_INFO, "Recovery Status = %d.\n", iter_maphostIDRecoveryFlowStatus->second.recoverystatus );
	}

	DebugPrintf(SV_LOG_DEBUG, "Exiting %s\n", __FUNCTION__);
}


//**************************************************************************
//startRollbackCloud()
//
//Use this method to perform rollback opertions when Target/DR environment 
//is a cloud environment. This method is a wrapper which executes different
//other methods to accomplish rollback, massaging to adopt cloud environment
//
//Input			: NONE.
//Output		: Returns a bool depending on action completion.
//
//**************************************************************************
bool RollBackVM::startRollBackCloud()
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool bReturn = true;
	bool bUpdateOveralStatus = false;
	vector<std::string> vecRecoveredVMs;
	StatusInfo statInfoOveral;

	do
	{
		if (isFailback())
			SetFailbackProperties();

		ChangeVmConfig obj;
		obj.m_bDrDrill = m_bPhySnapShot;
		//TFS ID : 1051609
		obj.m_strDatafolderPath = m_strDatafolderPath;
		obj.m_bCloudMT = m_bCloudMT;
		obj.m_vConVersion = 4.0;
		m_strHostName = m_objLin.GetHostName();
		
		//Converting to lower case
		for(unsigned int i=0;i<m_strCloudEnv.length();i++)
		{
			m_strCloudEnv[i] = tolower(m_strCloudEnv[i]);
		}
		obj.m_strCloudEnv = m_strCloudEnv;
		m_bArraySnapshot = false;

		m_objLin.AssignSecureFilePermission(m_strDatafolderPath, true);
		m_objLin.AssignSecureFilePermission(m_strConfigFile);

#ifdef WIN32
		m_InMageCmdLogFile = m_strDatafolderPath + string("\\inmage_cmd_output.log");
#else
		m_bV2P = obj.m_bV2P;
		m_InMageCmdLogFile = m_strDatafolderPath + string("/inmage_cmd_output.log");
#endif
		obj.m_InMageCmdLogFile = m_InMageCmdLogFile;

		if (!GetRollbackDetailsFromConfigFile(statInfoOveral))
		{
			DebugPrintf(SV_LOG_ERROR, "Failed while unserializing the recovery information or finding disk number mapping for each host.\n");
			bUpdateOveralStatus = true;
			bReturn = false;
			break;
		}

		m_objLin.UpdateSrcHostStaus();
		
		//Here we need to make changes for the mapping information.TFS ID : 1051609
		obj.m_RecVmsInfo = m_RecVmsInfo;
		map<string, pair<string, string> >::iterator iter;

		std::map< std::string, StatusInfo > map_hostIdToStatusInfo;

		if (!PreProcessRecoveryPlan(map_hostIdToStatusInfo))
		{
			DebugPrintf(SV_LOG_INFO, "Failed to pre-process recovery plan.This has to be considered as an error.\n");
			if (map_hostIdToStatusInfo.empty())
			{
				//When Rollback Details are empty or there is failure to process all machine information. Should we have to write an Error COde or message here?
				bUpdateOveralStatus = true;
			}
			else
			{
				std::map< std::string, StatusInfo >::iterator iterStatusInfo = map_hostIdToStatusInfo.begin();
				while (iterStatusInfo != map_hostIdToStatusInfo.end())
				{
					m_objLin.m_mapHostIdToStatus[iterStatusInfo->first] = iterStatusInfo->second;
					iterStatusInfo++;
				}
			}
			bReturn = false;
			break;
		}

		//Display Recovery Status what is the Status for each Machine.
		ListRecoveryStatusOfPolicy(map_hostIDRecoveryFlowStatus);
		
		map<string, pair<string, string> > recoveryPairDetails;
		//Updates recovery progress for each host as started
		//rollbackDetails is a member variable of RollBackVM.h class and filled in GetRollbackDetailsFromConfigFile()
		for (iter = rollbackDetails.begin(); iter != rollbackDetails.end(); iter++)
		{
			StatusInfo statInfo;
			RecoveryFlowStatus RecStatus;
			bool bWriteRecoveryProgressFile = false;
			DebugPrintf(SV_LOG_INFO, "Host ID = %s.\n", iter->first.c_str());
			if (!ReadRecoveryProgress(iter->first, m_strDatafolderPath, RecStatus))
			{
				DebugPrintf(SV_LOG_INFO , "Recovery progress file does not exist for machine %s. Treat recovery on this machine as first time.\n", iter->first.c_str());
				bWriteRecoveryProgressFile = true;
			}
			if (!MapPolicySettingsToFMA((map_hostIDRecoveryFlowStatus.find(iter->first))->second, RecStatus, statInfo))
			{
				statInfo.HostId = iter->first;
				statInfo.Status = "2";
				m_objLin.m_mapHostIdToStatus[iter->first] = statInfo;
				continue;
			}

			if (bWriteRecoveryProgressFile)
			{
				if (!UpdateRecoveryProgress(iter->first, m_strDatafolderPath, RecStatus))
				{
					DebugPrintf(SV_LOG_ERROR, "Failed to update the recovery progress in the local file for host - %s.\n", iter->first.c_str());
					//Currently ignoring the failure case.//if ignored what will happen if there happens a crash?
				}
			}

			DebugPrintf(SV_LOG_INFO, "Tag Type = %s.\n", RecStatus.tagtype.c_str());
			DebugPrintf(SV_LOG_INFO, "TagName = %s.\n", RecStatus.tagname.c_str());
			DebugPrintf(SV_LOG_INFO, "Recovery Status : %d.\n",RecStatus.recoverystatus );
			
			if ((RecStatus.tagname.empty() && RecStatus.tagtype.empty()) || (RecStatus.recoverystatus  == RECOVERY_STARTED))
				recoveryPairDetails[iter->first] = iter->second;
			else
				recoveryPairDetails[iter->first] = make_pair(RecStatus.tagname, RecStatus.tagtype);

			m_mapOfHostToRecoveryProgress[iter->first] = RecStatus;
		}

		RecoveryFlowStatus RecStatus;
		list<CdpcliJob> stCdpcliJobs;
		rollbackDetails = recoveryPairDetails;
		recoveryPairDetails.clear();
		for (iter = rollbackDetails.begin(); iter != rollbackDetails.end(); iter++)
		{
            DebugPrintf(SV_LOG_INFO,"----------------------------------------------------------------------------------------------\n");
            DebugPrintf(SV_LOG_INFO,"Virtual Machine Name - %s\n",iter->first.c_str());
		    DebugPrintf(SV_LOG_INFO,"----------------------------------------------------------------------------------------------\n");
			m_VmName    = iter->first;
            m_TagValue  = iter->second.first;
            m_TagType   = iter->second.second;
			StatusInfo statInfo;
			m_objLin.m_PlaceHolder.clear();

			string strCdpCliCmd = "";
			if (m_mapOfHostToRecoveryProgress.find(iter->first) != m_mapOfHostToRecoveryProgress.end())
				RecStatus = m_mapOfHostToRecoveryProgress.find(iter->first)->second;

			if (false == m_bSkipCdpcli && (RecStatus.recoverystatus == TAG_VERIFIED || RecStatus.recoverystatus == RECOVERY_STARTED)) // This check indicates we should not skip cdpcli rollback for this host
			{
				/*********CALL MAIN ROUTINE FROM HERE***********************/
				/*Ignoring return value,so that even if rollback process fails 
				for one VM ,it should continue with others FIX ME..discuss*/
				//Sending RecStatus. This would help in getting tag name returned in case of Linux Machines.Also helps to skip already executed tasks.Can be enhanced.
				if (false == RollBackVMs(iter->first, iter->second.first, iter->second.second, strCdpCliCmd, statInfo , RecStatus))
				{
					DebugPrintf(SV_LOG_ERROR,"Failed to perform rollback/snapshot on %s\n", m_VmName.c_str());

					statInfo.HostId = iter->first;
					statInfo.Status = "2";

					statInfo.Tag = "";
					if (!RecStatus.tagname.empty())
					{
						DebugPrintf(SV_LOG_INFO, "Fail Case : Adding tag name as %s.\n", RecStatus.tagname.c_str());
						statInfo.Tag = RecStatus.tagname;
					}

					m_objLin.m_mapHostIdToStatus[iter->first] = statInfo;
					continue;
				}
			}
			else
			{
				DebugPrintf(SV_LOG_DEBUG,"Skipping the rollback/snapshot...\n");
			}

			statInfo.Tag = "";
			if (!RecStatus.tagname.empty())
			{
				DebugPrintf(SV_LOG_INFO, "Success Case : Adding tag name as %s.\n", RecStatus.tagname.c_str());
				statInfo.Tag = RecStatus.tagname;
			}

			m_objLin.m_mapHostIdToStatus[iter->first] = statInfo;

			CdpcliJob cdpCliJob;
			cdpCliJob.uniqueid = m_VmName;
			cdpCliJob.cdpclicmd = strCdpCliCmd;
			cdpCliJob.cmderrfile = m_strDatafolderPath + m_VmName + string("_cdpclierr.txt") ;
			cdpCliJob.cmdoutfile = m_strDatafolderPath + m_VmName + string("_cdpcliout.txt") ;
			cdpCliJob.cmdstatusfile = m_strDatafolderPath + m_VmName + string("_cdpclistatus.txt") ;

			if (strCdpCliCmd.empty() && RecStatus.recoverystatus == TAG_VERIFIED)
			{
				std::ofstream outFileStream;
				outFileStream.open(cdpCliJob.cmdstatusfile.c_str(), std::ios::trunc);
				if (!outFileStream.is_open())
				{
					DebugPrintf(SV_LOG_ERROR, "Failed to open %s for writing\n", cdpCliJob.cmdstatusfile.c_str());
				}
				else
				{
					outFileStream << m_VmName << ":0:0:0";
					outFileStream.close();
				}
			}
			else
				stCdpcliJobs.push_back(cdpCliJob);

			recoveryPairDetails.insert(make_pair(iter->first, iter->second));
		}

		if(m_bCloudMT)
		{
			obj.m_nMaxCdpJobs = stCdpcliJobs.size();
		}
		DebugPrintf(SV_LOG_DEBUG,"Number of cdpcli's to execute parallely - %d\n", obj.m_nMaxCdpJobs);

		while(!stCdpcliJobs.empty())
		{
			list<CdpcliJob> lstCdpJobs;
			list<CdpcliJob>::iterator iterCdp = stCdpcliJobs.begin();
			if(stCdpcliJobs.size() >= obj.m_nMaxCdpJobs)
			{
				int cdpCount = 0; 
				for(;cdpCount < obj.m_nMaxCdpJobs; cdpCount++)
				{
					if (m_mapOfHostToRecoveryProgress.find(iterCdp->uniqueid) != m_mapOfHostToRecoveryProgress.end())
						RecStatus = m_mapOfHostToRecoveryProgress.find(iterCdp->uniqueid)->second;
					if (RecStatus.recoverystatus == TAG_VERIFIED || RecStatus.recoverystatus == RECOVERY_STARTED) // This check indicates we should not skip cdpcli rollback for this host
						lstCdpJobs.push_back(*iterCdp);
					stCdpcliJobs.pop_front();
					iterCdp = stCdpcliJobs.begin();
				}
			}
			else
			{
				for(;iterCdp != stCdpcliJobs.end(); iterCdp++)
				{
					if (m_mapOfHostToRecoveryProgress.find(iterCdp->uniqueid) != m_mapOfHostToRecoveryProgress.end())
						RecStatus = m_mapOfHostToRecoveryProgress.find(iterCdp->uniqueid)->second;
					if (RecStatus.recoverystatus == TAG_VERIFIED || RecStatus.recoverystatus == RECOVERY_STARTED) // This check indicates we should not skip cdpcli rollback for this host
						lstCdpJobs.push_back(*iterCdp);
				}
				stCdpcliJobs.clear();
			}

			if (!m_bSkipCdpcli && !lstCdpJobs.empty())
			{
				//Preparing the input file for inmexec binary
				string strFileName = m_strDatafolderPath + string("inmexec_input_") + GetCurrentLocalTime() + string(".txt");
				if(false == PrpareInmExecFile(lstCdpJobs, strFileName))
				{
					DebugPrintf(SV_LOG_ERROR,"Failed to prepare the input file for inmexec, Failed to rollback some of the VM's.\n");
				}

				string strOutputFileName = m_strDatafolderPath + string("inmexec_output_") + GetCurrentLocalTime() + string(".txt");

				//Execute the inmexec binary for cdpcli recovery\snapshot operation
				if (false == RunInmExec(strFileName, strOutputFileName))
				{
					DebugPrintf(SV_LOG_ERROR,"Failed to run the inmexec, Failed to rollback some of the VM's.\n");
				}
			}
		}

		//Need to check the status of each cdpcli which are exectued.
		rollbackDetails.clear();
		for (iter = recoveryPairDetails.begin(); iter != recoveryPairDetails.end(); iter++)
		{
			StatusInfo statInfo;

			map<string, StatusInfo>::iterator iterStatus = m_objLin.m_mapHostIdToStatus.find(iter->first);
			if (iterStatus != m_objLin.m_mapHostIdToStatus.end())
				statInfo = iterStatus->second;
			
			int cdpclistatus = 1;    //Initially marked to failed status
			string cdpCliOutputfile = m_strDatafolderPath + iter->first + string("_cdpclistatus.txt") ;
			ifstream FIN(cdpCliOutputfile.c_str(), std::ios::in);
			if(! FIN.is_open())
			{
				DebugPrintf(SV_LOG_ERROR, "Failed to open the file - %s\n", cdpCliOutputfile.c_str());
				DebugPrintf(SV_LOG_ERROR,"Considering Recovery/Dr-Drill failed for VM : %s\n",iter->first.c_str());

				/***** Recovery has been failed for this VM, so printing the logs which are required for debugging ****/

				//Reading the cdpcli status file
				DebugPrintf(SV_LOG_INFO, "\n********** Reading File  : [%s] **********\n\n", cdpCliOutputfile.c_str());
				ReadLogFileAndPrint(cdpCliOutputfile);
				DebugPrintf(SV_LOG_INFO, "********** END **********\n");
				
				//Reading the cdpcli error file
				cdpCliOutputfile = m_strDatafolderPath + iter->first + string("_cdpclierr.txt");
				DebugPrintf(SV_LOG_INFO, "\n********** Reading File  : [%s] **********\n\n", cdpCliOutputfile.c_str());
				ReadLogFileAndPrint(cdpCliOutputfile);
				DebugPrintf(SV_LOG_INFO, "********** END **********\n");

				//Reading the cdpcli output file
				cdpCliOutputfile = m_strDatafolderPath + iter->first + string("_cdpcliout.txt");
				DebugPrintf(SV_LOG_INFO, "\n********** Reading File  : [%s] **********\n\n", cdpCliOutputfile.c_str());
				ReadLogFileAndPrint(cdpCliOutputfile);
				DebugPrintf(SV_LOG_INFO, "********** END **********\n");

				statInfo.HostId = iter->first;
				statInfo.Status = "2";
				statInfo.ErrorMsg = "Unrecoverable Internal error while executing cdpcli process on Master Target server " + m_strHostName;
				statInfo.ErrorCode = "EA0608";
				statInfo.PlaceHolder.Properties[MT_HOSTNAME] = m_strHostName;
				m_objLin.m_mapHostIdToStatus[iter->first] = statInfo;
				continue;
			}
			string strLineRead;
			while(getline(FIN, strLineRead))
			{
				if(strLineRead.empty())
					continue;
				vector<std::string> v;
				std::string Separator = std::string(":");
				size_t pos = 0;

				size_t found = strLineRead.find(Separator, pos);
				while(found != std::string::npos)
				{
					DebugPrintf(SV_LOG_DEBUG,"Element - [%s]\n",strLineRead.substr(pos,found-pos).c_str());
					v.push_back(strLineRead.substr(pos,found-pos));
					pos = found + 1; 
					found = strLineRead.find(Separator, pos);            
				}
				DebugPrintf(SV_LOG_DEBUG,"Element - [%s]\n",strLineRead.substr(pos).c_str());
				v.push_back(strLineRead.substr(pos));

				std::stringstream ss;
				ss<<v[2];
				ss>>cdpclistatus;
			}
			FIN.close();

			if(0 == cdpclistatus)
			{
				DebugPrintf(SV_LOG_INFO,"Successfully performed the cdpcli operation for host : %s\n",iter->first.c_str());
				rollbackDetails.insert(make_pair(iter->first, iter->second));

				if (m_mapOfHostToRecoveryProgress.find(iter->first) != m_mapOfHostToRecoveryProgress.end())
					RecStatus = m_mapOfHostToRecoveryProgress.find(iter->first)->second;
				if (RecStatus.recoverystatus == TAG_VERIFIED || RecStatus.recoverystatus == RECOVERY_STARTED) // This check indicates we should update cdpcli rollback completed for this host
				{
					RecStatus.recoverystatus = CDPCLI_ROLLBACK;
					RecStatus.tagname = "";
					if (!UpdateRecoveryProgress(iter->first, m_strDatafolderPath, RecStatus))
					{
						DebugPrintf(SV_LOG_ERROR, "Failed to update the recovery progress in the local file for host - %s.\n", iter->first.c_str());
						//Currently ignoring the failure case.
					}
				}
			}
			else
			{
				/***** Recovery has been failed for this VM, so printing the logs which are required for debugging ****/

				//Reading the cdpcli status file
				DebugPrintf(SV_LOG_INFO, "\n********** Reading File  : [%s] **********\n\n", cdpCliOutputfile.c_str());
				ReadLogFileAndPrint(cdpCliOutputfile);
				DebugPrintf(SV_LOG_INFO, "********** END **********\n");

				//Reading the cdpcli error file
				cdpCliOutputfile = m_strDatafolderPath + iter->first + string("_cdpclierr.txt");
				DebugPrintf(SV_LOG_INFO, "\n********** Reading File  : [%s] **********\n\n", cdpCliOutputfile.c_str());
				ReadLogFileAndPrint(cdpCliOutputfile);
				DebugPrintf(SV_LOG_INFO, "********** END **********\n");

				//Reading the cdpcli output file
				cdpCliOutputfile = m_strDatafolderPath + iter->first + string("_cdpcliout.txt");
				DebugPrintf(SV_LOG_INFO, "\n********** Reading File  : [%s] **********\n\n", cdpCliOutputfile.c_str());
				ReadLogFileAndPrint(cdpCliOutputfile);
				DebugPrintf(SV_LOG_INFO, "********** END **********\n");

				DebugPrintf(SV_LOG_ERROR, "Cdpcli operation failed for host : %s with error : %d\n", iter->first.c_str(), cdpclistatus);
				DebugPrintf(SV_LOG_ERROR, "For cdpcli output check the log file %s in MT plan name folder\n", cdpCliOutputfile.c_str());

				stringstream strError;
				strError << cdpclistatus;
				statInfo.HostId = iter->first;
				statInfo.Status = "2";
				statInfo.ErrorMsg = "Unrecoverable Internal error while executing cdpcli process on Master Target server " + m_strHostName;
				statInfo.ErrorCode = "EA0608";
				statInfo.PlaceHolder.Properties[MT_HOSTNAME] = m_strHostName;
				m_objLin.m_mapHostIdToStatus[iter->first] = statInfo;
			}
		}

#ifndef WIN32
		// Added sleep for Bug 12128763: [V2A] Failback post massaging failed using Ubuntu MT
		// Reference link : https:////microsoft.visualstudio.com/WSSC/_workitems?id=12128763&_a=edit
		sleep(POST_CDPCLI_ROLLBACK_SLEEPTIME_IN_SEC);
#endif

		DebugPrintf(SV_LOG_DEBUG,"\n----------------------------------------------------------------------------------------------\n");
		DebugPrintf(SV_LOG_INFO,"***Started Post Rollback Operation***\n");
		DebugPrintf(SV_LOG_DEBUG,"----------------------------------------------------------------------------------------------\n\n");
		iter = rollbackDetails.begin();
		while(iter != rollbackDetails.end())
		{
			if (m_mapOfHostToRecoveryProgress.find(iter->first) != m_mapOfHostToRecoveryProgress.end())
			{
				RecStatus = m_mapOfHostToRecoveryProgress.find(iter->first)->second;
				if (RecStatus.recoverystatus == POST_ROLLBACK || RecStatus.recoverystatus == DISK_SIGNATURE_UPDATE || RecStatus.recoverystatus == RECOVERY_COMPLETED)
				{
					DebugPrintf(SV_LOG_DEBUG, "Skipping post rollback operation for this host : %s . Post rollback operation already completed for this host.", iter->first.c_str());
					vecRecoveredVMs.push_back(iter->first); iter++;
					continue;
				}
			}
			bool bFailed = false;
			m_VmName    = iter->first;
			StatusInfo statInfo;

			map<string, StatusInfo>::iterator iterStatus = m_objLin.m_mapHostIdToStatus.find(iter->first);
			if (iterStatus != m_objLin.m_mapHostIdToStatus.end())
				statInfo = iterStatus->second;

            DebugPrintf(SV_LOG_DEBUG,"Cloud Machine Host Name - %s\n",iter->first.c_str());
#ifdef WIN32
			obj.m_mapOfVmInfoForPostRollback.clear();
			map<std::string, map<string,string> >::iterator iterVmRecInfo = m_RecVmsInfo.m_mapVMRecoveryInfo.find(iter->first);
			map<std::string, map<string,string> >::iterator iterVmVolPairInfo = m_RecVmsInfo.m_volumeMapping.find(iter->first);
			if(iterVmRecInfo != m_RecVmsInfo.m_mapVMRecoveryInfo.end())
			{
				map<string,string>::iterator iterHostSysInfo = iterVmRecInfo->second.begin();
				for(; iterHostSysInfo != iterVmRecInfo->second.end(); iterHostSysInfo++)
				{
					if(iterHostSysInfo->first.compare("MachineType") == 0)
					{
						obj.m_mapOfVmInfoForPostRollback.insert(make_pair(iterHostSysInfo->first, iterHostSysInfo->second));
						DebugPrintf(SV_LOG_DEBUG,"Machine Type : %s\n", iterHostSysInfo->second.c_str());
					}
					else if(iterHostSysInfo->first.compare("OperatingSystem") == 0)
					{
						DebugPrintf(SV_LOG_DEBUG,"Found Opearting system : %s\n", iterHostSysInfo->second.c_str());
						obj.m_mapOfVmInfoForPostRollback.insert(make_pair(iterHostSysInfo->first, iterHostSysInfo->second));
					}
					else if((iterHostSysInfo->first.compare("TagName") == 0) || (iterHostSysInfo->first.compare("TagType") == 0) || (iterHostSysInfo->first.compare("HostId") == 0) || (iterHostSysInfo->first.compare("HostName") == 0) || (iterHostSysInfo->first.compare("Failback") == 0))
					{
						//For now just ignore these.
						DebugPrintf(SV_LOG_DEBUG, "Skipping processing for vm:%s property: %s...\n", iter->first.c_str(), iterHostSysInfo->first.c_str());
					}
				}

				if (!ParseSourceOsVolumeDetails(iter->first, obj))
				{
					DebugPrintf(SV_LOG_ERROR, "Post rollback operations could not be done for host - %s as system volume information is not sent by CS.\n", iter->first.c_str());
					bFailed = true;
				}
				
				if (!bFailed)
				{
					bool rv = false;
					if (isVirtualizationTypeVmWare()){
						rv = obj.MakeChangesPostRollbackVmWare(iter->first);
					}
					else{
						rv = obj.MakeChangesPostRollbackCloud(iter->first);
					}

					if (!rv)
					{
						DebugPrintf(SV_LOG_ERROR, "Post rollback operations failed for host - %s\n", iter->first.c_str());
						bFailed = true;
					}
					else
					{
						DebugPrintf(SV_LOG_INFO, "\n******Successfully done post rollback operations for host - %s*******\n", iter->first.c_str());
					}
				}
			}
			else
			{
				DebugPrintf(SV_LOG_ERROR,"\nPost rollback operations failed, because host - %s not found\n",iter->first.c_str());
			}
#else
			runFlushBufAndKpartx(iter->first); //Flush the buffer of device using blockdev

			//Ignoring the return value.
			if(false == editLVMconfFile(iter->first))
			{
				DebugPrintf(SV_LOG_ERROR,"[ERROR]: Failed to Edit the LVM.Conf file\n");
			}

			if(false == pvScan())
			{
				DebugPrintf(SV_LOG_ERROR,"[ERROR]: Failed to PV Scan in Master Target\n");
			}
			
			if(false == vgScan())
			{
				DebugPrintf(SV_LOG_ERROR,"[ERROR]: Failed to VG Scan in Master Target\n");
			}

			obj.m_mapOfVmToSrcAndTgtDisks = m_mapOfVmToSrcAndTgtDisks;
			obj.m_mapOfVmNameToId = m_mapOfVmNameToId;
			string strOsType = "";

			string strXmlData;
			string strCxApiResXmlFile = std::string(m_objLin.getVxInstallPath()) +  std::string("failover_data/") + m_VmName + string("_cx_api.xml"); 
			strXmlData = m_objLin.prepareXmlForGetHostInfo(m_VmName);
			
			if(false == m_objLin.processXmlData(strXmlData, strCxApiResXmlFile))
			{
				DebugPrintf(SV_LOG_ERROR,"Failed to process the request XML data of CX API call for host : %s\n", m_VmName.c_str());
			}

			if(false == obj.PostRollbackCloudVirtualConfig(m_VmName)) //for P2V changes. (When required need to add this for windows)
            {
                DebugPrintf(SV_LOG_ERROR,"Failed to make changes required to convert Physical machine to Virtual Machine for - %s\n", iter->first.c_str());
				bFailed = true;
            }
			
			//Ignoring the return value.
			if(false == repalceOldLVMconfFile(iter->first))
			{
				DebugPrintf(SV_LOG_ERROR,"[ERROR]: Failed to replace the LVM.Conf file\n");
			}

			if(false == pvScan())
			{
				DebugPrintf(SV_LOG_ERROR,"[ERROR]: Failed to PV Scan in Master Target\n");
			}

			if(false == vgScan())
			{
				DebugPrintf(SV_LOG_ERROR,"[ERROR]: Failed to VG Scan in Master Target\n");
			}

			//Added flush dev twice otherwise DI issue is coming
			map<string, map<string, string> >::iterator iterH;
			if((iterH = m_mapOfVmToSrcAndTgtDisks.find(iter->first)) != m_mapOfVmToSrcAndTgtDisks.end())
			{
				map<string,string>::iterator mapIterDev = iterH->second.begin();
				for(;mapIterDev != iterH->second.end(); mapIterDev++)
				{
					blockDevFlushBuffer(mapIterDev->second);

					//TODO-V2: as per vCon worflow blockDevFlushBuffer is calling again with multipath device name.
					//         Conform that is it required.
					std::string strStandardDevice;
					getStandardDevice(mapIterDev->second, strStandardDevice);
					if(strStandardDevice.find("/dev") == string::npos)
						strStandardDevice = "/dev/" + strStandardDevice;
					blockDevFlushBuffer(strStandardDevice);
				}
			}

			//Proper clean up
			removeLogicalVolumes(obj.m_lvs);
			obj.m_lvs.clear();

			if ( boost::algorithm::iequals(m_strCloudEnv,"azure") ||
				 boost::algorithm::iequals(m_strCloudEnv, MT_ENV_VMWARE) 
			   )
			{
				list<string> lstLsscsiId;
				findLscsiDevice(iter->first, lstLsscsiId);//Ignoring the return value.

				removeMultipathDevices(iter->first);
				removeLsscsiDevice(lstLsscsiId);
			}

			//Collecting debugging logs
			collectMessagesLog();
			collectDmsegLog();
#endif
			if(bFailed)
			{
				statInfo.HostId = iter->first;
				statInfo.PlaceHolder.clear();
				statInfo.PlaceHolder.Properties[MT_HOSTNAME] = m_strHostName;

				statInfo.Status = "2";
				statInfo.ErrorMsg = "Unable to insert boot-time script/drivers in to the recovered volumes for protected item " + iter->first;
				statInfo.ErrorCode = "EA0609";				
				m_objLin.m_mapHostIdToStatus[iter->first] = statInfo;

				DebugPrintf(SV_LOG_ERROR, "Unable to insert boot-time script/drivers in to the recovered volumes for protected item - %s.\n", iter->first.c_str());
				iter++;
                continue;
			}

			RecStatus.recoverystatus = POST_ROLLBACK;
			RecStatus.tagname = "";
			if (!UpdateRecoveryProgress(iter->first, m_strDatafolderPath, RecStatus))
			{
				DebugPrintf(SV_LOG_ERROR, "Failed to update the recovery progress in the local file for host - %s.\n", iter->first.c_str());
				//Currently ignoring the failure case.
			}

			//Push all the VMs which are rollbacked successfully.
			//if we are here, it means this VM is rollbacked.
			vecRecoveredVMs.push_back(iter->first);
			iter++;
		}
	}while(0);

	DebugPrintf(SV_LOG_INFO,"----------------------------------------------------------------------------------------------\n");

	vector<std::string> vecFinalRecoveredVms;
	vector<std::string>::iterator iter;
	if(vecRecoveredVMs.empty())
    {
		DebugPrintf(SV_LOG_ERROR,"\n\n******  NONE OF THE VMs ARE RECOVERED SUCCESSFULLY ******\n");
        bReturn = false;
	}
	else
	{		
#ifdef WIN32
		if (!usingDiskBasedProtection())
		{

			//Unmount and delete all mount points for this VM.
			//Even if some mount points are not unmounted/deleted, continuing with remaining VMs.
			DebugPrintf(SV_LOG_DEBUG, "\nClearing all the mount points for the recovered VMs.\n");
			UnmountAndDeleteMtPoints(vecRecoveredVMs);
			//Restore Disk signatures on all disks of recovered VMs.
			DebugPrintf(SV_LOG_DEBUG, "\nRestoring the disk signatures of all the recovered VMs.\n");

			//Re-updation will create problem if the disk is dynamic. So skip is required.
			for (iter = vecRecoveredVMs.begin(); iter != vecRecoveredVMs.end(); iter++)
			{
				RecoveryFlowStatus RecStatus;
				if (m_mapOfHostToRecoveryProgress.find(*iter) != m_mapOfHostToRecoveryProgress.end())
				{
					RecStatus = m_mapOfHostToRecoveryProgress.find(*iter)->second;
					if (RecStatus.recoverystatus == DISK_SIGNATURE_UPDATE || RecStatus.recoverystatus == RECOVERY_COMPLETED)
					{
						DebugPrintf(SV_LOG_DEBUG, "Skipping Disk Signature update for this host : %s . This operation is already completed for this host.", iter->c_str());
						vecFinalRecoveredVms.push_back(*iter);
						continue;
					}
				}

				if (false == RestoreDiskSignature(*iter))
				{
					DebugPrintf(SV_LOG_ERROR, "Failed to restore the disk signatures for recovered VM %s.\n", iter->c_str());
					StatusInfo statInfo;
					map<string, StatusInfo>::iterator iterStatus = m_objLin.m_mapHostIdToStatus.find(*iter);
					if (iterStatus != m_objLin.m_mapHostIdToStatus.end())
						statInfo = iterStatus->second;
					statInfo.PlaceHolder.Properties[MT_HOSTNAME] = m_strHostName;
					statInfo.HostId = *iter;
					statInfo.Status = "2";
					statInfo.ErrorMsg = "Unrecoverable error occurred while restoring disk signatures for protected item";
					statInfo.ErrorCode = "EA0603";
					m_objLin.m_mapHostIdToStatus[*iter] = statInfo;
				}
				else
				{
					RecStatus.recoverystatus = DISK_SIGNATURE_UPDATE;
					RecStatus.tagname = "";
					if (!UpdateRecoveryProgress(*iter, m_strDatafolderPath, RecStatus))
					{
						DebugPrintf(SV_LOG_ERROR, "Failed to update the recovery progress in the local file for host - %s.\n", iter->c_str());
						//Currently ignoring the failure case.
					}
					vecFinalRecoveredVms.push_back(*iter);
				}
			}
		} else{
			vecFinalRecoveredVms = vecRecoveredVMs;
		}
#else
		vecFinalRecoveredVms = vecRecoveredVMs;
#endif

		if (vecFinalRecoveredVms.empty())
		{
			DebugPrintf(SV_LOG_ERROR, "\n\n******  NONE OF THE VMs ARE RECOVERED SUCCESSFULLY ******\n");
			bReturn = false;
		}
		else
		{
			if (m_bPhySnapShot)
				DebugPrintf(SV_LOG_INFO, "\n\n******  SUCCESSFULLY COMPLETED THE SNAPSHOT OPERATION FOR THE FOLLOWING VMs - \n");
			else
				DebugPrintf(SV_LOG_INFO, "\n\n******  THE FOLLOWING VMs ARE RECOVERED - \n");

			iter = vecFinalRecoveredVms.begin();
			for (int i = 1; iter != vecFinalRecoveredVms.end(); iter++, i++)
			{
				map<string, string>::iterator iterMap = m_mapOfVmNameToId.find(*iter);
				if (iterMap != m_mapOfVmNameToId.end())
				{
					if (iter->compare(iterMap->second) != 0)
						DebugPrintf(SV_LOG_INFO, "%d -> InMageHostId = %s\tHostName = %s\n", i, iter->c_str(), iterMap->second.c_str());
					else
						DebugPrintf(SV_LOG_INFO, "%d -> InMageHostId = %s\tHostName = %s\n", i, iter->c_str(), iter->c_str());
				}
				else
					DebugPrintf(SV_LOG_INFO, "%d. %s\n", i, iter->c_str());

				StatusInfo statInfo;

				map<string, StatusInfo>::iterator iterStatus = m_objLin.m_mapHostIdToStatus.find(*iter);
				if (iterStatus != m_objLin.m_mapHostIdToStatus.end())
					statInfo = iterStatus->second;

				statInfo.HostId = *iter;
				statInfo.Status = "1";
				statInfo.ErrorMsg = "Successfully completed recovery operation at MT for host : " + *iter;
				m_objLin.m_mapHostIdToStatus[*iter] = statInfo;

				RecoveryFlowStatus RecStatus;
				RecStatus.recoverystatus = RECOVERY_COMPLETED;
				RecStatus.tagname = "";
				if (!UpdateRecoveryProgress(*iter, m_strDatafolderPath, RecStatus))
				{
					DebugPrintf(SV_LOG_ERROR, "Failed to update the recovery progress in the local file for host - %s.\n", iter->c_str());
					//Currently ignoring the failure case.
				}
			}
		}
	}

	if (!m_objLin.UpdateRecoveryStatusInFile(m_strResultFile, statInfoOveral, bUpdateOveralStatus))
	{
		DebugPrintf(SV_LOG_ERROR, "Serialization of recovered vms informaton failed.\n");
	}
	else
	{
		for (iter = vecFinalRecoveredVms.begin(); iter != vecFinalRecoveredVms.end(); iter++)
			DeleteRecoveryProgressFiles(*iter, m_strDatafolderPath);                 //Ignoring the return value.
	}

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bReturn;
}


bool RollBackVM::startRollBack()
{
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	
	taskInfo_t currentTask, nextTask;
	bool bUpdateToCXbyMTid = false;

	vector<std::string> vecFinalRecoveredVms;
	vector<std::string> vecRecoveredVMs;
	m_mapHostToType.clear();
	bool bReturn = true;
	do
	{
#ifdef WIN32
		string sysDrive = "c:\\windows\\";
		char strVol[MAX_PATH]={0};
		if(GetWindowsDirectory((LPTSTR)strVol,MAX_PATH))
		{
			sysDrive.clear();
			sysDrive = string(strVol);
		}
		if(m_bPhySnapShot)
			sysDrive = sysDrive + string("\\temp\\") + SNAPSHOT_VM_FILE_NAME;
		else
			sysDrive = sysDrive + string("\\temp\\") + ROLLBACK_VM_FILE_NAME;

		if(checkIfFileExists(sysDrive))
		{
			if(-1 == remove(sysDrive.c_str()))
			{
				DebugPrintf(SV_LOG_ERROR,"Failed to delete the file - %s\n",sysDrive.c_str());
			}
			else
			{
				DebugPrintf(SV_LOG_INFO,"SuccessFully deleted stale file - %s\n",sysDrive.c_str());			
			}
		}
		m_InMageCmdLogFile = m_strDatafolderPath + string("\\inmage_cmd_output.log");
#else
		m_InMageCmdLogFile = m_strDatafolderPath + string("/inmage_cmd_output.log");
#endif

		ChangeVmConfig obj;
		obj.m_bDrDrill = m_bPhySnapShot;
		obj.m_strDatafolderPath = m_strDatafolderPath;
		obj.m_InMageCmdLogFile = m_InMageCmdLogFile;

		m_objLin.AssignSecureFilePermission(m_strDatafolderPath);

		m_objLin.m_strDataDirPath = m_strDatafolderPath;
		m_objLin.m_strCxPath = m_strCxPath;

#ifndef WIN32
		if(!m_bPhySnapShot)
		{
			m_objLin.GetDiskUsage();
			m_objLin.GetMemoryInfo();
		}
#endif
		if(m_bPhySnapShot)
			InitDrdrillTaskUpdate();
		else
			InitRecoveryTaskUpdate();
		m_objLin.UpdateTaskInfoToCX();

		if(!m_bPhySnapShot)
		{
			//Updating Task1 as completed and Task2 as inprogress
			currentTask.TaskNum = INM_VCON_TASK_1;
			currentTask.TaskStatus = INM_VCON_TASK_COMPLETED;
			nextTask.TaskNum = INM_VCON_TASK_2;
			nextTask.TaskStatus = INM_VCON_TASK_INPROGRESS;
			m_objLin.ModifyTaskInfoForCxApi(currentTask, nextTask);
			m_objLin.UpdateTaskInfoToCX();

			currentTask.TaskNum = INM_VCON_TASK_2;
			nextTask.TaskNum = INM_VCON_TASK_3;

			if (m_strPlanId.empty())
			{
				DebugPrintf(SV_LOG_INFO, "WMI based recovery selected. Proceeding recovery without downlaoding any files.\n");
			}
			else
			{
				if(false == m_objLin.ProcessCxPath()) //This will download all the required files from the given CX Path.
				{
					currentTask.TaskStatus = INM_VCON_TASK_FAILED;
					currentTask.TaskErrMsg = "Failed to download the required files from the CX path : " + m_strCxPath;
					currentTask.TaskFixSteps = "Check the existance CX path and the required files inside that one. Rerun the job again after specifying the correct details.";
					nextTask.TaskStatus = INM_VCON_TASK_QUEUED;
					bUpdateToCXbyMTid = true;

					DebugPrintf(SV_LOG_ERROR,"[ERROR]Failed to download the required files from the given CX path %s\n", m_strCxPath.c_str());
					DebugPrintf(SV_LOG_ERROR,"Can't Proceed further, Stopping the Recovery/Snapshot operation.\n");
					bReturn = false;
					break;
				}
			}
			m_objLin.AssignSecureFilePermission(m_strDatafolderPath + std::string(RECOVERY_FILE_NAME));
		}
		else
		{
#ifdef WIN32
			currentTask.TaskNum = INM_VCON_TASK_5;
			nextTask.TaskNum = INM_VCON_TASK_6;
#else
			currentTask.TaskNum = INM_VCON_TASK_4;
			nextTask.TaskNum = INM_VCON_TASK_5;
#endif
		}

		if(false == obj.xGetvConVersion())
		{
			DebugPrintf(SV_LOG_ERROR,"\nFailed to read version details from Recovery XML file\n\n");
			obj.m_vConVersion = 1.2;
		}

		m_vConVersion = obj.m_vConVersion;
		m_bV2P = obj.m_bV2P;

		if(m_bPhySnapShot)
			m_bArraySnapshot = obj.UpdateArraySnapshotFlag();  //Updates the arraybased snapshot falg
		else
			m_bArraySnapshot = false;
		
        //This function will fill values in the "rollbackDetails"        
        if(false == xGetHostDetailsFromXML())
		{
			currentTask.TaskErrMsg = "Failed to read rollback details from Recovery XML file. For extended Error information download EsxUtil.log in vCon Wiz";
			currentTask.TaskFixSteps = "Check the File Existance, rerun the job again if issue persists contact inmage customer support.";
			currentTask.TaskStatus = INM_VCON_TASK_FAILED;
			nextTask.TaskStatus = INM_VCON_TASK_QUEUED;
			bUpdateToCXbyMTid = true;

			DebugPrintf(SV_LOG_ERROR,"\nFailed to read rollback details from Recovery XML file\n\n");
			bReturn = false;
			break;
		}

		m_objLin.m_mapVmIdToVmName = m_mapOfVmNameToId;
		obj.m_mapOfVmNameToId = m_mapOfVmNameToId;

		m_objLin.UpdateSrcHostStaus();

		map<string, pair<string, string> >::iterator iter;
		RecoveryFlowStatus RecStatus;
		//Updates recovery progress for each host as started
		for (iter = rollbackDetails.begin(); iter != rollbackDetails.end(); iter++)
		{
			if (!ReadRecoveryProgress(iter->first, m_strDatafolderPath, RecStatus))
			{
				if (!UpdateRecoveryProgress(iter->first, m_strDatafolderPath, RecStatus))
				{
					DebugPrintf(SV_LOG_ERROR, "Failed to update the recovery progress in the local file for host - %s.\n", iter->first.c_str());
					//Currently ignoring the failure case.
				}
			}
			m_mapOfHostToRecoveryProgress[iter->first] = RecStatus;
		}
		
		bool bRecoverySkip = true;
		std::map<std::string, RecoveryFlowStatus>::iterator iterRecStatus;
		for (iterRecStatus = m_mapOfHostToRecoveryProgress.begin(); iterRecStatus != m_mapOfHostToRecoveryProgress.end(); iterRecStatus++)
		{
			if (iterRecStatus->second.recoverystatus != RECOVERY_COMPLETED)
				bRecoverySkip = false;
		}

#ifdef WIN32
		if (!bRecoverySkip)
		{
			if (false == createMapOfVmToSrcAndTgtDiskNmbrs())
			{
				currentTask.TaskErrMsg = "Failed to generate Map of VM to Source and Target Disk Numbers. For extended Error information download EsxUtil.log in vCon Wiz";
				currentTask.TaskFixSteps = "Check the .pinfo and recovery.xml file in proper place, rerun the job if issue persists contact inmage customer support.";
				currentTask.TaskStatus = INM_VCON_TASK_FAILED;
				nextTask.TaskStatus = INM_VCON_TASK_QUEUED;
				m_objLin.UpdateSrcHostStaus(currentTask);

				DebugPrintf(SV_LOG_ERROR, "Failed to generate Map of VM to Source and Target Disk Numbers.\n");
				bReturn = false;
				break;
			}
		}
		if(true == m_bFetchDiskNumMap) // To display only the disk numbers map info..added this for debug
        {
            bReturn = true;
            break;
        }
        DebugPrintf(SV_LOG_INFO,"\n"); // For neat display.
#else
		string strErrorMsg;
		if(false == createMapOfVmToSrcAndTgtDisk(strErrorMsg))
		{
			currentTask.TaskErrMsg = string("Failed to generate Map of VM to Source and Target Disk Numbers. For extended Error information download EsxUtil.log in vCon Wiz");
			currentTask.TaskFixSteps = "Check the .pinfo and recovery.xml file in proper place, rerun the job if issue persists contact inmage customer support.";
			currentTask.TaskStatus = INM_VCON_TASK_FAILED;
			nextTask.TaskStatus = INM_VCON_TASK_QUEUED;
			m_objLin.UpdateSrcHostStaus(currentTask);

			DebugPrintf(SV_LOG_ERROR,"Failed to generate Map of VM to Source and Target Disks.\n");
			bReturn = false;
			break;
		}
#endif

		//Updating Task2 as completed and Task3 as inprogress
		currentTask.TaskStatus = INM_VCON_TASK_COMPLETED;
		nextTask.TaskStatus = INM_VCON_TASK_INPROGRESS;
		m_objLin.ModifyTaskInfoForCxApi(currentTask, nextTask);
		m_objLin.UpdateTaskInfoToCX();

		if (m_bPhySnapShot)
		{
#ifdef WIN32
			currentTask.TaskNum = INM_VCON_TASK_6;
			nextTask.TaskNum = INM_VCON_TASK_7;
#else
			currentTask.TaskNum = INM_VCON_TASK_5;
			nextTask.TaskNum = INM_VCON_TASK_6;
#endif
		}
		else
		{
			currentTask.TaskNum = INM_VCON_TASK_3;
			nextTask.TaskNum = INM_VCON_TASK_4;
		}

		currentTask.TaskErrMsg.clear();
		currentTask.TaskFixSteps.clear();

		list<CdpcliJob> stCdpcliJobs;        
		map<string, pair<string,string> > recoveryPairDetails;
		iter = rollbackDetails.begin();
		bool bWarning = false;
		taskInfo_t taskInfo;

		while(iter != rollbackDetails.end())
		{
            DebugPrintf(SV_LOG_INFO,"----------------------------------------------------------------------------------------------\n");
            DebugPrintf(SV_LOG_INFO,"Virtual Machine Name - %s\n",iter->first.c_str());
		    DebugPrintf(SV_LOG_INFO,"----------------------------------------------------------------------------------------------\n");
			m_VmName    = iter->first;
            m_TagValue  = iter->second.first;
            m_TagType   = iter->second.second;
			StatusInfo statInfo;

			string strCdpCliCmd = "";

			if (m_mapOfHostToRecoveryProgress.find(iter->first) != m_mapOfHostToRecoveryProgress.end())
				RecStatus = m_mapOfHostToRecoveryProgress.find(iter->first)->second;

			if (false == m_bSkipCdpcli && (RecStatus.recoverystatus == TAG_VERIFIED || RecStatus.recoverystatus == RECOVERY_STARTED)) // This check indicates we should not skip cdpcli rollback for this host
			{
				/*********CALL MAIN ROUTINE FROM HERE***********************/
				/*Ignoring return value,so that even if rollback process fails 
				for one VM ,it should continue with others FIX ME..discuss*/
				if (false == RollBackVMs(iter->first, iter->second.first, iter->second.second, strCdpCliCmd, statInfo, RecStatus))
				{
					DebugPrintf(SV_LOG_ERROR,"Failed to perform rollback/snapshot on %s\n", m_VmName.c_str());
					bWarning = true;
					currentTask.TaskErrMsg = currentTask.TaskErrMsg + string("Failed to perform recovery/snapshot for machine : ") + m_VmName + string(". For extended Error information download EsxUtil.log in vCon Wiz\n");
	#ifdef WIN32
					currentTask.TaskFixSteps = currentTask.TaskFixSteps + string("contact inmage customer support for recovery/snapshot of this VM : ") + m_VmName + string("\n"); 
	#else
					currentTask.TaskFixSteps = currentTask.TaskFixSteps + string("Do Recover/Dr-drill manually, detach the disks and power-on, Do n/w changes manually for VM : ") + m_VmName + string("Command for recovery is in the log file\n"); 
	#endif
					taskInfo.TaskNum = nextTask.TaskNum;
					taskInfo.TaskErrMsg = "Failed to perform recovery/snapshot for this machine. Either pairs list not found, retention db might not be exist or common consistency point does not exist.";
					taskInfo.TaskStatus = INM_VCON_TASK_FAILED;
					m_objLin.UpdateSrcHostStaus(taskInfo, m_VmName);

					iter++;
					continue;
				}
			}
			else
			{
				DebugPrintf(SV_LOG_INFO,"Skipping cdpcli rollback/snapshot...\n");
			}

			CdpcliJob cdpCliJob;
			cdpCliJob.uniqueid = m_VmName;
			cdpCliJob.cdpclicmd = strCdpCliCmd;
			cdpCliJob.cmderrfile = m_strDatafolderPath + m_VmName + string("_cdpclierr.txt") ;
			cdpCliJob.cmdoutfile = m_strDatafolderPath + m_VmName + string("_cdpcliout.txt") ;
			cdpCliJob.cmdstatusfile = m_strDatafolderPath + m_VmName + string("_cdpclistatus.txt") ;

			if (strCdpCliCmd.empty() && RecStatus.recoverystatus == TAG_VERIFIED)
			{
				std::ofstream outFileStream;
				outFileStream.open(cdpCliJob.cmdstatusfile.c_str(), std::ios::trunc);
				if (!outFileStream.is_open())
				{
					DebugPrintf(SV_LOG_ERROR, "Failed to open %s for writing\n", cdpCliJob.cmdstatusfile.c_str());
				}
				else
				{
					outFileStream << m_VmName << ":0:0:0";
					outFileStream.close();
				}
			}
			else
				stCdpcliJobs.push_back(cdpCliJob);

			recoveryPairDetails.insert(make_pair(iter->first, iter->second));			
			iter++;
		}

		DebugPrintf(SV_LOG_INFO,"Number of cdpcli's to execute parallely - %d\n", obj.m_nMaxCdpJobs);

		while(!stCdpcliJobs.empty())
		{
			list<CdpcliJob> lstCdpJobs;
			list<CdpcliJob>::iterator iterCdp = stCdpcliJobs.begin();
			if(stCdpcliJobs.size() >= obj.m_nMaxCdpJobs)
			{
				int cdpCount = 0; 
				for(;cdpCount < obj.m_nMaxCdpJobs; cdpCount++)
				{
					if (m_mapOfHostToRecoveryProgress.find(iterCdp->uniqueid) != m_mapOfHostToRecoveryProgress.end())
						RecStatus = m_mapOfHostToRecoveryProgress.find(iterCdp->uniqueid)->second;
					if (RecStatus.recoverystatus == TAG_VERIFIED || RecStatus.recoverystatus == RECOVERY_STARTED) // This check indicates we should not skip cdpcli rollback for this host
						lstCdpJobs.push_back(*iterCdp);
					stCdpcliJobs.pop_front();
					iterCdp = stCdpcliJobs.begin();
				}
			}
			else
			{
				for(;iterCdp != stCdpcliJobs.end(); iterCdp++)
				{
					if (m_mapOfHostToRecoveryProgress.find(iterCdp->uniqueid) != m_mapOfHostToRecoveryProgress.end())
						RecStatus = m_mapOfHostToRecoveryProgress.find(iterCdp->uniqueid)->second;
					if (RecStatus.recoverystatus == TAG_VERIFIED || RecStatus.recoverystatus == RECOVERY_STARTED) // This check indicates we should not skip cdpcli rollback for this host
						lstCdpJobs.push_back(*iterCdp);
				}
				stCdpcliJobs.clear();
			}

			if (!m_bSkipCdpcli && !lstCdpJobs.empty())
			{
				//Preparing the input file for inmexec binary
				string strFileName = m_strDatafolderPath + string("inmexec_input_") + GetCurrentLocalTime() + string(".txt");
				if(false == PrpareInmExecFile(lstCdpJobs, strFileName))
				{
					currentTask.TaskErrMsg = string("Failed to prepare the input file for inmexec, Failed to rollback the VM's. For extended Error information download EsxUtil.log in vCon Wiz");
					currentTask.TaskFixSteps = "";
					currentTask.TaskStatus = INM_VCON_TASK_WARNING;

					DebugPrintf(SV_LOG_ERROR,"Failed to prepare the input file for inmexec, Failed to rollback some of the VM's.\n");
					bWarning = true;
				}

				string strOutputFileName = m_strDatafolderPath + string("inmexec_output_") + GetCurrentLocalTime() + string(".txt");

				//Execute the inmexec binary for cdpcli recovery\snapshot operation
				if (false == RunInmExec(strFileName, strOutputFileName))
				{
					currentTask.TaskErrMsg = string("Failed to run the inmexec, Failed to rollback the VM's. For extended Error information download EsxUtil.log in vCon Wiz");
					currentTask.TaskFixSteps = "";
					currentTask.TaskStatus = INM_VCON_TASK_WARNING;

					DebugPrintf(SV_LOG_ERROR,"Failed to run the inmexec, Failed to rollback some of the VM's.\n");
					bWarning = true;
				}
			}
		}

		//Need to check the status of each cdpcli which are exectued.
		rollbackDetails.clear();
		iter = recoveryPairDetails.begin();
		for(; iter != recoveryPairDetails.end(); iter++)
		{
			int cdpclistatus = 1;
			string cmdstatusfile = m_strDatafolderPath + iter->first + string("_cdpclistatus.txt") ;
			ifstream FIN(cmdstatusfile.c_str(), std::ios::in);
			if(! FIN.is_open())
			{
				DebugPrintf(SV_LOG_ERROR,"Failed to open the file - %s\n",cmdstatusfile.c_str());
				DebugPrintf(SV_LOG_ERROR,"Considering Recovery/Dr-Drill failed for VM : %s\n",iter->first.c_str());
				currentTask.TaskErrMsg = currentTask.TaskErrMsg + string("Considering Recovery/Dr-Drill failed for VM : ") + iter->first;
				currentTask.TaskFixSteps = "";
				currentTask.TaskStatus = INM_VCON_TASK_WARNING;
				
				taskInfo.TaskNum = nextTask.TaskNum;
				taskInfo.TaskErrMsg = "Failed to open the file : " + cmdstatusfile + ", Considering Recovery/Dr-Drill failed.";
				taskInfo.TaskStatus = INM_VCON_TASK_FAILED;
				m_objLin.UpdateSrcHostStaus(taskInfo, iter->first);

				bWarning = true;
				continue;
			}
			string strLineRead;
			while(getline(FIN, strLineRead))
			{
				if(strLineRead.empty())
					continue;
				vector<std::string> v;
				std::string Separator = std::string(":");
				size_t pos = 0;

				size_t found = strLineRead.find(Separator, pos);
				while(found != std::string::npos)
				{
					DebugPrintf(SV_LOG_INFO,"Element - [%s]\n",strLineRead.substr(pos,found-pos).c_str());
					v.push_back(strLineRead.substr(pos,found-pos));
					pos = found + 1; 
					found = strLineRead.find(Separator, pos);            
				}
				DebugPrintf(SV_LOG_INFO,"Element - [%s]\n",strLineRead.substr(pos).c_str());
				v.push_back(strLineRead.substr(pos));

				std::stringstream ss;
				ss<<v[2];
				ss>>cdpclistatus;
			}
			FIN.close();

			if(0 == cdpclistatus)
			{
				DebugPrintf(SV_LOG_INFO,"Successfully performed the cdpcli operation for host : %s\n",iter->first.c_str());
				rollbackDetails.insert(make_pair(iter->first, iter->second));

				if (m_mapOfHostToRecoveryProgress.find(iter->first) != m_mapOfHostToRecoveryProgress.end())
					RecStatus = m_mapOfHostToRecoveryProgress.find(iter->first)->second;
				if (RecStatus.recoverystatus == TAG_VERIFIED || RecStatus.recoverystatus == RECOVERY_STARTED) // This check indicates we should update cdpcli rollback completed for this host
				{
					RecStatus.recoverystatus = CDPCLI_ROLLBACK;
					RecStatus.tagname = "";
					if (!UpdateRecoveryProgress(iter->first, m_strDatafolderPath, RecStatus))
					{
						DebugPrintf(SV_LOG_ERROR, "Failed to update the recovery progress in the local file for host - %s.\n", iter->first.c_str());
						//Currently ignoring the failure case.
					}
				}
			}
			else
			{
				string strCmdOut = m_strDatafolderPath + iter->first + string("_cdpcliout.txt");
				DebugPrintf(SV_LOG_INFO,"Cdpcli operation failed for host : %s with error : %d\n",iter->first.c_str(), cdpclistatus);
				DebugPrintf(SV_LOG_INFO,"For cdpcli output check the log file %s in MT plan name folder\n",strCmdOut.c_str());
				currentTask.TaskErrMsg = currentTask.TaskErrMsg + string("Cdpcli operation failed for host : ") + iter->first;
				currentTask.TaskErrMsg = currentTask.TaskErrMsg + string("For cdpcli output check the log file in MT plan name folder");
				currentTask.TaskFixSteps = "";
				currentTask.TaskStatus = INM_VCON_TASK_WARNING;
				
				taskInfo.TaskNum = nextTask.TaskNum;
				taskInfo.TaskErrMsg = "Cdpcli rollback/snapshot process failed. Check the cdpcli ouput log file : " + strCmdOut;
				taskInfo.TaskStatus = INM_VCON_TASK_FAILED;
				m_objLin.UpdateSrcHostStaus(taskInfo, iter->first);
				bWarning = true;
			}
		}

		DebugPrintf(SV_LOG_INFO,"\n----------------------------------------------------------------------------------------------\n");
		DebugPrintf(SV_LOG_INFO,"***Started Post Rollback Operation***\n");
		DebugPrintf(SV_LOG_INFO,"----------------------------------------------------------------------------------------------\n\n");
		iter = rollbackDetails.begin();
		while(iter != rollbackDetails.end())
		{
			if (m_mapOfHostToRecoveryProgress.find(iter->first) != m_mapOfHostToRecoveryProgress.end())
			{
				RecStatus = m_mapOfHostToRecoveryProgress.find(iter->first)->second;
				if (RecStatus.recoverystatus == POST_ROLLBACK || RecStatus.recoverystatus == DISK_SIGNATURE_UPDATE || RecStatus.recoverystatus == RECOVERY_COMPLETED)
				{
					DebugPrintf(SV_LOG_DEBUG, "Skipping post rollback operation for this host : %s . Post rollback operation already completed for this host.\n", iter->first.c_str());
					vecRecoveredVMs.push_back(iter->first); iter++;
					continue;
				}
			}

			bool bFailed = false;
			m_VmName    = iter->first;
            DebugPrintf(SV_LOG_INFO,"\nVirtual Machine Name - %s\n",iter->first.c_str());
#ifndef WIN32

			runFlushBufAndKpartx(iter->first); //Flush the buffer of device using blockdev creats the partitions using kpartx

			if(false == vgScan())
			{
				DebugPrintf(SV_LOG_ERROR,"[ERROR]: Failed to VG Scan in Master Target\n");
			}

			if(!m_bV2P)
			{
				//Ignoring the return value.
				if(false == editLVMconfFile(iter->first))
				{
					DebugPrintf(SV_LOG_ERROR,"[ERROR]: Failed to Edit the LVM.Conf file\n");
				}
				
				if(false == vgScan())
				{
					DebugPrintf(SV_LOG_ERROR,"[ERROR]: Failed to VG Scan in Master Target\n");
				}
				if(false == activateVg(iter->first))
					DebugPrintf(SV_LOG_ERROR,"[ERROR]: Failed while actiavting the VG in master Target\n");
			}

			obj.m_mapOfVmToSrcAndTgtDisks = m_mapOfVmToSrcAndTgtDisks;
#endif
			string strOsType = "";
			obj.m_mapOfVmNameToId = m_mapOfVmNameToId;
			obj.m_mapOfVmToClusOpn = m_mapOfVmToClusOpn;
			if(false == obj.MakeChangesPostRollback(iter->first, false, strOsType)) // pass false for n/w changes.
            {
			    DebugPrintf(SV_LOG_ERROR,"Failed to make changes required to configure network settings for %s\n", iter->first.c_str());
                DebugPrintf(SV_LOG_ERROR,"****** Please configure network settings manually for this VM.\n\n");
				currentTask.TaskErrMsg = currentTask.TaskErrMsg + string("Failed to make changes required to configure network settings for : ") + iter->first + string(". For extended Error information download EsxUtil.log in vCon Wiz\n");
				currentTask.TaskFixSteps = currentTask.TaskFixSteps + string("configure network settings manually for this VM : ") + iter->first + string("\n");
				bWarning = true;
            }

			m_mapHostToType.insert(make_pair(iter->first, strOsType));	//This one is added to find out the W2K8 machine(Using while updating partition info)

#ifndef WIN32
			
			list<string> lstLsscsiId;

			if(!m_bV2P)
			{
				if(false == deActivateVg(iter->first))
					DebugPrintf(SV_LOG_ERROR,"[ERROR]: Failed while deactiavting the VG in master Target\n");
				
				//Ignoring the return value.
				if(false == repalceOldLVMconfFile(iter->first))
				{
					DebugPrintf(SV_LOG_ERROR,"[ERROR]: Failed to replace the LVM.Conf file\n");
				}
				if(false == vgScan())
				{
					DebugPrintf(SV_LOG_ERROR,"[ERROR]: Failed to VG Scan in Master Target\n");
				}
				
				findLscsiDevice(iter->first, lstLsscsiId);//Ignoring the return value.

				if(false == editLVMconfFile(iter->first))
				{
					DebugPrintf(SV_LOG_ERROR,"[ERROR]: Failed to Edit the LVM.Conf file\n");
				}
				
				if(false == vgScan())
				{
					DebugPrintf(SV_LOG_ERROR,"[ERROR]: Failed to VG Scan in Master Target\n");
				}
				if(false == activateVg(iter->first))
					DebugPrintf(SV_LOG_ERROR,"[ERROR]: Failed while actiavting the VG in master Target\n");
			}
#endif
			
            if(false == obj.MakeChangesPostRollback(iter->first, true, strOsType)) // pass true for P2V changes.
            {
                DebugPrintf(SV_LOG_ERROR,"Failed to make changes required to convert Physical machine to Virtual Machine for - %s\n", iter->first.c_str());
				currentTask.TaskErrMsg = currentTask.TaskErrMsg + string("Failed to make changes required to convert Physical machine to Virtual Machine for : ") + iter->first + string(". For extended Error information download EsxUtil.log in vCon Wiz\n");
				currentTask.TaskFixSteps = currentTask.TaskFixSteps + string("contact inmage customer suppoert to power-on the VM : ") + iter->first + string("\n");

				taskInfo.TaskNum = nextTask.TaskNum;
				taskInfo.TaskErrMsg = "Failed to make changes required to convert Physical machine to Virtual Machine.";
				taskInfo.TaskStatus = INM_VCON_TASK_FAILED;
				m_objLin.UpdateSrcHostStaus(taskInfo, iter->first);

				bWarning = true;
				bFailed = true;
            }		

			if(m_bPhySnapShot)
			{
				if(false == obj.MakeInMageSVCManual(iter->first)) // Conevrts the Services properties from automatic to manual
				{
					DebugPrintf(SV_LOG_ERROR,"Failed to Change the InMage Services From Automatic to Manual for  %s\n", iter->first.c_str());
					DebugPrintf(SV_LOG_ERROR,"****** Please Stop the InMage Services and change the start type to manual for this VM.\n\n");
					currentTask.TaskErrMsg = currentTask.TaskErrMsg + string("Failed to Change the InMage Services From Automatic to Manual for : ") + iter->first + string(". For extended Error information download EsxUtil.log in vCon Wiz\n");
					currentTask.TaskFixSteps = currentTask.TaskFixSteps + string("Stop the InMage Services and change the start type to manual for VM : ") + iter->first + string("\n");
					bWarning = true;
				}
			}

#ifndef WIN32
			
			if(!m_bV2P)
			{
				if(false == deActivateVg(iter->first))
					DebugPrintf(SV_LOG_ERROR,"[ERROR]: Failed while deactiavting the VG in master Target\n");
				
				//Ignoring the return value.
				if(false == repalceOldLVMconfFile(iter->first))
				{
					DebugPrintf(SV_LOG_ERROR,"[ERROR]: Failed to replace the LVM.Conf file\n");
				}
				if(false == vgScan())
				{
					DebugPrintf(SV_LOG_ERROR,"[ERROR]: Failed to VG Scan in Master Target\n");
				}

				//Added flush dev twice otherwise DI issue is coming
				map<string, map<string, string> >::iterator iterH;
				if((iterH = m_mapOfVmToSrcAndTgtDisks.find(iter->first)) != m_mapOfVmToSrcAndTgtDisks.end())
				{
					map<string,string>::iterator mapIterDev = iterH->second.begin();
					for(;mapIterDev != iterH->second.end(); mapIterDev++)
					{
						blockDevFlushBuffer(mapIterDev->second);
						string strStandardDevice;
						getStandardDevice(mapIterDev->second, strStandardDevice);
						if(strStandardDevice.find("/dev") == string::npos)
							strStandardDevice = string("/dev/") + strStandardDevice;
						blockDevFlushBuffer(strStandardDevice);
					}
				}

				//This is to remove multipath and scsi devices and vg if exists
				removeLogicalVolumes(obj.m_lvs);
				obj.m_lvs.clear();
				removeMultipathDevices(iter->first);
				removeLsscsiDevice(lstLsscsiId);
			}
			else
			{
				//Added flush dev twice otherwise DI issue is coming
				map<string, map<string, string> >::iterator iterH;
				if((iterH = m_mapOfVmToSrcAndTgtDisks.find(iter->first)) != m_mapOfVmToSrcAndTgtDisks.end())
				{
					map<string,string>::iterator mapIterDev = iterH->second.begin();
					for(;mapIterDev != iterH->second.end(); mapIterDev++)
					{
						blockDevFlushBuffer(mapIterDev->second);
					}
				}
				collectMessagesLog();
				collectDmsegLog();
			}
#endif			
			if(bFailed)
			{
				iter++;
				continue;
			}

			RecStatus.recoverystatus = POST_ROLLBACK;
			RecStatus.tagname = "";
			if (!UpdateRecoveryProgress(iter->first, m_strDatafolderPath, RecStatus))
			{
				DebugPrintf(SV_LOG_ERROR, "Failed to update the recovery progress in the local file for host - %s.\n", iter->first.c_str());
				//Currently ignoring the failure case.
			}

            //Push all the VMs which are rollbacked successfully.
            //if we are here, it means this VM is rollbacked.
            vecRecoveredVMs.push_back(iter->first);
			iter++;
		}
        DebugPrintf(SV_LOG_INFO,"----------------------------------------------------------------------------------------------\n");

		nextTask.TaskStatus = INM_VCON_TASK_QUEUED;
		currentTask.TaskStatus = INM_VCON_TASK_FAILED;

		if(false == bReturn)
		{
			break;
		}

		vector<std::string>::iterator iterVec;
		//check if atleast one VM is recovered else return false.
        if(vecRecoveredVMs.empty())
        {
			currentTask.TaskErrMsg = string("NONE OF THE VMs ARE RECOVERED SUCCESSFULLY. For extended Error information download EsxUtil.log in vCon Wiz\n");
			currentTask.TaskFixSteps = string("Contact inmage customer support\n");
			m_objLin.UpdateSrcHostStaus(currentTask);
				
            DebugPrintf(SV_LOG_ERROR,"\n\n******  NONE OF THE VMs ARE RECOVERED SUCCESSFULLY ******\n");

            bReturn = false;
			break;
        }
        else if(true == bReturn)
        {
#ifdef WIN32
            //Unmount and delete all mount points for this VM.
            //Even if some mount points are not unmounted/deleted, continuing with remaining VMs.
			DebugPrintf(SV_LOG_INFO,"\nClearing all the mount points for the recovered VMs.\n");
            UnmountAndDeleteMtPoints(vecRecoveredVMs);
            //Restore Disk signatures on all disks of recovered VMs.
			DebugPrintf(SV_LOG_INFO,"\nRestoring the disk signatures of all the recovered VMs.\n");

			for (iterVec = vecRecoveredVMs.begin(); iterVec != vecRecoveredVMs.end(); iterVec++)
			{
				RecoveryFlowStatus RecStatus;
				if (m_mapOfHostToRecoveryProgress.find(*iterVec) != m_mapOfHostToRecoveryProgress.end())
				{
					RecStatus = m_mapOfHostToRecoveryProgress.find(*iterVec)->second;
					if (RecStatus.recoverystatus == DISK_SIGNATURE_UPDATE || RecStatus.recoverystatus == RECOVERY_COMPLETED)
					{
						DebugPrintf(SV_LOG_DEBUG, "Skipping Disk Signature update for this host : %s . This operation is already completed for this host.", iterVec->c_str());
						vecFinalRecoveredVms.push_back(*iterVec);
						continue;
					}
				}

				if (false == RestoreDiskSignature(*iterVec))
				{
					currentTask.TaskErrMsg = "Failed to restore the disk signatures of recovered VMs.Vm bootup may fail. For extended Error information download EsxUtil.log in vCon Wiz";
					currentTask.TaskFixSteps = "Detach the disks from MT and try to power-on recoveerd/snapshot VMs, if power-on fails conatct inmage cutomer support";
					m_objLin.UpdateSrcHostStaus(currentTask, *iterVec);

					DebugPrintf(SV_LOG_ERROR, "Failed to restore the disk signatures for recovered VM %s.\n", iterVec->c_str());
				}
				else
				{
					RecStatus.recoverystatus = DISK_SIGNATURE_UPDATE;
					RecStatus.tagname = "";
					if (!UpdateRecoveryProgress(*iterVec, m_strDatafolderPath, RecStatus))
					{
						DebugPrintf(SV_LOG_ERROR, "Failed to update the recovery progress in the local file for host - %s.\n", iterVec->c_str());
						//Currently ignoring the failure case.
					}
					vecFinalRecoveredVms.push_back(*iterVec);
				}
			}
#else
			vecFinalRecoveredVms = vecRecoveredVMs;
#endif
			if (m_bPhySnapShot)
				DebugPrintf(SV_LOG_INFO, "\n\n******  SUCCESSFULLY COMPLETED THE SNAPSHOT OPERATION FOR THE FOLLOWING VMs - \n");
			else
				DebugPrintf(SV_LOG_INFO, "\n\n******  THE FOLLOWING VMs ARE RECOVERED - \n");

			iterVec = vecFinalRecoveredVms.begin();
			for (int i = 1; iterVec != vecFinalRecoveredVms.end(); iterVec++, i++)
			{
				map<string, string>::iterator iterMap = m_mapOfVmNameToId.find(*iterVec);
				if (iterMap != m_mapOfVmNameToId.end())
				{
					if (iterVec->compare(iterMap->second) != 0)
						DebugPrintf(SV_LOG_INFO, "%d -> InMageHostId = %s\tHostName = %s\n", i, iterVec->c_str(), iterMap->second.c_str());
					else
						DebugPrintf(SV_LOG_INFO, "%d -> InMageHostId = %s\tHostName = %s\n", i, iterVec->c_str(), iterVec->c_str());
				}
				else
					DebugPrintf(SV_LOG_INFO, "%d. %s\n", i, iterVec->c_str());
			}

			if (false == writeRecoveredVmsName(vecFinalRecoveredVms))
			{
				currentTask.TaskErrMsg = currentTask.TaskErrMsg + string("Failed to write VM name in InMage_Recovered_Vms.rollback file : ") + iter->first + string(". For extended Error information download EsxUtil.log in vCon Wiz\n");
				currentTask.TaskFixSteps = currentTask.TaskFixSteps + string("Check file permission for the file, rerun the recovery/snapshot process for other non-recovered/snapshot VMs, contact inmage customer support\n");

				m_objLin.UpdateSrcHostStaus(currentTask);

				DebugPrintf(SV_LOG_ERROR, "Failed to write VM name in InMage_Recovered_Vms.rollback/InMage_Recovered_Vms.snapshot file.\n");
				bReturn = false;
				break;
			}

			for (iterVec = vecFinalRecoveredVms.begin(); iterVec != vecFinalRecoveredVms.end(); iterVec++)
			{
				RecoveryFlowStatus RecStatus;
				RecStatus.recoverystatus = RECOVERY_COMPLETED;
				RecStatus.tagname = "";
				if (!UpdateRecoveryProgress(*iterVec, m_strDatafolderPath, RecStatus))
				{
					DebugPrintf(SV_LOG_ERROR, "Failed to update the recovery progress in the local file for host - %s.\n", iterVec->c_str());
					//Currently ignoring the failure case.
				}
			}

			if (!m_bPhySnapShot)
			{
				string strRecoveredVmFileName = m_strDatafolderPath + string(ROLLBACK_FAILED_VM_FILE);

				DebugPrintf(SV_LOG_INFO, "Opening the file to write: %s\n", strRecoveredVmFileName.c_str());
				ofstream outFile(strRecoveredVmFileName.c_str(), ios::out);
				if (!outFile.is_open())
				{
					DebugPrintf(SV_LOG_ERROR, "Failed to open file %s for writing\n", strRecoveredVmFileName.c_str());
				}
				else
				{
					map<string, statusInfo_t>::iterator iterStatus = m_objLin.m_mapHostIdToStatus.begin();
					for (; iterStatus != m_objLin.m_mapHostIdToStatus.end(); iterStatus++)
					{
						if (iterStatus->second.Status == "Failed")
						{
							outFile << iterStatus->second.HostId;
							outFile << "!@!@!";
							outFile << iterStatus->second.ErrorMsg << endl;
						}
					}
				}
				outFile.close();
				DebugPrintf(SV_LOG_INFO, "Successfully written the VM entries in file: %s\n", strRecoveredVmFileName.c_str());

				m_objLin.AssignSecureFilePermission(strRecoveredVmFileName);
			}
        }

		if(!m_strCxPath.empty())
		{
			//uploads InMage_Recovered_Vms.rollback/InMage_Recovered_Vms.snapshot the file and input.txt file to CX
			string strLocalFile;
			if(m_bPhySnapShot)
				strLocalFile = m_strDatafolderPath + SNAPSHOT_VM_FILE_NAME;
			else
				strLocalFile = m_strDatafolderPath + ROLLBACK_VM_FILE_NAME;

			string strInputTextFile = m_strDatafolderPath + "input.txt";
			ofstream outfile(strInputTextFile.c_str(), ios::out | ios::trunc);
			if(!outfile.is_open())
			{
				currentTask.TaskErrMsg = "Failed to open the file : " + strInputTextFile;
				currentTask.TaskFixSteps = "check the file is created properly or not. Contact inmage customer support";
				m_objLin.UpdateSrcHostStaus(currentTask);

				DebugPrintf(SV_LOG_ERROR, "Failed to open file : %s\n", strInputTextFile.c_str());

				bReturn = false;
				break;
			}

			if(m_bPhySnapShot)
				outfile << string(SNAPSHOT_VM_FILE_NAME) << endl;
			else
			{
				outfile << string(ROLLBACK_VM_FILE_NAME) << endl;
				outfile << string(ROLLBACK_FAILED_VM_FILE) << endl;
			}
			outfile.flush();
			outfile.close();

			if(false == m_objLin.UploadFileToCx(strLocalFile, m_strCxPath))
			{
				currentTask.TaskErrMsg = "Failed to upload the " + strLocalFile + " file to CX. For extended Error information download EsxUtil.log in vCon Wiz";
				currentTask.TaskFixSteps = "check the file is created properly or not. Contact inmage customer support";
				m_objLin.UpdateSrcHostStaus(currentTask);

				DebugPrintf(SV_LOG_ERROR, "Failed to upload the %s file to CX", strLocalFile.c_str());

				bReturn = false;
				break;
			}

			//Currently not marking it as failure.
			strLocalFile = m_strDatafolderPath + string(ROLLBACK_FAILED_VM_FILE);
			if (false == m_objLin.UploadFileToCx(strLocalFile, m_strCxPath))
			{
				DebugPrintf(SV_LOG_ERROR, "Failed to upload the %s file to CX", strLocalFile.c_str());
			}

			if(false == m_objLin.UploadFileToCx(strInputTextFile, m_strCxPath))
			{
				currentTask.TaskErrMsg = "Failed to upload the " + strInputTextFile + " file to CX. For extended Error information download EsxUtil.log in vCon Wiz";
				currentTask.TaskFixSteps = "check the file is created properly or not. Contact inmage customer support";
				m_objLin.UpdateSrcHostStaus(currentTask);

				DebugPrintf(SV_LOG_ERROR, "Failed to upload the %s file to CX", strInputTextFile.c_str());

				bReturn = false;
				break;
			}
		}
		
		if(!bWarning)
			currentTask.TaskStatus = INM_VCON_TASK_COMPLETED;
		else
			currentTask.TaskStatus = INM_VCON_TASK_WARNING;
		nextTask.TaskStatus = INM_VCON_TASK_INPROGRESS;

	}while(0);
	
	m_objLin.ModifyTaskInfoForCxApi(currentTask, nextTask);
	m_objLin.UpdateTaskInfoToCX();

	if (!bReturn)
	{
		map<string, SendAlerts> sendAlertsInfo;
		if (bUpdateToCXbyMTid)
			m_objLin.UpdateSendAlerts(currentTask, m_strCxPath, "recovery", sendAlertsInfo);
		else
		{
			m_objLin.UpdateSendAlerts(sendAlertsInfo, "recovery");
		}
		if (!m_objLin.SendAlertsToCX(sendAlertsInfo))
		{
			DebugPrintf(SV_LOG_ERROR, "Failed to update the alerts to CX!!!\n");
		}
	}

	if(!m_strCxPath.empty())
	{
		//uploads the Log file to CX
		if(false == m_objLin.UploadFileToCx(m_strLogFile, m_strCxPath))
			DebugPrintf(SV_LOG_ERROR, "Failed to upload the %s file to CX", m_strLogFile.c_str());
		else
			DebugPrintf(SV_LOG_INFO, "Successfully upload file %s file to CX", m_strLogFile.c_str());
	}

    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bReturn;
}


bool RollBackVM::checkIfFileExists(string &strFileName)
{
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    bool blnReturn = true;

#ifdef WIN32
    WIN32_FIND_DATA FindFileData;
    HANDLE hFind;
    hFind = FindFirstFile(strFileName.c_str(), &FindFileData);
    if (hFind == INVALID_HANDLE_VALUE) 
    {
        DebugPrintf(SV_LOG_DEBUG,"Failed to find the file : %s\n",strFileName.c_str());
        DebugPrintf(SV_LOG_DEBUG,"Invalid File Handle. GetLastError reports [Error %lu]\n",GetLastError());
        blnReturn = false;
    }     
#else
    struct stat stFileInfo;
    int intStat;

    // Attempt to get the file attributes
    intStat = stat(strFileName.c_str(),&stFileInfo);
    if(intStat == 0) {
        // We were able to get the file attributes
        // so the file obviously exists.
        blnReturn = true;
    } else {
        // We were not able to get the file attributes.
        // This may mean that we don't have permission to
        // access the folder which contains this file. If you
        // need to do that level of checking, lookup the
        // return values of stat which will give you
        // more details on why stat failed.
        blnReturn = false;
    }
#endif

    if(true == blnReturn)
        DebugPrintf(SV_LOG_DEBUG,"Found the file : %s\n",strFileName.c_str());

    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return blnReturn;
}

std::string RollBackVM::getVxAgentInstallPath()
{
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return m_obj.getInstallPath();
}

bool RollBackVM::getRetentionSettings(const std::string& volName, std::string& retLogLocation)
{
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    using namespace std;
    try
    {
		Configurator* TheConfigurator = NULL;
        if(!GetConfigurator(&TheConfigurator))
        {
            DebugPrintf(SV_LOG_ERROR,"GetConfigurator failed. Failing the Job...\n");
			DebugPrintf(SV_LOG_DEBUG, "Exiting %s\n", __FUNCTION__);
			return false;
        }
        string cxFomattedVolumeName = volName;
#ifdef SV_WINDOWS
        FormatVolumeNameForCxReporting(cxFomattedVolumeName);
        cxFomattedVolumeName[0]=toupper(cxFomattedVolumeName[0]);
#endif
        retLogLocation = TheConfigurator->getCdpDbName(cxFomattedVolumeName);         
    }
	catch(...)
	{
        DebugPrintf(SV_LOG_ERROR,"Unknown Exception occured.\n");
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;
	}
    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return true;
}


bool RollBackVM::GetMapOfSrcDiskToTgtDiskNumFromDinfo(const string strVmName, string strDInfofile, map<string, dInfo_t>& mapTgtDiskSigToSrcDiskInfo)
{
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	std::string strLineRead;
	std::string token("!@!@!");
	size_t index=0;
	bool bResult = true;
		
	std::ifstream inFile(strDInfofile.c_str());
	if(!inFile.is_open())
	{
		DebugPrintf(SV_LOG_ERROR,"Failed to read the file - %s\n",strDInfofile.c_str());
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;
	}
	while(getline(inFile,strLineRead))
	{
		if(strLineRead.empty())
		{
			DebugPrintf(SV_LOG_INFO, "Reached end of the file %s", strDInfofile.c_str());
			break;
		}
		
		string strTargetDiskSig;
		dInfo_t dinfo;
		vector<string> v;
        DebugPrintf(SV_LOG_INFO,"strLineRead =  %s\n",strLineRead.c_str());
		try
		{
			index = 0;
			size_t found = strLineRead.find(token, index);
			while(found != std::string::npos)
			{
				DebugPrintf(SV_LOG_INFO,"Element - [%s]\n",strLineRead.substr(index,found-index).c_str());
				v.push_back(strLineRead.substr(index,found-index));
				index = found + 5; 
				found = strLineRead.find(token, index);            
			}
			DebugPrintf(SV_LOG_INFO,"Element - [%s]\n",strLineRead.substr(index).c_str());
			v.push_back(strLineRead.substr(index));
		}
		catch(...)
		{
			DebugPrintf(SV_LOG_INFO,"failed while reading the file, try to read in old format");
			bResult = false;
            break;
		}

        if(v.size() < 4)
        {
            DebugPrintf(SV_LOG_INFO,"Number of elements in this line - %u\n",v.size());
			bResult = false;
            break;
        }
		dinfo.DiskNum = v[0];
		dinfo.DiskSignature = v[1];
		dinfo.DiskDeviceId = v[2];
		strTargetDiskSig = v[3];

		mapTgtDiskSigToSrcDiskInfo.insert(make_pair(strTargetDiskSig, dinfo));
	}
	inFile.close();

	if(!bResult)
	{
		bResult = true;
		ifstream readFile(strDInfofile.c_str());
		if (!readFile.is_open())
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to read the File : %s \n",strDInfofile.c_str());
			DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);        
			return false;
		}
		while(getline(readFile,strLineRead))
		{
			if(strLineRead.empty())
			{
				DebugPrintf(SV_LOG_INFO, "Reached end of the file %s", strDInfofile.c_str());
				break;
			}
			string strSecondValue;
			dInfo_t dinfo;
			DebugPrintf(SV_LOG_INFO,"strLineRead =  %s\n",strLineRead.c_str());
			index = strLineRead.find_first_of(token);
			if(index != std::string::npos)
			{
				dinfo.DiskNum = strLineRead.substr(0,index);
				strSecondValue = strLineRead.substr(index+(token.length()),strLineRead.length());
				DebugPrintf(SV_LOG_INFO,"strFirstValue = %s  <==>  ", dinfo.DiskNum.c_str());
				DebugPrintf(SV_LOG_INFO,"strSecondValue = %s\n", strSecondValue.c_str());
				if ((!dinfo.DiskNum.empty()) && (!strSecondValue.empty()))
					mapTgtDiskSigToSrcDiskInfo.insert(make_pair(strSecondValue,dinfo));
			}
			else
			{
				DebugPrintf(SV_LOG_ERROR,"Failed to find token in lineread in the file : %s\n",strDInfofile.c_str());
				bResult = false;
				mapTgtDiskSigToSrcDiskInfo.clear();
			}
		}
		readFile.close();
	}

    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);	
	return bResult;
}

bool RollBackVM::CreateSnapshotPair(std::map<std::string,std::string> mapProtectedPairs,std::map<std::string,std::string> mapSnapshotPairs,std::map<std::string,std::string>& mapProtectedToSnapshotVol)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	
	std::map<std::string,std::string>::iterator snapshotIter;
	std::map<std::string,std::string>::iterator protectedIter = mapProtectedPairs.begin();
	for(;protectedIter != mapProtectedPairs.end(); protectedIter++)
	{
		snapshotIter = mapSnapshotPairs.find(protectedIter->first);
		if(snapshotIter != mapSnapshotPairs.end())
			mapProtectedToSnapshotVol.insert(make_pair(protectedIter->second, snapshotIter->second));
		else
		{
			DebugPrintf(SV_LOG_DEBUG,"Protected Volume %s --> correcponding snapshot Volume not found\n", protectedIter->second.c_str());
			return false;
		}
	}
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return true;
}

bool RollBackVM::CdpcliOperations(const string& cdpcli_cmd)
{
     DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	 bool result = true;
	 string installLocation (getVxAgentInstallPath());
	
     if (installLocation.empty()) 
	 {
	    DebugPrintf(SV_LOG_ERROR,"Could not determine the install location for the VX agent\n");
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;
     }
		
	 ACE_Process_Options options; 
     options.handle_inheritance(false);

#ifdef WIN32
     string fileName = installLocation + "\\" + cdpcli_cmd ;
	 options.command_line("%s %s",fileName.c_str(), "");
#else
	 string fileName = installLocation + string("/bin/") + cdpcli_cmd ;//Change for Linux VM	
	 options.command_line(fileName.c_str());
#endif
     DebugPrintf(SV_LOG_INFO,"Command to execute =  %s\n",fileName.c_str());
	 
	 ACE_Process_Manager* pm = ACE_Process_Manager::instance();
	 if (pm == NULL) {
			// need to continue with other volumes even if there was a problem generating an ACE process
	        DebugPrintf(SV_LOG_ERROR,"Failed to Get ACE Process Manager instance\n");
			DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	   	    return false;
	 }
	 pid_t pid = pm->spawn (options);
     if ( pid == ACE_INVALID_PID) {
			// need to continue with other volumes even if there was a problem generating an ACE process
            DebugPrintf(SV_LOG_ERROR,"Failed to create ACE Process for executing command :  %s \n",fileName.c_str());
			DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
			return false;
		}
	 // wait for the process to finish
	 ACE_exitcode status = 0;
	 pid_t rc = pm->wait(pid, &status);
	 if (status == 0) {
	 DebugPrintf(SV_LOG_INFO,"Successfully ran the command\n");
	 result = true;
	 }
	 else {
         DebugPrintf(SV_LOG_ERROR,"[Warning] : Encountered an error while executing command.\n");
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	 return false;
	 }
  DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
  return result;
}


bool RollBackVM::PrpareInmExecFile(list<CdpcliJob>& stCdpCliJobs, const string& FileName)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool result = true;

	DebugPrintf(SV_LOG_DEBUG,"InmExec Input file : %s\n",FileName.c_str());

	ofstream FOUT(FileName.c_str(), std::ios::out);
    if(! FOUT.is_open())
    {
        DebugPrintf(SV_LOG_ERROR,"Failed to open the file - %s\n",FileName.c_str());
        result = false;
    }	
	else
	{
		FOUT << "[version]" << endl << "version=1.0" << endl ;
		FOUT << "[global]" << endl;
		FOUT << "prescript=" << endl;
		FOUT << "postcript=" << endl;
		FOUT.flush();

		list<CdpcliJob>::iterator iter = stCdpCliJobs.begin();
		for(;iter != stCdpCliJobs.end(); iter++)
		{
			FOUT << "[job]" << endl;
			FOUT << "#inputs for host : " << iter->uniqueid << endl;
			FOUT << "prescript=" << endl;
			FOUT << "postcript=" << endl;
			FOUT << "#Cdpcli Command : " << endl;
			FOUT << "command=" << iter->cdpclicmd << endl;
			FOUT << "stdin=" << iter->cmdinfile << endl;
			FOUT << "stdout=" << iter->cmdoutfile << endl;
			FOUT << "stderr=" << iter->cmderrfile << endl;
			FOUT << "exitstatusfile=" << iter->cmdstatusfile << endl;
			FOUT << "uniqueid=" << iter->uniqueid << endl;
			FOUT.flush();
		}

		FOUT.close();
	}

	m_objLin.AssignSecureFilePermission(FileName);

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return result;
}


bool RollBackVM::RunInmExec(const string& strFileName, const string& strOutputFileName)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool result = true;

	std::string InmExecCmd;
#ifdef WIN32
	InmExecCmd = string("inmexec.exe --spec=\"") + strFileName + string("\" --output=\"") + strOutputFileName + string("\"");
#else
	InmExecCmd = string("inmexec --spec=") + strFileName + string(" --output=") + strOutputFileName;
#endif

	if(false == CdpcliOperations(InmExecCmd))
    {
		DebugPrintf(SV_LOG_ERROR,"Failed to run the InmExec Command : %s\n", InmExecCmd.c_str());
        result = false;
    }
	else
	{
		DebugPrintf(SV_LOG_INFO,"Successfully ran the InmExec Command : %s\n", InmExecCmd.c_str());
	}

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return result;
}


bool RollBackVM::UpdateRecoveryProgress(const std::string& strHost, const std::string& strDirPath, RecoveryFlowStatus& RecStatus)
{
	DebugPrintf(SV_LOG_DEBUG, "Entering %s\n", __FUNCTION__);
	bool bResult = true;

	string strFileName = strDirPath + DIRECTORY_SEPARATOR + strHost + RECOVERY_PROGRESS;

	FILE * pOutFile;
	pOutFile = fopen(strFileName.c_str(), "w");
	if (pOutFile == NULL)
	{
		DebugPrintf(SV_LOG_ERROR, "Failed to open %s for writing\n", strFileName.c_str());
		bResult = false;
	}
	else
	{
		fprintf(pOutFile, "%s%s%d\n", RECOVERY_PROGRESS_STATUS, "!@!@!", RecStatus.recoverystatus);
		fprintf(pOutFile, "%s%s%s\n", TAG_NAME, "!@!@!", RecStatus.tagname.c_str());
		fprintf(pOutFile, "%s%s%s", TAG_TYPE, "!@!@!", RecStatus.tagtype.c_str());
		fclose(pOutFile);
	}

	/*std::ofstream outFileStream;
	outFileStream.open(strFileName.c_str(), std::ios::trunc);
	if (!outFileStream.is_open())
	{
		DebugPrintf(SV_LOG_ERROR, "Failed to open %s for writing\n", strFileName.c_str());
		bResult = false;
	}
	else
	{
		outFileStream << RECOVERY_PROGRESS_STATUS << "!@!@!" << RecStatus.recoverystatus << endl;
		outFileStream << TAG_NAME << "!@!@!" << RecStatus.tagname << endl;
		outFileStream << TAG_TYPE << "!@!@!" << RecStatus.tagtype;
		outFileStream.close();
	}*/

	DebugPrintf(SV_LOG_DEBUG, "Exiting %s\n", __FUNCTION__);
	return bResult;
}


//*******************************************************************
//
//ReadRecoveryProgress	: Read Recovery progress of last instance of 
//							execution from a file.
//Sample Input File		:
//
//Input					: HostID, /failover/Data/ folder path, _IN_ RecStatus
//Output				: _IN_OUT_ RecStatus of type RecoveryFlowStatus
//
//*******************************************************************
bool RollBackVM::ReadRecoveryProgress(const std::string& strHost, const std::string& strDirPath, RecoveryFlowStatus& RecStatus)
{
	DebugPrintf(SV_LOG_DEBUG, "Entering %s\n", __FUNCTION__);
	bool bResult = true;

	string strFileName = strDirPath + DIRECTORY_SEPARATOR + strHost + RECOVERY_PROGRESS;

	if (!checkIfFileExists(strFileName))
	{
		DebugPrintf(SV_LOG_DEBUG, "Exiting %s\n", __FUNCTION__);
		return false;
	}

	std::ifstream inFile(strFileName.c_str());
	if (!inFile.is_open())
	{
		DebugPrintf(SV_LOG_ERROR, "Failed to read the file - %s\n", strFileName.c_str());
		bResult = false;
	}
	else
	{
		string token = "!@!@!";
		string strLineRead;
		while (getline(inFile, strLineRead))
		{
			std::size_t index;
			DebugPrintf(SV_LOG_DEBUG, "Line read =  %s \n", strLineRead.c_str());
			index = strLineRead.find_first_of(token);
			if (index != std::string::npos)
			{
				if (strLineRead.substr(0, index).compare(RECOVERY_PROGRESS_STATUS) == 0)
				{
					int nRecStatus;
					stringstream ss;
					ss << strLineRead.substr(index + (token.length()), strLineRead.length());
					ss >> nRecStatus;
					RecStatus.recoverystatus = RecoveryStatus(nRecStatus);       //RecoveryProgressStatus
				}
				else if (strLineRead.substr(0, index).compare(TAG_NAME) == 0)
					RecStatus.tagname = strLineRead.substr(index + (token.length()), strLineRead.length());   //TagName Coulde be tag or time
				else
					RecStatus.tagtype = strLineRead.substr(index + (token.length()), strLineRead.length());   //Tagtype FS (or) TIME
				
			}
			else
			{
				bResult = false;
			}
		}
		inFile.close();
	}

	DebugPrintf(SV_LOG_DEBUG, "Exiting %s\n", __FUNCTION__);
	return bResult;
}

bool RollBackVM::DeleteRecoveryProgressFiles(const std::string& strHost, const std::string& strDirPath)
{
	DebugPrintf(SV_LOG_DEBUG, "Entering %s\n", __FUNCTION__);
	bool bResult = true;

	RecoveryFlowStatus RecStatus;
	string strFileName = strDirPath + DIRECTORY_SEPARATOR + strHost + RECOVERY_PROGRESS;

	if (!ReadRecoveryProgress(strHost, strDirPath, RecStatus))
	{
		DebugPrintf(SV_LOG_DEBUG, "Skipping deletion of file [%s]. Either file does not exist or issue in reading the file\n", strFileName.c_str());
	}
	else
	{
		if (RecStatus.recoverystatus == RECOVERY_COMPLETED)
		{
			if (-1 == remove(strFileName.c_str()))
			{
				DebugPrintf(SV_LOG_WARNING, "Failed to delete the file - %s\n", strFileName.c_str());
				DebugPrintf(SV_LOG_WARNING, "Delete the file manually and rerun the job.\n");
				bResult = false;
			}
			else
			{
				DebugPrintf(SV_LOG_DEBUG, "SuccessFully deleted file - %s\n", strFileName.c_str());
			}
		}
	}

	DebugPrintf(SV_LOG_DEBUG, "Exiting %s\n", __FUNCTION__);
	return bResult;
}


//HostName -- Hostname of source server
bool RollBackVM::GenerateProtectionPairList(const std::string& strHostId, std::map<std::string, std::string>& ProtectedPairs, const std::string& strValue, const std::string& strType)
{
	DebugPrintf(SV_LOG_DEBUG, "Entering %s\n", __FUNCTION__);
	bool bResult = true;

	string strHostName = strHostId;
	map<string, string>::iterator iter = m_mapOfVmNameToId.find(strHostId);
	if (iter != m_mapOfVmNameToId.end())
		strHostName = iter->second;

	std::map<std::string, std::string> mapProtectedPairsFromCX, mapNewPairs;
	std::map<std::string, std::string>::iterator iterCXPair, iterLocalPair;
	//Getting the Vx pair details from CX
	DebugPrintf(SV_LOG_DEBUG, "Getting replication pairs details from CX for host : [%s].\n", strHostName.c_str());
	if (false == getVxPairs(strHostName, mapProtectedPairsFromCX))
	{
		DebugPrintf(SV_LOG_ERROR, "Failed to fetch the replication pairs.\n");
		//Currently ignoring and proceeding with localy availble information
	}

	for (iterCXPair = mapProtectedPairsFromCX.begin(); iterCXPair != mapProtectedPairsFromCX.end(); iterCXPair++)
	{
		DebugPrintf(SV_LOG_DEBUG, "Replication pair From CX : [%s] <==> [%s].\n", iterCXPair->first.c_str(), iterCXPair->second.c_str());
		iterLocalPair = ProtectedPairs.find(iterCXPair->first);
		if (iterLocalPair == ProtectedPairs.end())
			ProtectedPairs[iterCXPair->first] = iterCXPair->second;
	}

    //Check local list of volume pairs are in locked state or not
	for (iterLocalPair = ProtectedPairs.begin(); iterLocalPair != ProtectedPairs.end(); iterLocalPair++)
	{
		DebugPrintf(SV_LOG_DEBUG, "Lock checking for pair : [%s].\n", iterLocalPair->second.c_str());
		bool replicationIntact = false;

		if (usingDiskBasedProtection())
		{
			replicationIntact = true;
		}
		else {

#ifdef WIN32
			//Linux case the verification might not be accurate for full disk protection. So following different approach.		
			if (IsVolumeLocked(iterLocalPair->second.c_str()))
			{
				DebugPrintf(SV_LOG_DEBUG, "Pair is locked %s.\n", iterLocalPair->second.c_str());
				replicationIntact = true;
			}
			else
				DebugPrintf(SV_LOG_DEBUG, "Pair is Un-locked %s.\n", iterLocalPair->second.c_str());
#endif
		}

		if (replicationIntact && IsRetentionDBPathExist(iterLocalPair->second, strValue, strType))
		{
			DebugPrintf(SV_LOG_DEBUG, "Retention DB path exists. Considering pair as intact %s.\n", iterLocalPair->second.c_str());
			mapNewPairs[iterLocalPair->first] = iterLocalPair->second;
		}

	}

	ProtectedPairs.clear();
	ProtectedPairs = mapNewPairs;

	DebugPrintf(SV_LOG_DEBUG, "Exiting %s\n", __FUNCTION__);
	return bResult;
}


bool RollBackVM::BookMarkTheTag(const std::string& strTagName, const std::string& strTgtVol, const std::string& strTagType)
{
	DebugPrintf(SV_LOG_DEBUG, "Entering %s\n", __FUNCTION__);
	bool bResult = true;

	string CdpcliCmd = CDPCLI_BINARY + string(" --lockbookmark --vol=") + strTgtVol;

	if (strTagType.compare(TIME_TAGTYPE) == 0)
		CdpcliCmd = CdpcliCmd + " --time=\"" + strTagName + "\"";
	else
		CdpcliCmd = CdpcliCmd + " --event=\"" + strTagName + "\" --app=" + strTagType;

	CdpcliCmd = CdpcliCmd + string(" --loglevel=") + boost::lexical_cast<std::string>(SV_LOG_ALWAYS) ;

	string installLocation(getVxAgentInstallPath());

	if (installLocation.empty())
	{
		DebugPrintf(SV_LOG_ERROR, "Could not determine the install location for the VX agent\n");
		DebugPrintf(SV_LOG_DEBUG, "Exiting %s\n", __FUNCTION__);
		return false;
	}

#ifdef WIN32
	CdpcliCmd = installLocation + "\\" + CdpcliCmd;
#else
	CdpcliCmd = installLocation + "/bin/" + CdpcliCmd;
#endif

	std::string str_outputFile = m_strDatafolderPath + DIRECTORY_SEPARATOR + m_VmName + "_BookMark_" + GetCurrentLocalTime() + ".log";

	if (false == m_objLin.ExecuteCmdWithOutputToFileWithPermModes(CdpcliCmd, str_outputFile, O_APPEND | O_RDWR | O_CREAT))
	{
		DebugPrintf(SV_LOG_ERROR, "Failed to run the cdpcli command : [%s]\n", CdpcliCmd.c_str());
		bResult = false;
	}
	else
	{
		DebugPrintf(SV_LOG_INFO, "Successfully ran the cdplcli Command : [%s]\n", CdpcliCmd.c_str());
	}

	DebugPrintf(SV_LOG_DEBUG, "Exiting %s\n", __FUNCTION__);
	return bResult;
}

//Lists the availble tags for a particular volume in current situation
bool RollBackVM::CollectCdpCliListEventLogs(const std::string& strTgtVol)
{
	DebugPrintf(SV_LOG_DEBUG, "Entering %s\n", __FUNCTION__);
	bool bResult = true;

	std::string CdpcliCmd = CDPCLI_BINARY + string(" --listevents --vol=") + strTgtVol;
	

	string installLocation(getVxAgentInstallPath());

	if (installLocation.empty())
	{
		DebugPrintf(SV_LOG_ERROR, "Could not determine the install location for the VX agent\n");
		DebugPrintf(SV_LOG_DEBUG, "Exiting %s\n", __FUNCTION__);
		return false;
	}

#ifdef WIN32
	CdpcliCmd = installLocation + DIRECTORY_SEPARATOR + CdpcliCmd;
#else
	CdpcliCmd = installLocation + DIRECTORY_SEPARATOR + string( "bin/" ) + CdpcliCmd;
#endif

	CdpcliCmd = CdpcliCmd + string(" --loglevel=") + boost::lexical_cast<std::string>(SV_LOG_ALWAYS);

	std::string str_outputFile = m_strDatafolderPath + DIRECTORY_SEPARATOR + "ListEventLog_" + GetCurrentLocalTime() + ".log";

	if (false == m_objLin.ExecuteCmdWithOutputToFileWithPermModes(CdpcliCmd, str_outputFile, O_RDWR | O_CREAT))
	{
		DebugPrintf(SV_LOG_WARNING, "Failed to run the cdpcli command : [%s]\n", CdpcliCmd.c_str());
		bResult = false;
	}
	else
	{
		DebugPrintf(SV_LOG_DEBUG, "Successfully ran the cdplcli Command : [%s]\n", CdpcliCmd.c_str());

		//IF we don't want to print this log then we need to put a bool condition as parameter for this API
		DebugPrintf(SV_LOG_INFO, "\n********** List Events for Volume  : [%s] **********\n\n", strTgtVol.c_str());
		ReadLogFileAndPrint(str_outputFile);
		DebugPrintf(SV_LOG_INFO, "********** END **********\n", strTgtVol.c_str());
	}
	
	DebugPrintf(SV_LOG_DEBUG, "Exiting %s\n", __FUNCTION__);
	return bResult;
}


//Fetch Vx Pairs from this target to given 
bool RollBackVM::getVxPairs(const std::string& srcHostName, std::map<std::string, std::string> & mapVxPairs)
{
	DebugPrintf(SV_LOG_DEBUG, "Entering %s\n", __FUNCTION__);
	bool rv = true;

	do
	{
		try
		{
			Configurator* TheConfigurator = NULL;
			if (!GetConfigurator(&TheConfigurator))
			{
				DebugPrintf(SV_LOG_ERROR, "Failed to get configurator instance. Failing the Job...\n");
				rv = false;
				break;
			}
			//Fetch the map of volume pairs set to this machine from source
			mapVxPairs = TheConfigurator->getReplicationPairInfo(srcHostName);
		}
		catch (...)
		{
			DebugPrintf(SV_LOG_ERROR, "Failed to get volume pairs set to this machine from source %s.\n", srcHostName.c_str());
			rv = false;
			break;
		}
	} while (0);

	DebugPrintf(SV_LOG_DEBUG, "Exiting %s\n", __FUNCTION__);
	return rv;
}

bool RollBackVM::ReadLogFileAndPrint(const string& strFileName)
{
	DebugPrintf(SV_LOG_DEBUG, "Entering %s\n", __FUNCTION__);
	bool rv = true;

	string strLine;
	ifstream INFILE(strFileName.c_str());
	if (INFILE.is_open())
	{
		while (getline(INFILE, strLine))
			DebugPrintf(SV_LOG_INFO, "%s\n", strLine.c_str());
		INFILE.close();
	}
	else
	{
		DebugPrintf(SV_LOG_WARNING, "Unable to open the file : [%s]\n", strFileName.c_str());
		rv = false;
	}

	DebugPrintf(SV_LOG_DEBUG, "Exiting %s\n", __FUNCTION__);
	return rv;
}

#ifdef WIN32
bool RollBackVM::startVxServices(const string& serviceName)
{
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	SC_HANDLE schService;        

    SERVICE_STATUS_PROCESS  serviceStatusProcess;
    DWORD actualSize = 0;
	ULONG dwRet = 0;
	LPVOID lpMsgBuf;

	SC_HANDLE schSCManager = ::OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if (NULL == schSCManager) {
		//cout << "Failed to Get handle to Windows Services Manager in OpenSCMManager\n";
        DebugPrintf(SV_LOG_ERROR,"Failed to Get handle to Windows Services Manager in OpenSCMManager\n");
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
        return false;
    }

	// start requested service
	// PR#10815: Long Path support
	schService = SVOpenService(schSCManager, serviceName.c_str(), SERVICE_ALL_ACCESS); 
    if (NULL != schService) { 
		if (::QueryServiceStatusEx(schService, SC_STATUS_PROCESS_INFO, (LPBYTE)&serviceStatusProcess, sizeof(serviceStatusProcess), &actualSize) && SERVICE_RUNNING == 
serviceStatusProcess.dwCurrentState) {            
			    DebugPrintf(SV_LOG_INFO,"The service : %s  is already running.\n",serviceName.c_str());
				DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
				return true;
        }
		else if (::StartService(schService, 0, NULL) != 0)
		{	
			DebugPrintf(SV_LOG_INFO,"Successfully started the service : %s\n",serviceName.c_str());
			::CloseServiceHandle(schService); 
			DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
			return true;
		}

		// Collect the error ahead of other statement ::CloseServiceHandle()
		dwRet = GetLastError();

		// Close the service handle
		::CloseServiceHandle(schService); 
    }
	
	if( !dwRet )
        	dwRet = GetLastError();
	::FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER |
					FORMAT_MESSAGE_FROM_SYSTEM,
                    NULL,
                    dwRet,
                    0,
                    (LPTSTR)&lpMsgBuf,
                    0,
                    NULL);
	if(dwRet)
	{
		DebugPrintf(SV_LOG_ERROR,"ServiceName: %s => Error: %lu(%s)\n",serviceName.c_str(),dwRet,(LPCTSTR)lpMsgBuf);
	}		

	// Free the buffer
	LocalFree(lpMsgBuf);
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	// Return failure in stopping the service
	return false;
}
#else
bool RollBackVM::startVxServices(const string& serviceName)
{
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	std::string productInstallPath(getVxAgentInstallPath());
	std::string script = productInstallPath + string("/bin/start");

   if (script.empty() || '\0' == script[0] || "\n" == script )
	{
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return true;
	}
    CDPUtil::trim(script);
    string stuff_to_trim = " \n\b\t\a\r\xc" ;
    
	printf("Running script %s\n",script.c_str());
    ACE_Process_Manager* pm = ACE_Process_Manager::instance();
    ACE_Process_Options options;
    options.command_line("%s", script.c_str());

    pid_t pid = pm->spawn(options);

    if (ACE_INVALID_PID == pid) 
	{
        std::ostringstream msg;
		DebugPrintf(SV_LOG_ERROR,"Failed to run %s \n",script.c_str());
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
        return false;
    }

    // wait for the process to finish
    ACE_exitcode status = 0;
    pid_t rc = pm->wait(pid, &status);
    DebugPrintf(SV_LOG_ERROR,"ACE Exit code status : %d  \n",status);
    if (status == 0) 
	{
	    DebugPrintf(SV_LOG_INFO,"Successfully ran SCRIPT command\n");
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return true;
    }
    else 
	{
	     DebugPrintf(SV_LOG_INFO,"[Warning] : Encountered an error while executing cdpcli command\n");
	}   
    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return false;
}
#endif

#ifdef WIN32
bool RollBackVM::stopVxServices(const string& serviceName)
{
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	SC_HANDLE schService;        
    SERVICE_STATUS_PROCESS  serviceStatusProcess;
    SERVICE_STATUS serviceStatus;
    DWORD actualSize = 0;
	ULONG dwRet = 0;
	LPVOID lpMsgBuf;

	SC_HANDLE schSCManager = ::OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if (NULL == schSCManager) {
		DebugPrintf(SV_LOG_ERROR,"Failed to Get handle to Windows Services Manager in OpenSCMManager\n");
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
        return false;
    }

	// stop requested service
	// PR#10815: Long Path support
	schService = SVOpenService(schSCManager, serviceName.c_str(), SERVICE_ALL_ACCESS); 
    if (NULL != schService) { 
		if (::QueryServiceStatusEx(schService, SC_STATUS_PROCESS_INFO, (LPBYTE)&serviceStatusProcess, sizeof(serviceStatusProcess), &actualSize) && SERVICE_RUNNING == 
serviceStatusProcess.dwCurrentState) {
            // stop service
			if (::ControlService(schService, SERVICE_CONTROL_STOP, &serviceStatus)) {
                // wait for it to actually stop
	            DebugPrintf(SV_LOG_INFO,"Waiting for the service %s to stop\n", serviceName.c_str() );
				int retry = 1;
                do {
                    Sleep(1000); 
				} while (::QueryServiceStatusEx(schService, SC_STATUS_PROCESS_INFO, (LPBYTE)&serviceStatusProcess, sizeof(serviceStatusProcess), &actualSize) && SERVICE_STOPPED != 
serviceStatusProcess.dwCurrentState && retry++ <= 1200);
				if (::QueryServiceStatusEx(schService, SC_STATUS_PROCESS_INFO, (LPBYTE)&serviceStatusProcess, sizeof(serviceStatusProcess), &actualSize) && SERVICE_STOPPED == 
serviceStatusProcess.dwCurrentState) {
	                DebugPrintf(SV_LOG_INFO,"Successfully stopped the service: %s \n",serviceName.c_str());
					::CloseServiceHandle(schService); 
					DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
					return true;
				}
            }
		}
		else
		{
	        DebugPrintf(SV_LOG_ERROR,"The service: %s  is not running.\n",serviceName.c_str() );
			//close the service handle
			::CloseServiceHandle(schService); 
			DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
			return true;
		}
		// Collect the error ahead of other statement ::CloseServiceHandle()
		dwRet = GetLastError();
		//close the service handle
		::CloseServiceHandle(schService); 
	}
	
	if( !dwRet )
		dwRet = GetLastError();
	::FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER |
					FORMAT_MESSAGE_FROM_SYSTEM,
                    NULL,
                    dwRet,
                    0,
                    (LPTSTR)&lpMsgBuf,
                    0,
                    NULL);
	if(dwRet)
	{
        DebugPrintf(SV_LOG_ERROR,"ServiceName: %s => Error: %lu(%s)\n",serviceName.c_str(),dwRet,(LPCTSTR)lpMsgBuf);
	}		

	// Free the buffer
	LocalFree(lpMsgBuf);
		
	if(dwRet == ERROR_SERVICE_DOES_NOT_EXIST)
	{
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return true;
	}
    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	// Return failure in stopping the service
    return false;	
}
#else
bool RollBackVM::stopVxServices(const string& serviceName)
{
   DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
   std::string productInstallPath(getVxAgentInstallPath());
   std::string script = productInstallPath + string("/bin/stop");

   if (script.empty() || '\0' == script[0] || "\n" == script )
	{
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return true;
	}

    CDPUtil::trim(script);
    string stuff_to_trim = " \n\b\t\a\r\xc" ;
    /*if (script.find_first_not_of(stuff_to_trim) == string::npos)
        return true;*/

    printf("Running script %s\n",script.c_str());
    ACE_Process_Manager* pm = ACE_Process_Manager::instance();
    ACE_Process_Options options;
    options.command_line("%s", script.c_str());

    pid_t pid = pm->spawn(options);

    if (ACE_INVALID_PID == pid) 
	{
        std::ostringstream msg;
        DebugPrintf(SV_LOG_ERROR,"Failed to run %s\n",script.c_str());
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
        return false;
    }

	// wait for the process to finish
	ACE_exitcode status = 0;
	pid_t rc = pm->wait(pid, &status);
	DebugPrintf(SV_LOG_ERROR,"ACE Exit code status :%d \n",status );
	if (status == 0) 
	{
		DebugPrintf(SV_LOG_INFO,"Successfully ran SCRIPT command\n");
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return true;
	}
	else 
	{
		DebugPrintf(SV_LOG_INFO,"[Warning] : Encountered an error while stopping the vxagent service\n");
	} 
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return true;
}
#endif

#ifdef WIN32
//Function to unmount and delete all mount points for all VMs in given vector.
void RollBackVM::UnmountAndDeleteMtPoints(vector<std::string> vecRecoveredVMs)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    
	ChangeVmConfig objVm;
	objVm.m_bDrDrill = m_bPhySnapShot;
	objVm.m_mapOfVmNameToId = m_mapOfVmNameToId;

    vector<std::string>::iterator iter = vecRecoveredVMs.begin();		
	for(; iter != vecRecoveredVMs.end(); iter++)
	{
        string strVMname = *iter;
        DebugPrintf(SV_LOG_DEBUG,"\nVM Name - %s\n",strVMname.c_str());
		std::map<std::string, std::string> mapOfProtetedDrives;
		if (m_bCloudMT)
		{
			map<string, map<string, string> >::iterator iterHost = m_RecVmsInfo.m_volumeMapping.find(strVMname);
			if (iterHost != m_RecVmsInfo.m_volumeMapping.end())
			{
				map<string, string>::iterator iterVolPair = iterHost->second.begin();
				for (; iterVolPair != iterHost->second.end(); iterVolPair++)
				{
					mapOfProtetedDrives.insert(make_pair(iterVolPair->first, iterVolPair->second + "\\"));
				}
			}
		}
		else
			mapOfProtetedDrives = objVm.getProtectedOrSnapshotDrivesMap(strVMname);
	    if(mapOfProtetedDrives.empty())
	    {
            DebugPrintf(SV_LOG_DEBUG,"No volumes found for the VM - %s.\n",strVMname.c_str());
		    continue;
	    }
        
        std::map<std::string,std::string>::iterator iter = mapOfProtetedDrives.begin();	
    	
	    //Now unmount and delete each mount point.
        //Src Vol = iter->first , Tgt Vol = iter->second (required one)
        for( ; iter != mapOfProtetedDrives.end(); iter++)
	    {

            DebugPrintf(SV_LOG_DEBUG,"Mount Point = %s\n",iter->second.c_str());
            if(false == DeleteVolumeMountPoint(iter->second.c_str()))
            {
                DebugPrintf(SV_LOG_ERROR,"Failed to unmount the volume with above mount point with Error : [%lu].\n",GetLastError());
                DebugPrintf(SV_LOG_INFO,"Please unmount the volume and delete the mount point manually.\n");
            }
            else
            {
                DebugPrintf(SV_LOG_DEBUG,"Unmounted the volume. Deleting the mount point now.\n");
                if(false == RemoveDirectory(iter->second.c_str()))
                    DebugPrintf(SV_LOG_ERROR,"Failed to delete the mount point with Error : [%lu].\n",GetLastError());
                else
                    DebugPrintf(SV_LOG_DEBUG,"Deleted the mount point Successfully.\n");

            }
	    }
    }

    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);    
}

//Function to check if the volume pair is cleared on Cx after its rollback
//Wait time is 60 min inclusive of all the volumes.
//End if Vx pair is deleted on Cx or Wait time is over.
//Input - target volume name and source VM hostname.
void RollBackVM::PollForVolumePair(std::string devicename, std::string srcHostName)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);    
    static int iTimerCount = 1; //static because this is to avoid 60 min for each pair.
      
    FormatVolumeNameForCxReporting(devicename);
    DebugPrintf(SV_LOG_INFO,"Polling to check whether the required volume pair is deleted on CX or not.\n");
    //Over all wait time is 30 min.    
    for(; iTimerCount <= 30; iTimerCount++)
    {            
        map<std::string,std::string> mVxPairs;
        if (false == getVxPairs(srcHostName, mVxPairs))
        {
            DebugPrintf(SV_LOG_ERROR,"Failed to fetch the replication pairs.\n");
        }
        else
        {
            //check the logic whether required volume pair is deleted or not.
            if(false == SearchKeyMapSecond(devicename, mVxPairs))
            {
                DebugPrintf(SV_LOG_INFO,"The volume pair is deleted in CX. Proceeding with cleanup of mount point.\n");
                break;
            }
            else
            {
                DebugPrintf(SV_LOG_INFO,"%d. The volume pair is not yet deleted in CX. ",iTimerCount);

            }
        }
        DebugPrintf(SV_LOG_INFO,"Sleeping for 60 seconds. Retry after that.\n");
        Sleep(60000); //Wait for 1 min.                
    }

    if(iTimerCount >= 60)
    {
        DebugPrintf(SV_LOG_INFO,"The Wait time of 60 minutes is completed already. So proceeding to cleanup of mount point.\n");
    }

    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
}


//Function to search the second values in a map.
//input - key
//return true if key is found else false.
bool RollBackVM::SearchKeyMapSecond(std::string key, map<std::string,std::string> mapPairs)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);    
    bool rv = false;

    DebugPrintf(SV_LOG_INFO,"Search Key = %s.\n",key.c_str());
    map<string,string>::iterator iter = mapPairs.begin();
    for(;iter != mapPairs.end(); iter++)
    {
        DebugPrintf(SV_LOG_INFO,"iter->first = %s <=====> iter->second = %s \n",iter->first.c_str(),iter->second.c_str());        
        if (! strcmpi(iter->second.c_str(),key.c_str()))
        {
            rv = true;      
            break;
        }
    }

    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return rv;
}


bool RollBackVM::generateMapOfVmToSrcAndTgtDiskNmbrs(string& strErrorMsg)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    bool rv = true;

	do
	{
		map<string, string> mapTgtScsiIdToTgtDiskNum;
		CVdsHelper objVds;
		if(objVds.InitilizeVDS())
		{
			DebugPrintf(SV_LOG_INFO,"\nFinding out the disk signatures of all the target disks.\n");
			objVds.GetDisksSignature(mapTgtScsiIdToTgtDiskNum);
			objVds.unInitialize();
		}
		DebugPrintf(SV_LOG_INFO,"\nDiscovered disk signatures at MT are :\n");
		map<string, string>::iterator iDiskSig = mapTgtScsiIdToTgtDiskNum.begin();
		for(; iDiskSig != mapTgtScsiIdToTgtDiskNum.end(); iDiskSig++)
		{
			DebugPrintf(SV_LOG_INFO,"\nDisk Num : %s \tDisk Signature : %s\n", iDiskSig->second.c_str(), iDiskSig->first.c_str());
		}

        map<string, pair<string,string> >::iterator i = rollbackDetails.begin();
        DebugPrintf(SV_LOG_INFO,"\nFetching the Map of VMs to Source and Target Disk Numbers.\n");
        for(; i != rollbackDetails.end(); i++)
        {
			string HostName = i->first;
			DebugPrintf(SV_LOG_INFO,"\nVM ID: %s\n----------------------\n", HostName.c_str());

			m_objLin.m_PlaceHolder.clear();
			string strSrcHostName = "";
			map<string, string>::iterator iterH = m_mapOfVmNameToId.find(HostName);
			if (iterH != m_mapOfVmNameToId.end())
				strSrcHostName = iterH->second;

			map<string, string> mapTgtScsiIdToSrcDiskNum;
			map<string, map<int,string> >::iterator iter;
			if(m_bPhySnapShot)
			{
				iter = m_SnapshotVmsInfo.m_snapshotDisksVolsInfo.m_diskSignatureMapping.find(HostName);
				if(iter == m_SnapshotVmsInfo.m_snapshotDisksVolsInfo.m_diskSignatureMapping.end())
				{
					DebugPrintf(SV_LOG_ERROR,"Disk pair info is not present for VM - %s... Rollback can't be possible for this host.\n", HostName.c_str());
					strErrorMsg = "Master Target " + m_strHostName + " is unable to identify the target disks corresponding to the source machine " + strSrcHostName;
					continue;
				}
			}
			else
			{
				iter = m_RecVmsInfo.m_diskSignatureMapping.find(HostName);
				if(iter == m_RecVmsInfo.m_diskSignatureMapping.end())
				{
					DebugPrintf(SV_LOG_ERROR,"Disk pair info is not present for VM - %s... Rollback can't be possible for this host.\n", HostName.c_str());
					strErrorMsg = "Master Target " + m_strHostName + " is unable to identify the target disks corresponding to the source machine " + strSrcHostName;
					continue;
				}
			}
			map<int,std::string>::iterator iterMap = iter->second.begin();
			for(; iterMap != iter->second.end(); iterMap++)
			{
				char diskNum[65];
				memset(diskNum,0,sizeof(diskNum));
				itoa(iterMap->first,diskNum,10);
				mapTgtScsiIdToSrcDiskNum.insert(make_pair(iterMap->second,diskNum));
			}

			string strMbrFile = string(getVxAgentInstallPath()) + string("\\Failover") + string("\\data\\") + HostName + string("_mbrdata");
			if(false == checkIfFileExists(strMbrFile))
			{
				DebugPrintf(SV_LOG_ERROR,"Failed to get the file %s for VM - %s... Rollback can't be possible for this host.\n",strMbrFile.c_str(), HostName.c_str());
				strErrorMsg = "Master Target " + m_strHostName + " is unable to identify the target disks corresponding to the source machine " + strSrcHostName;
				continue;
			}

			DisksInfoMap_t srcMapOfDiskNamesToDiskInfo;
			DLM_ERROR_CODE RetVal = ReadDiskInfoMapFromFile(strMbrFile.c_str(), srcMapOfDiskNamesToDiskInfo);
			if(DLM_ERR_SUCCESS != RetVal)
			{
				DebugPrintf(SV_LOG_ERROR,"Failed to collect the Source disks info map from the BLOB file(%s). Error Code - %u\n", strMbrFile, RetVal);
				strErrorMsg = "Master Target " + m_strHostName + " is unable to identify the target disks corresponding to the source machine " + strSrcHostName;
				continue;
			}
			m_mapOfHostToDiskInfo.insert(make_pair(HostName, srcMapOfDiskNamesToDiskInfo));

			map<string, string> mapSrcToTgtDiskNum;
			map<string, string>::iterator iterSrc = mapTgtScsiIdToSrcDiskNum.begin();
			map<string, string>::iterator iterTgt;
			for(; iterSrc != mapTgtScsiIdToSrcDiskNum.end(); iterSrc++)
			{
				iterTgt = mapTgtScsiIdToTgtDiskNum.find(iterSrc->first);
				if(iterTgt != mapTgtScsiIdToTgtDiskNum.end())
					mapSrcToTgtDiskNum.insert(make_pair(iterSrc->second, iterTgt->second));
			}

			if(!mapSrcToTgtDiskNum.empty())
			{
				map<unsigned int, unsigned int> MapSrcToTgtDiskNumbers;
				if(false == ConvertMapofStringsToInts(mapSrcToTgtDiskNum, MapSrcToTgtDiskNumbers))
				{
					DebugPrintf(SV_LOG_ERROR,"ConvertMapofStringsToInts failed.\n");
					rv = false; break;
				}
				m_mapOfVmToSrcAndTgtDiskNmbrs.insert(make_pair(HostName,MapSrcToTgtDiskNumbers));
			}
		}
	}while(0);

	if(m_mapOfVmToSrcAndTgtDiskNmbrs.empty())
    {
        DebugPrintf(SV_LOG_ERROR,"Map Of VMname to Source and Target Disk Numbers is empty.\n");
        rv = false;
    }

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return rv;
}


//Create the map for each VM to map of Source and Target Disk Numbers (mapOfVmToSrcAndTgtDiskNmbrs).
bool RollBackVM::createMapOfVmToSrcAndTgtDiskNmbrs()
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    bool rv = true;    

    do
    {
		std::multimap<string, pair<string, string> > mapMissingClusDisks;
		map<string, string> mapTgtScsiIdToTgtDiskNum;
		CVdsHelper objVds;
		if(objVds.InitilizeVDS())
		{
			DebugPrintf(SV_LOG_INFO,"\nFinding out the disk signatures of all the target disks.\n");
			objVds.GetDisksSignature(mapTgtScsiIdToTgtDiskNum);
			objVds.unInitialize();
		}
		DebugPrintf(SV_LOG_INFO,"\nDiscovered disk signatures at MT are :\n");
		map<string, string>::iterator iDiskSig = mapTgtScsiIdToTgtDiskNum.begin();
		for(; iDiskSig != mapTgtScsiIdToTgtDiskNum.end(); iDiskSig++)
		{
			DebugPrintf(SV_LOG_INFO,"\nDisk Num : %s \tDisk Signature : %s\n", iDiskSig->second.c_str(), iDiskSig->first.c_str());
		}

        map<string, pair<string,string> >::iterator i = rollbackDetails.begin();
        DebugPrintf(SV_LOG_INFO,"\nFetching the Map of VMs to Source and Target Disk Numbers.\n");
        for(; i != rollbackDetails.end(); i++)
        {
            string HostName = i->first;
			DebugPrintf(SV_LOG_INFO,"\nVM ID: %s\n----------------------\n", HostName.c_str());
			
			map<string, string>::iterator iter = m_mapOfVmNameToId.find(HostName);
			DebugPrintf(SV_LOG_INFO,"\nVM Name : %s\n----------------------\n", iter->second.c_str());

			string strMbrFile = string(getVxAgentInstallPath()) + string("\\Failover") + string("\\data\\") + HostName + string("_mbrdata");

			if((true == checkIfFileExists(strMbrFile)) && (m_vConVersion >= 4.0))
			{
				DLM_ERROR_CODE RetVal;
				DisksInfoMap_t srcMapOfDiskNamesToDiskInfo;
				if(m_mapOfHostToDiskInfo.find(HostName) == m_mapOfHostToDiskInfo.end())
				{
					RetVal = ReadDiskInfoMapFromFile(strMbrFile.c_str(), srcMapOfDiskNamesToDiskInfo);
					if(DLM_ERR_SUCCESS != RetVal)
					{
						DebugPrintf(SV_LOG_ERROR,"Failed to collect the Source disks info map from the BLOB file(%s). Error Code - %u\n", strMbrFile.c_str(), RetVal);
						break;
					}
					m_mapOfHostToDiskInfo.insert(make_pair(HostName, srcMapOfDiskNamesToDiskInfo));
				}
				else
				{
					srcMapOfDiskNamesToDiskInfo = m_mapOfHostToDiskInfo.find(HostName)->second;
				}
				

				double dlmVersion = 0;
				RetVal = GetDlmVersion(strMbrFile.c_str(), dlmVersion);
				if(DLM_ERR_SUCCESS != RetVal)
				{
					DebugPrintf(SV_LOG_ERROR,"Failed to get the dlm version.");
				}
				m_mapOfHostIdDlmVersion[HostName] = dlmVersion;

				if(IsClusterNode(HostName))
				{
					string strClusName = GetNodeClusterName(HostName);
					DebugPrintf(SV_LOG_INFO,"Cluster Name for host %s is : %s\n", HostName.c_str(), strClusName.c_str());

					std::list<string> lstClusNodes;
					GetClusterNodes(strClusName, lstClusNodes);
					
					std::list<string>::iterator iterList = lstClusNodes.begin();
					for(; iterList != lstClusNodes.end(); iterList++)
					{
						/*if(iterList->compare(HostName) == 0)
							continue;*/
						if(m_mapOfHostToDiskInfo.find(HostName) != m_mapOfHostToDiskInfo.end())
							continue;

						strMbrFile = string(getVxAgentInstallPath()) + string("\\Failover") + string("\\data\\") + (*iterList) + string("_mbrdata");

						if(true == checkIfFileExists(strMbrFile))
						{
							DisksInfoMap_t mapOfDiskNamesToDiskInfo;
							DLM_ERROR_CODE RetVal = ReadDiskInfoMapFromFile(strMbrFile.c_str(), mapOfDiskNamesToDiskInfo);
							if(DLM_ERR_SUCCESS != RetVal)
							{
								DebugPrintf(SV_LOG_ERROR,"Failed to collect the Source disks info map from the BLOB file(%s). Error Code - %u\n", strMbrFile.c_str(), RetVal);
								continue;
							}
							m_mapOfHostToDiskInfo.insert(make_pair(*iterList, mapOfDiskNamesToDiskInfo));

							dlmVersion = 0;
							RetVal = GetDlmVersion(strMbrFile.c_str(), dlmVersion);
							if(DLM_ERR_SUCCESS != RetVal)
							{
								DebugPrintf(SV_LOG_ERROR,"Failed to get the dlm version.");
							}
							m_mapOfHostIdDlmVersion[*iterList] = dlmVersion;
						}
						else
							DebugPrintf(SV_LOG_INFO,"DLM MBR file %s does not exist for node : %s\n", strMbrFile.c_str(), iterList->c_str());
					}
				}

				string strDiskInfoFile;
				if(m_bPhySnapShot)
					strDiskInfoFile = std::string(getVxAgentInstallPath()) + std::string("\\Failover") + std::string("\\data\\") + HostName + string(".dsinfo");
				else
					strDiskInfoFile = std::string(getVxAgentInstallPath()) + std::string("\\Failover") + std::string("\\data\\") + HostName + string(".dpinfo");


				if(true == checkIfFileExists(strDiskInfoFile))
				{
					m_vDInfoVersion = 1.0; // Setting to default value for the host.
					map<string, string> mapSrcToTgtDiskNum;
					map<string, dInfo_t> mapTgtScsiIdToSrcDiskInfo;
					if(false == GetMapOfSrcDiskToTgtDiskNumFromDinfo(HostName, strDiskInfoFile, mapTgtScsiIdToSrcDiskInfo))
					{
						DebugPrintf(SV_LOG_ERROR,"Failed to get the map of source to target disk numbers for VM - %s\n", HostName.c_str());
						rv = false; break;
					}

					map<string, dInfo_t>::iterator iterSrc;
					map<string, string>::iterator iterTgt;

					iterSrc = mapTgtScsiIdToSrcDiskInfo.find("Version");
					if(iterSrc != mapTgtScsiIdToSrcDiskInfo.end())
					{
						stringstream ss;
						ss << iterSrc->second.DiskNum;
						ss >> m_vDInfoVersion;
						mapTgtScsiIdToSrcDiskInfo.erase("Version");
					}

					for(iterSrc = mapTgtScsiIdToSrcDiskInfo.begin(); iterSrc != mapTgtScsiIdToSrcDiskInfo.end(); iterSrc++)
					{
						iterTgt = mapTgtScsiIdToTgtDiskNum.find(iterSrc->first);
						if(iterTgt != mapTgtScsiIdToTgtDiskNum.end())
						{
							if(m_vDInfoVersion > 1.0 && dlmVersion >= 1.2)
							{
								DisksInfoMapIter_t iterDiskInfo = srcMapOfDiskNamesToDiskInfo.begin();
								while(iterDiskInfo != srcMapOfDiskNamesToDiskInfo.end())
								{
									if((0 == iterSrc->second.DiskSignature.compare(iterDiskInfo->second.DiskSignature)) &&
										(0 == iterSrc->second.DiskDeviceId.compare(iterDiskInfo->second.DiskDeviceId)))
									{
										DebugPrintf(SV_LOG_INFO,"Both Disk Signature and device id matched for host %s\n", HostName.c_str());
										break;
									}
									else if((0 == iterSrc->second.DiskSignature.compare(iterDiskInfo->second.DiskSignature)))
									{
										DebugPrintf(SV_LOG_WARNING,"Device id not found for host %s, proceeding with signature comparison\n", HostName.c_str());
										break;
									}
									iterDiskInfo++;
								}
								if(iterDiskInfo != srcMapOfDiskNamesToDiskInfo.end())
								{
									size_t pos = 17; //Size of "\\\\.\\PHYSICALDRIVE"
									string strDiskNum = iterDiskInfo->first.substr(pos);
									mapSrcToTgtDiskNum.insert(make_pair(strDiskNum, iterTgt->second));
								}
								else
								{
									mapMissingClusDisks.insert(make_pair(HostName,make_pair(iterSrc->second.DiskSignature,iterSrc->first)));
									DebugPrintf(SV_LOG_WARNING,"Disk signature[%s (in DWORD)] not found in mbr file of %s\n",iterSrc->second.DiskSignature.c_str(),HostName.c_str());
								}
							}
							else
							{
								mapSrcToTgtDiskNum.insert(make_pair(iterSrc->second.DiskNum, iterTgt->second));
							}
						}
					}

					if(!mapSrcToTgtDiskNum.empty())
					{
						map<unsigned int, unsigned int> MapSrcToTgtDiskNumbers;
						if(false == ConvertMapofStringsToInts(mapSrcToTgtDiskNum, MapSrcToTgtDiskNumbers))
						{
							DebugPrintf(SV_LOG_ERROR,"ConvertMapofStringsToInts failed.\n");
							rv = false; break;
						}
						m_mapOfVmToSrcAndTgtDiskNmbrs.insert(make_pair(HostName,MapSrcToTgtDiskNumbers));
					}
				}
				else
				{
					DebugPrintf(SV_LOG_ERROR,"Failed to get the file %s for VM - %s\n",strDiskInfoFile.c_str(), HostName.c_str());
				}
			}
			else
			{
				map<unsigned int, unsigned int> MapSrcToTgtDiskNumbers;
				if(false == GenerateDisksMap(HostName, MapSrcToTgtDiskNumbers))
				{
					DebugPrintf(SV_LOG_ERROR,"Failed to get the map of source to target disk numbers for VM - %s\n", HostName.c_str());
					rv = false; break;
				}
				m_mapOfVmToSrcAndTgtDiskNmbrs.insert(make_pair(HostName,MapSrcToTgtDiskNumbers));
			}
        }
        if(false == rv)
            break;

		//Add the missing disk mapping to the VM which is holding corresponding disk info. We see this case in win2k3 cluster
		// 1. Locate the node where the disk information is available using disk signature
		// 2. Build the src-tgt disk number mapping
		// 3. Insert the src-tgt disk number mapping to the node.
		std::multimap<string, pair<string, string> >::iterator iterMisDisk = mapMissingClusDisks.begin();
		for(;iterMisDisk != mapMissingClusDisks.end(); iterMisDisk++)
		{
			double dlmVersion = 0;
			if(m_mapOfHostIdDlmVersion.find(iterMisDisk->first) != m_mapOfHostIdDlmVersion.end())
				dlmVersion = m_mapOfHostIdDlmVersion[iterMisDisk->first];

			if(dlmVersion >= 1.2 && IsClusterNode(iterMisDisk->first) && IsWin2k3OS(iterMisDisk->first))
			{
				std::list<string> clusNodes;
				GetClusterNodes(GetNodeClusterName(iterMisDisk->first),clusNodes);
				std::list<string>::iterator iterNode = clusNodes.begin();
				for(;iterNode!=clusNodes.end();iterNode++)
				{
					DisksInfoMapIter_t iterDisk;
					if(GetClusterDiskMapIter(*iterNode,iterMisDisk->second.first,iterDisk))
					{
						size_t pos = iterDisk->first.find("\\\\.\\PHYSICALDRIVE");
						if(pos != string::npos)
						{
							pos += 17;//Size of "\\\\.\\PHYSICALDRIVE"
							string DiskNum = iterDisk->first.substr(pos);
							int iSrcDisk = atoi(DiskNum.c_str());
							//If we get 0 for iSrcDisk then this will be handled below so that if a 0 disk is laready included 
							//    then this 0 will not be included again.

							DiskNum = mapTgtScsiIdToTgtDiskNum[iterMisDisk->second.second];
							int iTgtDisk = atoi(DiskNum.c_str());
							//No replicating disk on mt get disk numner as 0.
							//So considering 0 as error in this case
							DebugPrintf(SV_LOG_INFO,"Updating missing disk entry [%d]=[%d] to the node %s\n",
								iSrcDisk, iTgtDisk,
								iterNode->c_str());
							if( iTgtDisk>0 &&
								m_mapOfVmToSrcAndTgtDiskNmbrs.find(*iterNode) != m_mapOfVmToSrcAndTgtDiskNmbrs.end() &&
								m_mapOfVmToSrcAndTgtDiskNmbrs[*iterNode].find(iSrcDisk) == m_mapOfVmToSrcAndTgtDiskNmbrs[*iterNode].end())
							{
								m_mapOfVmToSrcAndTgtDiskNmbrs[*iterNode].insert(make_pair(iSrcDisk,iTgtDisk));
								DebugPrintf(SV_LOG_INFO,"Successfully updated the missing disk maping\n");
							}
						}
						break;
					}
				}
				if(iterNode == clusNodes.end())
				{
					DebugPrintf(SV_LOG_ERROR,"Disk information not found for [%s]=[%s] in %s mbr file. This disk will be skipped in recovery\n",
					iterMisDisk->second.first.c_str(),
					iterMisDisk->second.second.c_str(),
					iterMisDisk->first.c_str());
				}
			}
			else
			{
				DebugPrintf(SV_LOG_ERROR,"Disk information not found for [%s]=[%s] in %s mbr file. This disk will be skipped in recovery\n",
					iterMisDisk->second.first.c_str(),
					iterMisDisk->second.second.c_str(),
					iterMisDisk->first.c_str());
			}
		}

        if(m_mapOfVmToSrcAndTgtDiskNmbrs.empty())
        {
            DebugPrintf(SV_LOG_ERROR,"Map Of VMname to Source and Target Disk Numbers is empty.\n");
            rv = false;
        }
    }while(0);    

    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return rv;
}

//Note: Need to enhance this API for cluster
bool RollBackVM::createMapOfVmToSrcAndTgtDiskNmbrs(string& strHostName)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    bool rv = true;

	do
	{
		map<string, string> mapTgtScsiIdToTgtDiskNum;
		map<string, string> mapSrcToTgtDiskSig;
		CVdsHelper objVds;
		if(objVds.InitilizeVDS())
		{
			DebugPrintf(SV_LOG_INFO,"\nFinding out the disk signatures of all the target disks.\n");
			objVds.GetDisksSignature(mapTgtScsiIdToTgtDiskNum);
			objVds.unInitialize();
		}
		DebugPrintf(SV_LOG_INFO,"\nDiscovered disk signatures at MT are :\n");
		map<string, string>::iterator iDiskSig = mapTgtScsiIdToTgtDiskNum.begin();
		for(; iDiskSig != mapTgtScsiIdToTgtDiskNum.end(); iDiskSig++)
		{
			DebugPrintf(SV_LOG_INFO,"\nDisk Num : %s \tDisk Signature : %s\n", iDiskSig->second.c_str(), iDiskSig->first.c_str());
		}

		string strDiskInfoFile = std::string(getVxAgentInstallPath()) + std::string("\\Failover") + std::string("\\data\\") + strHostName + string(".dpinfo");
		if(!checkIfFileExists(strDiskInfoFile))
		{
			DebugPrintf(SV_LOG_INFO,"File [%s] not found. Can't proceed further.\n", strDiskInfoFile.c_str());
			rv = false; break;
		}

		m_vDInfoVersion = 1.0; // Setting to default value for the host.
		map<string, string> mapSrcToTgtDiskNum;
		map<string, dInfo_t> mapTgtScsiIdToSrcDiskInfo;
		if(false == GetMapOfSrcDiskToTgtDiskNumFromDinfo(strHostName, strDiskInfoFile, mapTgtScsiIdToSrcDiskInfo))
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to get the map of source to target disk numbers for VM - %s\n", strHostName.c_str());
			rv = false; break;
		}

		map<string, dInfo_t>::iterator iterSrc;
		map<string, string>::iterator iterTgt;

		iterSrc = mapTgtScsiIdToSrcDiskInfo.find("Version");
		if(iterSrc != mapTgtScsiIdToSrcDiskInfo.end())
		{
			stringstream ss;
			ss << iterSrc->second.DiskNum;
			ss >> m_vDInfoVersion;
			mapTgtScsiIdToSrcDiskInfo.erase("Version");
		}

		double dlmVersion = 0;
		if(m_mapOfHostIdDlmVersion.find(strHostName) != m_mapOfHostIdDlmVersion.end())
			dlmVersion = m_mapOfHostIdDlmVersion[strHostName];

		DisksInfoMap_t srcMapOfDiskNamesToDiskInfo;
		if(m_mapOfHostToDiskInfo.find(strHostName) != m_mapOfHostToDiskInfo.end())
			srcMapOfDiskNamesToDiskInfo = m_mapOfHostToDiskInfo[strHostName];
		else
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to get source layout information for host - %s\n", strHostName.c_str());
			rv = false; break;
		}

		for(iterSrc = mapTgtScsiIdToSrcDiskInfo.begin(); iterSrc != mapTgtScsiIdToSrcDiskInfo.end(); iterSrc++)
		{
			iterTgt = mapTgtScsiIdToTgtDiskNum.find(iterSrc->first);
			if(iterTgt != mapTgtScsiIdToTgtDiskNum.end())
			{
				if(m_vDInfoVersion > 1.0 && dlmVersion >= 1.2)
				{
					DisksInfoMapIter_t iterDiskInfo = srcMapOfDiskNamesToDiskInfo.begin();
					while(iterDiskInfo != srcMapOfDiskNamesToDiskInfo.end())
					{
						if((0 == iterSrc->second.DiskSignature.compare(iterDiskInfo->second.DiskSignature)) &&
							(0 == iterSrc->second.DiskDeviceId.compare(iterDiskInfo->second.DiskDeviceId)))
						{
							DebugPrintf(SV_LOG_INFO,"Both Disk Signature and device id matched for host %s\n", strHostName.c_str());
							break;
						}
						else if((0 == iterSrc->second.DiskSignature.compare(iterDiskInfo->second.DiskSignature)))
						{
							DebugPrintf(SV_LOG_WARNING,"Device id not found for host %s, proceeding with signature comparison\n", strHostName.c_str());
							break;
						}
						iterDiskInfo++;
					}
					if(iterDiskInfo != srcMapOfDiskNamesToDiskInfo.end())
					{
						size_t pos = 17; //Size of "\\\\.\\PHYSICALDRIVE"
						string strDiskNum = iterDiskInfo->first.substr(pos);
						mapSrcToTgtDiskNum.insert(make_pair(strDiskNum, iterTgt->second));
					}
					else
					{
						DebugPrintf(SV_LOG_WARNING,"Disk signature[%s (in DWORD)] not found in mbr file of %s\n",iterSrc->second.DiskSignature.c_str(),strHostName.c_str());
					}
				}
				else
				{
					mapSrcToTgtDiskNum.insert(make_pair(iterSrc->second.DiskNum, iterTgt->second));
				}
				mapSrcToTgtDiskSig.insert(make_pair(iterTgt->second, iterTgt->first));
			}
		}

		if(!mapSrcToTgtDiskNum.empty())
		{
			m_mapOfVmToSrcAndTgtDisks.insert(make_pair(strHostName, mapSrcToTgtDiskNum));
			map<unsigned int, unsigned int> MapSrcToTgtDiskNumbers;
			if(false == ConvertMapofStringsToInts(mapSrcToTgtDiskNum, MapSrcToTgtDiskNumbers))
			{
				DebugPrintf(SV_LOG_ERROR,"ConvertMapofStringsToInts failed.\n");
				rv = false; break;
			}
			m_mapOfVmToSrcAndTgtDiskNmbrs.insert(make_pair(strHostName,MapSrcToTgtDiskNumbers));
		}
		else
		{
			DebugPrintf(SV_LOG_ERROR,"Empty disk mapping list found for host - %s\n", strHostName.c_str());
			rv = false;
		}
		if(!mapSrcToTgtDiskSig.empty())
		{
			m_mapOfVmToSrcAndTgtDiskSig.insert(make_pair(strHostName, mapSrcToTgtDiskSig));			
		}
		else
		{
			DebugPrintf(SV_LOG_ERROR,"Empty disk mapping list found for host - %s\n", strHostName.c_str());
			rv = false;
		}
	}while(0);

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return rv;
}

//Convert the map of strings to map of integers
//Input - Map of strings 
//Output - Map of integers
//Return value - true if successfull and false else
bool RollBackVM::ConvertMapofStringsToInts(map<string,string> mapOfStrings, map<unsigned int,unsigned int> &mapOfIntegers)
{
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    map<string,string>::iterator iterString;
    if (mapOfStrings.empty())
    {
        DebugPrintf(SV_LOG_ERROR,"Input Map of strings is empty. Cannot continue.\n");
        DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
        return false;
    }
    iterString = mapOfStrings.begin();
    while (iterString != mapOfStrings.end())
    {
        unsigned int iFirst,iSecond;
        DebugPrintf(SV_LOG_INFO,"iterString->first=%s  <==>  iterString->second=%s\n",iterString->first.c_str(),iterString->second.c_str());
        if(isAnInteger(iterString->first.c_str()))
            iFirst = atoi(iterString->first.c_str());
        else
        {
            DebugPrintf(SV_LOG_ERROR,"Failed to convert %s into integer(1).\n",iterString->first.c_str());
            DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
            return false;
        }
        if(isAnInteger(iterString->second.c_str()))
            iSecond = atoi(iterString->second.c_str());
        else
        {
            DebugPrintf(SV_LOG_ERROR,"Failed to convert %s into integer(2).\n",iterString->second.c_str());
            DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
            return false;
        }
        mapOfIntegers.insert(make_pair(iFirst,iSecond));
        DebugPrintf(SV_LOG_INFO,"iFirst=%d  <==>  iSecond=%d\n",iFirst,iSecond);
        iterString++;
    }
    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return true;
}

bool RollBackVM::GenerateDisksMap(string HostName, map<unsigned int, unsigned int> & MapSrcToTgtDiskNumbers)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    bool rv = true;    

    do
    {
        map<string, unsigned int> SrcMapVolNameToDiskNumber;
		map<string, string>::iterator iter = m_mapOfVmNameToId.find(HostName);

		//if(m_vConVersion >= 4.0 && (HostName.compare(iter->second) != 0))
		//{
		//	if(false == GetMapVolNameToDiskNumberFromDiskInfoMap(HostName, SrcMapVolNameToDiskNumber))
		//	{
		//		DebugPrintf(SV_LOG_ERROR,"Failed to get the map of volumes to disk numbers from Disk Binary file.\n");
		//		rv = false; break;
		//	}
		//}
		//else
		//{
		HostName = iter->second;
		if(false == GetMapVolNameToDiskNumberFromDiskBin(HostName, SrcMapVolNameToDiskNumber))
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to get the map of volumes to disk numbers from Disk Binary file.\n");
			rv = false; break;
		}
		//}

        map<string, unsigned int> TgtMapVolNameToDiskNumber;
        map<string, string> MapSrcVolToTgtVol;

		//search in pinfo file in case of rollback else sinfo file in case of physical snapshot
        if(false == GetMapVolNameToDiskNumber(HostName, TgtMapVolNameToDiskNumber, MapSrcVolToTgtVol)) 
        {
            DebugPrintf(SV_LOG_ERROR,"Failed to get the map of volumes to disk numbers from pInfo file.\n");
            rv = false; break;
        }

        // Generate required map from above three maps
        map<string, string>::iterator iterVol = MapSrcVolToTgtVol.begin();
        for(; iterVol != MapSrcVolToTgtVol.end(); iterVol++)
        {
            // iterVol->first = source vol             
            map<string, unsigned int>::iterator iterSrcMap = SrcMapVolNameToDiskNumber.find(iterVol->first);
            if(iterSrcMap == SrcMapVolNameToDiskNumber.end())
            {
                DebugPrintf(SV_LOG_ERROR,"Failed to find the source volume[%s] in source map of volume names to disk numbers.\n",iterVol->first.c_str());
                rv = false; break;
            }

            // iterVol->second = target vol
            map<string, unsigned int>::iterator iterTgtMap = TgtMapVolNameToDiskNumber.find(iterVol->second);
            if(iterTgtMap == TgtMapVolNameToDiskNumber.end())
            {
                DebugPrintf(SV_LOG_ERROR,"Failed to find the target volume[%s] in target map of volume names to disk numbers.\n",iterVol->second.c_str());
                rv = false; break;
            }
            
            //(Key,Value) = (Source,Target) Disk Number
            MapSrcToTgtDiskNumbers.insert(make_pair(iterSrcMap->second, iterTgtMap->second));
        }   
        if(false == rv)
            break;

        if(MapSrcToTgtDiskNumbers.empty())
        {
            DebugPrintf(SV_LOG_ERROR, "Map of source to target disk numbers is empty for VM - %s\n", HostName.c_str());
            rv = false; break;       
        }
        else
        {
            map<unsigned int, unsigned int>::iterator i = MapSrcToTgtDiskNumbers.begin();
            for(; i!=MapSrcToTgtDiskNumbers.end(); i++)
            {
                DebugPrintf(SV_LOG_INFO,"Source::Target(Disk Numbers) ==> %u :: %u\n", i->first, i->second);
            }
        }

    }while(0);
    
    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return rv;
}

bool RollBackVM::GetMapVolNameToDiskNumberFromDiskBin(string HostName, 
                                                      map<string, unsigned int> & SrcMapVolNameToDiskNumber)
{
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    bool rv = true;    

    string DiskBinFileName = std::string(getVxAgentInstallPath()) + std::string("\\Failover\\data\\") + HostName + string(DISKBIN_EXTENSION);
    FILE *f = fopen(DiskBinFileName.c_str(),"rb");
	if(!f)
	{
		DebugPrintf(SV_LOG_ERROR,"Failed to open file : %s with Error: %ld \n",DiskBinFileName.c_str(), GetLastError());
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;
	}

    DiskInformation * pDiskInfo;
    pDiskInfo = new DiskInformation;
    memset(pDiskInfo,0,sizeof(*pDiskInfo));
    while(fread(pDiskInfo,sizeof(*pDiskInfo),1,f))
    {
        DebugPrintf(SV_LOG_INFO,"========================================================\n");                
        DebugPrintf(SV_LOG_INFO," Disk Id                      = %u\n",pDiskInfo->uDiskNumber);
        DebugPrintf(SV_LOG_INFO," Disk Type                    = %d\n",pDiskInfo->dt);
        DebugPrintf(SV_LOG_INFO," Disk No of Actual Partitions = %lu\n",pDiskInfo->dwActualPartitonCount);
        DebugPrintf(SV_LOG_INFO," Disk No of Partitions        = %lu\n",pDiskInfo->dwPartitionCount);
        DebugPrintf(SV_LOG_INFO," Disk Partition Style         = %d\n",pDiskInfo->pst);
        DebugPrintf(SV_LOG_INFO," Disk Signature               = %lu\n",pDiskInfo->ulDiskSignature);
        DebugPrintf(SV_LOG_INFO," Disk Size                    = %I64d\n",pDiskInfo->DiskSize.QuadPart);

        PartitionInfo * ArrPartInfo;
        ArrPartInfo = new PartitionInfo [pDiskInfo->dwActualPartitonCount];
        if(NULL == ArrPartInfo)
	    {
		    DebugPrintf(SV_LOG_ERROR,"Cannot create Partition Information structures. Cannot continue!\n");
		    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
            delete pDiskInfo;
		    return false;
	    }
        //Fetch partitions on the disk
	    for(unsigned int i = 0; i < pDiskInfo->dwActualPartitonCount; i++)
	    {			    
		    fread(&ArrPartInfo[i],sizeof(ArrPartInfo[i]),1,f);
            DebugPrintf(SV_LOG_INFO,"\n  Start of Partition : %d\n  ------------------------\n",i+1);
            DebugPrintf(SV_LOG_INFO,"  Partition Disk Number       = %u\n",ArrPartInfo[i].uDiskNumber);
            DebugPrintf(SV_LOG_INFO,"  Partition Partition Type    = %d\n",ArrPartInfo[i].uPartitionType);
            DebugPrintf(SV_LOG_INFO,"  Partition Boot Indicator    = %d\n",ArrPartInfo[i].iBootIndicator);
            DebugPrintf(SV_LOG_INFO,"  Partition No of Volumes     = %u\n",ArrPartInfo[i].uNoOfVolumesInParttion);
            DebugPrintf(SV_LOG_INFO,"  Partition Partition Length  = %I64d\n",ArrPartInfo[i].PartitionLength.QuadPart);
            DebugPrintf(SV_LOG_INFO,"  Partition Start Offset      = %I64d\n",ArrPartInfo[i].startOffset.QuadPart);
            DebugPrintf(SV_LOG_INFO,"  Partition Ending Offset     = %I64d\n",ArrPartInfo[i].EndingOffset.QuadPart);

	        volInfo * ArrVolInfo;
            ArrVolInfo = new volInfo [ArrPartInfo[i].uNoOfVolumesInParttion];
		    if(NULL == ArrVolInfo)
		    {
			    DebugPrintf(SV_LOG_ERROR,"Cannot create Volume Information Structures. Cannot continue!\n");
			    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
                delete[] ArrPartInfo;
                delete pDiskInfo;
			    return false;
		    }
            //Fetch volumes on each partition.
		    for(unsigned int j = 0; j < ArrPartInfo[i].uNoOfVolumesInParttion; j++)
		    {
			    fread(&ArrVolInfo[j],sizeof(ArrVolInfo[j]),1,f);              
                DebugPrintf(SV_LOG_INFO,"\n   Start of Volume : %d\n   ---------------------\n",j+1);
	            DebugPrintf(SV_LOG_INFO,"   Volume Volume Name         = %s\n",ArrVolInfo[j].strVolumeName);
	            DebugPrintf(SV_LOG_INFO,"   Volume Partition Length    = %I64d\n",ArrVolInfo[j].partitionLength.QuadPart);
	            DebugPrintf(SV_LOG_INFO,"   Volume Starting Offset     = %I64d\n",ArrVolInfo[j].startingOffset.QuadPart);
	            DebugPrintf(SV_LOG_INFO,"   Volume Ending Offset       = %I64d\n",ArrVolInfo[j].endingOffset.QuadPart);	            
                SrcMapVolNameToDiskNumber.insert(make_pair(ArrVolInfo[j].strVolumeName, pDiskInfo->uDiskNumber));
            }           
            delete[] ArrVolInfo;
	    }
        delete[] ArrPartInfo;
    }
    delete pDiskInfo;
    DebugPrintf(SV_LOG_INFO,"------------------------------------------------------------\n");

    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return rv;
}

/********************************************************************************************
This function will create the map of volumename to disk number for the given host .
It will get the informations form diskinfo map .
Input -- hostname
output -- map of source volume to disk number.
********************************************************************************************/
bool RollBackVM::GetMapVolNameToDiskNumberFromDiskInfoMap(string HostName, map<string, unsigned int> & SrcMapVolNameToDiskNumber)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    bool rv = true;

	map<string, DisksInfoMap_t>::iterator iter = m_mapOfHostToDiskInfo.find(HostName);
	DisksInfoMapIter_t diskIter = iter->second.begin();

	for(; diskIter != iter->second.end(); diskIter++)
	{
		string strPhysicalDeviceName = "\\\\.\\PHYSICALDRIVE";
		string strDiskNumber = diskIter->first.substr(strPhysicalDeviceName.size());
		stringstream ss; 
		ss << strDiskNumber;
		unsigned int nDiskNum;
		ss >> nDiskNum;
		VolumesInfoVecIter_t volIter = diskIter->second.VolumesInfo.begin();
		for(;volIter != diskIter->second.VolumesInfo.end(); volIter++)
		{			
			SrcMapVolNameToDiskNumber.insert(make_pair(string(volIter->VolumeName), nDiskNum));
		}
	}

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return rv;
}

//Will get this info from Pinfo file if its rollback, else will get from Sinfo file snapshot case.
bool RollBackVM::GetMapVolNameToDiskNumber(string HostName,
                                                    map<string, unsigned int> & TgtMapVolNameToDiskNumber,
                                                    map<string, string> & MapSrcVolToTgtVol)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    bool rv = true;

	ChangeVmConfig objVm;
	objVm.m_bDrDrill = m_bPhySnapShot;
	objVm.m_mapOfVmNameToId = m_mapOfVmNameToId;

    do
    {
		MapSrcVolToTgtVol = objVm.getProtectedOrSnapshotDrivesMap(HostName);
        if(MapSrcVolToTgtVol.empty())
        {
            DebugPrintf(SV_LOG_ERROR,"Protected/Snapshot Pairs is empty for the VM - %s\n", HostName.c_str());
            rv = false; break;
        }

        map<string, string>::iterator i = MapSrcVolToTgtVol.begin();
        for(; i!= MapSrcVolToTgtVol.end(); i++)
        {
            // i->first  : Src Vol; (C:\)
            // i->second : Tgt Vol; (C:\ESX\HostName_C)
            unsigned int dn;
            if(false == GetDiskNumberOfVolume(i->second, dn))
            {
                DebugPrintf(SV_LOG_ERROR,"Failed to get Disk Number of the volume - %s\n",i->second.c_str());
                rv = false; break;
            }
            DebugPrintf(SV_LOG_INFO,"dn = %u\n",dn);
            TgtMapVolNameToDiskNumber.insert(make_pair(i->second,dn));
        }
    }while(0);
    
    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return rv;
}

bool RollBackVM::GetDiskNumberOfVolume(string VolName, unsigned int & dn)
{
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    bool rv = true;
    do
    {
        // Fetch volume GUID
        char VolGuid[MAX_PATH];
        memset(VolGuid,0,MAX_PATH);
        if(0 == GetVolumeNameForVolumeMountPoint(VolName.c_str(), VolGuid, MAX_PATH))
		{            
            DebugPrintf(SV_LOG_ERROR,"Failed to get volume GUID for the volume[%s] with Errorcode : [%lu].\n", VolName.c_str(), GetLastError());
			rv = false;	break;                   
		}

        DebugPrintf(SV_LOG_INFO,"Before : %s ==> %s\n", VolName.c_str(), VolGuid); 
        // Remove the trailing "\" from volume GUID
        std::string strTemp = string(VolGuid);        
        std::string::size_type pos = strTemp.find_last_of("\\");
        if(pos != std::string::npos)
        {
            strTemp.erase(pos);
        }
        DebugPrintf(SV_LOG_INFO,"After : %s ==> %s\n", VolName.c_str(), strTemp.c_str()); 

        HANDLE	hVolume;	
		hVolume = CreateFile(strTemp.c_str(),GENERIC_READ, 
                             FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 
                             FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT  , NULL 
                             );
		if( hVolume == INVALID_HANDLE_VALUE ) 
		{
            //Note - in creating handle vol guid is passed but to understand error displaying vol name below.
			DebugPrintf(SV_LOG_ERROR,"Failed to open handle for volume[%s] with Error Code : [%lu].\n", VolName.c_str(), GetLastError());
			rv = false;	break;
		}

		ULONG	bytesWritten;
        UCHAR	DiskExtentsBuffer[0x400];
        PVOLUME_DISK_EXTENTS DiskExtents = (PVOLUME_DISK_EXTENTS)DiskExtentsBuffer;
		if( DeviceIoControl(hVolume, IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS,
                            NULL, 0, DiskExtents, sizeof(DiskExtentsBuffer),
                            &bytesWritten, NULL) ) 
		{	            
			for( DWORD extent = 0; extent < DiskExtents->NumberOfDiskExtents; extent++ ) 
			{
                dn = DiskExtents->Extents[extent].DiskNumber;                
			}                      
		}
		else
		{
            DebugPrintf(SV_LOG_ERROR,"IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS failed for volume[%s] with Error code : [%lu].\n", VolName.c_str(), GetLastError());
			CloseHandle(hVolume);
            rv = false;	break;
		}
		CloseHandle(hVolume);
    }while(0);

    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return rv;
}


//Read the File with 2 Value separated by following separator.
//Separator - !@!@!
//Sample Format - val1!@!@!val2
//Input - File Name , Output - map of values in the files.
//Return value - true if file is successfully read and in correct format; else false.            
bool RollBackVM::ReadFileWith2Values(string strFileName, map<string,string> &mapOfStrings)
{
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    std::string token("!@!@!");
	std::size_t index;
    string strLineRead;
    string strFirstValue,strSecondValue;

    if(false == checkIfFileExists(strFileName))
	{
        DebugPrintf(SV_LOG_ERROR,"File %s does not exist.\n",strFileName.c_str());
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;
	}

    ifstream readFile(strFileName.c_str());
    if (!readFile.is_open())
    {
        DebugPrintf(SV_LOG_ERROR,"Failed to read the File : %s with Error : [%lu].\n",strFileName.c_str(),GetLastError());
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);        
        return false;
    }
    while (getline(readFile,strLineRead))
    {    	
        stringstream sstream;
        sstream << strLineRead;
        DebugPrintf(SV_LOG_INFO,"strLineRead =  %s\n",strLineRead.c_str());
        if(strLineRead.empty())
		{
			continue;
		}
		
        index = strLineRead.find_first_of(token);
        if(index != std::string::npos)
		{
            strFirstValue = strLineRead.substr(0,index);
            strSecondValue = strLineRead.substr(index+(token.length()),strLineRead.length());
            DebugPrintf(SV_LOG_INFO,"strFirstValue = %s  <==>  ",strFirstValue.c_str());
            DebugPrintf(SV_LOG_INFO,"strSecondValue = %s\n",strSecondValue.c_str());
            if ((!strFirstValue.empty()) && (!strSecondValue.empty()))
                mapOfStrings.insert(make_pair(strFirstValue,strSecondValue));
        }
        else
		{
            DebugPrintf(SV_LOG_ERROR,"Failed to find token in lineread in the file : %s\n",strFileName.c_str());
			readFile.close();
            mapOfStrings.clear();
		    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);			
			return false;
		}
    }
	readFile.close();
    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return true;
}

//Convert the map of strings to map of integers
//Input - Map of strings 
//Output - Map of integers
//Return value - true if successfull and false else
bool RollBackVM::ConvertMapofStringsToInts(map<string,string> mapOfStrings, map<int,int> &mapOfIntegers)
{
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    map<string,string>::iterator iterString;
    if (mapOfStrings.empty())
    {
        DebugPrintf(SV_LOG_ERROR,"Input Map of strings is empty. Cannot continue.\n");
        DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
        return false;
    }
    iterString = mapOfStrings.begin();
    while (iterString != mapOfStrings.end())
    {
        int iFirst,iSecond;
        DebugPrintf(SV_LOG_INFO,"iterString->first=%s  <==>  iterString->second=%s\n",iterString->first.c_str(),iterString->second.c_str());
        if(isAnInteger(iterString->first.c_str()))
            iFirst = atoi(iterString->first.c_str());
        else
        {
            DebugPrintf(SV_LOG_ERROR,"Failed to convert %s into integer(1).\n",iterString->first.c_str());
            DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
            return false;
        }
        if(isAnInteger(iterString->second.c_str()))
            iSecond = atoi(iterString->second.c_str());
        else
        {
            DebugPrintf(SV_LOG_ERROR,"Failed to convert %s into integer(2).\n",iterString->second.c_str());
            DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
            return false;
        }
        mapOfIntegers.insert(make_pair(iFirst,iSecond));
        DebugPrintf(SV_LOG_INFO,"iFirst=%d  <==>  iSecond=%d\n",iFirst,iSecond);
        iterString++;
    }
    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return true;
}

//check if a given string is a unsigned integer or not
bool RollBackVM::isAnInteger(const char *pcInt)
{
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    bool bReturnValue = true;
    DebugPrintf(SV_LOG_INFO,"Input String - %s\n",pcInt);
    for(unsigned int i=0; i<strlen(pcInt); i++)
    {
        if(!isdigit(pcInt[i]))
        {
            DebugPrintf(SV_LOG_INFO,"%c is not a digit in input string %s\n",pcInt[i],pcInt);
            bReturnValue = false;
            break;
        }
    }

    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return bReturnValue;
}

// --------------------------------------------------------------------------
// format volume name the way the CX wants it
// final format should be one of
//   <drive>
//   <drive>:\[<mntpoint>]
// where: <drive> is the drive letter and [<mntpoint>] is optional
//        and <mntpoint> is the mount point name
// e.g.
//   A for a drive letter
//   B:\mnt\data for a mount point
// --------------------------------------------------------------------------
void RollBackVM::FormatVolumeNameForCxReporting(std::string& volumeName)
{
    // we need to strip off any leading \, ., ? if they exists
    std::string::size_type idx = volumeName.find_first_not_of("\\.?");
    if (std::string::npos != idx) {
        volumeName.erase(0, idx);
    }

    // strip off trailing :\ or : if they exist
    std::string::size_type len = volumeName.length();
	//if we get the len as 0 we are not proceeding and simply returning
	//this is as per the bug 10377
	if(!len)
		return;
    idx = len;
    if ('\\' == volumeName[len - 1] || ':' == volumeName[len - 1]) {
        --idx;
    }

    if ((len > 2) && (':' == volumeName[len - 2]) )
    {
            --idx;
    }

    if (idx < len) {
        volumeName.erase(idx);
    }
}

// Restore the Disk signature of all the disks of VMs which are recovered successfully.
// Input - Vector of recovered VMs.
// If something goes against the plan return false.
bool RollBackVM::RestoreDiskSignature(const std::string& strHostName)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool rv = true;
	
	string strDataFolderPath = std::string(getVxAgentInstallPath()) + std::string("\\Failover") + std::string("\\data\\");
	int iNumberOfBytesToBeRead = 0;

	DebugPrintf(SV_LOG_DEBUG, "\nVM Name - %s\n", strHostName.c_str());

	DebugPrintf(SV_LOG_DEBUG, "Finding out the host : %s in \"m_mapOfVmNameToId\" map\n", strHostName.c_str());
	map<string, string>::iterator iterVm = m_mapOfVmNameToId.find(strHostName);
	if(iterVm != m_mapOfVmNameToId.end())
	{
		DebugPrintf(SV_LOG_DEBUG, "Finding out the host : %s in \"m_mapOfVmToSrcAndTgtDiskNmbrs\" map\n", strHostName.c_str());
		map<string, map<unsigned int, unsigned int> >::iterator iterFind = m_mapOfVmToSrcAndTgtDiskNmbrs.find(strHostName);

		map<string, DisksInfoMap_t>::iterator iterHost = m_mapOfHostToDiskInfo.find(strHostName);
		if(iterHost != m_mapOfHostToDiskInfo.end())
		{				
			if(iterFind != m_mapOfVmToSrcAndTgtDiskNmbrs.end()) //will always find VM as vecRecoveredVMs is subset of m_mapOfVmToSrcAndTgtDiskNmbrs
			{
				if (false == UpdatePartitionInfo(strHostName, iterFind->second))
				{
					DebugPrintf(SV_LOG_ERROR, "Failed to update Partition information for host : %s\n", strHostName.c_str());
					rv = false;
				}
				if (false == UpdateMbrOrGptInTgtDisk(strHostName, iterFind->second))
				{
					DebugPrintf(SV_LOG_ERROR, "Failed to update MBR or GPT for some Target disks of host : %s\n", strHostName.c_str());
					rv = false;
				}
				else
				{
					DebugPrintf(SV_LOG_DEBUG, "Successfully updated MBR or GPT for host : %s\n", strHostName.c_str());
				}
			}
			else
			{
				DebugPrintf(SV_LOG_ERROR, "Unable to get the host : %s in \"m_mapOfVmToSrcAndTgtDiskNmbrs\" map\n", strHostName.c_str());
				rv = false;
			}
		}
		else
		{
			DebugPrintf(SV_LOG_WARNING, "Specified host %s does not found in DiskInfo Map\n", strHostName.c_str());
			DebugPrintf(SV_LOG_INFO,"Considering this one was proetcted in older version and proceeding with older work flow\n");

			if(iterFind != m_mapOfVmToSrcAndTgtDiskNmbrs.end()) //will always find VM as vecRecoveredVMs is subset of m_mapOfVmToSrcAndTgtDiskNmbrs
			{
				string strVMname = iterVm->second;
				string tempfilename = strVMname + string("_Disk_");
				map<unsigned int,unsigned int>::iterator iterDiskNmbrs = iterFind->second.begin(); //first-src ; second-tgt
				//this Loop is for Each Disk Present in the VM
				for(; iterDiskNmbrs != iterFind->second.end(); iterDiskNmbrs++)
				{
					stringstream ss;
					string filename;
					string strdisknumber;
					ss << iterDiskNmbrs->first;
					strdisknumber = ss.str();
					filename = strDataFolderPath + tempfilename + strdisknumber +  MBR_FILE_NAME;
					if(checkIfFileExists(filename))
					{
						iNumberOfBytesToBeRead = 512;
					}
					else
					{
						filename = strDataFolderPath + tempfilename + strdisknumber +  GPT_FILE_NAME;
						if(checkIfFileExists(filename))
						{
							iNumberOfBytesToBeRead = 17408;
						}
						else
						{
							DebugPrintf(SV_LOG_ERROR,"Failed to find MBR/GPT file for the source disk-%u",iterDiskNmbrs->first);
							rv = false;
							continue; // continue with remaining disks. But still this will return fail in the end.
						}
					}		
					DebugPrintf(SV_LOG_DEBUG,"Target Disk number          = %u.\n",iterDiskNmbrs->second);
					DebugPrintf(SV_LOG_DEBUG,"Source MBR/GPT Filename     = %s.\n",filename.c_str());
					DebugPrintf(SV_LOG_DEBUG,"Number of bytes to be read  = %d.\n",iNumberOfBytesToBeRead);
						
					LARGE_INTEGER iLargeInt;
					iLargeInt.LowPart = 0;
					iLargeInt.HighPart = 0;
						
					if(true == UpdateDiskSignature(iterDiskNmbrs->second,iLargeInt,filename,iNumberOfBytesToBeRead))				
					{
						DebugPrintf(SV_LOG_DEBUG,"Updated disk signature of source disk-%u on target disk-%u.\n", iterDiskNmbrs->first, iterDiskNmbrs->second);				
					}	
					else
					{
						DebugPrintf(SV_LOG_ERROR,"Failed to update disk signature of source disk-%u on target disk-%u.\n", iterDiskNmbrs->first, iterDiskNmbrs->second);					
						rv = false;
						break;
					}
				}			
			}
		}
	}
	else
	{
		DebugPrintf(SV_LOG_ERROR, "Unable to get the host : %s in \"m_mapOfVmNameToId\" map\n", strHostName.c_str());
		rv = false;
	}
	
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return rv;
}

bool RollBackVM::UpdateDiskWithMbrData(const char* chTgtDisk, SV_LONGLONG iBytesToSkip, const SV_UCHAR * chDiskData, unsigned int noOfBytesToDeal = 512)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	DebugPrintf(SV_LOG_DEBUG,"Drive name is %s.\n", chTgtDisk);
	DebugPrintf(SV_LOG_DEBUG,"No. of bytes to skip : %lld, And No. bytes to write : %u\n", iBytesToSkip, noOfBytesToDeal);

	ACE_HANDLE hdl;    
    size_t BytesWrite = 0;

    hdl = ACE_OS::open(chTgtDisk,O_RDWR,ACE_DEFAULT_OPEN_PERMS);
    if(ACE_INVALID_HANDLE == hdl)
    {
		DebugPrintf(SV_LOG_ERROR,"{Error] ACE_OS::open failed: disk %s, err = %d\n ", chTgtDisk, ACE_OS::last_error());
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
        return false;
    }

    if (ACE_OS::llseek(hdl, (ACE_LOFF_T)iBytesToSkip, SEEK_SET) < 0)
    {   
        DebugPrintf(SV_LOG_ERROR,"[Error] ACE_OS::lseek failed: disk %s, offset 0, err = %d\n", chTgtDisk, ACE_OS::last_error());
        ACE_OS::close(hdl);
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
        return false;
    }

    BytesWrite = ACE_OS::write(hdl, chDiskData, noOfBytesToDeal);
	if(BytesWrite != noOfBytesToDeal)
    {
        DebugPrintf(SV_LOG_ERROR,"[Error] ACE_OS::write failed: disk %s, err = %d\n", chTgtDisk, ACE_OS::last_error());
		DebugPrintf(SV_LOG_ERROR,"Number of bytes has to be written : %ud, But written bytes are : %u\n", noOfBytesToDeal, BytesWrite);
        ACE_OS::close(hdl);
	    DebugPrintf(SV_LOG_DEBUG, "Exiting %s\n",__FUNCTION__);
        return false;
	}
    ACE_OS::close(hdl);

	DebugPrintf(SV_LOG_DEBUG,"Successfully Updated the MBR/GPT/LDM in disk %s.\n No. of bytes Written : %u\n", chTgtDisk, BytesWrite);
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return true;
}

bool RollBackVM::UpdateDiskSignature(unsigned int iDiskNumber,LARGE_INTEGER iBytesToSkip,const string strFileName,unsigned int noOfBytesToDeal = 512)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	HANDLE diskHandle;
	unsigned char *buf;
	unsigned long bytes,bytesWritten;
	char driveName [256];
	/************************************/

    size_t buflen;
    INM_SAFE_ARITHMETIC(buflen = noOfBytesToDeal * InmSafeInt<size_t>::Type(sizeof(unsigned char)), INMAGE_EX(noOfBytesToDeal)(sizeof(unsigned char)))
    buf = (unsigned char *)malloc(buflen);
    if (buf == NULL)
	{
		DebugPrintf(SV_LOG_ERROR,"Failed to allocate memory for buf.\n");
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
        return false;
	}

	FILE *fp;
	fp = fopen(strFileName.c_str(),"rb");
	if(!fp)
	{
		 DebugPrintf(SV_LOG_ERROR,"Failed to open file : %s.\n",strFileName.c_str());
         free(buf);
		 DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		 return false;
	}
    int inumberOfBytesRead = fread(buf, 1, noOfBytesToDeal, fp);

    if(noOfBytesToDeal != inumberOfBytesRead)
    {
        DebugPrintf(SV_LOG_ERROR,"Reading failed in UpdateDiskSignature().\n");
        fclose(fp);
        free(buf);
        DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
        return false;
    }
	fclose(fp);

	inm_sprintf_s(driveName, ARRAYSIZE(driveName), "\\\\.\\PhysicalDrive%d", iDiskNumber);
    DebugPrintf(SV_LOG_DEBUG,"Drive name is %s.\n",driveName);
	// PR#10815: Long Path support
	diskHandle = SVCreateFile(driveName,GENERIC_READ | GENERIC_WRITE ,FILE_SHARE_READ | FILE_SHARE_WRITE,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
	
	if(diskHandle == INVALID_HANDLE_VALUE)
	{
		DebugPrintf(SV_LOG_ERROR,"Error in opening device. Error Code : [%lu].\n",GetLastError());
        free(buf);
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;
	}
	
	// Try to move hFile file pointer some distance  
	DWORD dwPtr = SetFilePointerEx( diskHandle, 
									iBytesToSkip, 
                                    NULL, 
                                    FILE_BEGIN ); 
	 
	if(0 == dwPtr)
	{
		DebugPrintf(SV_LOG_ERROR,"SetFilePointerEx() failed in UpdateDiskSignature() with Error code : [%lu].\n",GetLastError());
		CloseHandle(diskHandle);
        free(buf);
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;
	}

    bytes = noOfBytesToDeal;	
	OVERLAPPED osWrite = {0};
	BOOL fRes;
	// Create this write operation's OVERLAPPED structure's hEvent.
	osWrite.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (osWrite.hEvent == NULL)
	{
		DebugPrintf(SV_LOG_ERROR,"CreateEvent() failed in UpdateDiskSignature() with Error code : [%lu].\n",GetLastError());
		// error creating overlapped event handle
		CloseHandle(diskHandle);
        free(buf);
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;
	}

	// Issue write.
	if (!WriteFile(diskHandle,buf,noOfBytesToDeal,&bytesWritten, NULL/*&osWrite*/))
	{
		DebugPrintf(SV_LOG_ERROR,"WriteFile() failed in UpdateDiskSignature() with Error code : [%lu].\n",GetLastError());
		fRes = FALSE;
        CloseHandle(diskHandle);
        free(buf);
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;
	}	
	else
	{
		// WriteFile completed immediately.
		fRes = TRUE;
	}	
	CloseHandle(diskHandle);
	free(buf);
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return true;
}

// Do a "chkdsk" on each volume in windows after rollback
bool RollBackVM::ChkdskVolumes(std::map<std::string,std::string> mapProtectedPairs)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    bool rv = true;

    std::map<std::string, std::string>::iterator iter = mapProtectedPairs.begin();
    for( ; iter != mapProtectedPairs.end(); iter++)
    {
        std::string chkdsk = "chkdsk /f "; 
        chkdsk += iter->second;
        system(chkdsk.c_str());
        DebugPrintf(SV_LOG_INFO,"Successfully performed chkdsk on volume %s\n",iter->second.c_str());
    }

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return rv;
}

bool RollBackVM::GetSystemDrive(std::string& systemDrive)
{
   DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
   std::string variableName = "SystemDrive";
   bool bRet = false ;
   char buf[BUFSIZ] ;
   memset(buf, '\0', BUFSIZ) ;
   if( GetEnvironmentVariable(variableName.c_str(), buf, BUFSIZ) == 0 )
   {
       DebugPrintf(SV_LOG_ERROR, "GetEnvironmentVariable failed with error %d\n", GetLastError()) ;
   }
   else
   {
        systemDrive = buf ;
        bRet = true ;
   }
   DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
   return bRet ;
}

/****************************************************************************************************
This function Will update the MBR/GPT/LDM/EBR to their corresponfing target disks for the given host.
Input : Hostname\Vmname
Input : Map of source and target disk number
*****************************************************************************************************/
bool RollBackVM::UpdateMbrOrGptInTgtDisk(string hostname, map<unsigned int, unsigned int> mapOfSrcAndTgtDiskNmbr)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);

	bool bResult = true;

	DebugPrintf(SV_LOG_DEBUG,"\n*****Going to update MBR/LDM information****\n\n");
	map<string, DisksInfoMap_t>::iterator iter = m_mapOfHostToDiskInfo.find(hostname);
	if(iter == m_mapOfHostToDiskInfo.end())
	{
		DebugPrintf(SV_LOG_ERROR,"Specified host %s does not found in DiskInfo Map\n", hostname.c_str());
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;
	}

	map<unsigned int, unsigned int>::iterator iterDisk = mapOfSrcAndTgtDiskNmbr.begin();

	for(; iterDisk != mapOfSrcAndTgtDiskNmbr.end(); iterDisk++)
	{	
		char srcDisk[256], tgtDisk[256];
		inm_sprintf_s(srcDisk, ARRAYSIZE(srcDisk), "\\\\.\\PHYSICALDRIVE%d", iterDisk->first);
		inm_sprintf_s(tgtDisk, ARRAYSIZE(tgtDisk), "\\\\.\\PHYSICALDRIVE%d", iterDisk->second);

		DebugPrintf(SV_LOG_DEBUG,"Source Disk = %s, Target Disk = %s\n", srcDisk, tgtDisk);

		DisksInfoMapIter_t iterDiskInfo = iter->second.find(string(srcDisk));
		if(iterDiskInfo == iter->second.end())
		{
			DebugPrintf(SV_LOG_ERROR,"Source disk information doesnot found for disk %s of host %s\n", srcDisk, hostname.c_str());
			bResult = false;
			continue;
		}
		
		SV_LONGLONG iBytesToSkip = 0;
		if(MBR == iterDiskInfo->second.DiskInfoSub.FormatType && BASIC == iterDiskInfo->second.DiskInfoSub.Type)
		{
			DebugPrintf(SV_LOG_DEBUG,"Updating MBR for Basic disk %s(source disk) of host %s to Target disk %s\n", srcDisk, hostname.c_str(), tgtDisk);
			if(false == UpdateDiskWithMbrData(tgtDisk, iBytesToSkip, iterDiskInfo->second.MbrSector, MBR_BOOT_SECTOR_LENGTH))
			{
				DebugPrintf(SV_LOG_ERROR,"Failed to Update MBR for basic disk %s(source disk) of host %s\n", srcDisk, hostname.c_str());
				bResult = false;
			}
		}
		else if(MBR == iterDiskInfo->second.DiskInfoSub.FormatType && DYNAMIC == iterDiskInfo->second.DiskInfoSub.Type)
		{
			SV_LONGLONG tgtDiskSize; //Need to get the exact size for updating LDM in case of P2V
			if(true == GetDiskSize(tgtDisk, tgtDiskSize))
			{
				DebugPrintf(SV_LOG_DEBUG,"Source disk : %s --> size : %lld\n", srcDisk, iterDiskInfo->second.DiskInfoSub.Size);
				DebugPrintf(SV_LOG_DEBUG,"Target disk : %s --> size : %lld\n", tgtDisk, tgtDiskSize);
			}
			else
				tgtDiskSize = iterDiskInfo->second.DiskInfoSub.Size;

			DebugPrintf(SV_LOG_DEBUG,"Updating MBR for Dynamic disk %s(source disk) of host %s to Target disk %s\n", srcDisk, hostname.c_str(), tgtDisk);

			if(false == OfflineTargetDisk(iterDisk->second))
			{
				DebugPrintf(SV_LOG_ERROR,"Failed to clean the MBR for dynamic disk %s .. Proceeding with MBR updation.\n", tgtDisk);
			}
			if(false == UpdateDiskWithMbrData(tgtDisk, iBytesToSkip, iterDiskInfo->second.MbrDynamicSector, 31744))
			{
				DebugPrintf(SV_LOG_ERROR,"Failed to Update MBR for dynamic disk %s(source disk) of host %s\n", srcDisk, hostname.c_str());
			}

			DebugPrintf(SV_LOG_DEBUG,"Updating LDM for Dynamic disk %s(source disk) of host %s to Target disk %s\n", srcDisk, hostname.c_str(), tgtDisk);
			iBytesToSkip = tgtDiskSize - LDM_DYNAMIC_BOOT_SECTOR_LENGTH;
			if(false == UpdateDiskWithMbrData(tgtDisk, iBytesToSkip,  iterDiskInfo->second.LdmDynamicSector, LDM_DYNAMIC_BOOT_SECTOR_LENGTH))
			{
				DebugPrintf(SV_LOG_ERROR,"Failed to Update LDM for dynamic disk %s(source disk) of host %s\n", srcDisk, hostname.c_str());
				DebugPrintf(SV_LOG_ERROR,"Import dynamic disk %s(source disk) Manually once the recovered machines bootsup\n", srcDisk);
				bResult = false;
			}			
		}
		else if(GPT == iterDiskInfo->second.DiskInfoSub.FormatType && BASIC == iterDiskInfo->second.DiskInfoSub.Type)
		{
			DebugPrintf(SV_LOG_DEBUG,"Updating GPT for Basic disk %s(source disk) of host %s to Target disk %s\n", srcDisk, hostname.c_str(), tgtDisk);
			if(false == UpdateDiskWithMbrData(tgtDisk, iBytesToSkip, iterDiskInfo->second.GptSector, GPT_BOOT_SECTOR_LENGTH))
			{
				DebugPrintf(SV_LOG_ERROR,"Failed to Update GPT for basic disk %s(source disk) of host %s\n", srcDisk, hostname.c_str());
				bResult = false;
			}
		}
		else if(GPT == iterDiskInfo->second.DiskInfoSub.FormatType && DYNAMIC == iterDiskInfo->second.DiskInfoSub.Type)
		{
			if(false == CleanTargetDisk(iterDisk->second))
			{
				DebugPrintf(SV_LOG_ERROR,"Failed to clean the GPT for dynamic disk %s .. Proceeding with GPT updation.\n", tgtDisk);
			}

			DebugPrintf(SV_LOG_DEBUG,"Updating GPT for Dynamic disk %s(source disk) of host %s to Target disk %s\n", srcDisk, hostname.c_str(), tgtDisk);
			if(false == UpdateDiskWithMbrData(tgtDisk, iBytesToSkip, iterDiskInfo->second.GptDynamicSector, GPT_DYNAMIC_BOOT_SECTOR_LENGTH))
			{
				DebugPrintf(SV_LOG_ERROR,"Failed to Update GPT for dynamic disk %s(source disk) of host %s\n", srcDisk, hostname.c_str());
				DebugPrintf(SV_LOG_ERROR,"Import dynamic disk %s(source disk) Manually once the recovered machines bootsup\n", srcDisk);
				bResult = false;
			}
		}
	}

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bResult;
}

/************************************************
Cleans up the disk info map 
************************************************/
void RollBackVM::CleanupDiskInfoOfAllVm()
{
	map<string, DisksInfoMap_t>::iterator iter = m_mapOfHostToDiskInfo.begin();
	for(; iter != m_mapOfHostToDiskInfo.end(); iter++)
	{
		//CleanUpDiskInfoMemory(iter->second);
	}
}


/*********************************************************
This will offline the specified disk
**********************************************************/
bool RollBackVM::OfflineTargetDisk(unsigned int nDiskNum)
{
	bool bResult = true;
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	DebugPrintf(SV_LOG_ERROR,"Offline target disk = %d \n", nDiskNum);

	string strDiskPartFile = std::string(getVxAgentInstallPath()) + std::string("\\Failover") + std::string("\\data\\") + string("inmage_offline_disks.txt");
	DebugPrintf(SV_LOG_DEBUG,"File Name - %s.\n",strDiskPartFile.c_str());

	ofstream outfile(strDiskPartFile.c_str(), ios_base::out);
	if(! outfile.is_open())
	{
		DebugPrintf(SV_LOG_ERROR,"\n Failed to open %s \n",strDiskPartFile.c_str());
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;
	}

	string strDiskNumber = boost::lexical_cast<string>(nDiskNum);
	strDiskNumber = string("select disk ") + strDiskNumber; 
	outfile << strDiskNumber << endl;
	outfile << "offline disk" << endl;
	outfile << "attribute disk clear readonly" << endl;
	//outfile << "clean" << endl;
	outfile.close();

	string strExeName = string("diskpart.exe");
	string strParameterToDiskPartExe = string (" /s ") + string("\"")+ strDiskPartFile + string("\"");

	DebugPrintf(SV_LOG_DEBUG,"Command to clean the disks: %s%s\n",strExeName.c_str(),strParameterToDiskPartExe.c_str());

	ChangeVmConfig objVm;
	if(FALSE == objVm.ExecuteProcess(strExeName,strParameterToDiskPartExe))
	{
		DebugPrintf(SV_LOG_ERROR,"Failed to clean the target disk = %d \n", nDiskNum);
		bResult = false;
	}

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bResult;
}


/*********************************************************
This will clean the configuration for the specified disk
**********************************************************/
bool RollBackVM::CleanTargetDisk(unsigned int nDiskNum)
{
	bool bResult = true;
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	DebugPrintf(SV_LOG_ERROR,"Clean target disk = %d \n", nDiskNum);

	string strDiskPartFile = std::string(getVxAgentInstallPath()) + std::string("\\Failover") + std::string("\\data\\") + string("inmage_clean_disks.txt");
	DebugPrintf(SV_LOG_DEBUG,"File Name - %s.\n",strDiskPartFile.c_str());

	ofstream outfile(strDiskPartFile.c_str(), ios_base::out);
	if(! outfile.is_open())
	{
		DebugPrintf(SV_LOG_ERROR,"\n Failed to open %s \n",strDiskPartFile.c_str());
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;
	}

	string strDiskNumber = boost::lexical_cast<string>(nDiskNum);
	strDiskNumber = string("select disk ") + strDiskNumber; 
	outfile << strDiskNumber << endl;
	//outfile << "offline disk" << endl;
	//outfile << "attribute disk clear readonly" << endl;
	outfile << "clean" << endl;
	outfile.close();

	string strExeName = string("diskpart.exe");
	string strParameterToDiskPartExe = string (" /s ") + string("\"")+ strDiskPartFile + string("\"");

	DebugPrintf(SV_LOG_DEBUG,"Command to clean the disks: %s%s\n",strExeName.c_str(),strParameterToDiskPartExe.c_str());

	ChangeVmConfig objVm;
	if(FALSE == objVm.ExecuteProcess(strExeName,strParameterToDiskPartExe))
	{
		DebugPrintf(SV_LOG_ERROR,"Failed to clean the target disk = %d \n", nDiskNum);
		bResult = false;
	}

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bResult;
}


//*************************************************************************************************
// Fetches the disk size for given disk name
//*************************************************************************************************
bool RollBackVM::GetDiskSize(const char *DiskName, SV_LONGLONG & DiskSize)
{
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);    
    bool bFlag = true;

    GET_LENGTH_INFORMATION dInfo;
    DWORD dInfoSize = sizeof(GET_LENGTH_INFORMATION);
    DWORD dwValue = 0;
    do
    {
        HANDLE hPhysicalDriveIOCTL;
        hPhysicalDriveIOCTL = CreateFile (DiskName, 
                                          GENERIC_WRITE |GENERIC_READ, 
                                          FILE_SHARE_READ | FILE_SHARE_WRITE, 
                                          NULL, OPEN_EXISTING, 
                                          FILE_ATTRIBUTE_NORMAL, NULL);

        if(hPhysicalDriveIOCTL == INVALID_HANDLE_VALUE)
        {
	        DebugPrintf(SV_LOG_ERROR,"Failed to get the disk handle for disk %s with Error code: %lu\n", DiskName, GetLastError());
	        bFlag = false;
	        break;
        }

        bFlag = DeviceIoControl(hPhysicalDriveIOCTL,
                                IOCTL_DISK_GET_LENGTH_INFO,
                                NULL, 0,	
                                &dInfo, dInfoSize,
                                &dwValue, NULL);
        if (bFlag) 
        {
            DiskSize = dInfo.Length.QuadPart;
	        CloseHandle(hPhysicalDriveIOCTL);
        }
        else 
        {
	        DebugPrintf(SV_LOG_ERROR,"IOCTL_DISK_GET_LENGTH_INFO failed for disk %s with Error code: %lu\n", DiskName, GetLastError());
	        CloseHandle(hPhysicalDriveIOCTL);
	        bFlag = false;
	        break;
        }        
        DebugPrintf(SV_LOG_DEBUG, "Disk(%s) : Size = %llu\n", DiskName, DiskSize);
    }while(0);

    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);  
    return bFlag;
}

bool RollBackVM::UpdatePartitionInfo(const string& HostName, map<unsigned int, unsigned int>& mapOfSrcAndTgtDiskNmbr)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);    
    bool bRet = true;

	DebugPrintf(SV_LOG_DEBUG,"Updating Partition information for the host : %s\n", HostName.c_str());
	string strPartitionFile = string(getVxAgentInstallPath()) + string("\\Failover") + string("\\data\\") + HostName + string("_mbrdata_partition");
	if(true == checkIfFileExists(strPartitionFile))
	{
		set<DiskName_t> setDiskName;
		map<DiskName_t, DiskName_t> mapSrcToTgtDisk;
		map<SV_UINT, SV_UINT>::iterator iterDisk = mapOfSrcAndTgtDiskNmbr.begin();
		for(; iterDisk != mapOfSrcAndTgtDiskNmbr.end(); iterDisk++)
		{
			char srcDisk[256], tgtDisk[256];
			inm_sprintf_s(srcDisk, ARRAYSIZE(srcDisk), "\\\\.\\PHYSICALDRIVE%d", iterDisk->first);
			inm_sprintf_s(tgtDisk, ARRAYSIZE(tgtDisk), "\\\\.\\PHYSICALDRIVE%d", iterDisk->second);
			mapSrcToTgtDisk.insert(make_pair(string(srcDisk), string(tgtDisk)));
		}
		DLM_ERROR_CODE retVal = RestoreDiskPartitions(strPartitionFile.c_str(), mapSrcToTgtDisk, setDiskName) ;
		if(DLM_ERR_SUCCESS == retVal)
		{
			DebugPrintf(SV_LOG_DEBUG,"Successfully updated the Partition information for the host : %s\n", HostName.c_str());
			DebugPrintf(SV_LOG_DEBUG,"Partition Information updated disks are :\n");
			set<DiskName_t>::iterator iter = setDiskName.begin();
			for(; iter != setDiskName.end(); iter++)
			{
				DebugPrintf(SV_LOG_DEBUG,"\t %s \n", iter->c_str());
				map<DiskName_t, DiskName_t>::iterator it = mapSrcToTgtDisk.find(*iter);
				OnlineOrOfflineDisk(it->second, true);		//After updating the disks partiton those will in offline state, making those online
				Sleep(2000);
			}
			std::map<std::string, std::string>::iterator iterHost = m_mapHostToType.find(HostName);
			if(iterHost != m_mapHostToType.end())
			{
				if((iterHost->second.compare("Microsoft Windows Server 2008 (64-bit)") == 0) || 
					(iterHost->second.compare("Microsoft Windows Server 2008 (32-bit)") == 0) ||
					(iterHost->second.compare("Windows_2008_32") == 0) ||
					(iterHost->second.compare("Windows_2008_64") == 0) )
				{
					DlmPartitionInfoSubMap_t mapDiskToPartition;
					retVal = ReadPartitionSubInfoFromFile(strPartitionFile.c_str(), mapDiskToPartition);
					if(DLM_ERR_SUCCESS == retVal)
					{
						DebugPrintf(SV_LOG_DEBUG,"Successfully read the Partition information for the host : %s\n", HostName.c_str());

						for(iter = setDiskName.begin(); iter != setDiskName.end(); iter++)
						{
							DlmPartitionInfoSubIterMap_t iterMap = mapDiskToPartition.find(*iter);
							map<string, string>::iterator iterDisk = mapSrcToTgtDisk.find(*iter);
							if(false == ProcessW2K8EfiPartition(iterDisk->second, iterMap->second))
							{
								DebugPrintf(SV_LOG_ERROR,"Failed to perform the W2K8 related EFI changes for host : %s \n", HostName.c_str());
								bRet = false;
							}
							else
							{
								DebugPrintf(SV_LOG_ERROR,"Successfully performed the W2K8 related EFI changes for host : %s \n", HostName.c_str());
							}
						}
					}
					else
						DebugPrintf(SV_LOG_DEBUG,"Failed to read the Partition information for the host : %s, DLM error code : %d\n", HostName.c_str(), retVal);
				}
				else
					DebugPrintf(SV_LOG_DEBUG,"Host %s is not a W2K8 machine\n", HostName.c_str());
			}
			else
				DebugPrintf(SV_LOG_DEBUG,"Host %s did not find in map of host to operatingsystemname\n", HostName.c_str());
		}
		else
		{
			DebugPrintf(SV_LOG_DEBUG,"Failed to update the Partition information for the host : %s, DLM error code : %d\n", HostName.c_str(), retVal);
			bRet = false;
		}
	}
	else
	{
		DebugPrintf(SV_LOG_DEBUG,"Partition information does not exist for the host : %s, Skipping updation\n", HostName.c_str());
	}

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);  
    return bRet;
}

bool RollBackVM::ProcessW2K8EfiPartition(const string& strTgtDisk, DlmPartitionInfoSubVec_t & vecDp)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);    
    bool bRet = true;

	set<string> stPrevVolumes, stCurVolumes;
	set<string>::iterator iter;
	list<string> lstNewVolumes;
	string strEfiVol, strEfiMountPoint;
	ChangeVmConfig obj;
	bool bEfiChange = false;

	do
	{
		DebugPrintf(SV_LOG_DEBUG,"Finding out all the volumes at Master Target\n");
		obj.EnumerateAllVolumes(stPrevVolumes);

		iter = stPrevVolumes.begin();
		DebugPrintf(SV_LOG_DEBUG,"Initially discovered Volumes are: \n");
		for(;iter != stPrevVolumes.end(); iter++)
		{
			DebugPrintf(SV_LOG_DEBUG,"\t %s \n", iter->c_str());
		}

		if(DLM_ERR_SUCCESS != ConvertEfiToNormalPartition(strTgtDisk, vecDp))
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to convert EFI to Normal partition\n");
			bRet = false;
			break;
		}
		
		Sleep(5000); //5sec sleep because disk rescan happened
		bEfiChange = true;

		DebugPrintf(SV_LOG_DEBUG,"Finding out all the volumes at Master Target after EFI to Normal partition convertion\n");
		obj.EnumerateAllVolumes(stCurVolumes);
		iter = stCurVolumes.begin();
		DebugPrintf(SV_LOG_DEBUG,"Volumes Discovered after EFI change: \n");
		for(;iter != stCurVolumes.end(); iter++)
		{
			DebugPrintf(SV_LOG_DEBUG,"\t %s \n", iter->c_str());
		}

		NewlyGeneratedVolume(stPrevVolumes, stCurVolumes, lstNewVolumes);

		DebugPrintf(SV_LOG_DEBUG,"Finding out the EFI volume\n");
		GetEfiVolume(lstNewVolumes, vecDp, strEfiVol);

		DebugPrintf(SV_LOG_DEBUG,"Mounting the EFI volume\n");
		if( FALSE == obj.MountVolume(strEfiVol, strEfiMountPoint))
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to mount the EFI partition %s\n", strEfiVol.c_str());
			bRet = false;
			break;
		}

		if(false == UpdateW2K8EfiChanges(strEfiMountPoint))
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to perform the EFI related changes for W2K8 server\n");
			bRet = false;
		}

		obj.RemoveAndDeleteMountPoints(strEfiMountPoint);

		break;
	}while(0);

	if(bEfiChange)
	{
		if(DLM_ERR_SUCCESS != ConvertNormalToEfiPartition(strTgtDisk, vecDp))
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to convert Normal To Efi partition\n");
			bRet = false;
		}
		Sleep(5000); //5sec sleep because disk rescan happened
	}

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);  
    return bRet;
}

void RollBackVM::NewlyGeneratedVolume(set<string>& stPrevVolumes, set<string>& stCurVolumes, list<string>& lstNewVolume)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);

	set<string>::iterator iterCur = stCurVolumes.begin();
	set<string>::iterator iterPrev;
	for(;iterCur != stCurVolumes.end(); iterCur++)
	{
		iterPrev = stPrevVolumes.find(*(iterCur));
		if(iterPrev == stPrevVolumes.end())
		{
			DebugPrintf(SV_LOG_DEBUG,"Newly created volume after converting EFI to normal partition %s\n", iterCur->c_str());
			lstNewVolume.push_back(*(iterCur));
		}
	}

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__); 
}

void RollBackVM::GetEfiVolume(list<string>& lstVolumes, DlmPartitionInfoSubVec_t & vecDp, string& strEfiVol)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);

	SV_ULONGLONG PartitionLength;
	DlmPartitionInfoSubIterVec_t iterVec = vecDp.begin();
	for(; iterVec != vecDp.end(); iterVec++)
	{
		if(0 == strcmp(iterVec->PartitionType, "{C12A7328-F81F-11D2-BA4B-00A0C93EC93B}"))
		{
			PartitionLength = (SV_ULONGLONG)iterVec->PartitionLength;
			DebugPrintf(SV_LOG_DEBUG,"EFI partition length : %lld \n",PartitionLength);
			break;
		}
	}

	list<string>::iterator iter = lstVolumes.begin();
	for(; iter != lstVolumes.end(); iter++)
	{
		strEfiVol = *iter;
		SV_ULONGLONG lnVolSize;
		ChangeVmConfig obj;
		if(obj.GetSizeOfVolume(*iter, lnVolSize))
		{
			DebugPrintf(SV_LOG_DEBUG,"Volume size : %lld \n",lnVolSize);
			if((PartitionLength - lnVolSize) <= 5242880)  //checking for approximate size..5MB here taken
			{
				DebugPrintf(SV_LOG_DEBUG,"Got EFI volume : %s\n", strEfiVol.c_str());
				break;
			}
		}
		else
		{
			continue;
		}
	}

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__); 
}

bool RollBackVM::UpdateW2K8EfiChanges(string & strEfiMountPoint)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool bRet = true;

	string w2k8BootEfi = strEfiMountPoint + string("EFI\\Boot") ;
	if(FALSE == CreateDirectory(w2k8BootEfi.c_str(),NULL))
	{
		if(GetLastError() != ERROR_ALREADY_EXISTS)// To Support Rerun scenario
		{
			DebugPrintf(SV_LOG_DEBUG,"Failed to create the directory : %s. ErrorCode : [%lu].\n", w2k8BootEfi.c_str(),GetLastError());
			bRet = false;
		}
		else
			DebugPrintf(SV_LOG_DEBUG,"Directory : %s already exists\n", w2k8BootEfi.c_str());
	}
	else
	{
		DebugPrintf(SV_LOG_DEBUG,"Successfully created Directory : %s\n", w2k8BootEfi.c_str());
	}

	if(bRet == true)
	{
		w2k8BootEfi = w2k8BootEfi + string("\\bootx64.efi");
		std::string w2k8SrcBootPath = strEfiMountPoint + string("EFI\\Microsoft\\Boot\\bootmgfw.EFI");
		if(false ==	SVCopyFile(w2k8SrcBootPath.c_str(),w2k8BootEfi.c_str(),false))
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to copy the file %s to %s. GetLastError - %lu\n", w2k8SrcBootPath.c_str(), w2k8BootEfi.c_str(), GetLastError());
            bRet = false;
		}
		else
            DebugPrintf(SV_LOG_DEBUG, "Copied the w2k8 efi related boot files.\n");
	}

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bRet;
}

bool RollBackVM::GetClusterDiskMapIter(std::string nodeName, std::string strSrcDiskSign, DisksInfoMapIter_t & iterDisk)
{
	bool bFound = false;
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);

	double dlmVersion = 0;
	if(m_mapOfHostIdDlmVersion.find(nodeName) != m_mapOfHostIdDlmVersion.end())
		dlmVersion = m_mapOfHostIdDlmVersion[nodeName];
	if(dlmVersion < 1.2)
	{
		DebugPrintf(SV_LOG_ERROR,"Dlm version of %s's MBR-File is not compatable",nodeName.c_str());
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return bFound;
	}

	if(strSrcDiskSign.empty())
	{
		DebugPrintf(SV_LOG_ERROR,"Signature should not be empty.\n");
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return bFound;
	}

	if(m_mapOfHostToDiskInfo.find(nodeName) != m_mapOfHostToDiskInfo.end())
	{
		DisksInfoMapIter_t iter = m_mapOfHostToDiskInfo[nodeName].begin();
		for(;iter != m_mapOfHostToDiskInfo[nodeName].end(); iter++)
		{
			if(strSrcDiskSign.compare(iter->second.DiskSignature)==0)
			{
				iterDisk = iter;
				bFound = true;
				break;
			}
		}
	}
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bFound;
}

bool RollBackVM::IsClusterNode(std::string hostName)
{
	MMIter iter = m_mapClusterNameToNodes.begin();
	while(iter != m_mapClusterNameToNodes.end())
	{
		if(hostName.compare(iter->second) == 0)
			return true;
		iter++;
	}
	return false;
}

bool RollBackVM::IsWin2k3OS(std::string hostId)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s(%s)\n",__FUNCTION__,hostId.c_str());
	bool bRet = false;
	if(m_mapOfClusNodeOS.find(hostId)!=m_mapOfClusNodeOS.end())
	{
		if( m_mapOfClusNodeOS[hostId].find("Microsoft Windows Server 2003") != string::npos ||
			m_mapOfClusNodeOS[hostId].find("Windows_2003") != string::npos )
			bRet = true;
	}
	else
	{
		DebugPrintf(SV_LOG_ERROR,"Host Id not found\n");
	}
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bRet;
}

string RollBackVM::GetNodeClusterName(std::string nodeName)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);

	string clusName;
	MMIter iter = m_mapClusterNameToNodes.begin();
	while(iter != m_mapClusterNameToNodes.end())
	{
		if(nodeName.compare(iter->second) == 0)
		{
			clusName = iter->first;
			DebugPrintf(SV_LOG_INFO,"Cluster Name %s\n",clusName.c_str());
			break;
		}
		iter++;
	}
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return clusName;
}

void RollBackVM::GetClusterNodes(std::string clusterName, std::list<string> &nodes)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	nodes.clear();
	DebugPrintf(SV_LOG_INFO,"Cluster Name: %s\n",clusterName.c_str());

	MMIter mmIter;
	pair<MMIter,MMIter> clusNodesRange = m_mapClusterNameToNodes.equal_range(clusterName);
	for(mmIter = clusNodesRange.first; mmIter != clusNodesRange.second; mmIter++)
	{
		nodes.push_back(mmIter->second);
		DebugPrintf(SV_LOG_INFO,"Node %s\n",mmIter->second.c_str());
	}
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
}
// This function is implemented to handle win2k3 cluster nodes in DR-Drill after addition of disk.
bool RollBackVM::CreateSnapshotPairForClusNode(std::string node,
											   std::map<std::string,std::string> mapProtectedPairs,
											   std::map<std::string,std::string> mapSnapshotPairs,
											   std::map<std::string,std::string>& mapProtectedToSnapshotVol)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	map<string,string> mapMissingProtPairs,
		               mapOtherClusNodePairs;
	std::map<std::string,std::string>::iterator snapshotIter;
	std::map<std::string,std::string>::iterator protectedIter = mapProtectedPairs.begin();
	for(;protectedIter != mapProtectedPairs.end(); protectedIter++)
	{
		snapshotIter = mapSnapshotPairs.find(protectedIter->first);
		if(snapshotIter != mapSnapshotPairs.end())
			mapProtectedToSnapshotVol.insert(make_pair(protectedIter->second, snapshotIter->second));
		else
		{
			DebugPrintf(SV_LOG_DEBUG,"Protected Volume %s --> corresponding snapshot Volume not found in the node %s\n", protectedIter->second.c_str(),node.c_str());
			mapMissingProtPairs[protectedIter->first] = protectedIter->second;
		}
	}
	
	ChangeVmConfig objVm;
	objVm.m_bDrDrill = m_bPhySnapShot;
	objVm.m_mapOfVmNameToId = m_mapOfVmNameToId;

	list<string> nodes;
	GetClusterNodes(GetNodeClusterName(node),nodes);
	list<string>::iterator iterNode = nodes.begin();
	for(;iterNode != nodes.end(); iterNode++)
	{
		if(0 == iterNode->compare(node))
			continue;
		map<string,string> protDrives =
		objVm.getProtectedOrSnapshotDrivesMap(*iterNode);
		mapOtherClusNodePairs.insert(protDrives.begin(),protDrives.end());
		//TODO: Instead of considering all the volumes of a cluster, consider only the cluster volumes
	}

	protectedIter = mapMissingProtPairs.begin();
	for(; protectedIter != mapMissingProtPairs.end(); protectedIter++)
	{
		snapshotIter = mapOtherClusNodePairs.find(protectedIter->first);
		if(snapshotIter != mapOtherClusNodePairs.end())
		{
			DebugPrintf(SV_LOG_DEBUG,"Protected Volume %s --> Missing snapshot Volume found\n", protectedIter->second.c_str());
			mapProtectedToSnapshotVol.insert(make_pair(protectedIter->second, snapshotIter->second));
		}
		else
		{
			DebugPrintf(SV_LOG_DEBUG,"Protected Volume %s --> correcponding snapshot Volume not found\n", protectedIter->second.c_str());
			return false;
		}
	}
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return true;
}

bool RollBackVM::RestoreTargetDisks(string& strHostName)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool bRetValue = true;
	ChangeVmConfig obj;

	do
	{
		string strDlmFile;
		if(!DownloadDlmFile(strHostName, strDlmFile))
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to download the file : %s for host : %s.\n", strDlmFile.c_str(), strHostName.c_str());
			bRetValue = false; break;
		}
		
		DisksInfoMap_t srcDisksInfo;
		if(DLM_ERR_SUCCESS != ReadDiskInfoMapFromFile(strDlmFile.c_str(), srcDisksInfo))
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to get source disks information from file : %s.\n", strDlmFile.c_str());
			bRetValue = false; break;
		}
		m_mapOfHostToDiskInfo[strHostName] = srcDisksInfo;

		double dlmVersion = 0;
		if(DLM_ERR_SUCCESS != GetDlmVersion(strDlmFile.c_str(), dlmVersion))
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to get the dlm version.");
		}
		m_mapOfHostIdDlmVersion[strHostName] = dlmVersion;

		//Creats the map of source to target disk number. also creates map of source disk num to target disk signature.
		if(!createMapOfVmToSrcAndTgtDiskNmbrs(strHostName))
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to create source and target disks mapping information for host : %s.\n", strHostName.c_str());
			bRetValue = false; break;
		}

		//Prepare the target disks for dumping the source information, operations like online, clean, clear reaonly get performed.
		map< string, map<string, string> >::iterator iter = m_mapOfVmToSrcAndTgtDisks.find(strHostName);
		if(!obj.PrepareDisksForConfiguration(iter->second, srcDisksInfo))
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to prepare the target disks...\n");
			bRetValue = false; break;
		}

		//Preparing the dynamic GPT disks(Need soem extra workaround for the dynamic gpt disks)
		if(!obj.PrepareDynamicGptDisks(iter->second, srcDisksInfo))
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to prepare the target dynamic gpt disks...\n");
			bRetValue = false; break;
		}

		//Configure the target disks same as source disk config
		map< string, map<string, string> >::iterator iterSig = m_mapOfVmToSrcAndTgtDiskSig.find(strHostName);
		if(!obj.ConfigureTargetDisks(iter->second, srcDisksInfo, iterSig->second))
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to Configure the Target disks same as source.\n");
			bRetValue = false; break;
		}

		Sleep(10000);
		obj.rescanIoBuses();  //Scans the disk management

		try
		{
			CVdsHelper objVds;
			if(objVds.InitilizeVDS())
				objVds.ClearFlagsOfVolume();
			objVds.unInitialize();
		}
		catch(...)
		{
			DebugPrintf(SV_LOG_ERROR,"Failed while trying to clear the hidden flags of volumes.\n\n");
		}

		//Getting Current system (MT) disk information
		DisksInfoMap_t tgtDiskInfo;
		if(DLM_ERR_SUCCESS != CollectDisksInfoFromSystem(tgtDiskInfo))
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to collect the disks info from the system.\n");
			bRetValue = false; break;
		}

		//Preparing disk mapping with "PhysicalDrive prefix
		map<string, string> mapSrcToTgtDiskName;
		map<string, string>::iterator iterDisk;
		for(iterDisk = iter->second.begin(); iterDisk != iter->second.end(); iterDisk++)
		{
			mapSrcToTgtDiskName.insert(make_pair("\\\\.\\PHYSICALDRIVE" + iterDisk->first, "\\\\.\\PHYSICALDRIVE" + iterDisk->second));
		}

		//Finding out volume mapping
		map<string, string> mapProtectedPairs = obj.getProtectedOrSnapshotDrivesMap(strHostName, true);
		if(!mapProtectedPairs.empty())
		{
			//Restoring the mount points.
			if (DLM_ERR_INCOMPLETE == RestoreVConVolumeMountPoints(srcDisksInfo, tgtDiskInfo, mapSrcToTgtDiskName, mapProtectedPairs)) 
			{
				DebugPrintf(SV_LOG_ERROR,"Failed while restoring the mount points for host [%s] ..Retry again after validating the disks\n", strHostName.c_str());
				bRetValue = false; break;
			}
			else
				DebugPrintf(SV_LOG_INFO,"Successfully restored the mount points for host [%s]...\n", strHostName.c_str());
		}
		else
		{
			DebugPrintf(SV_LOG_ERROR,"PINFO file does not exist for host [%s] .. Cannot assign the mount points.\n", strHostName.c_str());
			bRetValue = false; break;
		}

		//Register-Host to update the newly created volumes to CX.
		obj.RegisterHostUsingCdpCli();
	}while(0);

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bRetValue;
}

bool RollBackVM::DownloadDlmFile(string strHostName, string& strDlmFile)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool bRetValue = true;
	strDlmFile = getVxAgentInstallPath() + "\\Failover\\Data\\" + strHostName + "_mbrdata";

	do
	{
		if(checkIfFileExists(strDlmFile))
		{
			DebugPrintf(SV_LOG_INFO,"File [%s] already exists. Using it for further operations.\n", strDlmFile.c_str());
			break;
		}

		string strGetHostInfoXmlFile;				
		if(false == m_objLin.GetHostInfoFromCx(strHostName, strGetHostInfoXmlFile))
		{
			bRetValue = false; 
		    break;
		}

		string strDlmFileCxPath;
		if(!m_objLin.ReadDlmFileFromCxApiXml(strGetHostInfoXmlFile, strDlmFileCxPath))
		{
			bRetValue = false; 
			break;
		}

		if(!m_objLin.DownloadFileFromCx(strDlmFileCxPath, strDlmFile))
		{
			DebugPrintf(SV_LOG_ERROR, "Failed to download the %s file from CX.\n", strDlmFileCxPath.c_str());
			bRetValue = false; break;
		}
	}while(0);

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bRetValue;
}

bool RollBackVM::ConfigureMountPoints(std::string hostInfoFile)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool bRet = true;

	ChangeVmConfig objChVm;
	set<string> hostList;
	map<string, string> mapTgtDiskSigToNum;

	do
	{
		DebugPrintf(SV_LOG_INFO,"Getting Host information File %s.\n",hostInfoFile.c_str());	
		ifstream FIN(hostInfoFile.c_str(), std::ios::in);
		if(! FIN.is_open())
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to open the file - %s\n",hostInfoFile.c_str());
			bRet = false;
			break;
		}

		string strLineRead;
		while(getline(FIN, strLineRead))
		{
			if(strLineRead.empty())
				continue;

			DebugPrintf(SV_LOG_INFO,"\t%s\n",strLineRead.c_str());
			hostList.insert(strLineRead);
		}
		FIN.close();


		//Getting MT disk information
		DisksInfoMap_t TgtMapDiskNamesToDiskInfo;
		if(DLM_ERR_SUCCESS != CollectDisksInfoFromSystem(TgtMapDiskNamesToDiskInfo))
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to collect the disks info from the system.\n");
			bRet = false;
			break;
		}

		set<string>::iterator iterHost;	

		try
		{
			//Get all target disk signatures
			CVdsHelper objVds;
			if(objVds.InitilizeVDS())
			{
				DebugPrintf(SV_LOG_INFO,"Finding out the disk signatures of all the target disks.\n");
				objVds.GetDisksSignature(mapTgtDiskSigToNum);
			}
			objVds.unInitialize();
		}
		catch(...)
		{
			DebugPrintf(SV_LOG_ERROR,"Failed while trying to get disk signatures.\n\n");
		}
		DebugPrintf(SV_LOG_INFO,"\nDiscovered disk signatures at MT are :\n");
		map<string, string>::iterator iDiskSig = mapTgtDiskSigToNum.begin();
		for(; iDiskSig != mapTgtDiskSigToNum.end(); iDiskSig++)
		{
			DebugPrintf(SV_LOG_INFO,"\nDisk Num : %s \tDisk Signature : %s\n", iDiskSig->second.c_str(), iDiskSig->first.c_str());
		}

		//Get all Source disk metadata information from MBR files
		for(iterHost = hostList.begin(); iterHost != hostList.end(); iterHost++)
		{
			string strMbrFile = string(getVxAgentInstallPath()) + string("\\Failover") + string("\\data\\") + (*iterHost) + string("_mbrdata");
			if(true == checkIfFileExists(strMbrFile))
			{
				DisksInfoMap_t srcMapOfDiskNamesToDiskInfo;
				DLM_ERROR_CODE RetVal = ReadDiskInfoMapFromFile(strMbrFile.c_str(), srcMapOfDiskNamesToDiskInfo);
				if(DLM_ERR_SUCCESS != RetVal)
				{
					DebugPrintf(SV_LOG_ERROR,"Failed to collect the Source disks info map from the BLOB file(%s). Error Code - %u\n", strMbrFile.c_str(), RetVal);
					continue;
				}

				//Finding Out disk mappings
				map<string, string> mapSrcToTgtDiskName;
				string strDiskInfoFile = std::string(getVxAgentInstallPath()) + std::string("\\Failover") + std::string("\\data\\") + (*iterHost) + string(".dpinfo");
				if(true == checkIfFileExists(strDiskInfoFile))
				{
					map<string, dInfo_t> mapTgtScsiIdToSrcDiskNum;
					if((false == GetMapOfSrcDiskToTgtDiskNumFromDinfo(*iterHost, strDiskInfoFile, mapTgtScsiIdToSrcDiskNum)) && !mapTgtScsiIdToSrcDiskNum.empty())
					{
						DebugPrintf(SV_LOG_ERROR,"Failed to get the map of source to target disk numbers for Host [%s].. Cannot assign the mount points.\n", iterHost->c_str());
						continue;
					}

					map<string, dInfo_t>::iterator iterDisk = mapTgtScsiIdToSrcDiskNum.begin();
					for(; iterDisk != mapTgtScsiIdToSrcDiskNum.end(); iterDisk++)
					{
						if(mapTgtDiskSigToNum.find(iterDisk->first) != mapTgtDiskSigToNum.end())
						{
							mapSrcToTgtDiskName.insert(make_pair(string("\\\\.\\PHYSICALDRIVE") + iterDisk->second.DiskNum, string("\\\\.\\PHYSICALDRIVE") + mapTgtDiskSigToNum.find(iterDisk->first)->second));
						}
					}
				}
				else
				{
					DebugPrintf(SV_LOG_ERROR,"DPINFO file does not exist for host [%s] .. Cannot assign the mount points.\n", iterHost->c_str());
					continue;
				}

				//Finding out colume mappings
				map<string, string> mapProtectedPairs = objChVm.getProtectedOrSnapshotDrivesMap(*iterHost, true);
				if(!mapProtectedPairs.empty())
				{
					if (DLM_ERR_INCOMPLETE == RestoreVConVolumeMountPoints(srcMapOfDiskNamesToDiskInfo, TgtMapDiskNamesToDiskInfo, mapSrcToTgtDiskName, mapProtectedPairs)) 
					{
						DebugPrintf(SV_LOG_ERROR,"Failed while restoring the mount points for host [%s] ..Retry again after validating the disks\n", iterHost->c_str());
						break;
					}
					else
						DebugPrintf(SV_LOG_INFO,"Successfully restored the mount points for host [%s]...\n", iterHost->c_str());
				}
				else
					DebugPrintf(SV_LOG_ERROR,"PINFO file does not exist for host [%s] .. Cannot assign the mount points.\n", iterHost->c_str());
			}
			else
				DebugPrintf(SV_LOG_ERROR,"MBR file does not exist for host [%s] .. Cannot assign the mount points.\n", iterHost->c_str());

		}
	}while(0);

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bRet;
}


bool RollBackVM::AssignMountPointToTgtVols(const std::string& strHostName)
{
	DebugPrintf(SV_LOG_DEBUG, "Entering %s\n", __FUNCTION__);
	bool bRet = true;

	do
	{
		std::string strVolInfoFile;
		std::map<std::string, std::string> mapTgtVolGuidToName;
		//Read target volume map file.
		strVolInfoFile = m_strDatafolderPath + "\\" + strHostName + TGT_VOLUMESINFO;

		if (!GetMappingFromFile(strVolInfoFile, mapTgtVolGuidToName) || mapTgtVolGuidToName.empty())
		{
			DebugPrintf(SV_LOG_WARNING, "Failed to Target volume GUID to mount point mapping for the VM - %s\n", strHostName.c_str());
			bRet = false; break;
		}

		std::map<std::string, std::string>::iterator iter;

		for (iter = mapTgtVolGuidToName.begin(); iter != mapTgtVolGuidToName.end(); iter++)
		{
			std::string strMountPoint = "";
			//Verify mount point existance for each volume
			char szVolumePath[MAX_PATH + 1] = { 0 };
			char *pszVolumePath = NULL;
			DWORD dwVolPathSize = 0;
			if (GetVolumePathNamesForVolumeName(iter->first.c_str(), szVolumePath, MAX_PATH, &dwVolPathSize))
			{
				strMountPoint = std::string(szVolumePath);
			}
			else
			{
				if (GetLastError() == ERROR_MORE_DATA)
				{
					pszVolumePath = new char[dwVolPathSize];
				}
				if (!pszVolumePath)
				{
					DebugPrintf(SV_LOG_ERROR, "\n Failed to allocate required memory to get the list of Mount Point Paths.");
				}
				else
				{
					//Call GetVolumePathNamesForVolumeName function again
					if (GetVolumePathNamesForVolumeName(iter->first.c_str(), pszVolumePath, dwVolPathSize, &dwVolPathSize))
					{
						strMountPoint = *pszVolumePath;
					}
				}
			}

			DebugPrintf(SV_LOG_DEBUG, "Found mount point [%s] for volume GUID [%s].", strMountPoint.c_str(), iter->first.c_str());

			//If mount point doesnot exist then assign mount point.
			if (strMountPoint.empty())
			{
				if (0 == SetVolumeMountPoint(iter->second.c_str(), iter->first.c_str()))
				{
					if (GetLastError() == ERROR_DIR_NOT_EMPTY)
					{
						//Currently ignoring this case.. need to check in future
						DebugPrintf(SV_LOG_ERROR, "Directory already mounted : [%s].\n", iter->second.c_str());
					}
					else
					{
						DebugPrintf(SV_LOG_ERROR, "Failed to set mount point for volume guid %s at %s with Error Code : [%lu].\n", iter->first.c_str(), iter->second.c_str(), GetLastError());
						DebugPrintf(SV_LOG_DEBUG, "Exiting %s\n", __FUNCTION__);
						return false;
					}
				}
			}
		}
	} while (0);

	DebugPrintf(SV_LOG_DEBUG, "Exiting %s\n", __FUNCTION__);
	return bRet;
}

//Prepares the map of two entries which are differ by "!@!@!" in a file
bool RollBackVM::GetMappingFromFile(std::string& strFile, std::map<std::string, std::string>& strMap)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool bRet = true;
	std::string strLineRead;
	std::string token("!@!@!");
	
	if (false == checkIfFileExists(strFile))
	{
		DebugPrintf(SV_LOG_ERROR, "File not found - %s\n", strFile.c_str());
		DebugPrintf(SV_LOG_DEBUG, "Exiting %s\n", __FUNCTION__);
		return false;
	}

	std::ifstream inFile(strFile.c_str());
	if(!inFile.is_open())
	{
		DebugPrintf(SV_LOG_ERROR, "Failed to read the file - %s\n", strFile.c_str());
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;
	}
	while(getline(inFile,strLineRead))
	{
		std::string strFirstEntry;
		std::string strSecondEntry;
		std::size_t index;		
		DebugPrintf(SV_LOG_DEBUG,"Line read =  %s \n",strLineRead.c_str());
		index = strLineRead.find_first_of(token);
		if(index != std::string::npos)
		{
			strFirstEntry = strLineRead.substr(0, index);
			strSecondEntry = strLineRead.substr(index + (token.length()), strLineRead.length());
			DebugPrintf(SV_LOG_DEBUG, "First Value =  %s  :  Second Value =  %s\n", strFirstEntry.c_str(), strSecondEntry.c_str());
			if (!strFirstEntry.empty())
				strMap.insert(make_pair(strFirstEntry, strSecondEntry));
		}
		else
		{
			DebugPrintf(SV_LOG_ERROR, "Failed to find token in line read. Data Format is incorrect in file - %s.\n", strFile.c_str());
			bRet = false;
			break;
		}
	}
	inFile.close();
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);	
	return bRet;
}

#endif

#ifndef WIN32
bool RollBackVM::createMapOfVmToSrcAndTgtDisk(string& strErrorMsg)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    bool rv = true;
	
	do
    {
        map<string, pair<string,string> >::iterator i = rollbackDetails.begin();
        DebugPrintf(SV_LOG_INFO,"\nFetching the Map of VMs to Source and Target Disks\n");
        for(; i != rollbackDetails.end(); i++)
        {
            string hostName = i->first;
            DebugPrintf(SV_LOG_INFO,"\nVM : %s\n----------------------\n", hostName.c_str());

			m_objLin.m_PlaceHolder.clear();
			string strSrcHostName = "";
			map<string, string>::iterator iterH = m_mapOfVmNameToId.find(hostName);
			if (iterH != m_mapOfVmNameToId.end())
				strSrcHostName = iterH->second;
			m_objLin.m_PlaceHolder.Properties[SOURCE_HOSTNAME] = strSrcHostName;
			m_objLin.m_PlaceHolder.Properties[MT_HOSTNAME] = m_strHostName;

            map<string, string> mapSrcToTgtDisks;
			if(m_bCloudMT)
			{
				std::map<std::string,std::map<std::string,std::string> >::iterator iter; 
				if(m_bPhySnapShot)
				{
					iter = m_SnapshotVmsInfo.m_snapshotDisksVolsInfo.m_volumeMapping.find(hostName);
					if(iter != m_SnapshotVmsInfo.m_snapshotDisksVolsInfo.m_volumeMapping.end())
						mapSrcToTgtDisks = iter->second;
				}
				else
				{
					iter = m_RecVmsInfo.m_volumeMapping.find(hostName);
					if(iter != m_RecVmsInfo.m_volumeMapping.end())
						mapSrcToTgtDisks = iter->second;
				}
			}
			else
			{
				if(false == generateDisksMap(hostName, mapSrcToTgtDisks))
				{
					DebugPrintf(SV_LOG_ERROR,"Failed to get the map of source to target disks for VM - %s\n", hostName.c_str());
					strErrorMsg = "Master Target " + m_strHostName + " is unable to identify the target disks corresponding to the source machine " + strSrcHostName;
					rv = false; break;
				}
			}
			m_mapOfVmToSrcAndTgtDisks.insert(make_pair(hostName,mapSrcToTgtDisks));
        }
        if(false == rv)
            break;

		if(m_mapOfVmToSrcAndTgtDisks.empty())
		{
			DebugPrintf(SV_LOG_ERROR,"Map Of VMname to Source and Target Disks is empty.\n");
			rv = false;
		}
    }while(0);

    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return rv;
}

//Will get it from pinfo(if rollback) or sinfo(is snapshot) file
bool RollBackVM::generateDisksMap(string hostName, map<string, string>& mapSrcToTgtDisks)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    bool rv = true;
	
	string strPinfoFile;
	if(m_bPhySnapShot)
		strPinfoFile = string(getVxAgentInstallPath()) + "failover_data/" + hostName + SNAPSHOTINFO_EXTENSION;
	else
		strPinfoFile = string(getVxAgentInstallPath()) + "failover_data/" + hostName + PAIRINFO_EXTENSION;
	DebugPrintf(SV_LOG_INFO,"Reading the Pinfo File: %s\n",strPinfoFile.c_str());
	
	if(false == checkIfFileExists(strPinfoFile))
	{
		DebugPrintf(SV_LOG_ERROR,"File Not Found: %s\n",strPinfoFile.c_str());
		std::map<std::string, std::string>::iterator iter = m_mapOfVmNameToId.find(hostName);
		if(iter != m_mapOfVmNameToId.end())
			hostName = iter->second;
		if(m_bPhySnapShot)
			strPinfoFile = string(getVxAgentInstallPath()) + "failover_data/" + hostName + SNAPSHOTINFO_EXTENSION;
		else
			strPinfoFile = string(getVxAgentInstallPath()) + "failover_data/" + hostName + PAIRINFO_EXTENSION;
	}
	else
	{
		ifstream inFile(strPinfoFile.c_str());
		if(!inFile.is_open())
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to open file %s\n ",strPinfoFile.c_str());
			DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
			return false;
		}
		string strLineRead;
		string token = "!@!@!";
		while(getline(inFile,strLineRead))
		{
			std::string strSrcVolName;
			std::string strTgtVolName;
			std::size_t index;		
			DebugPrintf(SV_LOG_INFO,"Line read =  %s \n",strLineRead.c_str());
			index = strLineRead.find_first_of(token);
			if(index != std::string::npos)
			{
				strSrcVolName = strLineRead.substr(0,index);
				strTgtVolName = strLineRead.substr(index+(token.length()),strLineRead.length());
				DebugPrintf(SV_LOG_INFO,"Source Disk =  %s  :  Target disk =  %s\n",strSrcVolName.c_str(),strTgtVolName.c_str());            
				if((!strSrcVolName.empty()) && (!strTgtVolName.empty()))
				{
					mapSrcToTgtDisks.insert(make_pair(strSrcVolName,strTgtVolName));
				}
			}
		}		
		inFile.close();
	}
	
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return rv;
}


void RollBackVM::findLscsiDevice(const string& hostName, list<string>& lstLsscsiId)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);

	map<string, map<string, string> >::iterator iter;
	if((iter = m_mapOfVmToSrcAndTgtDisks.find(hostName)) != m_mapOfVmToSrcAndTgtDisks.end())
	{
		map<string,string>::iterator mapIter = iter->second.begin();
		for(;mapIter != iter->second.end(); mapIter++)
		{
			string strLsscsiId;
			getLsscsiId(mapIter->second, strLsscsiId);
			lstLsscsiId.push_back(strLsscsiId);
		}
	}

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
}

bool RollBackVM::getStandardDevice(const string& strMultipathDev, string& strStandardDevice)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool bResult = true;
	stringstream results;
	
	string cmd = string("multipath -l ") + strMultipathDev + string("|awk 'END { print $3}' 2>>") + m_InMageCmdLogFile;
	DebugPrintf(SV_LOG_DEBUG,"cmd = : %s\n", cmd.c_str());
	
	string cmd1 = string("echo running command : \"") + cmd + string("\" >>") + m_InMageCmdLogFile;
	if (!executePipe(cmd1, results))
	{
		DebugPrintf(SV_LOG_ERROR, "Failed to run the command to display the command on file\n");
	}
	
	results.clear();

	stringstream results1;
	if (!executePipe(cmd, results1))
	{
		DebugPrintf(SV_LOG_ERROR, "Failed to run the command: %s\n",cmd.c_str());
		bResult = false;
	}
	else
	{
		strStandardDevice = results1.str();
		DebugPrintf(SV_LOG_DEBUG, "Successfully ran the cmd: %s \n", cmd.c_str());
		DebugPrintf(SV_LOG_DEBUG, "Got standard device: %s \n", strStandardDevice.c_str());
		bResult = true;
	}

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bResult;
}

bool RollBackVM::getLsscsiId(const string& strMultipathDev, string& strLsscsiId)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool bResult = true;
	stringstream results;
	
	string cmd = string("multipath -l ") + strMultipathDev + string("|awk 'END { print $2}' 2>>") + m_InMageCmdLogFile;
	DebugPrintf(SV_LOG_DEBUG,"cmd = : %s\n", cmd.c_str());
	
	string cmd1 = string("echo running command : \"") + cmd + string("\" >>") + m_InMageCmdLogFile;
	if (!executePipe(cmd1, results))
	{
		DebugPrintf(SV_LOG_ERROR, "Failed to run the command to display the command on file\n");
	}
	
	results.clear();

	stringstream results1;
	if (!executePipe(cmd, results1))
	{
		DebugPrintf(SV_LOG_ERROR, "Failed to run the command: %s\n",cmd.c_str());
		bResult = false;
	}
	else
	{
		strLsscsiId = results1.str();
		DebugPrintf(SV_LOG_DEBUG, "Successfully ran the cmd: %s \n", cmd.c_str());
		DebugPrintf(SV_LOG_DEBUG, "Got scsi id: %s \n", strLsscsiId.c_str());
		bResult = true;
	}

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bResult;
}


void RollBackVM::collectMessagesLog()
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);

	stringstream results;
	string cmd = "cp /var/log/messages " + m_strDatafolderPath;
	DebugPrintf(SV_LOG_DEBUG,"cmd = : %s\n", cmd.c_str());
	if (!executePipe(cmd, results))
	{
		DebugPrintf(SV_LOG_ERROR, "Failed to run the command to copy the file /var/log/messages to the directory: %s\n", m_strDatafolderPath.c_str());
    }
	else
	{
		DebugPrintf(SV_LOG_DEBUG, "Successfully ran the command\n");
    }

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
}

void RollBackVM::collectDmsegLog()
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);

	stringstream results;
	string cmd = string("dmesg 1>>") + m_InMageCmdLogFile + string(" 2>>") + m_InMageCmdLogFile;
	DebugPrintf(SV_LOG_DEBUG,"cmd = : %s\n", cmd.c_str());
	if (!executePipe(cmd, results))
	{
		DebugPrintf(SV_LOG_ERROR, "Failed to run the command\n");
    }
	else
	{
		DebugPrintf(SV_LOG_DEBUG, "Successfully ran the command\n");
    }

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
}


void RollBackVM::removeLsscsiDevice(list<string>& lstLsscsiId)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);

	LinuxReplication linObj;

	list<string>::iterator iter = lstLsscsiId.begin();
	for(; iter != lstLsscsiId.end(); iter++)
	{
		linObj.removeLsscsiId(*iter);
	}

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
}


void RollBackVM::removeLogicalVolumes(std::set<std::string>& lvs)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	stringstream results;
	set<string>::iterator iter = lvs.begin();
	string cmd;
	for(; iter != lvs.end(); iter++)
	{
		cmd = string("dmsetup remove -f ") + string(*iter) + string(" 1>>") + m_InMageCmdLogFile + string(" 2>>") + m_InMageCmdLogFile;
		DebugPrintf(SV_LOG_DEBUG,"cmd = : %s\n", cmd.c_str());
		results.clear();
		if (!executePipe(cmd, results))
		{
			DebugPrintf(SV_LOG_ERROR, "Failed to run the command: %s\n",cmd.c_str());
		}
		else
		{
			DebugPrintf(SV_LOG_DEBUG, "Successfully ran the cmd: %s\n", cmd.c_str());
		}
	}
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
}



void RollBackVM::removeMultipathDevices(const string& hostName)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);

	ChangeVmConfig vmObj;
	vmObj.m_InMageCmdLogFile = m_InMageCmdLogFile;
	map<string, map<string, string> >::iterator iter;
	if((iter = m_mapOfVmToSrcAndTgtDisks.find(hostName)) != m_mapOfVmToSrcAndTgtDisks.end())
	{
		map<string,string>::iterator mapIter = iter->second.begin();
		for(;mapIter != iter->second.end(); mapIter++)
		{
			set<string> setPartitions;
			vmObj.FindAllPartitions(mapIter->second, setPartitions, true);
			if(setPartitions.find(mapIter->second) != setPartitions.end())
			{
				setPartitions.erase(setPartitions.find(mapIter->second));
			}
			removeMultipath(mapIter->second, setPartitions);
		}
	}

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
}


bool RollBackVM::removeMultipath(const string& strMultipathDev,  set<string>& setPartitions)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool bResult = true;
	stringstream results;

	set<string>::iterator iter = setPartitions.begin();

	string cmd;
	for(; iter != setPartitions.end(); iter++)
	{
		cmd = string("dmsetup remove -f ") + string(*iter) + string(" 1>>") + m_InMageCmdLogFile + string(" 2>>") + m_InMageCmdLogFile;
		DebugPrintf(SV_LOG_DEBUG,"cmd = : %s\n", cmd.c_str());
		results.clear();
		if (!executePipe(cmd, results))
		{
			DebugPrintf(SV_LOG_ERROR, "Failed to run the command: %s\n",cmd.c_str());
		}
		else
		{
			DebugPrintf(SV_LOG_DEBUG, "Successfully ran the cmd: %s\n", cmd.c_str());
		}
	}

	cmd = string("dmsetup remove -f ") + strMultipathDev + string(" 1>>") + m_InMageCmdLogFile + string(" 2>>") + m_InMageCmdLogFile;
	DebugPrintf(SV_LOG_DEBUG,"cmd = : %s\n", cmd.c_str());
	results.clear();
	if (!executePipe(cmd, results))
	{
		DebugPrintf(SV_LOG_ERROR, "Failed to run the command: %s\n",cmd.c_str());
	}
	else
	{
		DebugPrintf(SV_LOG_DEBUG, "Successfully ran the cmd: %s\n", cmd.c_str());
	}

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bResult;
}


void RollBackVM::runFlushBufAndKpartx(const string& hostName)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);

	map<string, map<string, string> >::iterator iter;
	if((iter = m_mapOfVmToSrcAndTgtDisks.find(hostName)) != m_mapOfVmToSrcAndTgtDisks.end())
	{
		map<string,string>::iterator mapIter = iter->second.begin();
		for(;mapIter != iter->second.end(); mapIter++)
		{
			blockDevFlushBuffer(mapIter->second);

			//Added because flush buf is not done for standard devices. Oracle db not came online because of this.
			if(!m_bV2P && (m_strCloudEnv.compare("aws") != 0))
			{
				string strStandardDevice;
				getStandardDevice(mapIter->second, strStandardDevice);
				if(strStandardDevice.find("/dev") == string::npos)
					strStandardDevice = string("/dev/") + strStandardDevice;
				blockDevFlushBuffer(strStandardDevice);
			}
		}

		if(!m_bV2P && (m_strCloudEnv.compare("aws") != 0))
		{
			for(mapIter = iter->second.begin();mapIter != iter->second.end(); mapIter++)
			{
				ShowPartitionsMultipath(mapIter->second);
			}
		}
		else
		{
			for(mapIter = iter->second.begin();mapIter != iter->second.end(); mapIter++)
			{
				rescanDisk(mapIter->second);
			}
		}
	}

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
}


bool RollBackVM::editLVMconfFile(string hostName)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    bool rv = true;
	
	map<string, map<string, string> >::iterator iter;
	list<string> includeDisksFromLVM;
	if((iter = m_mapOfVmToSrcAndTgtDisks.find(hostName)) != m_mapOfVmToSrcAndTgtDisks.end())
	{
		map<string,string>::iterator mapIter = iter->second.begin();
		for(;mapIter != iter->second.end(); mapIter++)
		{
			DebugPrintf(SV_LOG_DEBUG,"Removing Disk From Exclude list: %s\n",mapIter->second.c_str());
			rescanDisk(mapIter->second);
			includeDisksFromLVM.push_back(mapIter->second);
		}
	}
	
	string strLvmFile = "/etc/lvm/lvm.conf";
	string strLvmFileTemp = "/etc/lvm/lvm.conf.temp";
	if(false == checkIfFileExists(strLvmFile))
	{
		DebugPrintf(SV_LOG_ERROR,"File not found: %s\n",strLvmFile.c_str());
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;
	}
	ifstream inFile(strLvmFile.c_str());
	ofstream outFile(strLvmFileTemp.c_str());
	if(!inFile.is_open())
	{
		DebugPrintf(SV_LOG_ERROR,"Failed to open file %s\n ",strLvmFile.c_str());
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;
	}

	string strLineRead;
	bool bDeviceUpdated = false;
	while(std::getline(inFile,strLineRead))
	{
		outFile << strLineRead << endl;
		if((strLineRead == "devices {") || (strLineRead == "devices"))
		{
			if(strLineRead == "devices")
			{
				std::getline(inFile,strLineRead);
				outFile << strLineRead << endl;
				if(strLineRead != "{")
				{
					continue;
				}
			}
			while(std::getline(inFile,strLineRead))
			{
				if(strLineRead.find("scan = [") != string::npos)
				{
					std::getline(inFile,strLineRead);
					strLineRead = string("    scan = [ \"/dev\", \"/dev/mapper\" ]");
					outFile << strLineRead << endl;
					continue;
				}
				outFile << strLineRead << endl;
				if((strLineRead.find("# filter = [ ") != string::npos) && !bDeviceUpdated)
				{
					if(!includeDisksFromLVM.empty())
					{
						std::getline(inFile,strLineRead);
						string strIncludeList = "    filter = [ \"a|^/dev/sda$|\",";
						list<string>::iterator listIter = includeDisksFromLVM.begin();
						while(listIter != includeDisksFromLVM.end())
						{
							strIncludeList = strIncludeList + " \"a|^" + (*listIter) + "|\"," ;
							listIter++;
						}
						strIncludeList = strIncludeList + " \"r|.*|\" ]";
						DebugPrintf(SV_LOG_DEBUG,"include Volumes entry in lvm.conf file are: %s\n", strIncludeList.c_str());
						outFile << strIncludeList << endl;
					}
					else
					{
						DebugPrintf(SV_LOG_DEBUG,"There are no volumes to include in lvm.conf file\n");
					}
					bDeviceUpdated = true;
				}
			}
		}
	}
	
	inFile.close();
	outFile.close();
	int result;
	string hostLvmFile;
	if(m_bPhySnapShot)
		hostLvmFile = strLvmFile + "." + hostName + string("_Snapshot");
	else
		hostLvmFile = strLvmFile + "." + hostName;
	result = rename( strLvmFile.c_str() , hostLvmFile.c_str() );
	if(result != 0)
	{
		DebugPrintf(SV_LOG_ERROR,"Failed to rename file %s into %s \n ",strLvmFile.c_str(), hostLvmFile.c_str());	
	}
	result = rename( strLvmFileTemp.c_str() , strLvmFile.c_str());
	if(result != 0)
	{
		DebugPrintf(SV_LOG_ERROR,"Failed to rename file %s into %s \n ",strLvmFileTemp.c_str(), strLvmFile.c_str());
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;
	}
	else
	{
		DebugPrintf(SV_LOG_DEBUG, "Successfully edited the File: %s \n", strLvmFile.c_str());
	}
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return rv;
}

bool RollBackVM::repalceOldLVMconfFile(string hostName)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	string strLvmFile = "/etc/lvm/lvm.conf";
	string strLvmFileTemp = "/etc/lvm/lvm.conf.temp";
	string hostLvmFile;
	if(m_bPhySnapShot)
		hostLvmFile = strLvmFile + "." + hostName + string("_Snapshot");
	else
		hostLvmFile = strLvmFile + "." + hostName;
	int result;
	result = rename( strLvmFile.c_str() , strLvmFileTemp.c_str() );
	if(result != 0)
	{
		DebugPrintf(SV_LOG_ERROR,"Failed to rename file %s into %s \n ",strLvmFile.c_str(), strLvmFileTemp.c_str());	
	}
	result = rename( hostLvmFile.c_str() , strLvmFile.c_str());
	if(result != 0)
	{
		DebugPrintf(SV_LOG_ERROR,"Failed to rename file %s into %s \n ",hostLvmFile.c_str(), strLvmFile.c_str());
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;
	}
	else
	{
		DebugPrintf(SV_LOG_DEBUG, "Successfully Replaced the File: %s \n", strLvmFile.c_str());
	}
	result = rename( strLvmFileTemp.c_str() , hostLvmFile.c_str());
	if(result != 0)
	{
		DebugPrintf(SV_LOG_ERROR,"Failed to rename file %s into %s \n ",strLvmFileTemp.c_str(), hostLvmFile.c_str());	
	}	
	
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return true;
}

bool RollBackVM::vgScan()
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool bResult = true;
	stringstream results;
	
	string cmd = string("vgscan 1>>") + m_InMageCmdLogFile + string(" 2>>") + m_InMageCmdLogFile;
	DebugPrintf(SV_LOG_DEBUG,"cmd = : %s\n", cmd.c_str());
	
	string cmd1 = string("echo running command : \"") + cmd + string("\" on host : ") + m_VmName + string(" >>") + m_InMageCmdLogFile;
	if (!executePipe(cmd1, results))
	{
		DebugPrintf(SV_LOG_ERROR, "Failed to run the command to display the command on file\n");
	}
	
	results.clear();
	if (!executePipe(cmd, results))
	{
		DebugPrintf(SV_LOG_ERROR, "Failed to run the command: %s\n",cmd.c_str());
		bResult = false;
	}
	else
	{
		DebugPrintf(SV_LOG_DEBUG, "Successfully ran the cmd: %s\n", cmd.c_str());
		bResult = true;
	}
	
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bResult;
}

bool RollBackVM::pvScan()
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool bResult = true;
	stringstream results;

	string cmd = string("pvscan 1>>") + m_InMageCmdLogFile + string(" 2>>") + m_InMageCmdLogFile;
	DebugPrintf(SV_LOG_DEBUG,"cmd = : %s\n", cmd.c_str());

	string cmd1 = string("echo running command : \"") + cmd + string("\" on host : ") + m_VmName + string(" >>") + m_InMageCmdLogFile;
	if (!executePipe(cmd1, results))
	{
		DebugPrintf(SV_LOG_ERROR, "Failed to run the command to display the command on file\n");
	}

	results.clear();
	if (!executePipe(cmd, results))
	{
		DebugPrintf(SV_LOG_ERROR, "Failed to run the command: %s\n",cmd.c_str());
		bResult = false;
	}
	else
	{
		DebugPrintf(SV_LOG_DEBUG, "Successfully ran the cmd: %s\n", cmd.c_str());
		bResult = true;
	}

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bResult;
}

//This will the flush the buffer for the Mapper path. 
//Sometimes we saw partitions were not created in the mapper path after rollback
bool RollBackVM::blockDevFlushBuffer(string disk)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool bResult = true;
	stringstream results;
	
	string cmd = string("blockdev --flushbufs ") + disk + string(" 1>>")+ m_InMageCmdLogFile + string(" 2>>") + m_InMageCmdLogFile;
	DebugPrintf(SV_LOG_DEBUG,"cmd = : %s\n", cmd.c_str());
	
	string cmd1 = string("echo running command : \"") + cmd + string("\" on host : ") + m_VmName + string(" >>") + m_InMageCmdLogFile;
	if (!executePipe(cmd1, results))
	{
		DebugPrintf(SV_LOG_ERROR, "Failed to run the command to display the command on file\n");
	}
	
	results.clear();
	if (!executePipe(cmd, results))
	{
		DebugPrintf(SV_LOG_ERROR, "Failed to run the command: %s\n",cmd.c_str());
		bResult = false;
	}
	else
	{
		DebugPrintf(SV_LOG_DEBUG, "Successfully ran the cmd: %s\n", cmd.c_str());
		bResult = true;
	}
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bResult;
}


//This will remove the Partitions in Mapper Path.
//kpartx -d /dev/mapper/<device ID>
bool RollBackVM::RemovePartitionsMultipath(string disk)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool bResult = true;
	stringstream results;
	
	if(disk.find("dev/mapper") != string::npos)
	{
		string cmd = string("kpartx -d ") + disk + string(" 1>>") + m_InMageCmdLogFile + string(" 2>>") + m_InMageCmdLogFile;
		DebugPrintf(SV_LOG_INFO,"cmd = : %s\n", cmd.c_str());
		
		string cmd1 = string("echo running command : \"") + cmd + string("\" on host : ") + m_VmName + string(" >>") + m_InMageCmdLogFile;
		if (!executePipe(cmd1, results))
		{
			DebugPrintf(SV_LOG_ERROR, "Failed to run the command to display the command on file\n");
		}
		
		results.clear();
		if (!executePipe(cmd, results))
		{
			DebugPrintf(SV_LOG_ERROR, "Failed to run the command: %s\n",cmd.c_str());
			bResult = false;
		}
		else
		{
			DebugPrintf(SV_LOG_INFO, "Successfully ran the cmd: %s\n", cmd.c_str());
			bResult = true;
		}
	}
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bResult;
}


//This will show the Partitions in Mapper Path.
//kpartx -a /dev/mapper/<device ID>
bool RollBackVM::ShowPartitionsMultipath(string disk)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool bResult = true;
	stringstream results;
	
	if(disk.find("dev/mapper") != string::npos)
	{
		string cmd = string("kpartx -a ") + disk + string(" 1>>") + m_InMageCmdLogFile + string(" 2>>") + m_InMageCmdLogFile;
		DebugPrintf(SV_LOG_DEBUG,"cmd = : %s\n", cmd.c_str());
		
		string cmd1 = string("echo running command : \"") + cmd + string("\" on host : ") + m_VmName + string(" >>") + m_InMageCmdLogFile;
		if (!executePipe(cmd1, results))
		{
			DebugPrintf(SV_LOG_ERROR, "Failed to run the command to display the command on file\n");
		}
		
		results.clear();
		if (!executePipe(cmd, results))
		{
			DebugPrintf(SV_LOG_ERROR, "Failed to run the command: %s\n",cmd.c_str());
			bResult = false;
		}
		else
		{
			DebugPrintf(SV_LOG_DEBUG, "Successfully ran the cmd: %s\n", cmd.c_str());
			bResult = true;
		}
	}
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bResult;
}

bool RollBackVM::rescanDisk(string disk)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool bResult = true;
	stringstream results;
	
	string cmd = string("blockdev --rereadpt ") + disk + string(" 1>>") + m_InMageCmdLogFile + string(" 2>>") + m_InMageCmdLogFile;
	DebugPrintf(SV_LOG_DEBUG,"cmd = : %s\n", cmd.c_str());
	
	string cmd1 = string("echo running command : \"") + cmd + string("\" on host : ") + m_VmName + string(" >>") + m_InMageCmdLogFile;
	if (!executePipe(cmd1, results))
	{
		DebugPrintf(SV_LOG_ERROR, "Failed to run the command to display the command on file\n");
	}
	
	results.clear();
	if (!executePipe(cmd, results))
	{
		DebugPrintf(SV_LOG_ERROR, "Failed to run the command: %s\n",cmd.c_str());
		bResult = false;
	}
	else
	{
		DebugPrintf(SV_LOG_DEBUG, "Successfully ran the cmd: %s\n", cmd.c_str());
		bResult = true;
	}
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bResult;
}

bool RollBackVM::activateVg(string hostName)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool bResult = true;
	LinuxReplication objLin;
	std::string xmlFileName = std::string(objLin.getVxInstallPath()) + "failover_data/" + hostName + NETWORK_DETAILS_FILE;
	DebugPrintf(SV_LOG_INFO,"XML File - %s\n", xmlFileName.c_str());
	xmlDocPtr xDoc = xmlParseFile(xmlFileName.c_str());
	if (xDoc == NULL)
	{
		DebugPrintf(SV_LOG_ERROR,"XML document is not Parsed successfully.\n");
		std::map<std::string, std::string>::iterator iter = m_mapOfVmNameToId.find(hostName);
		if(iter == m_mapOfVmNameToId.end())
		{
			DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
			return false;
		}
		hostName = iter->second;
		xmlFileName = std::string(objLin.getVxInstallPath()) + "failover_data/" + hostName + NETWORK_DETAILS_FILE;
		DebugPrintf(SV_LOG_INFO,"XML File - %s\n", xmlFileName.c_str());
		xDoc = xmlParseFile(xmlFileName.c_str());
		if (xDoc == NULL)
		{
			DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
			return false;
		}
	}
	//Found the doc. Now process it.
	xmlNodePtr xRoot = xmlDocGetRootElement(xDoc);
	if (xRoot == NULL)
	{
		DebugPrintf(SV_LOG_ERROR,"The XML is empty. Cannot Proceed further.\n");
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		xmlFreeDoc(xDoc); return false;
	}
	if (xmlStrcmp(xRoot->name,(const xmlChar*)"Host")) 
	{
		//root is not found
		DebugPrintf(SV_LOG_ERROR,"The XML document is not in expected format : <host> entry not found\n");
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		xmlFreeDoc(xDoc); return false;
	}
	xmlNodePtr xChild = objLin.xGetChildNode(xRoot,(const xmlChar*)"Storage");
	if (xChild == NULL)
	{
		DebugPrintf(SV_LOG_ERROR,"The XML document is not in expected format : <Storage> entry not found\n");
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;
	}
	xmlChar *attr_value_temp;
	attr_value_temp = xmlGetProp(xChild,(const xmlChar*)"RootVolumeType");
	if (attr_value_temp == NULL)
	{
		DebugPrintf(SV_LOG_ERROR,"The XML document is not in expected format : <Storage RootVolumeType> entry not found\n");
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;
	}
    string strRootVolType = string((char *)attr_value_temp);
	
	if(strRootVolType == "LVM")
	{
		attr_value_temp = xmlGetProp(xChild,(const xmlChar*)"VgName");
		if (attr_value_temp == NULL)
		{
			DebugPrintf(SV_LOG_ERROR,"The XML document is not in expected format : <Storage VgName> entry not found\n");
			DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
			return false;
		}
		string strVgName = string((char *)attr_value_temp);
		stringstream results;
		string cmd = string("vgchange -ay ") + strVgName + string(" 1>>") + m_InMageCmdLogFile + string(" 2>>") + m_InMageCmdLogFile;
		DebugPrintf(SV_LOG_INFO,"cmd = : %s\n", cmd.c_str());
		
		string cmd1 = string("echo running command : \"") + cmd + string("\" on host : ") + m_VmName + string(" >>") + m_InMageCmdLogFile;
		if (!executePipe(cmd1, results))
		{
			DebugPrintf(SV_LOG_ERROR, "Failed to run the command to display the command on file\n");
		}
		
		results.clear();
		if (!executePipe(cmd, results))
		{
			DebugPrintf(SV_LOG_ERROR, "Failed to run the command: %s\n",cmd.c_str());
			bResult = false;
		}
		else
		{
			string result = results.str();
			trim(result);
			DebugPrintf(SV_LOG_INFO, "Successfully ran the cmd: %s Output: %s\n", cmd.c_str(), result.c_str());
			bResult = true;
		}
		
		cmd = string("vgmknodes ") + strVgName + string(" 1>>") + m_InMageCmdLogFile + string(" 2>>") + m_InMageCmdLogFile;
		DebugPrintf(SV_LOG_INFO,"cmd = : %s\n", cmd.c_str());
		
		cmd1 = string("echo running command : \"") + cmd + string("\" on host : ") + m_VmName + string(" >>") + m_InMageCmdLogFile;
		if (!executePipe(cmd1, results))
		{
			DebugPrintf(SV_LOG_ERROR, "Failed to run the command to display the command on file\n");
		}
		
		results.clear();
		if (!executePipe(cmd, results))
		{
			DebugPrintf(SV_LOG_ERROR, "Failed to run the command: %s\n",cmd.c_str());
			bResult = false;
		}
		else
		{
			string result = results.str();
			trim(result);
			DebugPrintf(SV_LOG_INFO, "Successfully ran the cmd: %s Output: %s\n", cmd.c_str(), result.c_str());
			bResult = true;
		}
		sleep(2);
	}
	
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bResult;
}

bool RollBackVM::deActivateVg(string hostName)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool bResult = true;
	LinuxReplication objLin;
	std::string xmlFileName = std::string(objLin.getVxInstallPath()) + "failover_data/" + hostName + NETWORK_DETAILS_FILE;
	DebugPrintf(SV_LOG_INFO,"XML File - %s\n", xmlFileName.c_str());
	xmlDocPtr xDoc = xmlParseFile(xmlFileName.c_str());
	if (xDoc == NULL)
	{
		DebugPrintf(SV_LOG_ERROR,"XML document is not Parsed successfully.\n");
	    std::map<std::string, std::string>::iterator iter = m_mapOfVmNameToId.find(hostName);
		if(iter == m_mapOfVmNameToId.end())
		{
			DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
			return false;
		}
		hostName = iter->second;
		xmlFileName = std::string(objLin.getVxInstallPath()) + "failover_data/" + hostName + NETWORK_DETAILS_FILE;
		DebugPrintf(SV_LOG_INFO,"XML File - %s\n", xmlFileName.c_str());
		xDoc = xmlParseFile(xmlFileName.c_str());
		if (xDoc == NULL)
		{
			DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
			return false;
		}
	}
	//Found the doc. Now process it.
	xmlNodePtr xRoot = xmlDocGetRootElement(xDoc);
	if (xRoot == NULL)
	{
		DebugPrintf(SV_LOG_ERROR,"The XML is empty. Cannot Proceed further.\n");
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		xmlFreeDoc(xDoc); return false;
	}
	if (xmlStrcmp(xRoot->name,(const xmlChar*)"Host")) 
	{
		//root is not found
		DebugPrintf(SV_LOG_ERROR,"The XML document is not in expected format : <host> entry not found\n");
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		xmlFreeDoc(xDoc); return false;
	}
	xmlNodePtr xChild = objLin.xGetChildNode(xRoot,(const xmlChar*)"Storage");
	if (xChild == NULL)
	{
		DebugPrintf(SV_LOG_ERROR,"The XML document is not in expected format : <Storage> entry not found\n");
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;
	}
	xmlChar *attr_value_temp;
	attr_value_temp = xmlGetProp(xChild,(const xmlChar*)"RootVolumeType");
	if (attr_value_temp == NULL)
	{
		DebugPrintf(SV_LOG_ERROR,"The XML document is not in expected format : <Storage RootVolumeType> entry not found\n");
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;
	}
    string strRootVolType = string((char *)attr_value_temp);
	
	if(strRootVolType == "LVM")
	{
		attr_value_temp = xmlGetProp(xChild,(const xmlChar*)"VgName");
		if (attr_value_temp == NULL)
		{
			DebugPrintf(SV_LOG_ERROR,"The XML document is not in expected format : <Storage VgName> entry not found\n");
			DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
			return false;
		}
		string strVgName = string((char *)attr_value_temp);
		stringstream results;
		
		string cmd = string("vgchange -an ") + strVgName + string(" 1>>") + m_InMageCmdLogFile + string(" 2>>") + m_InMageCmdLogFile;
		DebugPrintf(SV_LOG_INFO,"cmd = : %s\n", cmd.c_str());
		
		string cmd1 = string("echo running command : \"") + cmd + string("\" on host : ") + m_VmName + string(" >>") + m_InMageCmdLogFile;
		if (!executePipe(cmd1, results))
		{
			DebugPrintf(SV_LOG_ERROR, "Failed to run the command to display the command on file\n");
		}
		
		results.clear();
		if (!executePipe(cmd, results))
		{
			DebugPrintf(SV_LOG_ERROR, "Failed to run the command: %s\n",cmd.c_str());
			bResult = false;
		}
		else
		{
			string result = results.str();
			trim(result);
			DebugPrintf(SV_LOG_INFO, "Successfully ran the cmd: %s, OutPut from the cmd: %s\n", cmd.c_str(), result.c_str());
			bResult = true;
		}		
	}
	
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bResult;
}

bool RollBackVM::checkOStypeFromXml(string hostName)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool bResult = true;
	
	LinuxReplication objLin;
	std::string xmlFileName = std::string(objLin.getVxInstallPath()) + "failover_data/" + hostName + NETWORK_DETAILS_FILE;
	DebugPrintf(SV_LOG_INFO,"XML File - %s\n", xmlFileName.c_str());
	
	xmlDocPtr xDoc = xmlParseFile(xmlFileName.c_str());
	if (xDoc == NULL)
	{
		DebugPrintf(SV_LOG_ERROR,"XML document is not Parsed successfully.\n");
		std::map<std::string, std::string>::iterator iter=m_mapOfVmNameToId.find(hostName);
		if(iter != m_mapOfVmNameToId.end())
		{
			hostName = iter->second;
			xmlFileName = std::string(objLin.getVxInstallPath()) + "failover_data/" + hostName + NETWORK_DETAILS_FILE;
			DebugPrintf(SV_LOG_INFO,"XML File - %s\n", xmlFileName.c_str());
			xDoc = xmlParseFile(xmlFileName.c_str());
			if (xDoc == NULL)
			{
				DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
				return false;
			}
		}
		else
		{
			DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
			return false;
		}
	}

	//Found the doc. Now process it.
	xmlNodePtr xRoot = xmlDocGetRootElement(xDoc);
	if (xRoot == NULL)
	{
		DebugPrintf(SV_LOG_ERROR,"The XML is empty. Cannot Proceed further.\n");
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		xmlFreeDoc(xDoc); return false;
	}
	if (xmlStrcmp(xRoot->name,(const xmlChar*)"Host")) 
	{
		//root is not found
		DebugPrintf(SV_LOG_ERROR,"The XML document is not in expected format : <host> entry not found\n");
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		xmlFreeDoc(xDoc); return false;
	}
	
	xmlChar *attr_value_temp;
	
	attr_value_temp = xmlGetProp(xRoot,(const xmlChar*)"OsType");
	if (attr_value_temp == NULL)
	{
		DebugPrintf(SV_LOG_ERROR,"The XML document is not in expected format : <Storage OsType> entry not found\n");		
		m_OsFlavor = "RedHat";
	}
	else
		m_OsFlavor = string((char *)attr_value_temp);
	
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bResult;
}

bool RollBackVM::addEntryMpathConfigFile(string hostName)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool bResult = true;
	list<string> listMapperPath;
	list<string>::iterator listIter;
	std::map<std::string,std::map<std::string, std::string> >::iterator iter;
	if((iter = m_mapOfVmToSrcAndTgtDisks.find(hostName)) != m_mapOfVmToSrcAndTgtDisks.end())
	{
		map<string, string> mapDisk = iter->second;
		map<string, string>::iterator mapIter = mapDisk.begin();
		for(;mapIter != mapDisk.end(); mapIter++)
		{
			string strDevicePath;
			strDevicePath = mapIter->second.substr(5);
			DebugPrintf(SV_LOG_INFO,"Pushing into list: %s\n",strDevicePath.c_str());
			listMapperPath.push_back(strDevicePath);
		}
	}
	std::string strMultiPathFile = "/etc/multipath.conf";
	std::string strMultiPathFileTemp = "/etc/multipath.conf.temp";
	ifstream inFile(strMultiPathFile.c_str());
	ofstream outFile(strMultiPathFileTemp.c_str());
	if(!inFile.is_open())
	{
		DebugPrintf(SV_LOG_ERROR,"Failed to open file %s\n ",strMultiPathFile.c_str());
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;
	}
    string strLineRead;
	while(std::getline(inFile,strLineRead))
	{
		outFile << strLineRead << endl;
		if(strLineRead == "blacklist {")
		{
			std::getline(inFile,strLineRead);
			if(strLineRead == "devnode \"sda$\"")
			{
				outFile << strLineRead << endl;
			}
			else
			{
				outFile << "devnode \"sda$\"" << endl;
			}
			for(listIter = listMapperPath.begin(); listIter != listMapperPath.end(); listIter++)
			{
				outFile << "devnode \"" << (*listIter) << "$\"" << endl;
			}
		}
	}
	
	inFile.close();
	outFile.close();
	int result;
	string tmpMultipathFile;
	if(m_bPhySnapShot)
		tmpMultipathFile = "/etc/multipath.conf." + hostName + string("_Snapshot");
	else
		tmpMultipathFile = "/etc/multipath.conf." + hostName;
	result = rename( strMultiPathFile.c_str() , tmpMultipathFile.c_str() );
	if(result != 0)
	{
		DebugPrintf(SV_LOG_ERROR,"Failed to rename file %s into %s \n ",strMultiPathFile.c_str(), tmpMultipathFile.c_str());	
	}
	result = rename( strMultiPathFileTemp.c_str() , strMultiPathFile.c_str());
	if(result != 0)
	{
		DebugPrintf(SV_LOG_ERROR,"Failed to rename file %s into %s \n ",strMultiPathFileTemp.c_str(), strMultiPathFile.c_str());
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;
	}
	else
	{
		DebugPrintf(SV_LOG_INFO, "Successfully edited the File: %s \n", strMultiPathFile.c_str());
	}
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return true;
}

bool RollBackVM::repalceOldMultiPathconfFile(string hostName)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	string strMultiPathFile = "/etc/multipath.conf";
	string strMultiPathFileTemp = "/etc/multipath.conf.temp";
	string hostMultiPathFile;
	if(m_bPhySnapShot)
		hostMultiPathFile = strMultiPathFile + "." + hostName + string("_Snapshot");
	else
		hostMultiPathFile = strMultiPathFile + "." + hostName;
	int result;
	result = rename( strMultiPathFile.c_str() , strMultiPathFileTemp.c_str() );
	if(result != 0)
	{
		DebugPrintf(SV_LOG_ERROR,"Failed to rename file %s into %s \n ",strMultiPathFile.c_str(), strMultiPathFileTemp.c_str());	
	}
	result = rename( hostMultiPathFile.c_str() , strMultiPathFile.c_str());
	if(result != 0)
	{
		DebugPrintf(SV_LOG_ERROR,"Failed to rename file %s into %s \n ",hostMultiPathFile.c_str(), strMultiPathFile.c_str());
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;
	}
	else
	{
		DebugPrintf(SV_LOG_INFO, "Successfully Replaced the File: %s \n", strMultiPathFile.c_str());
	}
	result = rename( strMultiPathFileTemp.c_str() , hostMultiPathFile.c_str());
	if(result != 0)
	{
		DebugPrintf(SV_LOG_ERROR,"Failed to rename file %s into %s \n ",strMultiPathFileTemp.c_str(), hostMultiPathFile.c_str());	
	}	
	
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return true;
}

#endif


// A2E failback support
//========================

#ifdef WIN32

bool RollBackVM::ParseSourceOsVolumeDetails(const std::string &srcVMId, ChangeVmConfig & vmObj)
{
	if (isFailback())
		return ParseSourceOsVolumeDetailsForFailbackProtection(srcVMId, vmObj);
	else
		return ParseSourceOsVolumeDetailsForForwardProtection(srcVMId,vmObj);
}

bool RollBackVM::ParseSourceOsVolumeDetailsForForwardProtection(const std::string &srcVMId, ChangeVmConfig & vmObj)
{
	DebugPrintf(SV_LOG_DEBUG, "Entered %s\n", __FUNCTION__);
	bool bSystemVol = false;

	map<std::string, map<string, string> >::iterator iterVmRecInfo = m_RecVmsInfo.m_mapVMRecoveryInfo.find(srcVMId);
	map<std::string, map<string, string> >::iterator iterVmVolPairInfo = m_RecVmsInfo.m_volumeMapping.find(srcVMId);
	if (iterVmRecInfo != m_RecVmsInfo.m_mapVMRecoveryInfo.end())
	{

		map<string, string>::iterator iterHostSysInfo = iterVmRecInfo->second.begin();
		for (; iterHostSysInfo != iterVmRecInfo->second.end(); iterHostSysInfo++)
		{

			if (iterHostSysInfo->first.compare("SystemVolume") == 0)
			{
				map<string, string>::iterator iterSysVol = iterVmVolPairInfo->second.find(iterHostSysInfo->second);
				if (iterSysVol != iterVmVolPairInfo->second.end())
				{
					DebugPrintf(SV_LOG_DEBUG, "Found system volume : %s --> respective target volume : %s\n", iterHostSysInfo->second.c_str(), iterSysVol->second.c_str());
					vmObj.m_mapOfVmInfoForPostRollback.insert(make_pair("SrcSystemVolume", iterHostSysInfo->second));
					vmObj.m_mapOfVmInfoForPostRollback.insert(make_pair("SrcRespTgtSystemVolume", iterSysVol->second));
					bSystemVol = true;
				}

				break;
			}

		}

		if (!bSystemVol)
		{
			map<string, string>::iterator iterSysVol = iterVmVolPairInfo->second.find("C");
			if (iterSysVol != iterVmVolPairInfo->second.end())
			{
				DebugPrintf(SV_LOG_DEBUG, "Found system volume : \"C\" --> respective target volume : %s\n", iterSysVol->second.c_str());
				vmObj.m_mapOfVmInfoForPostRollback.insert(make_pair("SrcSystemVolume", "C"));
				vmObj.m_mapOfVmInfoForPostRollback.insert(make_pair("SrcRespTgtSystemVolume", iterSysVol->second));
				bSystemVol = true;
			}
			else
			{
				DebugPrintf(SV_LOG_ERROR, "Target system volume not found, failed to perform post rollback operation for host - %s\n", srcVMId.c_str());
			}
		}
	}

	DebugPrintf(SV_LOG_DEBUG, "Exiting %s\n", __FUNCTION__);
	return bSystemVol;
}

bool RollBackVM::ParseSourceOsVolumeDetailsForFailbackProtection(const std::string &srcVMId, ChangeVmConfig & vmObj)
{
	DebugPrintf(SV_LOG_DEBUG, "Entered %s\n", __FUNCTION__);
	bool bFoundBootDiskExtent = false;
	bool bSystemVol = false;

	map<std::string, map<string, string> >::iterator iterVmRecInfo = m_RecVmsInfo.m_mapVMRecoveryInfo.find(srcVMId);

	if (iterVmRecInfo != m_RecVmsInfo.m_mapVMRecoveryInfo.end())
	{

		map<string, string>::iterator iterHostSysInfo = iterVmRecInfo->second.begin();
		for (; iterHostSysInfo != iterVmRecInfo->second.end(); iterHostSysInfo++)
		{

			if (iterHostSysInfo->first.compare("boot_disk_drive_extent") == 0)
			{
				DebugPrintf(SV_LOG_DEBUG, "VM: %s Found boot_disk_drive_extent : %s\n", srcVMId.c_str(), iterHostSysInfo->second.c_str());
				vmObj.m_mapOfVmInfoForPostRollback.insert(make_pair("boot_disk_drive_extent", iterHostSysInfo->second));
				bFoundBootDiskExtent = true;
			}
			else if (iterHostSysInfo->first.compare("SystemVolume") == 0)
			{
				DebugPrintf(SV_LOG_DEBUG, "VM: %s Found system volume : %s.\n", srcVMId.c_str(), iterHostSysInfo->second.c_str());
				vmObj.m_mapOfVmInfoForPostRollback.insert(make_pair("SystemVolume", iterHostSysInfo->second));
				bSystemVol = true;
			}

			if (bFoundBootDiskExtent && bSystemVol)
			{
				break;
			}

		}
	}

	if (!bFoundBootDiskExtent)
	{
		DebugPrintf(SV_LOG_ERROR, "VM: %s boot_disk_drive_extent property is not sent by CS.\n", srcVMId.c_str());
	}

	if (!bSystemVol)
	{
		DebugPrintf(SV_LOG_ERROR, "VM: %s SystemVolume property is not sent by CS.\n", srcVMId.c_str());
	}

	DebugPrintf(SV_LOG_DEBUG, "Exiting %s\n", __FUNCTION__);
	return (bFoundBootDiskExtent && bSystemVol);
}

#endif
