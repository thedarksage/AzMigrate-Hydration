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

#include "portablehelpers.h"
#include "portablehelpersmajor.h"
#include "portablehelpersminor.h"
#include "globs.h"
#include "DiskBtreeCore.h"
#include "VsnapCommon.h"
#include "VsnapUser.h"
#include "DiskBtreeHelpers.h"
#include "cdpplatform.h"
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
#ifndef SV_AIX
#include <sys/mount.h>
#endif

#include "logger.h"
#include "VsnapShared.h"
#include "localconfigurator.h"
#include <sys/sysinfo.h>
#include "configwrapper.h"
#include "inmcommand.h"
#include "vsnapuserminor.h"
#include "cdplock.h"

#include "inmageex.h"

#include <boost/lexical_cast.hpp>
#include "volumereporter.h"
#include "volumereporterimp.h"
#include "inmsafecapis.h"
#include "inmsafeint.h"
#include "inmageex.h"

#define MKDEV(ma,mi)    ((ma)<<8 | (mi))
const size_t MOUNT_PATH_LEN = 2048; 

using namespace std;

Configurator* m_configurator;
extern void UnloadExistingVolumes(ACE_HANDLE hDevice);
int WaitForProcessCompletion(ACE_Process_Manager * pm, pid_t pid,VsnapErrorInfo *);
extern void VsnapCallBack(void *, void *, void *, bool);
extern bool operator<(VsnapKeyData &, VsnapKeyData &);
extern bool operator>(VsnapKeyData &, VsnapKeyData &);
extern bool operator==(VsnapKeyData &, VsnapKeyData &);

//bool VsnapUserGetFileNameFromFileId(string, unsigned long long, unsigned long, char *);
void VsnapNodeTraverseCallback(void *NodeKey, void *NewKey, void *param, bool flag);
bool GetdevicefilefromMountpoint(const char* MntPnt,string& DeviceFile) ;
bool GetdevicefilefromMountpointAndFileSystem(const char* MntPnt,std::string& DeviceFile, std::string &filesystem) ;
std::string FormMountCommand(const std::string filesystem, 
                             const std::string device, 
                             const std::string mountpoint);
/******************** UTILITY FUNCTIONS - END *******************/


#define MAX_LENGTH 15 
void UnixVsnapMgr::DeleteMountPointsForTargetVolume(char* HostVolumeName, char* TargetVolumeName, 
                                                    VsnapVirtualVolInfo *VirtualInfo)
{
    SV_ULONG       BufferLength            = 2048;
    bool        bResult                 = TRUE;
    char       *VolumeMountPoint		= NULL;
    char       *FullVolumeMountPoint	= NULL;
    char       *MountPointTargetVolume	= NULL;


    ACE_HANDLE      hMountHandle;

#ifdef SV_WINDOWS
    LIST_ENTRY  MountPointHead;
    InitializeListHead(&MountPointHead);
#else

#endif

    VolumeMountPoint = (char *)malloc(MOUNT_PATH_LEN);
    if(!VolumeMountPoint)
        return;

    FullVolumeMountPoint = (char *)malloc(MOUNT_PATH_LEN);
    if(!FullVolumeMountPoint)
    {
        free(VolumeMountPoint);
        return;
    }

    MountPointTargetVolume = (char *)malloc(MOUNT_PATH_LEN);
    if(!MountPointTargetVolume)
    {
        free(VolumeMountPoint);
        free(FullVolumeMountPoint);
        return;
    }

#ifdef SV_WINDOWS
    hMountHandle = FindFirstVolumeMountPoint(HostVolumeName, VolumeMountPoint, BufferLength);
    if(ACE_INVALID_HANDLE == hMountHandle)
    {
        free(VolumeMountPoint);
        free(FullVolumeMountPoint);
        free(MountPointTargetVolume);
        return;
    }

	inm_tcscpy_s(FullVolumeMountPoint, MOUNT_PATH_LEN HostVolumeName);
	inm_tcscat_s(FullVolumeMountPoint, MOUNT_PATH_LEN,VolumeMountPoint);

    do
    {
        bResult = GetVolumeNameForVolumeMountPoint(FullVolumeMountPoint, MountPointTargetVolume, 2048);
        if(bResult && 0 == _tcscmp(MountPointTargetVolume, TargetVolumeName))
        {
            PMOUNT_POINT MountPoint = (PMOUNT_POINT)malloc(sizeof(MOUNT_POINT));
            // HACK: MAX_STRING_SIZE < MOUNT_PATH_LEN
            inm_tcscpy_s(MountPoint->MountPoint, ARRAYSIZE(MountPoint->MountPoint), FullVolumeMountPoint);
            InsertTailList(&MountPointHead, &MountPoint->ListEntry);
        }

        bResult = FindNextVolumeMountPoint(hMountHandle, VolumeMountPoint, BufferLength);
		inm_tcscpy_s(FullVolumeMountPoint, MOUNT_PATH_LEN, HostVolumeName);
		inm_tcscat_s(FullVolumeMountPoint, MOUNT_PATH_LEN,VolumeMountPoint);

    }while(bResult);

    FindVolumeMountPointClose(hMountHandle);

    while(!IsListEmpty(&MountPointHead))
    {
        char errstr[100];
        PMOUNT_POINT MountPoint = (PMOUNT_POINT) RemoveHeadList(&MountPointHead);

        if(!DeleteVolumeMountPoint(MountPoint->MountPoint))
        {
            VirtualInfo->Error.VsnapErrorStatus = VSNAP_UNMOUNT_FAILED;
            VirtualInfo->Error.VsnapErrorCode = ACE_OS::last_error();
            inm_sprintf_s(errstr, ARRAYSIZE(errstr), "DeleteVolumeMountPoint Failed for point:%s with Error:%#x", MountPoint->MountPoint, GetLastError());
            VsnapPrint(errstr);
        }

        free(MountPoint);
    }
#endif

    free(VolumeMountPoint);
    free(FullVolumeMountPoint);
    free(MountPointTargetVolume);
}



bool UnixVsnapMgr::UnmountFileSystem(const wchar_t* VolumeName, VsnapVirtualVolInfo *VirtualInfo)
{   
    wchar_t FormattedVolumeName[MAX_STRING_SIZE];
    wchar_t FormattedVolumeNameWithBackSlash[MAX_STRING_SIZE];

    inm_wcscpy_s(FormattedVolumeName, ARRAYSIZE(FormattedVolumeName), VolumeName);
#ifdef SV_WINDOWS
    FormattedVolumeName[1] = _T('\\');
#else
#endif

    inm_wcscpy_s(FormattedVolumeNameWithBackSlash, ARRAYSIZE(FormattedVolumeNameWithBackSlash), FormattedVolumeName);
    inm_wcscat_s(FormattedVolumeNameWithBackSlash, ARRAYSIZE(FormattedVolumeNameWithBackSlash), L"\\");

    char FormattedVolumeNameWithBackSlashT[MAX_STRING_SIZE];
    wcstotcs(FormattedVolumeNameWithBackSlashT, ARRAYSIZE(FormattedVolumeNameWithBackSlashT), FormattedVolumeNameWithBackSlash);

    char FormattedVolumeNameT[MAX_STRING_SIZE];
    wcstotcs(FormattedVolumeNameT, ARRAYSIZE(FormattedVolumeNameT), FormattedVolumeName);

    SV_ULONG   BytesReturned;
    ACE_HANDLE VolHdl;

    // PR#10815: Long Path support
    VolHdl=ACE_OS::open(getLongPathName(FormattedVolumeNameT).c_str(),3,ACE_DEFAULT_OPEN_PERMS);
    if (VolHdl == ACE_INVALID_HANDLE)
    {
        VirtualInfo->Error.VsnapErrorStatus = VSNAP_VIRVOL_OPEN_FAILED;
        VirtualInfo->Error.VsnapErrorCode = ACE_OS::last_error();
        return FALSE;
    }

#ifdef SV_WINDOWS

    FlushFileBuffers(VolHdl);
    DeviceIoControl(
        VolHdl,
        FSCTL_DISMOUNT_VOLUME,
        NULL,
        0,
        NULL,
        0,
        &BytesReturned,
        NULL
        );

    bool Lock = false;

    if (DeviceIoControl(
        VolHdl,
        FSCTL_LOCK_VOLUME,
        NULL,
        0,
        NULL,
        0,
        &BytesReturned,
        NULL
        ))
    {
        Lock = true;	
    }
    //FlushFileBuffers(VolHdl);
#endif
    DeleteMountPoints(FormattedVolumeNameWithBackSlashT, VirtualInfo);
    DeleteDrivesForTargetVolume(FormattedVolumeNameWithBackSlashT, VirtualInfo);
#ifdef SV_WINDOWS
    if(Lock)
        DeviceIoControl( VolHdl, FSCTL_UNLOCK_VOLUME, NULL, 0, NULL, 0, &BytesReturned, NULL);
#endif
    ACE_OS::close(VolHdl);

    return true;
}

SV_UCHAR UnixVsnapMgr::FileDiskUmount(char* VolumeName)
{
    ACE_HANDLE  Device;
    SV_ULONG   BytesReturned;

    // PR#10815: Long Path support
    Device = ACE_OS::open(
        getLongPathName(VolumeName).c_str(),
        3,ACE_DEFAULT_OPEN_PERMS);


    if (Device == ACE_INVALID_HANDLE)
    {
        return FALSE;
    }

#ifdef SV_WINDOWS
    FlushFileBuffers(Device);
    if (!DeviceIoControl(
        Device,
        FSCTL_DISMOUNT_VOLUME,
        NULL,
        0,
        NULL,
        0,
        &BytesReturned,
        NULL
        ))
    {
        ACE_OS::close(Device);
        return FALSE;
    }
#endif
    ACE_OS::close(Device);

    return TRUE;
}

void UnixVsnapMgr::DeleteDrivesForTargetVolume(const char* TargetVolumeLink, VsnapVirtualVolInfo *VirtualInfo)
{
    //Please see Windows dependent Code in WinVsnapUser.cpp File 
    return ;  
}

bool UnixVsnapMgr::SetMountPointForVolume(char* MountPoint, char* VolumeSymLink, char* VolumeName)
{
    //Please see Windows dependent Code in WinVsnapUser.cpp File 
    return true ;
}


bool UnixVsnapMgr::UpdateMap(DiskBtree *VsnapBtree, SV_UINT FileId, SVD_DIRTY_BLOCK_INFO *Block)
{
    bool			Success = true;

    SINGLE_LIST_ENTRY		DirtyBlockHead;

    VsnapOffsetSplitInfo	*OffsetSplit = NULL;
    enum BTREE_STATUS		Status = BTREE_SUCCESS;

    VsnapBtree->SetCallBackThirdParam(&DirtyBlockHead);

    InitializeEntryList(&DirtyBlockHead);
    InsertInList(&DirtyBlockHead, Block->DirtyBlock.ByteOffset, (SV_UINT)Block->DirtyBlock.Length, FileId, 
        Block->DataFileOffset, 0);

    while(true)
    {
        if(IsSingleListEmpty(&DirtyBlockHead))
            break;

        VsnapKeyData NewKey;

        OffsetSplit = (VsnapOffsetSplitInfo *)PopEntryList(&DirtyBlockHead);

        NewKey.Offset		= OffsetSplit->Offset;		
        NewKey.Length		= OffsetSplit->Length;
        NewKey.FileId		= OffsetSplit->FileId;
        NewKey.FileOffset	= OffsetSplit->FileOffset;
        NewKey.TrackingId	= 0;

        /**
        *
        * Print the new key
        *
        */
        DebugPrintf(SV_LOG_DEBUG, "Key Start -------------\n");
        DebugPrintf(SV_LOG_DEBUG, "\n");

        DebugPrintf(SV_LOG_DEBUG, "NewKey.Offset = " ULLSPEC "\n", NewKey.Offset);
        DebugPrintf(SV_LOG_DEBUG, "NewKey.FileOffset = " ULLSPEC "\n", NewKey.FileOffset);
        DebugPrintf(SV_LOG_DEBUG, "NewKey.FileId = %u\n", NewKey.FileId);
        DebugPrintf(SV_LOG_DEBUG, "NewKey.Length = %u\n", NewKey.Length);
        DebugPrintf(SV_LOG_DEBUG, "NewKey.TrackingId = %u\n", NewKey.TrackingId);

        DebugPrintf(SV_LOG_DEBUG, "\n");
        DebugPrintf(SV_LOG_DEBUG, "Key End -------------\n");

        Status = VsnapBtree->BtreeInsert(&NewKey, true);
        if(Status == BTREE_FAIL)
        {
            DebugPrintf(SV_LOG_ERROR, "Btree insertion failed\n");
            Success = false;
            free(OffsetSplit);
            break;
        }
        free(OffsetSplit);
    }
    return Success;
}




bool UnixVsnapMgr::GetVsnapSourceVolumeDetails(char *SrcVolume, VsnapPairInfo *PairInfo)
{
    SVERROR				hr = SVS_OK;
    VsnapVirtualVolInfo	*VirtualInfo = PairInfo->VirtualInfo;
    VsnapSourceVolInfo	*SrcInfo = PairInfo->SrcInfo;
    VsnapErrorInfo		*VsnapError = (VsnapErrorInfo *)&VirtualInfo->Error;
    string				VolName;


    do
    {


        // In case of unix, we do not modify the filesystem signature
        // we can directly read it from the replicated volume.
        // no need to check if it is hidden ntfs etc as in case of windows
        SrcInfo->FsData = NULL;
        SrcInfo ->FsType = VSNAP_FS_UNKNOWN;

        VOLUME_STATE srcvolumeState;	
        srcvolumeState=GetVolumeState(SrcVolume);
        if(!((srcvolumeState==VOLUME_HIDDEN)||(srcvolumeState==VOLUME_VISIBLE_RO)))
            //		if(!IsVolumeLocked(SrcVolume))
        {
            VsnapError->VsnapErrorStatus = VSNAP_TARGET_UNHIDDEN_RW;
            VsnapError->VsnapErrorCode = ACE_OS::last_error();
            return false;
        }

        //VolName += SrcVolume;
        std::string sparsefile;
        bool isnewvirtualvolume = false;
        bool is_volpack = IsVolPackDevice(SrcVolume,sparsefile,isnewvirtualvolume);
        if(isnewvirtualvolume)
        {
            VolName = sparsefile;
            VolName += SPARSE_PARTFILE_EXT;
            VolName += "0";
        }
        else if(is_volpack)
        {
            VolName = sparsefile;
        }
        else
        {
            VolName = SrcVolume;
        }

        SV_ULONG dwerror;

        int fd = 0;
        char *Name = (char *)VolName.c_str();

        fd = open(Name, O_RDONLY, 0);
        if (fd < 0) {
            VsnapError->VsnapErrorStatus = VSNAP_TARGET_HANDLE_OPEN_FAILED;
            VsnapError->VsnapErrorCode = ACE_OS::last_error(); 
            return false;
        }
        if((!IsVolpackDriverAvailable())&& is_volpack)
        {
            SrcInfo->SectorSize = VSNAP_DEFAULT_SECTOR_SIZE;
        }
        else if (!GetSectorSizeWithFd(fd, Name, SrcInfo->SectorSize))
        {
            VsnapError->VsnapErrorCode = ACE_OS::last_error(); 
            VsnapError->VsnapErrorStatus = VSNAP_TARGET_SECTORSIZE_UNKNOWN;
            SrcInfo->SectorSize = VSNAP_DEFAULT_SECTOR_SIZE;	
            close(fd);
            return false; 
        }

        std::string volumeName = SrcVolume;

        if(m_TheConfigurator)
        {
            if(!getSourceRawSize((*m_TheConfigurator),volumeName,SrcInfo->ActualSize))
            {
                VsnapError->VsnapErrorCode = 0;//ACE_OS::last_error(); 
                VsnapError->VsnapErrorStatus = VSNAP_TARGET_VOLUMESIZE_UNKNOWN;
                close(fd);
                return false;
            }

            if(!getSourceCapacity((*m_TheConfigurator),volumeName,SrcInfo->UserVisibleSize))
            {
                VsnapError->VsnapErrorCode = 0;//ACE_OS::last_error(); 
                VsnapError->VsnapErrorStatus = VSNAP_TARGET_VOLUMESIZE_UNKNOWN;
                close(fd);
                return false;
            }

        }
        else
        {
            DebugPrintf(SV_LOG_INFO, "Configuration information is not available. Unable to get the source volumesize.\n");
            VsnapError->VsnapErrorCode = 0;//ACE_OS::last_error(); 
            VsnapError->VsnapErrorStatus = VSNAP_TARGET_VOLUMESIZE_UNKNOWN;
            close(fd);
            return false;
        }

        close(fd);

    } while (FALSE);

    return true;
}

ACE_HANDLE UnixVsnapMgr::OpenVirtualVolumeControlDevice()
{
    int hDevice = 0;
    char *vsnap_dev = "/dev/vsnapcontrol";

    hDevice = open(vsnap_dev, O_RDONLY, 0); 
    if (hDevice < 0) {  
        DebugPrintf(SV_LOG_INFO,"VSNAP INFO : Unable to open the Vsnap Device \n "); 
        return -1  ; 
    }

    return ((ACE_HANDLE)hDevice);
}


void UnixVsnapMgr::DeleteAutoAssignedDriveLetter(ACE_HANDLE hDevice, char* MountPoint, bool IsMountPoint)
{
    //Please see Windows Specfic Vsnap code in WinVsnapUser.cpp File 
    return ;
}


int WaitForProcessCompletion(ACE_Process_Manager * pm, pid_t pid,VsnapErrorInfo *Error)
{
    std::ostringstream msg;

    ACE_Time_Value timeout;
    ACE_exitcode status = 0;

    //Error = &VirtualInfo->Error;

    while (true)
    {

        pid_t rc = pm->wait(pid, timeout, &status); // just check for exit don't wait
        if (ACE_INVALID_PID == rc)
        {
            // msg << "script failed in ACE_Process_Manager::wait with error no: "
            //       << ACE_OS::last_error() << '\n';
            //m_err += msg.str();
            Error->VsnapErrorCode = ACE_OS::last_error();
            Error->VsnapErrorStatus = L_VSNAP_MOUNT_FAILED;
            return 1;
        }
        else if (rc == pid)
        {
            if (0 != status)
            {
                //                                msg << "script failed with exit status " << status << '\n';
                //                                m_err += msg.str();
                Error->VsnapErrorStatus=L_VSNAP_MOUNT_FAILED;

                Error->VsnapErrorCode = status;
                return status;
            }
            return 0;
        }
    }
    return 0;
}

bool UnixVsnapMgr::GetVsnapContextFromVolumeLink(ACE_HANDLE hDevice, wchar_t * VolumeLink, VsnapVirtualVolInfo *VirtualInfo, 
                                                 VsnapContextInfo *VsnapContext)
{
    DebugPrintf(SV_LOG_ERROR,"%s is not implemented for linux\n", FUNCTION_NAME);
    return true;

}

bool UnixVsnapMgr::UnmountVirtualVolume(char* SnapshotDrive, const size_t SnapshotDriveLen, VsnapVirtualVolInfo* VirtualInfo,bool bypassdriver)
{
    std::string output, error;
    return UnmountVirtualVolume(SnapshotDrive, SnapshotDriveLen, VirtualInfo, output, error,bypassdriver);
}

bool UnixVsnapMgr::Unmount(VsnapVirtualInfoList & VirtualInfoList, bool ShouldDeleteVsnapLogs, bool SendStatus,bool bypassdriver)
{
    std::string output, error;
    return Unmount(VirtualInfoList, ShouldDeleteVsnapLogs, SendStatus, output, error,bypassdriver);
}

//Bug #4044
bool UnixVsnapMgr::Unmount(VsnapVirtualInfoList & VirtualInfoList, bool ShouldDeleteVsnapLogs, bool SendStatus, std::string& output, std::string& error,bool bypassdriver)
{

    VsnapVirtualIter	VirtualIterBegin = VirtualInfoList.begin();
    VsnapVirtualIter	VirtualIterEnd = VirtualInfoList.end();
    char				MntPt[MOUNT_PATH_LEN];
    string				LogDir;

    std::string m_Substring ;
    m_Substring.clear();	

    for( ; VirtualIterBegin != VirtualIterEnd ; ++VirtualIterBegin )
    {
        VsnapVirtualVolInfo *VirtualInfo = (VsnapVirtualVolInfo *)*VirtualIterBegin;
        VirtualInfo->Error.VsnapErrorStatus = VSNAP_SUCCESS;
        VirtualInfo->Error.VsnapErrorCode = ACE_OS::last_error(); 

        size_t len = 0;

        // VSNAPFC - Persistence	
        // VsnapVirtualVolInfo->VolumeName can be either mountpoint or devicename,
        // when the unmount is done from cdpcli
        // It will be mountpoint only (or "") when invoked from Cx
        // Hence we are checking whether VolumeName contains the pattern "/dev/"

        std::string temp = VirtualInfo->VolumeName.empty() ? VirtualInfo->DeviceName : VirtualInfo->VolumeName;

        if(!VirtualInfo->DeviceName.empty())
            temp = VirtualInfo->DeviceName;

        inm_strcpy_s(MntPt, ARRAYSIZE(MntPt), temp.c_str());

        //Bug 5347
        //The trailing '/' needs to be removed while querying driver

        TruncateTrailingBackSlash(MntPt);

        if (temp.substr(0,5) != "/dev/")
        {
            temp.clear();
            GetdevicefilefromMountpoint(MntPt, temp);
            if(temp.empty())
            {
                VirtualInfo->Error.VsnapErrorStatus = VSNAP_GET_VOLUME_INFO_FAILED ;
                VirtualInfo->Error.VsnapErrorCode = ACE_OS::last_error() ;
                error = "Failed to get volume information for the mountpoint ";
                error += MntPt;
                DebugPrintf(SV_LOG_INFO, "%s", error.c_str());
                return false ;
            }
        }

        VsnapPersistInfo vsnapinfo;
        std::ostringstream errstr;

        std::vector<VsnapPersistInfo> vsnaps;
        if(!RetrieveVsnapInfo(vsnaps, "this", "", temp))
        {
            errstr << "Failed to retrieve vsnap information for " << VirtualInfo->VolumeName
                << std::endl;
            error = errstr.str().c_str();
            DebugPrintf(SV_LOG_INFO, "%s", errstr.str().c_str());
            return false;
        }

        if(!vsnaps.empty())
        {
            if(vsnaps.size() > 1)
            {
                errstr << "ERROR - More than 1 vsnap present with the same mountpoint" << VirtualInfo->VolumeName
                    << std::endl;
                error = errstr.str().c_str();
                DebugPrintf(SV_LOG_INFO, "%s", errstr.str().c_str());
                return false;
            }

            vsnapinfo = vsnaps.back();

            VirtualInfo->State = VSNAP_UNMOUNT;
            VirtualInfo->DeviceName = temp;
            DebugPrintf(SV_LOG_INFO, "Started unmounting the vsnap for the vsnap device %s\n",
                temp.c_str());

            int fd = -1;
            if (!OpenLinvsnapSyncDev(temp, fd))
            {
                VirtualInfo->Error.VsnapErrorStatus = VSNAP_OPEN_LINVSNAP_BLOCK_DEV_FAILED ;
                VirtualInfo->Error.VsnapErrorCode = ACE_OS::last_error() ;
                errstr << "ERROR - failed to open the linvsnap sync driver. Hence not deleting vsnap " << temp << "\n";
                error = errstr.str().c_str();
                DebugPrintf(SV_LOG_ERROR, "%s", errstr.str().c_str());
                return false;
            }

            bool bzpooldestroyrval = RetryZpoolDestroyIfReq(temp, vsnapinfo.target, ShouldDeleteVsnapLogs);

            if (!CloseLinvsnapSyncDev(temp, fd))
            {
                VirtualInfo->Error.VsnapErrorStatus = VSNAP_CLOSE_LINVSNAP_BLOCK_DEV_FAILED ;
                VirtualInfo->Error.VsnapErrorCode = ACE_OS::last_error() ;
                errstr << "ERROR - failed to close the linvsnap sync driver. This state is inconsistent for vsnap " << temp << "\n";
                error = errstr.str().c_str();
                DebugPrintf(SV_LOG_ERROR, "%s", errstr.str().c_str());
                return false;
            }

            bool bunmountrval = false;
            if (bzpooldestroyrval)
            {
				if(!bypassdriver)
        		{
					bool vgcleanupneeded = true;
					if(!ShouldDeleteVsnapLogs)
					vgcleanupneeded = false;
					if(!RemoveVgWithAllLvsForVsnap(VirtualInfo->DeviceName,output,error,vgcleanupneeded))
            		{
						DebugPrintf(SV_LOG_INFO, "%s", error.c_str());
                		return false;
            		}			
				}
                bunmountrval = UnmountVirtualVolume(MntPt, ARRAYSIZE(MntPt), VirtualInfo, output, error,bypassdriver);
            }

            if (!bzpooldestroyrval)
            {
                VirtualInfo->Error.VsnapErrorStatus = VSNAP_ZPOOL_DESTROY_EXPORT_FAILED;
                VirtualInfo->Error.VsnapErrorCode = ACE_OS::last_error() ;
                errstr << "ERROR - failed to destroy or export the zpool for vsnap. Hence not deleting vsnap " << temp << "\n";
                error = errstr.str().c_str();
                DebugPrintf(SV_LOG_INFO, "%s", errstr.str().c_str());
                return false;
            }

            if(bunmountrval)
            {
                if(ShouldDeleteVsnapLogs)
                {
                    //Bug #8543
                    // Delete the vsnap persistence file when the
                    // corresponding vsnap is deleted


                    bool rv = DeleteVsnapLogs(vsnapinfo);

                    if(!rv)
                    {
                        errstr << "Unable to delete vsnap logs for " << vsnapinfo.mountpoint << " for target "
                            << vsnapinfo.target << std::endl;
                        error = errstr.str().c_str();
                        DebugPrintf(SV_LOG_INFO, "%s", errstr.str().c_str());
                        return false;
                    }
                    DebugPrintf(SV_LOG_DEBUG, "Delete vsnap logs suceeded for the vsnap ",
                        vsnapinfo.mountpoint.c_str());
                    std::string devicefile = VirtualInfo->VolumeName;

                    if(devicefile.substr(0,5) != "/dev/")
                    {
                        devicefile = temp;		
                    }

                    std::string location;


                    if(!VsnapUtil::construct_persistent_vsnap_filename(devicefile, vsnapinfo.target, location))
                    {
                        errstr << "Failed to construct vsnap persistent information for " 
                            << devicefile << " " << FUNCTION_NAME << " " <<  LINE_NO << std::endl;
                        error = errstr.str().c_str();
                        DebugPrintf(SV_LOG_DEBUG, "%s", errstr.str().c_str());
                        return false;
                    }

                    // PR#10815: Long Path support
                    if((ACE_OS::unlink(getLongPathName(location.c_str()).c_str()) < 0) && (ACE_OS::last_error() != ENOENT))
                    {
                        errstr << "Unable to delete persistent information for " << devicefile
                            << " with error " << ACE_OS::strerror(ACE_OS::last_error()) << "\n";
                        error = errstr.str().c_str();
                        DebugPrintf(SV_LOG_DEBUG, "%s", errstr.str().c_str());
                        return false;
                    }
                    DebugPrintf(SV_LOG_DEBUG, "Delete vsnap persistent information for %s succeeded\n",
                        devicefile.c_str());
                }
                bool notifyTocx = false;
                if(SendStatus)
                    notifyTocx =  SendUnmountStatus(0, VirtualInfo, vsnapinfo);
                if(!notifyTocx)
                {
                    ACE_HANDLE handle = ACE_INVALID_HANDLE;
                    if(!VsnapUtil::create_pending_persistent_vsnap_file_handle(temp,
                        vsnapinfo.target,handle))
                    {
                        DebugPrintf(SV_LOG_ERROR, "The vsnap device %s will not be removed from CX UI. Use force delete option to delete the vsnap from CX UI.\n",
                            temp.c_str());
                        return false;
                    }
                    ACE_OS::close(handle);
                }
                DebugPrintf(SV_LOG_INFO, "Unmount suceeded for the vsnap %s\n",
                    temp.c_str());
                //unlock the vsnap locked bookmark	
                LocalConfigurator localConfigurator;
                if((!vsnapinfo.tag.empty()) && localConfigurator.isRetainBookmarkForVsnap())
                {
                    std::string dbname;
                    if(getCdpDbName(vsnapinfo.target,dbname ))
                    {
                        if(std::equal(dbname.begin() + dbname.size() - CDPV3DBNAME.size(), dbname.end(), CDPV3DBNAME.begin()))
                        {
                            SV_USHORT newstatus= BOOKMARK_STATE_UNLOCKED;
                            SV_USHORT oldstatus= BOOKMARK_STATE_LOCKEDBYVSNAP;
                            if(!updateBookMarkLockStatus(dbname,vsnapinfo.tag,oldstatus,newstatus))
                            {
                                DebugPrintf(SV_LOG_DEBUG, "Unable to unlock the bookmark %s in the db.\n",
                                    vsnapinfo.tag.c_str());
                            }
                        }
                    }
                    else
                    {
                        DebugPrintf(SV_LOG_DEBUG, "Unable to get the db name. So the bookmark %s remains in locked state.\n",
                            vsnapinfo.tag.c_str());
                    }
                }				
            }
            else
            {
                DebugPrintf(SV_LOG_INFO, "Unmount failed for the vsnap %s\n",
                    VirtualInfo->VolumeName.c_str());
                if(SendStatus)
                    SendUnmountStatus(1, VirtualInfo, vsnapinfo);
                return false;
            }
        }
        else
        {
            VirtualInfo->Error.VsnapErrorStatus = VSNAP_VIRTUAL_VOLUME_NOTEXIST ;
            errstr << VirtualInfo->VolumeName << " is not a valid virtual snapshot name." << std::endl;
            error = errstr.str().c_str();
            DebugPrintf(SV_LOG_INFO, "%s", errstr.str().c_str());
            return false;
        }
    }

    return true;
}


bool UnixVsnapMgr::ValidateSourceVolumeOfVsnapPair(VsnapPairInfo *PairInfo)
{
    char				SrcVolume[MOUNT_PATH_LEN];
    VsnapSourceVolInfo	*SrcInfo = PairInfo->SrcInfo;
    VsnapVirtualVolInfo	*VirtualInfo = PairInfo->VirtualInfo;

    inm_strcpy_s(SrcVolume, ARRAYSIZE(SrcVolume), SrcInfo->VolumeName.c_str());
    TruncateTrailingBackSlash(SrcVolume);

    if(!GetVsnapSourceVolumeDetails(SrcVolume, PairInfo))
    {
        VsnapErrorPrint(PairInfo);
        return false;
    }

    SrcInfo->VolumeName = SrcVolume;

    return true;
}


bool UnixVsnapMgr::ValidateVirtualVolumeOfVsnapPair(VsnapPairInfo *PairInfo, bool & ShouldUnmount)
{
    char				VirtualVolume[MOUNT_PATH_LEN];
    VsnapVirtualVolInfo *VirtualInfo = PairInfo->VirtualInfo;
    VsnapErrorInfo		*Error = &PairInfo->VirtualInfo->Error;

    inm_strcpy_s(VirtualVolume, ARRAYSIZE(VirtualVolume), VirtualInfo->VolumeName.c_str());

    TruncateTrailingBackSlash(VirtualVolume); //3664 
    VirtualInfo->VolumeName = VirtualVolume ; //3664

    std::string virtualVolume = VirtualInfo->VolumeName;
    std::string vol;

    vol = virtualVolume;

    if(*(vol.c_str() + vol.size() - 1) != '\\')
        vol += '\\';

    //
    //	Bug #6133
    //	check for vsnap on linux:

    //	In  case of recovery: Drive Letter/Mount point should be non existent
    //	eg: /home = /home should  be a non existent directory or it exists but no filesystems mounted on it or beneath any directory under it


    //	In case of schedule,
    //	eg: /home = /home can exist and at max a vsnap can be mounted on it (which should be unmounted)


    std::string mountedVolumes;
    if(VirtualInfo->VsnapType == VSNAP_RECOVERY)
    {
        if(IsFileORVolumeExisting(virtualVolume.c_str()) && (::containsMountedVolumes(virtualVolume, mountedVolumes)))
        {
            Error->VsnapErrorStatus	= VSNAP_CONTAINS_MOUNTED_VOLUMES;
            Error->VsnapErrorMessage = 	mountedVolumes;
            Error->VsnapErrorCode =	0;//ACE_OS::last_error();
            return false;
        }
    }
    else if(VirtualInfo->VsnapType == VSNAP_POINT_IN_TIME)
    {
        if(IsFileORVolumeExisting(virtualVolume.c_str()))
        {
            bool contains = ::containsMountedVolumes(virtualVolume, mountedVolumes);
            if(contains && (mountedVolumes == VirtualInfo->VolumeName))
            {
                std::string deviceName;
                GetdevicefilefromMountpoint(VirtualInfo->VolumeName.c_str(), deviceName);
                if(sameVolumeCheck(VirtualInfo->DeviceName, deviceName))
                {
                    contains = false;
                }	
            }
            if(contains)
            {
                Error->VsnapErrorStatus	= VSNAP_CONTAINS_MOUNTED_VOLUMES;
                Error->VsnapErrorMessage =  mountedVolumes;
                Error->VsnapErrorCode =	0;//ACE_OS::last_error();
                return false;
            }
        }

    }

    if(m_TheConfigurator)
    {
        if(isTargetVolume(virtualVolume.c_str()))
        {
            Error->VsnapErrorStatus	= VSNAP_VIRTUAL_DRIVELETTER_INUSE;
            Error->VsnapErrorCode =	0;//ACE_OS::last_error();
            return false;
        }
    }

    if(!IsMountPointValid(VirtualVolume, PairInfo,ShouldUnmount))
    {
        VsnapErrorPrint(PairInfo);
        return false;
    }
    else if(!CheckForVirtualVolumeReusability(PairInfo,ShouldUnmount))
        return false;

    return true;
}
/*
* FUNCTION NAME :  UnixVsnapMgr::RemountV2
*
* DESCRIPTION : Remounts Vsnap
*				
* INPUT PARAMETERS : VsnapPersistInfo
*
* OUTPUT PARAMETERS : None
*
* NOTES : 
*
* return value : true if success, else false
*
*/
bool UnixVsnapMgr::RemountV2(const VsnapPersistInfo& remountInfo)
{
    bool			rv = true;
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s for %s\n", FUNCTION_NAME, remountInfo.devicename.c_str());
    try 
    {
        do 
        {
            VsnapMountInfo	MountData;	
            SV_ULONG		StartTime = 0;
            char			SnapDrives[2048];

            StartTick(StartTime);
            memset(&MountData, 0, sizeof(VsnapMountInfo));
            memset(&SnapDrives, 0, sizeof(SnapDrives));


            VsnapVirtualVolInfo VirtualInfo;
            VsnapSourceVolInfo SrcInfo;

            VirtualInfo.VolumeName = remountInfo.mountpoint;
            VirtualInfo.DeviceName = remountInfo.devicename;
            VirtualInfo.State = VSNAP_REMOUNT;

            VsnapPairInfo PairInfo;
            PairInfo.SrcInfo = &SrcInfo;
            PairInfo.VirtualInfo = &VirtualInfo;

            MountData.IsMapFileInRetentionDirectory = boost::lexical_cast<bool>(remountInfo.mapfileinretention);
            inm_strncpy_s(MountData.RetentionDirectory, ARRAYSIZE(MountData.RetentionDirectory), remountInfo.retentionpath.c_str(), remountInfo.retentionpath.size());
            inm_strncpy_s(MountData.PrivateDataDirectory, ARRAYSIZE(MountData.PrivateDataDirectory), remountInfo.datalogpath.c_str(), remountInfo.datalogpath.size());

            // PR#10815: Long Path support
            if(ACE_OS::access(getLongPathName(MountData.PrivateDataDirectory).c_str(), 0)
                && SVMakeSureDirectoryPathExists(MountData.PrivateDataDirectory).failed())
            {
                DebugPrintf(SV_LOG_ERROR, "%s  %d: Error while accessing %s\n", FUNCTION_NAME,
                    LINE_NO, MountData.PrivateDataDirectory);
                rv = false;
                break;
            }

            std::string MapFileDir = MountData.IsMapFileInRetentionDirectory ? 
                MountData.RetentionDirectory : MountData.PrivateDataDirectory;

            PairInfo.SrcInfo->RetentionDbPath = MapFileDir;
            PairInfo.VirtualInfo->SnapShotId = remountInfo.snapshot_id;

            if(!InitializeRemountVsnap(MapFileDir, &PairInfo))
            {
                DebugPrintf(SV_LOG_ERROR, "%s  %d: Error accessing vsnap metadata present in %s\n", FUNCTION_NAME,
                    LINE_NO, MapFileDir.c_str());
                rv = false;
                break;
            }

            MountData.VolumeSize = remountInfo.volumesize;
            MountData.VolumeSectorSize = remountInfo.sectorsize;
            MountData.SnapShotId = remountInfo.snapshot_id;
            MountData.NextDataFileId = 0;
            MountData.RecoveryTime = remountInfo.recoverytime;
            inm_strncpy_s(MountData.VolumeName, ARRAYSIZE(MountData.VolumeName), remountInfo.target.c_str(), remountInfo.target.size());
            std::string dbpath = m_TheConfigurator->getCdpDbName(remountInfo.target);
            if( (dbpath.size() > CDPV3DBNAME.size()) &&
                (std::equal(dbpath.begin() + dbpath.size() - CDPV3DBNAME.size(), dbpath.end(), CDPV3DBNAME.begin()))
                )
            {
                MountData.IsSparseRetentionEnabled = true;
            }
            else
                MountData.IsSparseRetentionEnabled = false;

            MountData.IsFullDevice = isSourceFullDevice(remountInfo.target);

            switch(remountInfo.accesstype)
            {
            case 0:
                MountData.AccessType = VSNAP_READ_ONLY;
                break;
            case 1:
                MountData.AccessType = VSNAP_READ_WRITE;
                break;
            case 2:
                MountData.AccessType = VSNAP_READ_WRITE_TRACKING;
                break;
            }

            VirtualInfo.AccessMode = MountData.AccessType;

            inm_strncpy_s(MountData.VirtualVolumeName, ARRAYSIZE(MountData.VirtualVolumeName), remountInfo.devicename.c_str(), remountInfo.devicename.size());
            MountData.IsTrackingEnabled = (MountData.AccessType == VSNAP_READ_WRITE_TRACKING);

            DebugPrintf(SV_LOG_INFO,"Creating the vsnap device %s\n",remountInfo.devicename.c_str());

            inm_strncpy_s(SnapDrives, ARRAYSIZE(SnapDrives), remountInfo.mountpoint.c_str(), remountInfo.mountpoint.size());


            bool fsmount=false;
            if(!MountVol(&VirtualInfo, &MountData, SnapDrives,fsmount))
            {
                /*if(!fsmount)
                    CleanupFiles(&PairInfo);*/
                DebugPrintf(SV_LOG_ERROR, "%s  %d: Unable to mount vsnap %s\n", FUNCTION_NAME,
                    LINE_NO, SnapDrives);
                rv = false;
            }
            else
            {
                EndTick(StartTime);
            }
        } while (0);
    }
    catch ( ContextualException& ce )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
            "\n%s encountered exception %s.\n",
            FUNCTION_NAME, ce.what());
    }
    catch( exception const& e )
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
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s for %s\n", FUNCTION_NAME, remountInfo.devicename.c_str());
    return rv;
}



void UnixVsnapMgr::PerformMountOrRemount(VsnapSourceVolInfo *SourceInfo)
{
    bool			Success = true;
    bool			SourceValidated = false;
    string			MapFileDir;
    VsnapMountInfo	MountData;	
    VsnapPairInfo	PairInfo;
    SV_ULONG		StartTime = 0;
    string			DisplayString;
    char			SnapDrives[2048];

    StartTick(StartTime);
    memset(&MountData, 0, sizeof(VsnapMountInfo));

    VsnapVirtualIter VirtualIterBegin = SourceInfo->VirtualVolumeList.begin();
    VsnapVirtualIter VirtualIterEnd = SourceInfo->VirtualVolumeList.end();

    SourceInfo->FsData = NULL;
    SourceInfo->FsType = VSNAP_FS_UNKNOWN;
    //    SourceInfo->SectorSize = 0;
    //    SourceInfo->VolumeSize = 0;

    std::string recoveryLock = SourceInfo->VolumeName;
    FormatVolumeName(recoveryLock);

    // while purge operation is in progress, we do not want
    // any recovery jobs to access the retention files
    CDPLock purge_lock(recoveryLock, true, ".purgelock");
    purge_lock.acquire_read();

    CDPLock recoverCdpLock(recoveryLock);
    if(!recoverCdpLock.acquire_read())
    {
		for( ; VirtualIterBegin != VirtualIterEnd ; ++VirtualIterBegin )
		{
			VsnapVirtualVolInfo *VirtualInfo = (VsnapVirtualVolInfo *)*VirtualIterBegin;
			VirtualInfo->Error.VsnapErrorCode = 0;
			VirtualInfo->Error.VsnapErrorStatus = VSNAP_ACQUIRE_RECOVERY_LOCK_FAILED;
			VirtualInfo->VsnapProgress = VSNAP_FAILED_STATUS;
		}
        DebugPrintf(SV_LOG_ERROR, "%s%s%s","cdpcli could not acquire read lock on volume.",recoveryLock.c_str(),"\nPlease stop any process (such as svagent) accessing the volume and try again.\n");
        return;
    }

    for( ; VirtualIterBegin != VirtualIterEnd ; ++VirtualIterBegin )
    {
        bool			ShouldUnmount = false;
        VsnapVirtualVolInfo *VirtualInfo = (VsnapVirtualVolInfo *)*VirtualIterBegin;
        bool prescript_executed = false;
        VirtualInfo->Error.VsnapErrorCode = ACE_OS::last_error() ; // TODO: Error Code is a SV_ULONG
        VirtualInfo->Error.VsnapErrorStatus = VSNAP_SUCCESS;
        VirtualInfo->VsnapProgress = VSNAP_FAILED_STATUS;

        PairInfo.SrcInfo = SourceInfo;
        PairInfo.VirtualInfo = VirtualInfo;


        if(!ValidateMountArgumentsForPair(&PairInfo))
        {
            VsnapErrorPrint(&PairInfo);
            continue;
        }

        if(!SourceValidated && !ValidateSourceVolumeOfVsnapPair(&PairInfo))
        {
            VsnapErrorPrint(&PairInfo);
            break;
        }

        SourceValidated = true;
        if(!ValidateVirtualVolumeOfVsnapPair(&PairInfo,ShouldUnmount))
        {
            SendStatus(&PairInfo); 
            VsnapErrorPrint(&PairInfo);
            continue; 
        }
        VirtualInfo->VsnapProgress = MAP_START_STATUS;  
        SendStatus(&PairInfo); 


        std::string dbpath = dirname_r(SourceInfo->RetentionDbPath.c_str());

        if(strlen(VirtualInfo->PrivateDataDirectory.c_str())!=0)
        {
            MapFileDir = VirtualInfo->PrivateDataDirectory;
            MountData.IsMapFileInRetentionDirectory = false;
            inm_strcpy_s(MountData.RetentionDirectory, ARRAYSIZE(MountData.RetentionDirectory), dbpath.c_str());
            inm_strcpy_s(MountData.PrivateDataDirectory, ARRAYSIZE(MountData.PrivateDataDirectory), VirtualInfo->PrivateDataDirectory.c_str());

            // PR#10815: Long Path support
            if(ACE_OS::access(getLongPathName(MapFileDir.c_str()).c_str(), 0)
                && SVMakeSureDirectoryPathExists(MapFileDir.c_str()).failed())
            {
                VirtualInfo->Error.VsnapErrorStatus = VSNAP_DATADIR_CREATION_FAILED;
                VirtualInfo->Error.VsnapErrorCode = ACE_OS::last_error() ;// TODO: Error Code is a SV_ULONG
                VsnapErrorPrint(&PairInfo);

                VirtualInfo->VsnapProgress = VSNAP_FAILED_STATUS;
                SendStatus(&PairInfo);
                continue;
            }

        }
        else
        {
            MapFileDir = dbpath;
            MountData.IsMapFileInRetentionDirectory = true;
            inm_strcpy_s(MountData.RetentionDirectory, ARRAYSIZE(MountData.RetentionDirectory), dbpath.c_str());
            inm_strcpy_s(MountData.PrivateDataDirectory, ARRAYSIZE(MountData.PrivateDataDirectory), dbpath.c_str());
        }

        VirtualInfo->PreScriptExitCode = 0;
        VirtualInfo->PostScriptExitCode = 0;

        std::string output, error;
        VirtualInfo->PreScriptExitCode = RunPreScript(VirtualInfo, SourceInfo, output, error);
        if(VirtualInfo->PreScriptExitCode != 0)
        {
            VirtualInfo->Error.VsnapErrorCode = VirtualInfo->PreScriptExitCode;
            VirtualInfo->Error.VsnapErrorStatus = VSNAP_PRESCRIPT_EXEC_FAILED;
            VirtualInfo->Error.VsnapErrorMessage = error;
            VsnapErrorPrint(&PairInfo);

            VirtualInfo->PostScriptExitCode = RunPostScript(VirtualInfo, SourceInfo, output, error); 

            if (VirtualInfo->PostScriptExitCode != 0 )  
            {
                VirtualInfo->Error.VsnapErrorCode = VirtualInfo->PostScriptExitCode; 
                VirtualInfo->Error.VsnapErrorStatus = VSNAP_POSTSCRIPT_EXEC_FAILED; 
                VirtualInfo->Error.VsnapErrorMessage = error;
                VsnapErrorPrint(&PairInfo); 

            }

            VirtualInfo->VsnapProgress = VSNAP_FAILED_STATUS; 
            SendStatus(&PairInfo, error);

            continue;
        }

        if(VirtualInfo->State == VSNAP_MOUNT)
        {
            if(VirtualInfo->VsnapType == VSNAP_RECOVERY)
            {
                VsnapPrint("Creating Recovery Virtual Snapshot...\n");
                if(CDPUtil::ToDisplayTimeOnConsole(VirtualInfo->TimeToRecover, DisplayString))
                    DebugPrintf(SV_LOG_INFO, "Recovery Time: %s ", DisplayString.c_str());
                if(!VirtualInfo->EventName.empty())
                    DebugPrintf(SV_LOG_INFO, "Event Name: %s \n",  VirtualInfo->EventName.c_str());
                else
                    DebugPrintf(SV_LOG_INFO, "\n");

                if(!VirtualInfo->SnapShotId)
                    VirtualInfo->SnapShotId = GetUniqueVsnapId();
                if(!VirtualInfo->SnapShotId)
                {
                    VirtualInfo->Error.VsnapErrorCode = 0;
                    VirtualInfo->Error.VsnapErrorStatus = VSNAP_UNIQUE_ID_FAILED;
                    VsnapErrorPrint(&PairInfo);

                    VirtualInfo->VsnapProgress = VSNAP_FAILED_STATUS; 
                    SendStatus(&PairInfo);
                    continue; 

                }

                VirtualInfo->VsnapProgress = MAP_PROGRESS_STATUS;
                SendStatus(&PairInfo);

                Success = ParseCDPDatabase(MapFileDir, &PairInfo);
                if(!Success)
                {
                    VsnapErrorPrint(&PairInfo);
                    std::string output, error;
                    VirtualInfo->PostScriptExitCode = RunPostScript(VirtualInfo, SourceInfo, output, error);

                    if (VirtualInfo->PostScriptExitCode != 0 ) 
                    {
                        VirtualInfo->Error.VsnapErrorCode = VirtualInfo->PostScriptExitCode ;
                        VirtualInfo->Error.VsnapErrorStatus = VSNAP_POSTSCRIPT_EXEC_FAILED;
                        VirtualInfo->Error.VsnapErrorMessage = error;
                        VsnapErrorPrint(&PairInfo);

                    }
                    VirtualInfo->VsnapProgress = VSNAP_FAILED_STATUS;
                    SendStatus(&PairInfo, error);
                    continue; 
                }

            }
            else if(VirtualInfo->VsnapType == VSNAP_POINT_IN_TIME)
            {
                SVERROR rc;

                VirtualInfo->VsnapProgress = MAP_PROGRESS_STATUS; 
                SendStatus(&PairInfo); 


                if(SourceInfo->RetentionDbPath.empty())
                {
                    SV_ULONGLONG recovery_time = 0;
                    if(!getTimeStampFromLastAppliedFile(SourceInfo->VolumeName,recovery_time))
                    {
                        VirtualInfo->TimeToRecover = 0;
                    }
                    else
                    {
                        VirtualInfo->TimeToRecover = recovery_time + SVEPOCHDIFF;
                    }
                }
                else
                {
                    if(!PollRetentionData(SourceInfo->RetentionDbPath, VirtualInfo->TimeToRecover))
                        VsnapPrint("Not able to get the retention settings...\n");

                }	

                VsnapPrint("Creating Point-in-time Virtual Snapshot...\n");
                if(!VirtualInfo->SnapShotId)
                    VirtualInfo->SnapShotId = GetUniqueVsnapId();

                if(!VirtualInfo->SnapShotId)
                {
                    VirtualInfo->Error.VsnapErrorCode = 0;
                    VirtualInfo->Error.VsnapErrorStatus = VSNAP_UNIQUE_ID_FAILED;
                    VsnapErrorPrint(&PairInfo);

                    VirtualInfo->VsnapProgress = VSNAP_FAILED_STATUS; 
                    SendStatus(&PairInfo);
                    continue; 

                }

                SendProgressToCx(&PairInfo, 0,0);

                if(!InitializePointInTimeVsnap(MapFileDir, &PairInfo,VirtualInfo->TimeToRecover))
                {
                    VirtualInfo->Error.VsnapErrorCode = ACE_OS::last_error();
                    VirtualInfo->Error.VsnapErrorStatus = VSNAP_PIT_INIT_FAILED;
                    VsnapErrorPrint(&PairInfo);
                    std::string output, error;
                    VirtualInfo->PostScriptExitCode = RunPostScript(VirtualInfo, SourceInfo, output, error);

                    if (VirtualInfo->PostScriptExitCode != 0 )  
                    {
                        VirtualInfo->Error.VsnapErrorCode = VirtualInfo->PostScriptExitCode ;
                        VirtualInfo->Error.VsnapErrorStatus = VSNAP_POSTSCRIPT_EXEC_FAILED;
                        VirtualInfo->Error.VsnapErrorMessage = error;
                        VsnapErrorPrint(&PairInfo);
                    }

                    VirtualInfo->VsnapProgress = VSNAP_FAILED_STATUS; 
                    SendStatus(&PairInfo, error);
                    continue;
                }

                SendProgressToCx(&PairInfo, 100,0); 
            }
            else
            {
                VsnapErrorPrint(&PairInfo);
                VirtualInfo->VsnapProgress = VSNAP_FAILED_STATUS; 
                SendStatus(&PairInfo);
                continue; 
            }
        }
        else if(VirtualInfo->State == VSNAP_REMOUNT)
        {	
            if(!InitializeRemountVsnap(MapFileDir, &PairInfo))
            {
                VsnapErrorPrint(&PairInfo);
                VirtualInfo->VsnapProgress = VSNAP_FAILED_STATUS; 
                SendStatus(&PairInfo);
                continue;
            }
        }
        else
        {
            VsnapErrorPrint(&PairInfo);
            VirtualInfo->VsnapProgress = VSNAP_FAILED_STATUS; 
            SendStatus(&PairInfo);
            continue;
        }

        VirtualInfo->VsnapProgress = MOUNT_START;
        SendStatus(&PairInfo);


        MountData.VolumeSize = SourceInfo->ActualSize;
        MountData.VolumeSectorSize = SourceInfo->SectorSize;
        MountData.SnapShotId = VirtualInfo->SnapShotId;
        MountData.NextDataFileId = VirtualInfo->NextFileId;
        MountData.RecoveryTime = VirtualInfo->TimeToRecover;
        inm_strcpy_s(MountData.VolumeName, ARRAYSIZE(MountData.VolumeName), SourceInfo->VolumeName.c_str());
        std::string retpath = SourceInfo->RetentionDbPath;
        if( (retpath.size() > CDPV3DBNAME.size()) &&
            (std::equal(retpath.begin() + retpath.size() - CDPV3DBNAME.size(), retpath.end(), CDPV3DBNAME.begin()))
            )
        {
            MountData.IsSparseRetentionEnabled = true;
        }
        else
            MountData.IsSparseRetentionEnabled = false;

        MountData.IsFullDevice = isSourceFullDevice(SourceInfo->VolumeName);

        MountData.AccessType = VirtualInfo->AccessMode;

        // VSNAPFC - Persistence
        // MountData(VsnapMountInfo)->VirtualVolumeName consists of the
        // autogenerated device name from cx or cdpcli like
        // /dev/vs/cxx or /dev/vx/clixx

        std::string deviceName = VirtualInfo->DeviceName;
        inm_strcpy_s(MountData.VirtualVolumeName, ARRAYSIZE(MountData.VirtualVolumeName), deviceName.c_str());

        if(VirtualInfo->AccessMode == VSNAP_READ_WRITE_TRACKING)
            MountData.IsTrackingEnabled = true;
        else
            MountData.IsTrackingEnabled = false;

        char str[2048];
        inm_sprintf_s(str, ARRAYSIZE(str), "Creating the vsnap device %s\n", VirtualInfo->DeviceName.c_str());
        VsnapPrint(str);

        inm_strcpy_s(SnapDrives, ARRAYSIZE(SnapDrives), VirtualInfo->VolumeName.c_str());
        bool FsmountFailed=false;	

        std::string psoutput, pserror;
        if(!MountVol(VirtualInfo, &MountData, SnapDrives,FsmountFailed))
        {
            VsnapErrorPrint(&PairInfo);
            CleanupFiles(&PairInfo);

            VirtualInfo->PostScriptExitCode = RunPostScript(VirtualInfo, SourceInfo, psoutput, pserror);

            if (VirtualInfo->PostScriptExitCode != 0 ) 
            {
                VirtualInfo->Error.VsnapErrorCode = VirtualInfo->PostScriptExitCode;
                VirtualInfo->Error.VsnapErrorStatus = VSNAP_POSTSCRIPT_EXEC_FAILED;
                VirtualInfo->Error.VsnapErrorMessage = pserror;
                VsnapErrorPrint(&PairInfo);
            }
        }
        else
        {
            LocalConfigurator localConfigurator;
            if((MountData.IsSparseRetentionEnabled) && (!VirtualInfo->EventName.empty())
                && (localConfigurator.isRetainBookmarkForVsnap()))
            {
                SV_USHORT newstatus= BOOKMARK_STATE_LOCKEDBYVSNAP;
                SV_USHORT oldstatus= BOOKMARK_STATE_UNLOCKED;
                if(!updateBookMarkLockStatus(SourceInfo->RetentionDbPath,VirtualInfo->EventName,oldstatus,newstatus))
                {
                    DebugPrintf(SV_LOG_DEBUG, "Unable to update the bookmark %s as locked in the db. So the tag based recovery vsnap may not be exist after coallesce. \n",
                        VirtualInfo->EventName.c_str());
                }                    
            }

            VirtualInfo->PostScriptExitCode = RunPostScript(VirtualInfo, SourceInfo, psoutput, pserror);

            VirtualInfo->VsnapProgress = VSNAP_COMPLETE_STATUS;

            std::ostringstream InfoForCx;
            if(FsmountFailed)
            {	
                if(VirtualInfo->Error.VsnapErrorStatus == L_VSNAP_MOUNT_FAILED)
                {
                    VirtualInfo->Error.VsnapErrorStatus = VSNAP_SUCCESS;

                    InfoForCx << "The vsnap device "<< VirtualInfo->DeviceName << " is created.\n"
                        << "However filesystem mount failed with " << VirtualInfo->Error.VsnapErrorMessage;
                }
                else
                {
                    InfoForCx << "The vsnap will not be mounted as the source file system is raw or unspecified\n";
                }
            }

            if(VirtualInfo->PostScriptExitCode != 0)
            {
                InfoForCx << "Postscript " << VirtualInfo->PostScript << " execution Failed.\n";
            }

            if(!InfoForCx.str().empty() && m_SVServer.empty())
                DebugPrintf(SV_LOG_INFO, "%s", InfoForCx.str().c_str());
            bool notifytocx = false;
            notifytocx = SendStatus(&PairInfo, InfoForCx.str().c_str());
            //change 9231 start
            if(!notifytocx)
            {
                if(!PersistPendingVsnap(MountData.VirtualVolumeName, MountData.VolumeName ))
                {
                    DebugPrintf(SV_LOG_ERROR, "The vsnap %s will not appear in the CX UI.\n", 
                        MountData.VirtualVolumeName);
                }
            }
            //change 9231 end

            EndTick(StartTime);
        }
    }

    if(SourceInfo->FsData)
        free(SourceInfo->FsData);

    return;
}


void UnixVsnapMgr::ProcessVolumeAndMountPoints(string & MountPoints, char *Vol, char *Buf, int iBufSize, bool UnmountFlag)
{
    return;
}

void UnixVsnapMgr::GetAllVirtualVolumes(string & Volumes)
{
    return GetOrUnmountAllVirtualVolumes(Volumes, false);
}



void UnixVsnapMgr::GetOrUnmountAllVirtualVolumes(string & MountPoints, bool DoUnmount)
{
    vector<volinfo> mtablist;
    process_proc_mounts(mtablist);
    std::string src;
    src = "/dev/vs";
#ifdef SV_SUN
    src += "/dsk";
#endif
    ACE_DIR *dirp = 0;
    dirent *dp = 0;
    if ((dirp = sv_opendir(src.c_str())) == NULL)
    {
        return ;
    }
    while((dp = ACE_OS ::readdir(dirp)) != NULL)
    {
        //Bug# 7934
        if(VsnapQuit())
        {
            DebugPrintf(SV_LOG_INFO, "Vsnap Operation Aborted as Quit Requested\n");
            break;
        }
        std::string dName = ACE_TEXT_ALWAYS_CHAR(dp->d_name);
        if ( dName == "." || dName == ".." )
            continue;
        std::string devname = src;
        devname += ACE_DIRECTORY_SEPARATOR_CHAR_A;
        devname += dName;
        if(IsValidVsnapDeviceFile(devname.c_str()))
        {
            string device=devname;
#ifdef SV_SUN
            size_t sz = devname.rfind("s");
            if(sz >14)
            {
                std::string tmp_dev = devname.substr(0,sz);
                if(std::string::npos == (sz = MountPoints.find(tmp_dev)))
                {
                    MountPoints +=tmp_dev;
                }
                else if((MountPoints[sz] != ' ') || (MountPoints[sz] != ','))
                {
                    MountPoints +=tmp_dev;
                }
                else
                    continue;
            }
            else
#endif
                MountPoints +=devname;
            for(int i=0; i<mtablist.size(); i++)
            {
                volinfo m = mtablist[i];

                if(device==m.devname)
                {
                    MountPoints +=" ";
                    MountPoints +=m.mountpoint;

                    if(DoUnmount)
                    {
                        VsnapVirtualInfoList VirtualList;
                        VsnapVirtualVolInfo VirtualInfo;
                        VirtualInfo.VolumeName =m.mountpoint;
                        VirtualInfo.State = VSNAP_UNMOUNT;
                        VirtualList.push_back(&VirtualInfo);
                        Unmount(VirtualList, true);
                    }
                }

            }		
            MountPoints += ",";

        }
    }
    ACE_OS::closedir(dirp);
}




bool UnixVsnapMgr::IsVolumeVirtual(char const * Vol)
{
    ACE_HANDLE	VVCtrlDevice = ACE_INVALID_HANDLE;
    wchar_t		VolumeLink[256];
    int		bResult;
    SV_ULONG   	BytesReturned = 0;
    string		UniqueId;
    char*		Buffer = NULL;
    SV_ULONG 	ulLength = 0;
    size_t		UniqueIdLength;
    UniqueId.clear();


    VVCtrlDevice = OpenVirtualVolumeControlDevice();
    if(ACE_INVALID_HANDLE == VVCtrlDevice)
        return false;

    UniqueId = Vol;
    UniqueIdLength = UniqueId.size();

    int ByteOffset  = 0;
    INM_SAFE_ARITHMETIC(ulLength    = (SV_ULONG) sizeof(SV_USHORT) + (InmSafeInt<size_t>::Type(UniqueIdLength) + 1), INMAGE_EX(sizeof(SV_USHORT))(UniqueIdLength))
    Buffer      = (char*) calloc(ulLength , 1);
    *(SV_USHORT*)Buffer = (SV_USHORT)(UniqueIdLength + 1);
    ByteOffset += sizeof(SV_USHORT);
    inm_memcpy_s(Buffer + ByteOffset, ulLength - ByteOffset, UniqueId.c_str(), (UniqueIdLength + 1));

    if (ioctl(VVCtrlDevice , IOCTL_INMAGE_GET_VOLUME_DEVICE_ENTRY, Buffer) < 0) 
    {
        DebugPrintf(SV_LOG_ERROR, "@ LINE %d in FILE %s IOCTL_INMAGE_GET_VOLUME_DEVICE_ENTRY failed with errno = %d for device %s\n", 
            LINE_NO, FILE_NAME, errno, UniqueId.c_str());
        close(VVCtrlDevice);
        return false;
    }

    free(Buffer);
    ACE_OS::close(VVCtrlDevice);
    return true;
}

void UnixVsnapMgr::DisconnectConsoleOutAndErrorHdl()
{
    //ACE_HANDLE NulHdl;	
    NulHdl = ACE_OS::open("NUL:", 3,ACE_DEFAULT_OPEN_PERMS);
    if(NulHdl == ACE_INVALID_HANDLE)
        return;

#ifdef SV_WINDOWS
    StdOutHdl = GetStdHandle(STD_OUTPUT_HANDLE);
    StdErrHdl = GetStdHandle(STD_ERROR_HANDLE);

    bool status;
    SV_ULONG err;
    status = SetStdHandle(STD_OUTPUT_HANDLE, NulHdl);
    if(!status)
        err = ACE_OS::last_error();
    status = SetStdHandle(STD_ERROR_HANDLE, NulHdl);
    if(!status)
        err = ACE_OS::last_error();
#endif
    /*CloseHandle(GetStdHandle(STD_OUTPUT_HANDLE));
    CloseHandle(GetStdHandle(STD_ERROR_HANDLE));*/
}


void UnixVsnapMgr::ConnectConsoleOutAndErrorHdl()
{
    /*ACE_HANDLE OutHdl = ACE_INVALID_HANDLE;

    OutHdl = CreateFile("CONOUT$", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 
    FILE_ATTRIBUTE_NORMAL, NULL);8
    if(OutHdl == ACE_INVALID_HANDLE)
    return;

    SetStdHandle(STD_OUTPUT_HANDLE, OutHdl);
    SetStdHandle(STD_ERROR_HANDLE, OutHdl);*/

    ACE_OS::close(NulHdl);
    NulHdl = ACE_INVALID_HANDLE;
#ifdef SV_WINDOWS
    SetStdHandle(STD_OUTPUT_HANDLE, StdOutHdl);
    SetStdHandle(STD_ERROR_HANDLE, StdErrHdl);
#endif
}

bool UnixVsnapMgr::OpenMountPointExclusive(ACE_HANDLE *Handle, char *TgtGuid, const size_t TgtGuidLen, char *TgtVol)
{

    return true;
}

bool UnixVsnapMgr::CloseMountPointExclusive(ACE_HANDLE	Handle, char *TgtGuid, char *TgtVol)
{

    return true;
}


long UnixVsnapMgr::OpenInVirVolDriver(ACE_HANDLE &hDevice, VsnapErrorInfo *Error)
{
    DRSTATUS Status = DRSTATUS_SUCCESS;
    hDevice = OpenVirtualVolumeControlDevice();

    if(hDevice < 0) {
        // TODO: driver is not loaded insmod it here. 

        hDevice = OpenVirtualVolumeControlDevice();
        if(hDevice < 0) {
            Error->VsnapErrorStatus = VSNAP_VIRVOL_OPEN_FAILED;
            Error->VsnapErrorCode = ACE_OS::last_error(); 
            Status = DRSTATUS_UNSUCCESSFUL;
            return Status;
        }
    }
    return Status;
}
bool UnixVsnapMgr::VolumeSupportsReparsePoints(char* VolumeName)
{
    SV_ULONG FileSystemFlags                       = 0;
    SV_ULONG SerialNumber                          = 0;
    bool  bResult                               = TRUE;
    bool  MountPointSupported                   = FALSE;
    char FileSystemNameBuffer[MAX_STRING_SIZE] = {0};

    return MountPointSupported;
}

void UnixVsnapMgr::DeleteMountPoints(char* TargetVolumeName, VsnapVirtualVolInfo *VirtualInfo)
{
    char   HostVolumeName[2048];
    bool    bResult         = TRUE;
    ACE_HANDLE  hHostVolume;
    hHostVolume=ACE_INVALID_HANDLE;
#ifdef SV_WINDOWS
    hHostVolume     = FindFirstVolume(HostVolumeName, 2048);
#else

#endif
    char	errstr[100];

    if(hHostVolume == ACE_INVALID_HANDLE)
    {
        inm_sprintf_s(errstr, ARRAYSIZE(errstr), "Could not enumerate volumes Error:%#x\n", ACE_OS::last_error());
        VsnapPrint(errstr);
        return;
    }

    do
    {
        if(VolumeSupportsReparsePoints(HostVolumeName))
            DeleteMountPointsForTargetVolume(HostVolumeName, TargetVolumeName, VirtualInfo);
#ifdef SV_WINDOWS    
        bResult = FindNextVolume(hHostVolume, HostVolumeName, 2048);
#endif
    }while(bResult);

#ifdef SV_WINDOWS 
    FindVolumeClose(hHostVolume);
#endif
}


size_t UnixVsnapMgr::tcstombs(char* mbstr, const size_t mbstrlen, const char* tstring)
{
    size_t size = 0;
    return size;
}

size_t UnixVsnapMgr::mbstotcs(char* tstring, const size_t tstringlen, const char* mbstr)
{
    size_t size = 0;

    return size;
}

size_t UnixVsnapMgr::wcstotcs(char* tstring, const size_t tstringlen, const wchar_t* wcstr)
{
    size_t size = 0;
    return size;
}


// Windows specific mount point validation.
bool UnixVsnapMgr::IsMountPointValid(char *MountPoint, VsnapPairInfo *PairInfo,bool & ShouldUnmount)
{
    return true ;

}

bool UnixVsnapMgr::DirectoryEmpty(char *Directory)
{

    return true ;
}

bool UnixVsnapMgr::CheckForVirtualVolumeReusability(VsnapPairInfo *PairInfo,bool & ShouldUnmount)

{
    VsnapVirtualVolInfo	*VirtualInfo 			= PairInfo->VirtualInfo;
    VsnapSourceVolInfo	*SrcInfo 				= PairInfo->SrcInfo;
    bool				tounmount 				= false;
    int					vollen 					= (int)VirtualInfo->VolumeName.size();
    bool				Success 				= true;
    VsnapErrorInfo		*Error 					= &PairInfo->VirtualInfo->Error;
    bool				VirtualVolumeExists 	= false;

    do
    {
        // VSNAPFC - Persistence
        // In case of vsnaps without mountpoint, we need to unmount them,
        // before trying to create them, required for scheduled vsnaps

        // Algo :
        // If mountpoint is provided, then try to obtain the devicename from the mountpoint,
        // and query the driver using the devicename obtained.
        // Otherwise query the driver using the devicename supplied by Cx or CDPCLI.

        std::string devName = VirtualInfo->DeviceName;

        if(!VirtualInfo->VolumeName.empty())
        {
            std::string mntPt = VirtualInfo->VolumeName;
            CDPUtil::trim(mntPt);
            if(!GetDeviceNameForMountPt(mntPt, devName))
                devName = VirtualInfo->DeviceName;
        }

        if(IsValidVsnapDeviceFile(devName.c_str()))
        {
            VirtualVolumeExists = true;
        }

        if(VirtualInfo->ReuseDriveLetter)
        {
            if(VirtualVolumeExists)
            {
                VsnapVirtualVolInfo *VirVol = new VsnapVirtualVolInfo();
                if(!VirVol)
                {
                    Error->VsnapErrorCode = 0;//ACE_OS::last_error(); 
                    Error->VsnapErrorStatus = VSNAP_MEM_UNAVAILABLE; 
                    Success = false ;
                    break;
                }

#ifdef SV_AIX
                if(!VirtualInfo->VolumeName.empty())
                {
                    std::string mountpoint = "\"";
                    mountpoint += VirtualInfo->VolumeName;
                    mountpoint += "\"";
                    DebugPrintf(SV_LOG_INFO, "Performing unmount operation on %s ...\n",mountpoint.c_str());
                    std::string command = "/usr/sbin/umount -f ";
                    command += mountpoint;

                    InmCommand unmount_cmd(command);
                    InmCommand::statusType status = unmount_cmd.Run();
                    if (status != InmCommand::completed) {
                        Error->VsnapErrorCode = ACE_OS::last_error();
                        Error->VsnapErrorStatus = VSNAP_UNMOUNT_FAILED_FS_PRESENT;
                        Error->VsnapErrorMessage = "vsnap is in use\n";
                        delete VirVol;
                        return false;
                    }
                    if(unmount_cmd.ExitCode())
                    {
                        Error->VsnapErrorCode = ACE_OS::last_error();
                        Error->VsnapErrorStatus = VSNAP_UNMOUNT_FAILED_FS_PRESENT;
                        Error->VsnapErrorMessage = "vsnap is in use\n";
                        delete VirVol;
                        return false;
                    }
                }
                VirVol->DeviceName = VirtualInfo->DeviceName;
#else
                VirVol->VolumeName = VirtualInfo->VolumeName;
                VirVol->DeviceName = VirtualInfo->DeviceName;
#endif

                VsnapVirtualInfoList VirList;
                VirList.push_back(VirVol);

                DebugPrintf(SV_LOG_DEBUG, "VSNAP DEBUG Unmount called from CheckForVirtualVolumeReusability\n");
                std::string output, error;
                if(!Unmount(VirList, true, false, output, error))
                {
                    Error->VsnapErrorCode = ACE_OS::last_error();  
                    Error->VsnapErrorStatus = VSNAP_UNMOUNT_FAILED; 
                    Error->VsnapErrorMessage = error;
                    delete VirVol;
                    return false;
                }
                delete VirVol;
            }
        }
        else
        {
            if(VirtualVolumeExists)
            {
                Error->VsnapErrorCode = 0;//ACE_OS::last_error() ;
                Error->VsnapErrorStatus = VSNAP_VIRTUAL_DRIVELETTER_INUSE;
                Success = false;
            }
        }
    } while(FALSE);

    return Success;
}


void UnixVsnapMgr::VsnapGetVirtualInfoDetaisFromVolume(VsnapVirtualVolInfo *VirtualInfo, VsnapContextInfo *VsnapContext)
{
    int 				CtrlDevice = 0;
    char 				*buffer = NULL;
    char				*Volname = NULL;
    VsnapErrorInfo			*Error = &VirtualInfo->Error;

    const int SIZE = 2048;

    Volname = (char *)malloc(SIZE);
    if(!Volname)
    {
        free(VsnapContext);
        Error->VsnapErrorCode = 0;//ACE_OS::last_error();
        Error->VsnapErrorStatus = VSNAP_MEM_UNAVAILABLE;
        return;
    }
    memset(Volname, 0, SIZE);
    // VSNAPFC - Persistence
    // As the linvsnap driver doesn't maintain the mountpoints,
    // we need to get the corresponding device name
    std::string devicename;
    if( VirtualInfo->DeviceName.empty())
    {
        devicename = VirtualInfo->VolumeName;
    }
    else
    {
        devicename = VirtualInfo->DeviceName;
    }
    if(devicename.substr(0,5) != "/dev/")
    {
        char volume[SIZE];
        memset(volume, 0, SIZE);
        inm_strncpy_s(volume, ARRAYSIZE(volume), devicename.c_str(), devicename.size());	
        TruncateTrailingBackSlash(volume);
        devicename.clear();
        if(!GetdevicefilefromMountpoint(volume,devicename))
        {
            VirtualInfo->Error.VsnapErrorStatus = VSNAP_VOLUME_INFO_FAILED ;
            VirtualInfo->Error.VsnapErrorCode = ACE_OS::last_error() ;
            return ;    
        }
    }

    // VSNAPFC - Persistence

    inm_strcpy_s(Volname, SIZE, devicename.c_str());

    //Bug 5347
    //The trailing '/' needs to be removed for obtaining
    //the context(VsnapContextInfo) from the driver
    TruncateTrailingBackSlash(Volname);

    SV_USHORT  Vollen;
    INM_SAFE_ARITHMETIC(Vollen = InmSafeInt<size_t>::Type(strlen(Volname))+1, INMAGE_EX(strlen(Volname)))

    do
    {
        ACE_HANDLE hDevice;
        hDevice =	ACE_INVALID_HANDLE;	

        // Open the device
        if (OpenInVirVolDriver(hDevice, &VirtualInfo->Error) != DRSTATUS_SUCCESS) {
            //success = false;
            break;
        }


        if( (!IsValidVsnapMountPoint(Volname))&&(!IsValidVsnapDeviceFile(Volname)) )
        {
            ACE_OS::close(hDevice);
            break;
        }

        unsigned int total_buf_size=0;
        INM_SAFE_ARITHMETIC(total_buf_size = InmSafeInt<size_t>::Type(sizeof(Vollen)) + Vollen + sizeof(VsnapContextInfo), INMAGE_EX(sizeof(Vollen))(Vollen)(sizeof(VsnapContextInfo)))
        buffer = (char *)calloc(total_buf_size, sizeof(char));
        if (!buffer)
        {
            Error->VsnapErrorCode = 0;
            Error->VsnapErrorStatus = VSNAP_MEM_UNAVAILABLE;
            ACE_OS::close(hDevice);
            return;
        }

        *(SV_USHORT*)buffer = (SV_USHORT) Vollen;

        inm_memcpy_s(buffer + sizeof(SV_USHORT), total_buf_size - sizeof(SV_USHORT), Volname, Vollen);

        if (ioctl(hDevice , IOCTL_INMAGE_GET_VOLUME_CONTEXT, buffer) < 0) 
        {
            DebugPrintf(SV_LOG_ERROR,"ioctl : IOCTL_INMAGE_GET_VOLUME_CONTEXT Failed on device %s with errno = %d. Error @LINE %d in FILE %s\n",
                devicename.c_str(), errno, LINE_NO, FILE_NAME);
            Error->VsnapErrorStatus = VSNAP_GET_VOLUME_INFO_FAILED;
            Error->VsnapErrorCode = ACE_OS::last_error() ; // Lack of error code
            ACE_OS::close(hDevice);
            break;
        }

		inm_memcpy_s(VsnapContext, sizeof(VsnapContextInfo), (char *)(buffer + Vollen + sizeof(Vollen)), sizeof(VsnapContextInfo));

        ACE_OS::close(hDevice);
    } while (FALSE);

    free(Volname);
    free(buffer);
}

bool UnixVsnapMgr::TraverseAndApplyLogs(VsnapSourceVolInfo *SrcInfo)
{
    VsnapVirtualVolInfo		*VirtualInfo = (VsnapVirtualVolInfo *)*(SrcInfo->VirtualVolumeList.begin());
    char					TgtVol[2048];
    char					TgtGuid[MAX_PATH];
    bool					Success = true;
    char					MapFileName[2048];
    VsnapErrorInfo			*Error = &VirtualInfo->Error;
    bool					TgtUnmounted = false;
    SVERROR					sve = SVS_OK;
    string Devicefile  ;
    bool					proceedApply = true;
    std::string filesystem;

    do
    {
        // 1. Dismount the target volume
        // 2. Traverse through the node
        //		1. For each tracking id, read the data from the logs
        //		2. Write the data to the target volume.
        // 3. Exit.

        //This FlushFileSystemBuffers function is added by sanjaya to flush the buffer before applying tracklog for the bug Id 4563
        if(!VirtualInfo->VolumeName.empty())
        {
            if(IsFileORVolumeExisting(VirtualInfo->VolumeName.c_str()))
            {
                string sourceFile;
                char volName[2048];
                inm_strcpy_s(volName,ARRAYSIZE(volName), VirtualInfo->VolumeName.c_str());
                TruncateTrailingBackSlash(volName);
                //VSNAPFC
                if(VirtualInfo->VolumeName.substr(0,5) != "/dev/")
                {	
                    if(GetdevicefilefromMountpoint(volName,sourceFile))
                        FlushFileSystemBuffers((char *)sourceFile.c_str());
                }
                else
                {
                    FlushFileSystemBuffers((char *)VirtualInfo->VolumeName.c_str());
                }
            }
            else
            {
                Error->VsnapErrorCode = ACE_OS::last_error() ;
                Error->VsnapErrorStatus = VSNAP_VIRTUAL_VOLUME_NOTEXIST;
                proceedApply = false;
                Success = false;
                break;
            }
        }
        inm_strcpy_s(TgtVol, ARRAYSIZE(TgtVol), SrcInfo->VolumeName.c_str());
        TruncateTrailingBackSlash(TgtVol);

        if(!IsFileORVolumeExisting(TgtVol))
        {
            DebugPrintf(SV_LOG_ERROR,"Invalid Target volume %s\n",TgtVol);
            Success = false;
            proceedApply = false;
            Error->VsnapErrorCode = 0;
            Error->VsnapErrorStatus = VSNAP_TARGET_OPEN_FAILED;
            break;
        }

        ACE_HANDLE Handle;
        inm_sprintf_s(MapFileName, ARRAYSIZE(MapFileName), "%s%c" ULLSPEC "_VsnapMap",VirtualInfo->PrivateDataDirectory.c_str(), ACE_DIRECTORY_SEPARATOR_CHAR, VirtualInfo->SnapShotId);


        if(!OPEN_FILE(MapFileName, O_RDONLY, 0644 | 0666 , &Handle))
        {
            Error->VsnapErrorCode = ACE_OS::last_error() ;
            Error->VsnapErrorStatus = VSNAP_MAP_INIT_FAILED;
            proceedApply = false;
            Success = false;
            break;
        }

        //1 . Get the device file corresponding to the particular mount point 

        //TruncateTrailingBackSlash(SrcInfo->VolumeName);	
        if(!GetdevicefilefromMountpointAndFileSystem(TgtVol,Devicefile, filesystem))
        {
            VirtualInfo->Error.VsnapErrorStatus = VSNAP_VOLUME_INFO_FAILED ;
            VirtualInfo->Error.VsnapErrorCode = ACE_OS::last_error() ;
            CLOSE_FILE(Handle);
            Success = false;
            proceedApply = false;
            break;
            //return false ;    
        }


        if ((Devicefile.length() <=1 ) && (SrcInfo->VolumeName.substr(0,5) == "/dev/")) //This case will come if user gives physical device and not mnt point 
            Devicefile = SrcInfo->VolumeName ;  

        SVERROR sve = SVS_OK;	
        std::string output, error;
        sve=HideDrive(Devicefile.c_str(),"", output,error,false);
        if(sve.failed())
        {
            //bug 4147
            if((error.find("CHK_DIR:") == 0)) 
            {
                Error->VsnapErrorCode = ACE_OS::last_error();
                Error->VsnapErrorStatus = VSNAP_DIR_SAME_AS_PWD;
                Error->VsnapErrorMessage = error.erase(0,9);
            }
            else
            {
                Error->VsnapErrorCode = ACE_OS::last_error();
                Error->VsnapErrorStatus = VSNAP_UNMOUNT_FAILED_FS_PRESENT;
                Error->VsnapErrorMessage = error;
            }
            Success = false;
            proceedApply = false;
            break;
            //return false;
        }	



        //3 . Open the device file to write  
        // PR#10815: Long Path support
        ACE_HANDLE HandlVolume  = ACE_OS::open(getLongPathName(Devicefile.c_str()).c_str(), O_RDWR, FILE_SHARE_READ);
        if (HandlVolume == ACE_INVALID_HANDLE) 	{
            VirtualInfo->Error.VsnapErrorStatus = VSNAP_VIRVOL_OPEN_FAILED;
            VirtualInfo->Error.VsnapErrorCode = ACE_OS::last_error() ;
            Success = false ;
            break;
        }


        if(!ParseMap(HandlVolume, Handle, VirtualInfo))
            Success = false;
        CLOSE_FILE(Handle);

    } while (FALSE);

    do
    {
        if(!proceedApply)
            break;
        DebugPrintf(SV_LOG_INFO,"Performing post apply track logs operations. Please wait....\n");
        if (SrcInfo->VolumeName.substr(0,5) != "/dev/")
        {
            int exitCode;
            std::string errorMsg;
            if(!MountDevice(Devicefile,SrcInfo->VolumeName,filesystem,false,exitCode,errorMsg))
            {
                std::ostringstream out;
                out << "mount " << Devicefile << " on " << SrcInfo->VolumeName << " failed." << std::endl;
                DebugPrintf(SV_LOG_ERROR, "%s\n", out.str().c_str());
                Success = false;
                Error->VsnapErrorMessage = out.str().c_str();
                break;
            }
        }
    } while (FALSE);
    return Success;
}


bool UnixVsnapMgr::IsValidVsnapDeviceFile(char const * Vol)
{
    bool rv = true;

    do
    {
        std::string devicename = Vol;
        std::string const pattern = "/dev/vs";	
        if((devicename.substr(0,7) != pattern)	
            || !(IsFileORVolumeExisting(devicename)))
        {
#ifdef SV_SUN
            devicename += "s2";
            if((devicename.substr(0,7) == pattern) && IsFileORVolumeExisting(devicename))
            {
                break;
            }
#endif

            rv = false;
            break;
        }
    } while(0);

    return rv;
}

bool UnixVsnapMgr::IsValidVsnapMountPoint(char const *Vol)
{
    ACE_HANDLE      VVCtrlDevice = ACE_INVALID_HANDLE;
    wchar_t         VolumeLink[256];
    int             bResult;
    SV_ULONG        BytesReturned = 0;
    string          UniqueId;
    char*           Buffer = NULL;
    SV_ULONG        ulLength = 0;
    size_t          UniqueIdLength;
    UniqueId.clear();


    VVCtrlDevice = OpenVirtualVolumeControlDevice();
    if(ACE_INVALID_HANDLE == VVCtrlDevice)
        return false;

    UniqueId = Vol;
    UniqueIdLength = UniqueId.size();
    int ByteOffset  = 0;
    INM_SAFE_ARITHMETIC(ulLength    = (SV_ULONG) sizeof(SV_USHORT) + (InmSafeInt<size_t>::Type(UniqueIdLength) + 1), INMAGE_EX(sizeof(SV_USHORT))(UniqueIdLength))
    Buffer      = (char*) calloc(ulLength , 1);

    *(SV_USHORT*)Buffer = (SV_USHORT)(UniqueIdLength + 1);
    ByteOffset += sizeof(SV_USHORT);
    inm_memcpy_s(Buffer + ByteOffset, ulLength - ByteOffset, UniqueId.c_str(), (UniqueIdLength + 1));

    if (ioctl(VVCtrlDevice , IOCTL_INMAGE_GET_VOLUME_SYMLINK_FOR_RETENTION_POINT, Buffer) < 0)
    {
        DebugPrintf(SV_LOG_ERROR, "@ LINE %d in FILE %s IOCTL_INMAGE_GET_VOLUME_SYMLINK_FOR_RETENTION_POINT failed with errno = %d for device %s\n", 
            LINE_NO, FILE_NAME, errno, UniqueId.c_str());
        free(Buffer) ; 
        close(VVCtrlDevice);
        return false;
    }

    free(Buffer);
    ACE_OS::close(VVCtrlDevice);
    return true;
}

bool UnixVsnapMgr::CreateVsnapDevice(VsnapVirtualVolInfo* VirtualInfo,VsnapMountInfo *MountData,char * Buffer,SV_ULONG& ByteOffset, SV_ULONG BufferSize)
{
    bool            rv = true;
    int             CtrlDevice = 0;
    std::string     UniqueVolumeId;
    size_t          UniqueIdLength;
    VsnapErrorInfo      *Error = &VirtualInfo->Error;
    VsnapErrorInfo* vsnaperr = &VirtualInfo->Error;
    std::string vsnapmsg;
    UniqueVolumeId.clear();
    do
    {
		std::string tgtVolume = MountData->VolumeName;
		std::string sparsefile;
        bool isnewvirtualvolume = false;
        if((!IsVolpackDriverAvailable())&& IsVolPackDevice(MountData->VolumeName,sparsefile,isnewvirtualvolume))
        {
            memset(MountData->VolumeName,0,sizeof(MountData->VolumeName));
            inm_strcpy_s(MountData->VolumeName, ARRAYSIZE(MountData->VolumeName), sparsefile.c_str());
        }
        //opening the control device
        if (OpenInVirVolDriver(CtrlDevice,&VirtualInfo->Error) != DRSTATUS_SUCCESS) {
            VsnapGetErrorMsg(VirtualInfo->Error,vsnapmsg);
            DebugPrintf(SV_LOG_ERROR,"%s : %s error code : %lu \n ",FUNCTION_NAME,vsnapmsg.c_str(),vsnaperr->VsnapErrorCode);
            rv = false;
            break;
        }

        //filling the buffer
        UniqueVolumeId = VirtualInfo->DeviceName;
        UniqueIdLength = UniqueVolumeId.size();
        ByteOffset  = 0;
        *(SV_USHORT*)Buffer = (SV_USHORT)(UniqueIdLength + 1);
        ByteOffset += sizeof(SV_USHORT);
		inm_memcpy_s(Buffer + ByteOffset, (BufferSize - ByteOffset), UniqueVolumeId.c_str(), (UniqueIdLength + 1));
        ByteOffset += (SV_ULONG)(UniqueIdLength + 1);
		inm_memcpy_s(Buffer + ByteOffset, (BufferSize - ByteOffset), MountData, sizeof(VsnapMountInfo));

        //creating necessary directory

        if(!CreateRequiredVsnapDIR())
        {
            close(CtrlDevice);
            rv = false;
            break;
        }
		DebugPrintf(SV_LOG_INFO, "target volume is %s\n", MountData->VolumeName);
        if (ioctl(CtrlDevice, IOCTL_INMAGE_ADD_VIRTUAL_VOLUME_FROM_RETENTION_LOG , Buffer) < 0)
        {
            DebugPrintf(SV_LOG_ERROR, "@ LINE %d in FILE %s IOCTL_INMAGE_ADD_VIRTUAL_VOLUME_FROM_RETENTION_LOG failed with errno = %d for device %s\n", 
                LINE_NO, FILE_NAME, errno, UniqueVolumeId.c_str());
            Error->VsnapErrorStatus = VSNAP_MOUNT_IOCTL_FAILED;
            Error->VsnapErrorCode = ACE_OS::last_error();
            close(CtrlDevice);
            rv = false;
            break;
        }
        close(CtrlDevice);
		memset(MountData->VolumeName,0,sizeof(MountData->VolumeName));
        inm_strcpy_s(MountData->VolumeName, ARRAYSIZE(MountData->VolumeName), tgtVolume.c_str());
    }while(false);
    return rv;
}

bool UnixVsnapMgr::DeleteVsnapDevice(VsnapVirtualVolInfo* VirtualInfo,const std::string & devicefile,std::string & error) 
{
    bool rv = true;
    char *Buffer = NULL;
    VsnapErrorInfo *Error = NULL;
    VsnapErrorInfo errinfo;
    errinfo.VsnapErrorCode = 0;
    errinfo.VsnapErrorStatus = VSNAP_SUCCESS;
    bool nofail = true;
    SV_ULONG ByteOffset  = 0, ulLength = 0;
    std::string                 UniqueId;
    size_t                      UniqueIdLength;
    int CtrlDevice = -1;
    do
    {
        if(VirtualInfo)
        {
            Error = &VirtualInfo->Error;
            nofail = VirtualInfo->NoFail;
        }
        else
            Error = &errinfo;

        if (OpenInVirVolDriver(CtrlDevice, &VirtualInfo->Error) != DRSTATUS_SUCCESS)
        {
            rv = false;
            error = "UnmountVirtualVolume: Unable to open the driver\n";
            break;
        }
        UniqueId = devicefile;
        UniqueIdLength = UniqueId.size();
        ByteOffset  = 0;
        INM_SAFE_ARITHMETIC(ulLength    = (SV_ULONG) sizeof(SV_USHORT) + (InmSafeInt<size_t>::Type(UniqueIdLength) + 1), INMAGE_EX(sizeof(SV_USHORT))(UniqueIdLength))
        Buffer      = (char*) calloc(ulLength , 1);

        if (!Buffer)
        {
            Error->VsnapErrorCode = 0;
            Error->VsnapErrorStatus = VSNAP_MEM_UNAVAILABLE;
            rv = false;
            error = "UnmountVirtualVolume: Memory unavailable while allocating for IOCTL_INMAGE_REMOVE_VIRTUAL_VOLUME_FROM_RETENTION_LOG ioctl. Try again later.\n";
            break;
        }

        *(SV_USHORT*)Buffer = (SV_USHORT)(UniqueIdLength + 1);
        ByteOffset += sizeof(SV_USHORT);
        inm_memcpy_s(Buffer + ByteOffset, ulLength - ByteOffset, UniqueId.c_str(), (UniqueIdLength + 1));

        if(!nofail)
        {
            if (ioctl(CtrlDevice, IOCTL_INMAGE_REMOVE_VIRTUAL_VOLUME_FROM_RETENTION_LOG, Buffer) < 0) 
            {
                DebugPrintf(SV_LOG_ERROR, "@ LINE %d in FILE %s IOCTL_INMAGE_REMOVE_VIRTUAL_VOLUME_FROM_RETENTION_LOG failed with errno = %d for device %s\n", LINE_NO, FILE_NAME, errno, devicefile.c_str());
                DebugPrintf(SV_LOG_ERROR,"The device %s is in use. Please shutdown any process accessing the device and try again.\n",
                    devicefile.c_str());
                Error->VsnapErrorStatus = VSNAP_UNMOUNT_FAILED;
                Error->VsnapErrorMessage = "Unable to delete the vsnap device ";
                Error->VsnapErrorMessage += devicefile;
                Error->VsnapErrorMessage += ". REMOVE_VIRTUAL_VOLUME_FROM_RETENTION_LOG Failed\n";
                error = Error->VsnapErrorMessage;
                Error->VsnapErrorCode = ACE_OS::last_error();// TODO: Error Code is a SV_ULONG
                rv = false;
                break;
            }
            else
            {
                DebugPrintf(SV_LOG_INFO,"Device %s removal succeeded.\n", devicefile.c_str());
            }
        }
        else
        {
            if (ioctl(CtrlDevice, IOCTL_INMAGE_REMOVE_VIRTUAL_VOLUME_FROM_RETENTION_LOG_NOFAIL, Buffer) < 0) 
            {
                DebugPrintf(SV_LOG_ERROR, "@ LINE %d in FILE %s IOCTL_INMAGE_REMOVE_VIRTUAL_VOLUME_FROM_RETENTION_LOG_NOFAIL failed with errno = %d for device %s\n", LINE_NO, FILE_NAME, errno, devicefile.c_str());
                DebugPrintf(SV_LOG_ERROR,"Could not delete the vsnap device forcefully.\n",
                    devicefile.c_str());
                Error->VsnapErrorStatus = VSNAP_UNMOUNT_FAILED;
                Error->VsnapErrorMessage = "NOFAIL deletion of vsnap failed\n ";
                Error->VsnapErrorMessage += devicefile;
                Error->VsnapErrorMessage += ". REMOVE_VIRTUAL_VOLUME_FROM_RETENTION_LOG_NOFAIL Failed\n";
                error = Error->VsnapErrorMessage;
                Error->VsnapErrorCode = ACE_OS::last_error();// TODO: Error Code is a SV_ULONG
                rv = false;
                break;
            }
            else
            {
                DebugPrintf(SV_LOG_INFO,"Device %s removal succeeded.\n", devicefile.c_str());
            }            
        }
    }while(false);
    if(CtrlDevice != -1)
        close(CtrlDevice);
    if (Buffer)
    {
        free(Buffer);
        Buffer = NULL;
    }
    return rv;
}

bool UnixVsnapMgr::StatAndWait(const std::string &device)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s with devicename = %s\n",FUNCTION_NAME, 
        device.c_str());
    std::string execdprocess = GetExecdName();
    DebugPrintf(SV_LOG_DEBUG, "@ LINE %d in FILE %s, execd process is %s\n", __LINE__, __FILE__, execdprocess.c_str());
    const std::string SERVICE = AGENT_SERVICE;
    bool bvsnapdeviceexists = false;
    bool breakout = false;
    int loopcnt = (TOTALSECS_TO_WAITFOR_VSNAP / CHECK_FREQUENCY_INSECS);
    int cnt = 0;

    do
    {
        bvsnapdeviceexists = FileExists(device);
        if (bvsnapdeviceexists)
        {
            DebugPrintf(SV_LOG_DEBUG, "@ LINE %d in FILE %s stat succeeded on vsnap device %s\n", LINE_NO, FILE_NAME,
                device.c_str());
            breakout = true;
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR, "vsnap file %s does not exists even though create vsnap ioctl succeeded. Retrying stat after %d seconds\n",
                device.c_str(), CHECK_FREQUENCY_INSECS);
            if (SERVICE == execdprocess)
            {
                sleep(CHECK_FREQUENCY_INSECS);
                cnt++;        
                if (cnt >= loopcnt)
                {
                    breakout = true;
                }
            }
            else
            {
                bool bquitrequested = CDPUtil::QuitRequested(CHECK_FREQUENCY_INSECS);
                if (bquitrequested)
                {
                    breakout = true;
                }
            }
        }
    } while (!breakout);

    if (!bvsnapdeviceexists)
    {
        DebugPrintf(SV_LOG_ERROR, "@ LINE %d in FILE %s vsnap file %s does not exists even after the wait\n", 
            LINE_NO, FILE_NAME, device.c_str());
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s with devicename = %s\n",FUNCTION_NAME, 
        device.c_str());
    return bvsnapdeviceexists;    
}
std::string FormMountCommand(const std::string filesystem, 
                             const std::string device, 
                             const std::string mountpoint)
{
    std::string mountcommand = MOUNT_COMMAND;  
    mountcommand += " ";
    mountcommand += OPTION_TO_SPECIFY_FILESYSTEM;
    mountcommand += " ";
    mountcommand += filesystem;
    mountcommand += " ";
    mountcommand += "\"";
    mountcommand += device;
    mountcommand += "\"";
    mountcommand += " ";
    mountcommand += "\"";
    mountcommand += mountpoint;
    mountcommand += "\"";

    return mountcommand;
}
