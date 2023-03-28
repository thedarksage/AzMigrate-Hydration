#include<stdio.h>
#include<cerrno>
#include "file.h"
#include <sstream>
#include "unixvirtualvolume.h"
#include "hostagenthelpers_ported.h"
#include "globs.h"
#include "basicio.h"
#include "cdputil.h"
#include "inmcommand.h"
#include "configurevxagent.h"
#include <ace/Process_Manager.h>
#include "configurator2.h"
#include <fstream>
//#include <sys/mount.h>
#include "svtypes.h"
#include <libgen.h>
#include "portablehelpersmajor.h"
#include "portablehelpersminor.h"
#include "inmsafeint.h"
#include "inmageex.h"
#include <cf.h>
#include <sys/cfgodm.h>
#include <sys/cfgdb.h>
#include <odmi.h>


void VirVolume:: process_proc_mounts(std::vector<volinfo> &fslist)
{
}



/*Function to mount a volume from the sparse file*/

bool VirVolume::mountsparsevolume(const char* sparsefile,const char* volume,std::string &device )
{
    bool needcleanup = false;
    bool rv = true;
    ACE_HANDLE  hVVCtrlDevice = ACE_INVALID_HANDLE;
    char*			Buffer = NULL;
    do
    {
        //open the vsnap control device
        hVVCtrlDevice= createvirtualvolume(); 

        if(hVVCtrlDevice==ACE_INVALID_HANDLE)
        {
            DebugPrintf(SV_LOG_ERROR,"Unable to load the volpack driver,Error Coe= %d\n",ACE_OS::last_error());
            rv = false;
            break;
        }

        //filling the buffer need to send to driver
        
        int bufsize = sizeof(SV_USHORT) + (2 * sizeof(int));
        int offset = sizeof(SV_USHORT);

        INM_SAFE_ARITHMETIC(bufsize += InmSafeInt<size_t>::Type(strlen(sparsefile)) + strlen(volume) + 1, INMAGE_EX(bufsize)(strlen(sparsefile))(strlen(volume)))
        size_t buflen;
        INM_SAFE_ARITHMETIC(buflen = InmSafeInt<int>::Type(bufsize) + MAX_STRING_SIZE_FOR_VOLPACK, INMAGE_EX(bufsize))
        Buffer =(char *) calloc(buflen, 1);

        if(!Buffer)
        {
            DebugPrintf(SV_LOG_ERROR,"Unbale to allocate memory to Buffer\n");
            rv = false;
            break;
        }

        *(SV_USHORT *)Buffer = bufsize;
        int strlenofvol = strlen(volume);
        inm_memcpy_s((Buffer + offset), bufsize + MAX_STRING_SIZE_FOR_VOLPACK - offset, &strlenofvol, sizeof (int));
        offset += sizeof(int);
        inm_memcpy_s(Buffer + offset, bufsize + MAX_STRING_SIZE_FOR_VOLPACK - offset, volume, strlen(volume));
        Buffer[offset + strlen(volume) + 1] = '\0';
        offset += strlen(volume);
        int strlenofsparsefile = strlen(sparsefile);
        inm_memcpy_s((Buffer + offset), bufsize + MAX_STRING_SIZE_FOR_VOLPACK - offset, &strlenofsparsefile, sizeof (int));
        offset += sizeof(int);
        inm_memcpy_s(Buffer + offset, bufsize + MAX_STRING_SIZE_FOR_VOLPACK - offset, sparsefile, strlen(sparsefile));
        Buffer[offset + strlen(sparsefile) + 1]='\0';

        //create ioctl call for virtualvolume

        if (ioctl(hVVCtrlDevice, IOCTL_INMAGE_ADD_VIRTUAL_VOLUME_FROM_FILE, (SV_ULONG)Buffer))
        {
            DebugPrintf(SV_LOG_ERROR,"The mount ioctl IOCTL_INMAGE_ADD_VIRTUAL_VOLUME_FROM_FILE failed,Error Code= %d \n",ACE_OS::last_error());
            rv = false;
            break;
        }
        //Extracting minor from buffer and the major from control device
        volume=Buffer + bufsize;
        device=volume;
        DebugPrintf(SV_LOG_DEBUG,"Devicename: %s\n",device.c_str());
        std::string devicedir = dirname_r(device.c_str());
        std::string devName = basename_r(device.c_str());
        std::string rdevice = devicedir + "/r";
        rdevice += devName;
        std::string minorstr = (Buffer + bufsize + 12);
        DebugPrintf(SV_LOG_DEBUG,"Minor: %s\n",minorstr.c_str());
        int minor = atoi(minorstr.c_str());
        struct stat statbuf;
        if(stat("/dev/vsnapcontrol",&statbuf))
        {
            DebugPrintf(SV_LOG_ERROR,"failed to start on /dev/vsnapcontrol,Error Code= %d \n",ACE_OS::last_error());
            rv = false;
            break;
        }
        int major = major64(statbuf.st_rdev);
        DebugPrintf(SV_LOG_DEBUG,"Major: %d\n",major);
        //creating dev_t
        dev_t devnumber = makedev64(major,minor);
        //doing mknode for block device and char device
        if(0 != mknod(device.c_str(), S_IFBLK, devnumber))
        {
            DebugPrintf(SV_LOG_ERROR,"The mknode failed for %s,Error Code= %d \n",device.c_str(),ACE_OS::last_error());
            needcleanup = true;
            break;
        }
        if(0 != mknod(rdevice.c_str(), S_IFCHR, devnumber))
        {
            DebugPrintf(SV_LOG_ERROR,"The mknode failed for %s,Error Code= %d \n",rdevice.c_str(),ACE_OS::last_error());
            needcleanup = true;
            break;
        }
        //registering the minor number
        int * new_minor;
		if(-1 == odm_initialize())
		{
			DebugPrintf(SV_LOG_INFO,"Failed to initialize ODM for device %s.\n",device.c_str());
			needcleanup = true;
			break;
		}
        new_minor =genminor((char*)(device.c_str()), major, minor, 1, 0, 0);
		if(-1 == odm_terminate())
		{
			DebugPrintf(SV_LOG_INFO,"Failed to terminate ODM for device %s.\n",device.c_str());
		}
		if((!new_minor) || (*new_minor != minor) )
        {
            DebugPrintf(SV_LOG_DEBUG,"Genmior returned %d\n", *new_minor);
            DebugPrintf(SV_LOG_INFO,"Failed to register the minor number for %s\n",device.c_str());
            needcleanup = true;
            break;
        }
        //registering the device
        std::string dev_cmd = MKDEV_CMD;
        dev_cmd += devName;
        InmCommand mkdevcmd(dev_cmd);

        InmCommand::statusType dev_status = mkdevcmd.Run();

        if (dev_status != InmCommand::completed)
        {
            DebugPrintf(SV_LOG_ERROR,"The mkdev failed for %s,Error Code= %d \n",device.c_str(),ACE_OS::last_error());
            needcleanup = true;
            break;
        }
        if (mkdevcmd.ExitCode())
        {
            DebugPrintf(SV_LOG_ERROR,"The mkdev failed for %s,Error Code= %d \n",device.c_str(),ACE_OS::last_error());
            needcleanup = true;
            break;
        }
    } while(false);
    if(needcleanup)
    {
        UnmountVirtualVolume(hVVCtrlDevice,device);
        rv = false;
    }
    if(Buffer)
        free(Buffer);
    if(hVVCtrlDevice !=ACE_INVALID_HANDLE)
        close(hVVCtrlDevice);
    if(rv)
        DebugPrintf(SV_LOG_INFO,"The devicefile %s has been created succesfully for %s\n",device.c_str(),sparsefile);

    return rv;
}


bool VirVolume::UnLinkDskAndRDskIfRequired(const std::string device)
{
    bool rv = true;
    do
    {
        //unregistering the device
        std::string devName = basename_r(device.c_str());
        std::string rdevice = AIX_RDEVICE_PREFIX;
        rdevice += devName;
        std::string rmdevcmdstr = RMDEV_CMD;
        rmdevcmdstr += devName;
        rmdevcmdstr += RMDEV_CMD_PREFIX;

        InmCommand rmdevcmd(rmdevcmdstr);

        InmCommand::statusType dev_status = rmdevcmd.Run();

        if (dev_status != InmCommand::completed)
        {
            DebugPrintf(SV_LOG_ERROR,"The rmdev failed for %s,Error Code= %d \n",device.c_str(),ACE_OS::last_error());
        }

        if (rmdevcmd.ExitCode())
        {
            DebugPrintf(SV_LOG_ERROR,"The rmdev failed for %s,Error Code= %d \n",device.c_str(),ACE_OS::last_error());
        }
        //releasing the minor
		if(-1 == odm_initialize())
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to initialize ODM for %s\n",device.c_str());
		}
        if(reldevno((char *)device.c_str(),true) != 0)
        {
            DebugPrintf(SV_LOG_ERROR,"The reldevno failed for %s,Error Code= %d \n",device.c_str(),ACE_OS::last_error());
        }
		if(-1 == odm_terminate())
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to terminate ODM for %s.\n",device.c_str());
		}
        //deleting the device
        unlink(device.c_str());
        unlink(rdevice.c_str());
    }while(false);
    return rv;
}
