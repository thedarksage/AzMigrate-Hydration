#include<stdio.h>


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
#ifndef SV_AIX
#include <sys/mount.h>
#endif
#include "svtypes.h"
#include <libgen.h>

#include "portablehelpersmajor.h"
#include "inmsafeint.h"
#include "inmageex.h"

using namespace std;

int WaitForProcessCompletion(ACE_Process_Manager * , pid_t );

VirVolume::VirVolume()
{
    m_TheConfigurator = NULL ;

    try {
        if(!GetConfigurator(&m_TheConfigurator))
        {
            DebugPrintf(SV_LOG_INFO, "Communication with Central Management Server is currently unavailable.\n");
        }
    } 
    catch(...) {
        DebugPrintf(SV_LOG_INFO, "Communication with Central Management Server is currently unavailable.\n");
    }
}

VirVolume::~VirVolume()
{

}

ACE_HANDLE VirVolume::createvirtualvolume()
{
    ACE_HANDLE  hVVCtrlDevice = ACE_INVALID_HANDLE;
    hVVCtrlDevice=OpenVirtualVolumeControlDevice();

    return hVVCtrlDevice;
}


ACE_HANDLE VirVolume::OpenVirtualVolumeControlDevice()
{
    int hDevice = 0;
    char *vsnap_dev = "/dev/vsnapcontrol";

    hDevice = open(vsnap_dev, O_RDONLY, 0);

    return ((ACE_HANDLE)hDevice);
}
SV_ULONGLONG VirVolume::getMaxSizeToResizePartFile(const std::string & filename)
{
    SV_ULONGLONG maxconfigsize = m_TheLocalConfigurator.getSparseFileMaxSize();
    SV_ULONGLONG maxfilesize = 0, currentfilesize =0;
    std::string sparsepartfile = filename;
    sparsepartfile += SPARSE_PARTFILE_EXT;
    sparsepartfile += "0";
    do
    {
        ACE_stat db_stat ={0};
        if(sv_stat(getLongPathName(sparsepartfile.c_str()).c_str(),&db_stat) != 0)
        {
            DebugPrintf(SV_LOG_INFO,"The sparse file %s does not exist.\n",filename.c_str());
            maxfilesize = 0;
            break;
        }
        currentfilesize = db_stat.st_size;
        sparsepartfile.clear();
        sparsepartfile = filename;
        sparsepartfile += SPARSE_PARTFILE_EXT;
        sparsepartfile += "1";
        if(sv_stat(getLongPathName(sparsepartfile.c_str()).c_str(),&db_stat) != 0)
        {
            maxfilesize = max(maxconfigsize,currentfilesize);
        }
        else
        {
            maxfilesize = currentfilesize;
        }        
    }while(0);
    return maxfilesize;
}
bool VirVolume::resize_new_version_sparse_file(std::string filename, SV_ULONGLONG size)
{
    bool rv = true;
    ACE_stat db_stat ={0};
    int i = 0;
    SV_ULONGLONG maxsize = getMaxSizeToResizePartFile(filename);
    stringstream sparsepartfile;
    sparsepartfile << filename << SPARSE_PARTFILE_EXT << i;
    SV_ULONGLONG currentsize = 0;
    SV_ULONGLONG lastFileSize = 0;
    SV_ULONGLONG filesizetoresize = 0;

    do
    {
        if(sv_stat(getLongPathName(sparsepartfile.str().c_str()).c_str(),&db_stat) != 0)
        {
            DebugPrintf(SV_LOG_INFO,"The sparse file %s does not exist.\n",filename.c_str());
            rv = false;
            break;
        }
        //Get the total file count and size.
        while(true)
        {
            currentsize += db_stat.st_size; // The total size of the sparse files.
            lastFileSize = db_stat.st_size; // Size of the last sparse file.
            sparsepartfile.str("");
            i++;
            sparsepartfile << filename << SPARSE_PARTFILE_EXT << i;
            if(sv_stat(getLongPathName(sparsepartfile.str().c_str()).c_str(),&db_stat) != 0)
            {
                break;
            }
        }

        //i-1 will the index of last file.

        std::stringstream sparsefile;

        if(size > currentsize)//Expand the sparse volume.
        {
            //Requested size is greater than the current size, so expand the sparse volume.
            SV_ULONGLONG resize  = size - currentsize;

            if(maxsize - lastFileSize) //There is room in the last file. 
            {
                SV_ULONGLONG filesize = resize > (maxsize - lastFileSize)?maxsize:(lastFileSize + resize);
                sparsefile.str("");
                sparsefile << filename << SPARSE_PARTFILE_EXT << (i-1); //Index of the last file is i-1.

                if(!createsparsefileonresize(sparsefile.str(),filesize))
                {
                    rv = false;
                    break;
                }
                resize -= (filesize - lastFileSize);
            }

            while(resize > 0)
            {
                SV_ULONGLONG filesize = resize > maxsize ? maxsize:resize;
                sparsefile.str("");
                sparsefile << filename << SPARSE_PARTFILE_EXT << i++;

                if(!createsparsefileonresize(sparsefile.str(),filesize))
                {
                    rv = false;
                    break;
                }
                resize -= filesize;
            }
        }
        else if(size < currentsize) // Shrink the existing volume.
        {
            SV_ULONGLONG resize = currentsize - size;

            while(resize >= lastFileSize) //If resize is greater than the lastFileSize remove the last file.
            {
                resize -= lastFileSize;
                //Unlink the last file.
                sparsefile.str("");
                sparsefile << filename << SPARSE_PARTFILE_EXT << --i;
                if(0!=ACE_OS::unlink(getLongPathName(sparsefile.str().c_str()).c_str()))
                {
                    rv = false;
                    DebugPrintf(SV_LOG_ERROR,"Unable to delete the sparse file %s .Manually delete the file.\n",
                        sparsefile.str().c_str());
                    break;
                }
                lastFileSize = maxsize;
            }

            if(rv && (resize > 0)) //Resize the last file.
            {
                sparsefile.str("");
                sparsefile << filename << SPARSE_PARTFILE_EXT << --i;
                if(!createsparsefileonresize(sparsefile.str(),(lastFileSize - resize)))
                {
                    rv = false;                                
                }
            }
        }

    }while(false);
    return rv;

}

bool VirVolume::resizesparsefile(const string &filename, const SV_ULONGLONG &size, InmSparseAttributeState_t sparseattr_state)
{
    bool rv = true;
    ACE_stat db_stat ={0};	
	SV_ULONGLONG filesize = size ;
    SV_ULONGLONG maxsize = m_TheLocalConfigurator.getSparseFileMaxSize();
    bool new_version = false;
    if(maxsize)
        new_version = IsNewSparseFileFormat(filename.c_str());

    do
    {
        if(new_version)
        {
            if(!resize_new_version_sparse_file(filename,filesize))
            {
                rv= false;
                break;
            }
        }
        else
        {
            if(!createsparsefileonresize(filename.c_str(),filesize))
            {
                rv = false;
                break;
            }
        }
        DebugPrintf(SV_LOG_INFO,"Sparse file %s resize to size %lu MB succesfully\n",filename.c_str(),size);
    }while(false);

    return rv;
}

bool VirVolume::createsparsefileonresize(std::string filename, SV_ULONGLONG size)
{
    bool rv = true;
    do
    {
        ACE_HANDLE hHandle = ACE_INVALID_HANDLE;
        umask(S_IWGRP | S_IWOTH);
        hHandle = ACE_OS::open(getLongPathName(filename.c_str()).c_str(),O_WRONLY | O_CREAT,FILE_SHARE_WRITE );
        if(hHandle==ACE_INVALID_HANDLE)
        {
            DebugPrintf(SV_LOG_INFO,"Sparse File creation failed ERROR: %d\n",ACE_OS::last_error());
            rv = false;
            break;
        }
        if( ftruncate(hHandle, size) == -1)
        {
            DebugPrintf(SV_LOG_INFO,"Seeking a sparse file failed: %d\n",ACE_OS::last_error());
            ACE_OS::close(hHandle);
            rv = false;
            break;
        }
        ACE_OS::close(hHandle);
    }while(false);

    return rv;
}

/*This function creates a sparse file*/
bool VirVolume::createsparsefile(const std::string &filename, const SV_ULONGLONG &size, InmSparseAttributeState_t sparseattr_state)
{
    bool rv = true;
    ACE_HANDLE hHandle = ACE_INVALID_HANDLE;
    SV_ULONG   BytesReturned = 0;
    SV_ULONGLONG maxsize = m_TheLocalConfigurator.getSparseFileMaxSize();
    if(m_TheLocalConfigurator.IsVolpackDriverAvailable())
        maxsize = 0;
    int i = 0;


    do
    {
        SV_ULONGLONG filesize = 0 ;
		SV_ULONGLONG pendingsize = size ;
        stringstream sparsepartfile;
        do
        {
            ACE_stat db_stat ={0};
            sparsepartfile.str("");
            sparsepartfile<<filename;
            filesize = pendingsize ;
            if(maxsize > 0 )
            {
                sparsepartfile << SPARSE_PARTFILE_EXT <<i;
                if(pendingsize > maxsize)
                {
                    pendingsize-=maxsize;
                    filesize = maxsize;
                    i++;
                }
                else
                    pendingsize=0;
            }
            else
                pendingsize=0;

            if(sv_stat(getLongPathName(sparsepartfile.str().c_str()).c_str(),&db_stat) == 0)
            {
                DebugPrintf(SV_LOG_INFO,"The file %s is already existing.\n",filename.c_str());
                rv = false;
                break;
            }
            umask(S_IWGRP | S_IWOTH); 
            hHandle = ACE_OS::open(getLongPathName(sparsepartfile.str().c_str()).c_str(),O_WRONLY | O_CREAT | O_EXCL, (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH));
            if(hHandle==ACE_INVALID_HANDLE)
            {
                DebugPrintf(SV_LOG_INFO,"Sparse File creation failed ERROR: %d\n",ACE_OS::last_error());
                rv = false;
                break;
            }

            if( ftruncate(hHandle, filesize) == -1)
            {
                if(ACE_OS::last_error() == EFBIG)
                    DebugPrintf(SV_LOG_INFO,"File size too large. Please provide size less than or equal to MAX FILE SIZE supported by the filesystem.\n");
                else
                    DebugPrintf(SV_LOG_INFO,"Seeking a sparse file failed: %d\n",ACE_OS::last_error());
                ACE_OS::close(hHandle);
                ACE_OS::unlink(getLongPathName(sparsepartfile.str().c_str()).c_str());
                rv = false;
                break;
            }
            ACE_OS::close(hHandle);

        }while(pendingsize > 0);
        if(!rv)
            break;
        DebugPrintf(SV_LOG_INFO,"Sparse file %s of size %lu Bytes created succesfully\n",filename.c_str(),size);
    }while(false);

    return rv;
}

bool VirVolume::unmountmountpoint(std::string &strvolpack)
{
    bool found=false;
    string mountpoint;
    string devicefile;
    vector<volinfo> mtablist;
    process_proc_mounts(mtablist);
    for(int i=0; i<mtablist.size(); i++)
    {
        volinfo m = mtablist[i];
        while(!found)
        {
            if((strvolpack.compare(m.mountpoint)==0)||(strvolpack.compare(m.devname)==0))
            {
                mountpoint=m.mountpoint.c_str();
                devicefile=m.devname.c_str();
                found = true;
                strvolpack = devicefile;	
                break;
            }
            break;
        }

    }

    if(found)
    {
        string s="umount ";
        s+=mountpoint.c_str();
        if(0!=system(s.c_str()))
        {
            DebugPrintf(SV_LOG_ERROR,"The FS mount- %s - corresponding to Volpack- %s - has failed \n ",mountpoint.c_str(),devicefile.c_str());
            return false;
        }
        else
        {
            DebugPrintf(SV_LOG_INFO,"The FS mount- %s - corresponding to Volpack- %s - has been unmounted \n ",mountpoint.c_str(),devicefile.c_str());

            string deleteDir;
            deleteDir ="rm -rf ";
            deleteDir += mountpoint;

            ACE_Process_Manager* pm = ACE_Process_Manager::instance();
            std::ostringstream args;
            ACE_exitcode status = 0;
            ACE_Time_Value timeout;
            ACE_Process_Options options;

            args << deleteDir.c_str() ;
            options.command_line(ACE_TEXT("%s"), args.str().c_str());
            pid_t pid = pm->spawn(options);
            pid_t rc = pm->wait(pid, timeout, &status);
            if ((ACE_INVALID_PID == pid) || (ACE_INVALID_PID == status )) 
            {
                return false;
            }
        }
        return true;
    }
    return false;
}

/*This function informs CX about the creation of Virtual Volumes*/
void VirVolume::sendInformationtoCX()
{
    SVERROR rc;
    rc = RegisterHost();
}

bool VirVolume::VirVolumeCheckForRemount(string &virVolList,bool createlist)
{

    ACE_HANDLE  hDevice =ACE_INVALID_HANDLE;
    SVERROR svError = SVS_OK;

    bool bExist=false;

    SVERROR hr=SVS_OK;

    int counter = m_TheLocalConfigurator.getVirtualVolumesId();
    while(counter!=0)
    {
        char regData[26];
		inm_sprintf_s(regData, ARRAYSIZE(regData), "VirVolume%d", counter);

        string data = m_TheLocalConfigurator.getVirtualVolumesPath(regData);

        std::string sparsefilename;
        std::string volpackname;

        if(!ParseSparseFileNameAndVolPackName(data, sparsefilename, volpackname))
        {
            counter--;
            continue;
        }

        if(createlist)
        {
            virVolList+=volpackname;
            virVolList+=',';
        }

        else
        {
            // PR#10815: Long Path support
            hDevice = ACE_OS::open(getLongPathName(volpackname.c_str()).c_str(),O_RDWR, FILE_SHARE_READ );
            if(hDevice !=ACE_INVALID_HANDLE)
                bExist=true;
            if ( !bExist )
            {
                string device_not_used_here;
                if(m_TheLocalConfigurator.IsVolpackDriverAvailable())
                {
                    bool res=mountsparsevolume(sparsefilename.c_str(),volpackname.c_str(),device_not_used_here);
                    if(res==false)
                    {
                        hr = ACE_OS::last_error();
                        DebugPrintf(SV_LOG_ERROR,"Returned Failed from mount virtual volume, err %d\n", ACE_OS::last_error() );
                        DebugPrintf(SV_LOG_ERROR, "FAILED : Unable to mount virtual volume %s \n ",volpackname.c_str());

                    }
                }
            }
            ACE_OS::close(hDevice);
        }


        counter--;		
    }

    return bExist;
}

bool  VirVolume::UnmountVirtualVolume(ACE_HANDLE  CtrlDevice , string VolumeName,bool bypassdriver)
{

    if(CtrlDevice==ACE_INVALID_HANDLE)
    {
        DebugPrintf(SV_LOG_ERROR,"Unable to load the volpack driver \n");
        return false;
    }

    int bufsize;
    INM_SAFE_ARITHMETIC(bufsize = InmSafeInt<size_t>::Type(sizeof(SV_USHORT)) + strlen(VolumeName.c_str()) + 1, INMAGE_EX(sizeof(SV_USHORT))(strlen(VolumeName.c_str())))
    char *Buffer =(char*) calloc(bufsize, 1);
    int offset = sizeof(SV_USHORT);

    if (!Buffer)
    {
        DebugPrintf("Unable to allocate memory for Buffer \n ");	
        return false;
    }

    *(SV_USHORT *)Buffer = strlen(VolumeName.c_str());
    inm_memcpy_s(Buffer + offset, bufsize - offset, VolumeName.c_str(), strlen(VolumeName.c_str()));
    Buffer[offset + strlen(VolumeName.c_str()) + 1]='\0';
    if(!bypassdriver)
    {
        if (ioctl(CtrlDevice, IOCTL_INMAGE_REMOVE_VIRTUAL_VOLUME_FROM_FILE, (SV_ULONG)Buffer)<0)
        {
            DebugPrintf(SV_LOG_ERROR,"Unmounting the volpack device %s using IOCTL_INMAGE_REMOVE_VIRTUAL_VOLUME_FROM_FILE ioctl failed, Error Code %d \n",VolumeName.c_str(), ACE_OS::last_error());
            free(Buffer);
            return false;
        }
    }
    DebugPrintf(SV_LOG_INFO,"The volpack device %s has been deleted succesfully \n",VolumeName.c_str());

    free(Buffer);
    return true;
}


bool VirVolume::Unmount(string VolumeName,bool checkfortarget,bool bypassdriver)
{

    char				MntPt[2048];
	
    if(checkfortarget)
    {
        if(IsVolpackUsedAsTarget(VolumeName))
            return false;
        }

    bool res = true;
    std::string sparsefile;
    bool isnewvirtualvolume = false;
    if(!IsVolPackDevice(VolumeName.c_str(),sparsefile,isnewvirtualvolume))
    {
        DebugPrintf(SV_LOG_ERROR, "%s is not a virtual volume.\n",
            VolumeName.c_str());
        return false;
    }

    ACE_HANDLE  CtrlDevice = ACE_INVALID_HANDLE;
    inm_strcpy_s(MntPt, ARRAYSIZE(MntPt), VolumeName.c_str());

    if(!bypassdriver)
    {
        CtrlDevice=createvirtualvolume();
        std::string output, error;
        SVERROR sve = HideDrive(MntPt,MntPt, output, error,false);
        if ( sve.failed()  )
        {
            DebugPrintf(SV_LOG_ERROR, "Failed to unmount %s.\n\n", MntPt);
            return false;
        }
        else
        {
            DebugPrintf(SV_LOG_INFO, "%s is successfully unmounted.\n\n",MntPt);
        }
        res=UnmountVirtualVolume(CtrlDevice,MntPt,bypassdriver);
        ACE_OS::close(CtrlDevice);
    }
    UnLinkDskAndRDskIfRequired(VolumeName);
    if(!res)
        return false;
    return true;
}

// Bug 3549 
// Added deletesparse flag which when false causes the sparse file and the drscout.conf entries for 
// virtual volume to be retained (i.e, not deleted) 
bool VirVolume::DeletePersistVirtualVolumes(string volumename,string filename, bool IfExixts, bool deletepersistinfo, bool deletesparse)
{
    if(deletesparse)
    {
        assert(deletepersistinfo == true);
        if(deletepersistinfo == false)
        {
            DebugPrintf(SV_LOG_ERROR, "Getting request for deleting sparse file without deleting the persistinfo. Aborting...\n");
            return false;
        }
    }
    SVERROR svError = SVS_OK;
    char regName[26];
    int counter = m_TheLocalConfigurator.getVirtualVolumesId();	
    while(counter!=0)
    {
		inm_sprintf_s(regName, ARRAYSIZE(regName), "VirVolume%d", counter);

        string data = m_TheLocalConfigurator.getVirtualVolumesPath(regName);

        std::string sparsefilename;
        std::string volpackname;

        if(!ParseSparseFileNameAndVolPackName(data, sparsefilename, volpackname))
        {
            counter--;
            continue;
        }

        if(IfExixts)
        {
            if(strcmp(volpackname.c_str(),volumename.c_str())==0)
            {
                DebugPrintf(SV_LOG_ERROR, "The specified Virtual Volume %s already exists\n",volumename.c_str() );			return false;
            }

            if(strcmp(sparsefilename.c_str(),filename.c_str())==0)
            {
                DebugPrintf(SV_LOG_ERROR,"There is a volume already mounted on the specified SparseFile %s\n",filename.c_str() );			return false;
            }
        }
        else
        {
            // Bug 3549
            // Delete the sparse file and remove the entries from drscout.conf
            // only when deletesparse is true(--softremoval is provided)
            if(deletepersistinfo && strcmp(volpackname.c_str(),volumename.c_str())==0) 
            {
                string new_value="";
                m_TheLocalConfigurator.setVirtualVolumesPath(regName,new_value);
                if(deletesparse)
                    deletesparsefile(sparsefilename.c_str());			
                return true;
            }
        }

        counter--;
    }
    return true;
}


// Bug 3549 
// deletesparsefile is passed which is true by default 
bool VirVolume::UnmountAllVirtualVolumes(bool checkfortarget,bool deletepersistinfo,bool deletesparsefile,bool bypassdriver)
{

    bool rv = true;
    int counter = m_TheLocalConfigurator.getVirtualVolumesId();
    int stalecnt = counter;

    while(counter!=0)
    {
        stringstream regData;
        regData << "VirVolume" << counter;
        string data = m_TheLocalConfigurator.getVirtualVolumesPath(regData.str());	

        std::string sparsefilename;
        std::string volpackname;
        if(!ParseSparseFileNameAndVolPackName(data, sparsefilename, volpackname))
        {
            counter--;
            stalecnt--;
            continue;
        }

        else
        {	
            std::string strvolume;
            strvolume = volpackname;
            bool res=true;
            if( m_TheLocalConfigurator.IsVolpackDriverAvailable())
            {
                unmountmountpoint(strvolume);
                res=Unmount(strvolume,checkfortarget);
            }
            else if(checkfortarget)
            {
                if(IsVolpackUsedAsTarget(strvolume))
                    res = false;
            }
            if(res)

                // Bug 3549 
                // deletesparsefile is passed using which sparsefile and drscout.conf entries are retained 
                res=DeletePersistVirtualVolumes(strvolume,"", false,deletepersistinfo,deletesparsefile); 
            if(!res) 
                rv = false;

        }

        counter--;		
    }

    if(stalecnt == 0)
        DebugPrintf(SV_LOG_INFO,"There are no virtual volumes mounted in the system.\n");

    if(counter!=0)
        rv = false;

    return rv;
}

void VirVolume::deletesparsefile(const char *sparsefile)
{
    // PR#10815: Long Path support

    if(IsNewSparseFileFormat(sparsefile))
    {
        int i = 0;
        std::stringstream sparsepartfile;
        ACE_stat s;
        while(true)
        {
            sparsepartfile.str("");
            sparsepartfile << sparsefile << SPARSE_PARTFILE_EXT << i;
            if( sv_stat( getLongPathName(sparsepartfile.str().c_str()).c_str(), &s ) < 0 )
            {
                break;
            }

            if(0!=ACE_OS::unlink(getLongPathName(sparsepartfile.str().c_str()).c_str()))
            {
                DebugPrintf(SV_LOG_ERROR, "FAILED: Not able to delete the sparse file \n");
                break;
            }
            i++;
        }
    }
    else
    {
        if(0!=ACE_OS::unlink(getLongPathName(sparsefile).c_str()))
            DebugPrintf(SV_LOG_ERROR, "FAILED: Not able to delete the sparse file \n");
    }	
}
bool VirVolume::IsNewSparseFileFormat(const char * filename)
{
    ACE_stat s;
    if( sv_stat( getLongPathName(filename).c_str(), &s ) < 0 )
    {
        return true;
    }
    return false;
}

bool VirVolume::IsVolpackUsedAsTarget(const std::string & volume)
{
    bool rv = false;
    std::string vol_name = volume;
    FormatVolumeName(vol_name);
    if(m_TheConfigurator)
    {
        if(m_TheConfigurator->isTargetVolume(vol_name))
        {
            DebugPrintf(SV_LOG_INFO,"The volume %s can not be unmounted as it is used as target volume\n", volume.c_str());
            rv = true;
        }
    }
    else
    {
        DebugPrintf(SV_LOG_INFO,"Unable to unmount as there is no cache information available.\n");
        rv = true;
    }
    return rv;
}
