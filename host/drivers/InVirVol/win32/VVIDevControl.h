#ifndef _VV_INTERNAL_DEVICE_CONTROL_H_
#define _VV_INTERNAL_DEVICE_CONTROL_H_

#include "VVDevControl.h"

#define DOS_DEVICES_PATH                L"\\DosDevices\\"
#define UNC_PATH                        L"\\??\\UNC"
//#define VV_VOLUME_DEVICE_SYMLINK_NAME_PREFIX    L"\\DosDevices\\Global\\VVolume"
#define VV_VOLUME_DEVICE_SYMLINK_NAME_PREFIX    L"VVolume"

#define VV_CMD_PROCESS_CONTROL_FLAGS    0x0001
#define VV_BUFFER_SIZE_2K               2048;

NTSTATUS
VVHandleDeviceControlForControlDevice(
    IN PDEVICE_OBJECT   DeviceObject,
    IN PIRP             Irp
    );

NTSTATUS
VVHandleSetControlFlagsIOCTL(
    IN PDEVICE_OBJECT   DeviceObject,
    IN PIRP             Irp
    );

VOID
VVProcessControlFlagsCommand(
    PDEVICE_EXTENSION DeviceExtension, 
    PCOMMAND_STRUCT pCommand
    );

ULONG
VVSetOutOfOrderWrite();

ULONG
VVResetOutOfOrderWrite();

NTSTATUS
VVDeleteVolumeMountPoint(PDEVICE_EXTENSION DeviceExtension);

NTSTATUS
VVGetVolumeNameLinkForRetentionPoint(PDEVICE_OBJECT ControlDeviceObject, PIRP Irp);

NTSTATUS
VVHandleRemoveVolumeFromRetentionLogIOCTL(IN PDEVICE_OBJECT ControlDeviceObject, IN PIRP Irp);

VOID
VVCleanupVolume(PDEVICE_EXTENSION VolumeDeviceExtension);

PDEVICE_EXTENSION
GetVolumeFromVolumeID(PVOID VolumeID, ULONG VolumeIDLength);

VOID 
CopyUnicodeString(PUNICODE_STRING Target, PUNICODE_STRING Source, POOL_TYPE PoolType);

NTSTATUS
VVCanUnloadDriver(PDEVICE_OBJECT DeviceObject, PIRP Irp);

NTSTATUS
VVGetMountedVolumeList(PDEVICE_OBJECT DeviceObject, PIRP Irp);

NTSTATUS
VVUpdateVsnapVolumes(PDEVICE_OBJECT DeviceObject, PIRP Irp);

NTSTATUS
VVGetVolumeInformation(PDEVICE_OBJECT ControlDeviceObject, PIRP Irp);

PDEVICE_EXTENSION
GetVolumeFromMountDevLink(PVOID MountDevLink, ULONG MountDevLinkLength);

NTSTATUS
VVSetOrResetTrackingForVsnap(PDEVICE_OBJECT DeviceObject, PIRP Irp);

NTSTATUS
VVSetVolumeFlags(PDEVICE_OBJECT DeviceObject, PIRP Irp);

NTSTATUS
VVConfigureDriver(PDEVICE_OBJECT DeviceObject, PIRP Irp);
#endif // _VV_INTERNAL_DEVICE_CONTROL_H_
