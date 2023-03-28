#ifndef _VIRTUAL_VOLUME_EXPORT_H_
#define _VIRTUAL_VOLUME_EXPORT_H_

#include "VVCmdQueue.h"

// Commands processed by VolumeExportCmdRoutine
#define VE_CMD_EXPORT_VOLUME_IMAGE                  0x0001
#define VE_CMD_EXPORT_VOLUME_FROM_RETENTION_LOG     0x0002
#define VE_CMD_REMOVE_VOLUME                        0x0003

#define CONTROL_DEVICE_CMD_QUEUE_DESCRIPTION    L"Control device command queue"
#define VVOLUME_IMAGE_CMD_QUEUE_DESCRIPTION     L"Virtual volume from volume image command queue"
#define VVOLUME_RETENTION_LOG_CMD_QUEUE_DESCRIPTION     L"Virtual volume from retention log command queue"

NTSTATUS
VVHandleAddVolumeFromVolumeImageIOCTL(
    IN PDEVICE_OBJECT   DeviceObject,
    IN PIRP             Irp
    );

VOID
FreeAddVolumeImageCmd(PCOMMAND_STRUCT pCommand);

VOID
VolumeExportCmdRoutine(struct _DEVICE_EXTENSION *DeviceExtension, PCOMMAND_STRUCT pCommand);

NTSTATUS
ExportVolumeImage(struct _DEVICE_EXTENSION *CtrlDevExtension, PCOMMAND_STRUCT pCommand);

NTSTATUS
VVHandleAddVolumeFromRetentionLogIOCTL(PDEVICE_OBJECT DeviceObject, PIRP Irp);

NTSTATUS 
ExportVolumeFromRetentionLog(struct _DEVICE_EXTENSION *CtrlDevExtension, PCOMMAND_STRUCT pCommand);

NTSTATUS
RemoveVolumeForRetentionOrVolumeImage(struct _DEVICE_EXTENSION *CtrlDevExtension, PCOMMAND_STRUCT pCommand);

VOID
FreeAddVolumeFromRetentionLogCmd(PCOMMAND_STRUCT pCommand);

NTSTATUS
CleanupVolumeContext(struct _DEVICE_EXTENSION *DeviceExtension);

NTSTATUS
VVSendMountMgrVolumeArrivalNotification(struct _DEVICE_EXTENSION *DeviceExtension);

NTSTATUS
VVHandleAddVolumeFromMultiPartSparseFile(PDEVICE_OBJECT DeviceObject, PIRP Irp);

#endif // _VIRTUAL_VOLUME_EXPORT_H_
