/*****************************************************************************
 * vsnap_mem_debug.h
 *
 * Copyright (c) 2007 InMage
 * This file contains proprietary and confidential information and remains
 * the unpublished property of InMage. Use, disclosure, or reproduction is
 * prohibited except as permitted by express written license agreement with
 * Inmage
 *
 * Author:             InMage Systems
 *                     http://www.inmage.net
 * Creation date:      Wed, Jan 30, 2008
 * Description:        for memory debugging
 *
 * Revision:           $Revision: 1.4 $
 * Checked in by:      $Author: chvirani $
 * Last Modified:      $Date:
 *
 * $Log: vdev_mem_debug.h,v $
 * Revision 1.4  2011/01/13 06:34:52  chvirani
 * 14878: Changes to support AIX.
 *
 * Revision 1.3  2010/06/09 10:10:04  chvirani
 * Support for sparse retention
 *
 * Revision 1.2  2010/03/10 12:08:43  chvirani
 * Multithreaded Driver
 *
 * Revision 1.1  2009/04/28 12:34:38  cswarna
 * Adding these files as part of vsnap driver code re-org
 *
 * Revision 1.1  2008/12/30 18:10:43  chvirani
 * Multithreaded pool changes, caching and other improvements - unfinished
 *
 * Revision 1.1  2008/04/01 13:04:09  praveenkr
 * added memory debugging support (for debug build only)
 *
 * Revision 1.1  2008/01/30 15:42:43  praveenkr
 * added support for debugging memory leaks
 *
 *****************************************************************************/

#ifndef _VSNAP_MEM_DEBUG_H_
#define _VSNAP_MEM_DEBUG_H_

#ifdef _AIX_
typedef caddr_t		inm_mem_flag_t;
#else
typedef int		inm_mem_flag_t;
#endif

#ifdef _VSNAP_DEBUG_

extern inm_u64_t mem_used;
#undef MEM_STATS
#define MEM_STATS mem_used

/*
 * Memory Hard limit
 * Set it to 0 for unlimited memory
 */
#define MAX_MEM_ALLOWED 0

void* inm_verify_alloc(size_t, inm_mem_flag_t, const char[], int32_t);
void inm_verify_for_mem_leak_kfree(const void *, size_t, inm_mem_flag_t,
				   const char[], int32_t, int32_t);

void inm_mem_init(void);
void dump_memory_list(void);
void inm_mem_destroy(void);

#endif

#endif /* _VSNAP_MEM_DEBUG_H_ */
