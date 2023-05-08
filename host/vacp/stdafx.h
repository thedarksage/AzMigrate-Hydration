// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//
#pragma once

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <winbase.h>

#include <cstdlib>

#include "..\common\win32\DiskHelpers.h"

// STL includes
#include <vector>
#include <algorithm>
#include <string>
#include <fstream>
#include <sstream>
#include <time.h>

// _ASSERTE declaration (used by ATL) and other macros
#include "macros.h"
//Use ATL macros for converting one type of string to another.
#include <atlconv.h>
#include <iostream>
#include <tchar.h>
#define _ATL_CSTRING_EXPLICIT_CONSTRUCTORS	// some CString constructors will be explicit

#include <atlbase.h>
#include <VersionHelpers.h>

// Do not deprecate sprint
#define STRSAFE_NO_DEPRECATE
// Used for safe string manipulation
#include <strsafe.h>

#define FUNCTION_NAME __FUNCTION__

//
// Debug utility functions
//
enum SV_LOG_LEVEL {
    SV_LOG_DISABLE,
    SV_LOG_FATAL,
    SV_LOG_SEVERE,
    SV_LOG_ERROR,
    SV_LOG_WARNING,
    SV_LOG_INFO,
    SV_LOG_DEBUG,
    SV_LOG_ALWAYS,
    SV_LOG_LEVEL_COUNT
};

extern void inm_printf(const char * format, ...);
extern void inm_printf(short logLevl, const char* format, ...);

//extern void DebugPrintf(const char * format, ...);
//extern void DebugPrintf(SV_LOG_LEVEL level, const char * format, ...);


#define MAX_DRIVES	26
#define DebugPrintf inm_printf

#ifdef DEBUG
#define XDebugPrintf inm_printf
#define XDebugWPrintf wprintf
#else
#define XDebugPrintf
#define XDebugWPrintf
#endif


#define MAXIMUM_DISK_INDEX          128
#define MAX_CONS_MISSING_INDICES    20

//We need to undefine __WFILE__  macro  as the same is deined in  driver\Include\win32\DrvDefines.h
//file which cause macro redefinition error. Either we need to change Drvdefines.h file or we need to 
//undefine here. I prefered undefine as i dont want to modify drivers file.
#ifdef __WFILE__
#undef __WFILE__
#endif
