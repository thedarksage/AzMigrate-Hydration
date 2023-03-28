/// @file svtypes.h
/// Defines all the types used by SV software.
///
#ifndef _DRIVER_SVTYPES_H
#define _DRIVER_SVTYPES_H

#include <guiddef.h>

typedef unsigned char       SV_UCHAR;
typedef unsigned short      SV_USHORT;
typedef unsigned int        SV_UINT;
typedef int                 SV_INT;
typedef long                SV_LONG;
typedef unsigned long       SV_ULONG;
typedef __int64             SV_LONGLONG;
typedef unsigned __int64    SV_ULONGLONG;
typedef unsigned short      SV_WCHAR;


typedef SV_USHORT           SV_EVENT_TYPE;
typedef SV_LONGLONG         SV_OFFSET_TYPE;
typedef LARGE_INTEGER       SV_LARGE_INTEGER;
typedef ULARGE_INTEGER      SV_ULARGE_INTEGER;
typedef GUID                SV_GUID;

typedef struct _SV_TIME {
    SV_USHORT wYear;
    SV_USHORT wMonth;
    SV_USHORT wDay;
    SV_USHORT wHour;
    SV_USHORT wMinute;
    SV_USHORT wSecond;
    SV_USHORT wMilliseconds;
    SV_USHORT wMicroseconds;
    SV_USHORT wHundrecNanoseconds;
} SV_TIME;

#endif // _DRIVER_SVTYPES_H
