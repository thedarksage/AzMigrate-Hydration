#include <stdio.h>
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

#include "ace/High_Res_Timer.h"
#include "ace/os_include/os_fcntl.h"
#include "ace/OS.h"
#include "ace/Process_Manager.h"

#include "sys/fs/vx_ioctl.h"

#include "volumegroupsettings.h"
#include "portablehelpers.h"
#include "volumeinfocollector.h"
#include "StreamEngine.h"
#include "volumereporter.h"
#include "volumereporterimp.h"

struct VxfsVolInfo
{
    VxfsVolInfo(int f, const std::string & s)
    {
        fd = f;
        volName = s;
    }
    int fd;
    std::string volName;
};
bool vxfsVolSortPredicate(const std::string &l, const std::string &r)
{
	return l>=r;
}
std::vector <VxfsVolInfo> vxfsInfoVec;
using namespace std;
//
// Function: GetFSTagSupportedVolumes
//   Gets the list of volumes on which file system consistency tag can be issued
//   Currently, we can issue filesystem consistency tag only if the
//   volume is mounted and the filesystem is a journaled filesystem
//
//   TODO: we are not verifying the filesystem for now

vector<string> volMntDirs;  //Set of mount dirs to be freezed

#define COMMAND_DMSETUP					"dmsetup"
#define OPTION_SUSPEND					"suspend"
#define OPTION_RESUME					"resume"
#define SV_PROC_MNTS        "/etc/mtab"
#define SV_PROC_MNTS        "/etc/mtab"

void lockvxfs(const vector<string> &vxfs);
void unlockvxfs(const vector<string> &vxfs);
bool GetFSForVolume(const std::string &dev, std::string &fsType);
bool GetMountPointsForVolumes(const std::vector<std::string> & fsVolumes, std::vector<std::string> & mntDirs);

bool GetFSTagSupportedolumes(std::set<std::string> & supportedFsVolumes)
{
    bool rv  = true;

    FILE *fp;
	struct mntent *entry = NULL;

	fp = setmntent("/proc/mounts", "r");

	if (fp == NULL)
	{
		std::cout << "unable to open /proc/mounts\n";
		return false;
	}

	while( (entry = getmntent(fp)) != NULL )
	{
        if (!IsValidDevfile(entry->mnt_fsname))
		    continue;

		std::string devname = entry -> mnt_fsname;
        if(!GetDeviceNameFromSymLink(devname))
            continue;

        supportedFsVolumes.insert(devname);
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

    fp = setmntent("/etc/mtab", "r");

    if (fp == NULL)
    {
        std::cout << "unable to open /proc/mounts\n";
        return false;
    }

    while( (entry = getmntent(fp)) != NULL )
    {
        if (!IsValidDevfile(entry->mnt_fsname))
            continue;

        std::string mntDir = entry -> mnt_dir;
        std::string devName = entry -> mnt_fsname;
        if(partitionName == devName && (mntDir == "/"))
        {
            rv = true;
            std::cout<<partitionName<<" is a root file system."<<std::endl;
            break;
        }
    }

    if ( NULL != fp )
    {
        endmntent(fp);
    }

    return rv;
}

bool GetFSForVolume(const std::string &dev, std::string &fsType)
{
    bool rv = false;
    FILE * fp = NULL;
    std::string proc_fsname = "";
    struct mntent *entry = NULL;

    do
    {
        fp = setmntent(SV_PROC_MNTS, "r");

        if (fp == NULL)
        {
            DebugPrintf(SV_LOG_ERROR,
                "Function %s @LINE %d in FILE %s :failed to open %s \n",
                FUNCTION_NAME, LINE_NO, FILE_NAME, SV_PROC_MNTS);
            rv = false;
            break;
        }

        std::string sMountPoint="";
        while( (entry = getmntent(fp)) != NULL )
        {
            proc_fsname = entry->mnt_fsname;
            if (!IsValidDevfile(proc_fsname))
                continue;

            GetDeviceNameFromSymLink(proc_fsname);
            //if ( 0 == strcmp(entry->mnt_dir, dev.c_str())) //compare with mount dir, as we pass mount point here.
			if ( 0 == strcmp(entry->mnt_fsname, dev.c_str())) //compare with file system name
            {
                fsType = entry->mnt_type;
                rv = true;
                break;
            }
        }

    }while(0);

    if ( NULL != fp )
    {
        endmntent(fp);
    }

    return rv;
}
void FreezeVxFsDevices(const set<string> &inputVolumes)
{
	vector<string> vfsMtPoints;
    set<string>::const_iterator deviceIter = inputVolumes.begin();

	cout << "\nStarting device I/O suspension...\n";
	std::set<std::string> supportedFSs;
	GetFSTagSupportedolumes(supportedFSs);
	for(; deviceIter != inputVolumes.end(); deviceIter++)
	{
		if(supportedFSs.find(*deviceIter) == supportedFSs.end())
		{
		   std::cout<<"Volume "<<*deviceIter<<" will not be freezed.\n";
		   continue;
		}
		if (IsRootPartition(*deviceIter))
		{
			std::cout<<"Root Partition "<<*deviceIter<<" will not be freezed.\n";
			continue;
		}
		std::string fsType;
		if (!GetFSForVolume(*deviceIter, fsType))
		{
			std::cout<<"Get File System For volume "<<*deviceIter<<" Failed.\n";
		}
		else if(fsType == "vxfs") //VXFS file system
		{
		    cout<<endl<<"fsType is vxfs."<<endl;
			vfsMtPoints.push_back(deviceIter->c_str());
			continue;
		}
		else
		{
		    cout<<endl<<"fsType is "<<fsType<<endl;
		}
	}
	lockvxfs(vfsMtPoints);
}
void FreezeDevices(const set<string> &inputVolumes)
{
	vector<string> vfsMtPoints;
	ACE_Process_Manager* pm = ACE_Process_Manager::instance();
    set<string>::const_iterator deviceIter = inputVolumes.begin();


	if (pm == NULL) 
	{
		// need to continue with other devices even if there was a problem generating an ACE process
		cout << "FAILED to Get ACE Process Manager instance" << std::endl;
	}
	else
	{
        ACE_Process_Options options;
        options.handle_inheritence(false);

		cout << "\nStarting device I/O suspension...\n";
        std::set<std::string> supportedFSs;
        GetFSTagSupportedolumes(supportedFSs);
		for(; deviceIter != inputVolumes.end(); deviceIter++)
		{
            if(supportedFSs.find(*deviceIter) == supportedFSs.end())
            {
               std::cout<<"Volume "<<*deviceIter<<" will not be freezed.\n";
               continue;
            }
            if (IsRootPartition(*deviceIter))
            {
                std::cout<<"Root Partition "<<*deviceIter<<" will not be freezed.\n";
                continue;
            }

            std::string fsType;
            if (!GetFSForVolume(*deviceIter, fsType))
            {
                std::cout<<"Get File System For volume "<<*deviceIter<<" Failed.\n";
            }
            else if(fsType == "vxfs") //VXFS file system
            {
				vfsMtPoints.push_back(deviceIter->c_str());
                continue;
            }

			options.command_line ("%s %s %s",COMMAND_DMSETUP, OPTION_SUSPEND, deviceIter->c_str());
			ACE_Process_Manager* pm = ACE_Process_Manager::instance();
			if (pm == NULL) 
			{
				// need to continue with other devices even if there was a problem generating an ACE process
				cout << "FAILED to Get ACE Process Manager instance" << std::endl;
			}

			pid_t pid = pm->spawn (options);
			if ( pid == ACE_INVALID_PID) 
			{
				// need to continue with other devices even if there was a problem generating an ACE process
				cout << "FAILED to create ACE Process to invoke dmsetup for device suspension" << endl;
			}

			// wait for the process to finish
			ACE_exitcode status = 0;
			pid_t rc = pm->wait(pid, &status);

			//cout << "ACE Exit code status : " << status << std::endl;
			if (status == 0) 
			{
				cout << "Successfully suspended I/O on the device: " << deviceIter->c_str() << endl;

			}
			else 
			{
				cout << "ERROR:" << status << " Failed to suspend I/O on the device: " << deviceIter->c_str() << endl;
			}
		}
	    lockvxfs(vfsMtPoints);
	}
}

void lockvxfs(const vector<string> &vxfsDevices)
{
	struct vx_freezeall freeze_info;
	if (vxfsDevices.size() == 0 )
		return;
    vector<string> vxfs;
    GetMountPointsForVolumes(vxfsDevices, vxfs);
	std::sort(vxfs.begin(), vxfs.end(), vxfsVolSortPredicate);
	for (unsigned short i = 0; i < vxfs.size(); i++) 
	{
		int fd = open(vxfs[i].c_str(), O_RDONLY);
		if (fd < 0) 
		{
			cout<<"Failed to open : "<<vxfs[i]<<endl;
		}
        else
        {
            if (ioctl(fd, VX_FREEZE, 180)) 
            {
                perror("IOCTL failed");
                cout<<"vxfs file system freeze failed for volume "<< vxfs[i] <<"."<<endl;
            }
            else
            {
                vxfsInfoVec.push_back(VxfsVolInfo(fd,vxfs[i]));
                cout<<"Volume "<<vxfs[i] <<" has beed frozen succesfully"<<endl;
            }
        }

	}

#if 0
	/*
	* Format the FREEZE_ALL data structure
	*/
	freeze_info.num = vxfs.size();
	/*We are taking 180 secs as max timeout, within which we try to unfreeze the volume */
	freeze_info.timeout = 180;
	freeze_info.fds = &vxfs_fd[0];

	/*
	* Initiate FREEZE_ALL using IOCTL interface
	*/

	if (ioctl(vxfs_fd[0], VX_FREEZE_ALL, &freeze_info)) 
	{
		perror("IOCTL failed");
		cout<<"vxfs file system freeze failed."<<endl;
	}
	else
	{
		cout<<"All vxfs file system(s) have been frozen succesfully"<<endl;
	}

#endif
/*
	for (unsigned short i = 0; i < vxfs.size(); i++)
		if (vxfs_fd[i] > 0)
			close(vxfs_fd[i]);

*/
}

void unlockvxfs(const vector<string> &vxfsDevices)
{
    //vector<string> vxfs;
    //GetMountPointsForVolumes(vxfsDevices, vxfs);
	for (unsigned short i = 0; i < vxfsInfoVec.size(); i++)
	{
		if (ioctl(vxfsInfoVec[i].fd, VX_THAW, NULL))
		{
			if (errno != ETIMEDOUT)
			{
				perror("IOCTL failed");
				cout << "I/O resume failed for " << vxfsInfoVec[i].volName << " with errno " << errno <<"." << endl;
			}
		}
		else
		{
			cout << "I/O resumed for " << vxfsInfoVec[i].volName << "." << endl;
		}
		close(vxfsInfoVec[i].fd);
	}
}

void ThawVxFsDevices(const set<string> &inputVolumes)
{
	vector<string> vfsMtPoints;
    set<string>::const_iterator deviceIter = inputVolumes.begin();
	cout << "\nStarting device I/O resumption...\n";
	std::set<std::string> supportedFSs;
	GetFSTagSupportedolumes(supportedFSs);
	for(; deviceIter != inputVolumes.end(); deviceIter++)
	{
		if(supportedFSs.find(*deviceIter) == supportedFSs.end())
		{
		   continue;
		}
		if (IsRootPartition(*deviceIter))
		{
			continue;
		}

		std::string fsType;
		if (!GetFSForVolume(*deviceIter, fsType))
		{
			std::cout<<"Get File System For volume "<<*deviceIter<<" Failed.\n";
		}
		else if(fsType == "vxfs") //VXFS file system
		{
			vfsMtPoints.push_back(deviceIter->c_str());
			continue;
		}
	}
	unlockvxfs(vfsMtPoints);
}
void ThawDevices(const set<string> &inputVolumes)
{
	vector<string> vfsMtPoints;

    set<string>::const_iterator deviceIter = inputVolumes.begin();

	ACE_Process_Manager* pm = ACE_Process_Manager::instance();
	if (pm == NULL) 
	{
		// need to continue with other devices even if there was a problem generating an ACE process
		cout << "FAILED to Get ACE Process Manager instance" << std::endl;
	}
	else
	{		
		ACE_Process_Options options;
		options.handle_inheritence(false);
		cout << "\nStarting device I/O resumption...\n";
        std::set<std::string> supportedFSs;
        GetFSTagSupportedolumes(supportedFSs);
		for(; deviceIter != inputVolumes.end(); deviceIter++)
		{
            if(supportedFSs.find(*deviceIter) == supportedFSs.end())
            {
               continue;
            }
            if (IsRootPartition(*deviceIter))
            {
                continue;
            }

            std::string fsType;
            if (!GetFSForVolume(*deviceIter, fsType))
            {
                std::cout<<"Get File System For volume "<<*deviceIter<<" Failed.\n";
            }
            else if(fsType == "vxfs") //VXFS file system
            {
				vfsMtPoints.push_back(deviceIter->c_str());
                continue;
            }
			options.command_line ("%s %s %s",COMMAND_DMSETUP, OPTION_RESUME, deviceIter->c_str());
			pid_t pid = pm->spawn (options);
			if ( pid == ACE_INVALID_PID) 
			{
				// need to continue with other devices even if there was a problem generating an ACE process
				cout << "FAILED to create ACE Process to invoke dmsetup to resume device I/O\n";
			}

			// wait for the process to finish
			ACE_exitcode status = 0;
			pid_t rc = pm->wait(pid, &status);

			//cout << "ACE Exit code status : " << status << std::endl;
			if (status == 0) 
			{
				cout << "Successfully resumed I/O on the device: " << deviceIter->c_str() << endl;
			}
			else 
			{
				cout << "ERROR:" << status << " Failed to resume I/O on the device: " << deviceIter->c_str() << endl;
			}
		}
	    unlockvxfs(vfsMtPoints);
	}
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
bool ParseVolume(set<string> & volumes, char *token)
{
	bool rv = true;

	do
	{
		if(0 == stricmp(token, "all"))
		{
			VolumeSummaries_t volumeSummaries;
            VolumeDynamicInfos_t volumeDynamicInfos;
			VolumeInfoCollector volumeCollector;
			volumeCollector.GetVolumeInfos(volumeSummaries, volumeDynamicInfos, false);
            VolumeReporter *pvr = new VolumeReporterImp();
            VolumeReporter::CollectedVolumes_t cvs;
            pvr->GetCollectedVolumes(volumeSummaries, cvs);
            delete pvr;
			std::set<std::string>::const_iterator viter(cvs.m_Volumes.begin());
			std::set<std::string>::const_iterator vend(cvs.m_Volumes.end());
			while(viter != vend)
			{
				string devname = *viter;

				// convert it to actual device name instead
				// of symlink
				if(GetDeviceNameFromSymLink(devname))
					volumes.insert(string(devname));
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
				cout << devname << " is not a valid device/mount point\n";
				break;
			}
			volumes.insert(string(devname));
		}
		else
		{
			string devname;
			// PR # 4060 - Begin
			if(token[strlen(token) -1] == '/')
				token[strlen(token) -1] = '\0' ;
			// PR # 4060 - End

			if(GetDeviceNameForMountPt(token, devname))
			{
				volumes.insert(string(devname));
			}
			else
			{
				rv = false;
				cout << token << " is not a valid device/mount point\n";
				break;
			}
		}

	} while(0);

	return rv;
}
bool GetMountPointsForVolumes(const std::vector<std::string> & fsVolumes, std::vector<std::string> & mntDirs)
{
    bool rv  = true;

    FILE *fp;
    struct mntent *entry = NULL;

    fp = fopen(SV_PROC_MNTS, "r");
    if (fp == NULL)
    {
        std::cout << "unable to open " << SV_PROC_MNTS << '\n';
        return false;
    }

    while( (entry = getmntent(fp) )!= NULL )
    {
        std::string devname = entry -> mnt_fsname;
        if(find(fsVolumes.begin(), fsVolumes.end(),devname)!= fsVolumes.end())
            mntDirs.push_back(entry->mnt_dir);
    }

    if ( fp )
    {
        endmntent(fp);
    }

    return rv;
}

