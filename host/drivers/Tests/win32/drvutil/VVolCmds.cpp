#define INITGUID
// This file consists of commands for virtual volume driver.
#include <windows.h>
#include <stdio.h>
#include <tchar.h>
#include <malloc.h>
#include <map>
#include <setupapi.h>
#include "VVDevControl.h"
#include "DrvUtil.h"
#include <conio.h>
#include <assert.h>
#include "drvstatus.h"
#include "installinvirvol.h"
#include "ListEntry.h"
#include <dbt.h>


HANDLE
OpenVirtualVolumeControlDevice( )
{
    HANDLE  hDevice;

    hDevice = CreateFile (
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

VOID
MountVirtualVolume(
    HANDLE hDevice,
    PMOUNT_VIRTUAL_VOLUME_DATA pMountVirtualVolumeData
    )
{
    switch(pMountVirtualVolumeData->VolumeType)
    {
    case ecSparseFileVolume:
        MountVirtualVolumeFromSparseFile(hDevice, pMountVirtualVolumeData);
        break;
    case ecRetentionVolume:
        MountVirtualVolumeFromRetentionLog(hDevice, pMountVirtualVolumeData);
        break;
    default:
        _tprintf(_T("Unsupported Mount option\n"));
    }
}

VOID
MountVirtualVolumeFromSparseFile(
    HANDLE hDevice,
    PMOUNT_VIRTUAL_VOLUME_DATA pMountVirtualVolumeData
    )
{
    BOOLEAN     bResult;
    PWCHAR      MountedDeviceNameLink = NULL;
    TCHAR       DeviceName[0x03] = {0};
    DWORD       dwReturn;
    ULONG       ulLength = 0;
    TCHAR       TargetPath[MAX_STRING_SIZE];
    TCHAR       MountPointF[256];
    
	_tcscpy_s(MountPointF, TCHAR_ARRAY_SIZE(MountPointF), pMountVirtualVolumeData->MountPoint);
	_tcscat_s(MountPointF, TCHAR_ARRAY_SIZE(MountPointF), _T("\\"));

    if(QueryDosDevice(MountPointF, TargetPath, sizeof(TargetPath)))
    {
        _tprintf(_T("Mount Point Already Exists\n"));
        return;
    }

    MountedDeviceNameLink = (PTCHAR) malloc(VV_MAX_CB_DEVICE_NAME);
    if (NULL == MountedDeviceNameLink) {
        _tprintf(_T("MountVirtualVolume: malloc failed\n"));
        return;
    }

    ULONG FileNameLengthWithNullChar = (ULONG) wcslen(pMountVirtualVolumeData->pVolumeImageFileName) + 1;
    ULONG MountPointLengthWithNullChar = (ULONG) wcslen(pMountVirtualVolumeData->MountPoint) + 1;
    ulLength = (ULONG) (FileNameLengthWithNullChar + MountPointLengthWithNullChar) * sizeof(WCHAR) + 2 * sizeof(ULONG);
    ulLength += sizeof(ULONG);
    PCHAR Buffer = (PCHAR) calloc(ulLength, 1);
    ULONG FieldOffset = 0;
    FieldOffset += sizeof(ULONG);
    *(PULONG)(Buffer + FieldOffset) = MountPointLengthWithNullChar  * sizeof(WCHAR);
    FieldOffset += sizeof(ULONG);
	inm_memcpy_s(Buffer + FieldOffset, ulLength - FieldOffset, pMountVirtualVolumeData->MountPoint, MountPointLengthWithNullChar  * sizeof(WCHAR));
    FieldOffset += MountPointLengthWithNullChar  * sizeof(WCHAR);
    *(PULONG)(Buffer + FieldOffset) = FileNameLengthWithNullChar  * sizeof(WCHAR);
    FieldOffset += sizeof(ULONG);
	inm_memcpy_s(Buffer + FieldOffset, ulLength - FieldOffset, pMountVirtualVolumeData->pVolumeImageFileName,
			FileNameLengthWithNullChar * sizeof(WCHAR));
    FieldOffset += FileNameLengthWithNullChar  * sizeof(WCHAR);

    bResult = DeviceIoControl(
                     hDevice,
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
        _tprintf(_T("Returned Success from add virtual volume DeviceIoControl for file %s:\n"), pMountVirtualVolumeData->pVolumeImageFileName);
    }
    else {
        _tprintf(_T("Returned Failed from add virtual volume DeviceIoControl for file %s: %d\n"), 
                 pMountVirtualVolumeData->pVolumeImageFileName, GetLastError());
        return;
    }

    TCHAR VolumeSymLink[MAX_STRING_SIZE];
    TCHAR VolumeName[MAX_STRING_SIZE];
    
    wcstotcs(VolumeName, MAX_STRING_SIZE, MountedDeviceNameLink);
    size_t NameLength = wcslen(MountedDeviceNameLink) + 1;
    wcstotcs(VolumeSymLink, MAX_STRING_SIZE, MountedDeviceNameLink + NameLength);

    VolumeSymLink[1] = _T('\\');
	_tcscat_s(VolumeSymLink, TCHAR_ARRAY_SIZE(VolumeSymLink), _T("\\"));

    DeleteAutoAssignedDriveLetter(hDevice, pMountVirtualVolumeData->MountPoint);

    if(!SetVolumeMountPoint(MountPointF, VolumeSymLink))
    {
        _tprintf(_T("SetVolumeMountPoint Failed: LastError:%x\n"), GetLastError());
        return;
    }

    DEV_BROADCAST_VOLUME DevBroadcastVolume;
    DevBroadcastVolume.dbcv_devicetype = DBT_DEVTYP_VOLUME;
    DevBroadcastVolume.dbcv_flags = 0;
    DevBroadcastVolume.dbcv_reserved = 0;
    DevBroadcastVolume.dbcv_size = sizeof(DEV_BROADCAST_VOLUME);
    DevBroadcastVolume.dbcv_unitmask = GetLogicalDrives() | (1 << (tolower(pMountVirtualVolumeData->MountPoint[0]) - _T('a'))) ;

    SendMessage(HWND_BROADCAST,
        WM_DEVICECHANGE,
        DBT_DEVICEARRIVAL,
        (LPARAM)&DevBroadcastVolume
    );
    
    _tprintf(_T("Mount Successful\n"));

    free(MountedDeviceNameLink);
    return;
}

VOID DeleteAutoAssignedDriveLetter(HANDLE hDevice, PTCHAR MountPoint)
{
    CHAR    OutputBuffer[MAX_STRING_SIZE];
    DWORD   BytesReturned = 0;
    BOOLEAN bResult;
    DEV_BROADCAST_VOLUME DevBroadcastVolume;

    bResult = DeviceIoControl(hDevice,
        IOCTL_INMAGE_GET_VOLUME_SYMLINK_FOR_RETENTION_POINT,
        MountPoint,
        (DWORD) (wcslen(MountPoint) + 1 ) * sizeof(TCHAR),
        OutputBuffer,
        (DWORD) sizeof(OutputBuffer),
        &BytesReturned,
        NULL);

    if(!bResult)
    {
        _tprintf(_T("IOCTL_INMAGE_GET_VOLUME_SYMLINK_FOR_RETENTION_POINT Failed: InputBuffer:%s Error:%x\n"), 
            MountPoint, GetLastError());
        return;
    }

    WCHAR *DosDeviceName = (WCHAR*)(OutputBuffer + (wcslen((PWCHAR)OutputBuffer) + 1) * sizeof(WCHAR));

    if(wcslen(DosDeviceName) > 0)
    {
        TCHAR DeviceToBeDeleted[4] = _T("X:\\");
        DeviceToBeDeleted[0] = DosDeviceName[12];

        if(!DeleteVolumeMountPoint(DeviceToBeDeleted))
        {
            _tprintf(_T("Device delete Failed: %s Error:%d\n"), DeviceToBeDeleted, GetLastError());
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

LARGE_INTEGER GetTotalBytes(TCHAR DriveLetter)
{
    TCHAR sVolume[] = _T("\\\\.\\X:");
    sVolume[4] = DriveLetter;
	ULARGE_INTEGER ulTotalNumberOfFreeBytes;
	GET_LENGTH_INFORMATION Length_Info;
	Length_Info.Length.HighPart	= 0;
	Length_Info.Length.LowPart	= 0;
	Length_Info.Length.QuadPart	= 0;

	ulTotalNumberOfFreeBytes.HighPart	= 0;
	ulTotalNumberOfFreeBytes.LowPart	= 0;
	ulTotalNumberOfFreeBytes.QuadPart	= 0;

	unsigned long int uliBytesReturned = 0;
	HANDLE hHandle = CreateFile( sVolume,GENERIC_READ | GENERIC_WRITE | GENERIC_EXECUTE,FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL  );
	if ( INVALID_HANDLE_VALUE == hHandle )
	{
        _tprintf(_T("Could not open handle\n"));
	}

    BOOL bResult = DeviceIoControl(
                        hHandle,              // handle to device
                        FSCTL_ALLOW_EXTENDED_DASD_IO,  // dwIoControlCode
                        NULL,                          // lpInBuffer
                        0,                             // nInBufferSize
                        NULL,                          // lpOutBuffer
                        0,                             // nOutBufferSize
                        &uliBytesReturned,     // number of bytes returned
                        NULL// OVERLAPPED structure
                        );

    if(!bResult)
        _tprintf(_T("failed\n"));

	else if ( !DeviceIoControl( hHandle,
								IOCTL_DISK_GET_LENGTH_INFO,
								NULL,
								0,
								&Length_Info, 
								sizeof(Length_Info),
								&uliBytesReturned,
								NULL) )
	{
		_tprintf(_T("IOCTL_DISK_GET_LENGTH_INFO failed\n"));
	}

	CloseHandle(hHandle);
	
	return Length_Info.Length;
}

VOID
MountVirtualVolumeFromRetentionLog(
    HANDLE hDevice,
    PMOUNT_VIRTUAL_VOLUME_DATA pMountVirtualVolumeData
    )
{
    BOOLEAN     bResult = true;
    PWCHAR      MountedDeviceNameLink = NULL;
    TCHAR       DeviceName[0x03] = {0};
    DWORD       dwReturn = 0;
    ULONG       ulLength = 0;
    HRESULT     hRes = NULL;
    ULONG       ByteOffset = 0;
    PCHAR       Buffer = NULL;

    MountedDeviceNameLink = (PWCHAR) malloc(VV_MAX_CB_DEVICE_NAME);
    if (NULL == MountedDeviceNameLink) {
        _tprintf(_T("MountVirtualVolumeFromRetentionLog: malloc failed\n"));
        return;
    }

    ByteOffset  = 0;
    ulLength    = (ULONG) sizeof(USHORT) + (_tcslen(pMountVirtualVolumeData->MountPoint) + 1) * sizeof(TCHAR) +
                        (wcslen(pMountVirtualVolumeData->pVolumeImageFileName) + 1 )* sizeof(WCHAR) + sizeof(LONGLONG);
    Buffer      = (PCHAR) calloc(ulLength, 1);
    *(PUSHORT)Buffer = (USHORT)(_tcslen(pMountVirtualVolumeData->MountPoint) + 1 ) * sizeof(TCHAR);
    ByteOffset += sizeof(USHORT);
	inm_memcpy_s(Buffer + ByteOffset, ulLength - ByteOffset, pMountVirtualVolumeData->MountPoint, (_tcslen(pMountVirtualVolumeData->MountPoint) + 1) * sizeof(TCHAR));
    ByteOffset += (ULONG)(wcslen(pMountVirtualVolumeData->MountPoint) + 1 ) * sizeof(TCHAR);
    *(PLONGLONG)(Buffer + ByteOffset)= GetTotalBytes(pMountVirtualVolumeData->pVolumeImageFileName[0]).QuadPart;
    ByteOffset += sizeof(LONGLONG);
	inm_memcpy_s(Buffer + ByteOffset, ulLength - ByteOffset,
		    pMountVirtualVolumeData->pVolumeImageFileName,(wcslen(pMountVirtualVolumeData->pVolumeImageFileName) + 1 ) * sizeof(WCHAR));

    bResult = DeviceIoControl(
                     hDevice,
                     (DWORD)IOCTL_INMAGE_ADD_VIRTUAL_VOLUME_FROM_RETENTION_LOG,
                     Buffer,
                     ulLength,
                     MountedDeviceNameLink,
                     VV_MAX_CB_DEVICE_NAME,
                     &dwReturn, 
                     NULL        // Overlapped
                     ); 

    free(Buffer);

    if (bResult)
        _tprintf(_T("Returned Success from add virtual volume DeviceIoControl for %ws\n"), pMountVirtualVolumeData->pVolumeImageFileName);
    else {
        _tprintf(_T("Returned Failed from add virtual volume DeviceIoControl for %ws %d\n"), 
                 pMountVirtualVolumeData->pVolumeImageFileName, GetLastError());
        return;
    }

    TCHAR VolumeSymLink[MAX_STRING_SIZE];
    TCHAR VolumeName[MAX_STRING_SIZE];
    
    wcstotcs(VolumeName, MAX_STRING_SIZE, MountedDeviceNameLink);
    size_t NameLength = wcslen(VolumeName) + 1;
    wcstotcs(VolumeSymLink, MAX_STRING_SIZE, MountedDeviceNameLink + NameLength);

    VolumeSymLink[1] = _T('\\');
	_tcscat_s(VolumeSymLink, TCHAR_ARRAY_SIZE(VolumeSymLink), _T("\\"));
    
    DeleteAutoAssignedDriveLetter(hDevice, pMountVirtualVolumeData->MountPoint);

    SetMountPointForVolume(pMountVirtualVolumeData->MountPoint, VolumeSymLink, VolumeName);

    free(MountedDeviceNameLink);
    return;
}

VOID SetMountPointForVolume(PTCHAR MountPoint, PTCHAR VolumeSymLink, PTCHAR VolumeName)
{
    DEV_BROADCAST_VOLUME DevBroadcastVolume;
    TCHAR MountPointF[MAX_STRING_SIZE];

	_tcscpy_s(MountPointF, TCHAR_ARRAY_SIZE(MountPointF), MountPoint);
	_tcscat_s(MountPointF, TCHAR_ARRAY_SIZE(MountPointF), _T("\\"));

    if(!SetVolumeMountPoint(MountPointF, VolumeSymLink))
    {
        _tprintf(_T("SetVolumeMountPoint Failed VolumeSymLink:%s MountPoint:%s Error:\n"), VolumeSymLink, MountPoint, GetLastError());
    }

    DevBroadcastVolume.dbcv_devicetype = DBT_DEVTYP_VOLUME;
    DevBroadcastVolume.dbcv_flags = 0;
    DevBroadcastVolume.dbcv_reserved = 0;
    DevBroadcastVolume.dbcv_size = sizeof(DEV_BROADCAST_VOLUME);
    DevBroadcastVolume.dbcv_unitmask = GetLogicalDrives() | (1 << (tolower(MountPoint[0]) - _T('a'))) ;

    SendMessage(HWND_BROADCAST,
        WM_DEVICECHANGE,
        DBT_DEVICEARRIVAL,
        (LPARAM)&DevBroadcastVolume
    );
}

ULONG
SetControlFlags(
    HANDLE                  hVFCtrlDevice,
    PCONTROL_FLAGS_INPUT    pControlFlagsInput
    )
{
    CONTROL_FLAGS_INPUT  ControlFlagsInput;
    CONTROL_FLAGS_OUTPUT ControlFlagsOutput;
	BOOL	bResult;
	DWORD	dwReturn;

    ControlFlagsInput.ulControlFlags = pControlFlagsInput->ulControlFlags;
    ControlFlagsInput.eOperation = pControlFlagsInput->eOperation;

    bResult = DeviceIoControl(
                    hVFCtrlDevice,
                    (DWORD)IOCTL_INMAGE_SET_CONTROL_FLAGS,
                    &ControlFlagsInput,
                    sizeof(CONTROL_FLAGS_INPUT),
                    &ControlFlagsOutput,
                    sizeof(CONTROL_FLAGS_OUTPUT),
                    &dwReturn,	
                    NULL        // Overlapped
                    );
    if (bResult) {
        _tprintf(_T("SetControlFlags: Set flags on Control Device: Succeeded\n"));
    } else {
        _tprintf(_T("SetControlFlags: Set flags on Control Device: Failed: %d\n"),  GetLastError());
		return ERROR_GEN_FAILURE;
    }

    if (sizeof(CONTROL_FLAGS_OUTPUT) != dwReturn) {
        _tprintf(_T("SetControlFlags: dwReturn(%#x) did not match CONTROL_FLAGS_OUTPUT size(%#x)\n"), dwReturn, sizeof(CONTROL_FLAGS_OUTPUT));
        return ERROR_GEN_FAILURE;
    }

	if (ControlFlagsOutput.ulControlFlags) {
		_tprintf(_T("Following flags are set for Control Device:\n"));
	} else {
        _tprintf(_T("No flags are set for this Device\n"));
		return ERROR_SUCCESS;
	}
    if (ControlFlagsOutput.ulControlFlags & VV_CONTROL_FLAG_OUT_OF_ORDER_WRITE)
        _tprintf(_T("\tVV_CONTROL_FLAG_OUT_OF_ORDER_WRITE\n"));

    return ERROR_SUCCESS;
}

VOID 
StartVirtualVolumeDriver(HANDLE hDevice, PSTART_VIRTAL_VOLUME_DATA pStartVirtualVolumeData)
{
    //The Driver is already started while opening the volume device.
}

VOID 
StopVirtualVolumeDriver(HANDLE hDevice, PSTOP_VIRTUAL_VOLUME_DATA pStopVirtualVolumeData)
{
    STOP_INVIRVOL_DATA StopData;
    DRSTATUS Status = DRSTATUS_SUCCESS;

    UnloadExistingVolumes(hDevice);

    StopData.DriverName = INVIRVOL_SERVICE;
    Status = StopInVirVolDriver(StopData);
    
    if(Status != DRSTATUS_SUCCESS)
        _tprintf(_T("Uninstall Failed\n"));
}

VOID 
ConfigureVirtualVolumeDriver(HANDLE hDevice, PCONFIGURE_VIRTUAL_VOLUME_DATA pConfigureVirtualVolumeData)
{
    VVOLUME_CD_INPUT  Input;
    Input.ulFlags                           = pConfigureVirtualVolumeData->ulFlags;
    Input.ulMemForFileHandleCacheInKB       = pConfigureVirtualVolumeData->ulMemFileHandleCacheInKB;
    DWORD   BytesReturned                   = 0;    
    BOOL    bResult                         = FALSE;

    bResult = DeviceIoControl(hDevice,
        IOCTL_INMAGE_VVOLUME_CONFIGURE_DRIVER,
        &Input,
        (DWORD) (sizeof(VVOLUME_CD_INPUT)),
        NULL,
        0,
        &BytesReturned,
        NULL);

    if(!bResult)
    {
        DWORD err = GetLastError();
        if(ERROR_INVALID_PARAMETER == err) {
            _tprintf(_T("Input Buffer provided is small\n"));
        } else if(ERROR_INVALID_FUNCTION == err) {
            _tprintf(_T("Vsnap already exist in the system\n"));
        }
        
        
    } else {
           _tprintf(_T("IOCTL_INMAGE_VVOLUME_CONFIGURE_DRIVER Succeed:  \n"));
     
    }
    return;
    
}

VOID UnloadExistingVolumes(HANDLE hDevice)
{
    ULONG RequiredSize = 0, VolumeCount = 0, NameOffsetInChar = 0;
    PWCHAR VolumeNameBuffer = NULL;
    DWORD BytesReturned = 0;

    DeviceIoControl(hDevice,
        IOCTL_INMAGE_GET_MOUNTED_VOLUME_LIST,
        NULL,
        0,
        &RequiredSize,
        sizeof(ULONG),
        &BytesReturned,
        NULL);

    if(0 == RequiredSize)
    {
        _tprintf(_T("No more volumes to unload\n"));
        return;
    }

    VolumeNameBuffer = (PWCHAR) malloc(RequiredSize);
    if(!DeviceIoControl(hDevice,
        IOCTL_INMAGE_GET_MOUNTED_VOLUME_LIST,
        NULL,
        0,
        VolumeNameBuffer,
        RequiredSize,
        &BytesReturned,
        NULL))
    {
        _tprintf(_T("IOCTL_INMAGE_GET_MOUNTED_VOLUME_LIST Failed Error%x\n"), GetLastError());
        return;
    }   

    VolumeCount = *(PULONG)VolumeNameBuffer;
    NameOffsetInChar = sizeof(ULONG) / sizeof(WCHAR);   //VolumeNameBuffer = NumberOfVolumes + VolumeString1 + VolumeString2 + ...
    
    for(ULONG i = 0; i < VolumeCount; i++)
    {
        TCHAR VolumeLink[MAX_STRING_SIZE];
        wcstotcs(VolumeLink, MAX_STRING_SIZE, &(VolumeNameBuffer)[NameOffsetInChar]);
        UnmountFileSystem(VolumeLink);
        NameOffsetInChar += (ULONG)(wcslen(&(VolumeNameBuffer)[NameOffsetInChar]) + sizeof(WCHAR));
    }
}

VOID UnmountFileSystem(const PTCHAR VolumeName)
{   
    TCHAR FormattedVolumeName[MAX_STRING_SIZE];

	_tcscpy_s(FormattedVolumeName, TCHAR_ARRAY_SIZE(FormattedVolumeName), VolumeName);
    FormattedVolumeName[1] = _T('\\');

    DeleteVolumeMountPointForVolume(VolumeName);
    if(!FileDiskUmount(FormattedVolumeName))
    {
        _tprintf(_T("UnmountFileSystem: FileDiskUmount Error: %x\n"), GetLastError());
        return;
    }
}

VOID DeleteVolumeMountPointForVolume(const PTCHAR VolumeName)
{
    TCHAR FormattedVolumeName[MAX_STRING_SIZE];
    TCHAR FormattedVolumeNameWithBackSlash[MAX_STRING_SIZE];

	_tcscpy_s(FormattedVolumeName, TCHAR_ARRAY_SIZE(FormattedVolumeName), VolumeName);
    FormattedVolumeName[1] = _T('\\');

	_tcscpy_s(FormattedVolumeNameWithBackSlash, TCHAR_ARRAY_SIZE(FormattedVolumeNameWithBackSlash), FormattedVolumeName);
	_tcscat_s(FormattedVolumeNameWithBackSlash, TCHAR_ARRAY_SIZE(FormattedVolumeNameWithBackSlash), _T("\\"));

    DeleteMountPoints(FormattedVolumeNameWithBackSlash);
    DeleteDrivesForTargetVolume(FormattedVolumeNameWithBackSlash);
}

VOID
UnmountVirtualVolume(
    HANDLE hDevice,
    PUNMOUNT_VIRTUAL_VOLUME_DATA pUnmountVirtualVolumeData
    )
{
    DWORD   BytesReturned                   = 0;
    ULONG   CanUnloadDriver                 = 0;
    BOOL    bResult                         = FALSE;
    PCHAR   OutputBuffer[MAX_STRING_SIZE];
    TCHAR   VolumeLink[MAX_STRING_SIZE];

    bResult = DeviceIoControl(hDevice,
        IOCTL_INMAGE_GET_VOLUME_SYMLINK_FOR_RETENTION_POINT,
        pUnmountVirtualVolumeData->MountPoint,
        (DWORD) (wcslen(pUnmountVirtualVolumeData->MountPoint) + 1) * sizeof(TCHAR),
        OutputBuffer,
        (DWORD) sizeof(OutputBuffer),
        &BytesReturned,
        NULL);

    if(!bResult)
    {
        _tprintf(_T("IOCTL_INMAGE_GET_VOLUME_NAME_FOR_RETENTION_POINT Failed: InputBuffer:%s Error:%x\n"), 
                VolumeLink, GetLastError());
        return;
    }
 
    wcstotcs(VolumeLink, MAX_STRING_SIZE, (PWCHAR)OutputBuffer);
    UnmountFileSystem(VolumeLink);

    bResult = DeviceIoControl(hDevice,
        IOCTL_INMAGE_REMOVE_VIRTUAL_VOLUME_FROM_RETENTION_LOG,
        pUnmountVirtualVolumeData->MountPoint,
        (DWORD) (wcslen(pUnmountVirtualVolumeData->MountPoint) + 1) * sizeof(TCHAR),
        NULL,
        0,
        &BytesReturned,
        NULL);

    if(!bResult)
    {
        _tprintf(_T("IOCTL_INMAGE_REMOVE_VIRTUAL_VOLUME_FROM_RETENTION_LOG Failed: MountPoint: %s Error:%x\n"), 
                pUnmountVirtualVolumeData->MountPoint, GetLastError());
        return;
    }

    if(_tcslen(pUnmountVirtualVolumeData->MountPoint) < 3)
    {
        DEV_BROADCAST_VOLUME DevBroadcastVolume;
        DevBroadcastVolume.dbcv_devicetype = DBT_DEVTYP_VOLUME;
        DevBroadcastVolume.dbcv_flags = 0;
        DevBroadcastVolume.dbcv_reserved = 0;
        DevBroadcastVolume.dbcv_size = sizeof(DEV_BROADCAST_VOLUME);
        DevBroadcastVolume.dbcv_unitmask = 1 << (tolower(pUnmountVirtualVolumeData->MountPoint[0]) - _T('a'));

        SendMessage(HWND_BROADCAST,
            WM_DEVICECHANGE,
            DBT_DEVICEREMOVECOMPLETE,
            (LPARAM)&DevBroadcastVolume
        );
    }
    
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
        _tprintf(_T("Driver Can Unload IOCTL Failed with Error:%x\n"), GetLastError());
        return;
    }

    if(TRUE == CanUnloadDriver)
    {
        STOP_INVIRVOL_DATA StopData;
        StopData.DriverName = INVIRVOL_SERVICE;
        DRSTATUS DrStatus = StopInVirVolDriver(StopData);
        
        if(DR_SUCCESS(DrStatus))
            _tprintf(_T("Driver unload Successful\n"));
    }
}
/*
BOOLEAN DeleteDevice(TCHAR *DeviceToBeDeleted)
{
    HDEVINFO hDevInfo;
    SP_DEVINFO_DATA DeviceInfoData;
    DWORD i = 0;

    // Create a HDEVINFO with all present devices.

    hDevInfo = SetupDiGetClassDevs(
                    NULL,
                    0,                                  // Enumerator
                    0,
                    DIGCF_PRESENT | DIGCF_ALLCLASSES 
                    );

    if (INVALID_HANDLE_VALUE == hDevInfo)
    {
        return FALSE;
    }

    // Enumerate through all devices in Set.
    _tprintf(_T("DeviceToBeDeleted:%s\n"), DeviceToBeDeleted);
    DeviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);
    
    for (i = 0; SetupDiEnumDeviceInfo(hDevInfo, i, &DeviceInfoData); i++)
    {
        LPTSTR Buffer       = NULL;
        DWORD Buffersize    = 0;
        
        DWORD DataT;

        //
        // Call function with null to begin with,
        // then use the returned buffer size
        // to Alloc the buffer. Keep calling until
        // success or an unknown failure.
        //
        
        while (!SetupDiGetDeviceRegistryProperty(
                    hDevInfo,
                    &DeviceInfoData,
                    SPDRP_DEVICEDESC,
                    &DataT,
                    (PBYTE)Buffer,
                    Buffersize,
                    &Buffersize
                    ))
        {
            if (ERROR_INSUFFICIENT_BUFFER == GetLastError())
            {
                // Change the buffer size.
                if (Buffer) LocalFree(Buffer);
                Buffer = (LPTSTR) LocalAlloc(LPTR, Buffersize);
            }
            else
            {
                // Insert error handling here.
                break;
            }
        }

        if(0 == _tcscmp(DeviceToBeDeleted, Buffer))
        {
            if(SetupDiRemoveDevice(hDevInfo, &DeviceInfoData))
                _tprintf(_T("Removed Sample Driver: %s\n"), DeviceToBeDeleted);
            else
                _tprintf(_T("Error removed Sample Driver: %s ErrorCode: %d\n"), DeviceToBeDeleted, GetLastError());
            
            if (Buffer) LocalFree(Buffer);

            break;
        }

        if (Buffer) LocalFree(Buffer);
    }

    if(NO_ERROR != GetLastError() && ERROR_NO_MORE_ITEMS  != GetLastError())
    {
        return FALSE;
    }

    //  Cleanup
    SetupDiDestroyDeviceInfoList(hDevInfo);

    return TRUE;
}
*/
BOOLEAN FileDiskUmount(PTCHAR VolumeName)
{
    HANDLE  Device;
    DWORD   BytesReturned;

    Device = CreateFile(
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

VOID DeleteMountPointsForTargetVolume(PTCHAR HostVolumeName, PTCHAR TargetVolumeName)
{
    DWORD       BufferLength                                = MAX_STRING_SIZE;
    BOOL        bResult                                     = TRUE;
    TCHAR       VolumeMountPoint[MAX_STRING_SIZE]           = {0};
    TCHAR       MountPointTargetVolume[MAX_STRING_SIZE]     = {0};
    TCHAR       FullVolumeMountPoint[MAX_STRING_SIZE]           = {0};
    LIST_ENTRY  MountPointHead;
    HANDLE      hMountHandle;
    
    InitializeListHead(&MountPointHead);

    hMountHandle = FindFirstVolumeMountPoint(HostVolumeName, VolumeMountPoint, BufferLength);
    if(INVALID_HANDLE_VALUE == hMountHandle)
    {
        return;
    }
    
	_tcscpy_s(FullVolumeMountPoint, TCHAR_ARRAY_SIZE(FullVolumeMountPoint), HostVolumeName);
	_tcscat_s(FullVolumeMountPoint, TCHAR_ARRAY_SIZE(FullVolumeMountPoint), VolumeMountPoint);
    
    do
    {
        bResult = GetVolumeNameForVolumeMountPoint(VolumeMountPoint, MountPointTargetVolume, MAX_STRING_SIZE);

        if(bResult && 0 == _tcscmp(MountPointTargetVolume, TargetVolumeName))
        {
            PMOUNT_POINT MountPoint = (PMOUNT_POINT)malloc(sizeof(MOUNT_POINT));
			_tcscpy_s(MountPoint->MountPoint, TCHAR_ARRAY_SIZE(MountPoint->MountPoint), VolumeMountPoint);
            InsertTailList(&MountPointHead, &MountPoint->ListEntry);
        }

        bResult = FindNextVolumeMountPoint(hMountHandle, VolumeMountPoint, BufferLength);
		_tcscpy_s(FullVolumeMountPoint, TCHAR_ARRAY_SIZE(FullVolumeMountPoint), HostVolumeName);
		_tcscat_s(FullVolumeMountPoint, TCHAR_ARRAY_SIZE(FullVolumeMountPoint),VolumeMountPoint);
    
    }while(bResult);

    FindVolumeMountPointClose(hMountHandle);

    while(!IsListEmpty(&MountPointHead))
    {
        PMOUNT_POINT MountPoint = (PMOUNT_POINT) RemoveHeadList(&MountPointHead);

        if(!DeleteVolumeMountPoint(MountPoint->MountPoint))
        {
            _tprintf(_T("DeleteVolumeMountPoint Failed for point:%s with Error:%#x"), MountPoint->MountPoint, GetLastError());
        }

        free(MountPoint);
    }
}

BOOL VolumeSupportsReparsePoints(PTCHAR VolumeName)
{
    DWORD FileSystemFlags                       = 0;
    DWORD SerialNumber                          = 0;
    BOOL  MountPointSupported                   = FALSE;
    TCHAR FileSystemNameBuffer[MAX_STRING_SIZE] = {0};

    if(GetVolumeInformation(VolumeName, NULL, 0, &SerialNumber, NULL, &FileSystemFlags, 
                    FileSystemNameBuffer, sizeof(FileSystemNameBuffer)))
    {
        if(FileSystemFlags & FILE_SUPPORTS_REPARSE_POINTS && SerialNumber > 0)
            MountPointSupported = TRUE;
    }

    return MountPointSupported;
}

VOID DeleteMountPoints(const PTCHAR TargetVolumeName)
{
    TCHAR   HostVolumeName[MAX_STRING_SIZE];
    BOOL    bResult         = TRUE;
    HANDLE  hHostVolume     = FindFirstVolume(HostVolumeName, MAX_STRING_SIZE);

    if(hHostVolume == INVALID_HANDLE_VALUE)
    {
        _tprintf(_T("Could not enumerate volumes Error:%#x\n"), GetLastError());
        return;
    }

    do
    {
        if(VolumeSupportsReparsePoints(HostVolumeName))
            DeleteMountPointsForTargetVolume(HostVolumeName, TargetVolumeName);
        
        bResult = FindNextVolume(hHostVolume, HostVolumeName, MAX_STRING_SIZE);
    }while(bResult);

    FindVolumeClose(hHostVolume);
}

VOID
DeleteDrivesForTargetVolume(const PTCHAR TargetVolumeLink)
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
            _tprintf(_T("DeleteDrivesForTargetVolume: DriveName:%s GetVolumeNameForVolumeMountPoint Failed Error:%#x"), 
                    DriveName, GetLastError());
            continue;
        }

        if(_tcscmp(UniqueVolumeName, TargetVolumeLink) != 0)
            continue;
        
        if(!DeleteVolumeMountPoint(DriveName))
        {
            _tprintf(_T("DeleteVolumeMountPoint Failed: Error:%d\n"), GetLastError());
        }

        _tprintf(_T("Drive %s deleted successfully\n"), DriveName);
    }
}
