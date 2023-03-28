//
// Implements toString() function for SVERROR
//
#include <assert.h>
#include <stdio.h>                      // _snprintf()
#include <stdlib.h>
#include "svtypes.h"
#include "hostagenthelpers_ported.h"    // GetThreadsafeErrorStringBuffer(), ARRAYSIZE()

#undef SVSTATUS__H                      // Need to re-include this file (first include from svtypes.h)
#define SVSTATUS_GET_STRINGS
#include "svstatus.h"
static char const* s_rgSVStatusList[] = { SVSTATUS_LIST };
#undef SVSTATUS_GET_STRINGS

char const* SVERROR::toString()
{
    /* place holder function */
    char* pszError = GetThreadsafeErrorStringBuffer();
    return( pszError );
}
