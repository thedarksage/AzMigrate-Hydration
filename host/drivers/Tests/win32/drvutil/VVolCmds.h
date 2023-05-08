#ifndef _VVOLCMDS_H_
#define _VVOLCMDS_H_
#include <VVDevControl.h>

//Used TCHAR_ARRAY_SIZE to get exact size of array in case of TCHAR/WCHAR/CHAR array cases.
#if !defined(TCHAR_ARRAY_SIZE)
#define TCHAR_ARRAY_SIZE( a ) ( sizeof( a ) / sizeof( a[ 0 ] ) )
#endif

#define FLAG_MEM_FILE_HANDLE_CACHE               0x1
#define CMDSTR_MOUNT_VIRTUAL_VOLUME         _T("--MountVirtualVolume")
#define CMDSTR_UNMOUNT_VIRTUAL_VOLUME       _T("--UnmountVirtualVolume")

#define CMDSTR_SET_VV_CONTROL_FLAGS         _T("--SetVVControlFlags")
#define CMDOPTSTR_RESET_VV_CONTROL_FLAGS    _T("-ResetVVControlFlags")
#define CMDOPTSTR_OUT_OF_ORDER_WRITE        _T("-OutOfOrderWrite")

#define CMDSTR_STOP_VIRTUAL_VOLUME          _T("--StopVirtualVolume")
#define CMDSTR_START_VIRTUAL_VOLUME         _T("--StartVirtualVolume")
#define CMDSTR_CONFIGURE_VIRTUAL_VOLUME     _T("--ConfigureVVDriver")
#define CMDOPTSTR_MEM_FILE_HANDLE_CACHE_KB  _T("-MemFileHandleCacheKB")
#define MAX_STRING_SIZE    256
#define INVIRVOL_SERVICE    _T("invirvol")

enum etVolumeType
{
    ecSparseFileVolume,
    ecRetentionVolume
};

typedef struct _MOUNT_VIRTUAL_VOLUME_DATA {
    WCHAR   *pVolumeImageFileName;
    ULONG   ulLengthOfBuffer;
    TCHAR   MountPoint[MAX_STRING_SIZE];
    etVolumeType VolumeType;
} MOUNT_VIRTUAL_VOLUME_DATA, *PMOUNT_VIRTUAL_VOLUME_DATA;

typedef struct _UNMOUNT_VIRTUAL_VOLUME_DATA {
    TCHAR   MountPoint[MAX_STRING_SIZE];
} UNMOUNT_VIRTUAL_VOLUME_DATA, *PUNMOUNT_VIRTUAL_VOLUME_DATA;

typedef struct _MOUNT_POINT
{
    LIST_ENTRY ListEntry;
    TCHAR      MountPoint[MAX_STRING_SIZE];
}MOUNT_POINT, *PMOUNT_POINT;

typedef struct _STOP_VIRTUAL_VOLUME_DATA
{
    TCHAR DriverName[MAX_STRING_SIZE];
}STOP_VIRTUAL_VOLUME_DATA, *PSTOP_VIRTUAL_VOLUME_DATA;

typedef struct _START_VIRTAL_VOLUME_DATA
{
    TCHAR DriverName[MAX_STRING_SIZE];
}START_VIRTAL_VOLUME_DATA, *PSTART_VIRTAL_VOLUME_DATA;

typedef struct _CONFIGURE_VIRTUAL_VOLUME_DATA
{
    ULONG   ulFlags;
    ULONG   ulMemFileHandleCacheInKB;
}CONFIGURE_VIRTUAL_VOLUME_DATA, *PCONFIGURE_VIRTUAL_VOLUME_DATA;


HANDLE
OpenVirtualVolumeControlDevice( );

void
MountVirtualVolume(
    HANDLE hDevice,
    PMOUNT_VIRTUAL_VOLUME_DATA pMountVirtualVolumeData
    );

void
MountVirtualVolumeFromRetentionLog(
    HANDLE hDevice,
    PMOUNT_VIRTUAL_VOLUME_DATA pMountVirtualVolumeData
    );

void
UnmountVirtualVolume(
    HANDLE hDevice,
    PUNMOUNT_VIRTUAL_VOLUME_DATA pUnmountVirtualVolumeData
    );

ULONG
SetControlFlags(
    HANDLE hVFCtrlDevice,
    PCONTROL_FLAGS_INPUT pControlFlagsData
    );

BOOLEAN FileDiskUmount(PTCHAR VolumeName);
//BOOLEAN DeleteDevice(TCHAR *DeviceToBeDeleted);

VOID DeleteMountPointsForTargetVolume(PTCHAR HostVolumeName, PTCHAR TargetVolumeName);
BOOL VolumeSupportsReparsePoints(PTCHAR VolumeName);
VOID DeleteMountPoints(PTCHAR TargetVolumeName);
VOID UnmountFileSystem(const PTCHAR VolumeName);
VOID StartVirtualVolumeDriver(HANDLE hDevice, PSTART_VIRTAL_VOLUME_DATA pStartVirtualVolumeData);
VOID StopVirtualVolumeDriver(HANDLE hDevice, PSTOP_VIRTUAL_VOLUME_DATA pStopVirtualVolumeData);
VOID ConfigureVirtualVolumeDriver(HANDLE hDevice, PCONFIGURE_VIRTUAL_VOLUME_DATA pConfigureVirtualVolumeData);
VOID DeleteDrivesForTargetVolume(const PTCHAR TargetVolumeLink);
VOID UnloadExistingVolumes(HANDLE hDevice);
VOID SetMountPointForVolume(PTCHAR MountPoint, PTCHAR VolumeSymLink, PTCHAR VolumeName);
VOID MountVirtualVolumeFromSparseFile(HANDLE hDevice, PMOUNT_VIRTUAL_VOLUME_DATA pMountVirtualVolumeData);
VOID DeleteVolumeMountPointForVolume(const PTCHAR VolumeName);
VOID DeleteAutoAssignedDriveLetter(HANDLE hDevice, PTCHAR MountPoint);

#endif //_VVOLCMDS_H_
