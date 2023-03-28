#include <stdio.h>
#include <iostream>
#include <mntent.h>
#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <unistd.h>
#include <netinet/in.h>

#include <iostream>
#include <vector>
#include <set>
#include <string>
#include <algorithm>

#include "ace/High_Res_Timer.h"
#include "ace/os_include/os_fcntl.h"
#include "ace/OS.h"
#include "ace/Process_Manager.h"

#include "vacpunix.h"
#include "volumegroupsettings.h"
#include "portablehelpers.h"
#include "volumeinfocollector.h"
#include "StreamEngine.h"
#include "volumereporter.h"
#include "volumereporterimp.h"
#include "datacacher.h"

#include "devicefilter.h"

#ifdef VXFSVACP
#include "sys/fs/vx_ioctl.h"
#endif

using namespace std;

#define COMMAND_DMSETUP                 "dmsetup"
#define OPTION_SUSPEND                  "suspend"
#define OPTION_RESUME                   "resume"

#define FS_FREEZE_TIMEOUT   60000 // in ms

#define VACP_IOBARRIER_MAX_RETRY_TIME  500 // in ms
#define VACP_IOBARRIER_RETRY_WAIT_TIME 100 // in ms

#define MAX_VOLUME_CACHE_READ_RETRIES   10
#define MAX_VOLUME_CACHE_READ_WAIT_TIME 10 // in sec

extern void inm_printf(const char* format, ... );

// In case of Linux hosts, the freeze/thaw of filesystems are done in the kernel through driver
// however if the involflt driver  is not present on the host, as with certain solutions,
// the remote vacp is used. gbRemoteSend indicates if this is a remote vacp invocation.
// for remote vap the filesystems are freezed/thawed in user mode. And freeze/thaw works only
// with dm devices.
// irrespective of local or remote vacp, vxfs filesystems should be frozen in user mode.
extern bool gbRemoteSend;

// extern bool g_bDistributedVacp;

extern bool DriverSupportsUserModeFreeze();
extern bool DriverSupportsIoBarrier();

int gDriverFd = -1;

bool fsVolSortPredicate(const std::string &l, const std::string &r)
{
    return l>=r;
}

//
// Function: GetFSTagSupportedVolumes
//   Gets the list of volumes on which file system consistency tag can be issued
//   Currently, we can issue filesystem consistency tag only if the
//   volume is mounted and the filesystem is a journaled filesystem
//
//   TODO: we are not verifying the filesystem for now

bool GetFSTagSupportedVolumes(std::set<std::string> & supportedFsVolumes)
{
    bool rv  = true;

    FILE *fp;
    struct mntent *entry = NULL;
    mntent ent;
    char buffer[MAXPATHLEN*4];
    fp = setmntent("/etc/mtab", "r");

    if (fp == NULL)
    {
        inm_printf("unable to open /etc/mtab \n");
        return false;
    }

    inm_printf("Filesystem consistency is supported on ext3, ext4, gfs2, reiser3 and reiser4 filesystems.\n");

    while( (entry = getmntent_r(fp, &ent, buffer, sizeof(buffer))) != NULL )
    {
        std::string devname = entry -> mnt_fsname;

        if (!IsValidDevfile(entry->mnt_fsname))
        {
            continue;
        }

        if(!GetDeviceNameFromSymLink(devname))
        {
            continue;
        }

        std::string fsType = entry->mnt_type;

        if ((strcmp(entry->mnt_type, "ext3") == 0) || 
            (strcmp(entry->mnt_type, "ext4") == 0) || 
            (strcmp(entry->mnt_type, "gfs2") == 0 ) ||
            (strcmp(entry->mnt_type, "reiser3") == 0) ||
            (strcmp(entry->mnt_type, "reiser4") == 0) ||
            (strcmp(entry->mnt_type, "xfs") == 0))
        {
            supportedFsVolumes.insert(devname);
        }

#ifdef VXFSVACP
        if ((strcmp(entry->mnt_type, "vxfs") == 0))
        {
            supportedFsVolumes.insert(devname);
        }
#endif
    }

    if ( NULL != fp )
    {
        endmntent(fp);
    }

    return rv;
}

bool IsRootPartition(const std::string & partitionName)
{
    bool  rv  = false;

    FILE *fp;
    struct mntent *entry = NULL;
    mntent ent;
    char buffer[MAXPATHLEN*4];
    fp = setmntent("/etc/mtab", "r");

    if (fp == NULL)
    {
        inm_printf("unable to open /etc/mtab \n");
        return false;
    }

     while( (entry = getmntent_r(fp, &ent, buffer, sizeof(buffer))) != NULL )
    {
        std::string devName = entry -> mnt_fsname;
        std::string mntDir = entry -> mnt_dir;
        if(partitionName == devName && (mntDir == "/"))
        {
            rv = true;
            break;
        }
    }

    if ( NULL != fp )
    {
        endmntent(fp);
    }

    return rv;
}

bool GetMountPointsForVolumes(const std::set<std::string> & fsVolumes, t_fsVolInfoMap &fsVolInfoMap, std::set<std::string> & mntDirs)
{
    bool rv  = true;

    FILE *fp;
    struct mntent *entry = NULL;
    mntent ent;
    char buffer[MAXPATHLEN*4];
    fp = setmntent("/etc/mtab", "r");

    if (fp == NULL)
    {
        inm_printf("unable to open /etc/mtab \n");
        return false;
    }

    while( (entry = getmntent_r(fp, &ent, buffer, sizeof(buffer))) != NULL )
    {
        std::string devname = entry -> mnt_fsname;

        if (!IsValidDevfile(entry->mnt_fsname))
        {
            continue;
        }


        if(!GetDeviceNameFromSymLink(devname))
        {
            continue;
        }

        if(fsVolumes.find(devname) != fsVolumes.end())
        {
            // select only one mount point for a device mounted multiple times
            bool bMultiMountDevice = false;
            std::map<std::string, struct FsVolInfo>::iterator iter = fsVolInfoMap.begin();
            for (/*empty*/; iter != fsVolInfoMap.end() ; iter++)
            {
                if (iter->second.volName == devname)
                {
                    bMultiMountDevice = true;
                    inm_printf("found multiple mount points for device %s prev %s curr %s.\n", 
                    devname.c_str(),
                    iter->second.mntDir.c_str(),
                    entry->mnt_dir);
                    break;
                }
            }

            if (bMultiMountDevice)
                continue;

            std::string fsType = entry->mnt_type;
            if ((strcmp(entry->mnt_type, "ext3") == 0) || 
                (strcmp(entry->mnt_type, "ext4") == 0) || 
                (strcmp(entry->mnt_type, "gfs2") == 0 ) ||
                (strcmp(entry->mnt_type, "reiser3") == 0) ||
                (strcmp(entry->mnt_type, "reiser4") == 0) ||
                (strcmp(entry->mnt_type, "xfs") == 0))
            {
                mntDirs.insert(entry->mnt_dir);
                fsVolInfoMap.insert(pair<std::string, struct FsVolInfo>(entry->mnt_dir,FsVolInfo(devname, entry->mnt_type, entry->mnt_dir))); 
            }

#ifdef VXFSVACP
            if ((strcmp(entry->mnt_type, "vxfs") == 0))
            {
                mntDirs.insert(entry->mnt_dir);
                fsVolInfoMap.insert(pair<std::string, struct FsVolInfo>(entry->mnt_dir,FsVolInfo(devname, entry->mnt_type, entry->mnt_dir))); 
            }
#endif
        }
    }

    if ( NULL != fp )
    {
        endmntent(fp);
    }

    return rv;
}

/*
bool GetVolumesWithoutMountPoints(std::set<std::string> & volumes)
{
    std::set<std::string>::iterator iterVolumes;
    std::map<std::string, struct FsVolInfo >::iterator iterVolMap = fsVolInfoMap.begin();
    while (iterVolMap != fsVolInfoMap.end())
    {
        struct FsVolInfo & fsVolInfo = iterVolMap->second;
        
        iterVolumes = volumes.find(fsVolInfo.volName); 
        if ( iterVolumes != volumes.end())
            volumes.erase(iterVolumes);
        iterVolMap++;
    }

    return true;
}
*/

bool lockfs(const struct FsVolInfo &fsVolInfo, const std::string strContextUuid)
{
    bool rv = true;

    do 
    {
        if (!DriverSupportsUserModeFreeze())
        {
            break;
        }

        if (fsVolInfo.volName.length() > 127)
        {
            rv = false;
            break;
        }

        int fdDriver = open(INMAGE_FILTER_DEVICE_NAME, O_RDWR );

        if (fdDriver < 0) 
        {
            inm_printf( "Function %s @LINE %d in FILE %s :unable to open %s with error %d\n",
                FUNCTION_NAME, LINE_NO, FILE_NAME, INMAGE_FILTER_DEVICE_NAME, errno);
            rv = false;
            break;
        }

        volume_info_t volInfo;
        freeze_info_t freezeInfo;
        freezeInfo.nr_vols = 1;
        freezeInfo.timeout = FS_FREEZE_TIMEOUT;
        freezeInfo.vol_info = &volInfo;
        volInfo.flags = 0;
        volInfo.status = 0;
        inm_strcpy_s(volInfo.vol_name, TAG_VOLUME_MAX_LENGTH, fsVolInfo.volName.c_str());
        inm_memcpy_s(freezeInfo.tag_guid, sizeof(freezeInfo.tag_guid), strContextUuid.c_str(), GUID_LEN);
 
        if(ioctl(fdDriver, IOCTL_INMAGE_FREEZE_VOLUME, &freezeInfo) < 0)
        {
            inm_printf("ioctl failed: freeze failed for %s at %s. errno %d\n", fsVolInfo.volName.c_str(), fsVolInfo.mntDir.c_str(), errno);
            rv = false;

            if (volInfo.status)
                inm_printf("Error freeze failed for %s at %s with status %d\n", volInfo.vol_name, fsVolInfo.mntDir.c_str(), volInfo.status);
        }
        else
        {
            inm_printf("Freeze ioctl successful for %s at %s.\n", volInfo.vol_name, fsVolInfo.mntDir.c_str());
            rv = true;
        }

        close(fdDriver);

    } while (false);

    return rv;
}

bool unlockfs(const struct FsVolInfo &fsVolInfo, const std::string strContextUuid)
{
    bool rv = true;

    do 
    {
        if (!DriverSupportsUserModeFreeze())
            break;

        int fdDriver = open(INMAGE_FILTER_DEVICE_NAME, O_RDWR );

        if (fdDriver < 0) 
        {
            inm_printf( "Function %s @LINE %d in FILE %s :unable to open %s with error %d\n",
                FUNCTION_NAME, LINE_NO, FILE_NAME, INMAGE_FILTER_DEVICE_NAME, errno);
            rv = false;
            break;
        }
        
        volume_info_t volInfo;
        thaw_info_t thawInfo;
        thawInfo.nr_vols = 1;
        thawInfo.vol_info = &volInfo;
        volInfo.flags = 0;
        volInfo.status = 0;
        inm_strcpy_s(volInfo.vol_name, TAG_VOLUME_MAX_LENGTH, fsVolInfo.volName.c_str());
        inm_memcpy_s(thawInfo.tag_guid, sizeof(thawInfo.tag_guid), strContextUuid.c_str(), GUID_LEN);

        if(ioctl(fdDriver, IOCTL_INMAGE_THAW_VOLUME, &thawInfo) < 0)
        {
            inm_printf("ioctl failed: unfreeze failed for %s at %s. errno %d\n", fsVolInfo.volName.c_str(), fsVolInfo.mntDir.c_str(), errno);
            rv = false;

            if (volInfo.status)
                inm_printf("Error unfreeze failed for %s at %s with status %d\n", volInfo.vol_name, fsVolInfo.mntDir.c_str(), volInfo.status);
        }
        else
        {
            inm_printf("Unfreeze ioctl successful for %s at %s.\n", volInfo.vol_name, fsVolInfo.mntDir.c_str());
        }

        close(fdDriver);

    } while (false);

    return rv;
}

#ifdef VXFSVACP
bool lockvxfs(struct FsVolInfo &fsVolInfo)
{
    int fd = open(fsVolInfo.mntDir.c_str(), O_RDONLY);
    if (fd < 0) 
    {
        inm_printf("Failed to open : %s\n", fsVolInfo.mntDir.c_str());
        return false;
    }
    else
    {
        fsVolInfo.fd = fd;

        if (ioctl(fd, VX_FREEZE, FS_FREEZE_TIMEOUT))
        {
            inm_printf("Failed to freeze filesystem: %s\n", fsVolInfo.mntDir.c_str());
            return false;
        }
        else
        {
            inm_printf("Filesystem freeze successful on: %s\n", fsVolInfo.mntDir.c_str());
            inm_printf("Successfully flushed I/O to : %s\n", fsVolInfo.mntDir.c_str());
        }
    }
    return true;
}

bool unlockvxfs(const struct FsVolInfo &fsVolInfo)
{
    bool ret = true;
    if (fsVolInfo.fd > 0)
    {
        if (ioctl(fsVolInfo.fd, VX_THAW, NULL)) 
        {
            if (errno != ETIMEDOUT)
            {
                inm_printf("Failed to resume filesystem: %s with error %d\n", fsVolInfo.mntDir.c_str(), errno);
                ret = false;
            }
            else
            {
                inm_printf("I/O resumed on filesystem: %s after timeout\n", fsVolInfo.mntDir.c_str());
                ret = false;
            }
        }
        else
        {
            inm_printf("Successfully resumed I/O on: %s\n", fsVolInfo.mntDir.c_str());
        }

        close(fsVolInfo.fd);
    }

    return ret;
}
#endif

bool AppConsistency::FreezeDevices(const set<string> &inputVolumes)
{
    bool ret = true;
    ACE_Process_Manager* pm = ACE_Process_Manager::instance();
    if (pm == NULL) 
    {
        // need to continue with other devices even if there was a problem generating an ACE process
        inm_printf("FAILED to Get ACE Process Manager instance\n");
        ret = false;
    }
    else
    {
        ACE_Process_Options options;
        options.handle_inheritance(false);

        inm_printf("\nStarting device I/O suspension...\n");

        m_fsVolInfoMap.clear();
        if (!GetMountPointsForVolumes(inputVolumes, m_fsVolInfoMap, m_volMntDirs))
            return false;
        
        //First copy m_volMntDirs set into a vector
        std::vector<string> vecvolMntDirs;
        std::set<string>::const_iterator iter = m_volMntDirs.begin();
        while(iter != m_volMntDirs.end())
        {
            vecvolMntDirs.push_back((*iter).c_str());
            iter++;
        }

        //sort the vector in descending order to freeze the inner most nested directoies first
        std::sort(vecvolMntDirs.begin(),vecvolMntDirs.end(),fsVolSortPredicate);

        std::vector<string>::const_iterator mntDirIter = vecvolMntDirs.begin();
        for(; mntDirIter!= vecvolMntDirs.end(); mntDirIter++)
        {
            std::map<std::string, struct FsVolInfo >::iterator iter = m_fsVolInfoMap.find(mntDirIter->c_str());
            if(iter != m_fsVolInfoMap.end())
            {
                struct FsVolInfo & fsVolInfo= iter->second;
                if ((strcmp(fsVolInfo.fsType.c_str(), "ext3") == 0 ) ||
                    (strcmp(fsVolInfo.fsType.c_str(), "ext4") == 0 ) ||
                    (strcmp(fsVolInfo.fsType.c_str(), "gfs2") == 0 ) ||
                    (strcmp(fsVolInfo.fsType.c_str(), "reiser3") == 0 ) ||
                    (strcmp(fsVolInfo.fsType.c_str(), "reiser4") == 0 ) ||
                    (strcmp(fsVolInfo.fsType.c_str(), "xfs") == 0 ))
                {
                    if (IsRootPartition(fsVolInfo.volName))
                    {
                        // inm_printf("Root partition on %s is being frozen.\n", fsVolInfo.volName.c_str());
                        continue; // skip
                    }

                    // skip in case of local vacp
                    if (!(strcmp(fsVolInfo.fsType.c_str(), "gfs2") == 0 ))
                    if (!gbRemoteSend)
                    {
                            if (!(lockfs(fsVolInfo, m_strContextUuid)))
                            {
                                ret = false;
                                break;
                            }

                        fsVolInfo.freezeState = FREEZE_STATE_LOCKED; 
                        continue;
                    }
                
                    options.command_line ("%s %s %s",COMMAND_DMSETUP, OPTION_SUSPEND, fsVolInfo.volName.c_str());
                    ACE_Process_Manager* pm = ACE_Process_Manager::instance();
                    if (pm == NULL) 
                    {
                        // need to continue with other devices even if there was a problem generating an ACE process
                        inm_printf("FAILED to Get ACE Process Manager instance.\n");
                        ret = false;
                        break;
                    }

                    pid_t pid = pm->spawn (options);
                    if ( pid == ACE_INVALID_PID) 
                    {
                        // need to continue with other devices even if there was a problem generating an ACE process
                        inm_printf("FAILED to create ACE Process to invoke dmsetup for device suspension.\n");
                        ret = false;
                        break;
                    }

                    // wait for the process to finish
                    ACE_exitcode status = 0;
                    pid_t rc = pm->wait(pid, &status);

                    if (status == 0) 
                    {
                        fsVolInfo.freezeState = FREEZE_STATE_LOCKED; 
                        inm_printf("Successfully suspended I/O on: %s\n", mntDirIter->c_str());
                        continue;
                    }
                    else 
                    {
                        inm_printf("ERROR: Failed to suspend I/O on: %s with error %d\n",
                            mntDirIter->c_str(),
                            status);
                        ret = false;
                        break;
                    }
                }
#ifdef VXFSVACP
                else if (strcmp(fsVolInfo.fsType.c_str(), "vxfs") == 0 )
                {
                    if (!lockvxfs(fsVolInfo))
                    {
                        ret = false;
                        break;
                    }
                    fsVolInfo.freezeState = FREEZE_STATE_LOCKED; 
                }
#endif
                else
                    inm_printf("Filesystem consistency not supported for mount point: %s of type %s.\n", 
                        mntDirIter->c_str(), 
                        fsVolInfo.fsType.c_str());
            }
        }

    }

    return ret;
}

bool AppConsistency::ThawDevices(const set<string> &inputVolumes)
{
    bool ret = true;
    ACE_Process_Manager* pm = ACE_Process_Manager::instance();
    
    if (pm == NULL) 
    {
        inm_printf("FAILED to Get ACE Process Manager instance\n");
        return false;
    }
    else
    {
        ACE_Process_Options options;
        options.handle_inheritance(false);

        inm_printf("\nStarting device I/O resumption...\n");
        //First copy m_volMntDirs set into a vector
        std::vector<string> vecvolMntDirs;
        std::set<string>::const_iterator iter = m_volMntDirs.begin();
        while(iter != m_volMntDirs.end())
        {
            vecvolMntDirs.push_back((*iter).c_str());
            iter++;
        }

        //sort the vector in descending order to freeze the inner most nested directoies first
        std::sort(vecvolMntDirs.begin(),vecvolMntDirs.end(),fsVolSortPredicate);

        std::vector<string>::const_iterator mntDirIter = vecvolMntDirs.begin();
        for(; mntDirIter != vecvolMntDirs.end(); mntDirIter++)
        {
            std::map<std::string, struct FsVolInfo >::iterator iter = m_fsVolInfoMap.find(mntDirIter->c_str());
            if(iter != m_fsVolInfoMap.end())
            {
                struct FsVolInfo & fsVolInfo= iter->second;

                if (fsVolInfo.freezeState != FREEZE_STATE_LOCKED)
                    continue;

                if ((strcmp(fsVolInfo.fsType.c_str(), "ext3") == 0 ) ||
                    (strcmp(fsVolInfo.fsType.c_str(), "ext4") == 0 ) ||
                    (strcmp(fsVolInfo.fsType.c_str(), "gfs2") == 0 ) ||
                    (strcmp(fsVolInfo.fsType.c_str(), "reiser3") == 0 ) ||
                    (strcmp(fsVolInfo.fsType.c_str(), "reiser4") == 0 ) ||
                    (strcmp(fsVolInfo.fsType.c_str(), "xfs") == 0 ))
                {
                    if (IsRootPartition(fsVolInfo.volName))
                    {
                        // inm_printf("Root partition on %s is being thawed.\n", fsVolInfo.volName.c_str());
                        continue; // skip
                    }

                    // skip in case of local vacp
                    if (!(strcmp(fsVolInfo.fsType.c_str(), "gfs2") == 0 ))
                    if (!gbRemoteSend)
                    {
                        if(!(unlockfs(fsVolInfo, m_strContextUuid)))
                            ret = false;
                        else
                            fsVolInfo.freezeState = FREEZE_STATE_UNLOCKED; 

                        continue;
                    }

                    options.command_line ("%s %s %s",COMMAND_DMSETUP, OPTION_RESUME, fsVolInfo.volName.c_str());
                    pid_t pid = pm->spawn (options);
                    if ( pid == ACE_INVALID_PID) 
                    {
                        // need to continue with other devices even if there was a problem generating an ACE process
                        inm_printf("FAILED to create ACE Process to invoke dmsetup to resume device I/O.\n");
                        ret = false;
                    }

                    // wait for the process to finish
                    ACE_exitcode status = 0;
                    pid_t rc = pm->wait(pid, &status);

                    if (status == 0) 
                    {
                        fsVolInfo.freezeState = FREEZE_STATE_UNLOCKED; 
                        inm_printf("Successfully resumed I/O on: %s\n", mntDirIter->c_str());
                    }
                    else 
                    {
                        inm_printf("ERROR: Failed to resume I/O on: %s with error %d\n",
                            mntDirIter->c_str(),
                            status);
                        ret = false;
                    }
                }
#ifdef VXFSVACP
                if (strcmp(fsVolInfo.fsType.c_str(), "vxfs") == 0 )
                {
                    if(!unlockvxfs(fsVolInfo))
                        ret = false;
                    else
                        fsVolInfo.freezeState = FREEZE_STATE_UNLOCKED; 
                }
#endif
            }
        }
    }

    return ret;
}

// 
// Function: ParseVolume
//   Go through the list of input volumes
//   if the input is a valid device file
//     accept it
//   else
//     if it is a valid mountpoint
//       get the corresponding device name
//       accept the device name as input volume
//     else
//       error
//     endif
//  endif
//
bool ParseVolume(std::set<std::string> &volumes, std::set<std::string> &disks, char *token)
{
    bool rv = true;

    do
    {
        if(0 == stricmp(token, "all"))
        {
            DataCacher::VolumesCache volumesCache;
            std::string err;
            int numCacheRetries = 0;
            while (!DataCacher::UpdateVolumesCache(volumesCache, err))
            {
                if (numCacheRetries < MAX_VOLUME_CACHE_READ_RETRIES)
                {
                    numCacheRetries++;
                    ACE_OS::sleep(MAX_VOLUME_CACHE_READ_WAIT_TIME);
                    continue;
                }
                else if (numCacheRetries > MAX_VOLUME_CACHE_READ_RETRIES)
                {
                    inm_printf("Failed to get volume cache with error %s\n", err.c_str());
                    return false;
                }

                // retry once with a volume discovery
                numCacheRetries++;

                VolumeSummaries_t volumeSummaries;
                VolumeDynamicInfos_t volumeDynamicInfos;
                VolumeInfoCollector volumeCollector((DeviceNameTypes_t)GetDeviceNameTypeToReport());

                volumeCollector.GetVolumeInfos(volumeSummaries, volumeDynamicInfos, false);

                DataCacher dataCacher;
                if (dataCacher.Init()) 
                {
                    if (dataCacher.PutVolumesToCache(volumeSummaries))
                    {
                        inm_printf("volume summaries are updated in local cache.\n");
                    }
                    else
                    {
                        inm_printf("Failed to cache volume summaries.\n");
                        return false;
                    }
                }
                else
                {
                    inm_printf("Failed to cache the volume summaries.\n");
                    return false;
                }
            }
           
            ConstVolumeSummariesIter viter(volumesCache.m_Vs.begin());
            ConstVolumeSummariesIter vend(volumesCache.m_Vs.end());
            while(viter != vend)
            {
                std::string devname = viter->name;

                // display_name attr is present only for disk
                ConstAttributesIter_t iter = viter->attributes.find(NsVolumeAttributes::DEVICE_NAME);
                if (iter != viter->attributes.end())
                {
                    devname = iter->second;
                }

                // convert it to actual device name instead
                // of symlink
                if(GetDeviceNameFromSymLink(devname))
                {
                    volumes.insert(string(devname));

                    if ((VolumeSummary::DISK == viter->type) &&
                        (VolumeSummary::NATIVE == viter->vendor))
                    {

                        disks.insert(devname);
                    }
                }

                viter++;
            }

            break;
        }

        if( IsValidDevfile(token))
        {
            string devname = token;
            if(!GetDeviceNameFromSymLink(devname))
            {
                rv = false;
                inm_printf("%s is not a valid device/mount point\n", devname.c_str());
                break;
            }
            volumes.insert(string(devname));
        }
        else
        {
            string devname;
            if (!strcmp(token, "/") == 0)
            {
                // PR # 4060 - Begin
                if(token[strlen(token) -1] == '/')
                    token[strlen(token) -1] = '\0' ;
                // PR # 4060 - End
            }

            if(GetDeviceNameForMountPt(token, devname))
            {
                volumes.insert(string(devname));
            }
            else
            {
                rv = false;
                inm_printf("%s is not a valid device/mount point\n", token);
                break;
            }
        }

    } while(0);

    return rv;
}

bool CrashConsistency::CreateIoBarrier(const long timeout)
{
    bool rv = true;

    do 
    {
        if (!DriverSupportsIoBarrier())
        {
            break;
        }

        int fdDriver = open(INMAGE_FILTER_DEVICE_NAME, O_RDWR );

        if (fdDriver < 0) 
        {
            m_exitCode = errno;
            inm_printf( "Function %s @LINE %d in FILE %s :unable to open %s with error %d\n",
                FUNCTION_NAME, LINE_NO, FILE_NAME, INMAGE_FILTER_DEVICE_NAME, m_exitCode);
            rv = false;
            break;
        }

        flt_barrier_create_t barrierInfo;
        barrierInfo.fbc_flags = 0;
        barrierInfo.fbc_timeout_ms = timeout;
        inm_memcpy_s(barrierInfo.fbc_guid, sizeof(barrierInfo.fbc_guid), m_strContextUuid.c_str(), GUID_LEN);

        ACE_Time_Value startTime = ACE_OS::gettimeofday();
        ACE_Time_Value currentTime;

        do 
        {

            inm_printf( "Creating io barrier\n");
            if(ioctl(fdDriver, IOCTL_INMAGE_CREATE_BARRIER_ALL, &barrierInfo) < 0)
            {
                int nRet = errno;

                if (nRet == EAGAIN)
                {
                    currentTime = ACE_OS::gettimeofday();
                    unsigned long elapsedTime = currentTime.msec() - startTime.msec();
                    if (elapsedTime < VACP_IOBARRIER_MAX_RETRY_TIME)
                    {
                        ACE_Time_Value tv;
                        tv.msec(VACP_IOBARRIER_RETRY_WAIT_TIME);
                        ACE_OS::sleep(tv);
                        continue;
                    }

                    inm_printf("ioctl failed: create io barrier failed after retries for %d ms. errno %d\n", VACP_IOBARRIER_MAX_RETRY_TIME, nRet);
                }
                else
                {
                    inm_printf("ioctl failed: create io barrier failed. errno %d\n", nRet);
                }
                if (nRet == 1)
                {
                    m_exitCode = VACP_E_DRIVER_IN_NON_DATA_MODE;
                }
                rv = false;
            }
            else
            {
                inm_printf( "Successfully created io barrier.\n");
            }
            break;

        } while(true);

        close(fdDriver);

    } while (false);

    return rv;
}

bool CrashConsistency::RemoveIoBarrier()
{
    bool rv = true;

    do 
    {
        if (!DriverSupportsIoBarrier())
        {
            break;
        }

        int fdDriver = open(INMAGE_FILTER_DEVICE_NAME, O_RDWR );

        if (fdDriver < 0) 
        {
            m_exitCode = errno;
            inm_printf( "Function %s @LINE %d in FILE %s :unable to open %s with error %d\n",
                FUNCTION_NAME, LINE_NO, FILE_NAME, INMAGE_FILTER_DEVICE_NAME, m_exitCode);
            rv = false;
            break;
        }

        flt_barrier_remove_t barrierInfo;
        barrierInfo.fbr_flags = 0;
        inm_memcpy_s(barrierInfo.fbr_guid, sizeof(barrierInfo.fbr_guid), m_strContextUuid.c_str(), GUID_LEN);

        inm_printf( "Removing io barrier\n");
        if(ioctl(fdDriver, IOCTL_INMAGE_REMOVE_BARRIER_ALL, &barrierInfo) < 0)
        {
            m_exitCode = errno;
            inm_printf( "ioctl failed: remove io barrier failed. errno %d\n", m_exitCode);
            rv = false;
        }
        else
        {
            inm_printf( "Successfully removed io barrier.\n");
            rv = true;
        }

        close(fdDriver);

    } while (false);

    return rv;
}
