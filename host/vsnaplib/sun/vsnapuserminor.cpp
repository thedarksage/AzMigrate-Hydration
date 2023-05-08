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
#include <algorithm>
#include <functional>
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
#include <sys/mount.h>

#include <sys/mnttab.h>
#include <sys/dklabel.h>

#include "logger.h"
#include "VsnapShared.h"
#include "localconfigurator.h"
#include "executecommand.h"  
#include <sys/sysinfo.h>
#include "configwrapper.h"
#include "inmcommand.h"
#include "svtypes.h"
#include "vsnapuserminor.h"
#include <alloca.h>
#include <ctime>
#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>

#include "inmsafeint.h"
#include "inmageex.h"

#define SV_PROC_MNTS    "/etc/mnttab"
#define VSNAP_USER_MSG_SIZE 100

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
    FILE *fp;
    struct mnttab *entry = NULL;
	char buffer[MAXPATHLEN*4];
    fp = fopen(SV_PROC_MNTS, "r");

    if (fp == NULL)
    {
        DebugPrintf(SV_LOG_INFO,"Unable to open the %s ...\n", SV_PROC_MNTS);
        return false ;
    }

    resetmnttab(fp);
    entry = (struct mnttab *) malloc(sizeof (struct mnttab));
   while( SVgetmntent(fp, entry,buffer,sizeof(buffer)) != -1  )
    {
        if (!IsValidMntEnt(entry))
            continue;

        if (!IsValidDevfile(entry->mnt_special))
            continue;

        if(strcmp(entry->mnt_mountp ,MntPnt ) == 0 )
        {
            DeviceFile+= entry->mnt_special ;
            resetmnttab(fp);
            fclose(fp);
            return true;
        }

    }

    if ( fp )
    {
        free (entry);
        resetmnttab(fp);
        fclose(fp);
    }

    return true ;
}

void UnixVsnapMgr:: process_proc_mounts(std::vector<volinfo> &fslist)
{
    FILE *fp;
    struct mnttab *entry = NULL;
	char buffer[MAXPATHLEN*4];
    fp = fopen(SV_PROC_MNTS, "r");

    if (fp == NULL)
    {
        DebugPrintf("Could not able to open %s \n", SV_PROC_MNTS);
        return;
    }

    resetmnttab (fp);
    entry = (struct mnttab *) malloc(sizeof (struct mnttab));
     while( SVgetmntent(fp, entry,buffer,sizeof(buffer)) != -1  )
    {
        volinfo vol;

        if (!IsValidMntEnt(entry))
            continue;

        if (!IsValidDevfile(entry->mnt_special))
            continue;

        vol.devname = entry->mnt_special;
        vol.mountpoint = entry->mnt_mountp;
        vol.fstype = entry->mnt_fstype;
        vol.devno = GetDevNumber(vol.devname);
        vol.mounted = true;

        fslist.push_back(vol);
    }
    if ( fp )
    {
        free (entry);
        resetmnttab(fp);
        fclose(fp);
    }
    return ; //False ;
}

bool UnixVsnapMgr::CreateRequiredVsnapDIR()
{
    bool rv = true;
    do
    {
        std::string dir = VSNAP_DSK_DIR;
        SVERROR rc = SVMakeSureDirectoryPathExists(dir.c_str());
        if (rc.failed())
        {
            DebugPrintf(SV_LOG_ERROR, "FAILED  SVMakeSureDirectoryPathExists %s :  %s \n",dir.c_str(),rc.toString());
            rv = false;
            break;
        }
        dir.clear();
        dir = VSNAP_RDSK_DIR;
        rc = SVMakeSureDirectoryPathExists(dir.c_str());
        if (rc.failed())
        {
            DebugPrintf(SV_LOG_ERROR, "FAILED  SVMakeSureDirectoryPathExists %s :  %s \n",dir.c_str(),rc.toString());
            rv = false;
            break;
        }
    }while(false);
    return rv;
}

bool createLinkToDevice(VsnapVirtualVolInfo *VirtualInfo, const std::string & devicename,const std::string & linkname)
{
    bool rv = true;
    VsnapErrorInfo	*Error = &VirtualInfo->Error;
    int dsksymlinkretval;
    int rdsksymlinkretval;
    const std::string vsnaprawdevicenamespacesuffix = ",raw";
    std::string rdsklinkname = GetRawVolumeName(linkname);
    std::string rdskpartition = devicename + vsnaprawdevicenamespacesuffix;
    if (LinkExists(linkname))
    {
        DebugPrintf(SV_LOG_DEBUG, "@ LINE %d in FILE %s unlinking existing link %s\n", LINE_NO, FILE_NAME, linkname.c_str()); 
        unlink(linkname.c_str());
    }

    if (LinkExists(rdsklinkname))
    {
        DebugPrintf(SV_LOG_DEBUG, "@ LINE %d in FILE %s unlinking existing link %s\n", LINE_NO, FILE_NAME, rdsklinkname.c_str()); 
        unlink(rdsklinkname.c_str());
    }
	DebugPrintf(SV_LOG_DEBUG,"Creating symlink for block device %s\n", linkname.c_str());
    dsksymlinkretval = symlink(devicename.c_str(), linkname.c_str());
    if (dsksymlinkretval)
    {
        /* symlink fails */
        DebugPrintf(SV_LOG_ERROR, "@ LINE %d in FILE %s creating link %s to file %s failed with errno = %d\n", LINE_NO, 
            FILE_NAME, linkname.c_str(), devicename.c_str(), errno);
        Error->VsnapErrorCode = ACE_OS::last_error();
        Error->VsnapErrorStatus = VSANP_NOTBLOCKDEVICE_FAILED;
        return false;
    }
	DebugPrintf(SV_LOG_DEBUG,"Creating symlink for char device %s\n", rdsklinkname.c_str());
    rdsksymlinkretval = symlink(rdskpartition.c_str(), rdsklinkname.c_str());

    if (rdsksymlinkretval)
    {
        DebugPrintf(SV_LOG_ERROR, "@ LINE %d in FILE %s creating link %s to file %s failed with errno = %d\n", LINE_NO, 
            FILE_NAME, rdsklinkname.c_str(), rdskpartition.c_str(), errno);
        unlink(linkname.c_str());
        Error->VsnapErrorCode = ACE_OS::last_error();
        Error->VsnapErrorStatus = VSANP_NOTBLOCKDEVICE_FAILED;
        return false;
    }
    return rv;
}
bool UnixVsnapMgr::MountVol(VsnapVirtualVolInfo *VirtualInfo, VsnapMountInfo *MountData, char* SnapshotDrive,bool &FsmountFailed)
{
    /* place holder function */
    return true;
}


bool UnixVsnapMgr::UnmountVirtualVolume(char* SnapshotDrive, VsnapVirtualVolInfo* VirtualInfo, std::string& output, std::string& error,bool bypassdriver)
{
    /* Place holder function */
    return true;
}


bool GetdevicefilefromMountpointAndFileSystem(const char* MntPnt,
                                              std::string& DeviceFile,
                                              std::string &filesystem)
{
    FILE *fp;
    struct mnttab *entry = NULL;
	char buffer[MAXPATHLEN*4];
    fp = fopen(SV_PROC_MNTS, "r");

    if (fp == NULL)
    {
        DebugPrintf(SV_LOG_INFO,"Unable to open the %s ...\n", SV_PROC_MNTS);
        return false ;
    }

    resetmnttab(fp);
    entry = (struct mnttab *) malloc(sizeof (struct mnttab));
     while( SVgetmntent(fp, entry,buffer,sizeof(buffer)) != -1  )
    {
        if (!IsValidMntEnt(entry))
            continue;

        if (!IsValidDevfile(entry->mnt_special))
            continue;

        if(strcmp(entry->mnt_mountp ,MntPnt ) == 0 )
        {
            DeviceFile+= entry->mnt_special ;
            filesystem = entry->mnt_fstype ;
            resetmnttab(fp);
            fclose(fp);
            return true;
        }

    }

    if ( fp )
    {
        free (entry);
        resetmnttab(fp);
        fclose(fp);
    }

    return true ;
}


bool UnixVsnapMgr::NeedToRecreateVsnap(const std::string &vsnapdevice)
{
    bool bdeviceexists = LinkExists(vsnapdevice);
    if(!bdeviceexists)
    {
        std::string devicename = vsnapdevice + "s2";
        bdeviceexists = LinkExists(devicename);
    }
    return !bdeviceexists;
}


bool UnixVsnapMgr::PerformZpoolDestroyIfReq(const std::string &devicefile, bool bshoulddeletevsnaplogs)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s with vsnap name = %s\n",FUNCTION_NAME, devicefile.c_str());
    ZpoolsWithStorageDevices_t zpoolswithstoragedevices;
    bool bretval = true;

    if (GetZpoolsWithStorageDevices(zpoolswithstoragedevices))
    {
        PrintZpoolsWithStorageDevices(zpoolswithstoragedevices);
        ConstZpoolsWithStorageDevicesIter_t iter = find_if(zpoolswithstoragedevices.begin(), zpoolswithstoragedevices.end(), Eq_vsnap(devicefile));

        if (zpoolswithstoragedevices.end() != iter)
        {
            DebugPrintf(SV_LOG_INFO, "vsnap name %s is in zpool name %s. %s this zpool\n", 
                devicefile.c_str(), iter->first.c_str(), bshoulddeletevsnaplogs?"destroying":"exporting");
            std::string errormsg;
            int errcode = 0; 
            std::string zpooldestroycommand = ZPOOL_EXPORT;

            if (bshoulddeletevsnaplogs)
            {
                zpooldestroycommand = ZPOOL_DESTROY_FORCE;        
            }

            zpooldestroycommand += " ";
            zpooldestroycommand += iter->first;

            bool brvalofzpooldestroy = ExecuteInmCommand(zpooldestroycommand, errormsg, errcode);

            if (brvalofzpooldestroy)
            {
                DebugPrintf(SV_LOG_INFO, "For vsnap %s and zpool %s, zpool %s succeeded\n", devicefile.c_str(), iter->first.c_str(), 
                    bshoulddeletevsnaplogs?"destroy":"export");
            }
            else
            {
                if (!bshoulddeletevsnaplogs)
                {
                    DebugPrintf(SV_LOG_INFO, 
                        "For vsnap %s and zpool %s, zpool %s command failed with error code = %d, error message = %s\n",
                        devicefile.c_str(), iter->first.c_str(), bshoulddeletevsnaplogs?"destroy":"export", errcode, errormsg.c_str()); 
                    DebugPrintf(SV_LOG_INFO, "Retrying with force option of zpool export\n");
                    zpooldestroycommand = ZPOOL_EXPORT_FORCE;
                    zpooldestroycommand += " ";
                    zpooldestroycommand += iter->first;

                    brvalofzpooldestroy = ExecuteInmCommand(zpooldestroycommand, errormsg, errcode); 
                }
            }
            bretval = brvalofzpooldestroy;

            if (!bretval)
            {
                DebugPrintf(SV_LOG_ERROR, 
                    "For vsnap %s and zpool %s, zpool %s command failed with error code = %d, error message = %s\n",
                    devicefile.c_str(), iter->first.c_str(), bshoulddeletevsnaplogs?"destroy":"export", errcode, errormsg.c_str()); 
            }
        }
        else
        {
            DebugPrintf(SV_LOG_DEBUG, "vsnap name %s is not in any zpool\n", devicefile.c_str());
        }
    }
    else
    {
        DebugPrintf(SV_LOG_ERROR, "Unable to delete vsnap %s since not able to get to which zpool it belongs, as could not get zpools\n", devicefile.c_str());
        bretval = false;
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s with vsnap name = %s, retval = %s\n",FUNCTION_NAME, devicefile.c_str(), bretval?"true":"false");
    return bretval;
}


bool UnixVsnapMgr::OpenLinvsnapSyncDev(const std::string &device, int &fd)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s with vsnap = %s\n",FUNCTION_NAME, 
        device.c_str());
    std::string execdprocess = GetExecdName();
    DebugPrintf(SV_LOG_DEBUG, "@ LINE %d in FILE %s, execd process is %s\n", __LINE__, __FILE__, execdprocess.c_str());
    const std::string SERVICE = AGENT_SERVICE;
    bool breakout = false;
    int loopcnt = (TOTALSECS_TO_WAITFOR_SYNCDEV / CHECK_FREQ_FOR_SYNCDEV);
    int cnt = 0;
    const std::string LINVSNAP_SYNC_DEV = LINVSNAP_SYNC_DRV;
    bool bretval = false;

    //Step1: open linvsnap sync
    do
    {
        fd = open(LINVSNAP_SYNC_DEV.c_str(), O_RDONLY);
        if (-1 != fd)
        {
            DebugPrintf(SV_LOG_DEBUG, "Successfully opened linvsnap sync device %s for vsnap %s\n", LINVSNAP_SYNC_DEV.c_str(), 
                device.c_str());
            bretval = true;
            breakout = true;
        }
        else
        {
            DebugPrintf(SV_LOG_INFO, "Opening linvsnap sync device %s failed for vsnap %s with errno = %d."
                "Retrying to open it after waiting for %d seconds\n", 
                LINVSNAP_SYNC_DEV.c_str(), 
                device.c_str(), errno, CHECK_FREQ_FOR_SYNCDEV);  

            /**
            *  
            *  Although code is added for service, service 
            *  should never ever delete a vsnap since the
            *  wait is indefinite
            *  
            **/

            if (SERVICE == execdprocess)
            {
                sleep(CHECK_FREQ_FOR_SYNCDEV);
                cnt++;
                if (cnt >= loopcnt)
                {
                    breakout = true;
                }
            }
            else
            {
                bool bquitrequested = CDPUtil::QuitRequested(CHECK_FREQ_FOR_SYNCDEV);
                if (bquitrequested)
                {
                    breakout = true;
                }
            }

        }
    } while (!breakout);

    if (!bretval)
    {
        DebugPrintf(SV_LOG_ERROR, "Opening linvsnap sync device %s failed for vsnap %s.\n",
            LINVSNAP_SYNC_DEV.c_str(), device.c_str());
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s with devicename = %s\n",FUNCTION_NAME, 
        device.c_str());
    return bretval;
}


bool UnixVsnapMgr::CloseLinvsnapSyncDev(const std::string &device, int &fd)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s with vsnap = %s\n",FUNCTION_NAME, 
        device.c_str());
    bool bretval = true;
    const std::string LINVSNAP_SYNC_DEV = LINVSNAP_SYNC_DRV;

    if (0 != close(fd))
    {
        DebugPrintf(SV_LOG_ERROR, "Closing linvsnap sync device %s failed for vsnap %s with errno = %d\n", 
            LINVSNAP_SYNC_DEV.c_str(), device.c_str(), errno);
        bretval = false;
    }
    else
    {
        DebugPrintf(SV_LOG_DEBUG, "Closed linvsnap sync device %s for vsnap %s\n", 
            LINVSNAP_SYNC_DEV.c_str(), device.c_str());
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s with devicename = %s\n",FUNCTION_NAME, 
        device.c_str());
    return bretval;
}


bool UnixVsnapMgr::RetryZpoolDestroyIfReq(const std::string &device, const std::string &target, bool bshoulddeletevsnaplogs)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s with vsnap = %s\n",FUNCTION_NAME,
        device.c_str());
    std::string execdprocess = GetExecdName();
    DebugPrintf(SV_LOG_DEBUG, "@ LINE %d in FILE %s, execd process is %s\n", __LINE__, __FILE__, execdprocess.c_str());
    const std::string SERVICE = AGENT_SERVICE;
    bool bzpoolexportstatus = false;
    bool breakout = false;
    int loopcnt = (TOTALSECS_TO_WAITFOR_ZPOOL / CHECK_FREQ_FOR_ZPOOL);
    int cnt = 0;
    const std::string ZFS_NAME = "zfs";

    std::string formattedVolumeName = target;
    CDPUtil::trim(formattedVolumeName);
    FormatVolumeName(formattedVolumeName);
    if (IsReportingRealNameToCx())
    {
        GetDeviceNameFromSymLink(formattedVolumeName);
    }

    std::string filesystem = getFSType(formattedVolumeName);


    if (ZFS_NAME == filesystem)
    {
        /**
        * 
        *  This loop is not infinite though
        * 
        **/

        do
        {
            bzpoolexportstatus = PerformZpoolDestroyIfReq(device, bshoulddeletevsnaplogs);
            if (bzpoolexportstatus)
            {
                DebugPrintf(SV_LOG_DEBUG, "@ LINE %d in FILE %s PerformZpoolDestroyIfReq succeeded on vsnap device %s\n", LINE_NO, FILE_NAME,
                    device.c_str());
                break;
            }
            else
            {
                DebugPrintf(SV_LOG_ERROR, "zpool destroy or export has failed for vsnap %s. Retrying it after waiting for %d seconds\n",
                    device.c_str(), CHECK_FREQ_FOR_ZPOOL);
                if (SERVICE == execdprocess)
                {
                    sleep(CHECK_FREQ_FOR_ZPOOL);
                }
                else
                {
                    bool bquitrequested = CDPUtil::QuitRequested(CHECK_FREQ_FOR_ZPOOL);
                    if (bquitrequested)
                    {
                        break;
                    }
                }
            }
            ++cnt;
        } while (cnt < loopcnt);
    }
    else
    {
        DebugPrintf(SV_LOG_DEBUG, "For vsnap device %s, file system is not zfs from CX, hence not forking any zpool command\n",
            device.c_str());
        bzpoolexportstatus = true;
    }

    if (!bzpoolexportstatus)
    {
        DebugPrintf(SV_LOG_ERROR, "For vsnap %s zpool export or destroy failed even after wait\n",
            device.c_str());
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s with devicename = %s\n",FUNCTION_NAME,
        device.c_str());
    return bzpoolexportstatus;
}

bool UnixVsnapMgr::StatAndMount(const VsnapPersistInfo& remountInfo)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s with devicename = %s\n",FUNCTION_NAME, 
        remountInfo.devicename.c_str());
    bool bretval = false;
    std::string mountpoint = remountInfo.mountpoint;
    std::string vsnapdevice = remountInfo.devicename;

    if (!mountpoint.empty())
    {
        std::string mounteddevice;
        if (GetdevicefilefromMountpoint(mountpoint.c_str(), mounteddevice))
        {
            if (vsnapdevice != mounteddevice)
            {
                std::string formattedVolumeName = remountInfo.target;
                CDPUtil::trim(formattedVolumeName);
                FormatVolumeName(formattedVolumeName);
                if (IsReportingRealNameToCx())
                {
                    GetDeviceNameFromSymLink(formattedVolumeName);
                }
                std::string fsType = getFSType(formattedVolumeName);

                if (!fsType.empty())
                {
                    //stat the device indefinitely if not service, If service, wait for some time and bail out
                    if (StatAndWait(remountInfo.devicename))
                    {
                        bool breadonly = (remountInfo.accesstype == 0);
                        int exitCode = 0;
                        std::string errorMsg = "";

                        bretval = MountDevice(remountInfo.devicename, mountpoint, fsType, breadonly, exitCode, errorMsg);
                        if (bretval)
                        {
                            DebugPrintf(SV_LOG_DEBUG, "@ LINE %d in FILE %s, vsnap device %s mounted %s successfully on mountpoint %s with file system %s\n",
                                __LINE__, __FILE__, remountInfo.devicename.c_str(), breadonly?"readonly":"readwrite", mountpoint.c_str(), fsType.c_str());
                        }
                        else
                        {
                            DebugPrintf(SV_LOG_ERROR, "@ LINE %d in FILE %s, vsnap device %s failed to mount %s on mountpoint %s with file system %s"
                                "with exitcode = %d, errormsg = %s\n", __LINE__, __FILE__, remountInfo.devicename.c_str(), 
                                breadonly?"readonly":"readwrite", mountpoint.c_str(), fsType.c_str(), exitCode, errorMsg.c_str()); 
                        }
                    }
                }
                else
                {
                    //Log that mount will not happen since source is not having a filesystem
                    DebugPrintf(SV_LOG_ERROR,"@ LINE %d in FILE %s, for vsnap device %s, mount will not happen since source file system is not present\n", 
                        __LINE__, __FILE__, remountInfo.devicename.c_str());
                }
            }
            else
            {
                bretval = true;
                DebugPrintf(SV_LOG_DEBUG,"@ LINE %d in FILE %s, vsnap device %s, it is already mounted\n", 
                    __LINE__, __FILE__, remountInfo.devicename.c_str());
            }
        }
        else
        {
            bretval = false;
            DebugPrintf(SV_LOG_ERROR,"@ LINE %d in FILE %s, vsnap device %s, failed to read from /etc/vfstab\n", 
                __LINE__, __FILE__, remountInfo.devicename.c_str());
        }
    }
    else
    {
        DebugPrintf(SV_LOG_DEBUG,"@ LINE %d in FILE %s, for vsnap device %s, mountpoint is empty from persistent store\n", 
            __LINE__, __FILE__, remountInfo.devicename.c_str());
        bretval = true;
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s with devicename = %s\n",FUNCTION_NAME, 
        remountInfo.devicename.c_str());
    return bretval; 
}

void UnixVsnapMgr::CleanUpIncompleteVsnap(VsnapVirtualVolInfo* VirtualInfo)
{
    return;
}

bool UnixVsnapMgr::MountVol_Platform_Specific(VsnapVirtualVolInfo *VirtualInfo, int minornumber, sv_dev_t MkdevNumber)
{
 return true;
}

bool UnixVsnapMgr::Unmount_Platform_Specific(const std::string & devicefile)
{
	return true;
}
