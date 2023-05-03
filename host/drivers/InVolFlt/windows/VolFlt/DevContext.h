/*++

Copyright (c) Microsoft Corporation

Module Name:

    DevContext.h

Abstract:

    This module contains the type and function definitions specifically for volume filter in Device Context

Environment:

    Kernel mode

Revision History:

--*/

#pragma once

#include "global.h"
#ifdef VOLUME_CLUSTER_SUPPORT
#include "mscs.h"
#endif
//Fix for Bug 27337
#include "fsvolumeoffsetinfo.h"
#include "FltContext.h"

#define FILTER_DEVICE_PROPOGATE_CHARACTERISTICS (FILE_REMOVABLE_MEDIA |  \
                                                 FILE_READ_ONLY_DEVICE | \
                                                 FILE_FLOPPY_DISKETTE    \
                                                 )

#define LOG_FILE_NAME_SUFFIX LOG_FILE_NAME_SUFFIX_VOLUME

#ifdef VOLUME_NO_REBOOT_SUPPORT
typedef enum _etContextType
{
    ecCompletionContext = 1,

    ecDevContext = 2
} etContextType, *petContextType;
#endif

//Reserved ranged for Volume Filt          0x00100000 to 0x08000000
#define DCF_READ_ONLY                      0x00000002 //Flag is overlapped with Indskflt

#define DCF_VOLUME_LETTER_OBTAINED         0x00100000 
#define DCF_PAGEFILE_WRITES_IGNORED        0x00200000
#define DCF_VOLUME_ON_BASIC_DISK           0x00400000

#ifdef VOLUME_CLUSTER_SUPPORT
#define DCF_CV_FS_UNMOUTNED                0x00800000
#define DCF_VOLUME_ON_CLUS_DISK            0x01000000
// This flag is set when only for cluster volumes, when the disk is acquired by SCSI ACQUIRE
#define DCF_CLUSTER_VOLUME_ONLINE          0x02000000

// This flag is set when offline status is returned for a write sent down to target disk.
// On Win2K we see writes succeed for a little time window between we see DISK RELEASE and
// Clus disk changes the status of volume to offline.
#define DCF_CLUSDSK_RETURNED_OFFLINE       0x04000000

#define DCF_SAVE_FILTERING_STATE_TO_BITMAP 0x08000000

#endif

#define MAX_UNIQUE_VOLUME_NAME_SIZE_IN_WCHARS    0x32
#define MIN_UNIQUE_VOLUME_NAME_SIZE_IN_WCHARS    0x30

#ifdef VOLUME_CLUSTER_SUPPORT
// This macro tells if a volume is on a cluster disk.
#define IS_CLUSTER_VOLUME(pVC)      (pVC->ulFlags & DCF_VOLUME_ON_CLUS_DISK)
// This macro tells if a volume is on a non cluster disk.
#define IS_NOT_CLUSTER_VOLUME(pVC)  (!IS_CLUSTER_VOLUME(pVC))
// This macro tells if volume is online.
// If volume is not on a cluster disk it is online or if it is on cluster disk it has to be online.
#define IS_VOLUME_ONLINE(pVC)       (IS_NOT_CLUSTER_VOLUME(pVC) || (pVC->ulFlags & DCF_CLUSTER_VOLUME_ONLINE))

// This macro tells if volume is offline
// If volume is on cluster disk and not online, the volume is considered as offline.
#define IS_VOLUME_OFFLINE(pVC)      (IS_CLUSTER_VOLUME(pVC) && (!(pVC->ulFlags & DCF_CLUSTER_VOLUME_ONLINE)))
#endif

typedef struct _DEVICE_ID
{
    struct _DEVICE_ID *NextGUID;
    WCHAR           GUID[GUID_SIZE_IN_CHARS + 1];   // 36 + 1
}DEVICE_ID, *PDEVICE_ID;

//Base structure FLT_CONTEXT is inherited
typedef struct _DEV_CONTEXT:FLT_CONTEXT
{
#ifdef VOLUME_NO_REBOOT_SUPPORT
    bool            IsDevUsingAddDevice;
    etContextType   eContextType;
#endif
#if (NTDDI_VERSION >= NTDDI_VISTA)
    etPartStyle     PartitionStyle;
    union{
        ULONG       ulDiskSignature;
        GUID        DiskId;
    };
#else 
    ULONG           ulDiskSignature;
#endif
    ULONG           ulDiskNumber;
    LONG            lAdditionalGUIDs;
    PDEVICE_ID      GUIDList;
    RTL_BITMAP      DriveLetterBitMap;
    ULONG           BufferForDriveLetterBitMap;
    UNICODE_STRING  UniqueVolumeName;
    WCHAR           BufferForUniqueVolumeName[MAX_UNIQUE_VOLUME_NAME_SIZE_IN_WCHARS];
#ifdef VOLUME_CLUSTER_SUPPORT
    bool            IsVolumeCluster;
    // The following three fields are used to register PnP notification for File System unmount.
    PFILE_OBJECT    LogVolumeFileObject;
    PDEVICE_OBJECT  LogVolumeDeviceObject;
#endif
    PBASIC_VOLUME   pBasicVolume;
    ULONGLONG IoctlDiskCopyDataCount;
    ULONGLONG IoctlDiskCopyDataCountSuccess;
    ULONGLONG IoctlDiskCopyDataCountFailure;
    //Fix for Bug 27337
    FSINFOWRAPPER   	FsInfoWrapper;
} DEV_CONTEXT, *PDEV_CONTEXT;

#ifdef VOLUME_NO_REBOOT_SUPPORT
typedef struct _COMPLETION_CONTEXT
{
    LIST_ENTRY      ListEntry;
    etContextType   eContextType;
    struct _DEV_CONTEXT*        DevContext;
    LONGLONG                       llOffset;
    ULONG                          ulLength;    
    LONGLONG                    llLength;
    PIO_COMPLETION_ROUTINE CompletionRoutine;
    PVOID Context;
    UCHAR Control;    
}COMPLETION_CONTEXT, *PCOMPLETION_CONTEXT;
#endif

NTSTATUS
InMageFltSaveAllChanges(PDEV_CONTEXT   pDevContext, BOOLEAN bWaitRequied, PLARGE_INTEGER pliTimeOut, bool bMarkVolumeOffline = false);


NTSTATUS
FillBitmapFilenameInDevContext(PDEV_CONTEXT pDevContext);

_DEV_CONTEXT *
GetDevContextWithGUID(PWCHAR    pGUID);

PDEV_CONTEXT
GetDevContextForThisIOCTL(
    PDEVICE_OBJECT  DeviceObject, 
    PIRP            Irp
    );

NTSTATUS
GetDevSize(
	IN	PDEVICE_OBJECT	TargetDeviceObject,
	OUT LONGLONG		*pllDevSize,
	OUT NTSTATUS		*pInMageStatus
);

