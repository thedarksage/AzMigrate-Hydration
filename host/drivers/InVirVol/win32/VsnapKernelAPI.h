#ifndef _VSNAP_KERNEL_API_H_
#define _VSNAP_KERNEL_API_H_

int VsnapVolumeRead(void *Buffer, ULONGLONG Offset, ULONG BufferLength, ULONG *BytesRead, PVOID VolumeContext);
int VsnapVolumeWrite(void *Buffer, ULONGLONG Offset, ULONG BufferLength, ULONG *BytesRead, PVOID VolumeContext);
NTSTATUS VsnapMount(void *Buffer, int BufferLength, LONGLONG *VolumeSize, PVOID *VolumeContext);
bool VsnapUnmount(PVOID VolumeContext);
VOID VsnapVolumesUpdateMaps(PVOID InputBuffer);
ULONG VsnapGetVolumeInformationLength(PVOID VolumeContext);
void VsnapGetVolumeInformation(PVOID VolumeContext, PVOID Buffer, ULONG Size);
bool VsnapSetOrResetTracking(PVOID, bool, int*);
int VsnapInit();
int VsnapExit();
#endif
