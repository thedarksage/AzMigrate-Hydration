// This is the main project file for VC++ application project 
// generated using an Application Wizard.

#include "stdafx.h"
#include "windows.h"
#include "stdio.h"

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
int i;


    if (argc != 2) {
        printf("Usage: ChangeReader <volumePathname i.e. '\\\\.\\E:'\n");
        exit(1);
    }

    volumeHandle = CreateFile(argv[1], GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                              NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

    while (true) {
        DeviceIoControl(volumeHandle,
                        IOCTL_SVSYS_GET_DIRTY_BLOCKS, 
                        (PVOID)&changes, 
                        sizeof(changes), 
                        (PVOID)&changes, 
                        sizeof(changes), 
                        &returnBytes, 
                        NULL);
        for (i = 0; i < changes.cChanges;i++) {
            printf("%8X,%16I64X\n", changes.Changes[i].Length, changes.Changes[i].ByteOffset);
        }
        Sleep(500);
    }

    CloseHandle(volumeHandle);

    return 0;
}