#include "stdafx.h"
#include <Mountmgr.h>
#include <strsafe.h>
#include <winerror.h>
#include <Wtypes.h>
#include "Finddiskid.h"

#define GLOBALROOT_SIZE             14      // \\?\GLOBALROOT
#define FINDDISK_ASSERT(_exp) ((!(_exp)) ? 	(__annotation(L"Debug", L"AssertFail", L#_exp), DbgRaiseAssertionFailure(), FALSE) : TRUE)
#define PmSafeHeapFree(PTR) if ((PTR) != NULL) { PmHeapFree(GetProcessHeap(), 0, PTR); (PTR) = NULL; }  

int _tmain(int argc, _TCHAR* argv[])
{
    ULONG NumberofDiskExtents = 0;
    ULONG *NumberOfDiskId = NULL;

   GetBootDiskNumber(&NumberofDiskExtents, &NumberOfDiskId);

    _tprintf(_T(" Number of Disk Extents : %d \n"), NumberofDiskExtents);
    for (unsigned int i = 0; i < NumberofDiskExtents; i++)
        _tprintf(_T(" Device Number : %d \n"), NumberOfDiskId[i]);

    return 0;

}

DWORD GetDiskLayout(
    HANDLE                          hDisk,
    PDRIVE_LAYOUT_INFORMATION_EX    *ppLayout
    )
{

    PDRIVE_LAYOUT_INFORMATION_EX  pLayout = NULL;
    DWORD                         dwRet = ERROR_SUCCESS;
    BOOL                          bRet = FALSE;
    DWORD                         dwBufferSize = 0L;
    DWORD                         dwBytesReturned = 0L;

    if ((hDisk == NULL) || (ppLayout == NULL))
        return dwRet;

    *ppLayout = NULL;

    dwBufferSize = FIELD_OFFSET(
        DRIVE_LAYOUT_INFORMATION_EX,
        PartitionEntry);
    while (TRUE) {

        dwBufferSize += 16 * sizeof(PARTITION_INFORMATION_EX);

        if (pLayout) {
            free(pLayout);
            pLayout = NULL;
        }

        pLayout = (PDRIVE_LAYOUT_INFORMATION_EX)
            malloc(dwBufferSize);

        if (pLayout == NULL)
        {
            dwRet = ERROR_NOT_ENOUGH_MEMORY;
            break;
        }

        ZeroMemory(pLayout, dwBufferSize);

        bRet = DeviceIoControl(
            hDisk,
            IOCTL_DISK_GET_DRIVE_LAYOUT_EX,
            NULL,
            0,
            pLayout,
            dwBufferSize,
            &dwBytesReturned,
            0);
        if (bRet)
        {
            // If succeeded exit loop.
            dwRet = ERROR_SUCCESS;
            break;
        }

        dwRet = GetLastError();

        if (dwRet != ERROR_INSUFFICIENT_BUFFER)
        {
            _tprintf(_T("ERROR_INSUFFICIENT_BUFFER \n"));
            break;
        }
    }

    if (dwRet == ERROR_SUCCESS)
    {
        if (NULL != pLayout)
            *ppLayout = pLayout;
    }
    else
    {
        if (pLayout) {
            free(pLayout);
            pLayout = NULL;
        }
    }

    return dwRet;
}


void
GetDiskId(LPCTSTR DiskName)
{
    // Open device and get GPT disk GUID as WMI call does not retrieve the same
    PDRIVE_LAYOUT_INFORMATION_EX pLayout = NULL;
    GUID guid;
    HANDLE handle = CreateFile(DiskName, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    if (handle == INVALID_HANDLE_VALUE) {
        printf("GetDiskId has failed with error : %d \n", GetLastError());
        return;
    }
    if (ERROR_SUCCESS == GetDiskLayout(handle, &pLayout)) {
        if (pLayout->PartitionStyle == PARTITION_STYLE_GPT) {
            memcpy_s(&guid, sizeof(GUID),&pLayout->Gpt.DiskId, sizeof(GUID));
            _tprintf(_T("\nGPT DEVICE ID :%08lX-%04hX-%04hX-%02hhX%02hhX-%02hhX%02hhX%02hhX%02hhX%02hhX%02hhX\n"),
                guid.Data1, guid.Data2, guid.Data3,
                guid.Data4[0], guid.Data4[1], guid.Data4[2], guid.Data4[3],
                guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7]);
        }
        else if (pLayout->PartitionStyle == PARTITION_STYLE_MBR) {
            _tprintf(_T("\n MBR DEVICE ID :%lu\n"), pLayout->Mbr.Signature);
        }
        if (pLayout) {
            free(pLayout);
            pLayout = NULL;
        }
    } else {
        _tprintf(_T("Failed to Get Disk Layout Information ; GetLastError() : %d\n"), GetLastError());
    }
}
DWORD GetDeviceName(
    __in                        HANDLE  hDevice,
    __in                        BOOL    bPolling,
    __in                        DWORD   cchDeviceName,
    __out_ecount(cchDeviceName) WCHAR   *pwszDeviceName
    )
    /*++    Description:        Get the device name of the given device.      
    E.g. \\?\GLOBALROOT\Device\HarddiskVolume###    
    Arguments:        hDevice - handle to the device.        
    bPolling - TRUE if we are polling for a hidden volume name to appear.        
    pwszDeviceName - a buffer to receive device name. The buffer size must be          
    MAX_PATH+GLOBALROOT_SIZE (including "\\?\GLOBALROOT").    
    Return Value:        Win32 errors    --*/  
{
    DWORD               dwRet = 0L;
    BOOL                bRet = 0;
    WCHAR               MountDevName[MAX_PATH + sizeof(MOUNTDEV_NAME)];
    // Based on GetVolumeNameForRoot (in volmount.c), MAX_PATH seems to          
    // be big enough as out buffer for IOCTL_MOUNTDEV_QUERY_DEVICE_NAME.          
    // But we assume the size of device name could be as big as MAX_PATH-1,          
    // so we allocate the buffer size to include MOUNTDEV_NAME size.      
    PMOUNTDEV_NAME      pMountDevName = NULL;  
    DWORD               dwBytesReturned = 0L;
    DWORD               dwDeviceNameLimit = 0;
    UNREFERENCED_PARAMETER(bPolling);
    //      // query the volume's device object name      //    
    bRet = DeviceIoControl(
        hDevice,            // handle to device                          
        IOCTL_MOUNTDEV_QUERY_DEVICE_NAME,  
        NULL,               // input data buffer                          
        0,                  // size of input data buffer                          
        MountDevName,       // output data buffer                          
        sizeof(MountDevName),   // size of output data buffer                          
        &dwBytesReturned,  
        NULL                // overlapped information                      
        );  

    if (bRet == FALSE)
    {
        dwRet = GetLastError();
        return dwRet;
    }

    pMountDevName = (PMOUNTDEV_NAME)MountDevName;
    if (pMountDevName->NameLength == 0)
    {
        FINDDISK_ASSERT(pMountDevName->NameLength != 0);
        return ERROR_INVALID_DATA;
    }

    //      // copy name      //    
    StringCchPrintf(pwszDeviceName,
        cchDeviceName,
        L"\\\\?\\GLOBALROOT");

    dwDeviceNameLimit = pMountDevName->NameLength / 2 + GLOBALROOT_SIZE;
    if (cchDeviceName <= dwDeviceNameLimit) {
        FINDDISK_ASSERT(cchDeviceName > dwDeviceNameLimit);
        return ERROR_BUFFER_OVERFLOW;
    }

    memcpy_s(pwszDeviceName + GLOBALROOT_SIZE,
             cchDeviceName,
             pMountDevName->Name,
             pMountDevName->NameLength);


    // appending terminating NULL      
    pwszDeviceName[pMountDevName->NameLength/2 + GLOBALROOT_SIZE] = L'\0';  

    return ERROR_SUCCESS;
}

DWORD GetVolumeDiskExtentInfo(
    IN  HANDLE                          hVolume,
    OUT VOLUME_DISK_EXTENTS             **ppDiskExtents
    )

    /*++    Description:        Get the disk extents for this volume.
    Arguments:        hVolume - Handle to volume.
    ppDiskExtents - Returns a pointer to the disk extents' buffer.
    Return Value:        WIN32 error    --*/
{
    VOLUME_DISK_EXTENTS     *pExtents = NULL;
    DWORD                   dwRet = 0L;
    BOOL                    bRet = 0;
    DWORD                   dwSize = 0L;
    DWORD                   dwBytes = 0L;

    if (NULL == hVolume
        || INVALID_HANDLE_VALUE == hVolume
        || NULL == ppDiskExtents) {

        dwRet = ERROR_INVALID_PARAMETER;
        goto Cleanup;
    }
    *ppDiskExtents = NULL;

    //      
    // Get the volume extent info.      
    //    
    for (;;) {

        dwRet = ERROR_SUCCESS;

        dwSize += (sizeof(VOLUME_DISK_EXTENTS)
            +(10 * sizeof(DISK_EXTENT))
            );

        PmSafeHeapFree(pExtents);

        pExtents = (VOLUME_DISK_EXTENTS *)PmHeapAlloc(
            GetProcessHeap(),
            HEAP_ZERO_MEMORY,
            dwSize
            );

        if (NULL == pExtents) {
            dwRet = ERROR_NOT_ENOUGH_MEMORY;
            goto Cleanup;
        }

        bRet = ::DeviceIoControl(hVolume,
            IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS,
            NULL,
            0,
            pExtents,
            dwSize,
            &dwBytes,
            0
            );
        if (bRet) {
            break;  // succeeded          
        }  

            dwRet = GetLastError();

            if (ERROR_INSUFFICIENT_BUFFER != dwRet) {
                goto Cleanup;
            }
        }

        *ppDiskExtents = pExtents;

    Cleanup:

        return dwRet;

    }

HRESULT GetBootVolumeHandle(
    __out HANDLE *phBootVolume
    )
/*++    
    Description:        Returns a handle to the current boot volume.
    Arguments:          phBootVolume - Returns a handle to the current
    boot volume.    
    Return value:        S_OK
    E_INVALIDARG      
    Win32 errors      E_UNEXPECTED    
--*/
{

    HRESULT     hr = S_OK;
    HANDLE      hVolumeFS = NULL;
    HANDLE      hDevice = NULL;
    DWORD       dwRet = 0L;
    WCHAR       *pwszBootName = NULL;
    UINT        nRet = 0;
    DWORD       dwSize = 0L;
    WCHAR       wszDL[8];
    WCHAR       wszDeviceName[MAX_PATH + GLOBALROOT_SIZE];


    //      
    // Check params      
    //    
    if (NULL == phBootVolume) {
        _tprintf(_T("GetBootVolumeHandle : Params Validation failed \n"));
        hr = E_INVALIDARG;
        goto Cleanup;
    }
	*phBootVolume = INVALID_HANDLE_VALUE;

    //      
    // Get the path to the system directory, for example,      
    // "C:\windows\system".      
    //    
    while (nRet >= dwSize) {

        dwSize += MAX_PATH;

        PmSafeHeapFree(pwszBootName);

        pwszBootName = (WCHAR *) ::PmHeapAlloc(
            GetProcessHeap(),
            HEAP_ZERO_MEMORY,
            dwSize * sizeof(WCHAR)
            );

        if (NULL == pwszBootName) {
            _tprintf(_T("GetBootVolumeHandle : Failed to Allocate Memory for Boot Name\n"));
            hr = E_OUTOFMEMORY;
            FINDDISK_ASSERT(NULL != pwszBootName);
            goto Cleanup;
        }

        nRet = GetSystemDirectory(pwszBootName, dwSize);

        if (0 == nRet) {
            hr = HRESULT_FROM_WIN32(::GetLastError());
            goto Cleanup;
        }
    }

    if (NULL == pwszBootName) {   // prevent prefast error          
        hr = E_UNEXPECTED;  
        goto Cleanup;
    }

    //      
    // Open a handle to the volume's FS using the drive      
    // letter pathname, '\\?\x:' for example.      
    //    
    memset(wszDL, 0, 8 * sizeof(WCHAR));
    StringCchPrintf(wszDL,
        7,
        L"\\\\?\\%c:",
        pwszBootName[0]
        );
    wszDL[6] = L'\0';


    hVolumeFS = CreateFile(wszDL, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

    if (hVolumeFS == INVALID_HANDLE_VALUE) {

        hr = HRESULT_FROM_WIN32(GetLastError());
        _tprintf(_T(" GetBootVolumeHandle : Opening Boot Volume failed for %ws with error : %d \n"), wszDL, GetLastError());
        goto Cleanup;
    }

    dwRet = GetDeviceName(
            hVolumeFS,
            FALSE,
            MAX_PATH + GLOBALROOT_SIZE,
            wszDeviceName
           );

    if (ERROR_SUCCESS != dwRet) {
        _tprintf(_T("GetBootVolumeHandle : GetDeviceName has failed %d \n"), dwRet);
        hr = HRESULT_FROM_WIN32(dwRet);
        goto Cleanup;
    }

    _tprintf(_T("Boot Device Name : %ws \n"), wszDeviceName);

    hDevice = CreateFile(wszDeviceName, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

    if (hDevice == INVALID_HANDLE_VALUE) {
        hr = HRESULT_FROM_WIN32(GetLastError());
        _tprintf(_T(" CreateFile failed : %d \n"), GetLastError());
        goto Cleanup;
    }

    *phBootVolume = hDevice;

Cleanup:

    if ((NULL != hVolumeFS) && (hDevice != INVALID_HANDLE_VALUE))
        CloseHandle(hVolumeFS);
    PmSafeHeapFree(pwszBootName);


    return hr;
}

HRESULT
GetBootDiskNumber(
_Out_ ULONG *pcDisk,
_Outptr_result_buffer_(*pcDisk) ULONG **ppDisks
)
/*++    Description:        Returns the PNP disk number for the boot disk.    --*/  
{
    HRESULT             hr = S_OK;
    DWORD               dwRet = ERROR_SUCCESS;
    DWORD               i = 0;
    HANDLE              hVolume = INVALID_HANDLE_VALUE;
    VOLUME_DISK_EXTENTS *pDiskExtents = NULL;
    WCHAR               *pDiskName = NULL;

    FINDDISK_ASSERT(pcDisk != NULL && ppDisks != NULL);

    *pcDisk = 0;
    *ppDisks = NULL;

    hr = GetBootVolumeHandle(&hVolume);
    if (FAILED(hr)) {
        goto Cleanup;
    }

    dwRet = GetVolumeDiskExtentInfo(hVolume, &pDiskExtents);
    if (dwRet != ERROR_SUCCESS) {
        hr = HRESULT_FROM_WIN32(dwRet);
        goto Cleanup;
    }

    if (pDiskExtents->NumberOfDiskExtents <= 0L ||
        pDiskExtents->NumberOfDiskExtents > 2) {
        hr = E_UNEXPECTED;
        goto Cleanup;
    }

    *ppDisks = (ULONG*)PmHeapAlloc(GetProcessHeap(),
        HEAP_ZERO_MEMORY,
        pDiskExtents->NumberOfDiskExtents * sizeof(ULONG));

    if (*ppDisks == NULL) {
        hr = E_OUTOFMEMORY;
        goto Cleanup;
    }
    pDiskName = (WCHAR*)PmHeapAlloc(GetProcessHeap(),
        HEAP_ZERO_MEMORY,
        MAX_DISK_NAME * sizeof(WCHAR));
    *pcDisk = pDiskExtents->NumberOfDiskExtents;
    for (i = 0; i < pDiskExtents->NumberOfDiskExtents; i++) {
        (*ppDisks)[i] = pDiskExtents->Extents[i].DiskNumber;
        printf(" DiskNumber : %d \n", pDiskExtents->Extents[i].DiskNumber);
        // Generate WCHAR based string with name \\.\physicaldrive#
        StringCchPrintf(pDiskName, MAX_DISK_NAME * sizeof(WCHAR), L"\\\\.\\PHYSICALDRIVE%d", pDiskExtents->Extents[i].DiskNumber);
        GetDiskId(pDiskName);
    }
    hr = S_OK;

Cleanup:

    if ((NULL != hVolume) && (hVolume != INVALID_HANDLE_VALUE))
        CloseHandle(hVolume);
    PmSafeHeapFree(pDiskExtents);

    return hr;

}


BOOL
PmHeapFree(
__in HANDLE hHeap,
__in DWORD  dwFlags,
__in LPVOID lpMem
)
{
    if (hHeap == NULL)
    {
        return FALSE;
    }

    if (lpMem == NULL)
    {
        return TRUE;
    }

    return HeapFree(hHeap, dwFlags, lpMem);
}

LPVOID
PmHeapAlloc(
__in HANDLE hHeap,
__in DWORD  dwFlags,
__in SIZE_T dwBytes
)
{
    if (hHeap == NULL)
    {
        return NULL;
    }

#ifndef _DEBUG  
    LPVOID lptr = HeapAlloc(hHeap, dwFlags, dwBytes);
#else  
    LPVOID lptr = HeapAlloc(hHeap, dwFlags | HEAP_GENERATE_EXCEPTIONS, dwBytes);
#endif  

    return lptr;
}
