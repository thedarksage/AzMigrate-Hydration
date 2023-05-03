#ifndef _INMAGE_IOCTL_INFO_H__
#define _INMAGE_IOCTL_INFO_H__

// DISK IOCTLs
#include <ntdddisk.h>
#if (NTDDI_VERSION < NTDDI_VISTA)
// FT IOCTLs
#include <ntddft.h>
#endif
// mount dev IOCTLs
#include <mountdev.h>
// volume IOCTLs
#include <ntddvol.h>

VOID
DumpIoctlInformation(
    PDEVICE_OBJECT DeviceObject, 
    PIRP    Irp
    );

#define IOCTL_MOUNTDEV_LINK_CREATED_W2K     (0x004DC010)
#define IOCTL_MOUNTDEV_LINK_CREATED_WNET    (0x004D0010)

#define IOCTL_DEVICE_TYPE(IOCTL_CODE)   ((IOCTL_CODE >> 16) & 0x0000FFFF)
#define IOCTL_ACCESS(IOCTL_CODE)        ((IOCTL_CODE >> 14) & 0x00000003)
#define IOCTL_FUNCTION(IOCTL_CODE)      ((IOCTL_CODE >> 2) & 0x00000FFF)
#define IOCTL_METHOD(IOCTL_CODE)        (IOCTL_CODE & 0x00000003)

#endif //_INMAGE_IOCTL_INFO_H__