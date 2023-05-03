#include<stdio.h>
#include <mntent.h>


#include "file.h"
#include <sstream>
#include "unixvirtualvolume.h"
#include "hostagenthelpers_ported.h"
#include "globs.h"
#include "basicio.h"
#include "cdputil.h"
#include "configurevxagent.h"
#include <ace/Process_Manager.h>
#include "configurator2.h"
#include <fstream>
#include <sys/mount.h>
#include <mntent.h>
#include "svtypes.h"
#include "inmsafeint.h"
#include "inmageex.h"
#include <libgen.h>
#define SV_PROC_MNTS "/etc/mtab"


bool IsValidMntEnt(struct mntent *entry);

void VirVolume:: process_proc_mounts(std::vector<volinfo> &fslist)
{
    FILE *fp;
	struct mntent *entry = NULL;
	mntent ent;
	char buffer[MAXPATHLEN*4];
	
	fp = setmntent(SV_PROC_MNTS, "r");

	if (fp == NULL)
	{
        printf("Could not able to open %s \n", setmntent);
        return;
	}

	while( (entry = getmntent_r(fp, &ent, buffer, sizeof(buffer))) != NULL )
	{
        volinfo vol;

        if (!IsValidMntEnt(entry))
            continue;

        if (!IsValidDevfile(entry->mnt_fsname))
            continue;

        vol.devname = entry->mnt_fsname;
        vol.mountpoint = entry->mnt_dir;
		vol.fstype = entry->mnt_type;
		vol.devno = GetDevNumber(vol.devname);
		vol.mounted = true;

		fslist.push_back(vol);
	}
	if ( NULL != fp )
	{
        endmntent(fp);
	}
}


/*Function to mount a volume from the sparse file*/

bool VirVolume::mountsparsevolume(const char* sparsefile,const char* volume,std::string &device )
{
	ACE_HANDLE  hVVCtrlDevice = ACE_INVALID_HANDLE;
	hVVCtrlDevice= createvirtualvolume(); //Function to load driver

	if(hVVCtrlDevice==ACE_INVALID_HANDLE)
	{
		DebugPrintf(SV_LOG_ERROR,"Unable to load the volpack driver,Error Coe= %d\n",ACE_OS::last_error());
		return false;
	}

	char*			Buffer = NULL;

	int bufsize = sizeof(SV_USHORT) + (2 * sizeof(int));
	int offset = sizeof(SV_USHORT);

	INM_SAFE_ARITHMETIC(bufsize += InmSafeInt<size_t>::Type(strlen(sparsefile)) + strlen(volume) + 1, INMAGE_EX(bufsize)(strlen(sparsefile))(strlen(volume)))
    size_t buflen;
    INM_SAFE_ARITHMETIC(buflen = InmSafeInt<int>::Type(bufsize) + MAX_STRING_SIZE_FOR_VOLPACK, INMAGE_EX(bufsize))
	Buffer =(char *) calloc(buflen, 1);

	if(!Buffer)
	{
		DebugPrintf(SV_LOG_ERROR,"Unbale to allocate memory to Buffer\n");
		return false;
	}

	*(SV_USHORT *)Buffer = bufsize;
	*(int *)(Buffer + offset) = strlen(volume);
	offset += sizeof(int);
	inm_memcpy_s(Buffer + offset, bufsize + MAX_STRING_SIZE_FOR_VOLPACK - offset, volume, strlen(volume));
	Buffer[offset + strlen(volume) + 1] = '\0';
	offset += strlen(volume);
	*(int *)(Buffer + offset) = strlen(sparsefile);
	offset += sizeof(int);
	inm_memcpy_s(Buffer + offset, bufsize + MAX_STRING_SIZE_FOR_VOLPACK - offset, sparsefile, strlen(sparsefile));
	Buffer[offset + strlen(sparsefile) + 1]='\0';

	if (ioctl(hVVCtrlDevice, IOCTL_INMAGE_ADD_VIRTUAL_VOLUME_FROM_FILE, (SV_ULONG)Buffer))
	{
		free(Buffer);
		close(hVVCtrlDevice);
		DebugPrintf(SV_LOG_ERROR,"The mount ioctl IOCTL_INMAGE_ADD_VIRTUAL_VOLUME_FROM_FILE failed,Error Code= %d \n",ACE_OS::last_error());
		return false;
	}
	//	RegisterHost(); This shud be called after writing the volpack entries into drscout.conf, from PersistVirtualVolume()

        /**
        * NEEDTODISCUSS:
        *
        * There is a problem here. Driver has to supply the out name with a '\0'.
        * otherwise there will be a crash here.
        *
        */

	volume=Buffer + bufsize;
	device=volume;
	DebugPrintf(SV_LOG_INFO,"The devicefile %s has been created succesfully for %s \n",volume,sparsefile);

	free(Buffer);
	close(hVVCtrlDevice);
	return true;
}


bool VirVolume::UnLinkDskAndRDskIfRequired(const std::string dskname)
{
    return true;
}
