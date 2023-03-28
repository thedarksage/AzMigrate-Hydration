#ifndef _VOLUME_RETENTION_TEST_H_
#define _VOLUME_RETENTION_TEST_H_

VOID
VVolumeRetentionProcessReadRequestTest(PDEVICE_EXTENSION DevExtension, PCOMMAND_STRUCT pCommand);

VOID
VVolumeRetentionProcessReadRequestDispatch(PDEVICE_EXTENSION DevExtension, PCOMMAND_STRUCT pCommand);

VOID
VVolumeRetentionProcessWriteRequestDispatch(PDEVICE_EXTENSION DevExtension, PCOMMAND_STRUCT pCommand);

NTSTATUS 
MountVolume(PVOID InputBuffer, ULONG Length, HANDLE *hFile, LONGLONG *VolumeSize);

#endif