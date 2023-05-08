/*
 * Copyright (c) 2005 InMage
 * This file contains proprietary and confidential information and
 * remains the unpublished property of InMage. Use, disclosure,
 * or reproduction is prohibited except as permitted by express
 * written license aggreement with InMage.
 *
 * File       : file_port_linux.cpp
 *
 * Description: Linux specific implementation of file.cpp
 */

#include <string>
#include "entity.h"

#include "globs.h"

#include "portablehelpers.h"
#include "error.h"
#include "synchronize.h"
#include "globs.h"

#include "genericfile.h"
#include "file.h"


/*
 * FUNCTION NAME : PreAllocate
 *
 * DESCRIPTION : Allocates space on the secondary memory
 *
 * INPUT PARAMETERS : size in bytes to allocate
 *
 * OUTPUT PARAMETERS : none
 *
 * NOTES :
 *
 * return value : true if success false otherwise
 *
 */
bool File::PreAllocate(const SV_ULONGLONG &size)
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
	bool rv = true;

	if(posix_fallocate( GetHandle(), SEEK_SET, size)!=0)
	{
		DebugPrintf(SV_LOG_ERROR, "Failed to allocate space for %s on the disk.\n",	GetName().c_str());
		rv = false;
	}

	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
	return rv;
}
