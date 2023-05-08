#ifndef _INMAGE_CDSKFL_COMMON_H_
#define _INMAGE_CDSKFL_COMMON_H_

#define INITGUID


extern "C"
{
#include "ntddk.h"

// Use this if we are using string safe library.
//#define NTSTRSAFE_LIB
//#include <ntstrsafe.h>
}

#include "InCDskFl.h"
#include "IoctlInfo.h"

#define MEM_TAG_CLUSDISK_NODE		'NCfC'
#define MEM_TAG_GENERIC_NONPAGED	'NGfC'
#define MEM_TAG_GENERIC_PAGED		'PGfC'

extern DRIVER_CONTEXT   DriverContext;

#endif // _INMAGE_CDSKFL_COMMON_H_