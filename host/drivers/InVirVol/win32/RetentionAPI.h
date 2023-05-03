#ifndef _RETENTION_API_H_
#define _RETENTION_API_H_

int VsnapVolumeRead(void *Buffer, LONGLONG Offset, ULONG BufferLength, ULONG *BytesRead, PVOID VolumeContext);
bool VsnapMount(void *Buffer, int BufferLength, LONGLONG *VolumeSize, PVOID *VolumeContext);
bool VsnapUnmount(PVOID VolumeContext);
VOID VsnapVolumesUpdateMaps(PVOID InputBuffer);
ULONG VsnapGetVolumeInformationLength(PVOID VolumeContext);
void VsnapGetVolumeInformation(PVOID VolumeContext, PVOID Buffer, ULONG Size);
int VsnapInit();
int VsnapExit();
#endif
