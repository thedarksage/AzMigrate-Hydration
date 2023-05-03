

#include "stdafx.h"
#include "resource.h"
#include "InMageVssProvider_i.h"
#include "dllmain.h"
#include "inmsafecapis.h"

CInMageVssProviderModule _AtlModule;


extern "C" BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved)
{	
	hInstance;
	init_inm_safe_c_apis();
	return _AtlModule.DllMain(dwReason, lpReserved); 
}
