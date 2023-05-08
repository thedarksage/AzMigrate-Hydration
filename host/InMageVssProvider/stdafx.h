

#pragma once

#ifndef STRICT
#define STRICT
#endif

#include "targetver.h"

#define _ATL_APARTMENT_THREADED
#define _ATL_NO_AUTOMATIC_NAMESPACE

#define _ATL_CSTRING_EXPLICIT_CONSTRUCTORS	

#include <winsock2.h>
#include <windows.h>
#include <winbase.h>

#include "resource.h"
#include <atlbase.h>
#include <atlcom.h>
#include <atlctl.h>
#include <atlctl.h>
#include <atlconv.h>
#include <new>
#include <string>
#include <vector>
#include <map>

#include "vss.h"
#include "vsprov.h"
#include "vsadmin.h"
#include "Utility.h"
#include "InMageVssProviderEventLogMsgs.h"
#include <windows.h>

using namespace ATL;
