// Copyright (c) 2005 InMage
// This file contains proprietary and confidential information and
// remains the unpublished property of InMage. Use, disclosure,
// or reproduction is prohibited except as permitted by express
// written license aggreement with InMage.
//
// File       : VsnapUser.cpp
//
// Description: Vsnap User library definitions.
//

#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#ifdef SV_WINDOWS
#include<windows.h>
#include<winioctl.h>
#include<io.h>
#include<sys\types.h>
#include<sys\stat.h> 
#include<dbt.h>
#include "hostagenthelpers.h"
#include "autoFS.h"
#include "VVDevControl.h"
#include "devicefilter.h"
#endif

#include "VacpUtil.h"
#include<iostream>
#include<sstream>
#include <iomanip>
#include<errno.h>

#include <boost/format.hpp>

#include "inmageex.h"

#include "portablehelpers.h"
#include "globs.h"
#include "DiskBtreeCore.h"
#include "VsnapCommon.h"
#include "VsnapUser.h"
#include "DiskBtreeHelpers.h"

#include "cdputil.h"
#include "cdpsnapshot.h"
#include "cdpsnapshotrequest.h"
#include "cdplock.h"   
#include <ace/Guard_T.h>
#include "ace/Thread_Mutex.h"
#include "ace/Recursive_Thread_Mutex.h"
#include "configwrapper.h"

#include "differentialfile.h"

#include "inmcommand.h"
#include "inmsafecapis.h"
#include "inmsafeint.h"
#include "inmageex.h"
#include "inmalertdefs.h"


#ifdef SV_WINDOWS
#include "VVolCmds.h"
#include <ctype.h>
#include "InstallInVirVol.h"
#include "ListEntry.h"
#include <setupapi.h>
#endif

#include <ace/ACE.h>
#include <ace/OS.h>
#include <ace/OS_NS_fcntl.h>
#include <ace/OS_NS_unistd.h>
#include <ace/OS_NS_errno.h>
#include <ace/Global_Macros.h>

#include <fstream>


#include <boost/lexical_cast.hpp>
#include <boost/shared_array.hpp>

#ifdef SV_WINDOWS
#include<windows.h>
#endif

using namespace std;

#define FILE_END            2
#define FILEID_VECTOR_MAX_SIZE	80

#ifdef SV_WINDOWS
const char SV_VIRTUAL_SNAPSHOT_ID_VALUE_NAME[] = _T("VirtualSnapShotId");
#endif


#ifdef SV_WINDOWS
extern bool IsSingleListEmpty(PSINGLE_LIST_ENTRY);
extern void InitializeEntryList(PSINGLE_LIST_ENTRY);
extern SINGLE_LIST_ENTRY *PopEntryList(PSINGLE_LIST_ENTRY);
extern void InsertInList(PSINGLE_LIST_ENTRY, SV_ULONGLONG, SV_UINT, SV_UINT , SV_ULONGLONG, SV_UINT);
extern void StartVirtualVolumeDriver(ACE_HANDLE hDevice, PSTART_VIRTAL_VOLUME_DATA pStartVirtualVolumeData);
extern void StopVirtualVolumeDriver(ACE_HANDLE hDevice, PSTOP_VIRTUAL_VOLUME_DATA pStopVirtualVolumeData);
#endif

extern void UnloadExistingVolumes(ACE_HANDLE hDevice);

extern void VsnapCallBack(void *, void *, void *, bool);
extern bool operator<(VsnapKeyData &, VsnapKeyData &);
extern bool operator>(VsnapKeyData &, VsnapKeyData &);
extern bool operator==(VsnapKeyData &, VsnapKeyData &);

bool VsnapUserGetFileNameFromFileId(string, unsigned long long, unsigned long, char *);
void VsnapNodeTraverseCallback(void *NodeKey, void *NewKey, void *param, bool flag);
unsigned long long      LastSnapShotId = 0;  
const size_t MOUNT_PATH_LEN = 2048;

VsnapMgr::VsnapMgr()
{
    SnapShotObj = NULL;
    m_SVServer = "";
    m_HostId = "";
    m_HttpPort = 0;
    m_TheConfigurator = NULL ; //  Ecase 2934 
    IsConfigAvailable = true ;  // Ecase 2934 
    notifyCx = true; //Enable notifications by default
    try {
        if(!GetConfigurator(&m_TheConfigurator))
        {
            DebugPrintf(SV_LOG_INFO, "Communication with Central Management Server is currently unavailable.\n");
            IsConfigAvailable = false ;
            ACE_OS::last_error(0);//Flush the ACE Error stack
        }
    } 
    catch(...) {
        DebugPrintf(SV_LOG_INFO, "Communication with Central Management Server is currently unavailable.\n");
        IsConfigAvailable = false ;
        ACE_OS::last_error(0);//Flush the ACE Error stack
    }	
}

void VsnapMgr::VsnapPrint(const char *string)
{
    if(m_SVServer.empty())
        DebugPrintf(SV_LOG_INFO,"%s\n", string); 
}

void VsnapMgr::VsnapGetErrorMsg(const VsnapErrorInfo & ErrorStatus, string & msg)
{

    switch (ErrorStatus.VsnapErrorStatus)
    {
    case VSNAP_UNKNOWN_FS_FOR_TARGET:
        msg += "File System type unknown for replicated target volume ";
        break;
    case VSNAP_TARGET_HANDLE_OPEN_FAILED:
        msg += "Failed to get handle to replicated target volume ";
        break;
    case VSNAP_TARGET_VOLUMESIZE_UNKNOWN:
        msg += "Failed to get volume size for replicated target volume ";
        break;
    case VSNAP_TARGET_SECTORSIZE_UNKNOWN: 
        msg += "Failed to get sector size for replicated target volume "; 
        break;	
    case VSNAP_READ_TARGET_FAILED:
        msg += "Failed to read data from target volume ";
        break;
    case VSNAP_DB_INIT_FAILED:
        msg += "Failed to initialize retention database ";
        break;
    case VSNAP_DB_PARSE_FAILED:
        msg += "Failed to parse retention log files ";
        break;
    case VSNAP_MAP_INIT_FAILED:
        msg += "Failed to initialize btree map file. Please make sure map file path is supplied correctly and has got enough space to create the map files. ";
        break;
    case VSNAP_MAP_OP_FAILED:
        msg += "Failed to read/write from the map file ";
        break;
    case VSNAP_FILETBL_OP_FAILED:
        msg += "Failed to read/write from the file table ";
        break;
    case VSNAP_MEM_UNAVAILABLE:
        msg += "Memory Unavailable ";
        break;
    case VSNAP_VIRVOL_OPEN_FAILED:
        msg += "Failed to open virtual volume driver ";
        break;
    case VSNAP_MOUNT_IOCTL_FAILED:
        msg += "Mount IOCTL Failed ";
        break;
    case VSNAP_MOUNT_FAILED:
        msg += "Mount Operation Failed ";
        break;
    case VSNAP_UNMOUNT_FAILED:
        msg += "UnMount Operation Failed ";
        break;
    case VSNAP_DRIVER_LOAD_FAILED:
        msg += "Virtual Volume Driver Failed to load ";
        break;
    case VSNAP_DRIVER_UNLOAD_FAILED:
        msg += "Virtual Volume Driver Unload Failed to load ";
        break;
    case VSNAP_DRIVER_INSTALL_FAILED:
        msg += "Virtual Volume Driver Install Failed ";
        break;
    case VSNAP_INVALID_MOUNT_POINT:
        msg += "Invalid Mount Point Specified";
        break;
    case VSNAP_VOLUME_INFO_FAILED:
        msg += "Failed to get volume information for the mount point";
        break;
    case VSNAP_MOUNTDIR_CREATION_FAILED:
        msg += "Failed to create mount directory";
        break;
    case VSNAP_MOUNTPT_NOT_DIRECTORY:
        msg += "Specified Mount Exists But is not a directory";
        break;
    case VSNAP_DIRECTORY_NOT_EMPTY:
        msg += "Specified mount directory is not empty";
        break;
    case VSNAP_REPARSE_PT_NOT_SUPPORTED:
        msg += "The specified volume for mount point doesnt support reparse points";
        break;
    case VSNAP_MOUNTDIR_STAT_FAILED:
        msg += "Failed to get mount directory details";
        break;
    case VSNAP_INVALID_SOURCE_VOLUME:
        msg += "Invalid Source Volume Specified";
        break;
    case VSNAP_GET_SYMLINK_FAILED:
        msg += "GET Symlink IOCTL Failed";
        break;
    case VSNAP_OPEN_HANDLES:
        msg += "There are applications accessing the virtual volume. Close them and try again.";
        break;
    case VSNAP_UNSUPPORTED_DB:
        msg += "Unsupported retention database version encountered. ";
        break;
    case VSNAP_TARGET_NOT_SPECIFIED:
        msg += "Target Volume Not Specified, please specify --target option. ";
        break;
    case VSNAP_VIRTUAL_NOT_SPECIFIED:
        msg += "Virtual Volume Not Specified, please specify --virtual option. ";
        break;
    case VSNAP_VIRTUAL_DRIVELETTER_INUSE:
        msg += "The specified Drive letter/Mount Point for virtual volume is in use.\nPlease specify different Drive letter/Mount point. ";
        break;
    case VSNAP_RECOVERYPOINT_CONFLICT:
        msg += "Please specify either time based recovery or event based recovery but not both. ";
        break;
    case VSNAP_DB_UNKNOWN:
        msg += "Unable to determine the retention path for the specified target.\nPlease use --db option. ";
        break;
    case VSNAP_PRIVATE_DIR_NOT_SPECIFIED:
        msg += "Private data directory path is mandatory for this option. ";
        break;
    case VSNAP_GET_DB_VERSION_FAILED:
        msg += "Unable to determine database version for the retention path supplied. ";
        break;
    case VSNAP_APPNAMETOTYPE_FAILED:
        msg += "Invalid application name specified. ";
        break;
    case VSNAP_GETMARKER_FAILED:
        msg += "Unable to determine the event associated with the specified criteris. ";
        break;
    case VSNAP_MATCHING_EVENT_NOT_FOUND:
        msg += "Could not find event matching the specified criteria. ";
        break;
    case VSNAP_MATHING_EVENT_NUMBER_NOT_FOUND:
        msg += "Please specify a valid event number. Actual number of consistency for this event is \nless than the supplied event number ";
        break;
    case VSNAP_GET_VOLUME_INFO_FAILED:
        msg += "Failed to get volume information for the specified mount point. ";
        break;
    case VSNAP_INVALID_MOUNT_PT:
        msg += "Specified drive letter or mount point is not a virtual volume. ";
        break;
    case VSNAP_TRACK_IOCTL_FAILED:
        msg += "Tracking IOCTL to the driver failed. ";
        break;
    case VSNAP_TRACK_IOCTL_ERROR:
        msg += "Can't perform the operation, virtual volume is mounted in read-only access mode. ";
        break;
    case VSNAP_MAP_FILE_NOT_FOUND:
        msg += "Failed to initialize the map file. \n";
        break;
    case VSNAP_GETVOLUMENAME_API_FAILED:
        msg += "GetVolumeNameFromVolumeMountPoint API failed. ";
        break;
    case VSNAP_DELETEMNTPT_API_FAILED:
        msg += "DeleteVolumeMountPoint API failed. ";
        break;
    case VSNAP_TARGET_OPEN_FAILED:
        msg += "Failed to open target volume. ";
        break;
    case VSNAP_SETMNTPT_API_FAILED:
        msg += "SetVolumeMountPoint API failed. ";
        break;
    case VSNAP_TARGET_OPENEX_FAILED:
        msg += "Failed to open target volume in exclusive mode. ";
        break;
    case VSNAP_ID_VIRTUALINFO_UNKNOWN:
        msg += "VsnapId or virtual volume unknown. ";
        break;
    case VSNAP_ID_VOLNAME_CONFLICT:
        msg += "Both VsnapId and virtual volume specified. Specify only one of these options. ";
        break;
    case VSNAP_ID_TGTVOLNAME_EMPTY:
        msg += "Target Volume name is unknown. ";
        break;
    case VSNAP_MAP_LOCATION_UNKNOWN:
        msg += "Map file directory unknown. ";
        break;
    case VSNAP_PRESCRIPT_EXEC_FAILED:
        msg += "PreScript Execution Failed. ";
        break;
    case VSNAP_POSTSCRIPT_EXEC_FAILED:
        msg += "PostScript Execution Failed. ";
        break;
    case VSNAP_DATADIR_CREATION_FAILED:
        msg += "Failed to create private vsnap data directory. ";
        break;
    case VSNAP_INVALID_VIRTUAL_VOLUME:
        msg += "Invalid Virtual Volume Specified. ";
        break;
    case VSNAP_DELETE_LOG_FAILED:
        msg += "Failed to delete the logs. Please specify the path using --datalogpath and vsnapid using --vsnapid option. ";
        break;
    case VSNAP_TARGET_UNHIDDEN_RW:
        msg += "Target volume is unhidden and read-write.\nVsnap can't be performed for this volume. ";
        break;
    case VSNAP_TARGET_HIDDEN:
        msg += "Target volume is hidden. Vsnap can't be taken to a hidden volume.";
        break;
    case VSNAP_PIT_INIT_FAILED:
        msg += "Failed to initialize Point-In-Time Vsnap. Please make sure datalogpath has enough space. ";
        break;
    case L_VSNAP_MOUNT_FAILED:
        msg+= "FS Mount Failed.";
        break;
    case VSNAP_UNMOUNT_FAILED_FS_PRESENT:
        msg+="Device is busy. Can't proceed with Vsnap::Unmount \n";
        break;
    case VSNAP_MKDIR_FAILED:/* Fall Through */
        msg+="Unable to create a directory for MountPoint\n";
        break;
    case VSNAP_MAJOR_NUMBER_FAILED: /* Fall Through */
        msg+="Unable to get the Major Number \n";
        break;
    case VSNAP_MKNOD_FAILED:/* Fall Through */
        msg+="MkNod Failed ...Unable to create block device \n";
        break;
    case VSANP_NOTBLOCKDEVICE_FAILED: /* Fall Through */
        msg+="It is not a block device \n";
        break;	
    case VSANP_NOTCHARDEVICE_FAILED: /* Fall Through */
        msg+="It is not a char device \n";
        break;
    case VSANP_MKDEV_FAILED: /* Fall Through */
        msg+="mkdev Failed \n";
        break;
    case VSANP_RMDEV_FAILED: /* Fall Through */
        msg+="rmdev Failed  \n";
        break;
    case VSANP_RELDEVNO_FAILED: /* Fall Through */
        msg+="reldevno Failed \n";
        break;
    case VSANP_MAJOR_MINOR_CONFLICT:
        msg+="Major -- Minor Conflict  \n";
        break;
    case VSNAP_RMDIR_FAILED:
        msg+="Unable to delete the dir   \n";
        break;
    case VSNAP_DELETE_LOGFILES_FAILED:
        msg+="Unable to delete the log files \n";
        break;
    case VSNAP_VIRTUAL_VOLUME_NOTEXIST:
        msg+="Make sure the virtual volume supplied  exist \n";
        break;
    case DATALOG_VOLNAME_CONFLICT:
        msg+="Both Datalog path and virtual volume specified. Specify only one of these options. ";
        break;
    case VSNAP_UNIQUE_ID_FAILED:
        msg+="Unable to retrieve unique id while creating the vsnap.";
        break;
        //Bug #6133
    case VSNAP_NOT_A_TARGET_VOLUME:
        msg+="vsnap cannot be taken as the volume specified isn't a replication target.\n";
        break;
    case VSNAP_TARGET_UNHIDDEN:
        msg+="vsnap cannot be taken as the volume specified is not locked.\n";
        break;
    case VSNAP_TARGET_RESYNC:
        msg+="vsnap cannot be taken as the source of vsnap pair is under resync\n";
        break;
    case VSNAP_CONTAINS_MOUNTED_VOLUMES:
        msg+="vsnap cannot be taken as the virtual drivename( mountpoint ) specified contains mounted volumes\n";
        break;
    case VSNAP_DIR_SAME_AS_PWD:
        msg+="vsnap cannot be unmounted\n";
        break;
    case VSNAP_VOLNAME_SAME_AS_DATALOG:
        msg+="Vsnap cannot be taken as Virtual Volume is same as Private Data Directory\n";
        break;
    case VSNAP_OPEN_LINVSNAP_BLOCK_DEV_FAILED:
        msg+="failed to open the linvsnap sync driver\n";
        break;
    case VSNAP_CLOSE_LINVSNAP_BLOCK_DEV_FAILED:
        msg+="failed to close the linvsnap sync driver\n";
        break;
    case VSNAP_ZPOOL_DESTROY_EXPORT_FAILED:
        msg+="failed to destroy or export the zpool for vsnap\n";
        break;
    case VSNAP_TARGET_SPARSEFILE_CHUNK_SIZE_FAILED:
        msg+="failed to get the chunk size of the sparse file used as target.\n";
        break;
	case VSNAP_ACQUIRE_RECOVERY_LOCK_FAILED:
        msg+="failed to acquire read lock on the target volume.\n";
        break;
    default:
        msg += "Unknown Error";
        break;
    }
    
    if(!ErrorStatus.VsnapErrorMessage.empty())
    {
        msg += ErrorStatus.VsnapErrorMessage;
    }
    else if(ErrorStatus.VsnapErrorCode)
    {
        std::stringstream errcode;
        errcode << "ErrorCode: " << ErrorStatus.VsnapErrorCode << "\n";
        msg += errcode.str();
    }
}

void VsnapMgr::VsnapDebugPrint(VsnapPairInfo *PairInfo, SV_LOG_LEVEL level)
{
    string Operation = "";

    if(level == SV_LOG_ERROR)
        Operation = "Failed";
    else
        Operation = "Success";

    if(m_SVServer[0] != '\0')
        DebugPrintf(level,"VSNAP DEBUG OP %d: %s -> SrcVol= %s, RetentionPath= %s, VirVol= %s, PrivDir= %s, AppName= %s,VsnapID" ULLSPEC ", AccessMode= %d, VsnapType= %d ErrCode = %d ErrorStatus = %x\n", PairInfo->VirtualInfo->State, Operation.c_str(), PairInfo->SrcInfo->VolumeName.c_str(),PairInfo->SrcInfo->RetentionDbPath.c_str(),PairInfo->VirtualInfo->VolumeName.c_str(),PairInfo->VirtualInfo->PrivateDataDirectory.c_str(),PairInfo->VirtualInfo->AppName.c_str(),PairInfo->VirtualInfo->SnapShotId,PairInfo->VirtualInfo->AccessMode, PairInfo->VirtualInfo->VsnapType, PairInfo->VirtualInfo->Error.VsnapErrorCode, PairInfo->VirtualInfo->Error.VsnapErrorStatus);

}

void VsnapMgr::VsnapDebugPrint(VsnapVirtualVolInfo *VirtualInfo, SV_LOG_LEVEL level)
{
    string Operation = "";

    if(level == SV_LOG_ERROR)
        Operation = "Failed";
    else
        Operation = "Success";

    if(m_SVServer[0] != '\0')
        DebugPrintf(level,"VSNAP DEBUG OP %d: %s -> VirVol= %s, PrivDir= %s, VsnapID " ULLSPEC "ErrCode = %d ErrorStatus = %x\n", VirtualInfo->State, Operation.c_str(), VirtualInfo->VolumeName.c_str(), VirtualInfo->PrivateDataDirectory.c_str(), VirtualInfo->SnapShotId, VirtualInfo->Error.VsnapErrorCode, VirtualInfo->Error.VsnapErrorStatus);

}

void VsnapMgr::VsnapErrorPrint(VsnapPairInfo *PairInfo, int SendToCx)
{
    string			msg;
    VsnapErrorInfo	*Error = &PairInfo->VirtualInfo->Error;
    VsnapVirtualVolInfo *VirtualInfo = PairInfo->VirtualInfo;
    
    //VsnapDebugPrint(PairInfo, SV_LOG_ERROR);

    assert(Error ->VsnapErrorStatus >= 0);
    if(Error->VsnapErrorStatus == 0)  
        return;

    VsnapGetErrorMsg(PairInfo->VirtualInfo->Error , msg);
    
    if(m_SVServer[0] != '\0' && SendToCx)
    {
        VirtualInfo->VsnapProgress = VSNAP_FAILED_STATUS;
        SendStatus(PairInfo, msg);
    }
}

bool VsnapMgr::SendStatus(VsnapPairInfo *PairInfo, string InfoForCx)
{
    string				timeAsString ;
    unsigned long long	uid = 0;
    VsnapVirtualVolInfo *VirtualInfo = PairInfo->VirtualInfo;
    VsnapSourceVolInfo	*SrcInfo = PairInfo->SrcInfo;
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s for the vsnap device %s\n",
        FUNCTION_NAME,VirtualInfo->VolumeName.c_str());
    if(m_SVServer[0] == '\0')
        return false;

    // PR #2257
    // CX does not understand the following states
    // If you send these state to cx, the state is interpreted
    // incorrectly as job completed and further states are not 
    // getting updated in the database
    if((PRE_SCRIPT_STARTS == VirtualInfo->VsnapProgress)       
        || ( PRE_SCRIPT_ENDS == VirtualInfo->VsnapProgress )        
        || ( POST_SCRIPT_STARTS == VirtualInfo->VsnapProgress ) 
        || (POST_SCRIPT_ENDS == VirtualInfo->VsnapProgress))
        return true;


    if ( ( MAP_START_STATUS == VirtualInfo->VsnapProgress) || ( VSNAP_COMPLETE_STATUS == VirtualInfo->VsnapProgress) || 
        ( MOUNT_START == VirtualInfo->VsnapProgress) || (MAP_PROGRESS_STATUS == VirtualInfo->VsnapProgress) || ( MOUNT_PROGRESS == VirtualInfo->VsnapProgress)|| (PRE_SCRIPT_STARTS == VirtualInfo->VsnapProgress) || ( PRE_SCRIPT_ENDS == VirtualInfo->VsnapProgress ) || ( POST_SCRIPT_STARTS == VirtualInfo->VsnapProgress ) ||(POST_SCRIPT_ENDS == VirtualInfo->VsnapProgress) ) 
    {
        char buffer[65];	

#ifdef SV_WINDOWS
        SYSTEMTIME systemTime ;
        GetSystemTime(&systemTime);
		inm_sprintf_s(buffer, ARRAYSIZE(buffer), "%d\\%d\\%d %d:%d:%d:%d",
            systemTime.wYear, systemTime.wMonth, systemTime.wDay,
            systemTime.wHour, systemTime.wMinute, systemTime.wSecond,
            systemTime.wMilliseconds);
#else //Linux portion
        ACE_TCHAR timebuf[26]; // This magic number is based on the ctime(3c) man page.
        ACE_Time_Value cur_time = ACE_OS::gettimeofday ();
        time_t secs = cur_time.sec ();

        ACE_OS::ctime_r (&secs, timebuf, sizeof timebuf);
#endif


        timeAsString = buffer ;	
    }
    int time=atoi(timeAsString.c_str());

    SV_ULONGLONG snapId=(VirtualInfo->SnapShotId);

    // Notifications are disabled upon encountering first exception
    // in contacting Cx. They are not enabled further.

    if (IsConfigAvailable == true && notifyCx)
    {
        notifyCx = notifyCxOnSnapshotStatus((*m_TheConfigurator), SrcInfo->vsnap_id,time,snapId,InfoForCx.c_str(),VirtualInfo->VsnapProgress);
    }

    DebugPrintf(SV_LOG_INFO, "%s\n", InfoForCx.c_str());
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s for the vsnap device %s\n",
        FUNCTION_NAME,VirtualInfo->VolumeName.c_str());
    return notifyCx;


}

bool VsnapMgr::isSourceFullDevice(const string & deviceName)
{
    bool rv = false;
    if (IsConfigAvailable == true)
        rv = m_TheConfigurator->isSourceFullDevice(deviceName);
    if(rv)
        DebugPrintf(SV_LOG_DEBUG, "Source of target device %s is a full device\n", deviceName.c_str());
    else
        DebugPrintf(SV_LOG_DEBUG, "Source target device %s is not a full device\n",deviceName.c_str());
    return rv;
}

OS_VAL VsnapMgr::getSourceOSVal(const string & deviceName)
{
    OS_VAL os_val = OS_UNKNOWN;
    if (IsConfigAvailable == true)
        os_val = m_TheConfigurator->getSourceOSVal(deviceName);
    DebugPrintf(SV_LOG_DEBUG, "GetSourceOSVal returning: %d\n",os_val);
    return os_val;
}

SV_ULONG VsnapMgr::getOtherSiteCompartibilityNumber(const string & deviceName)
{
    SV_ULONG compartibilitynum = 0;
    if (IsConfigAvailable == true)
        compartibilitynum = m_TheConfigurator->getOtherSiteCompartibilityNumber(deviceName);
    DebugPrintf(SV_LOG_DEBUG, "compartibilitynum returning: %ul\n",compartibilitynum);
    return compartibilitynum;
}

void VsnapMgr::SendProgressToCx(VsnapPairInfo *PairInfo, const SV_ULONG & PercentComplete, const SV_LONGLONG & time_remaining_in_secs)
{
	VsnapVirtualVolInfo *VirtualInfo = PairInfo->VirtualInfo;
	VsnapSourceVolInfo	*SrcInfo = PairInfo->SrcInfo;


	string snapId=SrcInfo->vsnap_id;

	// Notifications are disabled upon encountering first exception
	// in contacting Cx. They are not enabled further.

	if (IsConfigAvailable == true && notifyCx && (m_SVServer[0] != '\0') )
	{
		notifyCx = notifyCxOnSnapshotProgress((*m_TheConfigurator), snapId,PercentComplete,0);
	}
	if(PercentComplete && !(PercentComplete % 10))
	{
		if(time_remaining_in_secs > 0)
		{
			std::string display_message;
			SV_LONGLONG time_remaining = time_remaining_in_secs;
			SV_LONGLONG time_remaining_in_hr = 0;
			SV_LONGLONG time_remaining_in_min = 0;
			SV_LONGLONG time_remaining_in_sec = 0;

			if(time_remaining > 3600)
			{
				time_remaining_in_hr = time_remaining/3600;
				time_remaining = time_remaining % 3600;
			}

			if (time_remaining >= 60)
			{
				time_remaining_in_min =  time_remaining/60;
				time_remaining = time_remaining % 60;
			}

			if (time_remaining >= 1)
			{
				time_remaining_in_sec = time_remaining;
			}

			if(VirtualInfo->VolumeName.empty())
			{
				DebugPrintf(SV_LOG_INFO, "%s\t%d%% (" LLSPEC "hr " LLSPEC "min " LLSPEC "sec remaining\n", 
					VirtualInfo->DeviceName.c_str(), PercentComplete, time_remaining_in_hr,time_remaining_in_min,time_remaining_in_sec);
			}
			else
			{
				DebugPrintf(SV_LOG_INFO, "%s\t%d%% (" LLSPEC "hr " LLSPEC "min " LLSPEC "sec remaining\n", 
					VirtualInfo->VolumeName.c_str(), PercentComplete, time_remaining_in_hr,time_remaining_in_min,time_remaining_in_sec);
			}

		} else
		{
			if(VirtualInfo->VolumeName.empty())
			{
				DebugPrintf(SV_LOG_INFO, "%s\t%d%%\n", 
					VirtualInfo->DeviceName.c_str(), PercentComplete);
			}
			else
			{
				DebugPrintf(SV_LOG_INFO, "%s\t%d%%\n", 
					VirtualInfo->VolumeName.c_str(), PercentComplete);
			}
		}


	}
}

bool VsnapMgr::CreateFileIdTableFile(const string & MapFileDir, VsnapPairInfo *PairInfo)
{
    char FullFileNamePath[2048];
    ACE_HANDLE h = ACE_INVALID_HANDLE;

    VsnapVirtualVolInfo	*VirtualInfo = PairInfo->VirtualInfo;
    VsnapErrorInfo		*Error = &VirtualInfo->Error;
    inm_sprintf_s(FullFileNamePath, ARRAYSIZE(FullFileNamePath), "%s%c" ULLSPEC "_FileIdTable", MapFileDir.c_str(), ACE_DIRECTORY_SEPARATOR_CHAR_A, VirtualInfo->SnapShotId);

    // PR#10815: Long Path support
    h = ACE_OS::open(getLongPathName(FullFileNamePath).c_str(), O_RDWR | O_CREAT, FILE_SHARE_READ);
    if( h == ACE_INVALID_HANDLE)
    {
        Error->VsnapErrorStatus = VSNAP_FILETBL_OP_FAILED;
        Error->VsnapErrorCode = ACE_OS::last_error();
        return false;
    }

    ACE_OS::close(h);

    return true;
}


bool VsnapMgr::updateFileIdTableFile(string MapFileDir, VsnapPairInfo *PairInfo, std::vector<std::string>& filenames)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    SV_UINT				BytesWritten = 0;
    char				FullFileNamePath[2048];
    ACE_HANDLE			FileIdTableHandle;
    FileIdTableHandle= ACE_INVALID_HANDLE;
    bool				status = true;
    VsnapVirtualVolInfo	*VirtualInfo = PairInfo->VirtualInfo;
    VsnapSourceVolInfo	*SrcInfo = PairInfo->SrcInfo;
    VsnapErrorInfo		*Error = &VirtualInfo->Error;

    do
    {
        inm_sprintf_s(FullFileNamePath, ARRAYSIZE(FullFileNamePath), "%s%c" ULLSPEC "_FileIdTable", MapFileDir.c_str(), ACE_DIRECTORY_SEPARATOR_CHAR_A, VirtualInfo->SnapShotId);
#ifdef SV_WINDOWS
        if(!OPEN_FILE(FullFileNamePath, O_RDWR | O_CREAT, FILE_SHARE_READ, &FileIdTableHandle))
#else
        if(!OPEN_FILE(FullFileNamePath, O_RDWR | O_CREAT, 0644, &FileIdTableHandle))
#endif
        {
            Error->VsnapErrorStatus = VSNAP_FILETBL_OP_FAILED;
            Error->VsnapErrorCode = ACE_OS::last_error();
            DebugPrintf(SV_LOG_ERROR, "Failed to open file %s, error no: %d\n",
                FullFileNamePath, ACE_OS::last_error());
            status = false;
            break;
        }


#ifdef SV_WINDOWS
        LARGE_INTEGER DistanceToMove, DistanceMoved;
        DistanceToMove.QuadPart = 0;
        //DistanceMoved = ACE_OS::llseek(FileIdTableHandle,DistanceToMove.QuadPart,FILE_END);
        SetFilePointerEx(FileIdTableHandle, DistanceToMove, &DistanceMoved, FILE_END);
#else
        SV_ULONGLONG DistanceToMove, DistanceMoved;
        DistanceToMove = 0;
        if(!SEEK_FILE(FileIdTableHandle, DistanceToMove, &DistanceMoved, SEEK_END)) {
            Error->VsnapErrorStatus = VSNAP_FILETBL_OP_FAILED;
            Error->VsnapErrorCode = ACE_OS::last_error();
            DebugPrintf(SV_LOG_ERROR, "Seek failed for the file %s, error no: %d\n",
                FullFileNamePath, ACE_OS::last_error());
            status = false;
            break;
        }
#endif

        boost::shared_array<char> buffer;
        size_t bufferlen = 0;
        INM_SAFE_ARITHMETIC(bufferlen = InmSafeInt<size_t>::Type(filenames.size()) * sizeof(VsnapFileIdTable), INMAGE_EX(filenames.size())(sizeof(VsnapFileIdTable)))
        buffer.reset(new char[bufferlen]);
        memset(buffer.get(), 0, bufferlen);
        std::vector<std::string>::const_iterator iter = filenames.begin();
        size_t currpos = 0, currlen = 0;
        for( int entry = 0; iter != filenames.end(); iter++, entry++)
        {
            assert(iter->length() < 50);
            currpos = entry * sizeof(VsnapFileIdTable);
            currlen = bufferlen - currpos;
            inm_strcpy_s(buffer.get() + currpos, currlen, iter->c_str());
        }
#ifdef SV_WINDOWS
        // TODO: How does this work in Windows?
        if(!WRITE_FILE(FileIdTableHandle, buffer.get(), DistanceMoved.QuadPart, bufferlen, (int *)&BytesWritten))
#else
        if (!WRITE_FILE(FileIdTableHandle, buffer.get(), DistanceMoved, bufferlen, (int *)&BytesWritten))
#endif
        {
            Error->VsnapErrorStatus = VSNAP_FILETBL_OP_FAILED;
            Error->VsnapErrorCode = ACE_OS::last_error();
            DebugPrintf(SV_LOG_ERROR, "Failed to write to file %s, error no: %d\n",
                FullFileNamePath, ACE_OS::last_error());
            status = false;
            break;
        }
    } while (FALSE);

    if(ACE_INVALID_HANDLE != FileIdTableHandle)
        ACE_OS::close(FileIdTableHandle);

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return status;
}

bool VsnapMgr::InsertFSData(string MapFileDir, VsnapPairInfo *PairInfo,std::vector<std::string>&filenames, unsigned int& fileIdsWritten)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    VsnapFileIdTable		FileTable;
    char					FullFileNamePath[2048];
    SV_UINT				BytesWritten = 0;
    ACE_HANDLE				FsDataFileHandle = ACE_INVALID_HANDLE;
    bool					status = true;
    VsnapVirtualVolInfo		*VirtualInfo = PairInfo->VirtualInfo;
    VsnapSourceVolInfo		*SrcInfo = PairInfo->SrcInfo;
    VsnapErrorInfo			*Error = &VirtualInfo->Error;


    SV_ULONGLONG			DistanceToMove;
    memset(FileTable.FileName, 0, sizeof(FileTable.FileName));

	inm_sprintf_s(FileTable.FileName, ARRAYSIZE(FileTable.FileName), ULLSPEC "_FSData", VirtualInfo->SnapShotId);
    filenames.push_back(FileTable.FileName);
    if(!updateFileIdTableFile(MapFileDir, PairInfo, filenames)) 
    {
        return false ;  
    }
    fileIdsWritten += filenames.size();
    filenames.clear();

    do
    {
        inm_sprintf_s(FullFileNamePath, ARRAYSIZE(FullFileNamePath), "%s%c" ULLSPEC "_FSData", MapFileDir.c_str(), ACE_DIRECTORY_SEPARATOR_CHAR_A, VirtualInfo->SnapShotId);
#ifdef SV_WINDOWS
        if(!OPEN_FILE(FullFileNamePath, O_RDWR | O_CREAT, FILE_SHARE_READ, &FsDataFileHandle))
#else
        if(!OPEN_FILE(FullFileNamePath, O_RDWR | O_CREAT, 0644, &FsDataFileHandle))
#endif
        {
            Error->VsnapErrorStatus = VSNAP_FILETBL_OP_FAILED;
            Error->VsnapErrorCode = ACE_OS::last_error();
            DebugPrintf(SV_LOG_ERROR, "Failed to open file %s, error no:%d\n",
                FullFileNamePath, ACE_OS::last_error());
            status = false;
            break;
        }

        DistanceToMove = 0;
        ACE_OS::llseek(FsDataFileHandle,DistanceToMove,FILE_END);
        //SetFilePointerEx(FsDataFileHandle, DistanceToMove, &DistanceMoved, FILE_END);

        if(!WRITE_FILE(FsDataFileHandle, &SrcInfo->FsData[0], 0, VSNAP_FS_DATA_SIZE, (int *)&BytesWritten))
        {
            Error->VsnapErrorStatus = VSNAP_FILETBL_OP_FAILED;
            Error->VsnapErrorCode = ACE_OS::last_error();
            DebugPrintf(SV_LOG_ERROR, "Failed writing to file %s, error no:%d\n",
                FullFileNamePath, ACE_OS::last_error());
            status = false;
            break;
        }
    } while (FALSE);

    if(ACE_INVALID_HANDLE != FsDataFileHandle)
        ACE_OS::close(FsDataFileHandle);

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return status;
}

bool VsnapMgr::InsertVsnapWriteData(const string & MapFileDir, VsnapPairInfo *PairInfo,
                                    std::vector<std::string>&filenames,SV_ULONGLONG size,
                                    unsigned int& fileIdsWritten)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    VsnapFileIdTable		FileTable;
    char					FullFileNamePath[2048];
    bool					status = true;
    VsnapVirtualVolInfo		*VirtualInfo = PairInfo->VirtualInfo;
    VsnapSourceVolInfo		*SrcInfo = PairInfo->SrcInfo;
    VsnapErrorInfo			*Error = &VirtualInfo->Error;
    do
    {
        ACE_HANDLE hHandle = ACE_INVALID_HANDLE;
        inm_sprintf_s(FullFileNamePath, ARRAYSIZE(FullFileNamePath), "%s%c" ULLSPEC "_WriteData1", MapFileDir.c_str(), ACE_DIRECTORY_SEPARATOR_CHAR_A, VirtualInfo->SnapShotId);
        hHandle = ACE_OS::open(getLongPathName(FullFileNamePath).c_str(),O_WRONLY | O_CREAT | O_EXCL,FILE_SHARE_WRITE );
        if(hHandle==ACE_INVALID_HANDLE)
        {
            Error->VsnapErrorStatus = VSNAP_FILETBL_OP_FAILED;
            Error->VsnapErrorCode = ACE_OS::last_error();
            DebugPrintf(SV_LOG_INFO,"write data File creation failed ERROR: %d\n",ACE_OS::last_error());
            status = false;
            break;
        }
        if( ACE_OS::ftruncate(hHandle,(ACE_OFF_T) size) == -1)
        {
            Error->VsnapErrorStatus = VSNAP_FILETBL_OP_FAILED;
            Error->VsnapErrorCode = ACE_OS::last_error();
            DebugPrintf(SV_LOG_INFO,"Seeking a sparse file failed: %d\n",ACE_OS::last_error());
            ACE_OS::close(hHandle);
            status = false;
            break;
        }
        ACE_OS::close(hHandle);
        memset(FileTable.FileName, 0, sizeof(FileTable.FileName));
		inm_sprintf_s(FileTable.FileName, ARRAYSIZE(FileTable.FileName), ULLSPEC "_WriteData1", VirtualInfo->SnapShotId);
        filenames.push_back(FileTable.FileName);
        if(!updateFileIdTableFile(MapFileDir, PairInfo, filenames)) 
        {			
            status = false;
            break;
        }
        fileIdsWritten += filenames.size();
        filenames.clear();

    }while(false);
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return status;
}

void VsnapMgr::TruncateTrailingBackSlash(char *str)
{
    size_t Length = strlen(str);

    Length--;
    while (((str[Length]=='\\') && Length)  || ((str[Length]=='/') && Length))
    {
        str[Length] = '\0';
        Length--;
    }

    return ; 
}

bool VsnapMgr::RecoverPartiallyAppliedChanges(CDPDatabase & db, SV_UINT& cdpversion, unsigned int & FileId, std::string const & MapFileDir,
                                              VsnapPairInfo * PairInfo, VsnapFileIdTable & FileTable, 
                                              DiskBtree* & VsnapBtree,
                                              std::vector<std::string> &filenames,
                                              SV_UINT& BackwardFileId,
                                              SV_ULONGLONG& lastHourTimeStamp,
                                              unsigned int& fileIdsWritten)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    bool rv = true;

    try
    {
        do 
        {

            SVERROR sv = SVS_OK;
            cdp_rollback_drtd_t rollback_drtd;
            std::string cdpDataFile;
            std::vector<std::string>::const_iterator fiter = filenames.end();
            CDPDatabaseImpl::Ptr dbptr = db.FetchAppliedDRTDs();

            if(!dbptr)
            {
                DebugPrintf(SV_LOG_ERROR, "FUNCTION:%s failed to intialize db ptr.\n",
                    FUNCTION_NAME);
                rv = false;
                break;
            }

            VsnapVirtualVolInfo *VirtualInfo = PairInfo->VirtualInfo;
            VsnapErrorInfo *Error = &VirtualInfo->Error;

            while((sv = dbptr->read(rollback_drtd)) == SVS_OK)
            {
                if(rollback_drtd.filename.empty())
                {
                    DebugPrintf(SV_LOG_ERROR, "FUNCTION:%s recieved empty filename.\n",
                        FUNCTION_NAME);
                    rv = false;
                    break;
                }

                if(cdpDataFile != rollback_drtd.filename)
                {
                    if(cdpversion == 3)
                    {
                        std::string lastfileEndtimeAndTag;
                        if(!cdpDataFile.empty())
                        {
                            lastfileEndtimeAndTag = cdpDataFile.substr(7, 12);
                        }
                        std::string drtdEndtimeAndTag = rollback_drtd.filename.substr(7, 12);
                        if(lastfileEndtimeAndTag != drtdEndtimeAndTag)
                        {
                            if(!updateFileIdTableFile(MapFileDir, PairInfo, filenames))
                            {
                                DebugPrintf(SV_LOG_ERROR, "Failed to add filenames to file table.\n");
                                rv = false;
                                break;
                            }
                            fileIdsWritten += filenames.size();
                            filenames.clear();
                        }
                        cdpDataFile = rollback_drtd.filename;
                        fiter = find(filenames.begin(),filenames.end(),cdpDataFile);
                        if(fiter == filenames.end())
                        {
                            filenames.push_back(cdpDataFile);
                            FileId = filenames.size() + fileIdsWritten;

                            SV_ULONGLONG drtdTimeStampInHours = 0;
                            if(!CDPUtil::ToTimestampinHrs(rollback_drtd.timestamp, drtdTimeStampInHours))
                            {
                                DebugPrintf(SV_LOG_ERROR, "Failed to get drtd time stamp.\n");
                                rv = false;
                                break;
                            }

                            if(!lastHourTimeStamp)
                            {
                                lastHourTimeStamp = drtdTimeStampInHours;
                            }

                            if(drtdTimeStampInHours == lastHourTimeStamp)
                            {
                                BackwardFileId++;
                            }
                        }else
                        {
                            FileId = fiter - filenames.begin() + 1 + fileIdsWritten;
                        }
                    }
                    else if(cdpversion == 1)
                    {
                        cdpDataFile = rollback_drtd.filename;
                        filenames.push_back(cdpDataFile);
                        FileId++;
                        if(filenames.size() > FILEID_VECTOR_MAX_SIZE)
                        {
                            if(!updateFileIdTableFile(MapFileDir, PairInfo, filenames))
                            {
                                DebugPrintf(SV_LOG_ERROR, "Failed to add filenames to file table.\n");
                                rv = false;
                                break;
                            }
                            fileIdsWritten += filenames.size();
                            filenames.clear();
                        }
                    }
                }

                SVD_DIRTY_BLOCK_INFO	Dblock;
                Dblock.DirtyBlock.ByteOffset = rollback_drtd.voloffset;
                Dblock.DirtyBlock.Length = rollback_drtd.length;
                Dblock.DataFileOffset = rollback_drtd.fileoffset;
                if(!UpdateMap(VsnapBtree, FileId, &Dblock))
                {
                    Error->VsnapErrorStatus = VSNAP_MAP_OP_FAILED;			
                    Error->VsnapErrorCode = ACE_OS::last_error();
                    rv = false;
                    break;
                }
            }

            if(sv.failed())
            {
                DebugPrintf(SV_LOG_ERROR,"FUNCTION:%s read from db failed.\n",
                    FUNCTION_NAME);
                rv = false;
                break;
            }

        } while (FALSE);
    }
    catch (std::exception & ex)
    {
        DebugPrintf(SV_LOG_ERROR, "Encountered error while rolling back partially applied changes."
            "Caught exception %s\n", ex.what());
        rv = false;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return rv;
}


bool VsnapMgr::ParseCDPDatabase(string & MapFileDir, VsnapPairInfo *PairInfo)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    bool					success = true;
    unsigned int			FileId = 0;
    unsigned int			fileIdsWritten = 0;
	const unsigned int		maxWriteLimit = 4294967295;
    SV_UINT					BackwardFileId = 0;
    SV_ULONGLONG			lastHourTimeStamp = 0;
    VsnapFileIdTable		FileTable;
    std::vector<char>		vMapFileFullPath(2048);
    VsnapVirtualVolInfo		*VirtualInfo = PairInfo->VirtualInfo;
    VsnapSourceVolInfo		*SrcInfo = PairInfo->SrcInfo;
    VsnapErrorInfo			*Error = &VirtualInfo->Error;
    SVD_DIRTY_BLOCK_INFO	Dblock;
    DiskBtree				*VsnapBtree = NULL;
    VsnapPersistentHeaderInfo VsnapHdrInfo;
    std::vector<std::string> filenames;
    std::vector<std::string>::const_iterator fiter = filenames.end();
    SV_UINT					cdpversion = 0;


    LocalConfigurator lc;
    bool simulatesparse = lc.SimulateSparse();

	ACE_Time_Value startTime = ACE_OS::gettimeofday(); 
	ACE_Time_Value presentTime = ACE_OS::gettimeofday();
	SV_LONGLONG time_remaining_in_secs = 0;

    memset(&VsnapHdrInfo, 0, sizeof(VsnapPersistentHeaderInfo));

    memset(FileTable.FileName, 0, sizeof(FileTable.FileName));

    do
    {
        CDPDatabase database(SrcInfo->RetentionDbPath);
        std::string retpath = SrcInfo->RetentionDbPath;
        if( (retpath.size() > CDPV3DBNAME.size()) &&
            (std::equal(retpath.begin() + retpath.size() - CDPV3DBNAME.size(), retpath.end(), CDPV3DBNAME.begin()))
            )
        {
            cdpversion = 3;
        }
        else if( (retpath.size() > CDPV1DBNAME.size()) &&
            (std::equal(retpath.begin() + retpath.size() - CDPV1DBNAME.size(), retpath.end(), CDPV1DBNAME.begin()))
            )
        {
            cdpversion = 1;
        }

        SV_ULONGLONG recoveryRangeStartTime = 0;
        SV_ULONGLONG recoveryRangeEndTime = 0;
        if(!database.getrecoveryrange(recoveryRangeStartTime, recoveryRangeEndTime))
        {
            Error->VsnapErrorStatus = VSNAP_DB_INIT_FAILED;
            Error->VsnapErrorCode = 0;//ACE_OS::last_error();
            success = false;
            break;
        }


        CDPDRTDsMatchingCndn cndn;
        cndn.toEvent(VirtualInfo->EventName);
        cndn.toTime(VirtualInfo->TimeToRecover);
        cndn.toSequenceId(VirtualInfo->SequenceId);

        CDPDatabaseImpl::Ptr dbptr = database.FetchDRTDs(cndn);

        VsnapPrint("Parsing Retention Logs and Generating the map. Please wait.\n");
        SendProgressToCx(PairInfo, 0,0);

        VsnapBtree = new DiskBtree;
        if(!VsnapBtree)
        {
            Error->VsnapErrorStatus = VSNAP_MEM_UNAVAILABLE;
            Error->VsnapErrorCode = ACE_OS::last_error();
            DebugPrintf(SV_LOG_ERROR, "Failed to instantiate DiskBtree\n");
            success = false;
            break;
        }

        VsnapBtree->Init(sizeof(VsnapKeyData));

        inm_sprintf_s(&vMapFileFullPath[0], vMapFileFullPath.size(), "%s%c" ULLSPEC "_VsnapMap", MapFileDir.c_str(), ACE_DIRECTORY_SEPARATOR_CHAR_A, VirtualInfo->SnapShotId);
        DebugPrintf("About to create the file[%s].\n", vMapFileFullPath.data());
#ifdef SV_WINDOWS
        if (!VsnapBtree->InitFromDiskFileName(vMapFileFullPath.data(), O_RDWR | O_CREAT, FILE_SHARE_READ))
#else
        if (!VsnapBtree->InitFromDiskFileName(vMapFileFullPath.data(), O_RDWR | O_CREAT, 0644))
#endif
        {
            Error->VsnapErrorStatus = VSNAP_MAP_INIT_FAILED;
            Error->VsnapErrorCode = ACE_OS::last_error();
            DebugPrintf("Failed in creation.\n");
            VsnapBtree->Uninit();
            success = false;
            break;
        }

        if(!CreateFileIdTableFile(MapFileDir, PairInfo))
        {
            Error->VsnapErrorStatus = VSNAP_FILETBL_OP_FAILED;
            Error->VsnapErrorCode = ACE_OS::last_error();
            DebugPrintf(SV_LOG_ERROR, "Failed to create %s%c" ULLSPEC "_FileIdTable", MapFileDir.c_str(), ACE_DIRECTORY_SEPARATOR_CHAR_A, VirtualInfo->SnapShotId);
            VsnapBtree->Uninit();
            success = false;
            break;
        }

        VsnapBtree->SetCallBack(&VsnapCallBack);

        if(!RecoverPartiallyAppliedChanges(database, cdpversion, FileId, MapFileDir, PairInfo, FileTable, VsnapBtree,filenames,BackwardFileId,lastHourTimeStamp,fileIdsWritten))
        {
            Error->VsnapErrorStatus = VSNAP_DB_PARSE_FAILED;
            Error->VsnapErrorCode = 0;//ACE_OS::last_error();
            VsnapBtree->Uninit();
            success = false;
            break;
        }

        SVERROR sv = SVS_OK;
        std::string filename;
        int percentage = 0;
        int newpercentage = 0;
        SV_ULONGLONG recovery_period = 0;
        SV_ULONGLONG recovered_period = 0;
        cdp_rollback_drtd_t rollback_drtd;
        std::string display_ts = "";

        recovery_period = recoveryRangeEndTime - VirtualInfo->TimeToRecover;
        while((sv = dbptr -> read(rollback_drtd)) == SVS_OK)
        {
            if(VsnapQuit())
            {
                success = false;
                break;
            }

            if(rollback_drtd.filename.empty())
            {
                Error->VsnapErrorStatus = VSNAP_DB_PARSE_FAILED;
                Error->VsnapErrorCode = 0;//ACE_OS::last_error();
                DebugPrintf(SV_LOG_ERROR, "FUNCTION:%s recieved empty filename.\n",
                    FUNCTION_NAME);
                success = false;
                break;
            }

            if(cdpversion == 3)
            {

				if(simulatesparse)
				{
					std::string filetoread = dirname_r(SrcInfo->RetentionDbPath.c_str());
					filetoread += ACE_DIRECTORY_SEPARATOR_CHAR_A;
					filetoread += rollback_drtd.filename;
					ACE_stat statbuf;
					if(0 != sv_stat(getLongPathName(filetoread.c_str()).c_str(),&statbuf))
					{
						DebugPrintf(SV_LOG_DEBUG,"%s is not available (coalesced file?)\n", rollback_drtd.filename.c_str());
						continue;
					}
				}
            }

            //To accomodate the volume resize
            //if we got a dirty block with starting volume offset zero,
            //then we are overwritting the SrcInfo->FsData
            //later on SrcInfo->FsData will be stored in FsData
            if(rollback_drtd.voloffset == 0)
            {
                if(!fill_fsdata_from_retention(rollback_drtd.filename,rollback_drtd.fileoffset,PairInfo))
                {
                    Error->VsnapErrorStatus = VSNAP_MAP_OP_FAILED;
                    Error->VsnapErrorCode = ACE_OS::last_error();
                    success = false;
                    break;
                }
            }

            if(filename != rollback_drtd.filename)
            {
                if(cdpversion == 3)
                {

                    // cdpv3 data filename format is 
                    // cdpv30_<end time><end bookmark>_<file type(1 - overlap/ 0 - first write)>_
                    //        <start time><start bookmark>_<sequence no>.dat
                    // here endtime+tag is 12 chars long, starting from 7th pos
                    std::string lastfileEndtimeAndTag;
                    if(!filename.empty())
                    {
                        lastfileEndtimeAndTag = filename.substr(7, 12);
                    }
                    std::string drtdEndtimeAndTag = rollback_drtd.filename.substr(7, 12);

                    if(lastfileEndtimeAndTag != drtdEndtimeAndTag)
                    {
                        if(!updateFileIdTableFile(MapFileDir, PairInfo, filenames))
                        {
                            DebugPrintf(SV_LOG_ERROR, "Failed to add filenames to file table.\n");
                            Error->VsnapErrorStatus = VSNAP_MAP_OP_FAILED;
                            Error->VsnapErrorCode = ACE_OS::last_error();
                            success = false;
                            break;
                        }
                        fileIdsWritten += filenames.size();
                        filenames.clear();
                    }

                    filename = rollback_drtd.filename;
                    fiter = find(filenames.begin(),filenames.end(),filename);
                    if(fiter == filenames.end())
                    {
                        filenames.push_back(filename);
                        FileId = filenames.size() + fileIdsWritten;

                        SV_ULONGLONG drtdTimeStampInHours = 0;
                        if(!CDPUtil::ToTimestampinHrs(rollback_drtd.timestamp, drtdTimeStampInHours))
                        {
                            DebugPrintf(SV_LOG_ERROR, "Failed to get drtd time stamp.\n");
                            Error->VsnapErrorStatus = VSNAP_MAP_OP_FAILED;
                            Error->VsnapErrorCode = ACE_OS::last_error();
                            success = false;
                            break;
                        }

                        if(!lastHourTimeStamp)
                        {
                            lastHourTimeStamp = drtdTimeStampInHours;
                        }

                        if(drtdTimeStampInHours == lastHourTimeStamp)
                        {
                            BackwardFileId++;
                        }
                    }else
                    {
                        FileId = fiter - filenames.begin() + 1 + fileIdsWritten;
                    }
                }
                else if(cdpversion == 1)
                {
                    filename = rollback_drtd.filename;
                    filenames.push_back(filename);
                    FileId++;
                    if(filenames.size() > FILEID_VECTOR_MAX_SIZE)
                    {
                        if(!updateFileIdTableFile(MapFileDir, PairInfo, filenames))
                        {
                            DebugPrintf(SV_LOG_ERROR, "Failed to add filenames to file table.\n");
                            Error->VsnapErrorStatus = VSNAP_MAP_OP_FAILED;
                            Error->VsnapErrorCode = ACE_OS::last_error();
                            success = false;
                            break;
                        }
                        fileIdsWritten += filenames.size();
                        filenames.clear();
                    }
                }
            }

            Dblock.DirtyBlock.ByteOffset = rollback_drtd.voloffset;
            Dblock.DirtyBlock.Length = rollback_drtd.length;
            Dblock.DataFileOffset = rollback_drtd.fileoffset;

            CDPUtil::ToDisplayTimeOnConsole(rollback_drtd.timestamp, display_ts);
            DebugPrintf(SV_LOG_DEBUG, "%s" " %d " LLSPEC " " LLSPEC " %s\n",
                rollback_drtd.filename.c_str(),
                rollback_drtd.length,
                rollback_drtd.voloffset,
                rollback_drtd.fileoffset,
                display_ts.c_str());

            bool rv = UpdateMap(VsnapBtree, FileId, &Dblock);
            if(!rv)
            {
                Error->VsnapErrorStatus = VSNAP_MAP_OP_FAILED;			
                Error->VsnapErrorCode = ACE_OS::last_error();
                success = false;
                break;
            }

            recovered_period =  recoveryRangeEndTime - rollback_drtd.timestamp;
            if(recovery_period)
            {
                newpercentage  = (SV_ULONG)(recovered_period * 100 / recovery_period);
            } else
            {
                newpercentage = 100;
            }
            if(newpercentage &&(percentage != newpercentage))
            {
                percentage = newpercentage;
                if(0 == percentage % 5)
                {
                    presentTime = ACE_OS::gettimeofday();
					if (presentTime - startTime.sec())
						time_remaining_in_secs = (((presentTime - startTime).sec() * 100) / percentage) - (presentTime - startTime).sec();

					SendProgressToCx(PairInfo, percentage,time_remaining_in_secs);
                }
            }
        }

        if(!filenames.empty())
        {
            if(!updateFileIdTableFile(MapFileDir, PairInfo, filenames))
            {
                DebugPrintf(SV_LOG_ERROR, "Failed to add filenames to file table.\n");
                Error->VsnapErrorStatus = VSNAP_MAP_OP_FAILED;
                Error->VsnapErrorCode = ACE_OS::last_error();
                success = false;
                break;
            }
            fileIdsWritten += filenames.size();
            filenames.clear();
        }

        if(VsnapQuit())
        {
            DebugPrintf(SV_LOG_ERROR, "\nAborting Vsnap creation upon request\n");
            VsnapBtree->Uninit();
            success = false;
            break;
        }

        if(sv.failed())
        {
            Error->VsnapErrorStatus = VSNAP_DB_PARSE_FAILED;
            Error->VsnapErrorCode = 0;//ACE_OS::last_error();
            success = false;
            break;
        } else if((sv == SVS_FALSE) && success)
        {
            percentage = 100;
        }

        if(!success)
        {
            VsnapBtree->Uninit();
            break;
        }

        if((SrcInfo->FsType == VSNAP_FS_NTFS) 
			|| (SrcInfo->FsType == VSNAP_FS_FAT)
			|| (SrcInfo->FsType == VSNAP_FS_ReFS)
			|| (SrcInfo->FsType == VSNAP_FS_EXFAT))
        {
            if(InsertFSData(MapFileDir, PairInfo,filenames, fileIdsWritten))
            {
                Dblock.DataFileOffset = 0;
                Dblock.DirtyBlock.ByteOffset = 0;
                FileId = filenames.size() + fileIdsWritten;
                VirtualInfo->NextFileId = FileId + 1;
                Dblock.DirtyBlock.Length = VSNAP_FS_DATA_SIZE;
                bool rv = UpdateMap(VsnapBtree, ( FileId | VSNAP_RETENTION_FILEID_MASK ), &Dblock);
                if(!rv)
                {
                    Error->VsnapErrorStatus = VSNAP_MAP_OP_FAILED;			
                    Error->VsnapErrorCode = ACE_OS::last_error();
                    success = false;
                    VsnapBtree->Uninit();
                    break;
                }
            }
            else
            {
                VsnapErrorPrint(PairInfo);
                success = false;
            }

        }

        SV_LONGLONG difference = 0;
		SV_LONGLONG bytesprocessed = 0;
        if( SrcInfo ->ActualSize  >=  SrcInfo ->UserVisibleSize)
        {
            difference = (SrcInfo ->ActualSize - SrcInfo ->UserVisibleSize);
			
			//
			// scenario where the difference in the raw size vs fs size
			// runs in gbs, we end up requiring the corresponding disk space
			// for vsnap private data directory. see PR#23386  and PR#23456
			// for details.
			// to address this issue, (for now) we are bounding the difference
			// through config variable in drscout.conf
			//
			SV_LONGLONG MaxDifference = lc.MaxDifferenceBetweenFSandRawSize();
			if(MaxDifference && (difference >  MaxDifference))
			{
				SrcInfo ->ActualSize = SrcInfo ->UserVisibleSize + MaxDifference;
				difference = MaxDifference;
			}

			if(InsertVsnapWriteData(MapFileDir, PairInfo,filenames,difference, fileIdsWritten))
			{
				VirtualInfo->NextFileId = FileId + 1;
				FileId = filenames.size() + fileIdsWritten;
				while(difference) //when difference is more than 4 GB,split it
				{
					unsigned int writelength = difference >maxWriteLimit ? maxWriteLimit : difference;
					Dblock.DirtyBlock.ByteOffset = SrcInfo->UserVisibleSize + bytesprocessed;
					Dblock.DirtyBlock.Length = writelength;
					Dblock.DataFileOffset = bytesprocessed;
					bool rv = UpdateMap(VsnapBtree, (FileId | VSNAP_RETENTION_FILEID_MASK), &Dblock);
					if(!rv)
					{
						Error->VsnapErrorStatus = VSNAP_MAP_OP_FAILED;			
						Error->VsnapErrorCode = ACE_OS::last_error();
						success = false;
						break;
					}
					difference -= writelength;
					bytesprocessed += writelength;
				}
			}
			else
			{
				VsnapErrorPrint(PairInfo);
				success = false;
				break;
			}
			if(success == false)
			{
				break;
			}	
        }


        assert(filenames.empty());
        FileId = fileIdsWritten;
        VsnapHdrInfo.DataFileId = FileId + 1;
        VsnapHdrInfo.WriteFileId = FileId;
        VsnapHdrInfo.WriteFileSuffix = 1;
        VsnapHdrInfo.VsnapTime=PairInfo->VirtualInfo->TimeToRecover; //Modified for PIT VSNAP
        VsnapHdrInfo.BackwardFileId = BackwardFileId;
        VsnapHdrInfo.ForwardFileId = FileId + 1;

        if(!VsnapUpdatePersistentHdrInfo(VsnapBtree, &VsnapHdrInfo)) 
        {
            Error->VsnapErrorStatus = VSNAP_MAP_OP_FAILED;
            Error->VsnapErrorCode = ACE_OS::last_error();
            success = false;
            break ;
        }

   //     VsnapBtree->Uninit();

        //VsnapPrint("Map Generation, Percentage Completed: 100%\r");
        //VsnapPrint("                                                     \r");
        SendProgressToCx(PairInfo, 100,0);

    } while (FALSE);

    if (VsnapBtree)  
    {
		VsnapBtree->Uninit();
        delete VsnapBtree; 
        VsnapBtree = NULL ;
    }

    if(!success)
    {
        boost::format formatIter("%1%%2%%3%");
        formatIter % MapFileDir % ACE_DIRECTORY_SEPARATOR_CHAR_A % VirtualInfo->SnapShotId;
        SVUnlink( formatIter.str() + "_VsnapMap");
        SVUnlink( formatIter.str() + "_FileIdTable");	
    }


    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return success;
}

/*
* FUNCTION NAME : GetUniqueVsnapId
*
* DESCRIPTION : Helper routine to get a unique number for creating vsnap devicename
*
* INPUT PARAMETERS : None
*
* OUTPUT PARAMETERS :  None
*
* NOTES : 
*
* return value : Unique number 
*
*/
unsigned long long VsnapMgr::GetUniqueVsnapId()
{
    unsigned long long vsnapId = 0;

    if(!IncrementAndGetVsnapId(vsnapId))
    {
        return vsnapId;
    }

    return vsnapId;
}


void VsnapMgr::CleanupFiles(VsnapPairInfo *PairInfo)
{
    VsnapVirtualVolInfo *VirtualInfo = PairInfo->VirtualInfo;
    VsnapSourceVolInfo	*SrcInfo = PairInfo->SrcInfo;

    char filename[2048];
    inm_sprintf_s(filename, ARRAYSIZE(filename), "%s%c" ULLSPEC "_VsnapMap", SrcInfo->RetentionDbPath.c_str(), ACE_DIRECTORY_SEPARATOR_CHAR_A, VirtualInfo->SnapShotId);
    // PR#10815: Long Path support
    ACE_OS::unlink(getLongPathName(filename).c_str());

    inm_sprintf_s(filename, ARRAYSIZE(filename), "%s%c" ULLSPEC "_FileIdTable", SrcInfo->RetentionDbPath.c_str(), ACE_DIRECTORY_SEPARATOR_CHAR_A, VirtualInfo->SnapShotId);
    ACE_OS::unlink(getLongPathName(filename).c_str());

    inm_sprintf_s(filename, ARRAYSIZE(filename), "%s%c" ULLSPEC "_FSData", SrcInfo->RetentionDbPath.c_str(), ACE_DIRECTORY_SEPARATOR_CHAR_A, VirtualInfo->SnapShotId);
    ACE_OS::unlink(getLongPathName(filename).c_str());

    int i=1;
    inm_sprintf_s(filename, ARRAYSIZE(filename), "%s%c" ULLSPEC "_WriteData%d", SrcInfo->RetentionDbPath.c_str(), ACE_DIRECTORY_SEPARATOR_CHAR_A, VirtualInfo->SnapShotId, i);
    ACE_OS::unlink(getLongPathName(filename).c_str());

}


void VsnapMgr::UnmountDrive(char *SnapshotDrive)
{
    if(strlen(SnapshotDrive) > 3)
        return;
}

/*
* FUNCTION NAME: UnmountAllVirtualVolumes
*
* DESCRIPTION: calls UnMount() on each vsnap device. Returns the list of passed and failed vsnaps
* 
* INPUT PARAMETERS:  deletemapfiles: true indicates map files should be deleted while unmounting.
*
* OUTPUT PARAMETERS: passedvsnaps: contains the list of passed vsnaps.
*                    failedvsnaps: contains the list of failed vsnaps.
*                                    This argument is added for fixing bug #5569.
*
* RETURN VALUE: void
*
*/
void VsnapMgr::UnmountAllVirtualVolumes(std::vector<VsnapPersistInfo>& passedvsnaps, std::vector<VsnapPersistInfo>& failedvsnaps, bool deletemapfiles,bool bypassdriver)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n",FUNCTION_NAME);
    if(!IsVsnapDriverAvailable())
        return;
    std::vector<VsnapPersistInfo> allvsnaps;
    if(!RetrieveVsnapInfo(allvsnaps, "all"))
    {
        DebugPrintf(SV_LOG_ERROR, "Unable to retrive vsnap information from persistent store\n");
        return;
    }

    std::vector<VsnapPersistInfo>::const_iterator iter = allvsnaps.begin();

    for(; iter != allvsnaps.end(); ++iter)
    {
        bool unmountflag = false;
        std::string devName = iter->devicename;
        if(isSourceFullDevice(iter->target))
            devName += "s2";
        if(IsFileORVolumeExisting(iter->devicename))
        {
            unmountflag = true;
        }
#ifdef SV_SUN
        else if(IsFileORVolumeExisting(devName))
        {
            unmountflag = true;
        }	
#endif
#ifdef SV_AIX
        struct stat stbuf;
        if(lstat(iter->devicename.c_str(),&stbuf) == 0)
        {
            unmountflag = true;
        }	
#endif
        if(unmountflag || bypassdriver)
        {
            VsnapVirtualInfoList VirtualList;
            VsnapVirtualVolInfo VirtualInfo;
            VirtualInfo.VolumeName = iter->devicename;
            VirtualInfo.State = VSNAP_UNMOUNT;
            VirtualList.push_back(&VirtualInfo);
            if(Unmount(VirtualList, deletemapfiles,true,bypassdriver))
            {
                passedvsnaps.push_back(*iter);
            }
            else
            {
                //added for fixing bug #5569. used for printing summary info.
                failedvsnaps.push_back(*iter);
            }
        }
        else if(deletemapfiles)
        {
			if(!DeleteVsnapLogs(*iter))
			{
				std::ostringstream errstr;
				errstr << "Unable to delete vsnap logs for " << (*iter).devicename << " for target "
					<< (*iter).target << std::endl;
				DebugPrintf(SV_LOG_INFO, "%s", errstr.str().c_str());                        
			}
            std::string location;
            std::ostringstream errstr;
            std::string volumename = iter->devicename;
            CDPUtil::trim(volumename, " :\\");
            if(!VsnapUtil::construct_persistent_vsnap_filename(volumename, iter->target, location))
            {
                continue;
            }
            ACE_OS::unlink(getLongPathName(location.c_str()).c_str());
        }

        if(VsnapQuit())
        {
            DebugPrintf(SV_LOG_INFO, "Requested service aborted. So some vsnaps may not be unmounted.\n");
            break;
        }
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n",FUNCTION_NAME);
}
bool VsnapMgr::UnMountVsnapVolumesForTargetVolume(string Volume, bool nofail)
{
    bool rv = true;
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s for target volume %s\n",FUNCTION_NAME,Volume.c_str());
    try
    {
        do 
        {
            if(!IsVsnapDriverAvailable())
                break;
            std::vector<VsnapPersistInfo> vsnaps;

            if(!Volume.empty())
            {
                if(!RetrieveVsnapInfo(vsnaps, "target", Volume))
                {
                    DebugPrintf(SV_LOG_ERROR, "Unable to retrieve the vsnap info associated"
                        "with %s from persistent store.\n", Volume.c_str());
                    rv = false;
                    break;
                }
            }
            if(!vsnaps.empty())
            {
                std::vector<VsnapPersistInfo>::iterator iter = vsnaps.begin();
                for(; iter != vsnaps.end(); ++iter)
                {
                    bool unmountflag = false;
                    std::string devName = iter->devicename;
                    if(isSourceFullDevice(iter->target))
                        devName += "s2";
                    if(IsFileORVolumeExisting(iter->devicename))
                    {
                        unmountflag = true;
                    }
#ifdef SV_SUN
                    else if(IsFileORVolumeExisting(devName))
                    {
                        unmountflag = true;
                    }
#endif
                    if(unmountflag)
                    {
                        DebugPrintf(SV_LOG_DEBUG, "Unmounting vsnap %s for target %s\n",
                            iter->devicename.c_str(), iter->target.c_str());

                        VsnapVirtualInfoList VirtualList;
                        VsnapVirtualVolInfo VirtualInfo;

                        VirtualInfo.VolumeName = iter->devicename;
                        VirtualInfo.State = VSNAP_UNMOUNT;
                        VirtualInfo.NoFail = nofail;
                        VirtualList.push_back(&VirtualInfo);
                        if(!Unmount(VirtualList, true))
                        {
                            DebugPrintf(SV_LOG_ERROR,"UnMount failed Vsnap %s(%s). Error @LINE %d in FILE %s\n", 
                                iter->mountpoint.c_str(), iter->devicename.c_str(), LINE_NO, FILE_NAME);
                            rv = false;
                            break; 
                        }
                        //Bug #8065
                        VsnapDeleteAlert a(iter->mountpoint, iter->devicename, Volume);
                        SendAlertToCx(SV_ALERT_ERROR, SV_ALERT_MODULE_SNAPSHOT, a);
                    }
                    else
                    {
						if(!DeleteVsnapLogs(*iter))
						{
							std::ostringstream errstr;
							errstr << "Unable to delete vsnap logs for " << (*iter).devicename << " for target "
								<< (*iter).target << std::endl;
							DebugPrintf(SV_LOG_INFO, "%s", errstr.str().c_str());                        
						}
                        std::string location;
                        std::ostringstream errstr;
                        std::string volumename = iter->devicename;
                        CDPUtil::trim(volumename, " :\\");
                        if(!VsnapUtil::construct_persistent_vsnap_filename(volumename, iter->target, location))
                        {
                            continue;
                        }
                        ACE_OS::unlink(getLongPathName(location.c_str()).c_str());
                    }
                }
            }

        } while (0);
    }
    catch ( ... )
    {
        DebugPrintf(SV_LOG_ERROR, "%s\t%d Failed to unmount vsnaps associated with %s", FUNCTION_NAME, LINE_NO, Volume.c_str());
        rv = false;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s for target volume %s\n",FUNCTION_NAME,Volume.c_str());	
    return rv;
}

bool VsnapMgr::SendUnmountStatus(int status, VsnapVirtualVolInfo *VirtualInfo, VsnapPersistInfo const & vsnapinfo)
{
    bool rv = true;
    string	msg;
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s for the vsnap device %s\n",
        FUNCTION_NAME,VirtualInfo->VolumeName.c_str());

    do
    {
        if(m_SVServer[0] == '\0')
        {
            rv = false;
            break;
        }


        char *VVolume = NULL;
        VVolume = (char *)malloc(MOUNT_PATH_LEN);
        if(!VVolume)
            break;

        inm_strcpy_s(VVolume, MOUNT_PATH_LEN, VirtualInfo->VolumeName.c_str());
        TruncateTrailingBackSlash(VVolume);

        /*   3301 Now we are sending the Virtual Snaphot id to cx 
        stringstream targetVol;
        targetVol << VsnapContext->ParentVolumeName[0];
        stringstream vVol;

        if( VirtualInfo->VolumeName.size() <= 3)
        vVol <<  VVolume[0];
        else
        vVol  << VVolume;

        */

        if ((m_TheConfigurator != NULL  )  && IsConfigAvailable && notifyCx ) 
        {
            //
            // PR #5554
            // use a exception free configurator wrapper routine so that we do not crash on exception
            //
            if(status)
                VsnapGetErrorMsg(VirtualInfo->Error , msg);

            bool res=deleteVirtualSnapshot(/*targetVol.str()*/(*m_TheConfigurator), "",vsnapinfo.snapshot_id,status, msg);
            if(!res)
            {
                if(!VirtualInfo->VolumeName.empty())
                {
                    DebugPrintf(SV_LOG_INFO, "Cx is not updated with the deletion of the vsnap %s\n", VirtualInfo->VolumeName.c_str());
                }
                else
                {
                    DebugPrintf(SV_LOG_INFO, "Cx is not updated with the deletion of the vsnap %s\n", VirtualInfo->DeviceName.c_str());
                }
                rv = false;
            }
        }

        free(VVolume);

    } while (FALSE);
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s for the vsnap device %s\n",
        FUNCTION_NAME,VirtualInfo->VolumeName.c_str());
    return rv;
}
// Modified for Bug #5527
// Using SVUnlink from portablehelpers.cpp


bool VsnapMgr::DeleteVsnapLogs(VsnapPersistInfo const& vsnapinfo)
{
    std::stringstream prefix;
    bool deletesuccess = false;

    std::string vsnaplogpath;
    (vsnapinfo.datalogpath.empty()) ? (vsnaplogpath = vsnapinfo.retentionpath ) : ( vsnaplogpath = vsnapinfo.datalogpath );


    if(!vsnaplogpath.empty())
    {
        prefix << vsnaplogpath << ACE_DIRECTORY_SEPARATOR_CHAR_A << vsnapinfo.snapshot_id;
        if( SVUnlink( prefix.str() + "_VsnapMap"  ) 
            && SVUnlink( prefix.str() + "_FSData" ) 
            && SVUnlink( prefix.str() + "_FileIdTable" )
            && SVUnlink( prefix.str() + "_WriteData1" ) )
            deletesuccess = true;		
    }
    return deletesuccess;
}
// End of Change

// Bug #6133

bool VsnapMgr::containsMountedVolumes(const std::string & volumename, std::string & mountedvolume) const
{
    bool rv = false;
    std::string s1 = volumename;

#ifdef SV_WINDOWS

    FormatVolumeNameForCxReporting(s1);
    FormatVolumeNameForMountPoint(s1);   

    char MountPointBuffer[ MAX_PATH+1 ];
    HANDLE hMountPoint = FindFirstVolumeMountPoint( s1.c_str(),
        MountPointBuffer, sizeof(MountPointBuffer) - 1 );
    if( INVALID_HANDLE_VALUE == hMountPoint )
    {
        rv = false;
    }
    else
    {
        rv = true;
        mountedvolume = MountPointBuffer;
        FindVolumeMountPointClose(hMountPoint);
    }

#endif

    return rv;
}
void VsnapMgr::SetServerAddress(const char* str)
{
    m_SVServer = str;
}

void VsnapMgr::SetServerId(const char *str)
{
    m_HostId = str;
}

void VsnapMgr::SetServerPort(int port)
{
    m_HttpPort = port;
}

void ReplaceString(char *pszInString, char inputChar,char outputChar) 
{
    if (NULL != pszInString)
    {
        for (int i=0; '\0' != pszInString[i]; i++)
        {
            if (pszInString[i] == inputChar)
            {
                pszInString[i] = outputChar;
            }
        }
    }
}

bool VsnapMgr::ValidateMountArgumentsForPair(VsnapPairInfo *PairInfo)
{
    VsnapVirtualVolInfo		*VirtualInfo = PairInfo->VirtualInfo;
    VsnapSourceVolInfo		*SrcInfo = PairInfo->SrcInfo;
    VsnapErrorInfo			*Error = &PairInfo->VirtualInfo->Error;
    bool					TimeBased = false, EventBased = false;
    SV_EVENT_TYPE			type;
    CDPMarkersMatchingCndn	cndn;
    string					dbname;

    if(SrcInfo->VolumeName.empty())
    {		
        Error->VsnapErrorStatus = VSNAP_TARGET_NOT_SPECIFIED;
        Error->VsnapErrorCode = 0;
        return false;
    }
    if(VirtualInfo->TimeToRecover != 0)
        TimeBased = true;

    if(!VirtualInfo->AppName.empty())
        EventBased = true;

    if(!VirtualInfo->EventName.empty())
        EventBased = true;

    if(VirtualInfo->EventNumber)
        EventBased = true;

    //if(TimeBased && EventBased)
    //{
    //	Error->VsnapErrorCode = 0;
    //	Error->VsnapErrorStatus = VSNAP_RECOVERYPOINT_CONFLICT;
    //	return false;
    //}

    if(TimeBased || EventBased)
        VirtualInfo->VsnapType = VSNAP_RECOVERY;
    else
        VirtualInfo->VsnapType = VSNAP_POINT_IN_TIME;

    // VSNAPFC - Persistence
    // VolumeName shouldn't be empty in case of windows
    // can be empty in case of Linux


#ifdef SV_WINDOWS
    if(VirtualInfo->VolumeName.empty())
    {
        Error->VsnapErrorCode = 0;
        Error->VsnapErrorStatus = VSNAP_VIRTUAL_NOT_SPECIFIED;
        return false;
    }
#endif

    //Bug #6133

    //
    // checks on snapshot source
    // 1) should exist
    // 2) should be hidden volume
    // 3) should be replicated volume in case of recovery/rollback (and vsnap)
    // 4) should not be in resync for physical snapshot (and pit vsnap)

    char SrcVolume[MOUNT_PATH_LEN];
    inm_strcpy_s(SrcVolume, ARRAYSIZE(SrcVolume), SrcInfo->VolumeName.c_str());
    TruncateTrailingBackSlash(SrcVolume);


    std::string sparsefile;
    bool newvirtualvolume = false; 
    bool is_volpack = IsVolPackDevice(SrcVolume,sparsefile,newvirtualvolume);
        
    if(!newvirtualvolume)
    {
        if(!IsFileORVolumeExisting(SrcVolume))
        {
            Error->VsnapErrorStatus = VSNAP_TARGET_HANDLE_OPEN_FAILED;
            Error->VsnapErrorCode = ACE_OS::last_error();
            return false;
        }

        // The caller should ensure that the source volume is locked (hidden) 
        // and remains so until the snapshot completes
        if(!IsVolumeLocked(SrcVolume))
        {	
            Error->VsnapErrorStatus = VSNAP_TARGET_UNHIDDEN;
            Error->VsnapErrorCode = ACE_OS::last_error();
            return false;
        }		
    }

	if(m_TheConfigurator)
	{	
		if((!SrcInfo->skiptargetchecks)&&(!isTargetVolume(SrcVolume)))
    {
			Error->VsnapErrorStatus = VSNAP_NOT_A_TARGET_VOLUME;
            Error->VsnapErrorCode = 0;//ACE_OS::last_error();
        return false;
    }
	}

    if(m_TheConfigurator)
    {
        if(VirtualInfo->VsnapType == VSNAP_POINT_IN_TIME && isResyncing(SrcVolume))
        {
            Error->VsnapErrorStatus = VSNAP_TARGET_RESYNC;
            Error->VsnapErrorCode = 0;//ACE_OS::last_error();
            return false;
        }
    }

    if(SrcInfo->RetentionDbPath.empty())
    {
        string vols;
        if(strlen(SrcInfo->VolumeName.c_str()) > 3)
        {
            vols=SrcInfo->VolumeName;
        }
        else
        {
            vols=SrcInfo->VolumeName.substr(0,1);
        }

        string cxFomattedVolumeName = vols;
#ifdef SV_WINDOWS
        FormatVolumeNameForCxReporting(cxFomattedVolumeName);
#endif
        //bug# 5525 - Added try catch to handle the case when unable to obtain retention dbname from cx
        std::string dbpath;

        try 
        {
            if(IsConfigAvailable==true) 
                dbpath =  m_TheConfigurator->getCdpDbName(cxFomattedVolumeName);
        }
        catch(...)
        {
            DebugPrintf(SV_LOG_ERROR,"Unable to obtain the Retention DB Path\n\n");
            Error->VsnapErrorCode = 0;
            Error->VsnapErrorStatus = VSNAP_DB_UNKNOWN;
            return false;	
        }	

        if(!dbpath.empty())
        {
            // convert the path to real path (no symlinks)
            if( !resolvePathAndVerify( dbpath ) )
            {
                DebugPrintf( SV_LOG_INFO, 
                    "Unable to obtain the Retention DB Path %s for volume %s\n",
                    dbpath.c_str(),  cxFomattedVolumeName.c_str() ) ;
                Error->VsnapErrorCode = 0;
                Error->VsnapErrorStatus = VSNAP_DB_UNKNOWN;
                return false ;
            }

            SrcInfo->RetentionDbPath = dbpath;
        }
    }
    //bug# 5525 - Moved ReplaceString out as this has to be invoked even in case of retention db path set earlier.
#ifdef SV_WINDOWS	
    ReplaceString((char*)SrcInfo->RetentionDbPath.c_str(),'/','\\');
#endif	


    if((VirtualInfo->VsnapType == VSNAP_RECOVERY) && SrcInfo->RetentionDbPath.empty())
    {
        Error->VsnapErrorCode = 0;
        Error->VsnapErrorStatus = VSNAP_DB_UNKNOWN;
        return false;
    }

    if(VirtualInfo->VsnapType == VSNAP_POINT_IN_TIME) 
    {
        if(SrcInfo->RetentionDbPath.empty() && VirtualInfo->PrivateDataDirectory.empty())
        {
            Error->VsnapErrorCode = 0;
            Error->VsnapErrorStatus = VSNAP_PRIVATE_DIR_NOT_SPECIFIED;
            return false;
        }
    }

    if ((VirtualInfo->AccessMode == VSNAP_READ_WRITE || VirtualInfo->AccessMode == VSNAP_READ_WRITE_TRACKING) && VirtualInfo->PrivateDataDirectory.empty())
    {
        Error->VsnapErrorCode = 0;
        Error->VsnapErrorStatus = VSNAP_PRIVATE_DIR_NOT_SPECIFIED;
        return false;
    }

    std::string volume = VirtualInfo->VolumeName;
    std::string datalog = VirtualInfo->PrivateDataDirectory;

    CDPUtil::trim(volume);
    CDPUtil::trim(datalog);

#ifdef SV_WINDOWS
    volume = ToLower(volume);
    datalog = ToLower(datalog);
#endif

    if(!(volume.empty()) && (volume == datalog))
    {
        Error->VsnapErrorCode = 0;
        Error->VsnapErrorStatus = VSNAP_VOLNAME_SAME_AS_DATALOG;
        return false;
    }

    if(EventBased)
    {
        if(VirtualInfo->AppName != "")
        {
            if(VacpUtil::AppNameToTagType(VirtualInfo->AppName, type))
                cndn.type(type);
            else
            {
                Error->VsnapErrorCode = 0;
                Error->VsnapErrorStatus = VSNAP_APPNAMETOTYPE_FAILED;
                return false;
            }
        }

        if(VirtualInfo->EventNumber == 0)
            VirtualInfo->EventNumber = 1;

        cndn.value(VirtualInfo->EventName);
        cndn.limit(VirtualInfo->EventNumber);

        if(VirtualInfo->TimeToRecover != 0)
            cndn.atTime(VirtualInfo->TimeToRecover);



        vector<CDPEvent> events;
        if(!CDPUtil::getevents(SrcInfo->RetentionDbPath, cndn, events))
        {
            Error->VsnapErrorCode = 0;
            Error->VsnapErrorStatus = VSNAP_GETMARKER_FAILED;
            return false;
        }

        size_t numevents = events.size();
        if(events.size() == 0)
        {
            Error->VsnapErrorCode = 0;
            Error->VsnapErrorStatus = VSNAP_MATCHING_EVENT_NOT_FOUND;
            return false;
        }

        if(numevents != VirtualInfo->EventNumber)
        {
            Error->VsnapErrorCode = 0;
            Error->VsnapErrorStatus = VSNAP_MATHING_EVENT_NUMBER_NOT_FOUND;
            return false;
        }

        vector<CDPEvent>::reverse_iterator events_iter = events.rbegin();
        VirtualInfo->TimeToRecover = (*(events_iter)).c_eventtime;
        VirtualInfo->EventName = (*(events_iter)).c_eventvalue;
    }

    return true;
}

bool VsnapMgr::InitializeRemountVsnap(string MapFileDir, VsnapPairInfo *PairInfo)
{
    DiskBtree			*VsnapBtree=NULL;
    char				MapFileName[2048];
    VsnapVirtualVolInfo	*VirtualInfo = PairInfo->VirtualInfo;

    VsnapBtree = new DiskBtree();
    VsnapBtree->Init(sizeof(VsnapKeyData));

    inm_sprintf_s(MapFileName, ARRAYSIZE(MapFileName), "%s%c" ULLSPEC "_VsnapMap", MapFileDir.c_str(), ACE_DIRECTORY_SEPARATOR_CHAR_A, VirtualInfo->SnapShotId);
#ifdef SV_WINDOWS
    if(!VsnapBtree->InitFromDiskFileName(MapFileName, O_RDWR, FILE_SHARE_READ))
#else
    if(!VsnapBtree->InitFromDiskFileName(MapFileName, O_RDWR, 0644))
#endif
    {
        VsnapBtree->Uninit();
        delete VsnapBtree;
        return false;
    }

    VsnapBtree->Uninit();
    delete VsnapBtree;

    return true;
}

bool VsnapMgr::getTimeStampFromLastAppliedFile(const std::string targetDevice,SV_ULONGLONG & timeToRecover)
{
    bool rv = true;
    try
    {
        do
        {
            timeToRecover = 0;
            CDPLastProcessedFile appliedInfo;
            if(!CDPUtil::get_last_fileapplied_information(targetDevice,appliedInfo))
            {
                rv = false;
                break;
            }

            std::string lastprocessfile = appliedInfo.previousDiff.filename;
            if(lastprocessfile.empty())
            {
                rv = false;
                break;
            }
            std::string::size_type dotPosPrev = lastprocessfile.rfind("_");
            if(std::string::npos == dotPosPrev)
            {
                rv = false;
                break;
            }
            std::string::size_type dotPos = 0;
            while(std::string::npos != (dotPos = lastprocessfile.rfind("_",dotPosPrev - 1)))
            {
                if(lastprocessfile[dotPos + 1] == 'E')
                {
                    timeToRecover = boost::lexical_cast<SV_ULONGLONG>(lastprocessfile.substr(dotPos + 2, dotPosPrev - (dotPos + 2)).c_str());
                    break;
                }
                dotPosPrev = dotPos;
            }
        }while(false);
    } catch (...)
    {
        rv = false;
    }
    return rv;
}

bool VsnapMgr::InitializePointInTimeVsnap(string MapFileDir, VsnapPairInfo *PairInfo, SV_ULONGLONG &VsnapTime)
{
    DiskBtree    *VsnapBtree = NULL;
    unsigned int            FileId = 0;
    char     MapFileName[2048];
    SVD_DIRTY_BLOCK_INFO Dblock;
    bool     success = true;
    VsnapVirtualVolInfo  *VirtualInfo = PairInfo->VirtualInfo;
    VsnapSourceVolInfo  *SrcInfo = PairInfo->SrcInfo;
    VsnapPersistentHeaderInfo VsnapHdrInfo;
    VsnapErrorInfo		*Error = &VirtualInfo->Error;
    std::vector<std::string> filenames;
    unsigned int fileIdsWritten = 0;
	const unsigned int		maxWriteLimit = 4294967295;
 

    do
    {
        VsnapBtree = new DiskBtree;

        VsnapBtree->Init(sizeof(VsnapKeyData));

        inm_sprintf_s(MapFileName, ARRAYSIZE(MapFileName), "%s%c" ULLSPEC "_VsnapMap", MapFileDir.c_str(), ACE_DIRECTORY_SEPARATOR_CHAR_A, VirtualInfo->SnapShotId);
#ifdef SV_WINDOWS
        if(!VsnapBtree->InitFromDiskFileName(MapFileName, O_RDWR | O_CREAT, FILE_SHARE_READ))
#else
        if(!VsnapBtree->InitFromDiskFileName(MapFileName, O_RDWR | O_CREAT, 0644))
#endif
        {
            success = false;
            break;
        }

        VsnapBtree->SetCallBack(&VsnapCallBack);
        if(!CreateFileIdTableFile(MapFileDir, PairInfo))
        {
            success = false;
            break;
        }

        if((SrcInfo->FsType == VSNAP_FS_NTFS) 
			|| (SrcInfo->FsType == VSNAP_FS_FAT)
			|| (SrcInfo->FsType == VSNAP_FS_ReFS)
			|| (SrcInfo->FsType == VSNAP_FS_EXFAT))
        {
            if(InsertFSData(MapFileDir, PairInfo,filenames, fileIdsWritten))
            {
                Dblock.DataFileOffset = 0;
                Dblock.DirtyBlock.ByteOffset = 0;
                Dblock.DirtyBlock.Length = VSNAP_FS_DATA_SIZE;
                FileId = filenames.size() + fileIdsWritten;
                VirtualInfo->NextFileId = FileId + 1;
                bool rv = UpdateMap(VsnapBtree, (FileId | VSNAP_RETENTION_FILEID_MASK), &Dblock);
                if(!rv)
                {
                    Error->VsnapErrorStatus = VSNAP_MAP_OP_FAILED;			
                    Error->VsnapErrorCode = ACE_OS::last_error();
                    success = false;
                    break;
                }
            }
            else
            {
                //VsnapErrorPrint(PairInfo);
                success = false;
                break;
            }
        }

        SV_LONGLONG difference = 0;
		SV_LONGLONG bytesprocessed = 0;
        if( SrcInfo ->ActualSize  >=  SrcInfo ->UserVisibleSize)
        {
			difference = (SrcInfo ->ActualSize - SrcInfo ->UserVisibleSize);
			
			//
			// scenario where the difference in the raw size vs fs size
			// runs in gbs, we end up requiring the corresponding disk space
			// for vsnap private data directory. see PR#23386  and PR#23456
			// for details.
			// to address this issue, (for now) we are bounding the difference
			// through config variable in drscout.conf
			//
			LocalConfigurator lc;
			SV_LONGLONG MaxDifference = lc.MaxDifferenceBetweenFSandRawSize();
			if(MaxDifference && (difference >  MaxDifference))
			{
				SrcInfo ->ActualSize = SrcInfo ->UserVisibleSize + MaxDifference;
				difference = MaxDifference;
			}

			if(InsertVsnapWriteData(MapFileDir, PairInfo,filenames,difference, fileIdsWritten))
			{
				VirtualInfo->NextFileId = FileId + 1;
				FileId = filenames.size() + fileIdsWritten;
				while(difference) //when difference is more than 4 GB,split it
				{
					unsigned int writelength = difference >maxWriteLimit ? maxWriteLimit : difference;
					Dblock.DirtyBlock.ByteOffset = SrcInfo->UserVisibleSize + bytesprocessed;
					Dblock.DirtyBlock.Length = writelength;
					Dblock.DataFileOffset = bytesprocessed;
					bool rv = UpdateMap(VsnapBtree, (FileId | VSNAP_RETENTION_FILEID_MASK), &Dblock);
					if(!rv)
					{
						Error->VsnapErrorStatus = VSNAP_MAP_OP_FAILED;			
						Error->VsnapErrorCode = ACE_OS::last_error();
						success = false;
						break;
					}
					difference -= writelength;
					bytesprocessed += writelength;
				}
			}
			else
			{
				VsnapErrorPrint(PairInfo);
				success = false;
				break;
			}
			if(success == false)
			{
				break;
			}	
        }


        VsnapHdrInfo.DataFileId = FileId + 1;
        VsnapHdrInfo.BackwardFileId = 0;
        VsnapHdrInfo.ForwardFileId = FileId + 1;
        VsnapHdrInfo.WriteFileId = FileId;
        VsnapHdrInfo.WriteFileSuffix = 1;
        VsnapHdrInfo.VsnapTime=VsnapTime;

        if(!VsnapUpdatePersistentHdrInfo(VsnapBtree, &VsnapHdrInfo))
        {
            VirtualInfo->Error.VsnapErrorStatus = VSNAP_MAP_OP_FAILED; 
            VirtualInfo->Error.VsnapErrorCode = ACE_OS::last_error(); 
            success = false; 
            break ;
        }
        success = true;

    } while(0);

    if(VsnapBtree)
    {
        VsnapBtree->Uninit();
        delete VsnapBtree;
    }

    return success;
}


void VsnapMgr::Mount(VsnapSourceVolInfo  *SourceInfo)
{
	bool rv = true;
    VsnapPairInfo	PairInfo;
    PairInfo.SrcInfo = SourceInfo;
    //Now, iterate through all virtualinfo structures and set vsnap operation state.
    VsnapVirtualIter VirtualIterBegin = SourceInfo->VirtualVolumeList.begin();
    VsnapVirtualIter VirtualIterEnd = SourceInfo->VirtualVolumeList.end();

    for( ; VirtualIterBegin != VirtualIterEnd ; ++VirtualIterBegin )
    {
        VsnapVirtualVolInfo *VirtualInfo = (VsnapVirtualVolInfo *)*VirtualIterBegin;

        VirtualInfo->State = VSNAP_MOUNT;
        PairInfo.VirtualInfo = VirtualInfo;
        if(!IsVsnapDriverAvailable())
        {
            std::string error = "Vsnap functionality is not enabled. Enable it as described in the user manual.\n";
            VirtualInfo->VsnapProgress = VSNAP_FAILED_STATUS;
            SendStatus(&PairInfo, error);
			rv = false;
        }
    }

	if(rv)
	{
    PerformMountOrRemount(SourceInfo);
}
}


unsigned long VsnapMgr::GetNextFileId(unsigned long long SnapShotId, const char *LogDir)
{
    FILE *fp = NULL;
    std::stringstream SSFileTableName;
    SSFileTableName << LogDir << ACE_DIRECTORY_SEPARATOR_CHAR_A << SnapShotId << "_FileIdTable";
    fp = fopen(SSFileTableName.str().c_str(), "rb");
    if(!fp)
    {
        DebugPrintf(SV_LOG_FATAL , "VSNAP INFO : Unable to open the FileId \n ") ;
        return 0;
    }

    fseek(fp, 0, SEEK_END);

    int FileId = ftell(fp)/sizeof(VsnapFileIdTable);
    FileId++;

    fclose(fp);

    return FileId;
}

void VsnapMgr::Remount(VsnapSourceVolInfo * SourceInfo)
{
    VsnapVirtualIter VirtualIterBegin = SourceInfo->VirtualVolumeList.begin();
    VsnapVirtualIter VirtualIterEnd = SourceInfo->VirtualVolumeList.end();
    if(!IsVsnapDriverAvailable())
    {
        DebugPrintf(SV_LOG_INFO, "Vsnap functionality is not enabled. Enable it as described in the user manual.\n");
        return ;
    }

    SourceInfo->RetentionDbPath.clear();

    for( ; VirtualIterBegin != VirtualIterEnd ; ++VirtualIterBegin )
    {
        VsnapVirtualVolInfo *VirtualInfo = (VsnapVirtualVolInfo *)*VirtualIterBegin;
        VirtualInfo->State = VSNAP_REMOUNT;
    }

    PerformMountOrRemount(SourceInfo);

}

bool VsnapMgr::ApplyTrackLogs(VsnapSourceVolInfo *SrcInfo)
{
    VsnapVirtualVolInfo *VirtualInfo = (VsnapVirtualVolInfo *)*(SrcInfo->VirtualVolumeList.begin());
    bool				Success = true;
    VsnapErrorInfo		*Error = &VirtualInfo->Error;
    VsnapContextInfo	*VsnapContext = NULL;

    VsnapContext = (VsnapContextInfo *) malloc (sizeof(VsnapContextInfo));
    if(!VsnapContext)
    {
        Error->VsnapErrorCode = 0;
        Error->VsnapErrorStatus = VSNAP_MEM_UNAVAILABLE;
        return false;
    }

    Error->VsnapErrorCode = 0;
    Error->VsnapErrorStatus = VSNAP_SUCCESS;

    if((VirtualInfo->SnapShotId == 0) && VirtualInfo->VolumeName.empty())
    {
        Error->VsnapErrorStatus = VSNAP_ID_VIRTUALINFO_UNKNOWN;
        Error->VsnapErrorCode = 0;
        free(VsnapContext);
        return false;
    }

    if((VirtualInfo->SnapShotId != 0) && !VirtualInfo->VolumeName.empty())
    {
        Error->VsnapErrorStatus = VSNAP_ID_VOLNAME_CONFLICT;
        Error->VsnapErrorCode = 0;
        free(VsnapContext);
        return false;
    }
    if(!VirtualInfo->PrivateDataDirectory.empty() && !VirtualInfo->VolumeName.empty())
    {
        Error->VsnapErrorStatus = DATALOG_VOLNAME_CONFLICT;
        Error->VsnapErrorCode = 0;
        free(VsnapContext);
        return false;
    }
    if(SrcInfo->VolumeName.empty())
    {
        Error->VsnapErrorStatus = VSNAP_ID_TGTVOLNAME_EMPTY;
        Error->VsnapErrorCode = 0;
        free(VsnapContext);
        return false;
    }

    if(VirtualInfo->SnapShotId != 0 && VirtualInfo->PrivateDataDirectory.empty())
    {
        Error->VsnapErrorStatus = VSNAP_MAP_LOCATION_UNKNOWN;
        Error->VsnapErrorCode = 0;
        free(VsnapContext);
        return false;
    }

    if(VirtualInfo->SnapShotId == 0)
    {
        VsnapGetVirtualInfoDetaisFromVolume(VirtualInfo, VsnapContext);
        VirtualInfo->PrivateDataDirectory = VsnapContext->PrivateDataDirectory;
        VirtualInfo->SnapShotId = VsnapContext->SnapShotId;
    }

    if(Error->VsnapErrorStatus != VSNAP_SUCCESS)
    {
        free(VsnapContext);
        return false;
    }

    Success = TraverseAndApplyLogs(SrcInfo);

    free(VsnapContext);

    return Success;
}

bool VsnapUserGetFileNameFromFileId(string MapFile, unsigned long long id, unsigned long FileId, char *FileName)
{
    std::vector<char> vfilename(2048, '\0');
    ACE_HANDLE Handle;
    unsigned long long NewOffset = 0;
    int BytesRead;

    inm_sprintf_s(&vfilename[0], vfilename.size(), "%s%c" ULLSPEC "_FileIdTable", MapFile.c_str(), ACE_DIRECTORY_SEPARATOR_CHAR_A, id);
#ifdef SV_WINDOWS
    if (!OPEN_FILE(vfilename.data(), O_RDONLY, FILE_SHARE_READ | FILE_SHARE_WRITE, &Handle))
#else
    if(!OPEN_FILE(vfilename.data(), O_RDONLY, 0644 | 0666 , &Handle))
#endif
    {
        return false;
    }
    NewOffset = (FileId-1)*sizeof(VsnapFileIdTable);

    if(!READ_FILE(Handle, FileName, NewOffset, sizeof(VsnapFileIdTable), &BytesRead))
    {
        CLOSE_FILE(Handle);
        return false;
    }

    CLOSE_FILE(Handle);
    return true;
}

bool VsnapMgr::ParseNodeAndApply(ACE_HANDLE VolumeHandle, void *Buffer, VsnapVirtualVolInfo *VirtualInfo, DiskBtree *VsnapBtree)
{
    VsnapKeyData	*Key;
    int				KeySize = sizeof(VsnapKeyData);
    int				NumKeys = VsnapBtree->GetNumKeys(Buffer);
    int				i = 0;
    char			Filename[2048];
    bool			Success = true;

    while(i < NumKeys)
    {
        Key = (VsnapKeyData *)VsnapBtree->NodePtr(i, Buffer);

        if(Key->TrackingId == 0)
        {
            i++;
            continue;
        }

        inm_strcpy_s(Filename, ARRAYSIZE(Filename), VirtualInfo->PrivateDataDirectory.c_str());
        string File=Filename;
        File+=ACE_DIRECTORY_SEPARATOR_CHAR_A;
        inm_strcpy_s(Filename,ARRAYSIZE(Filename), File.c_str());

        if(!VsnapUserGetFileNameFromFileId(VirtualInfo->PrivateDataDirectory, VirtualInfo->SnapShotId, 
            (unsigned long)(Key->FileId & ~VSNAP_RETENTION_FILEID_MASK), Filename + strlen(Filename)))
        {
            DebugPrintf(SV_LOG_INFO, "Failed to get filename from file id\n");
            Success = false;
            break;
        }

        char *Buff =(char *) malloc(Key->Length);
        if(!Buff)
        {
            DebugPrintf(SV_LOG_INFO, "Memory allocation failed\n");
            Success = false;
            break;
        }

        ACE_HANDLE Handle;

#ifdef SV_WINDOWS
        if(!OPEN_FILE(Filename, O_RDONLY, FILE_SHARE_READ | FILE_SHARE_WRITE, &Handle))
#else
        if(!OPEN_FILE(Filename, O_RDONLY, 0644 | 0666 , &Handle))
#endif
        {
            DebugPrintf(SV_LOG_INFO, "Failed to open data file\n");
            free(Buff);
            Success = false;
            break;
        }

        int BytesRead = 0;
        if(!READ_FILE(Handle, Buff, Key->FileOffset, Key->Length, &BytesRead))
        {
            DebugPrintf(SV_LOG_INFO, "Failed to read from data file\n");
            free(Buff);
            CLOSE_FILE(Handle);
            Success = false;
            break;
        }

        int BytesWritten = 0;
        if(!WRITE_FILE(VolumeHandle, Buff, Key->Offset, Key->Length, &BytesWritten))
        { 
            DebugPrintf(SV_LOG_INFO, "Failed to write to target volume" ULLSPEC " %lu %x\n", Key->Offset, Key->Length, ACE_OS::last_error());
            free(Buff);
            CLOSE_FILE(Handle);
            Success = false;
            break;
        }
        CLOSE_FILE(Handle);

        free(Buff);

        i++;
    }

    return Success;
}

bool VsnapMgr::ParseMap(ACE_HANDLE VolumeHandle, ACE_HANDLE MapHandle, VsnapVirtualVolInfo *VirtualInfo)
{
    bool Success = true;
    char AlignedBuffer[DISK_PAGE_SIZE + ALIGNMENT_PAD_SIZE];
    //char Buffer[DISK_PAGE_SIZE];
    char *Buffer = AlignedBuffer + ALIGNMENT_PAD_SIZE;
    unsigned long long NewOffset = 0, MaxOffset = 0;
    int BytesRead = 0;
    DiskBtree VsnapBtree;
    char str[50];
    int percentage = 0;
    VsnapErrorInfo		*Error = &VirtualInfo->Error;

    VsnapBtree.Init(sizeof(VsnapKeyData));

    NewOffset = BTREE_HEADER_SIZE;

    DebugPrintf(SV_LOG_INFO, "Started applying track logs. This operation may take some time to complete. Aborting this operation in the middle may leave the target volume in invalid state.\n\n");

    if(!SEEK_FILE(MapHandle, 0, &MaxOffset, SEEK_END))
    {
        Error->VsnapErrorStatus = VSNAP_FILETBL_OP_FAILED;
        Error->VsnapErrorCode = ACE_OS::last_error();
        Success = false;
        //break;
    }

    while(true)
    {
        BytesRead = 0;

        if(!READ_FILE(MapHandle, Buffer, NewOffset, DISK_PAGE_SIZE, &BytesRead))
        {
            Error->VsnapErrorStatus = VSNAP_FILETBL_OP_FAILED;
            Error->VsnapErrorCode = ACE_OS::last_error();
            Success = false;
            break;			
        }

        if(!ParseNodeAndApply(VolumeHandle, Buffer, VirtualInfo, &VsnapBtree))
        {
            Success = false;
            break;
        }

        percentage = (int)((NewOffset * (unsigned long long)100)/MaxOffset);
        inm_sprintf_s(str, ARRAYSIZE(str), "Applying Logs, Percentage Completed: %d\r", percentage);
        VsnapPrint(str);

        NewOffset += DISK_PAGE_SIZE;

        if(NewOffset >= MaxOffset)
        {
            Success = true;
            break;
        }

    }

    if(NewOffset >= MaxOffset)
    {
        VsnapPrint("Applying Logs, Percentage Completed: 100%\n");
    }

    return Success;
}



void VsnapMgr::DeleteLogs(VsnapSourceVolInfo *SrcInfo)
{
    VsnapVirtualVolInfo	*VirtualInfo = (VsnapVirtualVolInfo *)*(SrcInfo->VirtualVolumeList.begin());

    VsnapErrorInfo		*Error = &VirtualInfo->Error;

    Error->VsnapErrorCode = 0;
    Error->VsnapErrorStatus = VSNAP_SUCCESS;

    VsnapPersistInfo vsnapinfo;

    vsnapinfo.retentionpath = SrcInfo->RetentionDbPath;
    vsnapinfo.datalogpath = VirtualInfo->PrivateDataDirectory;
    vsnapinfo.snapshot_id = VirtualInfo->SnapShotId;

    // Bug # 5527 Modified VSNAP_DELETE_LOG_FAILED to VSNAP_DELETE_LOGFILES_FAILED 
    // as VSNAP_DELETE_LOG_FAILED is handled prior to calling DeleteLogs
    //
    if(!DeleteVsnapLogs(vsnapinfo))
    {
        Error->VsnapErrorCode = 0;
        Error->VsnapErrorStatus = VSNAP_DELETE_LOGFILES_FAILED;
    }

    return ; 
}

void VsnapMgr::Sync(VsnapVirtualVolInfo *VirtualInfo)
{
    ACE_HANDLE		handleVolume  = ACE_INVALID_HANDLE;
    SVERROR			sve = SVS_OK;
    std::string			sTgtGuid;
    VsnapErrorInfo	*Error = &VirtualInfo->Error;

    Error->VsnapErrorCode = 0;
    Error->VsnapErrorStatus = VSNAP_SUCCESS;

    sve = OpenVolumeExclusive(&handleVolume, sTgtGuid, VirtualInfo->VolumeName.c_str(), VirtualInfo->VolumeName.c_str() /*TODO:MPOINT*/, TRUE, FALSE);
    if(sve.failed())
    {
        Error->VsnapErrorCode = 0;
        Error->VsnapErrorStatus = VSNAP_TARGET_OPENEX_FAILED;
        return;
    }
    CloseVolumeExclusive(handleVolume, sTgtGuid.c_str(), VirtualInfo->VolumeName.c_str(), VirtualInfo->VolumeName.c_str()/*TODO:MPOINT*/);
    //RegisterHost();
}
int  VsnapMgr::RunScript(std::string script, VsnapVirtualVolInfo *VirtualInfo, VsnapSourceVolInfo *SrcInfo, std::string& output, std::string& error)
{

    std::string prescript = script;

    if (prescript.empty() || '\0' == prescript[0] || "\n" == prescript )
        return 1;

    CDPUtil::trim(prescript);
    string stuff_to_trim = " \n\b\t\a\r\xc" ;
    if (prescript.find_first_not_of(stuff_to_trim) == string::npos)
        return 1;

    std::ostringstream args;

    args << prescript 
        << " --S " << '"' << SrcInfo->VolumeName.c_str()<< '"'
        << " --T " << '"' << VirtualInfo->VolumeName.c_str() << '"' 
        << " --D "  << '"' << VirtualInfo->DeviceName.c_str() << '"';

    DebugPrintf(SV_LOG_DEBUG, "Script commandline : %s\n",args.str().c_str());

    std::ostringstream msg;

    InmCommand inmscript(args.str());
    inmscript.SetShouldWait(false);
    InmCommand::statusType status = inmscript.Run();

    if(status != InmCommand::running)
    {
        msg << "internal error occured during execution of the script: " << script << std::endl;;
        if(!inmscript.StdErr().empty())
        {
            msg << inmscript.StdErr();
        }
        error += msg.str();
        return 1;
    }


    static const int TaskWaitTimeInSeconds = 5;
    while(true)
    {
        if (VsnapQuit(TaskWaitTimeInSeconds)) {
            inmscript.Terminate();
            msg << "pre/post script terminated because of service shutdown request";
            error += msg.str();
            return 1;
        }

        status = inmscript.TryWait(); // just check for exit don't wait
        if (status == InmCommand::internal_error) 
        {
            msg << "internal error occured during execution of the script: " << script;
            if(!inmscript.StdErr().empty())
            {
                msg << inmscript.StdErr();
            }
            error += msg.str();
            return 1;
        } 
        else if (status == InmCommand::completed) 
        {

            if(inmscript.ExitCode())
            {
                msg << "script failed : Exit Code = " << inmscript.ExitCode() << std::endl;
                if(!inmscript.StdOut().empty())
                {
                    msg << "Output = " << inmscript.StdOut() << std::endl;
                }
                if(!inmscript.StdErr().empty())
                {
                    msg << "Error = " << inmscript.StdErr() << std::endl;
                }
                m_err += msg.str();
                error += msg.str();
                return 1;
            }
            return 0;
        }
    }
}
int VsnapMgr::RunScript(string script, VsnapVirtualVolInfo *VirtualInfo, VsnapSourceVolInfo *SrcInfo)
{
    std::string output, error;
    return RunScript(script, VirtualInfo, SrcInfo, output, error);
}

int VsnapMgr::WaitForProcess(ACE_Process_Manager * pm, pid_t pid)
{
    std::ostringstream msg;

    ACE_Time_Value timeout;
    ACE_exitcode status = 0;

    while (true) 
    {

        pid_t rc = pm->wait(pid, timeout, &status); // just check for exit don't wait
        if (ACE_INVALID_PID == rc) 
        {
            msg << "script failed in ACE_Process_Manager::wait with error no: " 
                << ACE_OS::last_error() << '\n';
            m_err += msg.str();
            return 1;
        } 
        else if (rc == pid) 
        {
            if (0 != status) 
            {
                msg << "script failed with exit status " << status << '\n';
                m_err += msg.str();
                return status;
            }
            return 0;
        }
    }
    return 0;
}	

int VsnapMgr::RunPreScript(VsnapVirtualVolInfo *VirtualInfo, VsnapSourceVolInfo *SrcInfo, std::string& output, std::string& error)
{
    if(!VirtualInfo->PreScript.empty())
    {
        VsnapPairInfo	PairInfo; 
        PairInfo.SrcInfo = SrcInfo ;
        PairInfo.VirtualInfo = VirtualInfo ; 

        SV_UINT ReturnValue = 0 ;

        VirtualInfo->VsnapProgress = PRE_SCRIPT_STARTS;
        SendStatus(&PairInfo);

        DebugPrintf(SV_LOG_INFO, "Executing Prescript.....\n");

        ReturnValue =  RunScript(VirtualInfo->PreScript, VirtualInfo, SrcInfo, output, error);

        VirtualInfo->VsnapProgress = PRE_SCRIPT_ENDS;
        SendStatus(&PairInfo);

        return ReturnValue ;

    }
    else
        return 0;
}
int VsnapMgr::RunPostScript(VsnapVirtualVolInfo *VirtualInfo, VsnapSourceVolInfo *SrcInfo, std::string& output, std::string& error)
{
    if(!VirtualInfo->PostScript.empty())
    {
        VsnapPairInfo	PairInfo; 
        PairInfo.SrcInfo = SrcInfo ;
        PairInfo.VirtualInfo = VirtualInfo ; 

        SV_UINT ReturnValue = 0 ;

        VirtualInfo->VsnapProgress = POST_SCRIPT_STARTS;
        SendStatus(&PairInfo);

        DebugPrintf(SV_LOG_INFO, "Executing Postscript.....\n");

        ReturnValue =  RunScript(VirtualInfo->PostScript, VirtualInfo, SrcInfo, output, error);

        VirtualInfo->VsnapProgress = POST_SCRIPT_ENDS;
        SendStatus(&PairInfo);

        return  ReturnValue ;

    }
    else
        return 0;
}


int VsnapMgr::RunPreScript(VsnapVirtualVolInfo *VirtualInfo, VsnapSourceVolInfo *SrcInfo)
{
    std::string output, error;
    return RunPreScript(VirtualInfo, SrcInfo, output, error);
}

int VsnapMgr::RunPostScript(VsnapVirtualVolInfo *VirtualInfo, VsnapSourceVolInfo *SrcInfo)
{
    std::string output, error;
    return RunPostScript(VirtualInfo, SrcInfo, output, error);
}

bool VsnapMgr::VsnapUpdatePersistentHdrInfo(DiskBtree *VsnapBtree, VsnapPersistentHeaderInfo *VsnapHdrInfo)
{
    char Hdr[BTREE_HEADER_SIZE];
    VsnapPersistentHeaderInfo *temp = (VsnapPersistentHeaderInfo *) (Hdr + BTREE_HEADER_OFFSET_TO_USE);

    if(!VsnapBtree->GetHeader(Hdr))
    {
        DebugPrintf(SV_LOG_FATAL, "VSNAP : While getting the B tree header failed .....\n"); 
        return false;
    }	

    temp->DataFileId = VsnapHdrInfo->DataFileId;
    temp->WriteFileId = VsnapHdrInfo->WriteFileId;
    temp->WriteFileSuffix = VsnapHdrInfo->WriteFileSuffix;
    temp->VsnapTime=VsnapHdrInfo->VsnapTime; //Modified for PIT VSNAP
    temp->BackwardFileId=VsnapHdrInfo->BackwardFileId;
    temp->ForwardFileId=VsnapHdrInfo->ForwardFileId;

    if(!VsnapBtree->WriteHeader(Hdr))
    {
        DebugPrintf(SV_LOG_FATAL, "VSNAP : While updating the B tree header failed .....\n"); 
        return false;
    }

    return true;
}


VsnapVirtualVolInfoTag::VsnapVirtualVolInfoTag()
{
    VolumeName = "";
    PrivateDataDirectory = "";
    AccessMode = VSNAP_UNKNOWN_ACCESS_TYPE;
    VsnapType = VSNAP_CREATION_UNKNOWN;
    TimeToRecover = 0;
    AppName = "";
    EventName = "";
    EventNumber = 0;
    PreScript = "";
    PostScript = "";
    ReuseDriveLetter = false;

    SnapShotId = 0;
    NextFileId = 0;
    Error.VsnapErrorCode = 0;
    Error.VsnapErrorStatus = VSNAP_SUCCESS;
    State = VSNAP_UNKNOWN;
    PreScriptExitCode = 0;
    PostScriptExitCode = 0;
    SequenceId = 0;
    NoFail = false;
	VolumeGuid = "";
	MountVsnap = true;
}

VsnapSourceVolInfoTag::VsnapSourceVolInfoTag():VolumeName(""),
RetentionDbPath(""),
SectorSize(0),
ActualSize(0),
UserVisibleSize(0),
FsData(NULL),
skiptargetchecks(false),
FsType(VSNAP_FS_UNKNOWN)		

{
}

void VsnapMgr::SetCXSettings()
{
    LocalConfigurator localConfigurator;
    m_SVServer = GetCxIpAddress();// m_AgentParams.szSVServerName;
    if(IsConfigAvailable == true)
        m_HostId =m_TheConfigurator->getHostId();// szValue;
    else
        m_HostId = localConfigurator.getHostId() ; 

    m_HttpPort =localConfigurator.getHttp().port;// m_AgentParams.ServerHttpPort;

}
void VsnapMgr::SetSnapshotObj(void *Obj)
{
    SnapShotObj = Obj;
}

bool VsnapMgr::VsnapQuit(int waitSeconds)
{
    //Bug# 7934
    if(SnapShotObj)
    {
        class SnapshotTask *SnapPtr = (class SnapshotTask *)SnapShotObj;
        return SnapPtr->Quit(waitSeconds);
    }
    else
    {
        return CDPUtil::QuitRequested(waitSeconds);
    }
}

bool VsnapMgr::PollRetentionData(string volume, SV_ULONGLONG &end_time)
{
    bool rc=true; 

    CDPDatabase database(volume);
    CDPSummary summary;
    if(!database.getcdpsummary(summary))
    {
        rc = false;
    }
    end_time = summary.end_ts;
    return rc; 
}


/*
* FUNCTION NAME :  VsnapMgr::isTargetVolume
*
* DESCRIPTION : check if the specified volume is a target volume
*
*
* INPUT PARAMETERS : none
*
* OUTPUT PARAMETERS : none
*
* NOTES :
* 
*
* return value : true on success, otherwise false
*
*/

bool VsnapMgr::isTargetVolume(const std::string & volumename) const
{
    // Replication Pair configuration information is not available
    // we skip this check
    //

    if(!IsConfigAvailable)
    {
        return false;
    }

    return (m_TheConfigurator->isTargetVolume(volumename));
}
/*
* FUNCTION NAME :  VsnapMgr::isResyncing
*
* DESCRIPTION : check if specified volume is undergoing resync operation 
*               
*
* INPUT PARAMETERS : none
*
* OUTPUT PARAMETERS : none
*
* NOTES :
* 
*
* return value : true on success, otherwise false
*
*/
bool VsnapMgr::isResyncing(const std::string & volumename) const
{
    // 
    // Replication Pair configuration information is not available
    // we skip this check
    //
    if(!IsConfigAvailable)
    {
        return false;
    }

    return (m_TheConfigurator->isResyncing(volumename));
}


//Bug# 7277

/*
* FUNCTION NAME :  VsnapMgr::getFSType
*
* DESCRIPTION : get the file system type on volume from configurator
*               
*
* INPUT PARAMETERS : none
*
* OUTPUT PARAMETERS : none
*
* NOTES :
* 
*
* return value : filesystem type on the volume
*                "" if it is raw volume / information not available
*
*/
std::string VsnapMgr::getFSType(const std::string & volumename) const
{

    // 
    // Replication Pair configuration information is not available
    // we skip this check
    //
    if(!IsConfigAvailable)
    {
        return "";
    }

    return(m_TheConfigurator ->getVxAgent().getSourceFileSystem(volumename));
}

/*
* FUNCTION NAME :  VsnapUtil::create_persistent_vsnap_file_handle
*
* DESCRIPTION : Retrieves File Handle from Vsnap DeviceName and Replication Target 
*
* INPUT PARAMETERS :  1. Vsnap Device Name
*					  2. Replication Target 
*					  
*
* OUTPUT PARAMETERS : File Handle
*
* NOTES : None
*		  
*
* return value :  true on success, false otherwise
*/

bool VsnapUtil::create_persistent_vsnap_file_handle(const std::string& devicename, const std::string& target, ACE_HANDLE& handle)
{

    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s for %s\n", FUNCTION_NAME, devicename.c_str());

    bool rv = true;

    do 
    {
        std::string filename;
        if(!construct_persistent_vsnap_filename(devicename, target,filename))
        {
            std::ostringstream errstr;
            errstr << "Failed to persist vsnap information for " << devicename << " " << FUNCTION_NAME << " " << LINE_NO << std::endl;
            DebugPrintf(SV_LOG_ERROR, "%s", errstr.str().c_str());
            rv = false;
            break;
        }
        if(!create_vsnap_file_handle(filename,handle))
        {
            DebugPrintf(SV_LOG_ERROR, "Unable to create %s\n", filename.c_str());
            rv = false;
            break;
        }
    }while(false);
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s for %s\n", FUNCTION_NAME, devicename.c_str());
    return rv;
}

/*
* FUNCTION NAME :  VsnapUtil::create_pending_persistent_vsnap_file_handle
*
* DESCRIPTION : Retrieves File Handle from Vsnap DeviceName and Replication Target 
*
* INPUT PARAMETERS :  1. Vsnap Device Name
*					  2. Replication Target 
*					  
* OUTPUT PARAMETERS : File Handle
*
* NOTES : None
*		  
*
* return value :  true on success, false otherwise
*/
bool VsnapUtil::create_pending_persistent_vsnap_file_handle( const std::string& devicename, const std::string& target, ACE_HANDLE& handle)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s for %s\n", FUNCTION_NAME, devicename.c_str());
    bool rv = true;

    do
    {
        std::string filename;
        if(!construct_pending_persistent_vsnap_filename(devicename, target,filename))
        {
            std::ostringstream errstr;
            errstr << "Failed to create pending vsnap file for the vsnap " << devicename 
                << " corresponding to the target " << target
                << ". So the vsnap device will not be shown in the CX UI." << std::endl;
            DebugPrintf(SV_LOG_DEBUG, "%s", errstr.str().c_str());
            rv = false;
            break;
        }
        if(!create_vsnap_file_handle(filename,handle))
        {
            DebugPrintf(SV_LOG_ERROR, "Unable to create %s\n", filename.c_str());
            rv = false;
            break;
        }
    }while(false);
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s for %s\n", FUNCTION_NAME, devicename.c_str());
    return rv;
}

/*
* FUNCTION NAME :  VsnapUtil::create_vsnap_file_handle
*
* DESCRIPTION : Create File Handle from file name 
*
* INPUT PARAMETERS :  filename with path
*					  
* OUTPUT PARAMETERS : File Handle
*
* NOTES : None
*		  
*
* return value :  true on success, false otherwise
*/

bool VsnapUtil::create_vsnap_file_handle( const std::string& filename, ACE_HANDLE& handle)
{

    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s for %s\n", FUNCTION_NAME, filename.c_str());

    bool rv = true;

    do 
    {
        // PR#10815: Long Path support
        std::vector<char> vfile_path(SV_MAX_PATH, '\0');
        DirName(filename.c_str(),&vfile_path[0], vfile_path.size());
        if(SVMakeSureDirectoryPathExists(vfile_path.data()).failed())
        {
            DebugPrintf(SV_LOG_ERROR, "Unable to create %s\n", vfile_path.data());
            rv = false;
            break;
        }

        // PR#10815: Long Path support
        handle = ACE_OS::open(getLongPathName(filename.c_str()).c_str(), O_RDWR | O_CREAT);

        if(ACE_INVALID_HANDLE == handle)
        {
            std::ostringstream ostr;
            ostr << "Failed to create " << filename 
                << " error code = " << ACE_OS::last_error()
                << " error message = " << ACE_OS::strerror(ACE_OS::last_error())
                << std::endl;
            DebugPrintf(SV_LOG_DEBUG, "%s", ostr.str().c_str());
            rv = false;
            break;
        }

    }while(0);

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s for %s\n", FUNCTION_NAME, filename.c_str());

    return rv;
}
/*
* FUNCTION NAME :  VsnapUtil::write_persistent_vsnap_info
*
* DESCRIPTION : Writes Vsnap details to persistent file
*
* INPUT PARAMETERS :  1. File Handle to write to
*					  2. Vsnap Virtual Volume Info 
*					  3. Vsnap Mount Info
*
* OUTPUT PARAMETERS :None
*
* NOTES : 1. Writes Header
*		  2. Writes the vsnap info	
*
* return value :  true on success, false otherwise
*/

bool VsnapUtil::write_persistent_vsnap_info(ACE_HANDLE& handle, VsnapVirtualVolInfo* VirtualInfo, VsnapMountInfo* MountData)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s for %s\n", FUNCTION_NAME, MountData->VirtualVolumeName);

    bool rv = true;

    try 
    {
        do 
        {
            DiskVsnapPersistInfo info;
            const SV_UINT info_len = sizeof(info);

            memset(&info, 0, info_len);

            info.volumesize = MountData->VolumeSize;
            info.sectorsize = MountData->VolumeSectorSize;
            info.recoverytime = MountData->RecoveryTime;
            info.snapshot_id = MountData->SnapShotId;
            info.accesstype = MountData->AccessType;

            //previously we are not using reserved field in DiskVsnapPersistInfo, 
            //but now its being used to store the vsnap type and also the name converted to vsnapType
            info.vsnapType = VirtualInfo->VsnapType;

            info.mapfileinretention = boost::lexical_cast<SV_UINT>(MountData->IsMapFileInRetentionDirectory);

            INM_SAFE_ARITHMETIC(info.mtpt_len = InmSafeInt<size_t>::Type(VirtualInfo->VolumeName.size()) + 1, INMAGE_EX(VirtualInfo->VolumeName.size()))
            INM_SAFE_ARITHMETIC(info.dev_len = InmSafeInt<size_t>::Type(strlen(MountData->VirtualVolumeName)) + 1, INMAGE_EX(strlen(MountData->VirtualVolumeName)))
            INM_SAFE_ARITHMETIC(info.tgt_len = InmSafeInt<size_t>::Type(strlen(MountData->VolumeName)) + 1, INMAGE_EX(strlen(MountData->VolumeName)))
            INM_SAFE_ARITHMETIC(info.tag_len = InmSafeInt<size_t>::Type(VirtualInfo->EventName.size()) + 1, INMAGE_EX(VirtualInfo->EventName.size()))
            INM_SAFE_ARITHMETIC(info.datalog_len = InmSafeInt<size_t>::Type(strlen(MountData->PrivateDataDirectory)) + 1, INMAGE_EX(strlen(MountData->PrivateDataDirectory)))
            INM_SAFE_ARITHMETIC(info.ret_len = InmSafeInt<size_t>::Type(strlen(MountData->RetentionDirectory)) + 1, INMAGE_EX(strlen(MountData->RetentionDirectory)))

            std::string recoverytime;
            CDPUtil::ToDisplayTimeOnConsole(MountData->RecoveryTime, recoverytime);

            INM_SAFE_ARITHMETIC(info.time_len = InmSafeInt<size_t>::Type(recoverytime.size()) + 1, INMAGE_EX(recoverytime.size()))

            VsnapHeaderInfo header;
            memset(&header, 0, sizeof(header));

            header.uHdr.Hdr.magic = VSNAP_MAGIC;
            header.uHdr.Hdr.version = VSNAP_VERSION(VSNAP_MAJOR_VERSION, VSNAP_MINOR_VERSION);	

            const SV_UINT header_len = sizeof(header);

            SV_UINT total_len;
            INM_SAFE_ARITHMETIC(total_len = InmSafeInt<SV_UINT>::Type(info.mtpt_len) + info.dev_len + info.tgt_len + info.tag_len + info.datalog_len + info.ret_len + info.time_len, INMAGE_EX(info.mtpt_len)(info.dev_len)(info.tgt_len)(info.tag_len)(info.datalog_len)(info.ret_len)(info.time_len))

            SV_UINT SIZE;
            INM_SAFE_ARITHMETIC(SIZE = InmSafeInt<SV_UINT>::Type(header_len) + info_len + total_len, INMAGE_EX(header_len)(info_len)(total_len))

            char *bufp = new (nothrow) char[SIZE];

            boost::shared_array<char> buffer(bufp);

            if(!buffer)
            {
                DebugPrintf(SV_LOG_ERROR, "%s %d Insufficient Memory. Unable to persist vsnap info for %s\n",
                    FUNCTION_NAME, LINE_NO, MountData->VirtualVolumeName);
                rv = false;
                break;
            }


            SV_ULONGLONG offset = 0;

            inm_memcpy_s(buffer.get(), SIZE, &header, header_len);
            offset += header_len;


			inm_memcpy_s(buffer.get() + offset, (SIZE - offset), &info, info_len);
            offset += info_len;

			inm_memcpy_s(buffer.get() + offset, (SIZE - offset), VirtualInfo->VolumeName.c_str(), info.mtpt_len);
            offset += info.mtpt_len;


			inm_memcpy_s(buffer.get() + offset, (SIZE - offset), MountData->VirtualVolumeName, info.dev_len);
            offset += info.dev_len;


			inm_memcpy_s(buffer.get() + offset, (SIZE - offset), MountData->VolumeName, info.tgt_len);
            offset += info.tgt_len;


			inm_memcpy_s(buffer.get() + offset, (SIZE - offset), VirtualInfo->EventName.c_str(), info.tag_len);
            offset += info.tag_len;


			inm_memcpy_s(buffer.get() + offset, (SIZE - offset), MountData->PrivateDataDirectory, info.datalog_len);
            offset += info.datalog_len;


			inm_memcpy_s(buffer.get() + offset, (SIZE - offset), MountData->RetentionDirectory, info.ret_len);
            offset += info.ret_len;

			inm_memcpy_s(buffer.get() + offset, (SIZE - offset), recoverytime.c_str(), info.time_len);


            //Write Vsnap Data

            ssize_t byteswrote = ACE_OS::write(handle, buffer.get(), SIZE);

            if(byteswrote != SIZE)
            {
                DebugPrintf(SV_LOG_DEBUG, "Unable to persist vsnap information for %s for target %s\n",
                    MountData->VirtualVolumeName, MountData->VolumeName);
                rv = false;
                break;
            }

        } while(0);

    }
    catch (ContextualException& ce)
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
            "\n%s encountered exception %s.\n",
            FUNCTION_NAME, ce.what());
    }
    catch (exception const& e )
    { 
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
            "\n%s encountered exception %s.\n",
            FUNCTION_NAME, e.what());
    }
    catch ( ... )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
            "\n%s encountered unknown exception.\n",
            FUNCTION_NAME);
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s for %s\n", FUNCTION_NAME, MountData->VirtualVolumeName);

    return rv;
}

//	Bug #8543

/*
* FUNCTION NAME :  VsnapMgr::PersistVsnap
*
* DESCRIPTION : Persist the vsnap information.
*
* INPUT PARAMETERS : VsnapMountInfo
*					 VsnapVirtualVolInfo
*
* OUTPUT PARAMETERS : none
*
* NOTES : 
*		persist vsnap info
*		1. create file
*		2. write version
*		3. write vsnap data
*
* 
*
* return value : true on success, false otherwise
*                
*
*/
bool VsnapMgr::PersistVsnap(VsnapMountInfo* MountData, VsnapVirtualVolInfo* VirtualInfo)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s for %s\n", FUNCTION_NAME, MountData->VirtualVolumeName);

    bool rv = true;

    ACE_HANDLE PersistentFileHandle = ACE_INVALID_HANDLE;

    try
    {
        do 
        {
            //Create file
            if(!VsnapUtil::create_persistent_vsnap_file_handle(MountData->VirtualVolumeName, MountData->VolumeName, PersistentFileHandle))
            {
                DebugPrintf(SV_LOG_DEBUG, "Unable to create persistent vsnap file to store the vsnap information for %s\n", MountData->VirtualVolumeName);
                rv = false;
                break;
            }

            if(!VsnapUtil::write_persistent_vsnap_info(PersistentFileHandle, VirtualInfo, MountData))
            {
                DebugPrintf(SV_LOG_DEBUG, "Unable to write the vsnap information for %s in the persistent file.\n", MountData->VirtualVolumeName);
                rv = false;
                break;
            }

        }while(0);
    }
    catch (ContextualException& ce)
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
            "\n%s encountered exception %s.\n",
            FUNCTION_NAME, ce.what());
    }
    catch (exception const& e )
    { 
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
            "\n%s encountered exception %s.\n",
            FUNCTION_NAME, e.what());
    }
    catch ( ... )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
            "\n%s encountered unknown exception.\n",
            FUNCTION_NAME);
    }

    if(PersistentFileHandle != ACE_INVALID_HANDLE)
        ACE_OS::close(PersistentFileHandle);

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s for %s\n", FUNCTION_NAME, MountData->VirtualVolumeName);

    return rv;
}

/*
* FUNCTION NAME :  VsnapMgr::PersistPendingVsnap
*
* DESCRIPTION : Persist the vsnap information.
*
* INPUT PARAMETERS : Vsnap device name
*					 target volume
*
* OUTPUT PARAMETERS : none
*
* NOTES : 
*		persist vsnap info
*		1. create file
* 
*
* return value : true on success, false otherwise
*                
*
*/
bool VsnapMgr::PersistPendingVsnap(const std::string & deviceName, const std::string & target)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s for %s\n", FUNCTION_NAME, deviceName.c_str());
    bool rv = true;

    ACE_HANDLE PersistentFileHandle = ACE_INVALID_HANDLE;
    do
    {
        //Create file
        if(!VsnapUtil::create_pending_persistent_vsnap_file_handle(deviceName, target, PersistentFileHandle))
        {
            DebugPrintf(SV_LOG_DEBUG, "Unable to create pending vsnap file handle for %s\n", deviceName.c_str());
            rv = false;
            break;
        }
        if(PersistentFileHandle != ACE_INVALID_HANDLE)
            ACE_OS::close(PersistentFileHandle);
    }while(0);
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s for %s\n", FUNCTION_NAME, deviceName.c_str());
    return rv;
}
/*
* FUNCTION NAME : VsnapUtil::match_persistent_vsnap__filename
*
* DESCRIPTION : matches the target volumename and vsnapname with those obtained from the persistent filename
*
* INPUT PARAMETERS : 
*					a. Replication Target Name from filename
*					b. Vsnap Device Name from filename
*					c. match   "all"	- information about all vsnaps is desired
*	 				           "target" - information about vsnaps associated with target 
*							   "this"	- information about this specific vsnap
*					d. Replication Target to match
*					e. Vsnap Device Name to match
*					  
*
* OUTPUT PARAMETERS : None
*
* NOTES : true if success else false
* 
*
* return value :  true on success, false otherwise
*/
bool VsnapUtil::match_persistent_vsnap_filename(const std::string& parent, const std::string& vsnapname, const std::string& match,
                                                const std::string& target_to_match, const std::string& devicename_to_match)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s for %s, %s\n", FUNCTION_NAME, parent.c_str(), vsnapname.c_str());

    bool rv = true;

    do 
    {
        std::string targetvol = parent;
        std::string vsnapvol = vsnapname;

        if(!targetvol.empty())
        {
            FormatVolumeName(targetvol);
            FormatVolumeNameToGuid(targetvol);
            ExtractGuidFromVolumeName(targetvol);
            GetDeviceNameFromSymLink(targetvol);
        }

        if(!vsnapvol.empty())
        {
            FormatVolumeName(vsnapvol);
            FormatVolumeNameToGuid(vsnapvol);
            ExtractGuidFromVolumeName(vsnapvol);
        }

        if(match == "target" && target_to_match != targetvol)
        {
            rv = false;
            break;
        }
        else if(match == "this")
        {
            if((devicename_to_match != vsnapvol)
                || ((devicename_to_match == vsnapvol) && (!target_to_match.empty()) && (target_to_match != targetvol)))
            {
                rv = false;
                break;
            }
        }


    } while (0);

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s for %s, %s\n", FUNCTION_NAME, parent.c_str(), vsnapname.c_str());

    return rv;	
}

/*
* FUNCTION NAME :  VsnapUtil::read_persistent_vsnap_info
*
* DESCRIPTION : Read Vsnap details from persistent file
*
* INPUT PARAMETERS :  persistent filename
*					 
*					  
*
* OUTPUT PARAMETERS :VsnapPersistInfo (which contains the details of vsnap) 
*
* NOTES : 1. Reads Header
*		  2. If the version is supported, reads the vsnap info	
*
* return value :  true on success, false otherwise
*/
bool VsnapUtil::read_persistent_vsnap_info(const std::string& filename, VsnapPersistInfo& persist_info)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED: %s for %s\n", FUNCTION_NAME, filename.c_str());

    bool rv = true;

    try 
    {
        do 
        {
            // PR#10815: Long Path support
            std::ifstream InputStream(getLongPathName(filename.c_str()).c_str(),std::ios :: in | std::ios::binary);

            if(!InputStream.is_open())
            {
                DebugPrintf(SV_LOG_ERROR, "Unable to open persistent file.\n");
                rv = false;
                break;
            }
            // Find  the size of the stream
            InputStream.seekg (0, std::ios::end);
            SV_UINT filesize = InputStream.tellg();
            InputStream.seekg (0, std::ios::beg);

            char *bufp = new (nothrow) char[filesize];
            boost::shared_array<char> buffer(bufp);

            if(!buffer)
            {
                DebugPrintf(SV_LOG_ERROR, "Unable to read persistent file %s.\n", filename.c_str());
                rv = false;
                break;
            }

            InputStream.read (buffer.get(),(std::streamsize)filesize);
            if(InputStream.fail())
            {
                DebugPrintf(SV_LOG_ERROR, "Unable to read persistent file %s.\n", filename.c_str());
                rv = false;
                break;
            }

            InputStream.close();

            VsnapHeaderInfo header;
            const SV_USHORT header_len = sizeof(header);

            DiskVsnapPersistInfo info;


            SV_ULONGLONG offset = 0;

            inm_memcpy_s(&header, header_len, buffer.get(), header_len);
            offset += header_len;


            if(!(header.uHdr.Hdr.magic == VSNAP_MAGIC && header.uHdr.Hdr.version == VSNAP_VERSION(VSNAP_MAJOR_VERSION, VSNAP_MINOR_VERSION)))
            {
                DebugPrintf(SV_LOG_ERROR, "%s %d Incorrect version found while reading vsnap persistent header for %s\n", FUNCTION_NAME,
                    LINE_NO, filename.c_str());
                rv = false;
                break;
            }

            inm_memcpy_s(&info, sizeof(info), buffer.get()+offset, sizeof(info));
            offset += sizeof(info);


            persist_info.accesstype = info.accesstype;
            persist_info.mapfileinretention = info.mapfileinretention;
            persist_info.recoverytime = info.recoverytime;
            persist_info.sectorsize = info.sectorsize;
            persist_info.snapshot_id = info.snapshot_id;
            persist_info.volumesize = info.volumesize;

            //previously we are not using reserved field in VsnapPersistInfo, 
            //but now its being used to store the vsnap type and also the name converted to vsnapType
            persist_info.vsnapType = info.vsnapType;

            persist_info.mountpoint = (buffer.get() + offset);
            offset += info.mtpt_len;

            persist_info.devicename = (buffer.get() + offset);
            offset += info.dev_len;

            persist_info.target = (buffer.get() + offset);
            offset += info.tgt_len;

            persist_info.tag = (buffer.get() + offset);
            offset += info.tag_len;

            persist_info.datalogpath = (buffer.get() + offset);
            offset += info.datalog_len;

            persist_info.retentionpath = (buffer.get() + offset);
            offset += info.ret_len;

            persist_info.displaytime = (buffer.get() + offset);


        }while(0);

    }
    catch (ContextualException& ce)
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
            "\n%s encountered exception %s.\n",
            FUNCTION_NAME, ce.what());
    }
    catch (exception const& e )
    { 
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
            "\n%s encountered exception %s.\n",
            FUNCTION_NAME, e.what());
    }
    catch ( ... )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
            "\n%s encountered unknown exception.\n",
            FUNCTION_NAME);
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s for %s\n", FUNCTION_NAME, filename.c_str());

    return rv;
}

//	Bug #8543

/*
* FUNCTION NAME :  VsnapMgr::RetrieveVsnapInfo
*
* DESCRIPTION : Retrieves the details of the vsnaps
*
* INPUT PARAMETERS : match - "all" - information about all vsnaps is desired
*					 match - "target"  - information about vsnaps associated with target 
*					 match - "this" - information about this specific vsnap
*					 
*					 target - target volume name
*					 vsnap - vsnap name
*					  
*
* OUTPUT PARAMETERS : 
*
* NOTES : None
* 
*
* return value :  true on success, false otherwise
*/
bool VsnapMgr::RetrieveVsnapInfo(std::vector<VsnapPersistInfo>& vsnaps, const std::string& match, const std::string& targetvol, const std::string& device)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s for %s, %s\n", FUNCTION_NAME, targetvol.c_str(), device.c_str());

    bool rv = true;
    bool found = false;

    ACE_DIR *dir = NULL;

    try
    {
        do 
        {
            LocalConfigurator localConfigurator;
            std::string location = localConfigurator.getVsnapConfigPathname();
            location += ACE_DIRECTORY_SEPARATOR_CHAR_A;

            std::string target = targetvol;
            std::string devicename = device;

            if(!target.empty())
            {
                FormatVolumeNameForCxReporting(target);

                if (IsReportingRealNameToCx())
                {
                    GetDeviceNameFromSymLink(target);
                }
                else
                {
                    std::string srclinkname = target;
                    if (!GetLinkNameIfRealDeviceName(target, srclinkname))
                    {
                        DebugPrintf(SV_LOG_ERROR, "Invalid target volume specified\n");
                        rv = false;
                        break;
                    } 
                    target = srclinkname; 
                }

                target = VsnapUtil::construct_vsnap_persistent_name(target);
            }

            if(!devicename.empty())
            {
                FormatVolumeNameForCxReporting(devicename);

                if(devicename[0] == '/' && (devicename.substr(0,5) != "/dev/"))
                {
                    std::string mountpoint = devicename;
                    devicename.clear();
                    char volume[2048];
                    memset(volume, 0, 2048);
                    inm_strncpy_s(volume, ARRAYSIZE(volume), mountpoint.c_str(), mountpoint.size());	
                    TruncateTrailingBackSlash(volume);	
                    GetDeviceNameForMountPt(volume, devicename);
                }
                devicename = VsnapUtil::construct_vsnap_persistent_name(devicename);
            }

            struct ACE_DIRENT *dp = NULL;
            // PR#10815: Long Path support
            dir = sv_opendir(location.c_str());

            // Check if we could successfully open the directory
            if (dir == NULL)
            {
                if (ACE_OS::last_error() != ENOENT)
                {
                    // Get the error and display the error
                    DebugPrintf(SV_LOG_ERROR, " %s %d opendir failed, path = %s, errno = 0x%x\n", 
                        FUNCTION_NAME, LINE_NO,  location.c_str(), ACE_OS::last_error());
                    rv = false;
                }
                break;
            }

            //Get directory entry for location
            while ((dp = ACE_OS::readdir(dir)) != NULL)
            {
                std::string d_name = ACE_TEXT_ALWAYS_CHAR(dp->d_name);

                // we ignore the "." and ".." files
                if ( d_name == "." || d_name == ".." )
                    continue;

                if ( match == "target" || match == "this" )
                {
                    if(!target.empty())
#ifdef SV_WINDOWS
                        if( 0 != stricmp(d_name.c_str(), target.c_str()))
#else
                        if( 0 != strcmp(d_name.c_str(), target.c_str()))
#endif
                            continue;
                }

                std::string targetDirName = location + d_name;
                ACE_stat statBuff;
                if( (sv_stat(getLongPathName(targetDirName.c_str()).c_str(), &statBuff) == 0) )
                {
                    if( (statBuff.st_mode & S_IFDIR) != S_IFDIR ) 
                        continue;
                }

                // PR#10815: Long Path support
                ACE_DIR *targetDir = sv_opendir(targetDirName.c_str());
                if( targetDir == NULL )
                {
                    DebugPrintf(SV_LOG_ERROR, "opendir() failed for %s error no: %d\n",
                        targetDirName.c_str(), ACE_OS::last_error());
                    rv = false;
                    break;
                }

                struct ACE_DIRENT *dirEnt = NULL;
                while( (dirEnt = ACE_OS::readdir(targetDir)) != NULL )
                {
                    std::string filename = ACE_TEXT_ALWAYS_CHAR(dirEnt->d_name);

                    // we ignore the "." and ".." files
                    if ( filename == "." || filename == ".." )
                        continue;

                    if(match == "this")
                    {
#ifdef SV_WINDOWS
                        if( 0 != stricmp(filename.c_str(), devicename.c_str()))
#else
                        if( 0 != strcmp(filename.c_str(), devicename.c_str()))
#endif
                            continue;
                    }

                    filename = targetDirName + ACE_DIRECTORY_SEPARATOR_CHAR_A + filename;

                    VsnapPersistInfo vsnapinfo;
                    if(!VsnapUtil::read_persistent_vsnap_info(filename, vsnapinfo))
                    {
                        DebugPrintf(SV_LOG_ERROR, "Unable to retrieve vsnap info from %s\n", filename.c_str());
                        rv = false;
                        break;
                    }

                    vsnaps.push_back(vsnapinfo);

                    // For only one vsnap, we needn't read additional filenames

                    if(match == "this")
                    {
                        found = true;
                        break;
                    }
                }
                if(targetDir)
                    ACE_OS::closedir(targetDir);

                if(found || !rv)
                    break;
            }

        }while(0);

        if(dir)
            ACE_OS::closedir(dir);
    }
    catch (ContextualException& ce)
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
            "\n%s encountered exception %s.\n",
            FUNCTION_NAME, ce.what());
    }
    catch (exception const& e )
    { 
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
            "\n%s encountered exception %s.\n",
            FUNCTION_NAME, e.what());
    }
    catch ( ... )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
            "\n%s encountered unknown exception.\n",
            FUNCTION_NAME);
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s for %s, %s\n", FUNCTION_NAME, targetvol.c_str(), device.c_str());

    return rv;
}

/*
* FUNCTION NAME :  VsnapMgr::RetrievePendingVsnapInfo
*
* DESCRIPTION : Retrieves the created and deleted pending vsnaps not communicated to cx
*
* INPUT PARAMETERS : none
*
* OUTPUT PARAMETERS : list of pending created vsnap not updated to cx
*					  list of deleted vsnap not updated to cx
*
* NOTES : None
* 
*
* return value :  true on success, false otherwise
*/

bool VsnapMgr::RetrievePendingVsnapInfo(std::vector<VsnapPersistInfo>& createdvsnaps, std::vector<VsnapDeleteInfo>& deletedVsnap,SV_UINT nBatchRequest)
{
    bool rv = true;
    bool limitReached = false;
    ACE_DIR *dir = NULL;
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n",FUNCTION_NAME);
    try
    {
        do
        {
            LocalConfigurator localConfigurator;
            std::string pendingLocation = localConfigurator.getVsnapPendingPathname();
            pendingLocation += ACE_DIRECTORY_SEPARATOR_CHAR_A;
            std::string persistLocation = localConfigurator.getVsnapConfigPathname();
            persistLocation += ACE_DIRECTORY_SEPARATOR_CHAR_A;
            struct ACE_DIRENT *dp = NULL;
            // PR#10815: Long Path support
            dir = sv_opendir(pendingLocation.c_str());
            if (dir == NULL)
            {
                if (ACE_OS::last_error() != ENOENT)
                {
                    // Get the error and display the error
                    DebugPrintf(SV_LOG_ERROR, " %s %d opendir failed, path = %s, errno = 0x%x\n",
                        FUNCTION_NAME, LINE_NO,  pendingLocation.c_str(), ACE_OS::last_error());
                    rv = false;
                }
                break;
            }

            while ((dp = ACE_OS::readdir(dir)) != NULL)
            {
                std::string d_name = ACE_TEXT_ALWAYS_CHAR(dp->d_name);

                if ( d_name == "." || d_name == ".." )
                    continue;

                std::string pendingTargetDirName = pendingLocation + d_name;
                ACE_stat statBuff;
                if( (sv_stat(getLongPathName(pendingTargetDirName.c_str()).c_str(), &statBuff) == 0) )
                {
                    if( (statBuff.st_mode & S_IFDIR) != S_IFDIR ) 
                        continue;
                }

                // PR#10815: Long Path support
                ACE_DIR *pendingTargetDir = sv_opendir(pendingTargetDirName.c_str());
                if( pendingTargetDir == NULL )
                {
                    DebugPrintf(SV_LOG_ERROR, "Opendir() failed for %s error no: %d\n",
                        pendingTargetDirName.c_str(), ACE_OS::last_error());
                    rv = false;
                    break;
                }

                struct ACE_DIRENT *pendingDirEnt = NULL;
                while( (pendingDirEnt = ACE_OS::readdir(pendingTargetDir)) != NULL )
                {
                    std::string pendingFilename = ACE_TEXT_ALWAYS_CHAR(pendingDirEnt->d_name);

                    if ( pendingFilename == "." || pendingFilename == ".." )
                        continue;

                    std::string persistfilename = persistLocation + d_name + ACE_DIRECTORY_SEPARATOR_CHAR_A;
                    persistfilename += pendingFilename;
                    ACE_stat statbuf = {0};

                    // PR#10815: Long Path support
                    if ((0 == sv_stat(getLongPathName(persistfilename.c_str()).c_str(), &statbuf)) && ( (statbuf.st_mode & S_IFREG) == S_IFREG ) )
                    {
                        if(createdvsnaps.size() < nBatchRequest)
                        {
                            VsnapPersistInfo vsnapinfo;
                            if(!VsnapUtil::read_persistent_vsnap_info(persistfilename, vsnapinfo))
                            {
                                DebugPrintf(SV_LOG_ERROR, "Unable to retrieve pending vsnap info from %s\n", persistfilename.c_str());
                                rv = false;
                                break;
                            }
                            createdvsnaps.push_back(vsnapinfo);
                        }

                    }
                    else
                    {
                        if(deletedVsnap.size() < nBatchRequest)
                        {
                            std::string parent = d_name;
                            std::string vsnapname = pendingFilename;

                            if(parent.length() >= 2 && parent[1] == '.')
                                parent[1] = ':';
                            if(vsnapname.length() >= 2 && vsnapname[1] == '.')
                                vsnapname[1] = ':';

                            replace(parent.begin(), parent.end(), '.' , ACE_DIRECTORY_SEPARATOR_CHAR_A);
                            replace(vsnapname.begin(), vsnapname.end(), '.' , ACE_DIRECTORY_SEPARATOR_CHAR_A);

                            VsnapDeleteInfo vsnapDeleteInfo;
                            vsnapDeleteInfo.target_device = parent;
                            vsnapDeleteInfo.vsnap_device = vsnapname;
                            vsnapDeleteInfo.status = 0;
                            deletedVsnap.push_back(vsnapDeleteInfo);
                        }
                    }
                    if((deletedVsnap.size() == nBatchRequest) && (createdvsnaps.size() == nBatchRequest))
                    {
                        limitReached = true;
                        break;
                    }
                }

                if(pendingTargetDir)
                    ACE_OS::closedir(pendingTargetDir);

                if( limitReached || !rv )
                    break;
            }
        }while(0);
        if(dir)
            ACE_OS::closedir(dir);

    }
    catch (ContextualException& ce)
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR,
            "\n%s encountered exception %s.\n",
            FUNCTION_NAME, ce.what());
    }
    catch (exception const& e )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR,
            "\n%s encountered exception %s.\n",
            FUNCTION_NAME, e.what());
    }
    catch ( ... )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR,
            "\n%s encountered unknown exception.\n",
            FUNCTION_NAME);
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n",FUNCTION_NAME);
    return rv;
}

bool VsnapMgr::deletePendingPersistentVsnap(const std::string& targetvol)
{
    bool rv = true;
    ACE_DIR *dir = NULL;
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s for the target %s\n",FUNCTION_NAME,targetvol.c_str());
    std::string target = targetvol;
    std::vector<std::string> FullFileName;
    do
    {
        LocalConfigurator localConfigurator;
        std::string location =  localConfigurator.getVsnapPendingPathname();
        location += ACE_DIRECTORY_SEPARATOR_CHAR_A;
        if(!target.empty())
        {
            FormatVolumeName(target);

            if (IsReportingRealNameToCx())
            {
                GetDeviceNameFromSymLink(target);
            }
            else
            {
                std::string srclinkname = target;
                if (!GetLinkNameIfRealDeviceName(target, srclinkname))
                {
                    DebugPrintf(SV_LOG_ERROR, "Invalid target volume specified\n");
                    rv = false;
                    break;
                } 
                target = srclinkname; 
            }

            target = VsnapUtil::construct_vsnap_persistent_name(target);
        }
        else
            break;
        location += target;
        location += ACE_DIRECTORY_SEPARATOR_CHAR_A;

        struct ACE_DIRENT *dp = NULL;
        // PR#10815: Long Path support
        dir = sv_opendir(location.c_str());
        if((dir == NULL))
            break;
        while ((dp = ACE_OS::readdir(dir)) != NULL)
        {
            std::string filename = ACE_TEXT_ALWAYS_CHAR(dp->d_name);
            if ( filename == "." || filename == ".." )
                continue;

            std::string filePath = location + filename;
            // PR#10815: Long Path support
            if((ACE_OS::unlink(getLongPathName(filePath.c_str()).c_str()) < 0)&& (ACE_OS::last_error() != ENOENT))
            {
                std::stringstream errstr;
                errstr << "Unable to delete pending file " << filePath 
                    << " with error number " << ACE_OS::last_error() << "\n";
                DebugPrintf(SV_LOG_DEBUG, "%s", errstr.str().c_str());
            }
        }

        if(dir)
            ACE_OS::closedir(dir);
    }while(false);
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s for the target %s\n",FUNCTION_NAME,targetvol.c_str());
    return rv;
}
/*
* FUNCTION NAME :  VsnapMgr::RemountVsnapsFromPersistentStore
*
* DESCRIPTION : Recreates the vsnaps by retrieving their information 
*				from files created for this purpose.
*
*
* INPUT PARAMETERS : None
*                    
*                    
* OUTPUT PARAMETERS : None
*                     
*
* NOTES : Retrieves the vsnap information from persistent store and recreates(remounts)
* the vsnaps if they aren't already created.
*
* return value : true if success, else false
*
*/
bool VsnapMgr::RemountVsnapsFromPersistentStore()
{
    bool rv = true;
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    try{

        do 
        {
            // Retrieve the list of vsnaps to be mounted
            // Remount the vsnaps if required

            std::vector<VsnapPersistInfo> vsnaps;
            if(!RetrieveVsnapInfo(vsnaps, "all"))
            {
                DebugPrintf(SV_LOG_DEBUG, "Failed to retrieve vsnap information\n");
                rv = false;
                break;
            }

            std::vector<VsnapPersistInfo>::iterator iter = vsnaps.begin();
            for(; iter != vsnaps.end(); ++iter)
            {
                DebugPrintf(SV_LOG_DEBUG, "Vsnap: Remount: %s\n", iter->devicename.c_str());
                if(NeedToRecreateVsnap(iter->devicename))
                {
                    std::ostringstream out;
                    PrintVsnapInfo(out, (*iter));
                    DebugPrintf(SV_LOG_DEBUG, "%s", out.str().c_str());
                    if(!RemountV2((*iter)))
                    {
                        DebugPrintf(SV_LOG_ERROR, "Failed to remount the vsnap %s\n", iter->devicename.c_str());
						rv = false;
                    }
                }
                else
                {
                    StatAndMount(*iter);
                }
            }

        }while(0);

    }
    catch(...) {
		rv = false;
        DebugPrintf(SV_LOG_ERROR,"FAILED: Unable to remount virtual snapshot volumes. @LINE %d in FILE %s\n",LINE_NO,FILE_NAME);
    }
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}

/*
* FUNCTION NAME :  VsnapMgr::RemountVsnapsFromCX
*
* DESCRIPTION : Recreates the vsnaps by retrieving their information 
*				from CX
*
*
* INPUT PARAMETERS : None
*                    
*                    
* OUTPUT PARAMETERS : None
*                     
*
* NOTES : Retrieves the vsnap information from CX and recreates(remounts)
* the vsnaps if they aren't already created.
*
* return value : true if success, else false
*
*/
bool VsnapMgr::RemountVsnapsFromCX()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    try{

        std::string LogDir = "";

        SetServerAddress(GetCxIpAddress().c_str());
        SetServerPort(m_TheConfigurator->getVxTransport().getHttp().port);
        SetServerId(m_TheConfigurator->getVxAgent().getHostId().c_str());

        VsnapRemountVolumes vsnapRemountList;
        bool rv = getVsnapRemountVolumes((*m_TheConfigurator), vsnapRemountList);
        if(!rv)
        {
            DebugPrintf(SV_LOG_ERROR,"FAILED: Unable to detect list of virtual snapshot volumes. @LINE %d in FILE %s\n",LINE_NO,FILE_NAME);
            return false;
        }
        VsnapRemountVolumes::iterator iterRemountVolume;

        for(iterRemountVolume= vsnapRemountList.begin();iterRemountVolume!=vsnapRemountList.end();iterRemountVolume++)
        {
            DebugPrintf(SV_LOG_DEBUG, "Vsnap: Remount: \n");

            VsnapSourceVolInfo      SrcInfo;
            VsnapVirtualVolInfo     MountVirtualInfo;

            SrcInfo.VolumeName=(*iterRemountVolume).second.src; //toupper(*(char *)token);

            if(strlen(SrcInfo.VolumeName.c_str())<3)
                SrcInfo.VolumeName += ":";

            string SrcVolume=SrcInfo.VolumeName;
            SrcVolume.erase( std::remove_if( SrcVolume.begin(), SrcVolume.end(), IsColon ), SrcVolume.end() );

            //VSNAPFC - Persistence
            //Added #ifdef so that autogenerated devicename is used for linux
            //dest - /dev/vs/cx?
            //destMountPoint - /mnt/vsnap1


#ifdef SV_WINDOWS		

            //Virtual Vol
            MountVirtualInfo.VolumeName = (*iterRemountVolume).second.dest; //token;

#else
            MountVirtualInfo.VolumeName = (*iterRemountVolume).second.destMountPoint;
            MountVirtualInfo.DeviceName = (*iterRemountVolume).second.dest;

#endif

#ifdef SV_WINDOWS

            if(strlen(MountVirtualInfo.VolumeName.c_str())<3)
                MountVirtualInfo.VolumeName+=":";

#endif

            try
            {
                MountVirtualInfo.SnapShotId  = boost::lexical_cast<SV_ULONGLONG>((*iterRemountVolume).second.vsnapId.c_str());
            }
            catch (boost::bad_lexical_cast& e)
            {
                DebugPrintf(SV_LOG_WARNING, "CDPCli::vsnap lecial_cast failed for %s: %s\n", (*iterRemountVolume).second.vsnapId.c_str(), e.what());
            }


            if ( (*iterRemountVolume).second.recoveryPoint == "NULL")
                MountVirtualInfo.TimeToRecover = 0;
            else
            {
                SV_TIME svtime;
                sscanf((*iterRemountVolume).second.recoveryPoint.c_str(), "%hd,%hd,%hd,%hd,%hd,%hd,%hd,%hd,%hd",
                    &svtime.wYear,
                    &svtime.wMonth,
                    &svtime.wDay,
                    &svtime.wHour,
                    &svtime.wMinute,
                    &svtime.wSecond,
                    &svtime.wMilliseconds,
                    &svtime.wMicroseconds,
                    &svtime.wHundrecNanoseconds);


                SV_ULONGLONG  ftime;
                CDPUtil::ToFileTime(svtime, ftime);
                MountVirtualInfo.TimeToRecover = ftime;
            }

            if((*iterRemountVolume).second.vsnapMountOption == 1)
                MountVirtualInfo.AccessMode = VSNAP_READ_WRITE;
            else
                MountVirtualInfo.AccessMode = VSNAP_READ_ONLY;

            if( "NULL" == (*iterRemountVolume).second.dataLogPath)
                MountVirtualInfo.PrivateDataDirectory = "";
            else
                MountVirtualInfo.PrivateDataDirectory = (*iterRemountVolume).second.dataLogPath;

            if( "NULL" == (*iterRemountVolume).second.retLogPath)
                SrcInfo.RetentionDbPath = "";
            else
            {
#ifdef SV_WINDOWS
                LogDir = (*iterRemountVolume).second.retLogPath + "\\" + m_TheConfigurator->getVxAgent().getHostId() + "\\" + SrcVolume;
#else
                LogDir = (*iterRemountVolume).second.retLogPath + '/' + m_TheConfigurator->getVxAgent().getHostId() +  SrcVolume;

#endif                   
                SrcInfo.RetentionDbPath = LogDir;
            }

            SrcInfo.VirtualVolumeList.push_back(&MountVirtualInfo);
            Remount(&SrcInfo);

        }
        return true;
    }
    catch(...) {
        DebugPrintf(SV_LOG_ERROR,"FAILED: Unable to detect list of virtual snapshot volumes. @LINE %d in FILE %s\n",LINE_NO,FILE_NAME);
    }
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return true;
}


/*
* FUNCTION NAME : construct_vsnap_filename
*
* DESCRIPTION : Constructs filename from vsnap devicename and replication target
*				
*
*
* INPUT PARAMETERS : a. devicename - vsnap devicename
*                    b. target - replication target
*                    
* OUTPUT PARAMETERS : none
*                     
*
* NOTES : 
* 
*
* return value : name of the file
*
*/

std::string VsnapUtil::construct_vsnap_filename( const std::string& devicename, const std::string& target)
{
    std::string virtualDeviceName = devicename;
    std::string targetVolumeName = target;
    std::string vsnapFile;

    FormatVolumeNameForCxReporting(targetVolumeName);
    targetVolumeName = construct_vsnap_persistent_name(targetVolumeName);

    FormatVolumeNameForCxReporting(virtualDeviceName);
    virtualDeviceName = construct_vsnap_persistent_name(virtualDeviceName);

    vsnapFile = targetVolumeName;
    vsnapFile += ACE_DIRECTORY_SEPARATOR_CHAR_A;
    vsnapFile += virtualDeviceName;
    return vsnapFile;
}

/*
* FUNCTION NAME : construct_vsnap_persistent_name
*
* DESCRIPTION : Constructs vsnap devicename or target volume name that can be
*               used while reading or creating the persistent store
*
* INPUT PARAMETERS : name - vsnap devicename or target volume name
*                    
* OUTPUT PARAMETERS : none
*
* NOTES : 
*
* return value : name converted to persistent format
*/
std::string VsnapUtil::construct_vsnap_persistent_name(const std::string& name)
{
    std::string persistentName = name;

    replace(persistentName.begin(), persistentName.end(), ':', '.');
    replace(persistentName.begin(), persistentName.end(), '\\', '.');
    replace(persistentName.begin(), persistentName.end(), '/', '.');

    //Fix for Bug#11400
    //On Linux:
    //CDPUtil::trim() discards the specified chars from both starting and ending of the string.
    //For example: Say vsnap target vol name is '/dev/sdb1'. After replacing '/'s with '.'s
    //and trimming '.'s, the persistence file name becomes 'dev.sdb1'. When this is converted
    //back to device name for sending the updates to cx, it becomes 'dev/sdb1' which is not what
    //cx has. So the trim() should no be done on linux/solaris.
    //
    //On Windows:
    //CDPUtil::trim() is needed for windows for fixing the bug#10956
#ifdef SV_WINDOWS
    CDPUtil::trim(persistentName, ".");
#endif

    return persistentName;
}

/*
* FUNCTION NAME : construct_persistent_vsnap_filename
*
* DESCRIPTION : Constructs persistent filename from vsnap devicename and replication target
*				
*
* INPUT PARAMETERS : a. devicename - vsnap devicename
*                    b. target - replication target
*                    
* OUTPUT PARAMETERS : name of the file
*                     
*
* NOTES : 
* 
*
* return value : true on success otherwise false
*
*/

bool VsnapUtil::construct_persistent_vsnap_filename(const std::string& devicename, const std::string& target, std::string& filename)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s for %s\n", FUNCTION_NAME, devicename.c_str());

    bool rv = true;

    try {

        LocalConfigurator localConfigurator;
        std::string location = localConfigurator.getVsnapConfigPathname();
        location += ACE_DIRECTORY_SEPARATOR_CHAR_A;

        filename = location + construct_vsnap_filename(devicename,target);

    }
    catch (...) {
        DebugPrintf(SV_LOG_ERROR, "The key name VsnapConfigPathname does not exist in the configuration file (drscout.conf).\n");
        rv = false;
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s for %s\n", FUNCTION_NAME, devicename.c_str());

    return rv;
}

/*
* FUNCTION NAME : construct_pending_persistent_vsnap_filename
*
* DESCRIPTION : Constructs pending filename from vsnap devicename and replication target
*				
*
* INPUT PARAMETERS : a. devicename - vsnap devicename
*                    b. target - replication target
*                    
* OUTPUT PARAMETERS : name of the file
*                     
*
* NOTES : 
* 
*
* return value : true on success otherwise false
*
*/

bool VsnapUtil::construct_pending_persistent_vsnap_filename(const std::string& devicename, const std::string& target, std::string& filename )
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s for %s\n", FUNCTION_NAME, devicename.c_str());
    bool rv = true;
    try {
        LocalConfigurator localConfigurator;
        std::string location = localConfigurator.getVsnapPendingPathname();
        location += ACE_DIRECTORY_SEPARATOR_CHAR_A;
        filename = location + construct_vsnap_filename(devicename,target);
    }
    catch (...) {
        DebugPrintf(SV_LOG_ERROR, "The key name VsnapPendingPathname does not exist in the configuration file (drscout.conf).\n");
        rv = false;
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s for %s\n", FUNCTION_NAME, devicename.c_str());

    return rv;
}

/*
* FUNCTION NAME : delete_pending_persistent_vsnap_filename
*
* DESCRIPTION : delete the pending vsnap
*				
*
* INPUT PARAMETERS : a. devicename - vsnap devicename
*                    b. target - replication target
*                    
* OUTPUT PARAMETERS : none
*                     
*
* NOTES : 
* 
*
* return value : true on success otherwise false
*
*/

bool VsnapUtil::delete_pending_persistent_vsnap_filename(const std::string& devicename, const std::string& target)
{
    bool rv = true;
    std::ostringstream errstr;
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s for %s\n", FUNCTION_NAME, devicename.c_str());
    do
    {
        std::string filename;
        if(!construct_pending_persistent_vsnap_filename(devicename,target,filename))
        {
            errstr << "Failed to construct pending file " << devicename
                << " " << FUNCTION_NAME << " " << LINE_NO << std::endl;
            DebugPrintf(SV_LOG_DEBUG, "%s", errstr.str().c_str());
            rv = false;
            break;
        }

        // PR#10815: Long Path support
        if((ACE_OS::unlink(getLongPathName(filename.c_str()).c_str())<0) && (ACE_OS::last_error() != ENOENT))
        {
            errstr << "Unable to delete pending vsnap for " << devicename << " with error number "
                << ACE_OS::last_error() << "\n";
            DebugPrintf(SV_LOG_DEBUG, "%s", errstr.str().c_str());
            rv = false;
            break;
        }
    }while(false);
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s for %s\n", FUNCTION_NAME, devicename.c_str());
    return rv;
}


/*
************ THIS IS AN OBSOLETE FUNCTION ONLY USED IN CASE OF UPGRADE ********
*
* FUNCTION NAME : VsnapUtil::parse_persistent_vsnap_filename
*
* DESCRIPTION : extracts vsnap devicename and target volumename from filename
*				
*
*
* INPUT PARAMETERS : name of the persistent file
*                    
*                    
* OUTPUT PARAMETERS : a. Vsnap Device Name
*                     b. Replication Target
*
* NOTES : 
* 
*
* return value : true if success, else false
*
*/
bool VsnapUtil::parse_persistent_vsnap_filename(const std::string& filename, std::string& target, std::string& devicename)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s for %s\n", FUNCTION_NAME, filename.c_str());

    bool rv = true;

    try {

        do
        {
            svector_t filenames = CDPUtil::split(filename, "__--__");

            if(filenames.size() < 2)
            {
                DebugPrintf(SV_LOG_DEBUG,"Failed to parce filename %s\n", filename.c_str());
                rv = false;
                break;
            }

            target = filenames[0];
            devicename = filenames[1];

            if(target.length() >= 2 && target[1] == '.')
                target[1] = ':';
            if(devicename.length() >= 2 && devicename[1] == '.')
                devicename[1] = ':';

            replace(target.begin(), target.end(), '.' , ACE_DIRECTORY_SEPARATOR_CHAR_A);
            replace(devicename.begin(), devicename.end(), '.' , ACE_DIRECTORY_SEPARATOR_CHAR_A);

        }while(false);

    }
    catch (...) {
        DebugPrintf(SV_LOG_ERROR, "%s %d Unable to parse filename %s \n",
            FUNCTION_NAME, LINE_NO, filename.c_str());
        rv = false;
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s for %s\n", FUNCTION_NAME, filename.c_str());
    return rv;
}

/*
* FUNCTION NAME :  VsnapMgr::PrintVsnapInfo
*
* DESCRIPTION : Print information about a particular vsnap
*
*
* INPUT PARAMETERS : VsnapPersistInfo
*                    
*                    
* OUTPUT PARAMETERS : none
*                     
*
* NOTES : Displays details such as target vol of replication,
*			 Recovery Time, Tag, CDP Location, Private Data Location etc
*   
*
* return value : None
*
*/
void VsnapMgr::PrintVsnapInfo(std::ostringstream& out, const VsnapPersistInfo &vsnapinfo )
{

    out << "------------------------------------------------------\n";

    out << setw(26) << setiosflags(ios::left) << "Device:" << vsnapinfo.devicename << std::endl;
    std::string sMount = "";
    std::string mode;
    if(!vsnapinfo.mountpoint.empty()){
        if(IsVolumeMounted(vsnapinfo.devicename,sMount,mode))
        {
            out << setw(26) << setiosflags(ios::left) << "Mount Point:" << vsnapinfo.mountpoint << std::endl;
        }
    }

    out << setw(26) << setiosflags(ios::left) << "Target Volume:" << vsnapinfo.target << std::endl;

    out << setw(26) << setiosflags(ios::left) << "Size:" << vsnapinfo.volumesize << " bytes" << std::endl;

    std::string displaytime;
    CDPUtil::ToDisplayTimeOnConsole(vsnapinfo.recoverytime, displaytime);

    if(vsnapinfo.recoverytime == 0)
    {
        out << setw(26)  << setiosflags(ios::left) << "Recovery Time:" << "PIT [Point in time]" << std::endl;
    }
    else
    {
        out << setw(26)  << setiosflags(ios::left) << "Recovery Time:" << vsnapinfo.displaytime << std::endl;
    }

    if(!vsnapinfo.tag.empty())
        out << setw(26)  << setiosflags(ios::left) << "Recovery Tag:" << vsnapinfo.tag << std::endl;

    out << setw(26)  << setiosflags(ios::left) << "Data Log Path:" << vsnapinfo.datalogpath << std::endl;

    out << setw(26)  << setiosflags(ios::left) << "CDP Location:" << vsnapinfo.retentionpath << std::endl;

    std::string accesstype;

    switch(vsnapinfo.accesstype)
    {
    case VSNAP_READ_ONLY:
        accesstype = "Read Only";
        break;
    case VSNAP_READ_WRITE:
        accesstype = "Read Write";
        break;
    case VSNAP_READ_WRITE_TRACKING:
        accesstype = "Read Write Tracking";
        break;
    default:
        accesstype = "Unknown";
    }

    out << setw(26)  << setiosflags(ios::left) << "Vsnap Type:" << accesstype << std::endl;

    out << setw(26)  << setiosflags(ios::left) << "Snapshot Id:" << vsnapinfo.snapshot_id << std::endl;
    out << "\n";
}

/*
* FUNCTION NAME :  VsnapMgr::getCdpDbName
*
* DESCRIPTION : fetch the retention database path for the specified volume 
*               from local persistent store.
*
* INPUT PARAMETERS : target volume name
*
* OUTPUT PARAMETERS : retention database path
*
* NOTES :
* 
*
* return value : true on success, false otherwise
*
*/
bool VsnapMgr::getCdpDbName(const std::string& input, std::string& dbpath)
{
    bool rv = true;

    // 
    // Replication Pair configuration information is not available
    //
    if(!IsConfigAvailable)
    {
        DebugPrintf(SV_LOG_INFO, "Retention path for %s is not available.\n", input.c_str());
        return false;
    }

    string volname = input;
    if (IsReportingRealNameToCx())
    {
        GetDeviceNameFromSymLink(volname);
    }
    else
    {
        /** 
        * This code is for solaris.
        * If the user has given 
        * Real Name of device, get 
        * the link name of it and 
        * then proceed to process.
        * since CX is having only the
        * link name for solaris agent
        */
        std::string linkname = volname;
        if (GetLinkNameIfRealDeviceName(volname, linkname))
        {
            volname = linkname;
        } 
        else
        {
            DebugPrintf(SV_LOG_ERROR, "@LINE %d in FILE %s Invalid volume specified\n", LINE_NO, FILE_NAME);
            rv = false;
        }
    }

    if(volname != input)
    {
        DebugPrintf(SV_LOG_INFO,
            " %s is a symbolic link to %s \n\n", volname.c_str(), input.c_str());
    }

    FormatVolumeNameForCxReporting(volname);
    FirstCharToUpperForWindows(volname);

    //
    // note: configurator never throws any exception for getCdpDbName
    //
    dbpath = m_TheConfigurator->getCdpDbName(volname);
    if(dbpath != "")
    {
        // convert the path to real path (no symlinks)
        if( !resolvePathAndVerify( dbpath ) )
        {
            DebugPrintf( SV_LOG_INFO, 
                "Retention Database Path %s for volume %s is not yet created\n",
                dbpath.c_str(),  volname.c_str() ) ;
            rv = false ;
        }
    }
    else
        rv = false;

    return rv;
}
/*
* FUNCTION NAME : updateBookMarkLockStatus
*
* DESCRIPTION : mark bookmark as locked/unlocked for vsnap if it not locked as user locked
*
* INPUT PARAMETERS : bookmarkname
state
*
* OUTPUT PARAMETERS :  None
*
* NOTES : 
*
* return value : on success true otherwise false 
*
*/

bool VsnapMgr::updateBookMarkLockStatus(const std::string &dbname, const std::string &bkmkname,SV_USHORT oldstatus, SV_USHORT newstatus)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED:%s dbname:%s bookmarkname:%s\n", FUNCTION_NAME,dbname.c_str(),  bkmkname.c_str());
    bool rv = true;

    CDPMarkersMatchingCndn cndn;
    CDPDatabase database(dbname);
    std::vector<CDPEvent> cdpEvents;

    cndn.value(bkmkname);
    cndn.setlockstatus(oldstatus);

    rv = database.UpdateLockedBookmarks(cndn,cdpEvents,newstatus);

    DebugPrintf(SV_LOG_DEBUG, "EXITED:%s dbname:%s bookmarkname:%s\n", FUNCTION_NAME,dbname.c_str(), bkmkname.c_str());
    return rv;
}
