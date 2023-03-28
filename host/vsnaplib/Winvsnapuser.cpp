// Copyright (c) 2005 InMage
// This file contains proprietary and confidential information and
// remains the unpublished property of InMage. Use, disclosure,
// or reproduction is prohibited except as permitted by express
// written license aggreement with InMage.
//
// File       : VsnapUser.cpp
//
// Description: Vsnap User library definitions.
//
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#ifdef SV_WINDOWS
#include<windows.h>
#include<winioctl.h>
#include<io.h>
#include<sys\types.h>
#include<sys\stat.h> 
#include<dbt.h>
#include "hostagenthelpers.h"
#include "autoFS.h"
#include "VVDevControl.h"
#include "devicefilter.h"
#endif

#include<iostream>
#include<errno.h>
#include <retentioninformation.h>

#include "configwrapper.h"
#include "portablehelpers.h"
#include "globs.h"
#include "DiskBtreeCore.h"
#include "VsnapCommon.h"
#include "VsnapUser.h"
#include "DiskBtreeHelpers.h"
#include "cdputil.h"
#include "cdpsnapshot.h"
#include "cdplock.h"
#include "portablehelpersmajor.h"
#include "inmsafecapis.h"

#ifdef SV_WINDOWS
#include "VVolCmds.h"
#include <ctype.h>
#include "InstallInVirVol.h"
#include "ListEntry.h"
#include <setupapi.h>
#endif
#include <ace/ACE.h>
#include <ace/OS.h>
#include <ace/OS_NS_fcntl.h>
#include <ace/OS_NS_unistd.h>
#include <ace/OS_NS_errno.h>
#include <ace/Global_Macros.h>

#include <boost/lexical_cast.hpp>

#include "inmageex.h"

#ifdef SV_WINDOWS
#include<windows.h>
#endif
/* Added for all FAT and NTFS functions */
#include "portablehelpersmajor.h"

using namespace std;

const char *VsnapGlobalLockName = "InmageVsnapGlobalLock";
const size_t MOUNT_PATH_LEN = 2048;


#ifdef SV_WINDOWS
extern bool IsSingleListEmpty(PSINGLE_LIST_ENTRY);
extern void InitializeEntryList(PSINGLE_LIST_ENTRY);
extern SINGLE_LIST_ENTRY *PopEntryList(PSINGLE_LIST_ENTRY);
extern void InsertInList(PSINGLE_LIST_ENTRY, SV_ULONGLONG, SV_UINT, SV_UINT , SV_ULONGLONG, SV_UINT);
extern void StartVirtualVolumeDriver(ACE_HANDLE hDevice, PSTART_VIRTAL_VOLUME_DATA pStartVirtualVolumeData);
extern void StopVirtualVolumeDriver(ACE_HANDLE hDevice, PSTOP_VIRTUAL_VOLUME_DATA pStopVirtualVolumeData);
#endif
extern void UnloadExistingVolumes(ACE_HANDLE hDevice);

extern void VsnapCallBack(void *, void *, void *, bool);
extern bool operator<(VsnapKeyData &, VsnapKeyData &);
extern bool operator>(VsnapKeyData &, VsnapKeyData &);
extern bool operator==(VsnapKeyData &, VsnapKeyData &);

//bool VsnapUserGetFileNameFromFileId(string, unsigned long long, unsigned long, char *);
void VsnapNodeTraverseCallback(void *NodeKey, void *NewKey, void *param, bool flag);
/******************** UTILITY FUNCTIONS - END *******************/



void WinVsnapMgr::DeleteMountPointsForTargetVolume(char* HostVolumeName, char* TargetVolumeName, 
                                                   VsnapVirtualVolInfo *VirtualInfo)
{
    bool        bResult                 = TRUE;
    WCHAR       *VolumeMountPoint		= NULL;
    WCHAR       *FullVolumeMountPoint	= NULL;
    WCHAR       *MountPointTargetVolume	= NULL;


    ACE_HANDLE      hMountHandle;

    const size_t       BufferLength = 2048;
    size_t buffersize = 0;
    INM_SAFE_ARITHMETIC(buffersize = InmSafeInt<size_t>::Type(BufferLength) * sizeof(WCHAR), INMAGE_EX(BufferLength)(sizeof(WCHAR)))


    VolumeMountPoint = (WCHAR *)malloc(buffersize);
    if (!VolumeMountPoint)
    {
        return;
    }
    memset(VolumeMountPoint, 0, buffersize);

    FullVolumeMountPoint = (WCHAR *)malloc(buffersize);
    if(!FullVolumeMountPoint)
    {
        free(VolumeMountPoint);
        return;
    }
    memset(FullVolumeMountPoint, 0, buffersize);

    MountPointTargetVolume = (WCHAR *)malloc(buffersize);
    if(!MountPointTargetVolume)
    {
        free(VolumeMountPoint);
        free(FullVolumeMountPoint);
        return;
    }
    memset(MountPointTargetVolume, 0, buffersize);


    hMountHandle = FindFirstVolumeMountPointW(ACE_Ascii_To_Wide(HostVolumeName).wchar_rep(), VolumeMountPoint, BufferLength);
    if(ACE_INVALID_HANDLE == hMountHandle)
    {
        free(VolumeMountPoint);
        free(FullVolumeMountPoint);
        free(MountPointTargetVolume);
        return;
    }

    inm_wcscpy_s(FullVolumeMountPoint, BufferLength, ACE_Ascii_To_Wide(HostVolumeName).wchar_rep());
    inm_wcscat_s(FullVolumeMountPoint, BufferLength, VolumeMountPoint);

    do
    {
        bResult = GetVolumeNameForVolumeMountPointW(FullVolumeMountPoint, MountPointTargetVolume, BufferLength);
        if(bResult && 0 == ACE_OS::strcmp(MountPointTargetVolume, ACE_Ascii_To_Wide(TargetVolumeName).wchar_rep()))
        {
            if(!DeleteVolumeMountPointW(FullVolumeMountPoint))
            {
                char errstr[SV_MAX_PATH];
                VirtualInfo->Error.VsnapErrorStatus = VSNAP_UNMOUNT_FAILED;
                VirtualInfo->Error.VsnapErrorCode = GetLastError();
                inm_sprintf_s(errstr, ARRAYSIZE(errstr), "DeleteVolumeMountPoint Failed for point:%s with Error:%#x",
                    ACE_Wide_To_Ascii(FullVolumeMountPoint).char_rep(), GetLastError());
                VsnapPrint(errstr);
            }
        }

        bResult = FindNextVolumeMountPointW(hMountHandle, VolumeMountPoint, BufferLength);
        inm_wcscpy_s(FullVolumeMountPoint, BufferLength, ACE_Ascii_To_Wide(HostVolumeName).wchar_rep());
        inm_wcscat_s(FullVolumeMountPoint, BufferLength, VolumeMountPoint);

    }while(bResult);

    FindVolumeMountPointClose(hMountHandle);

    free(VolumeMountPoint);
    free(FullVolumeMountPoint);
    free(MountPointTargetVolume);
}


bool WinVsnapMgr::UnmountFileSystem(const wchar_t* VolumeName, VsnapVirtualVolInfo *VirtualInfo)
{   
    wchar_t FormattedVolumeName[MAX_STRING_SIZE];
    wchar_t FormattedVolumeNameWithBackSlash[MAX_STRING_SIZE];

    inm_wcscpy_s(FormattedVolumeName, ARRAYSIZE(FormattedVolumeName), VolumeName);

    FormattedVolumeName[1] = _T('\\');

    inm_wcscpy_s(FormattedVolumeNameWithBackSlash, ARRAYSIZE(FormattedVolumeNameWithBackSlash), FormattedVolumeName);
    inm_wcscat_s(FormattedVolumeNameWithBackSlash, ARRAYSIZE(FormattedVolumeNameWithBackSlash), L"\\");

    char FormattedVolumeNameWithBackSlashT[MAX_STRING_SIZE];
    wcstotcs(FormattedVolumeNameWithBackSlashT, ARRAYSIZE(FormattedVolumeNameWithBackSlashT), FormattedVolumeNameWithBackSlash);

    char FormattedVolumeNameT[MAX_STRING_SIZE];
    wcstotcs(FormattedVolumeNameT, ARRAYSIZE(FormattedVolumeNameT), FormattedVolumeName);

    SV_ULONG   BytesReturned;
    ACE_HANDLE VolHdl;

    // PR#10815: Long Path support
    VolHdl = SVCreateFile(
        FormattedVolumeNameT,
        GENERIC_READ| GENERIC_WRITE ,
        FILE_SHARE_READ  | FILE_SHARE_WRITE,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
        );

    if (VolHdl == ACE_INVALID_HANDLE)
    {
        VirtualInfo->Error.VsnapErrorStatus = VSNAP_VIRVOL_OPEN_FAILED;
        VirtualInfo->Error.VsnapErrorCode = ACE_OS::last_error();
        return FALSE;
    }

    FlushFileBuffers(VolHdl);

    DeviceIoControl(
        VolHdl,
        FSCTL_DISMOUNT_VOLUME,
        NULL,
        0,
        NULL,
        0,
        &BytesReturned,
        NULL
        );

    bool Lock = false;

    if (DeviceIoControl(
        VolHdl,
        FSCTL_LOCK_VOLUME,
        NULL,
        0,
        NULL,
        0,
        &BytesReturned,
        NULL
        ))
    {
        Lock = true;	
    }

    DeleteMountPoints(FormattedVolumeNameWithBackSlashT, VirtualInfo);
    DeleteDrivesForTargetVolume(FormattedVolumeNameWithBackSlashT, VirtualInfo);

    if(Lock)
        DeviceIoControl( VolHdl, FSCTL_UNLOCK_VOLUME, NULL, 0, NULL, 0, &BytesReturned, NULL);

    ACE_OS::close(VolHdl);

    return true;
}

SV_UCHAR WinVsnapMgr::FileDiskUmount(char* VolumeName)
{
    ACE_HANDLE  Device;
    SV_ULONG   BytesReturned;

    // PR#10815: Long Path support
    Device = SVCreateFile(
        VolumeName,
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
        );

    if (Device == ACE_INVALID_HANDLE)
    {
        return FALSE;
    }

    FlushFileBuffers(Device);

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
        ACE_OS::close(Device);
        return FALSE;
    }

    ACE_OS::close(Device);

    return TRUE;
}

void WinVsnapMgr::DeleteDrivesForTargetVolume(const char* TargetVolumeLink, VsnapVirtualVolInfo *VirtualInfo)
{
    char	MountedDrives[2048];
    memset(MountedDrives, 0, 2048);
    SV_ULONG	DriveStringSizeInChar = 0, CharOffset = 0;
    char	errstr[100];
    SV_ULONG	dwret = 0;

    DriveStringSizeInChar = GetLogicalDriveStrings(2048, MountedDrives);

    while(DriveStringSizeInChar - CharOffset)
    {
        char DriveName[4] = _T("X:\\");
        char UniqueVolumeName[2048] = {0};
        DriveName[0] = (MountedDrives + CharOffset)[0];

        CharOffset += (SV_ULONG) _tcslen(MountedDrives + CharOffset) + 1;

        if(!GetVolumeNameForVolumeMountPoint(DriveName, UniqueVolumeName, 60))
        {
            continue;
        }

        if(_tcscmp(UniqueVolumeName, TargetVolumeLink) != 0)
            continue;

        DriveName[2] = 0;
        if (!DefineDosDevice(
            DDD_REMOVE_DEFINITION,
            DriveName,
            NULL
            ))
        {
            VirtualInfo->Error.VsnapErrorStatus = VSNAP_UNMOUNT_FAILED;
            VirtualInfo->Error.VsnapErrorCode = GetLastError();
            inm_sprintf_s(errstr, ARRAYSIZE(errstr), "DeleteDrivesForTargetVolume: Could not delete Drive %s Error:%#x\n", DriveName, GetLastError());
            wprintf(L"DeleteDrivesForTargetVolume: Could not delete Drive %s Error:%#x\n", convertUtf8ToWstring(DriveName).c_str(), GetLastError());
            VsnapPrint(errstr);
        }

        DriveName[2] = '\\';
        DeleteVolumeMountPoint(DriveName);

        if(VirtualInfo->VolumeName.size() <= 3)
        {
            //DEV_BROADCAST_VOLUME DevBroadcastVolume;
            //DevBroadcastVolume.dbcv_devicetype = DBT_DEVTYP_VOLUME;
            //DevBroadcastVolume.dbcv_flags = 0;
            //DevBroadcastVolume.dbcv_reserved = 0;
            //DevBroadcastVolume.dbcv_size = sizeof(DEV_BROADCAST_VOLUME);
            //DevBroadcastVolume.dbcv_unitmask = (1 << (tolower(DriveName[0] - _T('a')))) ;

            //SendNotifyMessage(HWND_BROADCAST,
            //	WM_DEVICECHANGE,
            //	DBT_DEVICEREMOVECOMPLETE,
            //	(LPARAM)&DevBroadcastVolume
            //);

            ULONG ulAction = DBT_DEVICEREMOVECOMPLETE; 
            BroadcastSystemMessage(BSF_FLUSHDISK | BSF_FORCEIFHUNG,NULL,WM_DEVICECHANGE,    (WPARAM) ulAction,(LPARAM)(DEV_BROADCAST_HDR*)VirtualInfo->VolumeName.c_str() );

        }
    }

}

bool WinVsnapMgr::SetMountPointForVolume(char* MountPoint, char* VolumeSymLink, char* VolumeName)
{
    bool status = true;
    SV_ULONG dwret = 0;

    if(_tcslen(MountPoint) > 3)
    {
        DeleteVolumeMountPoint(MountPoint);

        if(!SetVolumeMountPoint(MountPoint, VolumeSymLink))
        {
            status = false;
        }
    }
    else
    {
        char DeviceName[3] = _T("X:");
        DeviceName[0] = MountPoint[0];

        DefineDosDevice(DDD_RAW_TARGET_PATH, DeviceName, VolumeName);
        DefineDosDevice(DDD_RAW_TARGET_PATH | DDD_REMOVE_DEFINITION | DDD_EXACT_MATCH_ON_REMOVE, DeviceName, VolumeName);
        if(!SetVolumeMountPoint(MountPoint, VolumeSymLink))
        {
            status = false;
        }

    }

    return status;
}


bool WinVsnapMgr::UpdateMap(DiskBtree *VsnapBtree, SV_UINT FileId, SVD_DIRTY_BLOCK_INFO *Block)
{
    bool			Success = true;

    SINGLE_LIST_ENTRY		DirtyBlockHead;

    VsnapOffsetSplitInfo	*OffsetSplit = NULL;
    enum BTREE_STATUS		Status = BTREE_SUCCESS;

    VsnapBtree->SetCallBackThirdParam(&DirtyBlockHead);

    InitializeEntryList(&DirtyBlockHead);
    InsertInList(&DirtyBlockHead, Block->DirtyBlock.ByteOffset, (SV_UINT)Block->DirtyBlock.Length, FileId, 
        Block->DataFileOffset, 0);

    while(true)
    {
        if(IsSingleListEmpty(&DirtyBlockHead))
            break;

        VsnapKeyData NewKey;

        OffsetSplit = (VsnapOffsetSplitInfo *)PopEntryList(&DirtyBlockHead);

        NewKey.Offset		= OffsetSplit->Offset;		
        NewKey.Length		= OffsetSplit->Length;
        NewKey.FileId		= OffsetSplit->FileId;
        NewKey.FileOffset	= OffsetSplit->FileOffset;
        NewKey.TrackingId	= 0;

        Status = VsnapBtree->BtreeInsert(&NewKey, true);
        if(Status == BTREE_FAIL)
        {
            DebugPrintf(SV_LOG_ERROR, "Btree insertion failed\n");
            Success = false;
            free(OffsetSplit);
            break;
        }
        free(OffsetSplit);
    }
    return Success;
}
bool WinVsnapMgr::GetVsnapSourceVolumeDetails(char *SrcVolume, VsnapPairInfo *PairInfo)
{
    SVERROR				hr = SVS_OK;
    ACE_HANDLE			VolHdl;
    VolHdl= ACE_INVALID_HANDLE;
    bool				IsHidden = false;

    DISK_GEOMETRY		disk_geometry = {0};

    VsnapVirtualVolInfo	*VirtualInfo = PairInfo->VirtualInfo;
    VsnapSourceVolInfo	*SrcInfo = PairInfo->SrcInfo;
    VsnapErrorInfo		*VsnapError = (VsnapErrorInfo *)&VirtualInfo->Error;
    string				VolName;

    do
    {
        SrcInfo->FsData = (char *)malloc(VSNAP_FS_DATA_SIZE);
        if(!SrcInfo->FsData)
            return false;

        memset(SrcInfo->FsData , 0, VSNAP_FS_DATA_SIZE);

        if(strlen(SrcVolume) == 2)
        {
            SrcVolume[1] = '\0';
            //SrcVolume[2] = '\0';
        }

        std::string sparsefile,volumename;
        bool newvirtualvolume = false; 
        bool is_volpack = IsVolPackDevice(SrcVolume,sparsefile,newvirtualvolume);

        if(newvirtualvolume)
        {
            volumename = sparsefile;
            volumename+=SPARSE_PARTFILE_EXT;
            volumename+="0";
            
        }
        else if(is_volpack)
        {
            volumename = sparsefile;
        }
        else
        {
           volumename = SrcVolume;
        }

		int fstype;
		if(GetFileSystemTypeForVolume(volumename.c_str(),fstype, IsHidden).succeeded())
        {
            SrcInfo->FsType = (VSNAP_FS_TYPE)fstype;
        }
        else
        {
            VsnapError->VsnapErrorStatus = VSNAP_UNKNOWN_FS_FOR_TARGET;
            VsnapError->VsnapErrorCode = 0;//ACE_OS::last_error();
            return false;
        }


        if(strlen(SrcVolume) == 1)
        {
            SrcVolume[1] = ':';
            SrcVolume[2] = '\0';
        }
        bool getsizefromtargetvolume = false;
        bool bReadOnly = false;
        if(!IsHidden && (isReadOnly(volumename.c_str(), bReadOnly).succeeded()) && (bReadOnly != true))
        {
            VsnapError->VsnapErrorStatus = VSNAP_TARGET_UNHIDDEN_RW;
            VsnapError->VsnapErrorCode = ACE_OS::last_error();
            return false;
        }
        if(!SrcInfo->skiptargetchecks)
        {
            if(m_TheConfigurator)
            {

                if(!getSourceRawSize((*m_TheConfigurator),SrcVolume,SrcInfo->ActualSize))
                {
                    //added the condition to support upgrade part, Bug:13760
                    //If source is not upgraded to 5.5 and target is upgraded to 5.5,
                    // at that time we will not be getting the rawsize for the source volume
                    //to get the size, we need to have a compartibility check,
                    //if source compartibility number less than 5.5 compartibility number
                    //then get the size from target volume else use the source raw size
                    SV_ULONG srccompartibilitynum = 0;
                    srccompartibilitynum = getOtherSiteCompartibilityNumber(SrcInfo->VolumeName);
                    if(srccompartibilitynum && (srccompartibilitynum < 550000))
                    {
                        getsizefromtargetvolume = true;
                    }
                    else
                    {
                        VsnapError->VsnapErrorCode = 0;//ACE_OS::last_error(); 
                        VsnapError->VsnapErrorStatus = VSNAP_TARGET_VOLUMESIZE_UNKNOWN;
                        return false;
                    }
                }

                if(!getSourceCapacity((*m_TheConfigurator),SrcVolume,SrcInfo->UserVisibleSize))
                {
                    VsnapError->VsnapErrorCode = 0;//ACE_OS::last_error(); 
                    VsnapError->VsnapErrorStatus = VSNAP_TARGET_VOLUMESIZE_UNKNOWN;
                    return false;
                }
            }
            else
            {
                DebugPrintf(SV_LOG_INFO, "Configuration information is not available. Unable to get the source volumesize.\n");
                VsnapError->VsnapErrorCode = 0;//ACE_OS::last_error(); 
                VsnapError->VsnapErrorStatus = VSNAP_TARGET_VOLUMESIZE_UNKNOWN;
                return false;
            }
        }

        string VolumeGuid=volumename;

        if(VolumeGuid.size()>3)
        {
            FormatVolumeNameToGuid(VolumeGuid);
            // PR#10815: Long Path support
            VolHdl = SVCreateFile(VolumeGuid.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, 
                OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

        }
        else
        {
            if(IsVolpackDriverAvailable() || (!is_volpack))
            {
                VolumeGuid= "\\\\.\\";
                VolumeGuid+=SrcVolume;
            }
            // PR#10815: Long Path support
            VolHdl = SVCreateFile(VolumeGuid.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, 
                OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

        }
        if(VolHdl == ACE_INVALID_HANDLE)
        {
            VsnapError->VsnapErrorStatus = VSNAP_TARGET_HANDLE_OPEN_FAILED;
            VsnapError->VsnapErrorCode = ACE_OS::last_error();
            return false;
        }

        SV_ULONG dwerror;
        DeviceIoControl(VolHdl, FSCTL_ALLOW_EXTENDED_DASD_IO, NULL, 0, NULL, 0, &dwerror, NULL);

        if(getsizefromtargetvolume)
        {
            if(GetVolumeSize(VolHdl, (ULONGLONG *)&SrcInfo->ActualSize).failed())
            {
                VsnapError->VsnapErrorStatus = VSNAP_TARGET_VOLUMESIZE_UNKNOWN;
                VsnapError->VsnapErrorCode = ACE_OS::last_error();
                ACE_OS::close(VolHdl);
                return false;
            }
        }

        if( 0 == DeviceIoControl( VolHdl, IOCTL_DISK_GET_DRIVE_GEOMETRY, NULL, 0, &disk_geometry,
            sizeof( DISK_GEOMETRY ), &dwerror, NULL ) )
        {
            SrcInfo->SectorSize = VSNAP_DEFAULT_SECTOR_SIZE;	
        }
        else
        {
            SrcInfo->SectorSize = disk_geometry.BytesPerSector;
        }

        LARGE_INTEGER DistanceToMove, DistanceMoved;
        SV_ULONG BytesRead = 0;
        DistanceToMove.QuadPart = 0;

        SetFilePointerEx(VolHdl, DistanceToMove, &DistanceMoved, FILE_BEGIN);

        if(!ReadFile(VolHdl, SrcInfo->FsData, VSNAP_FS_DATA_SIZE, &BytesRead, NULL))
        {
            VsnapError->VsnapErrorStatus = VSNAP_READ_TARGET_FAILED;
            VsnapError->VsnapErrorCode = ACE_OS::last_error();
            ACE_OS::close(VolHdl);
            return false;
        }	

		UnHideFileSystem(SrcInfo->FsData, VSNAP_FS_DATA_SIZE);
        ACE_OS::close(VolHdl);
        VolHdl = ACE_INVALID_HANDLE;


    } while (FALSE);

    return true;
}

ACE_HANDLE WinVsnapMgr::OpenVirtualVolumeControlDevice()
{
    ACE_HANDLE  hDevice;

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


void WinVsnapMgr::DeleteAutoAssignedDriveLetter(ACE_HANDLE hDevice, char* MountPoint, bool IsMountPoint)
{
    char					*OutputBuffer;
    SV_ULONG				BytesReturned = 0;
    SV_UCHAR					bResult;
    string					UniqueVolumeId;

    OutputBuffer = (char *)malloc(2048);
    if(!OutputBuffer)
        DebugPrintf(SV_LOG_ERROR, "Insufficient memory...\n");

    UniqueVolumeId.clear();
    UniqueVolumeId = VSNAP_UNIQUE_ID_PREFIX;
    UniqueVolumeId += MountPoint;

    //DEV_BROADCAST_VOLUME	DevBroadcastVolume;
    bResult = DeviceIoControl(hDevice,
        IOCTL_INMAGE_GET_VOLUME_SYMLINK_FOR_RETENTION_POINT,
        (void *)(UniqueVolumeId.c_str()),
        (SV_ULONG) UniqueVolumeId.size() + 1, 
        OutputBuffer,
        2048, 
        &BytesReturned,
        NULL);

    if(!bResult)
    {
        free(OutputBuffer);
        return;
    }


    wchar_t *DosDeviceName = (wchar_t*)(OutputBuffer + (wcslen((wchar_t*)OutputBuffer) + 1) * sizeof(wchar_t));

    if(wcslen(DosDeviceName) > 0)
    {

        char DeviceToBeDeleted[4] = _T("X:\\");
        DeviceToBeDeleted[0] = (char)DosDeviceName[12];


        if(!DeleteVolumeMountPoint(DeviceToBeDeleted))
        {
            free(OutputBuffer);
            return;
        }

        if(!IsMountPoint)
        {
            //DevBroadcastVolume.dbcv_devicetype = DBT_DEVTYP_VOLUME;
            //DevBroadcastVolume.dbcv_flags = 0;
            //DevBroadcastVolume.dbcv_reserved = 0;
            //DevBroadcastVolume.dbcv_size = sizeof(DEV_BROADCAST_VOLUME);
            //DevBroadcastVolume.dbcv_unitmask = 1 << (tolower(DeviceToBeDeleted[0]) - _T('a'));

            //SendNotifyMessage(HWND_BROADCAST,
            //	WM_DEVICECHANGE,
            //	DBT_DEVICEREMOVECOMPLETE,
            //	(LPARAM)&DevBroadcastVolume
            //);

            ULONG ulAction = DBT_DEVICEREMOVECOMPLETE; 
            BroadcastSystemMessage(BSF_FLUSHDISK | BSF_FORCEIFHUNG,NULL,WM_DEVICECHANGE,    (WPARAM) ulAction,(LPARAM)(DEV_BROADCAST_HDR*)MountPoint );
        }

    }

    UniqueVolumeId.clear();
    free(OutputBuffer);
}


bool WinVsnapMgr::MountVol(VsnapVirtualVolInfo *VirtualInfo, VsnapMountInfo *MountData, char* SnapshotDrive,bool &FsMountFailed)
{

    ACE_HANDLE		CtrlDevice = ACE_INVALID_HANDLE;
    SV_UCHAR			bResult;
    wchar_t *		MountedDeviceNameLink = NULL;
    SV_ULONG		dwReturn;
    SV_ULONG		ulLength = 0;
    bool			success = true;
    size_t			drivelen = strlen(SnapshotDrive);
    bool			IsMountPoint = false;
    VsnapErrorInfo	*Error = &VirtualInfo->Error;
    string			UniqueVolumeId;
    char			VolumeSymLink[MAX_STRING_SIZE];
    char			VolumeName[MAX_STRING_SIZE];
    PADD_RETENTION_IOCTL_DATA   InputData = NULL;
    VsnapMountIoctlDataInfo  new_mount_data;

    UniqueVolumeId.clear();

    if(drivelen > 3)
        IsMountPoint = true;

    do 
    {
        std::string tgtVolume = MountData->VolumeName;
        std::string sparsefile;
        bool isnewvirtualvolume = false;
        if((!IsVolpackDriverAvailable())&& IsVolPackDevice(MountData->VolumeName,sparsefile,isnewvirtualvolume))
        {
            memset(MountData->VolumeName,0,sizeof(MountData->VolumeName));
            inm_strcpy_s(MountData->VolumeName, ARRAYSIZE(MountData->VolumeName), sparsefile.c_str());
        }
        if(isnewvirtualvolume)
        {
            memset(&new_mount_data,0,sizeof(VsnapMountIoctlDataInfo));
            inm_memcpy_s(&new_mount_data, sizeof (new_mount_data), MountData, sizeof(VsnapMountInfo));

            std::string sparsepartfile = sparsefile + SPARSE_PARTFILE_EXT;
            sparsepartfile += "0";
            ACE_stat s;
            if( sv_stat( getLongPathName(sparsepartfile.c_str()).c_str(), &s ) != 0 )
            {
                DebugPrintf(SV_LOG_DEBUG, "Unable to get the size of the file %s\n", sparsepartfile.c_str());
                Error->VsnapErrorStatus = VSNAP_TARGET_SPARSEFILE_CHUNK_SIZE_FAILED;
                Error->VsnapErrorCode = ACE_OS::last_error();
                success = false;
                break;
            } 
            new_mount_data.sparsefilechunksize = s.st_size;
            inm_strcpy_s(new_mount_data.fileextention, ARRAYSIZE(new_mount_data.fileextention), SPARSE_PARTFILE_EXT.c_str()); 
            InputData = (PADD_RETENTION_IOCTL_DATA)calloc(sizeof(ADD_RETENTION_IOCTL_DATA) + sizeof(VsnapMountIoctlDataInfo)-2, 1);
        }
        else
            InputData = (PADD_RETENTION_IOCTL_DATA)calloc(sizeof(ADD_RETENTION_IOCTL_DATA) + sizeof(VsnapMountInfo)-2, 1);

        DebugPrintf(SV_LOG_INFO, "target volume is %s\n", MountData->VolumeName);
        if(NULL == InputData){
            Error->VsnapErrorStatus = VSNAP_MEM_UNAVAILABLE;
            Error->VsnapErrorCode = 0;//ACE_OS::last_error();
            success = false;
            break;
        }

        MountedDeviceNameLink = (wchar_t *) malloc(VV_MAX_CB_DEVICE_NAME);
        if (NULL == MountedDeviceNameLink) {
            Error->VsnapErrorStatus = VSNAP_MEM_UNAVAILABLE;
            Error->VsnapErrorCode = 0;//ACE_OS::last_error();
            success = false;
            break;
        }

        OpenInVirVolDriver(CtrlDevice, &VirtualInfo->Error);
        if(CtrlDevice == ACE_INVALID_HANDLE)
        {			
            success = false;
            break;

        }

        UniqueVolumeId  = VSNAP_UNIQUE_ID_PREFIX;
        UniqueVolumeId += SnapshotDrive;
        InputData->UniqueVolumeIdLength = UniqueVolumeId.size() + 1;
        inm_memcpy_s(InputData->UniqueVolumeID, sizeof (InputData->UniqueVolumeID), UniqueVolumeId.c_str(),InputData->UniqueVolumeIdLength);
        if(isnewvirtualvolume)
        {
			inm_memcpy_s(InputData->VolumeInfo, sizeof(VsnapMountIoctlDataInfo), &new_mount_data, sizeof(VsnapMountIoctlDataInfo));
            ulLength = sizeof(ADD_RETENTION_IOCTL_DATA) + sizeof(VsnapMountIoctlDataInfo) - 2;
            bResult = DeviceIoControl(
                CtrlDevice,
                (SV_ULONG)IOCTL_INMAGE_ADD_VIRTUAL_VOLUME_FROM_MULTIPART_SPARSE_FILE,
                InputData,
                ulLength,
                MountedDeviceNameLink,
                VV_MAX_CB_DEVICE_NAME,
                &dwReturn, 
                NULL        
                ); 
        }
        else
        {
			inm_memcpy_s(InputData->VolumeInfo, sizeof(VsnapMountInfo), MountData, sizeof(VsnapMountInfo));
            ulLength = sizeof(ADD_RETENTION_IOCTL_DATA) + sizeof(VsnapMountInfo) - 2;
            bResult = DeviceIoControl(
                CtrlDevice,
                (SV_ULONG)IOCTL_INMAGE_ADD_VIRTUAL_VOLUME_FROM_RETENTION_LOG,
                InputData,
                ulLength,
                MountedDeviceNameLink,
                VV_MAX_CB_DEVICE_NAME,
                &dwReturn, 
                NULL        
                ); 
        }        

        if(!bResult)
        {
            Error->VsnapErrorStatus = VSNAP_MOUNT_IOCTL_FAILED;
            Error->VsnapErrorCode = GetLastError();
            CloseHandle(CtrlDevice);
            success = false;
            break;
        }

        wcstotcs(VolumeName, ARRAYSIZE(VolumeName), MountedDeviceNameLink);
        size_t NameLength = wcslen(MountedDeviceNameLink) + 1;

        wcstotcs(VolumeSymLink, ARRAYSIZE(VolumeSymLink), MountedDeviceNameLink + NameLength);
        VolumeSymLink[1] = '\\';
		VirtualInfo->VolumeGuid = VolumeSymLink; //Copy the volume symlink to the user volinfo structure.
        inm_strcat_s(VolumeSymLink, ARRAYSIZE(VolumeSymLink), "\\");

        DeleteAutoAssignedDriveLetter(CtrlDevice, SnapshotDrive, IsMountPoint);

        if(VirtualInfo->MountVsnap && !SetMountPointForVolume(SnapshotDrive, VolumeSymLink, VolumeName))
        {
            Error->VsnapErrorStatus = VSNAP_MOUNT_FAILED;
            Error->VsnapErrorCode = GetLastError();

            DeviceIoControl(CtrlDevice, IOCTL_INMAGE_REMOVE_VIRTUAL_VOLUME_FROM_RETENTION_LOG,	(void *)UniqueVolumeId.c_str(),(SV_ULONG) (UniqueVolumeId.size()+ 1), NULL, 0, &dwReturn, NULL);


            ACE_OS::close(CtrlDevice);
            success = false;
            break;
        }

        memset(MountData->VolumeName,0,sizeof(MountData->VolumeName));
        inm_strcpy_s(MountData->VolumeName, ARRAYSIZE(MountData->VolumeName), tgtVolume.c_str());
        if(VirtualInfo->State != VSNAP_REMOUNT && !PersistVsnap(MountData, VirtualInfo))
        {
            DebugPrintf(SV_LOG_DEBUG, "Unable to Persist Vsnap Information for %s\n", VirtualInfo->VolumeName.c_str());
            DeviceIoControl(CtrlDevice, IOCTL_INMAGE_REMOVE_VIRTUAL_VOLUME_FROM_RETENTION_LOG,	(void *)UniqueVolumeId.c_str(),(SV_ULONG) (UniqueVolumeId.size()+ 1), NULL, 0, &dwReturn, NULL);
            ACE_OS::close(CtrlDevice);
            success = false;
            break;
        }

        ACE_OS::close(CtrlDevice);

        if(VirtualInfo->MountVsnap && drivelen <= 3)
        {
            //Hack.

            //DEV_BROADCAST_VOLUME DevBroadcastVolume;
            //DevBroadcastVolume.dbcv_devicetype = DBT_DEVTYP_VOLUME;
            //DevBroadcastVolume.dbcv_flags = 0;
            //DevBroadcastVolume.dbcv_reserved = 0;
            //DevBroadcastVolume.dbcv_size = sizeof(DEV_BROADCAST_VOLUME);
            //DevBroadcastVolume.dbcv_unitmask = GetLogicalDrives() | (1 << (tolower(SnapshotDrive[0] - _T('a')))) ;

            ULONG ulAction = DBT_DEVICEARRIVAL; 

            BroadcastSystemMessage(BSF_FLUSHDISK | BSF_FORCEIFHUNG,NULL,WM_DEVICECHANGE,    (WPARAM) ulAction,(LPARAM)(DEV_BROADCAST_HDR*)SnapshotDrive );

            //SendNotifyMessage(HWND_BROADCAST,
            //	WM_DEVICECHANGE,
            //	DBT_DEVICEARRIVAL,
            //	(LPARAM)&DevBroadcastVolume
            //);

            //RegisterHost();
        }
    } while(false);

    UniqueVolumeId.clear();

    if(MountedDeviceNameLink)
        free(MountedDeviceNameLink);
    if(InputData)
        free(InputData);

    return success;
}

bool WinVsnapMgr::GetVsnapContextFromVolumeLink(ACE_HANDLE hDevice, wchar_t * VolumeLink, VsnapVirtualVolInfo *VirtualInfo, 
                                                VsnapContextInfo *VsnapContext)
{
    SV_ULONG			MntPtLength = 0;
    SV_ULONG			BOffset = 0;
    char				*Buffer = NULL;
    int					BufferLen = sizeof(VsnapContextInfo) + ( 2048 * sizeof(char) ) + sizeof(SV_ULONG);
    VsnapErrorInfo		*Error = &VirtualInfo->Error;
    SV_ULONG			BytesReturned = 0;
    bool				bResult;

    if(VsnapContext == NULL)
        return false;

    Buffer = (char *)malloc(BufferLen);
    if(!Buffer)
        return false;

    bResult = DeviceIoControl(hDevice,
        IOCTL_INMAGE_GET_VOLUME_INFORMATION,
        VolumeLink,
        (SV_ULONG)((sizeof(wchar_t)*wcslen(VolumeLink)) + sizeof(wchar_t)),
        Buffer,
        BufferLen,
        &BytesReturned,
        NULL);

    if(!bResult)
    {
        Error->VsnapErrorStatus = VSNAP_GET_VOLUME_INFO_FAILED;
        Error->VsnapErrorCode = GetLastError();
        return false;
    }

    MntPtLength = *((SV_ULONG *)(Buffer));
    BOffset += (sizeof(SV_ULONG) + MntPtLength);

	inm_memcpy_s(VsnapContext, sizeof(VsnapContextInfo),((char *)Buffer + BOffset), sizeof(VsnapContextInfo));

    free(Buffer);

    return true;
}

bool WinVsnapMgr::UnmountVirtualVolume(char* SnapshotDrive, const size_t SnapshotDriveLen, VsnapVirtualVolInfo* VirtualInfo, bool bypassdriver)
{
    std::string output, error;
    return UnmountVirtualVolume(SnapshotDrive, SnapshotDriveLen, VirtualInfo, output, error, bypassdriver);
}
bool WinVsnapMgr::UnmountVirtualVolume(char* SnapshotDrive, const size_t SnapshotDriveLen, VsnapVirtualVolInfo* VirtualInfo, std::string& output, std::string& error, bool bypassdriver)
{
    SV_ULONG			BytesReturned = 0;
    SV_ULONG			CanUnloadDriver = 0;
    bool			bResult = FALSE;
    wchar_t			VolumeLink[MAX_STRING_SIZE];
    ACE_HANDLE			hDevice = ACE_INVALID_HANDLE;
    bool			status = true;
    VsnapErrorInfo	*Error = &VirtualInfo->Error;
    string			UniqueId;
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s for the vsnap device %s\n",
        FUNCTION_NAME,VirtualInfo->VolumeName.c_str());
    UniqueId.clear();

    do 
    {
        if(bypassdriver)
            break;
        hDevice = OpenVirtualVolumeControlDevice();

        if(ACE_INVALID_HANDLE == hDevice)
        {
            Error->VsnapErrorStatus = VSNAP_INVALID_MOUNT_PT;
            Error->VsnapErrorCode = ACE_OS::last_error();
            status = false;
            break;
        }

        UniqueId = VSNAP_UNIQUE_ID_PREFIX;
        UniqueId += SnapshotDrive;

        bResult = DeviceIoControl(hDevice,
            IOCTL_INMAGE_GET_VOLUME_SYMLINK_FOR_RETENTION_POINT,
            (void *)UniqueId.c_str(),
            (SV_ULONG) (UniqueId.size() + 1),
            VolumeLink,
            (SV_ULONG) sizeof(VolumeLink),
            &BytesReturned,
            NULL);

        if(!bResult)
        {
            ACE_OS::close(hDevice);
            Error->VsnapErrorStatus = VSNAP_INVALID_MOUNT_PT;
            Error->VsnapErrorCode = GetLastError();
            DebugPrintf(SV_LOG_DEBUG, "IOCTL_INMAGE_GET_VOLUME_SYMLINK_FOR_RETENTION_POINT call failed for the snapshot drive %s\n",
                UniqueId.c_str());
            status = false;
            break;
        }
        DebugPrintf(SV_LOG_DEBUG, "IOCTL_INMAGE_GET_VOLUME_SYMLINK_FOR_RETENTION_POINT call succeeded for the snapshot drive %s\n",
            UniqueId.c_str());

		if(!UnmountAllMountPointInVsnap(VirtualInfo->VolumeName))
		{
            DebugPrintf(SV_LOG_ERROR, "UnmountAllMountPointInVsnap call failed for the vsnap device %s\n",
                VirtualInfo->VolumeName.c_str());
			// continue even on error
		}
        
		if(!UnmountFileSystem(VolumeLink, VirtualInfo))
        {
            DebugPrintf(SV_LOG_DEBUG, "UnmountFileSystem call failed for the vsnap device %s\n",
                VirtualInfo->VolumeName.c_str());
            ACE_OS::close(hDevice);
            status = false;
            break;
        }
        DebugPrintf(SV_LOG_DEBUG, "UnmountFileSystem succeeded for the vsnap device %s\n",
            VirtualInfo->VolumeName.c_str());
        bResult = DeviceIoControl(hDevice,
            IOCTL_INMAGE_REMOVE_VIRTUAL_VOLUME_FROM_RETENTION_LOG,
            (void *)UniqueId.c_str(),
            (SV_ULONG) (UniqueId.size() + 1),
            NULL,
            0,
            &BytesReturned,
            NULL);

        if(!bResult)
        {
            DebugPrintf(SV_LOG_DEBUG, "IOCTL_INMAGE_REMOVE_VIRTUAL_VOLUME_FROM_RETENTION_LOG call failed for the snapshot drive %s\n",
                UniqueId.c_str());
            ACE_OS::close(hDevice);
            Error->VsnapErrorStatus = VSNAP_UNMOUNT_FAILED;
            Error->VsnapErrorCode = GetLastError();
            status = false;
            break;
        }
        DebugPrintf(SV_LOG_DEBUG, "IOCTL_INMAGE_REMOVE_VIRTUAL_VOLUME_FROM_RETENTION_LOG call succeeded for the snapshot drive %s\n",
            UniqueId.c_str());
		ACE_OS::close(hDevice);
        /*
        bResult = DeviceIoControl(hDevice,
        IOCTL_INMAGE_CAN_UNLOAD_DRIVER,
        NULL,
        0,
        &CanUnloadDriver,
        sizeof(CanUnloadDriver),
        &BytesReturned,
        NULL);

        if(!bResult)
        {
        CloseHandle(hDevice);
        Error->VsnapErrorStatus = VSNAP_UNMOUNT_FAILED;
        Error->VsnapErrorCode = GetLastError();
        status = false;
        break;
        }

        CloseHandle(hDevice);
        if(TRUE == CanUnloadDriver)
        {
        STOP_INVIRVOL_DATA StopData;
        StopData.DriverName = INVIRVOL_SERVICE;
        DRSTATUS DrStatus = StopInVirVolDriver(StopData);

        if(!DR_SUCCESS(DrStatus))
        {
        Error->VsnapErrorStatus = VSNAP_DRIVER_UNLOAD_FAILED;
        Error->VsnapErrorCode = GetLastError();
        status = false;
        break;
        }
        }
        */

        // Need to fix the below code. Should not call registerhost().
        //RegisterHost();

    } while (FALSE);

    UniqueId.clear();
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s for the vsnap device %s\n",
        FUNCTION_NAME,VirtualInfo->VolumeName.c_str());
    return status;
}

void WinVsnapMgr::StartTick(SV_ULONG & StartTime)
{

    StartTime = GetTickCount();	

}

void WinVsnapMgr::EndTick(SV_ULONG StartTime)
{
    SV_ULONG Diff = 0;
    SV_ULONG EndTime = 0;
    char msg[100];

    EndTime = GetTickCount();

    Diff = EndTime - StartTime;
    if(Diff/1000)
    {
        float val = (float)Diff/1000;
        inm_sprintf_s(msg, ARRAYSIZE(msg), "This operation took %f Seconds\n", val);
        VsnapPrint(msg);
    }
    else
    {
        inm_sprintf_s(msg, ARRAYSIZE(msg), "This operation took %lu milliseconds\n", Diff);
        VsnapPrint(msg);
    }
}
bool WinVsnapMgr::Unmount(VsnapVirtualInfoList & VirtualInfoList, bool ShouldDeleteVsnapLogs, bool SendStatus,bool bypassdriver)
{
    std::string output, error;
    return Unmount(VirtualInfoList, ShouldDeleteVsnapLogs, SendStatus, output, error,bypassdriver);
}
//Bug #4044
bool WinVsnapMgr::Unmount(VsnapVirtualInfoList & VirtualInfoList, bool ShouldDeleteVsnapLogs, bool SendStatus, std::string& output, std::string& error,bool bypassdriver)
{

    VsnapVirtualIter	VirtualIterBegin = VirtualInfoList.begin();
    VsnapVirtualIter	VirtualIterEnd = VirtualInfoList.end();
    char				MntPt[2048];
    string				LogDir;

    for( ; VirtualIterBegin != VirtualIterEnd ; ++VirtualIterBegin )
    {
        VsnapVirtualVolInfo *VirtualInfo = (VsnapVirtualVolInfo *)*VirtualIterBegin;
        VirtualInfo->Error.VsnapErrorStatus = VSNAP_SUCCESS;
        VirtualInfo->Error.VsnapErrorCode = 0;

        size_t len = 0;

        inm_strcpy_s(MntPt, ARRAYSIZE(MntPt), VirtualInfo->VolumeName.c_str());
        TruncateTrailingBackSlash(MntPt);

        len = strlen(MntPt);
        if( len <= 3 )
        {
            MntPt[0] = toupper(MntPt[0]);
            MntPt[1] = ':';
            MntPt[2] = '\\';
            MntPt[3] = '\0';
        }


        if(len > 3)
            _strlwr(MntPt);

        if((len > 3) && MntPt[len - 1] != '\\')
        {
            MntPt[len] = '\\';
            MntPt[len + 1] = '\0';
        }

        VsnapPersistInfo vsnapinfo;
        std::ostringstream errstr;

        std::vector<VsnapPersistInfo> vsnaps;
        if(!RetrieveVsnapInfo(vsnaps, "this", "", VirtualInfo->VolumeName))
        {
            errstr << "Failed to retrieve vsnap information for " << VirtualInfo->VolumeName
                << std::endl;
            error = errstr.str().c_str();
            DebugPrintf(SV_LOG_INFO, "%s", errstr.str().c_str());
            return false;
        }

        if(!vsnaps.empty())
        {
            if(vsnaps.size() > 1)
            {
                errstr << "ERROR - More than 1 vsnap present with the same mountpoint" << VirtualInfo->VolumeName
                    << std::endl;
                error = errstr.str().c_str();
                DebugPrintf(SV_LOG_INFO, "%s", errstr.str().c_str());
                return false;
            }

            vsnapinfo = vsnaps.back();

            VirtualInfo->State = VSNAP_UNMOUNT;
            DebugPrintf(SV_LOG_INFO, "Started unmounting the vsnap for the vsnap device %s\n",
                VirtualInfo->VolumeName.c_str());
            if (UnmountVirtualVolume(MntPt, ARRAYSIZE(MntPt), VirtualInfo, bypassdriver))
            {
                std::string volumename;
                volumename = MntPt;
                CDPUtil::trim(volumename, " :\\");
                if(ShouldDeleteVsnapLogs)
                {
                    bool rv = DeleteVsnapLogs(vsnapinfo);

                    if(!rv)
                    {
                        errstr << "Unable to delete vsnap logs for " << vsnapinfo.mountpoint << " for target "
                            << vsnapinfo.target << std::endl;
                        error = errstr.str().c_str();
                        DebugPrintf(SV_LOG_INFO, "%s", errstr.str().c_str());
                        return false;
                    }
                    DebugPrintf(SV_LOG_DEBUG, "Delete vsnap logs suceeded for the vsnap ",
                        VirtualInfo->VolumeName.c_str());
                    //Bug #8543

                    //Delete the persistent file storing the vsnap information
                    //when the corresponding is deleted.

                    std::string location;
                    std::ostringstream errstr;
                    if(!VsnapUtil::construct_persistent_vsnap_filename(volumename, vsnapinfo.target, location))
                    {
                        errstr << "Failed to construct vsnap persistent information for " << volumename << " " << FUNCTION_NAME << " " << LINE_NO << std::endl;
                        error = errstr.str().c_str();
                        DebugPrintf(SV_LOG_DEBUG, "%s", errstr.str().c_str());
                        return false;
                    }

                    // PR#10815: Long Path support
                    if((ACE_OS::unlink(getLongPathName(location.c_str()).c_str()) < 0) && (ACE_OS::last_error() != ENOENT))
                    {
                        errstr << "Unable to delete persistent information for " << volumename 
                            << " with error " << ACE_OS::strerror(ACE_OS::last_error()) << "\n";
                        error = errstr.str().c_str();
                        DebugPrintf(SV_LOG_DEBUG, "%s", errstr.str().c_str());
                        return false;
                    }
                    DebugPrintf(SV_LOG_DEBUG, "Delete vsnap persistent information for %s succeeded\n",
                        VirtualInfo->VolumeName.c_str());
                }
                bool cxnotifystatus = false;
                if(SendStatus)
                    cxnotifystatus = SendUnmountStatus(0, VirtualInfo, vsnapinfo);
                if((!cxnotifystatus) && ShouldDeleteVsnapLogs)
                {
                    ACE_HANDLE handle = ACE_INVALID_HANDLE;
                    if(!VsnapUtil::create_pending_persistent_vsnap_file_handle(volumename,
                        vsnapinfo.target,handle))
                    {
                        DebugPrintf(SV_LOG_ERROR, "The vsnap device %s will not be removed from CX UI. Use force delete option to delete the vsnap from CX UI\n",
                            volumename.c_str());
                        return false;
                    }
                    ACE_OS::close(handle);
                }
                DebugPrintf(SV_LOG_INFO, "Unmount suceeded for the vsnap %s\n",
                    VirtualInfo->VolumeName.c_str());
                //unlock the vsnap locked bookmark	
                LocalConfigurator localConfigurator;
                if((!vsnapinfo.tag.empty()) && localConfigurator.isRetainBookmarkForVsnap())
                {
                    std::string dbname;
                    if(getCdpDbName(vsnapinfo.target,dbname ))
                    {
                        if(std::equal(dbname.begin() + dbname.size() - CDPV3DBNAME.size(), dbname.end(), CDPV3DBNAME.begin()))
                        {
                            SV_USHORT newstatus= BOOKMARK_STATE_UNLOCKED;
                            SV_USHORT oldstatus= BOOKMARK_STATE_LOCKEDBYVSNAP;
                            if(!updateBookMarkLockStatus(dbname,vsnapinfo.tag,oldstatus,newstatus))
                            {
                                DebugPrintf(SV_LOG_DEBUG, "Unable to unlock the bookmark %s in the db.\n",
                                    vsnapinfo.tag.c_str());
                            }
                        }
                    }
                    else
                    {
                        DebugPrintf(SV_LOG_DEBUG, "Unable to get the db name. So the bookmark %s remains in locked state.\n",
                            vsnapinfo.tag.c_str());
                    }
                }
            }
            else
            {
                DebugPrintf(SV_LOG_INFO, "Unmount failed for the vsnap %s\n",
                    VirtualInfo->VolumeName.c_str());
                if(SendStatus)
                    SendUnmountStatus(1, VirtualInfo, vsnapinfo);
            }
        }
        else
        {
            DebugPrintf(SV_LOG_INFO, "The vsnap %s does not exist.\n",
                VirtualInfo->VolumeName.c_str());
            VirtualInfo->Error.VsnapErrorStatus = VSNAP_INVALID_VIRTUAL_VOLUME;
            VirtualInfo->Error.VsnapErrorCode = 0;
        }
    }

    return true;
}


bool WinVsnapMgr::ValidateSourceVolumeOfVsnapPair(VsnapPairInfo *PairInfo)
{
    char				SrcVolume[2048];
    VsnapSourceVolInfo	*SrcInfo = PairInfo->SrcInfo;
    VsnapVirtualVolInfo	*VirtualInfo = PairInfo->VirtualInfo;

    inm_strcpy_s(SrcVolume, ARRAYSIZE(SrcVolume), SrcInfo->VolumeName.c_str());
    TruncateTrailingBackSlash(SrcVolume);

    //if(!(((SrcVolume[0] >= 'a') && (SrcVolume[0] <= 'z')) || ((SrcVolume[0] >= 'A') && (SrcVolume[0] <= 'Z'))))
    //{
    //    VirtualInfo->Error.VsnapErrorStatus = VSNAP_INVALID_SOURCE_VOLUME;
    //    VirtualInfo->Error.VsnapErrorCode = 0;
    //    //VsnapErrorPrint(PairInfo);
    //    return false;
    //}

    if(strlen(SrcVolume) == 1)
    {
        SrcVolume[1] = ':';
        SrcVolume[2] = '\0';
    }


    if(!GetVsnapSourceVolumeDetails(SrcVolume, PairInfo))
    {
        //VsnapErrorPrint(PairInfo);
        return false;
    }

    SrcInfo->VolumeName = SrcVolume;

    return true;
}

bool WinVsnapMgr::ValidateVirtualVolumeOfVsnapPair(VsnapPairInfo *PairInfo, bool & ShouldUnmount)
{
    char				VirtualVolume[2048];
    int					vollen = 0;	
    VsnapVirtualVolInfo *VirtualInfo = PairInfo->VirtualInfo;
    VsnapErrorInfo		*Error = &PairInfo->VirtualInfo->Error;

    inm_strcpy_s(VirtualVolume, ARRAYSIZE(VirtualVolume), VirtualInfo->VolumeName.c_str());
    TruncateTrailingBackSlash(VirtualVolume);
    std::string virtualVolume = VirtualVolume;
    std::string vol;

    vol = virtualVolume;

    if(*(vol.c_str() + vol.size() - 1) != '\\')
        vol += '\\';


    vollen = (int)strlen(VirtualVolume);


    if(!(((VirtualVolume[0] >= 'a') && (VirtualVolume[0] <= 'z')) || ( (VirtualVolume[0] >= 'A') && (VirtualVolume[0] <= 'Z'))))
    {
        Error->VsnapErrorStatus = VSNAP_INVALID_VIRTUAL_VOLUME;
        Error->VsnapErrorCode = 0;
        return false;
    }

    if(vollen >= 2 && VirtualVolume[1] != ':' )
    {
        Error->VsnapErrorStatus = VSNAP_INVALID_VIRTUAL_VOLUME;
        Error->VsnapErrorCode = 0;
        return false;
    }

    if( vollen >= 3  && VirtualVolume[2] != '\\')
    {
        Error->VsnapErrorStatus = VSNAP_INVALID_VIRTUAL_VOLUME;
        Error->VsnapErrorCode = 0;
        return false;
    }

    if(vollen <= 3)
    {
        VirtualVolume[0] = toupper(VirtualVolume[0]);
        VirtualVolume[1] = ':';
        VirtualVolume[2] = '\\';
        VirtualVolume[3] = '\0';
    }

    //
    //	Bug #6133
    //	check for vsnap on windows:
    //
    //
    //	In  case of recovery: Drive Letter/Mount point should be non existent
    //	eg: E: = E should be a non existent drive
    //	eg: E:\mnt = E:\mnt can exist but no volumes beneath it
    //
    //	In case of schedule,
    //	eg: E: = E should be non existent drive or an earlier vsnap
    //	eg: E:\mnt = E:\mnt can exist but no volumes beneath it. E:\mnt can be a mounted vsnap
    //

    std::string mountedVolumes;

    if(vollen == 2)
    {	
        if(VirtualInfo->VsnapType == VSNAP_RECOVERY)
        {
            if(IsFileORVolumeExisting(virtualVolume))
            {
                Error->VsnapErrorStatus = VSNAP_VIRTUAL_DRIVELETTER_INUSE;
                Error->VsnapErrorCode = 0;//ACE_OS::last_error();
                return false;
            }
        }
        else if(VirtualInfo->VsnapType == VSNAP_POINT_IN_TIME)
        {
            if(IsFileORVolumeExisting(virtualVolume) && (!IsVolumeVirtual(vol.c_str())))
            {
                Error->VsnapErrorStatus = VSNAP_VIRTUAL_DRIVELETTER_INUSE;
                Error->VsnapErrorCode = 0;//ACE_OS::last_error();
                return false;
            }
        }
    }

    mountedVolumes.clear();

    if(vollen >= 3)	
    {
        if(VirtualInfo->VsnapType == VSNAP_RECOVERY)
        {
            if(IsFileORVolumeExisting(virtualVolume))
            {
                Error->VsnapErrorStatus = VSNAP_VIRTUAL_DRIVELETTER_INUSE;
                Error->VsnapErrorCode = 0;//ACE_OS::last_error();
                return false;
            }
        }
        else if(VirtualInfo->VsnapType == VSNAP_POINT_IN_TIME)
        {
            if(IsFileORVolumeExisting(virtualVolume) && (!IsVolumeVirtual(vol.c_str())))
            {
                Error->VsnapErrorStatus = VSNAP_VIRTUAL_DRIVELETTER_INUSE;
                Error->VsnapErrorCode = 0;//ACE_OS::last_error();
                return false;
            }
        }
        if(isTargetVolume(virtualVolume.c_str()))
        {
            Error->VsnapErrorStatus = VSNAP_VIRTUAL_DRIVELETTER_INUSE;
            Error->VsnapErrorCode = 0;//ACE_OS::last_error();
            return false;
        }
        /*if(IsVolumeLocked(virtualVolume.c_str()))
        {
            Error->VsnapErrorStatus = VSNAP_TARGET_HIDDEN;
            Error->VsnapErrorCode = ACE_OS::last_error();
            return false;
        }*/

    }
    if((vollen > 3) && !IsMountPointValid(VirtualVolume, PairInfo, ShouldUnmount))
    {
        return false;
    }
    else if(!CheckForVirtualVolumeReusability(PairInfo, ShouldUnmount))
        return false;

    VirtualInfo->VolumeName = VirtualVolume;

    return true;
}

void WinVsnapMgr::PerformMountOrRemount(VsnapSourceVolInfo *SourceInfo)
{
    bool			Success = true;
    bool			SourceValidated = false;
    string			MapFileDir;
    VsnapMountInfo	MountData;	
    VsnapPairInfo	PairInfo;
    SV_ULONG		StartTime = 0;
    string			DisplayString;
    char			SnapDrives[2048];
    std::vector<char> vlog(2048, '\0');

    StartTick(StartTime);
    memset(&MountData, 0, sizeof(VsnapMountInfo));

    VsnapVirtualIter VirtualIterBegin = SourceInfo->VirtualVolumeList.begin();
    VsnapVirtualIter VirtualIterEnd = SourceInfo->VirtualVolumeList.end();

    SourceInfo->FsData = NULL;
    SourceInfo->FsType = VSNAP_FS_UNKNOWN;
    SourceInfo->SectorSize = 0;
    if(SourceInfo->skiptargetchecks)
        SourceInfo->SectorSize = 512;
    else
    {
        SourceInfo->ActualSize = 0;
        SourceInfo->UserVisibleSize = 0;
    }
    LocalConfigurator localConfigurator;

    std::string recoveryLock = SourceInfo->VolumeName;
    FormatVolumeName(recoveryLock);

    // while purge operation is in progress, we do not want
    // any recovery jobs to access the retention files
    CDPLock purge_lock(recoveryLock, true, ".purgelock");
    purge_lock.acquire_read();

    CDPLock recoverCdpLock(recoveryLock);
    if(!recoverCdpLock.acquire_read())
    {
		for( ; VirtualIterBegin != VirtualIterEnd ; ++VirtualIterBegin )
		{
			VsnapVirtualVolInfo *VirtualInfo = (VsnapVirtualVolInfo *)*VirtualIterBegin;
			VirtualInfo->Error.VsnapErrorCode = 0;
			VirtualInfo->Error.VsnapErrorStatus = VSNAP_ACQUIRE_RECOVERY_LOCK_FAILED;
			VirtualInfo->VsnapProgress = VSNAP_FAILED_STATUS;
		}
        DebugPrintf(SV_LOG_ERROR, "%s%s%s","cdpcli could not acquire read lock on volume.",recoveryLock.c_str(),"\nPlease stop any process (such as svagent) accessing the volume and try again.\n");
        return;
    }

    for( ; VirtualIterBegin != VirtualIterEnd ; ++VirtualIterBegin )
    {
        bool			ShouldUnmount = false;
        VsnapVirtualVolInfo *VirtualInfo = (VsnapVirtualVolInfo *)*VirtualIterBegin;
        bool prescript_executed = false;
        VirtualInfo->Error.VsnapErrorCode = 0;
        VirtualInfo->Error.VsnapErrorStatus = VSNAP_SUCCESS;
        VirtualInfo->VsnapProgress = VSNAP_FAILED_STATUS;

        PairInfo.SrcInfo = SourceInfo;
        PairInfo.VirtualInfo = VirtualInfo;

        VirtualInfo->VsnapProgress = MAP_START_STATUS;
        SendStatus(&PairInfo);

        if(!ValidateMountArgumentsForPair(&PairInfo))
        {
            VsnapErrorPrint(&PairInfo);
            continue;
        }

        if(!SourceValidated && !ValidateSourceVolumeOfVsnapPair(&PairInfo))
        {
            VsnapErrorPrint(&PairInfo);
            break;
        }

        SourceValidated = true;

        if(!ValidateVirtualVolumeOfVsnapPair(&PairInfo,ShouldUnmount))
        {
            VsnapErrorPrint(&PairInfo);
            continue; 
        }

        std::string dbpath = dirname_r(SourceInfo->RetentionDbPath.c_str());


        if(!VirtualInfo->PrivateDataDirectory.empty())
        {
            MapFileDir = VirtualInfo->PrivateDataDirectory;
            MountData.IsMapFileInRetentionDirectory = false;
            inm_strcpy_s(MountData.RetentionDirectory, ARRAYSIZE(MountData.RetentionDirectory), dbpath.c_str());
            inm_strcpy_s(MountData.PrivateDataDirectory, ARRAYSIZE(MountData.PrivateDataDirectory), VirtualInfo->PrivateDataDirectory.c_str());

            // PR#10815: Long Path support
            if(ACE_OS::access(getLongPathName(MapFileDir.c_str()).c_str(), 0) && SVMakeSureDirectoryPathExists(MapFileDir.c_str()).failed())
            {
                VirtualInfo->Error.VsnapErrorStatus = VSNAP_DATADIR_CREATION_FAILED;
                VirtualInfo->Error.VsnapErrorCode = 0;
                VsnapErrorPrint(&PairInfo);
                continue;
            }
        }
        else
        {
            MapFileDir = dbpath;
            MountData.IsMapFileInRetentionDirectory = true;
            inm_strcpy_s(MountData.RetentionDirectory, ARRAYSIZE(MountData.RetentionDirectory), dbpath.c_str());
            inm_strcpy_s(MountData.PrivateDataDirectory, ARRAYSIZE(MountData.PrivateDataDirectory), dbpath.c_str());
        }

        VirtualInfo->PreScriptExitCode = 0;
        VirtualInfo->PostScriptExitCode = 0;

        std::string output, error;
        VirtualInfo->PreScriptExitCode = RunPreScript(VirtualInfo, SourceInfo, output, error);
        if(VirtualInfo->PreScriptExitCode != 0)
        {
            VirtualInfo->Error.VsnapErrorCode = VirtualInfo->PreScriptExitCode;
            VirtualInfo->Error.VsnapErrorStatus = VSNAP_PRESCRIPT_EXEC_FAILED;
            VirtualInfo->Error.VsnapErrorMessage = error;
            VsnapErrorPrint(&PairInfo); 

            VirtualInfo->PostScriptExitCode = RunPostScript(VirtualInfo, SourceInfo, output, error);

            if (VirtualInfo->PostScriptExitCode != 0 ) 
            {
                VirtualInfo->Error.VsnapErrorCode = VirtualInfo->PostScriptExitCode;
                VirtualInfo->Error.VsnapErrorStatus = VSNAP_POSTSCRIPT_EXEC_FAILED;
                VirtualInfo->Error.VsnapErrorMessage = error;
                VsnapErrorPrint(&PairInfo);
            }

            VirtualInfo->VsnapProgress = VSNAP_FAILED_STATUS; 
            SendStatus(&PairInfo, error);
            continue;
        }

        if(VirtualInfo->State == VSNAP_MOUNT)
        {
            if(VirtualInfo->VsnapType == VSNAP_RECOVERY)
            {											
                VsnapPrint("Creating Recovery Virtual Snapshot...\n");
                if(CDPUtil::ToDisplayTimeOnConsole(VirtualInfo->TimeToRecover, DisplayString))
                    DebugPrintf(SV_LOG_INFO, "Recovery Time: %s ", DisplayString.c_str());
                if(!VirtualInfo->EventName.empty())
                    DebugPrintf(SV_LOG_INFO, "Event Name: %s \n",  VirtualInfo->EventName.c_str());
                else
                    DebugPrintf(SV_LOG_INFO, "\n");

                if(!VirtualInfo->SnapShotId)
                    VirtualInfo->SnapShotId = GetUniqueVsnapId();

                if(!VirtualInfo->SnapShotId)
                {
                    VirtualInfo->Error.VsnapErrorCode = 0;
                    VirtualInfo->Error.VsnapErrorStatus = VSNAP_UNIQUE_ID_FAILED;
                    VsnapErrorPrint(&PairInfo);

                    VirtualInfo->VsnapProgress = VSNAP_FAILED_STATUS; 
                    SendStatus(&PairInfo);
                    continue; 

                }

                VirtualInfo->VsnapProgress = MAP_PROGRESS_STATUS;
                SendStatus(&PairInfo);

                Success = ParseCDPDatabase(MapFileDir, &PairInfo);
                if(!Success)
                {					
                    VsnapErrorPrint(&PairInfo);
                    VirtualInfo->Error.VsnapErrorCode = 0;
                    std::string output, error;
                    VirtualInfo->PostScriptExitCode = RunPostScript(VirtualInfo, SourceInfo, output, error);


                    if(VirtualInfo->PostScriptExitCode != 0 )
                    {
                        VirtualInfo->Error.VsnapErrorCode = VirtualInfo->PostScriptExitCode ;
                        VirtualInfo->Error.VsnapErrorStatus = VSNAP_POSTSCRIPT_EXEC_FAILED;
                        VirtualInfo->Error.VsnapErrorMessage = error;
                        VsnapErrorPrint(&PairInfo);
                    }

                    VirtualInfo->VsnapProgress = VSNAP_FAILED_STATUS; 
                    SendStatus(&PairInfo, error);
                    continue; 
                }

            }
            else if(VirtualInfo->VsnapType == VSNAP_POINT_IN_TIME)
            {
                SVERROR rc;

                if(SourceInfo->RetentionDbPath.empty())
                {
                    //VirtualInfo->TimeToRecover = 0;
                    SV_ULONGLONG recovery_time = 0;
                    if(!getTimeStampFromLastAppliedFile(SourceInfo->VolumeName,recovery_time))
                    {
                        VirtualInfo->TimeToRecover = 0;
                    }
                    else
                    {
                        VirtualInfo->TimeToRecover = recovery_time;
                    }
                }
                else
                {
#ifdef SV_WINDOWS
                    FormatVolumeNameForCxReporting(SourceInfo->VolumeName);
                    SourceInfo->VolumeName[0]=toupper(SourceInfo->VolumeName[0]);
#endif

                    if(!PollRetentionData(SourceInfo->RetentionDbPath, VirtualInfo->TimeToRecover))
                        VsnapPrint("Not able to get the retention settings...\n");

                    if(strlen(SourceInfo->VolumeName.c_str())< 3)
                        SourceInfo->VolumeName+=":";	

                }	

                VsnapPrint("Creating Point-in-time Virtual Snapshot...\n");
                if(!VirtualInfo->SnapShotId)
                    VirtualInfo->SnapShotId = GetUniqueVsnapId();
                if(!VirtualInfo->SnapShotId)
                {
                    VirtualInfo->Error.VsnapErrorCode = 0;
                    VirtualInfo->Error.VsnapErrorStatus = VSNAP_UNIQUE_ID_FAILED;
                    VsnapErrorPrint(&PairInfo);

                    VirtualInfo->VsnapProgress = VSNAP_FAILED_STATUS; 
                    SendStatus(&PairInfo);
                    continue; 

                }				
                SendProgressToCx(&PairInfo, 0,0);

                if(!InitializePointInTimeVsnap(MapFileDir, &PairInfo,VirtualInfo->TimeToRecover))
                {
                    VirtualInfo->Error.VsnapErrorCode = 0;
                    VirtualInfo->Error.VsnapErrorStatus = VSNAP_PIT_INIT_FAILED;
                    VsnapErrorPrint(&PairInfo);

                    std::string output, error;
                    VirtualInfo->PostScriptExitCode = RunPostScript(VirtualInfo, SourceInfo, output, error);

                    if (VirtualInfo->PostScriptExitCode != 0 ) 
                    {
                        VirtualInfo->Error.VsnapErrorCode = VirtualInfo->PostScriptExitCode;
                        VirtualInfo->Error.VsnapErrorStatus = VSNAP_POSTSCRIPT_EXEC_FAILED;
                        VirtualInfo->Error.VsnapErrorMessage = error;
                        VsnapErrorPrint(&PairInfo);
                    }

                    VirtualInfo->VsnapProgress = VSNAP_FAILED_STATUS; 
                    SendStatus(&PairInfo, error);
                    continue;
                }

                SendProgressToCx(&PairInfo, 100,0);
            }
            else
            {
                VsnapErrorPrint(&PairInfo);
                continue; 
            }
        }
        else if(VirtualInfo->State == VSNAP_REMOUNT)
        {	
            if(!InitializeRemountVsnap(MapFileDir, &PairInfo))
            {
                VsnapErrorPrint(&PairInfo);
                continue;
            }
        }
        else
        {
            VsnapErrorPrint(&PairInfo);
            continue;
        }

        VirtualInfo->VsnapProgress = MOUNT_START;
        SendStatus(&PairInfo);
        MountData.VolumeSize = SourceInfo->ActualSize;
        MountData.VolumeSectorSize = SourceInfo->SectorSize;
        MountData.SnapShotId = VirtualInfo->SnapShotId;
        MountData.NextDataFileId = VirtualInfo->NextFileId;
        MountData.RecoveryTime = VirtualInfo->TimeToRecover;
        inm_strcpy_s(MountData.VolumeName, ARRAYSIZE(MountData.VolumeName), SourceInfo->VolumeName.c_str());
        std::string retpath = SourceInfo->RetentionDbPath;
        if( (retpath.size() > CDPV3DBNAME.size()) &&
            (std::equal(retpath.begin() + retpath.size() - CDPV3DBNAME.size(), retpath.end(), CDPV3DBNAME.begin()))
            )
        {
            MountData.IsSparseRetentionEnabled = true;
        }
        else
            MountData.IsSparseRetentionEnabled = false;

        MountData.AccessType = VirtualInfo->AccessMode;
        inm_strcpy_s(MountData.VirtualVolumeName, ARRAYSIZE(MountData.VirtualVolumeName), VirtualInfo->VolumeName.c_str());

        if(VirtualInfo->AccessMode == VSNAP_READ_WRITE_TRACKING)
            MountData.IsTrackingEnabled = true;
        else
            MountData.IsTrackingEnabled = false;

        inm_sprintf_s(&vlog[0], vlog.size(), "Mounting the virtual volume %s\n", VirtualInfo->VolumeName.c_str());
        VsnapPrint(vlog.data());

        inm_strcpy_s(SnapDrives, ARRAYSIZE(SnapDrives), VirtualInfo->VolumeName.c_str());

        if(strlen(SnapDrives) > 3)
            _strlwr(SnapDrives);


        if(VirtualInfo->VolumeName.size() > 3 && (SnapDrives[(strlen(VirtualInfo->VolumeName.c_str())-1)] != '\\'))
        {
            SnapDrives[VirtualInfo->VolumeName.size()] = '\\';
            SnapDrives[VirtualInfo->VolumeName.size()+1] = '\0';
        }

        /* Critical Section starts here. Hold global vsnaplib lock and synchronize
        mount and unmount operations. */

        HANDLE SemObject = INVALID_HANDLE_VALUE;

        try {

            if(ShouldUnmount)
            {
                VsnapVirtualVolInfo *VirVol = new VsnapVirtualVolInfo();
                if(!VirVol)
                    break;

                VirVol->VolumeName = VirtualInfo->VolumeName;
                LockSemaphoreObject(VsnapGlobalLockName, SemObject);			
                VsnapVirtualInfoList VirList;
                VirList.push_back(VirVol);
                Unmount(VirList, true, false);
                UnLockSemaphoreObject(SemObject);
                delete VirVol;
            }

            LockSemaphoreObject(VsnapGlobalLockName, SemObject);
            bool fsmount=false;
            if(!MountVol(VirtualInfo, &MountData, SnapDrives,fsmount))
            {
                std::string output, error;
                UnLockSemaphoreObject(SemObject);
                SemObject = INVALID_HANDLE_VALUE;
                VirtualInfo->PostScriptExitCode = RunPostScript(VirtualInfo, SourceInfo, output, error);
                if (VirtualInfo->PostScriptExitCode != 0 ) 
                {
                    VirtualInfo->Error.VsnapErrorCode = VirtualInfo->PostScriptExitCode;
                    VirtualInfo->Error.VsnapErrorStatus = VSNAP_POSTSCRIPT_EXEC_FAILED;
                    VirtualInfo->Error.VsnapErrorMessage = error;

                }
                VsnapErrorPrint(&PairInfo);
                CleanupFiles(&PairInfo);

                VirtualInfo->VsnapProgress = VSNAP_FAILED_STATUS;
                // PR #4160
                // VsnapErrorPrint would have already sent the status
                // along with the message if VsnapErrorStatus >=0
                // if we sent it again, we would overwrite the message
                // so, we will sent it only if VsnapErrorPrint did not
                // send it.
                if(VirtualInfo->Error.VsnapErrorStatus <= 0)
                    SendStatus(&PairInfo);
            }
            else
            {
				UnmountAllMountPointInVsnap(VirtualInfo->VolumeName);
                if((MountData.IsSparseRetentionEnabled) && (!VirtualInfo->EventName.empty())
                    && (localConfigurator.isRetainBookmarkForVsnap()))
                {
                    SV_USHORT newstatus= BOOKMARK_STATE_LOCKEDBYVSNAP;
                    SV_USHORT oldstatus= BOOKMARK_STATE_UNLOCKED;
                    if(!updateBookMarkLockStatus(SourceInfo->RetentionDbPath,VirtualInfo->EventName,oldstatus,newstatus))
                    {
                        DebugPrintf(SV_LOG_DEBUG, "Unable to update the bookmark %s as locked in the db. So the tag based recovery vsnap may not be exist after coallesce. \n",
                            VirtualInfo->EventName.c_str());
                    }                    
                }
                inm_sprintf_s(&vlog[0], vlog.size(), "Virtual volume %s mounted successfully, VsnapId: " ULLSPEC "\n\n",
                    VirtualInfo->VolumeName.c_str(), VirtualInfo->SnapShotId);
                DebugPrintf(SV_LOG_INFO, "%s\n", vlog.data());
                UnLockSemaphoreObject(SemObject);
                SemObject = INVALID_HANDLE_VALUE;
                std::string output, error;
                VirtualInfo->PostScriptExitCode = RunPostScript(VirtualInfo, SourceInfo, output, error);

                std::ostringstream InfoForCx;
                if(VirtualInfo->PostScriptExitCode != 0)
                {
                    InfoForCx << "Postscript " << VirtualInfo->PostScript << " execution Failed.\n";
                }

                VsnapDebugPrint(&PairInfo, SV_LOG_DEBUG);
                VirtualInfo->VsnapProgress = VSNAP_COMPLETE_STATUS;


                if(!InfoForCx.str().empty() && m_SVServer.empty())
                    DebugPrintf(SV_LOG_INFO, "%s", InfoForCx.str().c_str());
                bool notifyToCx = false;
                notifyToCx = SendStatus(&PairInfo, InfoForCx.str().c_str());
                //change 9231 start
                if(!notifyToCx)
                {
                    if(!PersistPendingVsnap(MountData.VirtualVolumeName, MountData.VolumeName ))
                    {
                        DebugPrintf(SV_LOG_ERROR, "The vsnap %s will not appear in the CX UI.\n",
                            MountData.VirtualVolumeName);
                    }
                }
                //change 9231 end

                EndTick(StartTime);
            }
        }
        catch (...) 
        {
            UnLockSemaphoreObject(SemObject);
        }
    }

    if(SourceInfo->FsData)
        free(SourceInfo->FsData);

    return;
}

/*
* FUNCTION NAME : WinVsnapMgr::RemountV2
*
* DESCRIPTION : Remounts Vsnap
*				
* INPUT PARAMETERS : VsnapPersistInfo
*
* OUTPUT PARAMETERS : None
*
* NOTES : 
*
* return value : true if success, else false
*
*/
bool WinVsnapMgr::RemountV2(const VsnapPersistInfo& remountInfo)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s for %s\n", FUNCTION_NAME, remountInfo.devicename.c_str());

    bool			rv = true;

    try 
    {
        do 
        {
            VsnapMountInfo	MountData;	
            SV_ULONG		StartTime = 0;
            char			SnapDrives[2048];

            StartTick(StartTime);
            memset(&MountData, 0, sizeof(VsnapMountInfo));
            memset(&SnapDrives, 0, sizeof(SnapDrives));


            VsnapVirtualVolInfo VirtualInfo;
            VsnapSourceVolInfo SrcInfo;

            VsnapPairInfo PairInfo;
            PairInfo.SrcInfo = &SrcInfo;
            PairInfo.VirtualInfo = &VirtualInfo;

            MountData.IsMapFileInRetentionDirectory = boost::lexical_cast<bool>(remountInfo.mapfileinretention);
            inm_strncpy_s(MountData.RetentionDirectory, ARRAYSIZE(MountData.RetentionDirectory), remountInfo.retentionpath.c_str(), remountInfo.retentionpath.size());
            inm_strncpy_s(MountData.PrivateDataDirectory, ARRAYSIZE(MountData.PrivateDataDirectory), remountInfo.datalogpath.c_str(), remountInfo.datalogpath.size());

            // PR#10815: Long Path support
            if(ACE_OS::access(getLongPathName(MountData.PrivateDataDirectory).c_str(), 0) && SVMakeSureDirectoryPathExists(MountData.PrivateDataDirectory).failed())
            {
                DebugPrintf(SV_LOG_ERROR, "%s  %d: Error while accessing %s\n", FUNCTION_NAME,
                    LINE_NO, MountData.PrivateDataDirectory);
                rv = false;
                break;
            }

            std::string MapFileDir = MountData.IsMapFileInRetentionDirectory ? 
                MountData.RetentionDirectory : MountData.PrivateDataDirectory;

            PairInfo.SrcInfo->RetentionDbPath = MapFileDir;
            PairInfo.VirtualInfo->SnapShotId = remountInfo.snapshot_id;
            //Bug: 10702, previously these values are not set, so on second reboot
            //vsnap are unmounted
            PairInfo.VirtualInfo->State = VSNAP_REMOUNT;
            PairInfo.VirtualInfo->VolumeName = remountInfo.mountpoint;
            PairInfo.VirtualInfo->EventName = remountInfo.tag;

            if(!InitializeRemountVsnap(MapFileDir, &PairInfo))
            {
                DebugPrintf(SV_LOG_ERROR, "%s  %d: Error accessing vsnap metadata present in %s\n", FUNCTION_NAME,
                    LINE_NO, MapFileDir.c_str());
                rv = false;
                break;
            }

            MountData.VolumeSize = remountInfo.volumesize;
            MountData.VolumeSectorSize = remountInfo.sectorsize;
            MountData.SnapShotId = remountInfo.snapshot_id;
            MountData.NextDataFileId = 0;
            MountData.RecoveryTime = remountInfo.recoverytime;
            inm_strncpy_s(MountData.VolumeName, ARRAYSIZE(MountData.VolumeName), remountInfo.target.c_str(), remountInfo.target.size());
            std::string dbpath = m_TheConfigurator->getCdpDbName(remountInfo.target);
            if( (dbpath.size() > CDPV3DBNAME.size()) &&
                (std::equal(dbpath.begin() + dbpath.size() - CDPV3DBNAME.size(), dbpath.end(), CDPV3DBNAME.begin()))
                )
            {
                MountData.IsSparseRetentionEnabled = true;
            }
            else
                MountData.IsSparseRetentionEnabled = false;

            switch(remountInfo.accesstype)
            {
            case 0:
                MountData.AccessType = VSNAP_READ_ONLY;
                break;
            case 1:
                MountData.AccessType = VSNAP_READ_WRITE;
                break;
            case 2:
                MountData.AccessType = VSNAP_READ_WRITE_TRACKING;
                break;
            }

            inm_strncpy_s(MountData.VirtualVolumeName, ARRAYSIZE(MountData.VirtualVolumeName), remountInfo.mountpoint.c_str(), remountInfo.mountpoint.size());
            MountData.IsTrackingEnabled = (MountData.AccessType == VSNAP_READ_WRITE_TRACKING);

            DebugPrintf(SV_LOG_INFO,"Mounting the virtual volume %s\n", remountInfo.mountpoint.c_str());

            inm_strncpy_s(SnapDrives, ARRAYSIZE(SnapDrives), remountInfo.mountpoint.c_str(), remountInfo.mountpoint.size());

            if(strlen(SnapDrives) > 3)
                _strlwr(SnapDrives);


            if(remountInfo.mountpoint.size() > 3 && (SnapDrives[(remountInfo.mountpoint.size()-1)] != '\\'))
            {
                SnapDrives[remountInfo.mountpoint.size()] = '\\';
                SnapDrives[remountInfo.mountpoint.size()+1] = '\0';
            }

            /* Critical Section starts here. Hold global vsnaplib lock and synchronize
            mount and unmount operations. */

            HANDLE SemObject = INVALID_HANDLE_VALUE;

            LockSemaphoreObject(VsnapGlobalLockName, SemObject);
            bool fsmount=false;
            if(!MountVol(&VirtualInfo, &MountData, SnapDrives,fsmount))
            {
                UnLockSemaphoreObject(SemObject);
                SemObject = INVALID_HANDLE_VALUE;
                /*CleanupFiles(&PairInfo);*/
                DebugPrintf(SV_LOG_ERROR, "%s  %d: Unable to mount vsnap %s\n", FUNCTION_NAME,
                    LINE_NO, SnapDrives);
                rv = false;
            }
            else
            {
				UnmountAllMountPointInVsnap(remountInfo.mountpoint);
                LocalConfigurator localConfigurator;
                if((MountData.IsSparseRetentionEnabled) && (!remountInfo.tag.empty())
                    && (localConfigurator.isRetainBookmarkForVsnap()))
                {
                    SV_USHORT newstatus= BOOKMARK_STATE_LOCKEDBYVSNAP;
                    SV_USHORT oldstatus= BOOKMARK_STATE_UNLOCKED;
                    if(!updateBookMarkLockStatus(dbpath,remountInfo.tag,oldstatus,newstatus))
                    {
                        DebugPrintf(SV_LOG_DEBUG, "Unable to update the bookmark %s as locked in the db. So the tag based recovery vsnap may not be exist after coallesce. \n",
                            remountInfo.tag.c_str());
                    }                    
                }
                std::ostringstream ostr;
                ostr << "Virtual volume " << SnapDrives 
                    <<	" mounted successfully, VsnapId: "
                    << MountData.SnapShotId;
                UnLockSemaphoreObject(SemObject);
                SemObject = INVALID_HANDLE_VALUE;
                EndTick(StartTime);
            }
        } while (0);
    }
    catch ( ContextualException& ce )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
            "\n%s encountered exception %s.\n",
            FUNCTION_NAME, ce.what());
    }
    catch( exception const& e )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
            "\n%s encountered exception %s.\n",
            FUNCTION_NAME, e.what());
    }
    catch ( ... )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
            "\n%s encountered unknown exception.\n",
            FUNCTION_NAME);
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s for %s\n", FUNCTION_NAME, remountInfo.devicename.c_str());

    return rv;
}


void WinVsnapMgr::ProcessVolumeAndMountPoints(string & MountPoints, char *Vol, char *Buf, int iBufSize, bool UnmountFlag)
{
    bool bFlag;           // generic results flag for return
    ACE_HANDLE hPt;           // handle for mount point scan
    char PtBuf[2048]; // string buffer for mount points
    SV_ULONG dwSysFlags;     // flags that describe the file system
    char FileSysNameBuf[256];
    char FullMntPt[2048];

    // PR#10815: Long Path support
    SVGetVolumeInformation( Buf, NULL, 0, NULL, NULL, &dwSysFlags, FileSysNameBuf, 256);


    if (! (dwSysFlags & FILE_SUPPORTS_REPARSE_POINTS)) 
        return;

    else 
    {
        hPt = FindFirstVolumeMountPoint(Buf, PtBuf, 2048 );

        if (hPt == ACE_INVALID_HANDLE)
        {
            return;
        } 
        else 
        {
            inm_strcpy_s(FullMntPt, ARRAYSIZE(FullMntPt), Vol);
            inm_strcat_s(FullMntPt, ARRAYSIZE(FullMntPt), PtBuf);

            _strlwr(FullMntPt);
            if(IsVolumeVirtual(FullMntPt))
            {

                if(UnmountFlag)
                {
                    VsnapVirtualInfoList VirtualList;
                    VsnapVirtualVolInfo VirtualInfo;

                    VirtualInfo.VolumeName = FullMntPt;
                    VirtualInfo.State = VSNAP_UNMOUNT;
                    VirtualList.push_back(&VirtualInfo);

                    Unmount(VirtualList, true);	
                }

                MountPoints += FullMntPt;
                MountPoints += ",";
            }

            bFlag = FindNextVolumeMountPoint(hPt, PtBuf, 2048);

            while (bFlag)
            {
                inm_strcpy_s(FullMntPt, ARRAYSIZE(FullMntPt), Vol);
                inm_strcat_s(FullMntPt, ARRAYSIZE(FullMntPt), PtBuf);

                _strlwr(FullMntPt);
                if(IsVolumeVirtual(FullMntPt))
                {
                    if(UnmountFlag)
                    {
                        VsnapVirtualInfoList VirtualList;
                        VsnapVirtualVolInfo VirtualInfo;

                        VirtualInfo.VolumeName = FullMntPt;
                        VirtualInfo.State = VSNAP_UNMOUNT;
                        VirtualList.push_back(&VirtualInfo);

                        Unmount(VirtualList, true);	
                    }

                    MountPoints += FullMntPt;
                    MountPoints += ",";
                }

                bFlag = 
                    FindNextVolumeMountPoint(hPt, PtBuf, 2048);
            }

            FindVolumeMountPointClose(hPt);
        }
    }
    return;
}

void WinVsnapMgr::GetAllVirtualVolumes(string & Volumes)
{
    return GetOrUnmountAllVirtualVolumes(Volumes, false);
}

void WinVsnapMgr::GetOrUnmountAllVirtualVolumes(string & MountPoints, bool DoUnmount)
{

    SV_ULONG LogicalDrives = GetLogicalDrives();

    int i;
    char vol[4];
    char VolumeName[2048];

    vol[0] = _T('A') - 1;
    vol[1] = _T(':');
    vol[2] = _T( '\\');
    vol[3] = _T('\0');

    for( i = 0; i < 26; i++ )
    {
        //Bug# 7934
        if(VsnapQuit())
        {
            DebugPrintf(SV_LOG_INFO, "Vsnap Operation Aborted as Quit Requested\n");
            break;
        }

        SV_ULONG dwDriveMask = 1 << i;
        vol[0]++;

        if(LogicalDrives & dwDriveMask)
        {
            if((DRIVE_FIXED == GetDriveType(vol)) && GetVolumeNameForVolumeMountPoint(vol, VolumeName, 2048))
            {
                vol[0] = vol[0] + 32;
                ProcessVolumeAndMountPoints(MountPoints, vol, VolumeName, 2048, DoUnmount);
                vol[0] = vol[0] - 32;
                if(IsVolumeVirtual(vol))
                {
                    if(DoUnmount)
                    {
                        VsnapVirtualInfoList VirtualList;
                        VsnapVirtualVolInfo VirtualInfo;

                        VirtualInfo.VolumeName = vol;
                        VirtualInfo.State = VSNAP_UNMOUNT;
                        VirtualList.push_back(&VirtualInfo);

                        Unmount(VirtualList, true);
                    }
                    MountPoints += vol;
                    MountPoints += ",";
                }
            }

        }
    }
}

bool WinVsnapMgr::IsVolumeVirtual(char const *Vol)
{
    ACE_HANDLE	VVCtrlDevice = ACE_INVALID_HANDLE;
    wchar_t	VolumeLink[256];
    int		bResult;
    SV_ULONG   BytesReturned = 0;
    string	UniqueId;

    UniqueId.clear();

    VVCtrlDevice = OpenVirtualVolumeControlDevice();
    if(ACE_INVALID_HANDLE == VVCtrlDevice)
        return false;

    UniqueId = VSNAP_UNIQUE_ID_PREFIX;
    UniqueId += Vol;

    if(strlen(Vol) > 3) {
        std::string::iterator sIter = UniqueId.begin();
        sIter += strlen(VSNAP_UNIQUE_ID_PREFIX);
        std::transform(sIter, UniqueId.end(), sIter, tolower);
    }	

    bResult = DeviceIoControl(VVCtrlDevice, IOCTL_INMAGE_GET_VOLUME_SYMLINK_FOR_RETENTION_POINT, (void *)UniqueId.c_str(),
        (SV_ULONG) (UniqueId.size() + 1), VolumeLink, (SV_ULONG) sizeof(VolumeLink), &BytesReturned, NULL);

    if(bResult)
    {
        ACE_OS::close(VVCtrlDevice);
        return true;
    }

    ACE_OS::close(VVCtrlDevice);
    return false;
}


void WinVsnapMgr::DisconnectConsoleOutAndErrorHdl()
{

    // PR#10815: Long Path support
    NulHdl = SVCreateFile("NUL:", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 
        FILE_ATTRIBUTE_NORMAL, NULL);
    if(NulHdl == ACE_INVALID_HANDLE)
        return;

    StdOutHdl = GetStdHandle(STD_OUTPUT_HANDLE);
    StdErrHdl = GetStdHandle(STD_ERROR_HANDLE);

    bool status;
    SV_ULONG err;
    status = SetStdHandle(STD_OUTPUT_HANDLE, NulHdl);
    if(!status)
        err = ACE_OS::last_error();
    status = SetStdHandle(STD_ERROR_HANDLE, NulHdl);
    if(!status)
        err = ACE_OS::last_error();

    /*CloseHandle(GetStdHandle(STD_OUTPUT_HANDLE));
    CloseHandle(GetStdHandle(STD_ERROR_HANDLE));*/
}


void WinVsnapMgr::ConnectConsoleOutAndErrorHdl()
{
    /*ACE_HANDLE OutHdl = ACE_INVALID_HANDLE;

    OutHdl = CreateFile("CONOUT$", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 
    FILE_ATTRIBUTE_NORMAL, NULL);8
    if(OutHdl == ACE_INVALID_HANDLE)
    return;

    SetStdHandle(STD_OUTPUT_HANDLE, OutHdl);
    SetStdHandle(STD_ERROR_HANDLE, OutHdl);*/

    ACE_OS::close(NulHdl);
    NulHdl = ACE_INVALID_HANDLE;

    SetStdHandle(STD_OUTPUT_HANDLE, StdOutHdl);
    SetStdHandle(STD_ERROR_HANDLE, StdErrHdl);

}



//VsnapVirtualVolInfoTag::VsnapVirtualVolInfoTag()
//{
//	VolumeName = "";
//	PrivateDataDirectory = "";
//	AccessMode = VSNAP_UNKNOWN_ACCESS_TYPE;
//	VsnapType = VSNAP_CREATION_UNKNOWN;
//	TimeToRecover = 0;
//	AppName = "";
//	EventName = "";
//	EventNumber = 0;
//	PreScript = "";
//	PostScript = "";
//	ReuseDriveLetter = false;
//
//	SnapShotId = 0;
//	NextFileId = 0;
//	Error.VsnapErrorCode = 0;
//	Error.VsnapErrorStatus = VSNAP_SUCCESS;
//	State = VSNAP_UNKNOWN;
//	PreScriptExitCode = 0;
//	PostScriptExitCode = 0;
//}

//VsnapSourceVolInfoTag::VsnapSourceVolInfoTag()
//{
//	
//	VolumeName = "";
//	RetentionDbPath = "";
//	SectorSize = 0;
//	VolumeSize = 0;
//	FsData = NULL;
//	FsType = VSNAP_FS_UNKNOWN;
//}

bool WinVsnapMgr::OpenMountPointExclusive(ACE_HANDLE *Handle, char *TgtGuid, const size_t TgtGuidLen, char *TgtVol)
{
    string MountPoint;
    char volGuid[MAX_PATH];
    size_t len;

    MountPoint = TgtVol;

    if(MountPoint[MountPoint.size() - 1] != '\\')
        MountPoint += "\\";

    if( !GetVolumeNameForVolumeMountPoint( MountPoint.c_str(), volGuid, MAX_PATH ))
    {
        DebugPrintf(SV_LOG_INFO, "GetVolumeNameForVolumeMountPoint failed. Invalid mount point specified\n");
        return false;
    }

    volGuid[strlen(volGuid)-1] = '\0';

    //Flush the volume buffers. 
    // PR#10815: Long Path support
    HANDLE hDevice = SVCreateFile( volGuid, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, 
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL );
    if(hDevice != ACE_INVALID_HANDLE)
    {
        SV_ULONG BytesReturned = 0;
        FlushFileBuffers(hDevice);
        //DeviceIoControl((ACE_HANDLE) hDevice, FSCTL_DISMOUNT_VOLUME, NULL, 0, NULL, 0, (LPDWORD) &BytesReturned,(LPOVERLAPPED) NULL);
        ACE_OS::close(hDevice);
    }

    if(!DeleteVolumeMountPoint(MountPoint.c_str()))
    {
        DebugPrintf(SV_LOG_INFO, "DeleteVolumeMountPoint failed. \n");
        return false;
    }

    // PR#10815: Long Path support
    *Handle = ACE_OS::open(getLongPathName(volGuid).c_str(), O_RDWR | FILE_ATTRIBUTE_NORMAL, FILE_SHARE_READ);

    if(ACE_INVALID_HANDLE == *Handle)
    {
        len = strlen(volGuid);

        volGuid[len] = '\\';
        volGuid[len+1] = '\0';

        if( !SetVolumeMountPoint( MountPoint.c_str(), volGuid ) )
        {
            DebugPrintf(SV_LOG_INFO, "Failed to mount the volume back to its original mount point. \n");
            return false;
        }
    }

    len = strlen(volGuid);
    volGuid[len] = '\\';
    volGuid[len+1] = '\0';

    inm_strcpy_s(TgtGuid, TgtGuidLen, volGuid);
    return true;
}

bool WinVsnapMgr::CloseMountPointExclusive(ACE_HANDLE	Handle, char *TgtGuid, char *TgtVol)
{
    string MountPoint;
    if(ACE_INVALID_HANDLE == Handle)
        return false;

    ACE_OS::close(Handle);

    MountPoint = TgtVol;
    if(MountPoint[MountPoint.size() - 1] != '\\')
        MountPoint += "\\";

    if( !SetVolumeMountPoint( MountPoint.c_str(), TgtGuid ) )
    {
        DebugPrintf(SV_LOG_INFO, "SetVolumeMountPoint Failed\n");
        return false;
    }

    return true;
}


long WinVsnapMgr::OpenInVirVolDriver(ACE_HANDLE &hDevice, VsnapErrorInfo *Error)
{
    DRSTATUS Status = DRSTATUS_SUCCESS;
    hDevice = OpenVirtualVolumeControlDevice();
	const size_t MAXDIRPATH = 128;

    if(ACE_INVALID_HANDLE == hDevice)
    {

        INSTALL_INVIRVOL_DATA InstallData;
        InstallData.DriverName = _T("invirvol");
        char SystemDir[MAXDIRPATH];
        GetSystemDirectory(SystemDir, MAXDIRPATH);

        InstallData.PathAndFileName = new TCHAR[MAXDIRPATH];
        inm_tcscpy_s(InstallData.PathAndFileName, MAXDIRPATH, SystemDir);
        inm_tcscat_s(InstallData.PathAndFileName, MAXDIRPATH, _T("\\drivers\\invirvol.sys"));
        Status = InstallInVirVolDriver(InstallData);

        if(!DR_SUCCESS(Status))
        {
            Error->VsnapErrorStatus = VSNAP_DRIVER_INSTALL_FAILED;
            Error->VsnapErrorCode = ACE_OS::last_error();
            return Status;
        }

        START_INVIRVOL_DATA StartData;
        StartData.DriverName = _T("invirvol");

        Status = StartInVirVolDriver(StartData);
        if(!DR_SUCCESS(Status))
        {
            Error->VsnapErrorStatus = VSNAP_DRIVER_LOAD_FAILED;
            Error->VsnapErrorCode = ACE_OS::last_error();
            return Status;
        }

        hDevice = OpenVirtualVolumeControlDevice();

        if(ACE_INVALID_HANDLE == hDevice) {
            Error->VsnapErrorStatus = VSNAP_VIRVOL_OPEN_FAILED;
            Error->VsnapErrorCode = ACE_OS::last_error();
            Status = DRSTATUS_UNSUCCESSFUL;
            return Status;
        }
    }

    return Status;
}

bool WinVsnapMgr::VolumeSupportsReparsePoints(char* VolumeName)
{
    SV_ULONG FileSystemFlags                       = 0;
    SV_ULONG SerialNumber                          = 0;
    bool  bResult                               = TRUE;
    bool  MountPointSupported                   = FALSE;
    char FileSystemNameBuffer[MAX_STRING_SIZE] = {0};


    // PR#10815: Long Path support
    if(!SVGetVolumeInformation(VolumeName, NULL, 0, &SerialNumber, NULL, &FileSystemFlags, 
        FileSystemNameBuffer, ARRAYSIZE(FileSystemNameBuffer)))
    {
        return MountPointSupported;
    }
    if(FileSystemFlags & FILE_SUPPORTS_REPARSE_POINTS && SerialNumber > 0)
        MountPointSupported = TRUE;

    return MountPointSupported;
}

void WinVsnapMgr::DeleteMountPoints(char* TargetVolumeName, VsnapVirtualVolInfo *VirtualInfo)
{
    char   HostVolumeName[2048];
    bool    bResult         = TRUE;
    ACE_HANDLE  hHostVolume;
    hHostVolume=ACE_INVALID_HANDLE;
    hHostVolume     = FindFirstVolume(HostVolumeName, 2048);
    char	errstr[100];

    if(hHostVolume == ACE_INVALID_HANDLE)
    {
        inm_sprintf_s(errstr, ARRAYSIZE(errstr), "Could not enumerate volumes Error:%#x\n", ACE_OS::last_error());
        VsnapPrint(errstr);
        return;
    }

    do
    {
        if(VolumeSupportsReparsePoints(HostVolumeName))
            DeleteMountPointsForTargetVolume(HostVolumeName, TargetVolumeName, VirtualInfo);
        bResult = FindNextVolume(hHostVolume, HostVolumeName, 2048);

    }while(bResult);


    FindVolumeClose(hHostVolume);

}

size_t WinVsnapMgr::tcstombs(char* mbstr, const size_t mbstrlen, const char* tstring)
{
    inm_strcpy_s(mbstr, mbstrlen, tstring);
    return strlen(tstring);
}

size_t WinVsnapMgr::mbstotcs(char* tstring, const size_t tstringlen, const char* mbstr)
{
    inm_strcpy_s(tstring, tstringlen, mbstr);
    return strlen(mbstr);
}

size_t WinVsnapMgr::wcstotcs(char* tstring, const size_t tstringlen, const wchar_t* wcstr)
{
    size_t cntcopied = 0;
    wcstombs_s(&cntcopied, tstring, tstringlen, wcstr, tstringlen-1);
    return cntcopied;
}

// Windows specific mount point validation.
bool WinVsnapMgr::IsMountPointValid(char *MountPoint, VsnapPairInfo *PairInfo, bool & ShouldUnmount)
{
    SV_ULONG			FileSystemFlags = 0;
    SV_ULONG			SerialNumber = 0;
    bool				MountPointSupported= FALSE;
    char				FileSystemNameBuffer[MAX_STRING_SIZE] = {0};
    char				VolumeName[4];
    VsnapVirtualVolInfo	*VirtualInfo = PairInfo->VirtualInfo;

    if(!(((MountPoint[0] >= 'a') && (MountPoint[0] <= 'z')) || ((MountPoint[0] >= 'A') && (MountPoint[0] <= 'Z'))))
    {
        VirtualInfo->Error.VsnapErrorStatus = VSNAP_INVALID_MOUNT_POINT;
        VirtualInfo->Error.VsnapErrorCode = 0;
        return false;
    }

    if(MountPoint[1] != ':')
    {
        VirtualInfo->Error.VsnapErrorStatus = VSNAP_INVALID_MOUNT_POINT;
        VirtualInfo->Error.VsnapErrorCode = 0;
        return false;
    }

    VolumeName[0] = MountPoint[0];
    VolumeName[1] = ':';
    VolumeName[2] = '\\';
    VolumeName[3] = '\0';

    // PR#10815: Long Path support
    if(!SVGetVolumeInformation(VolumeName, NULL, 0, &SerialNumber, NULL, &FileSystemFlags, 
        FileSystemNameBuffer, ARRAYSIZE(FileSystemNameBuffer)))
    {
        VirtualInfo->Error.VsnapErrorStatus = VSNAP_VOLUME_INFO_FAILED;
        VirtualInfo->Error.VsnapErrorCode = 0;
        return false;
    }


    if(!((FileSystemFlags & FILE_SUPPORTS_REPARSE_POINTS) && (SerialNumber > 0)))
    {
        VirtualInfo->Error.VsnapErrorStatus = VSNAP_REPARSE_PT_NOT_SUPPORTED;
        VirtualInfo->Error.VsnapErrorCode = 0;
        return false;
    }

    struct _stat statbuf;
    if(_access(MountPoint, 0) && SVMakeSureDirectoryPathExists(MountPoint).failed())
    {
        VirtualInfo->Error.VsnapErrorStatus = VSNAP_MOUNTDIR_CREATION_FAILED;
        VirtualInfo->Error.VsnapErrorCode = 0;
        return false;
    }
    else
    {
        if(_stat(MountPoint, &statbuf) == -1)
        {
            VirtualInfo->Error.VsnapErrorStatus = VSNAP_MOUNTDIR_STAT_FAILED;
            VirtualInfo->Error.VsnapErrorCode = 0;
            return false;
        }

        if(!(statbuf.st_mode & _S_IFDIR))
        {
            VirtualInfo->Error.VsnapErrorStatus = VSNAP_MOUNTPT_NOT_DIRECTORY;
            VirtualInfo->Error.VsnapErrorCode = 0;
            return false;
        }

        if(!CheckForVirtualVolumeReusability(PairInfo, ShouldUnmount))
            return false;

        if(!DirectoryEmpty(MountPoint))
        {
            VirtualInfo->Error.VsnapErrorStatus = VSNAP_DIRECTORY_NOT_EMPTY;
            VirtualInfo->Error.VsnapErrorCode = 0;
            return false;
        }
    }
    return true;
}

bool WinVsnapMgr::DirectoryEmpty(char *Directory)
{
    WIN32_FIND_DATA FindFileData;

    ACE_HANDLE hFind = ACE_INVALID_HANDLE;
    char DirSpec[MAX_PATH + 1];  // directory specification
    bool empty = true;

    inm_strncpy_s (DirSpec, ARRAYSIZE(DirSpec), Directory, strlen(Directory)+1);
    inm_strncat_s (DirSpec, ARRAYSIZE(DirSpec), "\\*", 3);

    hFind = FindFirstFile(DirSpec, &FindFileData);

    if (hFind != ACE_INVALID_HANDLE) 
    {
        if(strcmp(FindFileData.cFileName, ".") == 0 || strcmp(FindFileData.cFileName, "..") == 0 )
        {
            while (FindNextFile(hFind, &FindFileData) != 0) 
            {
                if(strcmp(FindFileData.cFileName, ".") == 0 || strcmp(FindFileData.cFileName, "..") == 0 )
                    continue;
                else
                {
                    empty = false;
                    break;
                }
            }
        }
        else
        {
            empty = false;
        }

        FindClose(hFind);
    }

    else
    {
        empty = true;
    }
    return empty;
}

bool WinVsnapMgr::CheckForVirtualVolumeReusability(VsnapPairInfo *PairInfo,bool & ShouldUnmount)
{
    VsnapVirtualVolInfo	*VirtualInfo = PairInfo->VirtualInfo;
    VsnapSourceVolInfo	*SrcInfo = PairInfo->SrcInfo;
    bool				tounmount = false;
    int					vollen = (int)VirtualInfo->VolumeName.size();
    bool				Success = true;
    VsnapErrorInfo		*Error = &PairInfo->VirtualInfo->Error;
    bool				VirtualVolumeExists = false;

    do
    {
        if(vollen <= 3)
        {
            DWORD drivemask;

            drivemask = GetLogicalDrives();

            if(drivemask & (1 << (toupper(*(VirtualInfo->VolumeName.c_str())) - 'A')))
                VirtualVolumeExists = true;
        }
        else
        {
            string vol;

            vol = VirtualInfo->VolumeName;

            if(*(vol.c_str() + vol.size() - 1) != '\\')
                vol += '\\';

            DWORD Attributes = GetFileAttributes(VirtualInfo->VolumeName.c_str());
            if(Attributes == INVALID_FILE_ATTRIBUTES)
                break;

            if(IsVolumeVirtual((char*)vol.c_str()))
            {
                VirtualVolumeExists = true;

            }

        }

        if(VirtualInfo->ReuseDriveLetter)
        {
            if(VirtualVolumeExists)
            {
                VsnapVirtualVolInfo *VirVol = new VsnapVirtualVolInfo();
                if(!VirVol)
                    break;

                VirVol->VolumeName = VirtualInfo->VolumeName;

                VsnapVirtualInfoList VirList;
                VirList.push_back(VirVol);

                DebugPrintf(SV_LOG_DEBUG, "VSNAP DEBUG Unmount called from CheckForVirtualVolumeReusability\n");
                Unmount(VirList, true, false);
                delete VirVol;
            }
        }
        else
        {
            if(VirtualVolumeExists)
            {
                Error->VsnapErrorCode = 0;
                Error->VsnapErrorStatus = VSNAP_VIRTUAL_DRIVELETTER_INUSE;
                Success = false;
            }
        }
    } while(FALSE);

    return Success;
}

void WinVsnapMgr::VsnapGetVirtualInfoDetaisFromVolume(VsnapVirtualVolInfo *VirtualInfo, VsnapContextInfo *VsnapContext)
{
    wchar_t				VolumeLink[MAX_STRING_SIZE];
    SV_ULONG			BytesReturned = 0;
    string				LogDir;
    VsnapErrorInfo		*Error = &VirtualInfo->Error;
    char				*Volname = NULL;

    Volname = (char *)malloc(MOUNT_PATH_LEN);
    if(!Volname)
    {
        free(VsnapContext);
        Error->VsnapErrorCode = 0;
        Error->VsnapErrorStatus = VSNAP_MEM_UNAVAILABLE;
        return;
    }

    inm_strcpy_s(Volname, MOUNT_PATH_LEN, VirtualInfo->VolumeName.c_str());

    if(strlen(Volname) <= 3)
        Volname[0] = toupper(Volname[0]);

    if(strlen(Volname) > 3 )
        _strlwr(Volname);

    size_t Vollen = strlen(Volname);

    if(Volname[Vollen-1] != '\\')
    {
        Volname[Vollen] = '\\';
        Volname[Vollen+1] = '\0';
    }

    do
    {
        ACE_HANDLE hDevice;
        hDevice =	ACE_INVALID_HANDLE;	

        if(!IsVolumeVirtual(Volname))
            break;
        hDevice = OpenVirtualVolumeControlDevice();
        if(ACE_INVALID_HANDLE == hDevice)
            break;

        string UniqueId;
        UniqueId.clear();

        UniqueId = VSNAP_UNIQUE_ID_PREFIX;
        UniqueId += Volname;

        bool bResult = DeviceIoControl(hDevice,
            IOCTL_INMAGE_GET_VOLUME_SYMLINK_FOR_RETENTION_POINT,
            (void *)UniqueId.c_str(),
            (SV_ULONG) (UniqueId.size()+ 1),
            VolumeLink,
            (SV_ULONG) sizeof(VolumeLink),
            &BytesReturned,
            NULL);

        UniqueId.clear();

        if(!bResult)
        {
            ACE_OS::close(hDevice);
            break;
        }
        GetVsnapContextFromVolumeLink(hDevice, VolumeLink, VirtualInfo, VsnapContext);

        ACE_OS::close(hDevice);
    } while (FALSE);

    free(Volname);
}

bool WinVsnapMgr::TraverseAndApplyLogs(VsnapSourceVolInfo *SrcInfo)
{
    VsnapVirtualVolInfo		*VirtualInfo = (VsnapVirtualVolInfo *)*(SrcInfo->VirtualVolumeList.begin());
    char					TgtVol[2048];
    std::string				sTgtGuid;
    bool					Success = true;
    std::vector<char>		vMapFileName(2048, '\0');
    VsnapErrorInfo			*Error = &VirtualInfo->Error;
    bool					TgtUnmounted = false;
    SVERROR					sve = SVS_OK;
    ACE_HANDLE				handleVolume = ACE_INVALID_HANDLE;		
    do
    {
        // 1. Dismount the target volume
        // 2. Traverse through the node
        //		1. For each tracking id, read the data from the logs
        //		2. Write the data to the target volume.
        // 3. Exit.

        inm_strcpy_s(TgtVol, ARRAYSIZE(TgtVol), SrcInfo->VolumeName.c_str());

        if(!IsFileORVolumeExisting(TgtVol))
        {
            DebugPrintf(SV_LOG_ERROR,"Invalid Target volume %s\n",TgtVol);
            Success = false;
            Error->VsnapErrorCode = 0;
            Error->VsnapErrorStatus = VSNAP_TARGET_OPEN_FAILED;
            break;
        }

        //if(!(((SrcInfo->VolumeName[0] >= 'a') && (SrcInfo->VolumeName[0] <= 'z')) || 
        //    ((SrcInfo->VolumeName[0] >= 'A') && (SrcInfo->VolumeName[0] <= 'Z'))))
        //{
        //    Error->VsnapErrorCode = 0;
        //    Error->VsnapErrorStatus = VSNAP_INVALID_SOURCE_VOLUME;
        //    Success = false;
        //    break;
        //}

        //if(SrcInfo->VolumeName.size() >= 2 && SrcInfo->VolumeName[1] != ':')
        //{
        //    Error->VsnapErrorCode = 0;
        //    Error->VsnapErrorStatus = VSNAP_INVALID_SOURCE_VOLUME;
        //    Success = false;
        //    break;
        //}

        //if(SrcInfo->VolumeName.size() >= 3 && SrcInfo->VolumeName[2] != '\\')
        //{
        //    Error->VsnapErrorCode = 0;
        //    Error->VsnapErrorStatus = VSNAP_INVALID_SOURCE_VOLUME;
        //    Success = false;
        //    break;
        //}
        if(SrcInfo->VolumeName.size() <= 3)
        { 
            if(!VirtualInfo->VolumeName.empty())
            {
                if(VirtualInfo->VolumeName.size() <= 3)
                {
                    if(IsFileORVolumeExisting(VirtualInfo->VolumeName.c_str()))
                        FlushFileSystemBuffers((char *)VirtualInfo->VolumeName.c_str());
                    else
                    {
                        Error->VsnapErrorStatus = VSNAP_VIRTUAL_VOLUME_NOTEXIST;
                        Error->VsnapErrorCode = 0;
                        Success = false;
                        break;
                    }
                }
                else 
                {
                    char sourceVol[MAX_PATH];
                    if( GetVolumeNameForVolumeMountPoint( VirtualInfo->VolumeName.c_str(), sourceVol, MAX_PATH ))
                        FlushFileSystemBuffers(sourceVol);
                }
            }
            FlushFileSystemBuffers((char *)SrcInfo->VolumeName.c_str());
            sve = OpenVolumeExclusive(&handleVolume, sTgtGuid, TgtVol, TgtVol/*TODO:MPOINT*/, TRUE, FALSE);
            if(sve.failed())
            {
                Error->VsnapErrorCode = 0;
                Error->VsnapErrorStatus = VSNAP_TARGET_OPENEX_FAILED;
                Success = false;
                break;
            }
        }
        else
        {	
            if(!VirtualInfo->VolumeName.empty())
            {
                if(IsFileORVolumeExisting(VirtualInfo->VolumeName.c_str()))
                {
                    char sourceVol[MAX_PATH];
                    if( GetVolumeNameForVolumeMountPoint( VirtualInfo->VolumeName.c_str(), sourceVol, MAX_PATH ))
                        FlushFileSystemBuffers(sourceVol);
                }
                else
                {
                    Error->VsnapErrorStatus = VSNAP_VIRTUAL_VOLUME_NOTEXIST;
                    Error->VsnapErrorCode = 0;
                    Success = false;
                    break;
                }

            }
            sve = OpenVolumeExclusive(&handleVolume, sTgtGuid, TgtVol, TgtVol/*TODO:MPOINT*/, TRUE, FALSE);
            if(sve.failed())
            {
                Error->VsnapErrorCode = 0;
                Error->VsnapErrorStatus = VSNAP_TARGET_OPENEX_FAILED;
                Success = false;
                break;
            }
        }


        ACE_HANDLE Handle;
        inm_sprintf_s(&vMapFileName[0], vMapFileName.size(), "%s%c" ULLSPEC "_VsnapMap", VirtualInfo->PrivateDataDirectory.c_str(), ACE_DIRECTORY_SEPARATOR_CHAR_A, VirtualInfo->SnapShotId);

        if (!OPEN_FILE(vMapFileName.data(), O_RDONLY, FILE_SHARE_READ | FILE_SHARE_WRITE, &Handle))
        {
            Error->VsnapErrorCode = 0;
            Error->VsnapErrorStatus = VSNAP_MAP_INIT_FAILED;
            Success = false;
            break;
        }

        if(!ParseMap(handleVolume, Handle, VirtualInfo))
            Success = false;
        else
            DebugPrintf(SV_LOG_INFO,"Performing post apply track logs operations. Please wait....\n");
        CLOSE_FILE(Handle);

    } while (FALSE);


    if(handleVolume != ACE_INVALID_HANDLE)
    {
        CloseVolumeExclusive(handleVolume, sTgtGuid.c_str(), TgtVol, TgtVol /*TODO:MPOINT*/);
    }


    return Success;
}


bool WinVsnapMgr::NeedToRecreateVsnap(const std::string &vsnapdevice)
{
    bool bdeviceexists = IsFileORVolumeExisting(vsnapdevice);
    return !bdeviceexists;
}


bool WinVsnapMgr::StatAndMount(const VsnapPersistInfo& remountInfo)
{
    return true;
}


bool WinVsnapMgr::StatAndWait(const std::string &device)
{
    bool bretval = true;
    return bretval;    
}


bool WinVsnapMgr::PerformZpoolDestroyIfReq(const std::string &devicefile, bool bshoulddeletevsnaplogs)
{
    bool bretval = true;
    return bretval;    
}


bool WinVsnapMgr::RetryZpoolDestroyIfReq(const std::string &devicefile, const std::string &target, bool bshoulddeletevsnaplogs)
{
    bool bretval = true;
    return bretval;    
}


bool WinVsnapMgr::CloseLinvsnapSyncDev(const std::string &device, int &fd)
{
    bool bretval = true;
    return bretval;
}


bool WinVsnapMgr::OpenLinvsnapSyncDev(const std::string &device, int &fd)
{
    bool bretval = true;
    return bretval;
}

/*
* FUNCTION NAME :  WinVsnapMgr::fill_fsdata_from_retention
*
* DESCRIPTION : this function will overwrite the fsdata by the data from retention
*				by taking the info from the drtd
*     
*
* INPUT PARAMETERS : drtdPtr (this will contain the voloffset,fileoffset,filename)
*						
*
* OUTPUT PARAMETERS : PairInfo ( the data inside PairInfo->SrcInfo->FsData is going to be changed)
*
* NOTES :
* Algorithm: 1. get the retention file
*			 2. get the file offset
*            3. read the data of 512 bytes from the retention file starting from file offset
*			 4. overwrite the fsdata 
*            5. change the volume fstype to NTFS or FAT
*
* return value : true on success otherwise false
*
*/

bool WinVsnapMgr::fill_fsdata_from_retention(const std::string & fileName, SV_ULONGLONG fileoffset, VsnapPairInfo *PairInfo)
{
    bool rv = true;
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s \n",FUNCTION_NAME);
    VsnapSourceVolInfo		*SrcInfo = PairInfo->SrcInfo;
    do
    {
        std::string filetoread = dirname_r(SrcInfo->RetentionDbPath.c_str());
        filetoread += ACE_DIRECTORY_SEPARATOR_CHAR_A;
        filetoread += fileName;
        ACE_HANDLE Handle = ACE_INVALID_HANDLE;
        int BytesRead;
        memset(SrcInfo->FsData , 0, VSNAP_FS_DATA_SIZE);
        if(!OPEN_FILE(filetoread.c_str(), O_RDONLY, FILE_SHARE_READ | FILE_SHARE_WRITE, &Handle))
        {
            DebugPrintf(SV_LOG_ERROR, "\n@LINE %d in file %s Open failed for file %s.\n",
                LINE_NO,FILE_NAME,filetoread.c_str());
            rv = false;
            break;
        }

        if(!READ_FILE(Handle, SrcInfo->FsData, fileoffset, VSNAP_FS_DATA_SIZE, &BytesRead))
        {
            std::stringstream out;
            out << "LINE " << LINE_NO << " in file " << FILE_NAME
                << " Failed to read from file " << filetoread
                << " at offset " <<  fileoffset << "\n";
            DebugPrintf(SV_LOG_ERROR, "%s",out.str().c_str());
            CLOSE_FILE(Handle);
            rv = false;
            break;					
        }

		UnHideFileSystem(SrcInfo->FsData, VSNAP_FS_DATA_SIZE);

        CLOSE_FILE(Handle);
    }while(false);
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s \n",FUNCTION_NAME);
    return rv;
}

bool WinVsnapMgr::UnmountAllMountPointInVsnap(const std::string & deviceName)
{
	bool rv = true;

	std::string VsnapMtPt = deviceName;
	FormatVolumeNameForMountPoint(VsnapMtPt);
	std::wstring VsnapMtPtW = getLongPathName(VsnapMtPt.c_str());
    SV_ULONG   BufferLength = 2048;

	DebugPrintf(SV_LOG_DEBUG,"Searching Mount Points in Volume - %S to delete.\n",VsnapMtPtW.c_str());

    WCHAR * mountPointPath = (WCHAR *)malloc(BufferLength  * sizeof(WCHAR));
    memset(mountPointPath, 0, (BufferLength  * sizeof(WCHAR)));
	if(!mountPointPath)
	{
		DebugPrintf(SV_LOG_ERROR,"malloc failed in routine: %s\n", FUNCTION_NAME);
		return false;
	}

	HANDLE h = FindFirstVolumeMountPointW(VsnapMtPtW.c_str(), mountPointPath, BufferLength);

	if( INVALID_HANDLE_VALUE != h )
	{
		//If Mount Points are discovered, unmount them.
		//Even if one fails,proceed with remaining to clear as many as possible,but return as false.
		do
		{
			std::wstring mountPoint = VsnapMtPtW + mountPointPath;

			if(!DeleteVolumeMountPointW(mountPoint.c_str()))
			{
				DebugPrintf(SV_LOG_ERROR,
					"Failed to delete the Mount Point - %S with error [%lu]. Please delete this manually.\n",
					mountPoint.c_str(),GetLastError());
				rv = false;
			}
			else
			{
				DebugPrintf(SV_LOG_DEBUG,"Successfully deleted Mount Point - %S.\n",mountPoint.c_str());
			}  

            memset(mountPointPath, 0, (BufferLength  * sizeof(WCHAR)));
		}while(FindNextVolumeMountPointW(h, mountPointPath, BufferLength));

	}

	DWORD dwErrorValue = GetLastError();
	//if error is ERROR_NO_MORE_FILES, no more mount points else something went wrong.
	if(ERROR_NO_MORE_FILES != dwErrorValue)
	{
		DebugPrintf(SV_LOG_ERROR,
			"FindFirstVolumeMountPoint/FindNextVolumeMountPoint failed with Error : [%lu] on %S.\n",
			dwErrorValue, VsnapMtPtW.c_str());
		rv = false;
	}			

	if(INVALID_HANDLE_VALUE != h)
	{
		FindVolumeMountPointClose(h);
	}

	if(mountPointPath)
	{
		free(mountPointPath);
	}

	return rv;
}
