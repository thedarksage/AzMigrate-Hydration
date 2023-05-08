// inmapimgr.cpp : Defines the exported functions for the DLL application.
//

#include "stdafx.h"
#include "inmapimgr.h"


// This is an example of an exported variable
INMAPIMGR_API int ninmapimgr=0;

// This is an example of an exported function.
INMAPIMGR_API int fninmapimgr(void)
{
	return 42;
}

// This is the constructor of a class that has been exported.
// see inmapimgr.h for the class definition
Cinmapimgr::Cinmapimgr()
{
	return;
}
