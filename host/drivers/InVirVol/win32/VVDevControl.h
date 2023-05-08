#ifndef _VV_DEVICE_CONTROL_H_
#define _VV_DEVICE_CONTROL_H_
#include <DrvDefines.h>

#define VV_DEVICE_DIRECTORY         L"\\Device"
#define VV_INMAGE_DEVICE_DIRECTORY  VV_DEVICE_DIRECTORY L"\\InMage"
// If Kernel is sending device controls. one has to open the following device
#define VV_CONTROL_DEVICE_NAME      VV_INMAGE_DEVICE_DIRECTORY L"\\VVControl"
#define VV_VOLUME_NAME_PREFIX       VV_INMAGE_DEVICE_DIRECTORY L"\\VVolume"
#define VV_CONTROL_DOS_DEVICE_NAME  _T("\\\\.\\InMageVVolume")

#define VV_MAX_CB_DEVICE_NAME       0x1000

// Defining device type same as volume filter. This will allow us to merge the drivers
// if required.
#define FILE_DEVICE_INMAGE_VV   0x000080001

// Virtual volume IOCTLS start from 0x900. InMage filter ioctls are from 0x800 to 0x8FF.
// If we have to merge the virtual volume driver and volume filter driver, this would allow
// us to do with out much pain in compatibility.

// This IOCTL requests driver to add a new virtual volume.
// Virtual volume image stored in a file is given as input.
// Input Buffer has NULL terminated unicode file name to use as virtual volume image.
// Driver writes the device name as output buffer.
#define IOCTL_INMAGE_ADD_VIRTUAL_VOLUME_FROM_FILE               CTL_CODE(FILE_DEVICE_INMAGE_VV, 0x900, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_INMAGE_SET_CONTROL_FLAGS                          CTL_CODE(FILE_DEVICE_INMAGE_VV, 0x901, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_INMAGE_ADD_VIRTUAL_VOLUME_FROM_RETENTION_LOG      CTL_CODE(FILE_DEVICE_INMAGE_VV, 0x902, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_INMAGE_REMOVE_VIRTUAL_VOLUME_FROM_RETENTION_LOG   CTL_CODE(FILE_DEVICE_INMAGE_VV, 0x903, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_INMAGE_GET_VOLUME_SYMLINK_FOR_RETENTION_POINT     CTL_CODE(FILE_DEVICE_INMAGE_VV, 0x904, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_INMAGE_CAN_UNLOAD_DRIVER                          CTL_CODE(FILE_DEVICE_INMAGE_VV, 0x905, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_INMAGE_GET_MOUNTED_VOLUME_LIST                    CTL_CODE(FILE_DEVICE_INMAGE_VV, 0x906, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_INMAGE_UPDATE_VSNAP_VOLUME		                CTL_CODE(FILE_DEVICE_INMAGE_VV, 0x907, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_INMAGE_GET_VOLUME_INFORMATION                     CTL_CODE(FILE_DEVICE_INMAGE_VV, 0x908, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_INMAGE_ENABLE_TRACKING_FOR_VSNAP					CTL_CODE(FILE_DEVICE_INMAGE_VV, 0x909, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_INMAGE_DISABLE_TRACKING_FOR_VSNAP					CTL_CODE(FILE_DEVICE_INMAGE_VV, 0x90A, METHOD_BUFFERED, FILE_ANY_ACCESS)
//#define IOCTL_INMAGE_UNLOCK_RETENTION_VOLUME                    CTL_CODE(FILE_DEVICE_INMAGE_VV, 0x908, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_INMAGE_SET_VVVOLUME_FLAG							CTL_CODE(FILE_DEVICE_INMAGE_VV, 0x90B, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_INMAGE_VVOLUME_CONFIGURE_DRIVER                     CTL_CODE(FILE_DEVICE_INMAGE_VV, 0x90C, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_INMAGE_ADD_VIRTUAL_VOLUME_FROM_MULTIPART_SPARSE_FILE   CTL_CODE(FILE_DEVICE_INMAGE_VV, 0x90D, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_IMPERSONATE_CIFS                                  CTL_CODE(FILE_DEVICE_INMAGE_VV, 0x90E, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define VVOLUME_FLAG_READ_ONLY  0x0008
#define FLAG_MEM_FOR_FILE_HANDLE_CACHE   0x1

typedef struct _ADD_VOLUME_IMAGE_INPUT {
    ULONG   ulFlags;
    CHAR   VolumeInfo[1];  // This name is null terminated.
    //WCHAR   wImageFileName[1];
} ADD_VOLUME_IMAGE_INPUT, *PADD_VOLUME_IMAGE_INPUT;

#define VV_CONTROL_FLAG_OUT_OF_ORDER_WRITE  0x00000001
#define CONTROL_FLAGS_VALID                 0x00000001

typedef struct _CONTROL_FLAGS_INPUT
{
    // if eOperation is BitOpSet the bits in ulVolumeFlags will be set
    // if eOperation is BitOpReset the bits in ulVolumeFlags will be unset
    etBitOperation  eOperation;
    ULONG   ulControlFlags;
} CONTROL_FLAGS_INPUT, *PCONTROL_FLAGS_INPUT;

typedef struct _CONTROL_FLAGS_OUTPUT
{
    ULONG   ulControlFlags;
} CONTROL_FLAGS_OUTPUT, *PCONTROL_FLAGS_OUTPUT;


typedef struct _VVVOLUME_FLAGS_INPUT
{
	WCHAR   VolumeGUID[GUID_SIZE_IN_CHARS + 2];     // 0x00
    // if eOperation is BitOpSet the bits in ulVolumeFlags will be set
    // if eOperation is BitOpReset the bits in ulVolumeFlags will be unset
    etBitOperation  eOperation;                     // 0x4C
    ULONG   ulControlFlags;                          // 0x50
    // 64BIT : Keep structure size as multiple of 8 bytes
    ULONG   ulReserved;                             // 0x54
                                                    // 0x58   
} VVVOLUME_FLAGS_INPUT, *PVVVOLUME_FLAGS_INPUT;

typedef struct _VVVOLUME_FLAGS_OUTPUT
{
	ULONG   ulControlFlags;      //  0x00
    // 64BIT : Keep structure size as multiple of 8 bytes
    ULONG   ulReserved;         // 0x04    
} VVVOLUME_FLAGS_OUTPUT, *PVVVOLUME_FLAGS_OUTPUT;


typedef struct _VVOLUME_CD_INPUT
{
    ULONG   ulFlags;
    ULONG   ulMemForFileHandleCacheInKB;

}VVOLUME_CD_INPUT,*PVVOLUME_CD_INPUT;

typedef struct _UPDATE_VSNAP_VOLUME_INPUT
{
    LONGLONG VolOffset;
    ULONG    Length;
	ULONG	 Cow;
	LONGLONG FileOffset;
	char DataFileName[50];
	char ParentVolumeName[2048];
}UPDATE_VSNAP_VOLUME_INPUT, *PUPDATE_VSNAP_VOLUME_INPUT;

#endif // _VV_DEVICE_CONTROL_H_