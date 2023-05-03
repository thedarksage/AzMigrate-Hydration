#ifndef _VIRTUAL_VOLUME_IMAGE_H_
#define _VIRTUAL_VOLUME_IMAGE_H_
#include "VVCmdQueue.h"

struct VVolumeInfoTag 
{
    LONG                        IOsInFlight;
    LONG                        IOPending;
    bool                        IOWriteInProgress;
    ULONG                       ulFlags;
    struct _DEVICE_EXTENSION   *DevExtension;
    FAST_MUTEX                  hMutex;

};
typedef struct VVolumeInfoTag  VVolumeInfo;    

VOID
VVolumeFromVolumeImageCmdRoutine(struct _DEVICE_EXTENSION *DevExtension, PCOMMAND_STRUCT pCommand);

// This is the dispatch routine to read for volumes exported from volume image.
NTSTATUS
VVolumeImageRead(PDEVICE_OBJECT DeviceObject, PIRP Irp);

NTSTATUS
VVolumeImageWrite(PDEVICE_OBJECT DeviceObject, PIRP Irp);

NTSTATUS
VVHandleDeviceControlForVolumeImageDevice(
    PDEVICE_OBJECT  DeviceObject,
    PIRP            Irp
    );

VOID
VVolumeImageProcessReadRequest(struct _DEVICE_EXTENSION *DevExtension, PCOMMAND_STRUCT pCommand);

VOID
VVolumeImageProcessWriteRequest(struct _DEVICE_EXTENSION *DevExtension, PCOMMAND_STRUCT pCommand);

#endif // _VIRTUAL_VOLUME_IMAGE_H_