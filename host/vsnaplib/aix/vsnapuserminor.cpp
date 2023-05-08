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
#include<iostream>
#include<errno.h>
#include<sstream>
#include <string>
#include <vector>

#include "portablehelpers.h"
#include "portablehelpersmajor.h"
#include "portablehelpersminor.h"
#include "globs.h"
#include "DiskBtreeCore.h"
#include "VsnapCommon.h"
#include "VsnapUser.h"
#include "DiskBtreeHelpers.h"
#include "cdputil.h"
#include "cdpsnapshot.h"
#include <ace/ACE.h>
#include <ace/OS.h>
#include <ace/OS_NS_fcntl.h>
#include <ace/OS_NS_unistd.h>
#include <ace/OS_NS_errno.h>
#include <ace/Global_Macros.h>
#include "volumeinfocollector.h"
#include "volumegroupsettings.h"
#include "drvstatus.h"
//#include <sys/mount.h>

//#include <sys/mnttab.h>

#include "logger.h"
#include "VsnapShared.h"
#include "localconfigurator.h"
#include <sys/sysinfo.h>
#include "configwrapper.h"
#include "inmcommand.h"
#include "svtypes.h"
#include "vsnapuserminor.h"
#include <ctime>
#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>
#include <cf.h>
#include <sys/cfgodm.h>
#include <sys/cfgdb.h>
#include <alloca.h>
#include <odmi.h>

#include "inmsafecapis.h"
#include "inmsafeint.h"
#include "inmageex.h"

#define SV_PROC_MNTS    "/etc/mnttab"
#define VSNAP_USER_MSG_SIZE 100

ACE_Recursive_Thread_Mutex g_ODMGuardMutex;

void UnixVsnapMgr::CleanUpIncompleteVsnap(VsnapVirtualVolInfo* VirtualInfo)
{
	std::string devicefile = VirtualInfo->DeviceName;
	std::string error;
	//constructing the char device
	std::string vsnapbasedev = basename_r(devicefile.c_str());
	std::string vsnaprdev = VSNAP_RDEVICE_PREFIX;
	vsnaprdev += vsnapbasedev;

	std::string nsdevname = AIX_DEVICE_DIR;
	nsdevname += vsnapbasedev; 

	std::string nsrdevname = AIX_RDEVICE_PREFIX;
	nsrdevname += vsnapbasedev;

	// deleting the link
	unlink(devicefile.c_str());
	unlink(vsnaprdev.c_str());

	//calling rmdev pass only the base name i.e for /dev/clixxx pass clixxx
	//this will unregister the device            
	std::string rmdevcmdstr = RMDEV_CMD;
	rmdevcmdstr += vsnapbasedev;
	rmdevcmdstr += RMDEV_CMD_PREFIX;
	DebugPrintf(SV_LOG_DEBUG,"Executing - %s\n", rmdevcmdstr.c_str());
	InmCommand rmdevcmd(rmdevcmdstr);

	InmCommand::statusType dev_status = rmdevcmd.Run();

	if (dev_status != InmCommand::completed)
	{
		DebugPrintf(SV_LOG_DEBUG,"Fail to execute the rmdev command on %s\n", devicefile.c_str());                
	}

	if (rmdevcmd.ExitCode())
	{
		std::ostringstream msg;
		msg << "Exit Code = " <<rmdevcmd.ExitCode() << std::endl;
		if(!rmdevcmd.StdOut().empty())
		{
			msg << "Output = " << rmdevcmd.StdOut() << std::endl;
		}
		if(!rmdevcmd.StdErr().empty())
		{
			msg << "Error = " << rmdevcmd.StdErr() << std::endl;
		}
		DebugPrintf(SV_LOG_DEBUG,"Fail to execute the rmdev command on %s\n", devicefile.c_str());
		DebugPrintf(SV_LOG_DEBUG,"ERROR: %s\n", msg.str().c_str());
	}

	//releasing the minor number for the device
	if(-1 == odm_initialize())
	{
		DebugPrintf(SV_LOG_ERROR,"Failed to initialize ODM for device %s\n", nsdevname.c_str());
	}
	if(reldevno((char *)(nsdevname.c_str()),true) != 0)
	{
		DebugPrintf(SV_LOG_DEBUG,"reldevno failed for %s\n", nsdevname.c_str());                
	}
	if(-1 == odm_terminate())
	{
		DebugPrintf(SV_LOG_ERROR,"Failed to terminate ODM for device %s\n", nsdevname.c_str());
	}
	//deleting the device and the softlinks
	unlink(nsdevname.c_str());
	unlink(nsrdevname.c_str());
	DeleteVsnapDevice(NULL,devicefile,error);    
}

void UnixVsnapMgr::StartTick(SV_ULONG & StartTime)
{
	time_t currenttime = time(NULL);
	StartTime = ((time_t)-1 != currenttime)?currenttime:0;
}

void UnixVsnapMgr::EndTick(SV_ULONG StartTime)
{
    /* place holder function */
}

bool GetdevicefilefromMountpoint(const char* MntPnt,std::string &DeviceFile)
{
	bool rv = false;
	int num = 0;
	int size = 0;
	char * buf;
	std::string mountpoint = MntPnt;
	do
	{
		if(mountpoint.empty())
		{
			break;
		}

		num = mntctl(MCTL_QUERY, sizeof(size), (char *) &size);
		if(num < 0)
			break;

		INM_SAFE_ARITHMETIC(size *= InmSafeInt<int>::Type(2), INMAGE_EX(size))
		buf = (char *)alloca(size);
		num = mntctl(MCTL_QUERY, size, buf);
		if(num < 0)
			break;

		struct vmount * vm;
		int i, dataLength;
		char *data;
		for (vm = ( struct vmount *)buf, i = 0; i < num; i++)
		{
			if(vm != NULL)
			{
				dataLength = vm->vmt_data[VMT_OBJECT].vmt_size;
				data = NULL;
                size_t dataalloclen;
                INM_SAFE_ARITHMETIC(dataalloclen = InmSafeInt<int>::Type(dataLength) + 1, INMAGE_EX(dataLength))
				data = (char*)malloc(dataalloclen);
				inm_strncpy_s(data, dataLength + 1, (char *)vm + vm->vmt_data[VMT_OBJECT].vmt_off, dataLength);
				mount_info mInfo;
				mInfo.m_deviceName = data;
				if(data != NULL)
				{
					free(data);
				}
				dataLength = vm->vmt_data[VMT_STUB].vmt_size;
                INM_SAFE_ARITHMETIC(dataalloclen = InmSafeInt<int>::Type(dataLength) + 1, INMAGE_EX(dataLength))
				data = (char*)malloc(dataalloclen);
				inm_strncpy_s(data, dataLength + 1, (char *)vm + vm->vmt_data[VMT_STUB].vmt_off, dataLength);
				mInfo.m_mountedOver = data;
				if(data != NULL)
				{
					free(data);
				}

				if(mInfo.m_mountedOver == mountpoint)
				{                    
					if(std::string::npos == mInfo.m_deviceName.find(VSNAP_DEVICE_DIR))
					{
						DeviceFile = VSNAP_DEVICE_DIR;
						DeviceFile +=  basename_r(mInfo.m_deviceName.c_str());			
					}
					else
					{
						DeviceFile = mInfo.m_deviceName;
					}
					rv = true;
					break;
				}
			}
			vm = (struct vmount *)((char *)vm + vm->vmt_length);
		}

	}while(false);

	return rv ;
}

void UnixVsnapMgr:: process_proc_mounts(std::vector<volinfo> &fslist)
{
}

bool UnixVsnapMgr::CreateRequiredVsnapDIR()
{
	bool rv = true;
	std::string dir = VSNAP_DEVICE_DIR;
	SVERROR rc = SVMakeSureDirectoryPathExists(dir.c_str());
	if (rc.failed())
	{
		DebugPrintf(SV_LOG_ERROR, "FAILED  SVMakeSureDirectoryPathExists %s :  %s \n",dir.c_str(),rc.toString());
		rv = false;
	}
	return rv;
}

bool UnixVsnapMgr::MountVol_Platform_Specific(VsnapVirtualVolInfo *VirtualInfo, int minornumber, sv_dev_t MkdevNumber)
{
	bool			success = true;
	bool            needcleanup = false;
	VsnapErrorInfo		*Error = &VirtualInfo->Error;
	int Return_Mknod = 0 ;

	ACE_Guard<ACE_Recursive_Thread_Mutex> autoGuardMutex(g_ODMGuardMutex);

	std::string ODMLockPath = "/var/log/inmage_odm.lck";
	ACE_File_Lock OdmFileLock(ACE_TEXT_CHAR_TO_TCHAR(ODMLockPath.c_str()), O_CREAT | O_RDWR, SV_LOCK_PERMISSIONS,0);
	ACE_Guard<ACE_File_Lock> autoGuardFileLock(OdmFileLock);

	do 
	{

		//In AIX, we need 2 devices, one block device and other is char device
		//Every block device refers to /dev/clixxx and char device refer to /dev/rclixxx
		//so we need to mknode for both the device

		std::string vsnapdevice = basename_r(VirtualInfo->DeviceName.c_str());
		std::string vsnap_dev = AIX_DEVICE_DIR;
		std::string rvsnap_dev = AIX_RDEVICE_PREFIX;
		vsnap_dev += vsnapdevice;
		rvsnap_dev += vsnapdevice;
		DebugPrintf(SV_LOG_DEBUG,"Executing mknod for block device %s\n", vsnap_dev.c_str());

		Return_Mknod = mknod(vsnap_dev.c_str(), S_IFBLK, MkdevNumber);
		if(Return_Mknod!=0) {
			if(ACE_OS::last_error()==EEXIST) {
				struct stat statbuf;
				if (IsValidDevfile(vsnap_dev.c_str())) {
					if (0 == stat(vsnap_dev.c_str(), &statbuf)) {
						if (!S_ISBLK(statbuf.st_mode)) {
							Error->VsnapErrorCode = ACE_OS::last_error();
							Error->VsnapErrorStatus = VSANP_NOTBLOCKDEVICE_FAILED;
							needcleanup = true;
							break;
						} //Is Blk else Ends here
					} else {
						Error->VsnapErrorCode = ACE_OS::last_error();
						Error->VsnapErrorStatus = VSANP_NOTBLOCKDEVICE_FAILED;
						needcleanup = true;
						break;
					}
				} else {
					Error->VsnapErrorCode = ACE_OS::last_error();
					Error->VsnapErrorStatus = VSNAP_MKNOD_FAILED;
					needcleanup = true;
					break;
				}
			}
		}

		DebugPrintf(SV_LOG_DEBUG,"Executing mknod for char device %s\n", rvsnap_dev.c_str());
		Return_Mknod = mknod(rvsnap_dev.c_str(), S_IFCHR, MkdevNumber);
		if(Return_Mknod!=0) {
			if(ACE_OS::last_error()==EEXIST) {
				struct stat statbuf;
				if (IsValidDevfile(rvsnap_dev.c_str())) {
					if (0 == stat(rvsnap_dev.c_str(), &statbuf)) {
						if (!S_ISCHR(statbuf.st_mode)) {
							Error->VsnapErrorCode = ACE_OS::last_error();
							Error->VsnapErrorStatus = VSANP_NOTCHARDEVICE_FAILED;
							needcleanup = true;
							break;
						}
					} else {
						Error->VsnapErrorCode = ACE_OS::last_error();
						Error->VsnapErrorStatus = VSANP_NOTCHARDEVICE_FAILED;
						needcleanup = true;
						break;
					}
				} else {
					Error->VsnapErrorCode = ACE_OS::last_error();
					Error->VsnapErrorStatus = VSNAP_MKNOD_FAILED;
					needcleanup = true;
					break;
				}
			}
		}

		//Registering the minor number through api call genminor
		//extracting the major number from the dev number getting from driver

		int * new_minor;
		int major = major64(MkdevNumber);
		DebugPrintf(SV_LOG_DEBUG,"Executing genminor for device %s\n", vsnap_dev.c_str());
		if(-1 == odm_initialize())
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to initialize ODM for the device %s\n",VirtualInfo->DeviceName.c_str());
			Error->VsnapErrorStatus = VSANP_MAJOR_MINOR_CONFLICT;
			Error->VsnapErrorCode = ACE_OS::last_error();
			needcleanup = true;
			break;
		}
		new_minor =genminor((char*)(vsnap_dev.c_str()), major, minornumber, 1, 0, 0);
		if(-1 == odm_terminate())
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to terminate ODM for the device %s\n",VirtualInfo->DeviceName.c_str());
		}
		if((!new_minor) || (*new_minor != minornumber) )
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to register the minor number %d for the device %s, returned minor = %d\n",
				minornumber,VirtualInfo->DeviceName.c_str(), *new_minor);
			Error->VsnapErrorStatus = VSANP_MAJOR_MINOR_CONFLICT;
			Error->VsnapErrorCode = ACE_OS::last_error();
			needcleanup = true;
			break;
		}

		//running mkdev to register the device
		//we need to pass the clixxx to register the device

		std::string dev_cmd =  MKDEV_CMD;
		dev_cmd += vsnapdevice;
		DebugPrintf(SV_LOG_INFO,"Executing - %s\n", dev_cmd.c_str());
		InmCommand mkdevcmd(dev_cmd);
		InmCommand::statusType dev_status = mkdevcmd.Run();

		if (dev_status != InmCommand::completed)
		{
			Error->VsnapErrorStatus=VSANP_MKDEV_FAILED;
			Error->VsnapErrorCode = ACE_OS::last_error();
			if(!mkdevcmd.StdErr().empty())
			{
				Error->VsnapErrorMessage = mkdevcmd.StdErr();
			}
			needcleanup = true;
			break;
		}

		if (mkdevcmd.ExitCode())
		{
			Error->VsnapErrorStatus=VSANP_MKDEV_FAILED;
			Error->VsnapErrorCode = mkdevcmd.ExitCode();

			std::ostringstream msg;
			msg << "Exit Code = " <<mkdevcmd .ExitCode() << std::endl;
			if(!mkdevcmd.StdOut().empty())
			{
				msg << "Output = " << mkdevcmd.StdOut() << std::endl;
			}
			if(!mkdevcmd.StdErr().empty())
			{
				msg << "Error = " << mkdevcmd.StdErr() << std::endl;
			}
			Error->VsnapErrorMessage = msg.str();
			needcleanup = true;
			break;
		}
		//creating softlinks 
		//the device /dev/clixxx shown in the system for the the vsnap device /dev/vs/clixxx
		//if the link exist then deleting the link and 
		//creating softlink /dev/vs/clixx->/dev/clixxx;/dev/vs/rclixx->/dev/rclixxx

		std::string vsnaprdev = VSNAP_RDEVICE_PREFIX;
		vsnaprdev += vsnapdevice;        

		struct stat stbuf;
		if(lstat(VirtualInfo->DeviceName.c_str(),&stbuf) == 0)
		{
			unlink(VirtualInfo->DeviceName.c_str());
		}
		DebugPrintf(SV_LOG_DEBUG,"Creating symlink for block device %s\n", VirtualInfo->DeviceName.c_str());
		if(symlink(vsnap_dev.c_str(),VirtualInfo->DeviceName.c_str()) == -1)
		{
			needcleanup = true;
			break;
		}

		if(lstat(vsnaprdev.c_str(),&stbuf) == 0)
		{
			unlink(vsnaprdev.c_str());
		}
		DebugPrintf(SV_LOG_DEBUG,"Creating symlink for char device %s\n", rvsnap_dev.c_str());
		if(symlink(rvsnap_dev.c_str(),vsnaprdev.c_str()) == -1)
		{
			needcleanup = true;
			break;
		}

		std::stringstream str1;
		str1<<"Vsnap device "<<VirtualInfo->DeviceName<<" created successfully, VsnapId: "<<VirtualInfo->SnapShotId;
		DebugPrintf(SV_LOG_INFO, "%s\n", str1.str().c_str());
		str1.clear ();
		str1.str ("");

	}while(false);

	if(needcleanup)
	{
		CleanUpIncompleteVsnap(VirtualInfo);
		success = false;
	}
	return success;
}

bool UnixVsnapMgr::MountVol(VsnapVirtualVolInfo *VirtualInfo, VsnapMountInfo *MountData, char* SnapshotDrive,bool &FsmountFailed)
{
	SV_ULONG		ByteOffset  = 0, ulLength = 0;
	char*			Buffer = NULL;
	bool			success = true;
	VsnapErrorInfo		*Error = &VirtualInfo->Error;
	size_t			UniqueIdLength;
	int Return_Mknod = 0 ;
	int  minornumber = 0 ; 
	sv_dev_t  MkdevNumber = 0 ;

	do 
	{
		// VsnapVirtualVolInfo->DeviceName - autogenerated device name from cx like /dev/vs/vsnapxxx
		// or autogenerated device name from cdpcli like /dev/vs/clixxx
		// The linvsnap driver maintains /dev/vs/vsnapxxx or /dev/vs/clixxx

		UniqueIdLength = VirtualInfo->DeviceName.size();

		ByteOffset  = 0;
		INM_SAFE_ARITHMETIC(ulLength    = (SV_ULONG) sizeof(SV_USHORT) + (InmSafeInt<size_t>::Type(UniqueIdLength) + 1) + sizeof(VsnapMountInfo), INMAGE_EX(sizeof(SV_USHORT))(UniqueIdLength)(sizeof(VsnapMountInfo)))
		INM_SAFE_ARITHMETIC(ulLength   += InmSafeInt<size_t>::Type(sizeof(int)) + sizeof(sv_dev_t), INMAGE_EX(ulLength)(sizeof(int))(sizeof(sv_dev_t)))
		Buffer      = (char*) calloc(ulLength , 1);

		if (!Buffer)
		{
			DebugPrintf(SV_LOG_ERROR, "@LINE %d in FILE %s, calloc failed\n", LINE_NO, FILE_NAME);
			return false;
		}

		//opening vsnap control device, calling the ioctl to create vsnap

		if (!CreateVsnapDevice(VirtualInfo, MountData, Buffer, ByteOffset, ulLength))
		{
			free(Buffer);
			return false;
		}

		//getting the minor number and the dev number from the buffer which is filled by the driver 
		//at the end of the buffer that agent send
		inm_memcpy_s(&minornumber, sizeof(minornumber), (Buffer + (ByteOffset + sizeof(VsnapMountInfo))), sizeof (int));
		inm_memcpy_s(&MkdevNumber, sizeof(MkdevNumber), (Buffer + (ByteOffset + sizeof(VsnapMountInfo)) + sizeof(int)), sizeof (sv_dev_t));

		free(Buffer);

        std::string vsnapdevice = basename_r(VirtualInfo->DeviceName.c_str());
        std::string vsnap_dev = AIX_DEVICE_DIR;
        vsnap_dev += vsnapdevice;

		if(!MountVol_Platform_Specific(VirtualInfo, minornumber, MkdevNumber))
		{
			success = false;
			break;
		}

		std::stringstream str1;
		//Persisting the vsnap information

		if(VirtualInfo->State != VSNAP_REMOUNT && !PersistVsnap(MountData, VirtualInfo))
		{
			DebugPrintf(SV_LOG_ERROR, "Unable to Persist Vsnap Information for %s\n", VirtualInfo->DeviceName.c_str());
			VsnapVirtualInfoList VirtualList;
			VirtualInfo->State = VSNAP_UNMOUNT;
			VirtualList.push_back(VirtualInfo);
			Unmount(VirtualList, true);
			return false;
		}
		if(VirtualInfo->State ==  VSNAP_REMOUNT)
		{
			CleanStaleVgAndRecreateVgForVsnapOnReboot(VirtualInfo->DeviceName);
		}

		//validating the mount point
		std::string errmsg;
		if(!IsValidMountPoint(SnapshotDrive, errmsg))
		{
			DebugPrintf(SV_LOG_ERROR, "Unable to mount the vsnap on mount point %s. %s\n",SnapshotDrive,errmsg.c_str());
			success = true;
			break;
		}
		if(VirtualInfo->VolumeName.empty())
		{
			success = true;
			break;
		}

		//getting the fstype of the source volume
		std::string formattedVolumeName = MountData->VolumeName;
		CDPUtil::trim(formattedVolumeName);
		FormatVolumeName(formattedVolumeName);
		if (IsReportingRealNameToCx())
		{
			GetDeviceNameFromSymLink(formattedVolumeName);
		}

		std::string fsType = getFSType(formattedVolumeName);
		if(fsType.empty())
		{
			FsmountFailed = true;
			str1.clear();
			str1 << "Virtual Volume " << VirtualInfo->VolumeName << " will not be mounted as the protected volume doesn't contain any filesystem.";
			DebugPrintf(SV_LOG_INFO, "%s\n", str1.str().c_str());
			success = true;
			break;
		}

		if(!VirtualInfo->MountVsnap)
		{
			str1.clear();
			str1 << "Virtual Volume " << VirtualInfo->VolumeName << " will not be mounted as the user requested not to mount volume."; 
			DebugPrintf(SV_LOG_INFO, "%s\n", str1.str().c_str());
			success = true;
			break;
		}

		bool bSetReadOnly = false;
		if(VirtualInfo->AccessMode == VSNAP_READ_ONLY)
			bSetReadOnly = true;
		int exitcode = 0;

		// mounting the device
		if(!MountDevice(vsnap_dev,SnapshotDrive,fsType,bSetReadOnly,exitcode,errmsg))
		{
			FsmountFailed = true;
			Error->VsnapErrorStatus=L_VSNAP_MOUNT_FAILED;
			Error->VsnapErrorCode = exitcode;
			Error->VsnapErrorMessage = errmsg;
			success = true;
			break;
		}
	}while(false);

	return success;
}


bool UnixVsnapMgr::Unmount_Platform_Specific(const std::string & devicefile)
{
	bool			status = true;

	ACE_Guard<ACE_Recursive_Thread_Mutex> autoGuardMutex(g_ODMGuardMutex);

	std::string ODMLockPath = "/var/log/inmage_odm.lck";
	ACE_File_Lock OdmFileLock(ACE_TEXT_CHAR_TO_TCHAR(ODMLockPath.c_str()), O_CREAT | O_RDWR, SV_LOCK_PERMISSIONS,0);
	ACE_Guard<ACE_File_Lock> autoGuardFileLock(OdmFileLock);

	do 
	{

		//constructing the char device
		std::string vsnapbasedev = basename_r(devicefile.c_str());
		std::string vsnaprdev = VSNAP_RDEVICE_PREFIX;
		vsnaprdev += vsnapbasedev;

		std::string nsdevname = AIX_DEVICE_DIR;
		nsdevname += vsnapbasedev; 

		std::string nsrdevname = AIX_RDEVICE_PREFIX;
		nsrdevname += vsnapbasedev;

		// deleting the link
		unlink(devicefile.c_str());
		unlink(vsnaprdev.c_str());

		//calling rmdev pass only the base name i.e for /dev/clixxx pass clixxx
		//this will unregister the device            

		std::string rmdevcmdstr = RMDEV_CMD;
		rmdevcmdstr += vsnapbasedev;
		rmdevcmdstr += RMDEV_CMD_PREFIX;
		DebugPrintf(SV_LOG_DEBUG,"Executing - %s\n", rmdevcmdstr.c_str());
		InmCommand rmdevcmd(rmdevcmdstr);

		InmCommand::statusType dev_status = rmdevcmd.Run();

		if (dev_status != InmCommand::completed)
		{
			DebugPrintf(SV_LOG_ERROR,"Fail to execute the rmdev command on %s\n", devicefile.c_str());                
		}

		if (rmdevcmd.ExitCode())
		{
			std::ostringstream msg;
			msg << "Exit Code = " <<rmdevcmd.ExitCode() << std::endl;
			if(!rmdevcmd.StdOut().empty())
			{
				msg << "Output = " << rmdevcmd.StdOut() << std::endl;
			}
			if(!rmdevcmd.StdErr().empty())
			{
				msg << "Error = " << rmdevcmd.StdErr() << std::endl;
			}
			DebugPrintf(SV_LOG_ERROR,"Fail to execute the rmdev command on %s\n", devicefile.c_str());
			DebugPrintf(SV_LOG_ERROR,"ERROR: %s\n", msg.str().c_str());
		}

		//releasing the minor number for the device
		if(-1 == odm_initialize())
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to initialize ODM for device %s\n", nsdevname.c_str());
		}
		if(reldevno((char *)(nsdevname.c_str()),true) != 0)
		{
			DebugPrintf(SV_LOG_ERROR,"reldevno failed for %s\n", nsdevname.c_str());                
		}
		if(-1 == odm_terminate())
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to terminate ODM for device %s\n",nsdevname.c_str());
		}
		//deleting the device and the softlinks
		unlink(nsdevname.c_str());
		unlink(nsrdevname.c_str());            

	} while (FALSE);
	return status;
}



bool UnixVsnapMgr::UnmountVirtualVolume(char* SnapshotDrive, VsnapVirtualVolInfo* VirtualInfo, std::string& output, std::string& error,bool bypassdriver)
{
    /* place holder function */
	return true;
}



bool GetdevicefilefromMountpointAndFileSystem(const char* MntPnt,
											  std::string& DeviceFile,
											  std::string &filesystem)
{
	return true;
}


bool UnixVsnapMgr::NeedToRecreateVsnap(const std::string &vsnapdevice)
{
	bool bdeviceexists = LinkExists(vsnapdevice);
	return !bdeviceexists;
}


bool UnixVsnapMgr::PerformZpoolDestroyIfReq(const std::string &devicefile, bool bshoulddeletevsnaplogs)
{
	bool bretval = true;
	return bretval;
}

bool UnixVsnapMgr::OpenLinvsnapSyncDev(const std::string &device, int &fd)
{
	bool bretval = true;
	return bretval;
}


bool UnixVsnapMgr::CloseLinvsnapSyncDev(const std::string &device, int &fd)
{
	bool bretval = true;
	return bretval;
}


bool UnixVsnapMgr::RetryZpoolDestroyIfReq(const std::string &device, const std::string &target, bool bshoulddeletevsnaplogs)
{
	bool bretval = true;
	return bretval;
}

bool UnixVsnapMgr::StatAndMount(const VsnapPersistInfo& remountInfo)
{
	bool bretval = true;
	return bretval; 
}
