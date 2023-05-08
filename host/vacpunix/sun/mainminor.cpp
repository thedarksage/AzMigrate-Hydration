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
#include <algorithm>

#include <iostream>
#include <vector>
#include <set>
#include <string>
#include "portablehelpers.h"
#include "portablehelpersminor.h"
#include <sys/lockfs.h>
#include "volumeinfocollector.h"
#include "volumereporter.h"
#include "volumereporterimp.h"
#include "StreamEngine.h"

#ifdef VXFSVACP 
#include <sys/fs/vx_ioctl.h>
#endif

using namespace std;

struct FsVolInfo
{
    FsVolInfo(const std::string & s, const std::string & t, const std::string & m)
    {
        fd = -1;
        volName = s;
		fsType = t;
        mntDir = m;
    }
    int fd;
    std::string volName;
    std::string fsType;
    std::string mntDir;
};

std::map<std::string, struct FsVolInfo> fsVolInfoMap;

set<string> volMntDirs;  //Set of mount dirs to be freezed

bool fsVolSortPredicate(const std::string &l, const std::string &r)
{
	return l>=r;
}
bool IsSystemDir(const std::string & mountPoint);
bool GetMountPointsForVolumes(const set<string> &, set<string> &);

bool  lockfs(const char *fn, SV_USHORT lock = LOCKFS_ULOCK)
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
	if(lock == LOCKFS_WLOCK)
		cout << "Filesystem freeze successful on: " << fn <<  endl;
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

#ifdef VXFSVACP
void lockvxfs(struct FsVolInfo &fsVolInfo)
{
	// cout<<"Opening Volume "<< fsVolInfo.mntDir.c_str() <<"."<<  endl;
	int fd = open(fsVolInfo.mntDir.c_str(), O_RDONLY);
	if (fd < 0) 
	{
		cout<<"Failed to open : "<< fsVolInfo.mntDir.c_str() <<endl;
		return;
	}
	else
	{
	    // cout<<"Freezing Volume "<< fsVolInfo.mntDir.c_str() <<"..."<<  endl;
	    if (ioctl(fd, VX_FREEZE, 180))
	    {
            perror("IOCTL failed");
            cout<<"Failed to freeze filesystem: "<< fsVolInfo.mntDir.c_str() <<endl;
	    }
	    else
	    {
            fsVolInfo.fd = fd;
            cout << "Filesystem freeze successful on: " << fsVolInfo.mntDir.c_str() <<  endl;
            cout << "Successfully flushed I/O to : " << fsVolInfo.mntDir.c_str() << endl;
	    }
	}
}


void unlockvxfs(const struct FsVolInfo &fsVolInfo)
{
    if (fsVolInfo.fd > 0)
    {
        if (ioctl(fsVolInfo.fd, VX_THAW, NULL)) 
        {
            if (errno != ETIMEDOUT)
            {
                perror("IOCTL failed");
                cout << "Failed to resume I/O on: " << fsVolInfo.mntDir.c_str() << " with errno " << errno <<"." << endl;
            }
            else
                cout << "I/O resumed on " << fsVolInfo.mntDir.c_str() << " after timeout." << endl;

        }
        else
        {
            cout << "Successfully resumed I/O on: " << fsVolInfo.mntDir.c_str() << endl;
        }

        close(fsVolInfo.fd);
    }
}
#endif

void FreezeDevices(const set<string> &inputVolumes)
{
    cout << "\nStarting device I/O suspension...\n";
	GetMountPointsForVolumes(inputVolumes, volMntDirs);

	//First copy volMntDirs set into a vector
	std::vector<string> vecVolMntDirs;
	std::set<string>::const_iterator iter = volMntDirs.begin();
	while(iter != volMntDirs.end())
	{
        vecVolMntDirs.push_back((*iter).c_str());
        iter++;
	}

	//sort the vector in descending order to freeze the inner most nested directoies first
    std::sort(vecVolMntDirs.begin(),vecVolMntDirs.end(),fsVolSortPredicate);

	std::vector<string>::const_iterator mntDirIter = vecVolMntDirs.begin();
    for(; mntDirIter != vecVolMntDirs.end(); mntDirIter++)
    {
        std::map<std::string, struct FsVolInfo >::iterator iter = fsVolInfoMap.find(mntDirIter->c_str());
        if(iter != fsVolInfoMap.end())
        {
            struct FsVolInfo & fsVolInfo= iter->second;
            if (strcmp(fsVolInfo.fsType.c_str(), "ufs") == 0 )
            {
                if ( lockfs(mntDirIter->c_str(), LOCKFS_WLOCK))
                {
                     // cout << "Successfully suspended I/O on : " << mntDirIter->c_str() << endl;
                     if ( flushfs(mntDirIter->c_str()))
                     {
                         cout << "Successfully flushed I/O to : " << mntDirIter->c_str() << endl;
                     }
                     else
                     {
                         cout << "ERROR: Failed to flush I/O to : " << mntDirIter->c_str() << endl;
                     }
                 }
                 else
                 {
                     cout << "ERROR: Failed to freeze filesystem: " << mntDirIter->c_str() << endl;
                 }
             }
#ifdef VXFSVACP
            else if (strcmp(fsVolInfo.fsType.c_str(), "vxfs") == 0 )
            {
                lockvxfs(fsVolInfo);
            }
#endif
            else
                cout << "Filesystem consistency not supported for mount point: " << mntDirIter->c_str() << " of type " << fsVolInfo.fsType.c_str() << "." << endl;
         }
    }
}

void ThawDevices(const set<string> &inputVolumes)
{
     cout << "\nStarting device I/O resumption...\n";
	 //First copy volMntDirs set into a vector
	 std::vector<string>vecVolMntDirs;
	 std::set<string>::const_iterator iter = volMntDirs.begin();
	 while(iter != volMntDirs.end())
	 {
		 vecVolMntDirs.push_back((*iter).c_str());
		 iter++;
	 }
	 //sort the vector in descending order to freeze the inner most nested directoies first
	 std::sort(vecVolMntDirs.begin(),vecVolMntDirs.end(),fsVolSortPredicate);
	 std::vector<string>::const_iterator mntDirIter = vecVolMntDirs.begin();
     for(; mntDirIter != vecVolMntDirs.end(); mntDirIter++)
     {
        std::map<std::string, struct FsVolInfo>::iterator iter = fsVolInfoMap.find(mntDirIter->c_str());
        if(iter != fsVolInfoMap.end())
        {
            struct FsVolInfo &fsVolInfo = iter->second;
            if (strcmp(fsVolInfo.fsType.c_str(), "ufs") == 0 )
            {
                if ( lockfs(mntDirIter->c_str()))
                {
                    cout << "Successfully resumed I/O on: " << mntDirIter->c_str() << endl;
                }
                else
                {
                    cout << "ERROR: Failed to resume I/O on: " << mntDirIter->c_str() << endl;
                }
            }
#ifdef VXFSVACP
            else if (strcmp(fsVolInfo.fsType.c_str(), "vxfs") == 0 )
                unlockvxfs(fsVolInfo);
#endif
            else
                cout << "Filesystem consistency not supported for mount point: " << mntDirIter->c_str() << " of type " << fsVolInfo.fsType.c_str() << "." << endl;
        }
	}
}
bool IsSystemDir(const std::string & mountPoint)
{
  //For Solaris, the / and /var FileSystems should not be freezed.
    if( (mountPoint == "/") || (mountPoint == "/var") )
    {
        return true;
    }
    return false;
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

//
// Function: GetFSTagSupportedolumes
//   Gets the list of volumes on which file system consistency tag can be issued
//   Currently, we can issue filesystem consistency tag only if the
//   volume is mounted and the filesystem is a journaled filesystem
//
//   TODO: we are not verifying the filesystem for now
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
#ifdef VXFSVACP
	    if ((strcmp(entry->mnt_fstype, "ufs") == 0) || (strcmp(entry->mnt_fstype, "vxfs") == 0))
#else
	    if (strcmp(entry->mnt_fstype, "ufs") == 0)
#endif
		
		{
            // if (!isAcctLogDir(entry->mnt_mountp))
                supportedFsVolumes.insert(devname);
        }
         // else
           // cout << "Filesystem consistency not supported for mount point " << entry->mnt_mountp << " of type " << entry->mnt_fstype << "." << endl;
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
#ifdef VXFSVACP
	    if ((strcmp(entry->mnt_fstype, "ufs") == 0) || (strcmp(entry->mnt_fstype, "vxfs") == 0))
#else
	    if (strcmp(entry->mnt_fstype, "ufs") == 0)
#endif
	    {
            if ((!isAcctLogDir(entry->mnt_mountp)) && (!IsSystemDir(entry->mnt_mountp)))
            {
                mntDirs.insert(entry->mnt_mountp);
                fsVolInfoMap.insert(pair<std::string, struct FsVolInfo>(entry->mnt_mountp,FsVolInfo(devname, entry->mnt_fstype, entry->mnt_mountp))); 
            }
		    else
		    {
                cout << endl << "Process/task/flow accounting is being logged to " << entry->mnt_mountp << "." << endl
			    << "It will be excluded from filesystem consistency." << endl;
		    }
	    }
         // else
           //  cout << "Filesystem consistency not supported for mount point " << entry->mnt_mountp << " of type " << entry->mnt_fstype << "." << endl;
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
            if (!strcmp(token, "/") == 0 )
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
				cout << token << " is not a valid device/mount point\n";
				break;
			}
		}

	} while(0);

	return rv;
}
