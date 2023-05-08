/*
 * Copyright (c) 2005 InMage
 * This file contains proprietary and confidential information and
 * remains the unpublished property of InMage. Use, disclosure,
 * or reproduction is prohibited except as permitted by express
 * written license aggreement with InMage.
 *
 * File       : file_port_sun.cpp
 *
 * Description: Solaris specific implementation of file.cpp
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
    /* TODO: Need to find equivalent of posix_fallocate in sun os */
    return true;
}
