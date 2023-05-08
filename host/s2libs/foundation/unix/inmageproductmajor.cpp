/*
 * Copyright (c) 2005 InMage
 * This file contains proprietary and confidential information and
 * remains the unpublished property of InMage. Use, disclosure,
 * or reproduction is prohibited except as permitted by express
 * written license aggreement with InMage.
 *
 * File       : inmageproduct_port.cpp
 *
 * Description: Linux specific port of inmageproduct.cpp
 */


#include <string>
#include <cassert>

#include "inmageproduct.h"
#include "entity.h"
#include "error.h"
#include "portableheaders.h"
#include "hostagenthelpers_ported.h"
#include "synchronize.h"
#include "version.h"

/*
 * FUNCTION NAME : GetProductVersionFromResource
 *
 * DESCRIPTION : 
 *
 * INPUT PARAMETERS : 
 *
 * OUTPUT PARAMETERS : 
 *
 * NOTES :
 *
 * return value : 
 *
 */
bool InmageProduct::GetProductVersionFromResource( char* pszFile,char* pszBuffer,int pszBufferSize )
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
	DebugPrintf(SV_LOG_DEBUG,"This function is not needed in unix environments.\n");
	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return false;
}

