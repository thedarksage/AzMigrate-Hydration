
#include<stdio.h>


#ifdef WIN32
#include<windows.h>
#include<winioctl.h>
#include "virtualvolumewin.h"
#endif

#include<io.h>
#include<dbt.h>
#include <sstream>
#include "virtualvolume.h"
#include "hostagenthelpers.h"
#include "globs.h"
#include "VVolCmds.h"
#include "InstallInVirVol.h"
#include "ListEntry.h"
#include "configurevxagent.h"
#include "portablehelpersmajor.h"
#include "portablehelpers.h"
#include "inmsafeint.h"
#include "inmageex.h"

using namespace std;

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

/*Function to mount a volume from the sparse file*/

bool VirVolume::mountsparsevolume(WCHAR * sparsefile, TCHAR volumename[MAX_STRING_SIZE])
{

    USES_CONVERSION;

    HANDLE  hVVCtrlDevice = INVALID_HANDLE_VALUE;
    hVVCtrlDevice= createvirtualvolume(); //Function to create a file
    HRESULT hr;
    hr=SVS_OK;
    BOOLEAN     bResult;
    PWCHAR      MountedDeviceNameLink = NULL;
    DWORD       dwReturn;
    ULONG       ulLength = 0;
    TCHAR       TargetPath[MAX_STRING_SIZE];
    TCHAR       MountPointF[MAX_STRING_SIZE];

    inm_tcscpy_s(MountPointF, ARRAYSIZE(MountPointF), volumename);
    ULONG volsize = strlen(volumename);
    if(volumename[volsize - 1] != '\\')
        inm_tcscat_s(MountPointF, ARRAYSIZE(MountPointF),  _T("\\"));

    //__asm int 3;

    if(QueryDosDevice(MountPointF, TargetPath, sizeof(TargetPath)))
    {
        DebugPrintf( SV_LOG_INFO, "Mount Point Already Exists, err %d\n", GetLastError() );
        return false;
    }

    MountedDeviceNameLink = (PWCHAR) malloc(VV_MAX_CB_DEVICE_NAME);
    if (NULL == MountedDeviceNameLink)
    {
        DebugPrintf(SV_LOG_INFO, "MountVirtualVolume: malloc failed, err %d\n", GetLastError() );
        return false;
    }

    ULONG FileNameLengthWithNullChar;
    INM_SAFE_ARITHMETIC(FileNameLengthWithNullChar = InmSafeInt<ULONG>::Type((ULONG) wcslen(sparsefile)) + 1, INMAGE_EX((ULONG) wcslen(sparsefile)))
    ULONG MountPointLengthWithNullChar;
    INM_SAFE_ARITHMETIC(MountPointLengthWithNullChar = InmSafeInt<ULONG>::Type((ULONG) strlen(volumename)) + 1, INMAGE_EX((ULONG) strlen(volumename)))
    size_t firstlen;
    INM_SAFE_ARITHMETIC(firstlen = (FileNameLengthWithNullChar * InmSafeInt<size_t>::Type(sizeof (WCHAR))), INMAGE_EX(FileNameLengthWithNullChar)(sizeof (WCHAR)))
    size_t secondlen;
    INM_SAFE_ARITHMETIC(secondlen = (MountPointLengthWithNullChar * InmSafeInt<size_t>::Type(sizeof(CHAR))), INMAGE_EX(MountPointLengthWithNullChar)(sizeof(CHAR)))
    INM_SAFE_ARITHMETIC(ulLength = (ULONG) (firstlen + secondlen + 2 * InmSafeInt<size_t>::Type(sizeof(ULONG))), INMAGE_EX(firstlen)(secondlen)(sizeof(ULONG)))
    INM_SAFE_ARITHMETIC(ulLength += InmSafeInt<size_t>::Type(sizeof(ULONG)), INMAGE_EX(ulLength)(sizeof(ULONG)))
    PCHAR Buffer = (PCHAR) calloc(ulLength, 1);
    ULONG FieldOffset = 0;
    FieldOffset += sizeof(ULONG);
    *(PULONG)(Buffer + FieldOffset) = MountPointLengthWithNullChar  * sizeof(CHAR);
    FieldOffset += sizeof(ULONG);
    inm_memcpy_s(Buffer + FieldOffset, ulLength - FieldOffset, volumename, MountPointLengthWithNullChar  * sizeof(CHAR));
    FieldOffset += MountPointLengthWithNullChar  * sizeof(CHAR);
    *(PULONG)(Buffer + FieldOffset) = FileNameLengthWithNullChar  * sizeof(WCHAR);
    FieldOffset += sizeof(ULONG);
    inm_memcpy_s(Buffer + FieldOffset, ulLength - FieldOffset, sparsefile, FileNameLengthWithNullChar * sizeof(WCHAR));
    FieldOffset += FileNameLengthWithNullChar  * sizeof(WCHAR);
    bResult = DeviceIoControl(
        hVVCtrlDevice,
        (DWORD)IOCTL_INMAGE_ADD_VIRTUAL_VOLUME_FROM_FILE,
        Buffer,
        ulLength,
        MountedDeviceNameLink,
        VV_MAX_CB_DEVICE_NAME,
        &dwReturn, 
        NULL        // Overlapped
        ); 

    free(Buffer);

    if (bResult)
    {
        DebugPrintf(SV_LOG_DEBUG,"Returned Success from add virtual volume DeviceIoControl for file %s\n",volumename);

    }
    else 
    {
        DebugPrintf(SV_LOG_INFO, "Returned Failed from add virtual volume DeviceIoControl for %s file, err %d\n",volumename, GetLastError() );

        return false;
    }

    TCHAR VolumeSymLink[MAX_STRING_SIZE];

    memset(VolumeSymLink,0,MAX_STRING_SIZE);

    TCHAR VolumeName[MAX_STRING_SIZE];

    wcstotcs(VolumeName, ARRAYSIZE(VolumeName), MountedDeviceNameLink);
    size_t NameLength = wcslen(MountedDeviceNameLink) + 1;
    wcstotcs(VolumeSymLink, ARRAYSIZE(VolumeSymLink), MountedDeviceNameLink + NameLength);
    VolumeSymLink[1] = _T('\\');
    inm_tcscat_s(VolumeSymLink, ARRAYSIZE(VolumeSymLink), _T("\\"));

    DeleteAutoAssignedDriveLetter(hVVCtrlDevice, volumename);

    TCHAR DeviceName[3] = _T("X:");
    DeviceName[0] = MountPointF[0];

    if (!DefineDosDevice(DDD_RAW_TARGET_PATH, DeviceName, VolumeName)) 
    {

        DebugPrintf( SV_LOG_INFO, "Virtual Volume Mount Failed: hr = %08X\n",hr );
    }
    else
    {
        if(!DefineDosDevice(DDD_RAW_TARGET_PATH | DDD_REMOVE_DEFINITION | DDD_EXACT_MATCH_ON_REMOVE, DeviceName, VolumeName)) 
        {
            DebugPrintf( SV_LOG_INFO, "Virtual Volume Mount Failed: %d\n", GetLastError() );
        }
    }
    if(!SetVolumeMountPoint(MountPointF, VolumeSymLink))
    {
        DebugPrintf( SV_LOG_ERROR, "Returned Failed from SetVolumeMountPoint, err %d\n", GetLastError() );
        return false;
    }

    DEV_BROADCAST_VOLUME DevBroadcastVolume;
    DevBroadcastVolume.dbcv_devicetype = DBT_DEVTYP_VOLUME;
    DevBroadcastVolume.dbcv_flags = 0;
    DevBroadcastVolume.dbcv_reserved = 0;
    DevBroadcastVolume.dbcv_size = sizeof(DEV_BROADCAST_VOLUME);
    DevBroadcastVolume.dbcv_unitmask = GetLogicalDrives() | (1 << (tolower(volumename[0] - _T('a')))) ;

    SendMessage(HWND_BROADCAST,WM_DEVICECHANGE,DBT_DEVICEARRIVAL,(LPARAM)&DevBroadcastVolume);

    DebugPrintf(SV_LOG_INFO,"Mount Successful\n");

    sendInformationtoCX();  //This function updates CX about the creation of virtual Volume

    free(MountedDeviceNameLink);
    return true;
}



size_t VirVolume::wcstotcs(PTCHAR tstring, const size_t tstringlen, PCWCH wcstring)
{
    size_t size = 0;
#ifdef UNICODE
    inm_wcscpy_s(tstring, tstringlen, wcstring);
    size = wcslen(tstring);
#else
    wcstombs_s(&size, tstring, tstringlen, wcstring, tstringlen-1);
#endif
    return size;
}

HANDLE VirVolume::createvirtualvolume()
{
    HANDLE  hVVCtrlDevice = INVALID_HANDLE_VALUE;
    hVVCtrlDevice = OpenVirtualVolumeControlDevice();
    if( INVALID_HANDLE_VALUE == hVVCtrlDevice )
    {
        //DebugPrintf(SV_LOG_INFO,"Opening File %s\n",VV_CONTROL_DOS_DEVICE_NAME);
        hVVCtrlDevice = OpenVirtualVolumeControlDevice();
        if( INVALID_HANDLE_VALUE == hVVCtrlDevice ) 
        {
            // DebugPrintf(SV_LOG_INFO,"CreateFile %s Failed with Error = %#x\n", VV_CONTROL_DOS_DEVICE_NAME, GetLastError());

            INSTALL_INVIRVOL_DATA InstallData;
            InstallData.DriverName = INVIRVOL_SERVICE;
            TCHAR SystemDir[MAX_DRIVER_PATH_LENGTH];
            GetSystemDirectory(SystemDir, MAX_DRIVER_PATH_LENGTH);
            InstallData.PathAndFileName = new TCHAR[MAX_DRIVER_PATH_LENGTH];
            inm_tcscpy_s(InstallData.PathAndFileName, MAX_DRIVER_PATH_LENGTH, SystemDir);
            inm_tcscat_s(InstallData.PathAndFileName, MAX_DRIVER_PATH_LENGTH, _T("\\drivers\\invirvol.sys"));
            DRSTATUS Status = InstallInVirVolDriver(InstallData);

            if(!DR_SUCCESS(Status))
            {
                DebugPrintf(SV_LOG_INFO,"Service install Failed with Error:%#x", GetLastError());
                return false;
            }

            START_INVIRVOL_DATA StartData;
            StartData.DriverName = INVIRVOL_SERVICE;

            Status = StartInVirVolDriver(StartData);
            if(!DR_SUCCESS(Status))
            {
                DebugPrintf(SV_LOG_INFO,"Start Driver Failed with Error:%#x", GetLastError());
                return false;
            }

            hVVCtrlDevice = OpenVirtualVolumeControlDevice();

            if(INVALID_HANDLE_VALUE == hVVCtrlDevice ) {

                DebugPrintf(SV_LOG_INFO,"Retry to open Control device Failed with Error = %#x\n", GetLastError());
                return false;
            }
        } 
    }
    return hVVCtrlDevice;
}


HANDLE VirVolume::OpenVirtualVolumeControlDevice()
{
    HANDLE  hDevice;

    // PR#10815: Long Path support
    hDevice = SVCreateFile (
        VV_CONTROL_DOS_DEVICE_NAME,
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_EXISTING,
        NULL,
        NULL
        );
    return hDevice;
}


VOID VirVolumeWin::DeleteAutoAssignedDriveLetter(HANDLE hDevice, PTCHAR MountPoint)
{
    CHAR    OutputBuffer[MAX_STRING_SIZE];
    DWORD   BytesReturned = 0;
    BOOLEAN bResult;
    DEV_BROADCAST_VOLUME DevBroadcastVolume;

    bResult = DeviceIoControl(hDevice,
        IOCTL_INMAGE_GET_VOLUME_SYMLINK_FOR_RETENTION_POINT,
        MountPoint,
        (DWORD) (_tcslen(MountPoint) + 1 ) * sizeof(TCHAR),
        OutputBuffer,
        (DWORD) sizeof(OutputBuffer),
        &BytesReturned,
        NULL);

    if(!bResult)
    {
        DebugPrintf(SV_LOG_INFO,"IOCTL_INMAGE_GET_VOLUME_SYMLINK_FOR_RETENTION_POINT Failed: InputBuffer:%s Error:%x\n",MountPoint, GetLastError());
        return;
    }

    WCHAR *DosDeviceName = (WCHAR*)(OutputBuffer + (wcslen((PWCHAR)OutputBuffer) + 1) * sizeof(WCHAR));

    if(wcslen(DosDeviceName) > 0)
    {
        TCHAR DeviceToBeDeleted[4] = _T("X:\\");
        DeviceToBeDeleted[0] = DosDeviceName[12];

        if(!DeleteVolumeMountPoint(DeviceToBeDeleted))
        {
            DebugPrintf(SV_LOG_INFO,"Device delete Failed: %s Error:%d\n", DeviceToBeDeleted, GetLastError());
            // _tprintf(_T("Device delete Failed: %s Error:%d\n"), DeviceToBeDeleted, GetLastError());
        }

        DevBroadcastVolume.dbcv_devicetype = DBT_DEVTYP_VOLUME;
        DevBroadcastVolume.dbcv_flags = 0;
        DevBroadcastVolume.dbcv_reserved = 0;
        DevBroadcastVolume.dbcv_size = sizeof(DEV_BROADCAST_VOLUME);
        DevBroadcastVolume.dbcv_unitmask = 1 << (tolower(DeviceToBeDeleted[0]) - _T('a'));

        SendMessage(HWND_BROADCAST,
            WM_DEVICECHANGE,
            DBT_DEVICEREMOVECOMPLETE,
            (LPARAM)&DevBroadcastVolume
            );
    }
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
			DebugPrintf(SV_LOG_DEBUG,"The sparse file %s does not exist.\n",filename.c_str());
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


bool VirVolume::resize_new_version_sparse_file(std::string filename, SV_ULONGLONG size,
                                               bool enable_sparseattribute)
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
			DebugPrintf(SV_LOG_ERROR,"The sparse file %s does not exist.\n",filename.c_str());
            rv = false;
            break;
        }

		bool iscompressioenabled = IsCompressonEnabledOnFile(sparsepartfile.str());
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

                if(!createsparsefileonresize(sparsefile.str(),filesize,iscompressioenabled,enable_sparseattribute ))
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

                if(!createsparsefileonresize(sparsefile.str(),filesize,iscompressioenabled,enable_sparseattribute ))
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
                if(!createsparsefileonresize(sparsefile.str(),(lastFileSize-resize),iscompressioenabled,enable_sparseattribute ))
                {
                    rv = false;                                
                }
			}
		}

	}while(false);
	return rv;
}

bool VirVolume::resizesparsefile(const std::string& filename , const SV_ULONGLONG& size, InmSparseAttributeState_t sparseattr_state)
{
    bool rv = true;
    ACE_stat db_stat ={0};	
	SV_ULONGLONG filesize = size ;
    SV_ULONGLONG maxsize = m_TheLocalConfigurator.getSparseFileMaxSize();
    bool new_version = false;
    bool sparse_attr_supported = true;
    bool sparse_enabled = true;
    if(maxsize)
        new_version = IsNewSparseFileFormat(filename.c_str());

    do
    {
        if(sparseattr_state == E_INM_ATTR_STATE_DISABLED) {
            sparse_enabled = false;
        }else {

            std::vector<char> vVolpack_dir(SV_MAX_PATH, '\0');
            DirName(filename.c_str(), &vVolpack_dir[0], vVolpack_dir.size());
            inm_strcat_s(&vVolpack_dir[0], vVolpack_dir.size(), "\\");

            if(!VolumeSupportsSparseFiles(A2T(&vVolpack_dir[0]))) {
                sparse_attr_supported = false;
            }

            if(sparseattr_state == E_INM_ATTR_STATE_ENABLEIFAVAILABLE)
            {
                if(sparse_attr_supported) {
                    sparse_enabled = true;
                }
                else {
                    sparse_enabled = false;
                    DebugPrintf(SV_LOG_DEBUG, "%s does not support sparse attribute.\n", vVolpack_dir.data());
                }
            }else if(sparseattr_state == E_INM_ATTR_STATE_ENABLED)  {
                if(!sparse_attr_supported)  {
                    DebugPrintf(SV_LOG_ERROR,
                        " %s does not support creation of sparse file. Please choose another volume\n", vVolpack_dir.data());
                    rv = false;
                    break;
                }
                sparse_enabled = true;
            }
        }

        if(new_version)
        {
            if(!resize_new_version_sparse_file(filename,filesize,sparse_enabled))
            {
                rv = false;
                break;
            }
        }
        else
        {
			bool iscompressioenabled = IsCompressonEnabledOnFile(filename);
            if(!createsparsefileonresize(filename,filesize,iscompressioenabled,sparse_enabled))
            {
                rv = false;
                break;
            }        
        }
        DebugPrintf(SV_LOG_INFO,"Virtual volume file(s) resized Successfully\n");
    }while(false);
    return rv;
}
bool VirVolume::IsCompressonEnabledOnFile(const std::string & filename)
{
	bool rv =false;
	 WIN32_FIND_DATA data;
	 HANDLE h = FindFirstFile(filename.c_str(),&data);
	 if (h != INVALID_HANDLE_VALUE)
	 {
		 if(data.dwFileAttributes & FILE_ATTRIBUTE_COMPRESSED)
			 rv = true;
	 }
	 FindClose(h);
	 return rv;
}
bool VirVolume::createsparsefileonresize(const std::string & filename, const SV_ULONGLONG & size,
        bool iscompressioenabled, bool  enable_sparseattribute)
{
    bool rv = true;
	DWORD   BytesReturned = 0;


    do
    {

        ACE_HANDLE hHandle = ACE_INVALID_HANDLE;
        hHandle = SVCreateFile(filename.c_str(),GENERIC_WRITE ,0, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL  );
        bool bExist = IS_INVALID_HANDLE_VALUE(hHandle) == true ? false : true;
        if(!bExist)
        {
			DebugPrintf(SV_LOG_ERROR,"open/creation of %s failed with error: %d\n",filename.c_str(),GetLastError());
            rv = false;
            break;
        }

		if(enable_sparseattribute)
		{
			bool result = DeviceIoControl(hHandle,FSCTL_SET_SPARSE,NULL,0,NULL,0,&BytesReturned,NULL );
			if(!result)
			{
				DebugPrintf(SV_LOG_ERROR,"FSCTL_SET_SPARSE ioctl failed for %s. error: %d\n",filename.c_str(),GetLastError());
				SAFE_CLOSEHANDLE(hHandle);
				rv = false;
				break;
			}
		}

        LARGE_INTEGER li;
        li.QuadPart=size;
        li.LowPart=SetFilePointer(hHandle,li.LowPart,&li.HighPart,FILE_BEGIN);
		if ( li.LowPart == INVALID_SET_FILE_POINTER && GetLastError() != NO_ERROR )
		{
			DebugPrintf(SV_LOG_ERROR,"SetFilePointer call failed on %s. error: %d\n",filename.c_str(), GetLastError());
			SAFE_CLOSEHANDLE(hHandle);
			rv = false;
			break;
		}

		if(0 == SetEndOfFile(hHandle))
		{
			DebugPrintf(SV_LOG_ERROR,"SetEndOfFile call failed on %s. error: %d\n",filename.c_str(), GetLastError());
			SAFE_CLOSEHANDLE(hHandle);
			rv = false;
			break;
		}

        SAFE_CLOSEHANDLE(hHandle);
        if(iscompressioenabled)
        {
			rv = EnableCompressonOnFile(filename);
        }

    }while(false);
    return rv;
}

/*This function creates a sparse file*/
bool VirVolume::createsparsefile(const std::string& filename , 
                                 const SV_ULONGLONG& size, 
                                 InmSparseAttributeState_t sparseattr_state)
{
    bool rv = true;
    HANDLE hHandle = INVALID_HANDLE_VALUE;
    DWORD   BytesReturned = 0;
    SV_ULONGLONG maxsize = m_TheLocalConfigurator.getSparseFileMaxSize();
    bool sparse_attr_supported = true;
    bool sparse_enabled = true;
    if(m_TheLocalConfigurator.IsVolpackDriverAvailable())
        maxsize = 0;
    int i = 0;
    
    do
    {
        std::vector<char> vdb_dir(SV_MAX_PATH, '\0');
        DirName(filename.c_str(),&vdb_dir[0], vdb_dir.size());
        inm_strcat_s(&vdb_dir[0], vdb_dir.size(), "\\");
        ACE_stat db_stat ={0};
        SV_ULONGLONG filesize = 0 ;
        SV_ULONGLONG pendingsize = size ;
        stringstream sparsepartfile;
        
        if(sparseattr_state == E_INM_ATTR_STATE_DISABLED) {
            sparse_enabled = false;
        }else {


            if(!VolumeSupportsSparseFiles(A2T(&vdb_dir[0]))) {
                sparse_attr_supported = false;
            }

            if(sparseattr_state == E_INM_ATTR_STATE_ENABLEIFAVAILABLE)
            {
                if(sparse_attr_supported) {
                    sparse_enabled = true;
                }
                else {
                    sparse_enabled = false;
                    DebugPrintf(SV_LOG_DEBUG, "%s does not support sparse attribute.\n", vdb_dir.size());
                }
            }else if(sparseattr_state == E_INM_ATTR_STATE_ENABLED)  {
                if(!sparse_attr_supported)  {
                    DebugPrintf(SV_LOG_ERROR,
                        " %s does not support creation of sparse file. Please choose another volume\n", vdb_dir.size());
                    rv = false;
                    break;
                }
                sparse_enabled = true;
            }
        }

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

            hHandle = SVCreateFile(sparsepartfile.str().c_str(),GENERIC_WRITE ,0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL  );
            bool bExist = IS_INVALID_HANDLE_VALUE(hHandle) == true ? false : true;
            if(!bExist)
            {
				DebugPrintf(SV_LOG_ERROR,"open/creation of %s failed with error: %d\n",filename.c_str(),GetLastError());
                rv = false;
                break;
            }

			if(sparse_enabled)
			{
				bool result = DeviceIoControl(hHandle,FSCTL_SET_SPARSE,NULL,0,NULL,0,&BytesReturned,NULL );
				if(!result)
				{
					DebugPrintf(SV_LOG_ERROR,"FSCTL_SET_SPARSE ioctl failed for %s. error: %d\n",filename.c_str(),GetLastError());
					SAFE_CLOSEHANDLE(hHandle);
					rv = false;
					break;
				}
			}
            
            LARGE_INTEGER li;
            li.QuadPart= filesize;
            li.LowPart=SetFilePointer(hHandle,li.LowPart,&li.HighPart,FILE_END);
			if ( li.LowPart == INVALID_SET_FILE_POINTER && GetLastError() != NO_ERROR )
			{
				DebugPrintf(SV_LOG_ERROR,"SetFilePointer call failed on %s. error: %d\n",filename.c_str(), GetLastError());
				SAFE_CLOSEHANDLE(hHandle);
				rv = false;
				break;
			}

			if(0 == SetEndOfFile(hHandle))
			{
				DebugPrintf(SV_LOG_ERROR,"SetEndOfFile call failed on %s. error: %d\n",filename.c_str(), GetLastError());
				SAFE_CLOSEHANDLE(hHandle);
				rv = false;
				break;
			}

            SAFE_CLOSEHANDLE(hHandle);
        }while(pendingsize > 0);
        if(!rv)
            break;
        DebugPrintf(SV_LOG_INFO,"Virtual volume file(s) Created Successfully\n");
    }while(false);
    return rv;
}

/*This function informs CX about the creation of Virtual Volumes*/
void VirVolume::sendInformationtoCX()
{

    //const char* SV_ROOT_KEY = "SOFTWARE\\SV Systems";
    //SV_HOST_AGENT_PARAMS m_AgentParams;
    //char szValue[ MAX_STRING_SIZE ]; 
    //DWORD hostIdSize = MAX_STRING_SIZE;

    //GetAgentParamsFromRegistry( SV_ROOT_KEY, &m_AgentParams);

    //GetHostId( szValue, sizeof( szValue ) );
    //
    SVERROR rc;

    rc = RegisterHost();// SV_ROOT_KEY, m_AgentParams.szSVServerName, m_AgentParams.ServerHttpPort, m_AgentParams.szSVRegisterURL, m_AgentParams.AgentInstallPath, szValue, &hostIdSize);

}

BOOL VirVolume::VolumeSupportsReparsePoints(PTCHAR VolumeName)
{
    DWORD FileSystemFlags                       = 0;
    DWORD SerialNumber                          = 0;
    BOOL  bResult                               = TRUE;
    BOOL  MountPointSupported                   = FALSE;
    TCHAR FileSystemNameBuffer[SV_MAX_PATH]		= {0};
    TCHAR rootVolume[SV_MAX_PATH]				= {0};

    if(!GetVolumePathName(VolumeName,rootVolume,sizeof(rootVolume)))
    {
        int err=GetLastError();       
        return MountPointSupported;
    }

    // PR#10815: Long Path support
    if(!SVGetVolumeInformation(rootVolume, NULL, 0, &SerialNumber, NULL, &FileSystemFlags, 
        FileSystemNameBuffer, ARRAYSIZE(FileSystemNameBuffer)))
    {
        int err=GetLastError();       
        return MountPointSupported;
    }

    if(FileSystemFlags & FILE_SUPPORTS_REPARSE_POINTS && SerialNumber > 0)
        MountPointSupported = TRUE;

    return MountPointSupported;
}


BOOL VirVolume::VolumeSupportsSparseFiles(PTCHAR VolumeName)
{
    DWORD FileSystemFlags                       = 0;
    DWORD SerialNumber                          = 0;
    BOOL  bResult                               = TRUE;
    BOOL  SparseAttributeSupported              = FALSE;
    TCHAR FileSystemNameBuffer[SV_MAX_PATH]		= {0};
    TCHAR rootVolume[SV_MAX_PATH]				= {0};

    if(!GetVolumePathName(VolumeName,rootVolume,sizeof(rootVolume)))
    {
        int err=GetLastError();       
        return SparseAttributeSupported;
    }

    // PR#10815: Long Path support
    if(!SVGetVolumeInformation(rootVolume, NULL, 0, &SerialNumber, NULL, &FileSystemFlags, 
        FileSystemNameBuffer, ARRAYSIZE(FileSystemNameBuffer)))
    {
        int err=GetLastError();       
        return SparseAttributeSupported;
    }

    if(FileSystemFlags & FILE_SUPPORTS_SPARSE_FILES)
        SparseAttributeSupported = TRUE;

    return SparseAttributeSupported;
}


void VirVolume::VirVolumeCheckForRemount(string &virVolList,bool createlist)
{

    USES_CONVERSION;

    HANDLE  hDevice;
    SVERROR svError = SVS_OK;

    bool bExist=false;

    HRESULT hr=SVS_OK;

    int counter = m_TheLocalConfigurator.getVirtualVolumesId();
    while(counter!=0)
    {
        char regData[26];
		inm_sprintf_s(regData, ARRAYSIZE(regData), "VirVolume%d", counter);

        string data= m_TheLocalConfigurator.getVirtualVolumesPath(regData);

        std::string sparsefilename;
        std::string volume;

        if (!ParseSparseFileNameAndVolPackName(data, sparsefilename, volume))
        {
            counter--;
            continue;
        }

        if(createlist)
        {
            virVolList+=volume;
            virVolList+=',';
        }

        else
        {
            std::string mountpt = volume;
            FormatVolumeName(mountpt);
            ACE_HANDLE hHandle = ACE_INVALID_HANDLE;
            FormatVolumeNameToGuid(mountpt);
            // PR#10815: Long Path support
            hHandle = ACE_OS::open(getLongPathName(mountpt.c_str()).c_str(),O_RDONLY);
            bExist = (ACE_INVALID_HANDLE == hHandle) ? false : true;
            if ( (ACE_INVALID_HANDLE != hHandle) && ( bExist ) )
            {
                ACE_OS::close(hHandle);
            }
            if ( !bExist )
            {
                if(m_TheLocalConfigurator.IsVolpackDriverAvailable())
                {
                    if(volume.size() > 3)
                    {
                        // PR#10815: Long Path support
                        ACE_OS::rmdir(getLongPathName(volume.c_str()).c_str());
                    }
                    if((mountpt.size() > 3) && (SVMakeSureDirectoryPathExists(volume.c_str()).failed()))
                    {
                        DebugPrintf(SV_LOG_ERROR,"Failed to create the mount point directory %s, err %d\n",volume.c_str(),GetLastError() );
                        counter--;
                        continue;
                    }

                    bool res=mountsparsevolume(A2W(sparsefilename.c_str()),A2T((char *)volume.c_str()));
                    if(res==false)
                    {
                        hr = HRESULT_FROM_WIN32( GetLastError() );
                        DebugPrintf(SV_LOG_ERROR,"Returned Failed from mount virtual volume, err %d\n", GetLastError() );
                        DebugPrintf(SV_LOG_ERROR, "FAILED : Unable to mount virtual volume %s \n ",volume.c_str());

                    }
                }
            }
        }
        counter--;		
    }

}

bool  VirVolume::UnmountVirtualVolume(HANDLE hDevice,TCHAR MountPoint[MAX_STRING_SIZE])
{
    DWORD   BytesReturned                   = 0;
    ULONG   CanUnloadDriver                 = 0;
    BOOL    bResult                         = FALSE;
    PCHAR   OutputBuffer[MAX_STRING_SIZE];
    TCHAR   VolumeLink[MAX_STRING_SIZE];

    bResult = DeviceIoControl(hDevice,
        IOCTL_INMAGE_GET_VOLUME_SYMLINK_FOR_RETENTION_POINT,
        MountPoint,
        (DWORD) (strlen(MountPoint) + 1) * sizeof(TCHAR),
        OutputBuffer,
        (DWORD) sizeof(OutputBuffer),
        &BytesReturned,
        NULL);

    if(!bResult)
    {
        DebugPrintf(SV_LOG_ERROR,"IOCTL_INMAGE_GET_VOLUME_NAME_FOR_RETENTION_POINT Failed: %d\n", 
            GetLastError());
        return false;
    }

    wcstotcs(VolumeLink, ARRAYSIZE(VolumeLink), (PWCHAR)OutputBuffer);
    if(UnmountFileSystem(VolumeLink))
    {
        DebugPrintf(SV_LOG_INFO,"The virtual volume %s successfully deleted.\n",MountPoint);

    }

    bResult = DeviceIoControl(hDevice,
        IOCTL_INMAGE_REMOVE_VIRTUAL_VOLUME_FROM_RETENTION_LOG,
        MountPoint,
        (DWORD) (strlen(MountPoint) + 1) * sizeof(TCHAR),
        NULL,
        0,
        &BytesReturned,
        NULL);

    if(!bResult)
    {
        DebugPrintf(SV_LOG_ERROR,"IOCTL_INMAGE_REMOVE_VIRTUAL_VOLUME_FROM_RETENTION_LOG Failed: MountPoint: %s Error:%s\n", 
            MountPoint, GetLastError());
        return false;
    }

    if(_tcslen(MountPoint) < 3)
    {
        DEV_BROADCAST_VOLUME DevBroadcastVolume;
        DevBroadcastVolume.dbcv_devicetype = DBT_DEVTYP_VOLUME;
        DevBroadcastVolume.dbcv_flags = 0;
        DevBroadcastVolume.dbcv_reserved = 0;
        DevBroadcastVolume.dbcv_size = sizeof(DEV_BROADCAST_VOLUME);
        DevBroadcastVolume.dbcv_unitmask = 1 << (tolower(MountPoint[0]) - _T('a'));

        SendMessage(HWND_BROADCAST,
            WM_DEVICECHANGE,
            DBT_DEVICEREMOVECOMPLETE,
            (LPARAM)&DevBroadcastVolume
            );
    }


    return true;
}


bool VirVolume::Unmount(string VolumeName,bool checkfortarget,bool bypassdriver)
{

    char				MntPt[2048];
    if(checkfortarget)
    {
		if(IsVolpackUsedAsTarget(VolumeName))
		{
			DebugPrintf(SV_LOG_INFO,"The volume %s can not be unmounted as it is used as target volume\n", VolumeName.c_str());
                return false;
            }
	}

    HANDLE  CtrlDevice = INVALID_HANDLE_VALUE;
    bool res = true;

    inm_strcpy_s(MntPt, ARRAYSIZE(MntPt), VolumeName.c_str());
    if(!bypassdriver)
    {
        CtrlDevice=createvirtualvolume();
        res=UnmountVirtualVolume(CtrlDevice,A2T(MntPt));
        SAFE_CLOSEHANDLE(CtrlDevice);
    }
    if(!res)
        return false;
    if(VolumeName.size() > 3)
    {
        // PR#10815: Long Path support
        ACE_OS::rmdir(getLongPathName(VolumeName.c_str()).c_str());
    }
    RegisterHost();
    return true;
}
bool VirVolume::UnmountFileSystem(const PTCHAR VolumeName)
{   
    TCHAR FormattedVolumeName[MAX_STRING_SIZE];

    inm_tcscpy_s(FormattedVolumeName, ARRAYSIZE(FormattedVolumeName), VolumeName);
    FormattedVolumeName[1] = _T('\\');

    DeleteVolumeMountPointVolume(VolumeName);
    if(!FileDiskUmount(FormattedVolumeName))
    {
        DebugPrintf(SV_LOG_ERROR,"Unmounted Unformatted Drive. FileDiskUmount Error: %x\n", GetLastError());
        // return false;
    }
    return true;
}

VOID VirVolume::DeleteVolumeMountPointVolume(const PTCHAR VolumeName)
{
    TCHAR FormattedVolumeName[MAX_STRING_SIZE];
    TCHAR FormattedVolumeNameWithBackSlash[MAX_STRING_SIZE];

    inm_tcscpy_s(FormattedVolumeName, ARRAYSIZE(FormattedVolumeName), VolumeName);
    FormattedVolumeName[1] = _T('\\');

    inm_tcscpy_s(FormattedVolumeNameWithBackSlash, ARRAYSIZE(FormattedVolumeNameWithBackSlash), FormattedVolumeName);
    inm_tcscat_s(FormattedVolumeNameWithBackSlash, ARRAYSIZE(FormattedVolumeNameWithBackSlash), _T("\\"));

    DeleteMountPoints(FormattedVolumeNameWithBackSlash);
    DeleteDrivesForTargetVolume(FormattedVolumeNameWithBackSlash);
}

BOOLEAN VirVolumeWin::FileDiskUmount(PTCHAR VolumeName)
{
    HANDLE  Device;
    DWORD   BytesReturned;

    // PR#10815: Long Path support
    Device = SVCreateFile(
        VolumeName,
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_EXISTING,
        FILE_FLAG_NO_BUFFERING,
        NULL
        );

    if (Device == INVALID_HANDLE_VALUE)
        return FALSE;

    if (!DeviceIoControl(
        Device,
        FSCTL_DISMOUNT_VOLUME,
        NULL,
        0,
        NULL,
        0,
        &BytesReturned,
        NULL
        ))
    {
        return FALSE;
    }

    CloseHandle(Device);

    return TRUE;
}


bool VirVolume::DeleteDrivesForTargetVolume(const PTCHAR TargetVolumeLink)
{
    TCHAR MountedDrives[MAX_STRING_SIZE];
    DWORD DriveStringSizeInChar = 0, CharOffset = 0;

    DriveStringSizeInChar = GetLogicalDriveStrings(MAX_STRING_SIZE, MountedDrives);
    assert(DriveStringSizeInChar < MAX_STRING_SIZE);

    while(DriveStringSizeInChar - CharOffset)
    {
        TCHAR DriveName[4] = _T("X:\\");
        TCHAR UniqueVolumeName[MAX_STRING_SIZE] = {0};
        DriveName[0] = (MountedDrives + CharOffset)[0];

        CharOffset += (ULONG) _tcslen(MountedDrives + CharOffset) + 1;

        if(!GetVolumeNameForVolumeMountPoint(DriveName, UniqueVolumeName, 60))
        {
            DebugPrintf(SV_LOG_ERROR,"DeleteDrivesForTargetVolume: DriveName:%s GetVolumeNameForVolumeMountPoint Failed Error:%d", 
                DriveName, GetLastError());
            continue;
        }

        if(_tcscmp(UniqueVolumeName, TargetVolumeLink) != 0)
            continue;

        if(!DeleteVolumeMountPoint(DriveName))
        {
			DebugPrintf(SV_LOG_ERROR,"DeleteVolumeMountPoint %s Failed: Error:%d\n",DriveName,GetLastError());
            return false;
        }

        DebugPrintf(SV_LOG_DEBUG,"Drive %s deleted successfully\n", DriveName);
    }
    return true;
}

bool VirVolume::DeleteMountPoints(const PTCHAR TargetVolumeName)
{
    TCHAR   HostVolumeName[MAX_STRING_SIZE];
    BOOL    bResult         = TRUE;
    HANDLE  hHostVolume     = FindFirstVolume(HostVolumeName, MAX_STRING_SIZE);

    if(hHostVolume == INVALID_HANDLE_VALUE)
    {
        DebugPrintf(SV_LOG_ERROR,"Could not enumerate volumes Error:%d\n", GetLastError());
        return false;
    }

    do
    {
        if(VolumeSupportsReparsePoints(HostVolumeName))
            DeleteMountPointsForTargetVolume(HostVolumeName, TargetVolumeName);

        bResult = FindNextVolume(hHostVolume, HostVolumeName, MAX_STRING_SIZE);
    }while(bResult);

    FindVolumeClose(hHostVolume);
    return true;
}

bool VirVolume::DeleteMountPointsForTargetVolume(PTCHAR HostVolumeName, PTCHAR TargetVolumeName)
{
    DWORD       BufferLength                                = MAX_STRING_SIZE;
    BOOL        bResult                                     = TRUE;
    WCHAR       VolumeMountPoint[MAX_STRING_SIZE]           = {0};
    WCHAR       MountPointTargetVolume[MAX_STRING_SIZE]     = {0};
    WCHAR       FullVolumeMountPoint[SV_MAX_PATH]           = {0};
    HANDLE      hMountHandle;

    hMountHandle = FindFirstVolumeMountPointW(ACE_Ascii_To_Wide(HostVolumeName).wchar_rep(), VolumeMountPoint, BufferLength);
    if(INVALID_HANDLE_VALUE == hMountHandle)
    {
        return false;
    }

    inm_wcscpy_s(FullVolumeMountPoint, ARRAYSIZE(FullVolumeMountPoint), ACE_Ascii_To_Wide(HostVolumeName).wchar_rep());
    inm_wcscat_s(FullVolumeMountPoint, ARRAYSIZE(FullVolumeMountPoint), VolumeMountPoint);

    do
    {
        bResult = GetVolumeNameForVolumeMountPointW(FullVolumeMountPoint, MountPointTargetVolume, MAX_STRING_SIZE);

        if(bResult && 0 == ACE_OS::strcmp(MountPointTargetVolume, ACE_Ascii_To_Wide(TargetVolumeName).wchar_rep()))
        {
            if(!DeleteVolumeMountPointW(FullVolumeMountPoint))
            {
                DebugPrintf(SV_LOG_ERROR,"DeleteVolumeMountPoint Failed for point:%s with Error:%d",
                    ACE_Wide_To_Ascii(FullVolumeMountPoint).char_rep(), GetLastError());
            }
            else
            {
                DebugPrintf(SV_LOG_DEBUG,"Mountpoint %s successfully deleted.\n",
                    ACE_Wide_To_Ascii(FullVolumeMountPoint).char_rep());
            }
        }

        bResult = FindNextVolumeMountPointW(hMountHandle, VolumeMountPoint, BufferLength);
        inm_wcscpy_s(FullVolumeMountPoint, ARRAYSIZE(FullVolumeMountPoint), ACE_Ascii_To_Wide(HostVolumeName).wchar_rep());
        inm_wcscat_s(FullVolumeMountPoint, ARRAYSIZE(FullVolumeMountPoint), VolumeMountPoint);

    }while(bResult);

    FindVolumeMountPointClose(hMountHandle);

    return true;
}
// Bug 3549
// Added deletesparse flag which when false causes the sparse file and the drscout.conf entries for
// virtual volume to be retained (i.e, not deleted)
bool VirVolume::DeletePersistVirtualVolumes(string volumename,string filename, bool IfExixts, bool deletepersistinfo,bool deletesparse) 
{
    bool rv = false;
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
    int counter=m_TheLocalConfigurator.getVirtualVolumesId();
    while(counter!=0)
    {
		inm_sprintf_s(regName, ARRAYSIZE(regName), "VirVolume%d", counter);

        string data = m_TheLocalConfigurator.getVirtualVolumesPath(regName);
        std::string sparsefilename;
        std::string volume;

        if (!ParseSparseFileNameAndVolPackName(data, sparsefilename, volume))
        {
            counter--;
            continue;
        }

        if(IfExixts)
        {
            if(strcmp(volume.c_str(),volumename.c_str())==0)
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
            if(deletepersistinfo && stricmp(volume.c_str(),volumename.c_str())==0) 
            {
                DebugPrintf(SV_LOG_DEBUG,"Device %s exist in persistent store\n",volumename.c_str());
				rv = true;
                string new_value="";
                m_TheLocalConfigurator.setVirtualVolumesPath(regName,new_value);
                if(deletesparse)
                    deletesparsefile((char*)sparsefilename.c_str());			
                return true;
            }
        }

        counter--;
    }
    if(!rv)
        DebugPrintf(SV_LOG_DEBUG,"Device %s does not exist in persistent store.\n",volumename.c_str());

    return true;
}

// Bug 3549 
// deletesparsefile is passed which is true by default 
bool VirVolume::UnmountAllVirtualVolumes(bool checkfortarget,bool deletepersistinfo,bool deletesparsefile,bool bypassdriver)
{
    USES_CONVERSION;
    bool rv = true;
    int counter=m_TheLocalConfigurator.getVirtualVolumesId();
	std::vector<std::string> tgtvolumes;
	std::vector<std::string> virtual_volumes_to_unmont;

    while(counter!=0)
    {

        stringstream regData;

        regData << "VirVolume" << counter;

        string data=m_TheLocalConfigurator.getVirtualVolumesPath(regData.str());
        std::string sparsefilename;
        std::string volume;

        if (!ParseSparseFileNameAndVolPackName(data, sparsefilename, volume))
        {
            counter--;
            continue;
        }
        else
        {
			if(checkfortarget)
            {
				if(IsVolpackUsedAsTarget(volume))
					tgtvolumes.push_back(volume);
				else
					virtual_volumes_to_unmont.push_back(volume);				
            }
			else
            {
				virtual_volumes_to_unmont.push_back(volume);
            }
        }

        counter--;		
    }

    //sendInformationtoCX();
	do
	{
		if(tgtvolumes.size() > 0)
		{
			DebugPrintf(SV_LOG_INFO,"Following volumes are in use as target volumes,they will not be unmounted.\n");
			for(int cnt = 0; cnt < tgtvolumes.size(); cnt++)
			{
				DebugPrintf(SV_LOG_INFO,"%d) %s\n",cnt+1,tgtvolumes[cnt].c_str());
			}
			DebugPrintf(SV_LOG_INFO,"\n");
		}
		
		if(virtual_volumes_to_unmont.size() == 0)
		{
        DebugPrintf(SV_LOG_INFO,"There are no virtual volumes mounted in the system.\n");
			break;
		}

		for(int cnt = 0; cnt < virtual_volumes_to_unmont.size(); cnt++)
		{
			bool res = true;
			if(m_TheLocalConfigurator.IsVolpackDriverAvailable())
			{
				res=Unmount(virtual_volumes_to_unmont[cnt],checkfortarget,bypassdriver);
			}
			if(res)
				res = DeletePersistVirtualVolumes(virtual_volumes_to_unmont[cnt],"", false, deletepersistinfo,deletesparsefile); 
			if(!res)
        rv = false;
		}

		if(rv)
		{
			if(checkfortarget)
				DebugPrintf(SV_LOG_INFO, 
				"Deleting all the virtual volumes excluding the virtual volumes used as target... done\n");
			else
				DebugPrintf(SV_LOG_INFO, 
				"Deleting all the virtual volumes including the virtual volumes used as target... done\n");
		}
		else
			DebugPrintf(SV_LOG_INFO,"Some virtual volumes are not removed. Unmountall operation failed.\n");

	}while(false);
	

    return rv;
}

void VirVolume::deletesparsefile(char *sparsefile)
{
    bool res = true;
    DebugPrintf(SV_LOG_DEBUG,"virtual volume deletesparsefile called for the file %s\n",sparsefile);
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
            res=DeleteFile(sparsepartfile.str().c_str());
            if(!res)		
            {
                DebugPrintf(SV_LOG_DEBUG, "FAILED: Not able to delete the sparse file %s %lu\n ", sparsefile, GetLastError());
                DebugPrintf(SV_LOG_INFO, "Failed to delete the sparse file %s. Manually delete the file.\n ",
                    sparsepartfile.str().c_str());
            }            
            else
            {
                DebugPrintf(SV_LOG_DEBUG,"Sparse File %s deleted successfully.\n",sparsepartfile.str().c_str());
            }
            i++;
        }
    }
    else
    {
        res=DeleteFile(sparsefile);
    if(!res)
        {
            DebugPrintf(SV_LOG_DEBUG, "FAILED: Not able to delete the sparse file %s %lu\n ", sparsefile, GetLastError());
            DebugPrintf(SV_LOG_INFO, "Failed to delete the sparse file %s. Manually delete the file.\n ",
                sparsefile);
        }
        else
        {
            DebugPrintf(SV_LOG_DEBUG,"Sparse File %s deleted successfully.\n",sparsefile);
        }
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
			DebugPrintf(SV_LOG_DEBUG,"The volume %s is used as target volume\n", volume.c_str());
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
