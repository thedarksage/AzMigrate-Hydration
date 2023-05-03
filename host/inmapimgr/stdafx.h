// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently,
// but are changed infrequently

#pragma once
#pragma warning(disable : 4503)
#pragma warning(disable : 4995)
#pragma warning(disable : 4018)
#pragma warning(disable : 4996)

#ifndef STRICT
#define STRICT
#endif

#include "targetver.h"

#define _ATL_APARTMENT_THREADED
#define _ATL_NO_AUTOMATIC_NAMESPACE

#define _ATL_CSTRING_EXPLICIT_CONSTRUCTORS	// some CString constructors will be explicit

#include "resource.h"
#include <atlbase.h>
#include <atlcom.h>
#include <atlctl.h>

using namespace ATL;

#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
// Windows Header Files:
#include <windows.h>
#include <stdio.h>
#include <tchar.h>
#include <iostream>
using namespace std;

#include <string.h>
#include <strsafe.h>
