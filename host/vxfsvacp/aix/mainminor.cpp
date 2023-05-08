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


using namespace std;
//
// Function: GetFSTagSupportedVolumes
//   Gets the list of volumes on which file system consistency tag can be issued
//   Currently, we can issue filesystem consistency tag only if the
//   volume is mounted and the filesystem is a journaled filesystem
//
//   TODO: we are not verifying the filesystem for now

set<string> volMntDirs;  //Set of mount dirs to be freezed
//bool GetMountPointsForVolumes(const set<string> &, set<string> &);
bool  lockfs(const char *fn, SV_USHORT lock = FSCNTL_THAW)
{
	char cmd_buf[PATH_MAX + 100];
	char mount_point[PATH_MAX + 1];
	int fd_mnt, read_len;
	struct stat mnt_stat;

	inm_sprintf_s(mount_point, ARRAYSIZE(mount_point), "/tmp/mount_point%d", getpid());
	printf("The temp file for mount point is %s\n", mount_point);
	inm_sprintf_s(cmd_buf, ARRAYSIZE(cmd_buf), "mount | grep  -w '%s' | grep -v grep | awk -F' ' '{print $2}' > %s", fn, mount_point);
	system(cmd_buf);
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
			printf("The mount point is %s A, read_len = %d\n", cmd_buf, read_len);
			close(fd_mnt);
			unlink(mount_point);

			if(read_len)
			{
				stat(cmd_buf, &mnt_stat);
				printf("mode = %x, st_type = %u, VDIR = %u, st_vfs = %u\n", mnt_stat.st_mode, mnt_stat.st_type, VDIR, mnt_stat.st_vfs);
	
				if (lock == FSCNTL_FREEZE)
				{
					if(fscntl(mnt_stat.st_vfs, FSCNTL_FREEZE, (char *)120, 0))
					{
						printf("failed to freeze the filesystem mounted at %s\n", cmd_buf);
						return false;
					}
					printf("Frozen successfully\n");
				}
				else
				{
					if(fscntl(mnt_stat.st_vfs, FSCNTL_THAW, 0, 0))
					{
						printf("Failed to thaw the filesystem mounted at %s\n", cmd_buf);
						return false;;
					}
					printf("Unfrozen successfully\n");
				}

			}
}

void FreezeDevices(const set<string> &inputVolumes)
{
     cout << "\nStarting device I/O suspension...\n";
	 //oGetMountPointsForVolumes(inputVolumes, volMntDirs);
	volMntDirs = inputVolumes;
	 set<string>::const_iterator deviceIter = volMntDirs.begin();
     for(; deviceIter != volMntDirs.end(); deviceIter++)
     {
         if ( lockfs(deviceIter->c_str(), FSCNTL_FREEZE))
         {
             cout << "Successfully suspended I/O on the device: " << deviceIter->c_str() << endl;
         }
         else
         {
                 cout << "ERROR: Failed to suspend I/O on the device: " << deviceIter->c_str() << endl;
         }
     }

}

void ThawDevices(const set<string> &inputVolumes)
{
     cout << "\nStarting device I/O Resumtion...\n";
	 set<string>::const_iterator deviceIter = volMntDirs.begin();
     for(; deviceIter != volMntDirs.end(); deviceIter++)
     {
         if ( lockfs(deviceIter->c_str()))
         {
             cout << "Successfully resumed I/O on the device: " << deviceIter->c_str() << endl;
         }
         else
         {
             cout << "ERROR: Failed to resume I/O on the device: " << deviceIter->c_str() << endl;
         }
     }
}

bool GetFSTagSupportedolumes(std::set<std::string> & supportedFsVolumes)
{
	bool rv  = true;

	return rv;
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
