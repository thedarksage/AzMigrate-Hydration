// This is the main project file for VC++ application project 
// generated using an Application Wizard.

#include "stdafx.h"
#include "windows.h"
#include "Winioctl.h"
#include "stdio.h"
#include <string.h>
#include <stdlib.h>

#define IOCTL_DISK_GET_LENGTH_INFO          CTL_CODE(IOCTL_DISK_BASE, 0x0017, METHOD_BUFFERED, FILE_READ_ACCESS)


#using <mscorlib.dll>
#include <tchar.h>

#define NTSTATUS DWORD
#include "involflt.h"

using namespace System;

// This is the entry point for this application
int _tmain( int argc, char *argv[ ])
{
HANDLE volumeHandle;
DIRTYBLOCKS changes;
DWORD returnBytes;
__int64 diskLengthBuffer;
static char bigBuffer[1024*1024*16];
__int64 newPosition;
LARGE_INTEGER position;
char fsName[512];
char fsVolume[512];
DWORD fsFlags;
TCHAR dummyBuffer[256];
__int64 i;
int j;


    if (argc != 2) {
        printf("Usage: ChangeWriter <volumePathname i.e. '\\\\.\\E:'\n");
        exit(1);
    }

    // check it's a raw volume
    strcpy_s(fsVolume, _countof(fsVolume), argv[1]);
    strcat_s(fsVolume, _countof(fsVolume), "\\");
    if (GetVolumeInformation(fsVolume, dummyBuffer, sizeof(dummyBuffer), 
                (PDWORD)&dummyBuffer, (PDWORD)&dummyBuffer,(PDWORD)&dummyBuffer, fsName, sizeof(fsName))) {
        printf("This utility will destroy the %s file system, exiting\n",fsName);
        exit(1);
    } else {
        printf("Writing to RAW volume\n");
    }

    ZeroMemory(&bigBuffer, sizeof(bigBuffer));

    volumeHandle = CreateFile(argv[1], GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                              NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

    diskLengthBuffer = 0;
    
    DeviceIoControl(volumeHandle,
                    IOCTL_DISK_GET_LENGTH_INFO,
                    NULL,
                    0,
                    (LPVOID) &diskLengthBuffer,
                    sizeof(diskLengthBuffer),
                    &returnBytes,
                    NULL);

    printf("Volume is %I64X bytes long\n", diskLengthBuffer);

    printf("Sweep up through volume addresses\n");
    j = 0x200;
    for(i = 0x0; i < 0x1F4117;i += 0x1001) {
        position.QuadPart = (i * 0x1000);
        SetFilePointerEx(volumeHandle,position, (PLARGE_INTEGER)&newPosition, FILE_BEGIN);
        WriteFile(volumeHandle, bigBuffer, j, &returnBytes, NULL);
        printf("offset: %12I64X  Length: %8X\n", position.QuadPart, j);
        j += 0x200;
        j = j % 0x20000;
        if (j == 0)
            j = 0x200;
    }


    CloseHandle(volumeHandle);

    return 0;
}