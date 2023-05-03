///
///  \file  PlatformAPIsMajor.h
///
///  \brief contains PlatformAPIs Defines for windows
///

#ifndef PLATFORMAPISMAJOR_H
#define PLATFORMAPISMAJOR_H


#ifdef PLATFORMAPIS_EXPORTS
#define PLATFORMAPIS_API __declspec(dllexport)
#else
#define PLATFORMAPIS_API __declspec(dllimport)
#endif

/*typedef enum FileMoveMethod
{
	BEGIN = 0,
	CURRENT = 1,
	END = 2
};*/

#include <initguid.h>
#include <windows.h>
#include <tchar.h>
#include <strsafe.h>
#include <VirtDisk.h>
#include <cguid.h>

typedef void* HANDLE;

#endif /* PLATFORMAPISMAJORMAJOR_H */
