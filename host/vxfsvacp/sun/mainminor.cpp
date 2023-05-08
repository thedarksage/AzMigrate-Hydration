#include <stdio.h>
#include <sys/mnttab.h>
#include <sys/mkdev.h>
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

#include "volumegroupsettings.h"
#include "volumeinfocollector.h"
#include "portablehelpers.h"
#include "portablehelpersminor.h"

#include "volumereporter.h"
#include "volumereporterimp.h"

#include <limits.h>
#include <sys/lockfs.h>
#include <sys/fs/vx_ioctl.h>


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

set<string> volMntDirs;  //Set of mount dirs to be freezed
bool GetMountPointsForVolumes(const set<string> &, set<string> &);
bool  lockufs(const char *fn, SV_USHORT lock = LOCKFS_ULOCK)
{
	int		fd;
	struct lockfs	lf;

	fd = open64(fn, O_RDONLY);
	if (fd == -1) {
		perror(fn);
		cout << "Unable to Open: " << fn  << std::endl;
		return false;
	}

	memset((caddr_t)&lf, 0,  sizeof (struct lockfs));

	lf.lf_flags = LOCKFS_MOD;
	if (ioctl(fd, _FIOLFSS, &lf) == -1) {
		cout << "Invalid IOCTL: _FIOLFSS for "   << fn << std::endl;
		perror(fn);
		return false;
	}

	lf.lf_lock	= lock;
	lf.lf_flags	= 0;
	lf.lf_key	= lf.lf_key;
	if(lock == LOCKFS_WLOCK)
		lf.lf_comment	= "Locked by VACP";
	else 
		lf.lf_comment = "" ;

	lf.lf_comlen = strlen(lf.lf_comment);

	if (ioctl(fd, _FIOLFS, &lf) == -1) {
		cout << "Invalid IOCTL: _FIOLFS for "   << fn << std::endl;
		perror(fn);
		close(fd);
		return false;
	}
	close(fd);
	cout << "Volume " << fn <<  " Freezed Succesfully.\n";
	return true;
}


bool
flushfs(const char *fn)
{
	int		fd;

	fd = open64(fn, O_RDONLY);
	if (fd == -1) {
		perror(fn);
		cout << "Unable to Open: " << fn  << std::endl;
		return false;
	}

	if (ioctl(fd, _FIOFFS, NULL) == -1) {
		perror(fn);
		cout << "Invalid IOCTL:  _FIOFFS  for " << fn << std::endl;
		close(fd);
		return false;
	}
	close(fd);
	return true;
}

void lockvxfs(vector<string> &vxfs)
{
	struct vx_freezeall freeze_info;
	if (vxfs.size() == 0 )
		return;

	std::sort(vxfs.begin(), vxfs.end(), vxfsVolSortPredicate);
	for (unsigned short i = 0; i < vxfs.size(); i++) 
	{
		cout<<"Opening Volume "<<vxfs[i] <<"."<<  endl;
		int fd = open(vxfs[i].c_str(), O_RDONLY);
		if (fd < 0) 
		{
			cout<<"Failed to open : "<<vxfs[i]<<endl;
		}
		else
		{
		    cout<<"Freezing Volume "<<vxfs[i] <<"..."<<  endl;
		    if (ioctl(fd, VX_FREEZE, 180))
		    {
		        perror("IOCTL failed");
		        cout<<"vxfs file system freeze failed for volume "<< vxfs[i] <<"."<<endl;
		    }
		    else
		    {
				vxfsInfoVec.push_back(VxfsVolInfo(fd,vxfs[i]));
				cout<<"Volume "<<vxfs[i] <<" has been frozen succesfully."<<  endl;
		    }
		}

	}

	/*
	* Format the FREEZE_ALL data structure
	*/
	/*We are taking 180 secs as max timeout, within which we try to unfreeze the volume */
/*
	freeze_info.num = vxfs.size();
	freeze_info.timeout = 180;
	freeze_info.fds = &vxfs_fd[0];
*/

	/*
	* Initiate FREEZE_ALL using IOCTL interface
	*/

/*
	if (ioctl(vxfs_fd[0], VX_FREEZE_ALL, &freeze_info)) 
	{
		perror("IOCTL failed");
		cout<<"vxfs file system freeze failed."<<endl;
	}
	else
	{
		cout<<"All vxfs file system(s) have been frozen succesfully"<<endl;
	}

	for (unsigned short i = 0; i < vxfs.size(); i++)
		if (vxfs_fd[i] > 0)
			close(vxfs_fd[i]);

*/
}

void unlockvxfs(const vector<string> &vxfs)
{
	for (unsigned short i = 0; i < vxfsInfoVec.size(); i++) 
	{
//		cout<<"FD is "<<vxfsInfoVec[i].fd << " And Volume is "<<vxfsInfoVec[i].volName<<".\n";
		if (ioctl(vxfsInfoVec[i].fd, VX_THAW, NULL)) 
		{
			if (errno != ETIMEDOUT)
			{
				perror("IOCTL failed");
				cout << "I/O resume failed for " << vxfsInfoVec[i].volName << " with errno " << errno <<"." << endl;
			}
				perror("IOCTL failed");
		}
		else
		{
			cout << "I/O resumed for " << vxfsInfoVec[i].volName << "." << endl;
		}
		close(vxfsInfoVec[i].fd);
	}
}


void FreezeDevices(const set<string> &inputVolumes)
{
	struct mnttab	mnttab_entry, mnttab_filter;
	FILE		*mnttabfile;

	cout << "\nStarting device I/O suspension...\n";
	GetMountPointsForVolumes(inputVolumes, volMntDirs);
	set<string>::const_iterator deviceIter = volMntDirs.begin();

	vector<string> vfsMtPoints;

	mnttabfile = fopen(MNTTAB, "r");
	if (!mnttabfile) {
		cout << "Unable to open the MNTTAB. None of the Volumes will be freezed." << endl;
		return;
	}
	for(; deviceIter != volMntDirs.end(); deviceIter++)
	{
		mnttab_filter.mnt_special = NULL;
		mnttab_filter.mnt_mountp = (char *)deviceIter->c_str();
		mnttab_filter.mnt_fstype = "ufs";
		mnttab_filter.mnt_mntopts = NULL;
		mnttab_filter.mnt_time = NULL;
		rewind(mnttabfile);

		/*
		* Get the mnttab entry
		*/

		if (getmntany
			(mnttabfile, &mnttab_entry, &mnttab_filter)) 
		{
			mnttab_filter.mnt_fstype = "vxfs";
			rewind(mnttabfile);
			if (getmntany
				(mnttabfile, &mnttab_entry, &mnttab_filter)) 
			{
				cout<<deviceIter->c_str() <<" will not be freezed. \n Freeze is supported only for vfs and ufs file systems."<<endl;
			}
			else
			{
				vfsMtPoints.push_back(deviceIter->c_str());
			}
		}
		else
		{
			if ( lockufs(deviceIter->c_str(), LOCKFS_WLOCK))
			{
				cout << "Successfully suspended I/O on the device: " << deviceIter->c_str() << endl;
				if ( flushfs(deviceIter->c_str()))
				{
					cout << "Successfully flushed I/O to the device: " << deviceIter->c_str() << endl;
				}
				else
				{
					cout << "Failed to flush I/O to the device: " << deviceIter->c_str() << endl;
				}
			}
			else
			{
				cout << "Failed to suspend I/O on the device: " << deviceIter->c_str() << endl;
			}
		}
	}
	lockvxfs(vfsMtPoints);
}

void ThawDevices(const set<string> &inputVolumes)
{
	struct mnttab	mnttab_entry, mnttab_filter;
	FILE		*mnttabfile;
	cout << "\nStarting device I/O Resumtion...\n";

	vector<string> vfsMtPoints;

	mnttabfile = fopen(MNTTAB, "r");
	if (!mnttabfile) {
		cout << "ERROR: Unable to open the MNTTAB. None of the Volumes will be freezed." << endl;
		return;
	}
	set<string>::const_iterator deviceIter = volMntDirs.begin();
	for(; deviceIter != volMntDirs.end(); deviceIter++)
	{
		mnttab_filter.mnt_special = NULL;
		mnttab_filter.mnt_mountp = (char *)deviceIter->c_str();
		mnttab_filter.mnt_fstype = "ufs";
		mnttab_filter.mnt_mntopts = NULL;
		mnttab_filter.mnt_time = NULL;
		rewind(mnttabfile);

		/*
		* Get the mnttab entry
		*/

		if (getmntany
			(mnttabfile, &mnttab_entry, &mnttab_filter)) 
		{
            		mnttab_filter.mnt_fstype = "vxfs";
			rewind(mnttabfile);
		    	if (getmntany
			(mnttabfile, &mnttab_entry, &mnttab_filter))
			{
				cout<<deviceIter->c_str() <<" will not be thawed. \n Freeze/Thaw is supported only for vfs and ufs file systems."<<endl;
			}
		    	else
		    	{
				vfsMtPoints.push_back(deviceIter->c_str());
		    	}
		}
		else
		{
			if (strcmp(mnttab_filter.mnt_fstype, "ufs") == 0)
			{
				if ( lockufs(deviceIter->c_str()))
				{
					cout << "Successfully resumed I/O on the device: " << deviceIter->c_str() << endl;
				}
				else
				{
					cout << "Failed to resume I/O on the device: " << deviceIter->c_str() << endl;
				}
			}
		}
	}
	unlockvxfs(vfsMtPoints);
}

bool isAcctLogDir(const std::string & mountPoint)
{
    char cmd_buf[1024 + 100];
    int ret_val;

	inm_sprintf_s(cmd_buf, ARRAYSIZE(cmd_buf), "/usr/sbin/acctadm | grep 'accounting file' | grep -v 'accounting file: none' | awk '{print $NF}'| xargs -n 1 dirname | xargs -n 1 df -n 2>/dev/null | grep '^%s[ ]*.:' > /dev/null", mountPoint.c_str());


    // printf(" Running : %s\n", cmd_buf);
    ret_val = system(cmd_buf);
    if (ret_val < 0)
    {
        printf("%s returned error %d\n", cmd_buf, ret_val);
        return false;
    }

    if (ret_val != 0)
    {
        // printf("shell returned %d for %s\n", ret_val, cmd_buf);
        return false;
    }

    return true;
}

bool GetFSTagSupportedolumes(std::set<std::string> & supportedFsVolumes)
{
	bool rv  = true;

	FILE *fp;
	struct mnttab *entry = NULL;
	char buffer[MAXPATHLEN*4];
	fp = fopen(SV_PROC_MNTS, "r");
	if (fp == NULL)
	{
		std::cout << "unable to open " << SV_PROC_MNTS << '\n';
		return false;
	}

	entry = (struct mnttab *) malloc(sizeof (struct mnttab));
	resetmnttab(fp);
	  while( SVgetmntent(fp, entry,buffer,sizeof(buffer)) != -1  )
	{
		if (!IsValidDevfile(entry->mnt_special))
			continue;

		std::string devname = entry -> mnt_special;
            if (!isAcctLogDir(entry->mnt_mountp))
                supportedFsVolumes.insert(devname);
            else
            {
                cout << endl << "Process/task/flow accounting is being logged to " << entry->mnt_mountp << endl
                    << "It will be excluded from filesystem consistency." << endl;
            }

	}

	if ( fp )
	{
		free (entry);
		resetmnttab(fp);
		fclose(fp);
	}

	return rv;
}


bool GetMountPointsForVolumes(const std::set<std::string> & fsVolumes, std::set<std::string> & mntDirs)
{
	bool rv  = true;

	FILE *fp;
	struct mnttab *entry = NULL;
	char buffer[MAXPATHLEN*4];
	fp = fopen(SV_PROC_MNTS, "r");
	if (fp == NULL)
	{
		std::cout << "unable to open " << SV_PROC_MNTS << '\n';
		return false;
	}

	entry = (struct mnttab *) malloc(sizeof (struct mnttab));
	resetmnttab(fp);
	while( SVgetmntent(fp, entry,buffer,sizeof(buffer)) != -1  )
	{
		std::string devname = entry -> mnt_special;
		if(fsVolumes.find(devname) != fsVolumes.end())
		    if (!isAcctLogDir(entry->mnt_mountp))
			mntDirs.insert(entry->mnt_mountp);
	}

	if ( fp )
	{
		free (entry);
		resetmnttab(fp);
		fclose(fp);
	}

	return rv;
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
		//This needs to be included once we are able to build
		// VolumeInfoColector on all Solaris versions
		if(0 == stricmp(token, "all"))
		{
			VolumeSummaries_t volumeSummaries;
			VolumeInfoCollector volumeCollector;
			VolumeDynamicInfos_t volumeDynamicInfos;
			volumeCollector.GetVolumeInfos(volumeSummaries,volumeDynamicInfos, false);
			VolumeReporter *pvr = new VolumeReporterImp();
			VolumeReporter::CollectedVolumes_t cvs;
			pvr->GetCollectedVolumes(volumeSummaries, cvs);
			delete pvr;

			std::set<std::string>::const_iterator viter(cvs.m_Volumes.begin());
			std::set<std::string>::const_iterator vend(cvs.m_Volumes.end());
                        while(viter != vend)
                        {
                        	string devname = *viter;
                		volumes.insert(string(devname));
                        	viter++;
                        }
                        break;
		}

		if( IsValidDevfile(token))
		{
			string devname = token;
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
