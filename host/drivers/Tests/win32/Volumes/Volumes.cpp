// Volumes.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "windows.h"
#include "stdlib.h"
#include <string.h>
#include <devioctl.h>
#include <ntddscsi.h>
#include <mountmgr.h>


int main(int argc, char* argv[])
{
HANDLE volumeEnumHandle;
HANDLE mountEnumHandle;
HANDLE cdHandle;
TCHAR volumeName[256] = {0};
TCHAR mountName[256] = {0};
TCHAR physicalDeviceName[MAX_PATH] = {0};
TCHAR fileSystemName[256] = {0};
DWORD volumeLength;
char driveLetter;
SCSI_ADDRESS scsiAddress;
ULONG returned;
ULONG status;
IO_SCSI_CAPABILITIES scsiCapabilities;
PMOUNTDEV_NAME deviceName;
TCHAR dummyBuffer[102] = {0};

    deviceName = (PMOUNTDEV_NAME)malloc(4096); // bigger than any volume device name

	SetErrorMode(SEM_FAILCRITICALERRORS);

	volumeLength = 256;
	volumeEnumHandle = FindFirstVolume(volumeName, 256);
	if (volumeEnumHandle == INVALID_HANDLE_VALUE) {
		printf("error on FindFirstVolume\n");
		return 1;
	}
	printf("Volume Name: '%s'\n", volumeName);

	mountEnumHandle = FindFirstVolumeMountPoint(volumeName, mountName, 256);
	if (mountEnumHandle != INVALID_HANDLE_VALUE) {
		printf("  Mount Point: '%s'\n",mountName);
		while (FindNextVolumeMountPoint(mountEnumHandle, mountName, 256)) {
			printf("  Mount Point: '%s'\n",mountName);
		}
		FindVolumeMountPointClose(mountEnumHandle);
	}


	while (FindNextVolume(volumeEnumHandle, volumeName, volumeLength)) {
		printf("Volume Name: '%s'\n", volumeName);
		mountEnumHandle = FindFirstVolumeMountPoint(volumeName, mountName, 256);
		if (mountEnumHandle != INVALID_HANDLE_VALUE) {
			printf("  Mount Point: '%s'\n",mountName);
			while (FindNextVolumeMountPoint(mountEnumHandle, mountName, 256)) {
				printf("  Mount Point: '%s'\n",mountName);
			}
			FindVolumeMountPointClose(mountEnumHandle);
		}
	}

	FindVolumeClose(volumeEnumHandle);

	printf("\n\n\n");
	strcpy_s(mountName, _countof(mountName), "a:\\");
	for(driveLetter = 'A';driveLetter <= 'Z';driveLetter++) {
		mountName[0] = driveLetter;
		if (GetVolumeNameForVolumeMountPoint(mountName, volumeName, 256)) {
			// get the SCSI info
			// open the CD or port
		    strcpy_s(physicalDeviceName, _countof(physicalDeviceName), "\\\\.\\a:");
		    physicalDeviceName[4] = driveLetter;
	
			cdHandle = CreateFile(physicalDeviceName,
			   GENERIC_WRITE | GENERIC_READ,
			   FILE_SHARE_READ | FILE_SHARE_WRITE,
			   NULL,
			   OPEN_EXISTING,
			   0,
			   NULL);
			
			scsiAddress.Length = 0;
		    if (cdHandle != INVALID_HANDLE_VALUE) {
				status = DeviceIoControl(cdHandle,
						 IOCTL_SCSI_GET_ADDRESS,
						 NULL,
						 0,
	 					 &scsiAddress,
						 sizeof(SCSI_ADDRESS),
						 &returned,
						 FALSE);

				
				status = DeviceIoControl(cdHandle,
						 IOCTL_SCSI_GET_CAPABILITIES,
						 NULL,
						 0,
	 					 &scsiCapabilities,
						 sizeof(IO_SCSI_CAPABILITIES),
						 &returned,
						 FALSE);

                FillMemory(deviceName, 4096, 0);
   				status = DeviceIoControl(cdHandle,
						 IOCTL_MOUNTDEV_QUERY_DEVICE_NAME,
						 NULL,
						 0,
	 					 deviceName,
						 4096,
						 &returned,
						 FALSE);

				CloseHandle(cdHandle);
			}
          
            printf("Drive %s maps to\n\tvolume %s\n\tdevice %S\n",
                mountName, volumeName, deviceName->Name);
            
            strcat_s(physicalDeviceName, _countof(physicalDeviceName), "\\");
            status = GetVolumeInformation(physicalDeviceName, dummyBuffer, sizeof(dummyBuffer), 
                (PDWORD)&dummyBuffer, (PDWORD)&dummyBuffer,(PDWORD)&dummyBuffer, fileSystemName, sizeof(fileSystemName));

            if (status) {
                printf("\tFileSystemType %s\n",fileSystemName);
            } else {
                printf("\tFileSystemType unknown\n");
            }

			if (scsiAddress.Length == 4)
				printf("\tSCSI Port:0x%x Path:0x%x Target:0x%x Lun:0x%x\n",
					scsiAddress.PortNumber, scsiAddress.PathId, scsiAddress.TargetId, scsiAddress.Lun);
			else
				printf("\tSCSI target info unavailable\n");

			if (scsiCapabilities.Length == 0x18)
				printf("\tSCSI Adapter Max Xfer Len: 0x%x max Phy Pages:0x%x Align Mask:0x%x\n\n",
				scsiCapabilities.MaximumTransferLength, scsiCapabilities.MaximumPhysicalPages,
				scsiCapabilities.AlignmentMask);
			else
				printf("\tSCSI Adapter Capabilities unavailable\n\n");

		}
	}

	return 0;
}

