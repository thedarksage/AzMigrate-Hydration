#include<stdio.h>
#include<cerrno>
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
#include "svtypes.h"
#include <libgen.h>
#include "portablehelpersmajor.h"
#include "portablehelpersminor.h"
#include "inmsafeint.h"
#include "inmageex.h"


void VirVolume:: process_proc_mounts(std::vector<volinfo> &fslist)
{
	FILE *fp;
	struct mnttab *entry = NULL;
	char buffer[MAXPATHLEN*4];
	fp = fopen(SV_PROC_MNTS, "r");

	if (fp == NULL)
	{
		printf("Could not able to open %s \n", SV_PROC_MNTS);
		return;
	}

    resetmnttab(fp);
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
	//*(int *)(Buffer + offset) = strlen(volume);
        int strlenofvol = strlen(volume);
        inm_memcpy_s((Buffer + offset), bufsize + MAX_STRING_SIZE_FOR_VOLPACK - offset, &strlenofvol, sizeof (int));
	offset += sizeof(int);
	inm_memcpy_s(Buffer + offset, bufsize + MAX_STRING_SIZE_FOR_VOLPACK - offset, volume, strlen(volume));
	Buffer[offset + strlen(volume) + 1] = '\0';
	offset += strlen(volume);
	//*(int *)(Buffer + offset) = strlen(sparsefile);
        int strlenofsparsefile = strlen(sparsefile);
        inm_memcpy_s((Buffer + offset), bufsize + MAX_STRING_SIZE_FOR_VOLPACK - offset, &strlenofsparsefile, sizeof (int));
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
	volume=Buffer + bufsize;
	device=volume;
	free(Buffer);
	close(hVVCtrlDevice);

    const std::string volpackdevicesprefix = "/devices/pseudo/linvsnap@0:volpack";
    const std::string volpackdeviceexcludingminor = "/dev/volpack/dsk/volpack";
    const std::string rdevicesuffix = ",raw";

    size_t fixedsize = strlen(volpackdeviceexcludingminor.c_str());
    if (strncmp(device.c_str(), volpackdeviceexcludingminor.c_str(), fixedsize))
    {
        DebugPrintf(SV_LOG_ERROR,"@LINE %d in FILE %s, device name from IOCTL IOCTL_INMAGE_ADD_VIRTUAL_VOLUME_FROM_FILE is not valid\n", __LINE__, __FILE__);
        return false;
    }

    std::string minor = device.substr(fixedsize);

    DebugPrintf(SV_LOG_DEBUG,"@LINE %d in FILE %s, the device name returned by driver is %s\n", __LINE__, __FILE__, device.c_str());
    DebugPrintf(SV_LOG_DEBUG,"@LINE %d in FILE %s, the minor returned is %s\n", __LINE__, __FILE__, minor.c_str());

    const std::string zero = "0";

    if (zero == minor)
    {
        DebugPrintf(SV_LOG_ERROR, "@ LINE %d in FILE %s minor returned is zero\n", LINE_NO, FILE_NAME);
        return false;
    }

    std::string devicesvolpack = volpackdevicesprefix;
    devicesvolpack += minor;

    std::string rdevicesvolpack = devicesvolpack;
    rdevicesvolpack += rdevicesuffix;

    std::string rdskvolpack = GetRawVolumeName(device);

    const char* volpackdskdir = "/dev/volpack/dsk/";
    SVERROR rc = SVMakeSureDirectoryPathExists(volpackdskdir);
    if (rc.failed())
    {
        DebugPrintf(SV_LOG_ERROR, "FAILED  SVMakeSureDirectoryPathExists %s :  %s \n",volpackdskdir,rc.toString());
        return false;
    }

    const char* volpackrdskdir = "/dev/volpack/rdsk/";
    SVERROR rdskrc = SVMakeSureDirectoryPathExists(volpackrdskdir);
    if (rdskrc.failed())
    {
        DebugPrintf(SV_LOG_ERROR, "FAILED  SVMakeSureDirectoryPathExists %s :  %s \n",volpackrdskdir,rdskrc.toString());
        return false;
    }
      
    if (LinkExists(device))
    {
        DebugPrintf(SV_LOG_DEBUG, "@ LINE %d in FILE %s unlinking existing link %s\n", LINE_NO, FILE_NAME, device.c_str());
        unlink(device.c_str());
    }

    if (LinkExists(rdskvolpack))
    {
        DebugPrintf(SV_LOG_DEBUG, "@ LINE %d in FILE %s unlinking existing link %s\n", LINE_NO, FILE_NAME, rdskvolpack.c_str());
        unlink(rdskvolpack.c_str());
    }

    int dsksymlinkretval;
    int rdsksymlinkretval;

    dsksymlinkretval = symlink(devicesvolpack.c_str(), device.c_str());
 
    if (dsksymlinkretval)
    {
        DebugPrintf(SV_LOG_ERROR,"@LINE %d in FILE %s, symlink call to target file %s failed with errno = %d\n", 
                    __LINE__, __FILE__, devicesvolpack.c_str(), errno);
        return false;
    }

    rdsksymlinkretval = symlink(rdevicesvolpack.c_str(), rdskvolpack.c_str());
        
    if (rdsksymlinkretval)
    {
        DebugPrintf(SV_LOG_ERROR,"@LINE %d in FILE %s, symlink call to target file %s failed with errno = %d\n", __LINE__, __FILE__, 
                    rdevicesvolpack.c_str(), errno);
        unlink(device.c_str());
        return false;
    }

    DebugPrintf(SV_LOG_INFO,"The devicefile %s has been created succesfully for %s\n",device.c_str(),sparsefile);
    return true;
}


bool VirVolume::UnLinkDskAndRDskIfRequired(const std::string dskname)
{
    std::string rdskname = GetRawVolumeName(dskname);
    int unlinkrvalondsk = -1; 
    int unlinkrvalonrdsk = -1; 
    bool bretval = false;

    unlinkrvalondsk = unlink(dskname.c_str());
    unlinkrvalonrdsk = unlink(rdskname.c_str());

    if ((0 == unlinkrvalondsk) && (0 == unlinkrvalonrdsk))  
    {
        bretval = true;
    }
    else
    {
        DebugPrintf(SV_LOG_ERROR,"@LINE %d in FILE %s, unlink on %s failed\n", __LINE__, __FILE__, dskname.c_str());
    }

    return bretval;
}
