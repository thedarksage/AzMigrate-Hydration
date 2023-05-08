#ifndef _INMAGE_MNT_MGR_H_
#define _INMAGE_MNT_MGR_H_

#include <ioevent.h>

//
// Macro that defines what a "GUID volume name" mount point is.  This macro can
// be used to scan the input from MOUNTDEV_LINK_CREATED 
// The parameter takes MOUNTDEV_NAME as parameter

#define GUID_START_INDEX_IN_MOUNTDEV_NAME           11
#define VOLUME_NAME_START_INDEX_IN_MOUNTDEV_NAME    3
#define GUID_SIZE_IN_BYTES                          0x48
#define VOLUME_LETTER_START_INDEX_IN_MOUNTDEV_NAME  12
#define VOLUME_LETTER_SIZE_IN_BYTES                 4

#define INMAGE_IS_MOUNTDEV_NAME_VOLUME_GUID(s) (                                            \
    ((s)->NameLength == 0x60 || ((s)->NameLength == 0x62 && (s)->Name[48] == '\\')) &&      \
    (s)->Name[0] == '\\' &&                                                                 \
    ((s)->Name[1] == '?' || (s)->Name[1] == '\\') &&                                        \
    (s)->Name[2] == '?' &&                                                                  \
    (s)->Name[3] == '\\' &&                                                                 \
    (s)->Name[4] == 'V' &&                                                                  \
    (s)->Name[5] == 'o' &&                                                                  \
    (s)->Name[6] == 'l' &&                                                                  \
    (s)->Name[7] == 'u' &&                                                                  \
    (s)->Name[8] == 'm' &&                                                                  \
    (s)->Name[9] == 'e' &&                                                                  \
    (s)->Name[10] == '{' &&                                                                 \
    (s)->Name[19] == '-' &&                                                                 \
    (s)->Name[24] == '-' &&                                                                 \
    (s)->Name[29] == '-' &&                                                                 \
    (s)->Name[34] == '-' &&                                                                 \
    (s)->Name[47] == '}'                                                                    \
    )

#define INMAGE_IS_MOUNTDEV_NAME_VOLUME_LETTER(s) (   \
    (s)->NameLength == 28 &&                \
    (s)->Name[0] == '\\' &&           \
    (s)->Name[1] == 'D' &&            \
    (s)->Name[2] == 'o' &&            \
    (s)->Name[3] == 's' &&            \
    (s)->Name[4] == 'D' &&            \
    (s)->Name[5] == 'e' &&            \
    (s)->Name[6] == 'v' &&            \
    (s)->Name[7] == 'i' &&            \
    (s)->Name[8] == 'c' &&            \
    (s)->Name[9] == 'e' &&            \
    (s)->Name[10] == 's' &&           \
    (s)->Name[11] == '\\' &&          \
    (s)->Name[12] >= 'A' &&           \
    (s)->Name[12] <= 'Z' &&           \
    (s)->Name[13] == ':')

PDEVICE_OBJECT
OpenMountMgrDevice();

// This constant is used under W2K as the maximum volume size value for a binary search of the last
// valid sector on the volume. The tradoff is the larger it is the more wasted read probes there are
// when the volume is smaller that the maximum. The number of read probes will be this size as a
// power of 2 (i.e. a trillion is 2^40), so 40 read probes will be required to converge on the exact
// last sector
#define MAXIMUM_SECTORS_ON_VOLUME (1024i64 * 1024i64 * 1024i64 * 1024i64)

/*-------------------------------------------------------------------------------------------
    Undocumented OS functions and values needed to implement this module
 *-------------------------------------------------------------------------------------------
 */

#define VOLUMES_READY_EVENT L"\\device\\VolumesSafeForWriteAccess"

#define RELATIVE_MILLISECOND_TIME (10i64 * -1000i64)

extern "C" NTSTATUS 
ObOpenObjectByName(IN POBJECT_ATTRIBUTES ObjectAttributes,
   IN POBJECT_TYPE ObjectType,
   IN OUT PVOID ParseContext OPTIONAL,
   IN KPROCESSOR_MODE AccessMode,
   IN ACCESS_MASK DesiredAccess,
   IN PACCESS_STATE PassedAccessState,
   OUT PHANDLE Handle);

VOID
ConvertGUIDtoLowerCase(PWCHAR   pGUID);

NTSTATUS
SetBitRepresentingDriveLetter(WCHAR DriveLetter, PRTL_BITMAP pBitmap);

NTSTATUS
ClearBitRepresentingDriveLetter(WCHAR DriveLetter, PRTL_BITMAP pBitmap);

NTSTATUS
ConvertDriveLetterBitmapToDriveLetterString(PRTL_BITMAP pBitmap, PWCHAR pwString, ULONG ulStringSizeInBytes);

VOID WaitForVolumesToBeWriteable();

extern "C" NTSTATUS
DriverNotificationCallback(
    PVOID NotificationStructure,
    PVOID pContext
    );

#endif // _INMAGE_MNT_MGR_H_