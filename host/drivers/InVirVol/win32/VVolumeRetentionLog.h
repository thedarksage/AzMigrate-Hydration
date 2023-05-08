#ifndef _VIRTUAL_VOLUME_RETENTION_LOG_H_
#define _VIRTUAL_VOLUME_RETENTION_LOG_H_

VOID
VVolumeFromRetentionLogCmdRoutine(
    PDEVICE_EXTENSION DevExtension, 
    PCOMMAND_STRUCT pCommand
);

NTSTATUS
VVHandleDeviceControlForVolumeRetentionDevice(
    PDEVICE_OBJECT  DeviceObject,
    PIRP            Irp
);

VOID
VVolumeRetentionProcessReadRequest(PDEVICE_EXTENSION DevExtension, PCOMMAND_STRUCT pCommand);

VOID
VVolumeRetentionProcessWriteRequest(PDEVICE_EXTENSION DevExtension, PCOMMAND_STRUCT pCommand);

NTSTATUS
VVolumeRetentionLogRead(PDEVICE_OBJECT  DevObj, PIRP Irp);

NTSTATUS
VVolumeRetentionLogWrite(PDEVICE_OBJECT  DevObj, PIRP Irp);

#endif
