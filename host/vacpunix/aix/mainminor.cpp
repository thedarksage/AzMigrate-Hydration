#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <unistd.h>
#include <netinet/in.h>

#include <sys/stat.h>
#include <sys/vnode.h>
#include <sys/types.h>
#include <j2/j2_cntl.h>
#include <sys/vmount.h>


#include <iostream>
#include <vector>
#include <set>
#include <string>
#include "portablehelpers.h"
#include "portablehelpersminor.h"
#include "volumeinfocollector.h"
#include "volumereporter.h"
#include "volumereporterimp.h"
#include "StreamEngine.h"
#include "inmsafecapis.h"

#ifdef VXFSVACP
#include "sys/fs/vx_ioctl.h"
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
bool GetMountPointsForVolumes(const set<string> &, set<string> &);

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

bool lockfs(const char *fn, SV_USHORT lock = FSCNTL_THAW)
{
	struct stat mnt_stat;
    int ret_val;

    ret_val = stat(fn, &mnt_stat);
    if (ret_val != 0)
    {
        perror("stat");
        printf("stat failed for %s\n", fn);
        return false;
    }

    // printf("mode = %x, st_type = %u, VDIR = %u, st_vfs = %u\n", mnt_stat.st_mode, mnt_stat.st_type, VDIR, mnt_stat.st_vfs);

    if (lock == FSCNTL_FREEZE)
    {
        ret_val = fscntl(mnt_stat.st_vfs, FSCNTL_FREEZE, (char *)120, 0);
        if(ret_val != 0)
        { 
            perror(fn);
            printf("failed to freeze the filesystem mounted at %s\n", fn);
            return false;
        }
        // printf("Frozen successfully\n");
    }
    else
    {
        if(fscntl(mnt_stat.st_vfs, FSCNTL_THAW, 0, 0))
        {
            perror(fn);
            printf("Failed to thaw the filesystem mounted at %s\n", fn);
            return false;;
        }
        // printf("Unfrozen successfully\n");
    }

    return true;
}

void FreezeDevices(const set<string> &inputVolumes)
{
    cout << "\nStarting device I/O suspension...\n";

    if (!GetMountPointsForVolumes(inputVolumes, volMntDirs))
    {
        cout << "Failed to get mounted directories" << endl;
        return;
    }

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
	std::vector<string>::const_iterator mpIter = vecVolMntDirs.begin();
    for(; mpIter != vecVolMntDirs.end(); mpIter++)
    {
        std::map<std::string, struct FsVolInfo >::iterator iter = fsVolInfoMap.find(mpIter->c_str());
        if(iter != fsVolInfoMap.end())
        {
            struct FsVolInfo & fsVolInfo= iter->second;
            if (strcmp(fsVolInfo.fsType.c_str(), "jfs2") == 0 )
            {
                if (lockfs(mpIter->c_str(), FSCNTL_FREEZE))
                {
                    cout << "Successfully suspended I/O on filesystem: " << mpIter->c_str() << endl;
                }
                else
                {
                    cout << "ERROR: Failed to suspend I/O on filesystem: " << mpIter->c_str() << endl;
                }
            }

#ifdef VXFSVACP
            if (strcmp(fsVolInfo.fsType.c_str(), "vxfs") == 0 )
            {
                lockvxfs(fsVolInfo);
            }
#endif
        }
    }
}

bool IsSystemDir(const std::string & mountPoint)
{
    if( (mountPoint == "/") || (mountPoint == "/usr") || (mountPoint == "/var") || (mountPoint == "/tmp") )
    {
        return true;
    }
    return false;
}

bool GetMountPointForDevice(const std::string &devName, std::string &mntDir)
{
    char cmd_buf[PATH_MAX + 100];
    char mount_point[PATH_MAX + 1];
    int fd_mnt, read_len;
    struct stat mnt_stat;
    int ret_val;

	inm_sprintf_s(mount_point, ARRAYSIZE(mount_point), "/tmp/mount_point%d", getpid());
	inm_sprintf_s(cmd_buf, ARRAYSIZE(cmd_buf), "mount | grep -w '%s' | awk '{print $2}' > %s", devName.c_str(), mount_point);

    ret_val = system(cmd_buf);
    if (ret_val < 0)
    {
        printf("%s returned error %d\n", cmd_buf, ret_val);
        unlink(mount_point);
        return false;
    }

    if (ret_val != 0)
    {
        printf("shell returned %d for %s\n", ret_val, cmd_buf);
        unlink(mount_point);
        return false;
    }
        
    fd_mnt = open(mount_point, O_RDONLY, 0666);
    if(fd_mnt < 0)
    {
        printf("Error in opening the file %s\n", mount_point);
        unlink(mount_point);
        return false;;
    }

    read_len = read(fd_mnt, cmd_buf, PATH_MAX);
    if(read_len < 0){
        printf("Error in reading the file %s\n", mount_point);
        close(fd_mnt);
        unlink(mount_point);
        return false;;
    }

    if(cmd_buf[read_len - 1] == '\n')
    read_len--;

    cmd_buf[read_len] = '\0';
    // printf("The mount point is %s A, read_len = %d\n", cmd_buf, read_len);
    close(fd_mnt);
    unlink(mount_point);

    std::string mountPoint = cmd_buf;

    if (IsSystemDir(mountPoint))
    {
        cout << "Found system dir " << mountPoint << " on device " << devName << endl;
        return false;
    }

    mntDir = mountPoint; 

    return true;
}

bool GetFsType(std::string devName, std::string &fsType)
{
    char cmd_buf[PATH_MAX + 100];
    char mount_point[PATH_MAX + 1];
    int fd_mnt, read_len;
    struct stat mnt_stat;
    int ret_val;

	inm_sprintf_s(mount_point, ARRAYSIZE(mount_point), "/tmp/fstype%d", getpid());
	inm_sprintf_s(cmd_buf, ARRAYSIZE(cmd_buf), "mount | grep -w '%s' | awk '{print $3}' > %s", devName.c_str(), mount_point);

    ret_val = system(cmd_buf);
    if (ret_val < 0)
    {
        printf("%s returned error %d\n", cmd_buf, ret_val);
        unlink(mount_point);
        return false;
    }

    if (ret_val != 0)
    {
        printf("shell returned %d for %s\n", ret_val, cmd_buf);
        unlink(mount_point);
        return false;
    }
        
    fd_mnt = open(mount_point, O_RDONLY, 0666);
    if(fd_mnt < 0)
    {
        printf("Error in opening the file %s\n", mount_point);
        unlink(mount_point);
        return false;;
    }

    read_len = read(fd_mnt, cmd_buf, PATH_MAX);
    if(read_len < 0){
        printf("Error in reading the file %s\n", mount_point);
        close(fd_mnt);
        unlink(mount_point);
        return false;;
    }

    if(cmd_buf[read_len - 1] == '\n')
    read_len--;

    cmd_buf[read_len] = '\0';
    // printf("The mount point is %s A, read_len = %d\n", cmd_buf, read_len);
    close(fd_mnt);
    unlink(mount_point);

    fsType = cmd_buf;

    return true;
}

bool GetMountPointsForVolumes(const std::set<std::string> & fsVolumes, std::set<std::string> & mntDirs)
{
	char cmd_buf[PATH_MAX + 100];
	char mount_point[PATH_MAX + 1];
	FILE *fp_mnt;
    char fline[PATH_MAX];
    int ret_val;

	inm_sprintf_s(mount_point, ARRAYSIZE(mount_point), "/tmp/mount_point%d", getpid());
	inm_sprintf_s(cmd_buf, ARRAYSIZE(cmd_buf), "mount | grep -v 'mounted over' | grep -v '-' | awk '{print $1}' > %s", mount_point);

	ret_val = system(cmd_buf);
    if (ret_val < 0)
    {
		printf("%s returned error %d\n", cmd_buf, ret_val);
		unlink(mount_point);
		return false;;
    }

    if (ret_val != 0)
    {
		printf("shell returned %d for %s\n", ret_val, cmd_buf);
		unlink(mount_point);
		return false;;
    }
        

	fp_mnt = fopen(mount_point, "r");
	if(fp_mnt == NULL)
	{
        perror(mount_point);
		unlink(mount_point);
		return false;;
	}

    while ( fgets ( fline, sizeof fline, fp_mnt) != NULL )
    {
        int line_len = strlen(fline);        

        if (fline[line_len-1] == '\n')
            line_len--;

        fline[line_len] = '\0';

		std::string devname(fline,line_len);

        if (!IsValidDevfile(devname))
		    continue;

        if(!GetDeviceNameFromSymLink(devname))
            continue;

	   	if(fsVolumes.find(devname) != fsVolumes.end())
        {
            std::string mountDir;
            if (GetMountPointForDevice(devname, mountDir))
            {
                mntDirs.insert(mountDir);
                std::string fsType;
                if (!GetFsType(devname, fsType))
                {
                    printf("failed to get fstype for %s\n", devname.c_str());
                    unlink(mount_point);
                    return false;;
                }

                fsVolInfoMap.insert(pair<std::string, struct FsVolInfo>(mountDir, FsVolInfo(devname, fsType, mountDir))); 
            }
        }
    }

    fclose(fp_mnt);
    unlink(mount_point);
    return true;
}

void ThawDevices(const set<string> &inputVolumes)
{
     cout << "\nStarting device I/O Resumtion...\n";
	 set<string>::const_iterator mpIter = volMntDirs.begin();
     for(; mpIter != volMntDirs.end(); mpIter++)
     {
        std::map<std::string, struct FsVolInfo >::iterator iter = fsVolInfoMap.find(mpIter->c_str());
        if(iter != fsVolInfoMap.end())
        {
            struct FsVolInfo & fsVolInfo= iter->second;
            if (strcmp(fsVolInfo.fsType.c_str(), "jfs2") == 0 )
            {
                if (lockfs(mpIter->c_str()))
                {
                    cout << "Successfully resumed I/O on filesystem : " << mpIter->c_str() << endl;
                }
                else
                {
                    cout << "ERROR: Failed to resume I/O on filesystem: " << mpIter->c_str() << endl;
                }
            }

#ifdef VXFSVACP
            if (strcmp(fsVolInfo.fsType.c_str(), "vxfs") == 0 )
            {
                unlockvxfs(fsVolInfo);
            }
#endif

         }
     }
}

// Function: GetFSTagSupportedolumes
//   Gets the list of volumes on which file system consistency tag can be issued
//   Currently, we can issue filesystem consistency tag only if the
//   volume is mounted and the filesystem is a journaled filesystem
//
//   TODO: we are not verifying the filesystem for now
bool GetFSTagSupportedolumes(std::set<std::string> & supportedFsVolumes)
{
	char cmd_buf[PATH_MAX + 100];
	char mount_point[PATH_MAX + 1];
	FILE *fp_mnt;
    char fline[PATH_MAX];
    int ret_val;

    cout << endl << "Volumes belonging to the system directories /, /usr, /var and /tmp " << endl 
         << "will be excluded from filesystem consistency." << endl;

	inm_sprintf_s(mount_point, ARRAYSIZE(mount_point), "/tmp/mount_point%d", getpid());
	inm_sprintf_s(cmd_buf, ARRAYSIZE(cmd_buf), "mount | grep -v 'mounted over' | grep -v '-' | awk '{print $1}' > %s", mount_point);

	ret_val = system(cmd_buf);
    if (ret_val < 0)
    {
		printf("%s returned error %d\n", cmd_buf, ret_val);
		unlink(mount_point);
		return false;;
    }

    if (ret_val != 0)
    {
		printf("shell returned %d for %s\n", ret_val, cmd_buf);
		unlink(mount_point);
		return false;;
    }
        

	fp_mnt = fopen(mount_point, "r");
	if(fp_mnt == NULL)
	{
        perror(mount_point);
		unlink(mount_point);
		return false;;
	}

    while ( fgets ( fline, sizeof fline, fp_mnt) != NULL )
    {
        int line_len = strlen(fline);        

        if (fline[line_len-1] == '\n')
            line_len--;

        fline[line_len] = '\0';

		std::string devname(fline,line_len);

        if (!IsValidDevfile(devname))
		    continue;

        if(!GetDeviceNameFromSymLink(devname))
            continue;

        std::string mountDir;
        if (GetMountPointForDevice(devname, mountDir))
            supportedFsVolumes.insert(devname);
    }

    fclose(fp_mnt);
    unlink(mount_point);
    return true;
}

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
