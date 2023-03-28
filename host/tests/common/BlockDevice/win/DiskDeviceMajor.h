///
///  \file  DiskDeviceMajor.h
///
///  \brief contains BlockDeviceDefines for windows
///

#ifndef DISKDEVICEMAJOR_H
#define DISKDEVICEMAJOR_H

#include <Windows.h>

#ifdef BLOCKDEVICE_EXPORTS
#define BLOCKDEVICE_API __declspec(dllexport)
#else
#define BLOCKDEVICE_API __declspec(dllimport)
#endif


#define MAX_PATH								260
#define GUID_SIZE_IN_CHARS                      36  // Does not include NULL.

#endif //DISKDEVICEMAJOR_H