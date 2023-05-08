#include "stdafx.h"
#include<windows.h>
#include "terminateonheapcorruption.h"

// Open Registry Hive SOFTWARE
// Flush using registry call
// Close the hive

int _tmain(int argc, _TCHAR* argv[])
{
	TerminateOnHeapCorruption();
	//LPCTSTR Str = "SOFTWARE";
	HKEY hkRes = ERROR_SUCCESS;
    if (ERROR_SUCCESS == RegOpenKeyExA(HKEY_LOCAL_MACHINE,"SOFTWARE", 0,KEY_ALL_ACCESS, &hkRes))
		printf(" HKLM_software registry hive has opened\n");
	else 
		printf(" Failed to Open HKLM_software Registry hive \n");

	if (ERROR_SUCCESS == RegFlushKey(hkRes))
		printf(" HKLM_software Registry hive got flushed \n");
	else
		printf(" Flusing HKLM_software Registry hive got failed \n");
	RegCloseKey(hkRes);
}