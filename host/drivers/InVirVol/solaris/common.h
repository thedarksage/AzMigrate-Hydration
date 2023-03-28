/*****************************************************************************
 * common.h
 *
 * Copyright (c) 2007 InMage
 * This file contains proprietary and confidential information and remains
 * the unpublished property of InMage. Use, disclosure, or reproduction is
 * prohibited except as permitted by express written license agreement with
 * Inmage
 *
 * Author:             InMage Systems
 *                     http://www.inmage.net
 * Creation date:      Wed, Feb 28, 2007
 * Description:        Common Includes & Definitions & Declarations
 *
 * Revision:           $Revision: 1.17 $
 * Checked in by:      $Author: lamuppan $
 * Last Modified:      $Date: 2015/02/24 03:29:23 $
 *
 * $Log: common.h,v $
 * Revision 1.17  2015/02/24 03:29:23  lamuppan
 * Bug #1619650, 1619648, 1619642, 1619640, 1619637, 1619636, 1619635: Fix for policheck
 * bugs for Filter and Vsnap drivers. Please refer to bugs in TFS for more details.
 *
 * Revision 1.16  2014/10/10 08:04:11  praweenk
 * Bug ID:917800
 * Description: Safe API changes for filter and VSNAP driver cross platform.
 *
 * Revision 1.15  2014/10/01 10:17:35  praweenk
 * Task ID: 851258
 * Review: Code reviewed by Prasad.
 * Description: Fix to support safe integer to memory allocation function for filter driver, vsnap driver and inm_dmit code.
 * Test: Manual Testing.
 * Build: Build on Linux, Aix and Solaris.
 *
 * Revision 1.14  2014/07/16 07:11:57  vegogine
 * Bug/Enhancement No : 30547
 *
 * Code changed/written by : Venu Gogineni
 *
 * Code reviewed by : N/A
 *
 * Unit tested by : N/A
 *
 * Smokianide-tested by : N/A
 *
 * Checkin Description : TRUNK rebase with SCOUT_7-2_OSS_SEC_FIXES_BRANCH code base.
 *
 * Revision 1.12.6.1  2014/05/06 08:56:29  chvirani
 * 29837, 29838, 29852: segmap changes, endian changes, crc changes
 * remove unused code, comments cleanup,
 *
 * Revision 1.12  2014/02/21 09:00:09  prkharch
 * Removing crc32.h
 *
 * Revision 1.11  2011/01/13 06:25:27  chvirani
 * 14878: Changes to support AIX.
 *
 * Revision 1.10  2010/10/04 08:39:30  chvirani
 * ALERT() macro modified to report message as error and not
 * information.
 *
 * Revision 1.9  2010/10/01 10:06:58  chvirani
 * 13869: Full disk support
 *
 * Revision 1.8  2010/03/23 14:04:06  chvirani
 * Mark pointers NULL on free
 *
 * Revision 1.7  2010/03/10 15:15:54  hpadidakul
 *
 * Vsnap Solaris changes porting from Linux.
 * Code reviewed by: Chirag.
 *
 * Smokianide test done by: Sahoo.
 *
 * Revision 1.3  2010/03/10 09:22:07  hpadidakul
 * Changes is to remove warning while building the solaris
 *
 * Revision 1.2  2010/03/05 09:09:24  hpadidakul
 * Vsnap performance related changes, backported from Linux code.
 *
 * TODO: 1) Need to add comments
 *            2) Need to cleanup/remove unwanted code
 *
 * Revision 1.1  2009/12/04 10:47:56  chvirani
 * phase 1 - final
 *
 * Revision 1.1  2009/11/18 13:38:06  chvirani
 * multithreaded
 *
 * Revision 1.6  2009/07/20 11:02:24  chvirani
 * 9139: We now have a VTOC for src < 1TB and EFI otherwise.
 * -> Also add support for ioctl failure log in proc (debug only).
 * -> Remove excessive debug messages that pollute the log files
 *
 * Revision 1.5  2009/07/03 07:35:38  chvirani
 * 8920: zfs support and proc error reporting
 *
 * Revision 1.4  2009/05/27 14:09:08  chvirani
 * 8695: 	Check if the devices have been deleted in i/o and open/close path.
 * 	Mark the soft state NULL on deletion.
 *
 * Revision 1.3  2009/05/19 13:57:36  chvirani
 * Solaris 9 support. Map mem* functions to b* functions, remove devfs_clean()
 * Proc changes.
 *
 * Revision 1.2  2009/05/04 12:30:45  chvirani
 * o Add EFI support
 * o Remove ldi references
 * o Fix user printf to make it more readable
 *
 * Revision 1.1  2009/04/28 12:36:15  cswarna
 * Adding these files as part of vsnap driver code re-org
 *
 * Revision 1.7  2008/08/12 22:27:23  praveenkr
 * fix for #5404: Decoupled the mount point away from creation of vsnap
 * Moved the decision on the device name away from driver into the CX
 * & 'cdpcli'
 *
 * Revision 1.6  2008/04/01 13:04:09  praveenkr
 * added memory debugging support (for debug build only)
 *
 * Revision 1.5  2008/03/25 12:59:52  praveenkr
 * code cleanup for Solaris
 *
 * Revision 1.4  2007/12/04 12:53:38  praveenkr
 * fix for the following defects -
 * 	4483 - Under low memory condition, vsnap driver suspended forever
 * 	4484 - vsnaps & dataprotection not responding
 * 	4485 - vsnaps utilizing more than 90% CPU
 *
 * Revision 1.3  2007/09/14 13:11:16  sudhakarp
 * Merge from DR_SCOUT_4-0_DEV/DR_SCOUT_4-1_BETA2 to HEAD ( Onbehalf of RP).
 *
 * Revision 1.1.2.9  2007/08/21 15:24:11  praveenkr
 * 1. added a new function - dr_vdev_kfree, a wrapper over kfree to force NULL
 *     the pointers while freeing to easily capture mem leaks/mem corruption
 * 2. made _VSNAP_DEBUG_ as makefile flag
 *
 * Revision 1.1.2.8  2007/07/24 12:58:30  praveenkr
 * virtual volume (volpack) implementation
 *
 * Revision 1.1.2.7  2007/07/17 13:53:25  praveenkr
 * clean-up - unix-style naming, tab-spacing = 8, 78 col
 *
 * Revision 1.1.2.6  2007/07/12 11:39:21  praveenkr
 * removed devfs usage; fix for RH5 build failure
 *
 * Revision 1.1.2.5  2007/06/18 13:10:44  praveenkr
 * Enabled Debug prints for ioctls to track #3495
 *
 * Revision 1.1.2.4  2007/05/31 15:11:39  praveenkr
 * formatted a debug log prints
 *
 * Revision 1.1.2.3  2007/05/25 09:45:43  praveenkr
 * added comments & partial clean-up of code
 *
 * Revision 1.1.2.2  2007/04/17 15:36:30  praveenkr
 * #2944: 4.0_rhl4target-target is crashed when writting the data to R/W vsnap.
 * Sync-ed most of the data types keeping Trunk Windows code as bench-mark.
 * Did minimal cleaning of the code too.
 *
 * Revision 1.1.2.1  2007/04/11 19:49:36  gregzav
 *   initial release of the new make system
 *   note that some files have been moved from Linux to linux (linux os is
 *   case sensitive and we had some using lower l and some using upper L for
 *   linux) this is to make things consitent also needed to move cdpdefs.cpp
 *   under config added the initial brocade work that had been done.
 *
 *****************************************************************************/

#ifndef _INMAGE_COMMON_H
#define _INMAGE_COMMON_H

#include <sys/types.h>
#include <sys/sysmacros.h>
#include <sys/uio.h>
#include <sys/kmem.h>
#include <sys/cred.h>
#include <sys/mman.h>
#include <sys/errno.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/modctl.h>
#include <sys/conf.h>
#include <sys/debug.h>
#include <sys/vnode.h>
#include <sys/fcntl.h>
#include <sys/pathname.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/vtoc.h>
#include <sys/dkio.h>
#include <sys/atomic.h>
#include <sys/disp.h>
#include <sys/sunndi.h>
#include <vm/seg_map.h>
#include <sys/open.h>
#include <sys/uuid.h>
#include <sys/byteorder.h>
#include <sys/scsi/scsi.h>
#include <sys/dktp/fdisk.h>

#ifdef _USE_CMLB_
#include <sys/cmlb.h>
#endif

#ifndef _SOLARIS_8_
#include <sys/efi_partition.h>
#endif

/* inmage datatypes */
/* Decalre all inmage headers after these typedefs */
typedef char			inm_8_t;
typedef unsigned char		inm_u8_t;
typedef short			inm_16_t;
typedef unsigned short		inm_u16_t;
typedef int			inm_32_t;
typedef unsigned int		inm_u32_t;
typedef long long		inm_64_t;
typedef unsigned long long	inm_u64_t;

#include "vdev_mem_debug.h"
#include "linux_list.h"
#include "safecapismajor.h"

#ifndef __FUNCTION__
#define __FUNCTION__ __func__
#endif

#ifndef __user
#define __user
#endif

#undef INLINE
#ifdef _VSNAP_DEBUG_
#define INLINE
#else
#define INLINE inline
#endif

#undef UPRINT
#define UPRINT(...) 							\
	do {								\
		uprintf(__VA_ARGS__);					\
		uprintf("\n");						\
	} while(0)

#undef DBG
#ifdef _VSNAP_DEBUG_
#define DBG(...)							\
	do {								\
		printf("VDEV[%s:%d]:(DBG)", __FUNCTION__, __LINE__); 	\
		printf(__VA_ARGS__);					\
	} while(0)
#else
#define DBG(...)	/* Do nothing */
#endif

#undef DEV_DBG
#ifdef _VSNAP_DEV_DEBUG_
#define DEV_DBG(...)							\
	do {								\
		printf("VDEV[%s:%d]:(TEST)", __FUNCTION__, __LINE__); 	\
		printf(__VA_ARGS__);					\
	} while(0);
#else
#define DEV_DBG(...)	/* Do nothing */
#endif

#undef INFO
#define INFO(...) 							\
	do {								\
	cmn_err(CE_NOTE, __VA_ARGS__);                                  \
	} while(0)


#undef ERR
#define ERR(...) 							\
	do {								\
		cmn_err(CE_WARN, "VDEV[%s:%d]:(ERR)", __FUNCTION__, 	\
			__LINE__); 					\
		cmn_err(CE_WARN, __VA_ARGS__);				\
	} while(0);

#undef ALERT
#ifdef _VSNAP_DEBUG_
#define ALERT()								\
	do {								\
		cmn_err(CE_PANIC, "VDEV[%s:%d]:(Bug)\n", __FUNCTION__, __LINE__);					\
	} while(0);
#else
#define ALERT()	\
	ERR("VDEV[%s:%d]:(Bug)\n", __FUNCTION__, __LINE__);
#endif

/* wrapper around BUG_ON for release build */
#undef ASSERT
#define ASSERT(EXPR)			\
	{				\
		if (EXPR) {		\
			ALERT(); 	\
		}			\
	}

/*
 * wrapper around kmalloc for porting
 */

#define CHECK_OVERFLOW(size)\
({\
    int ret;\
    unsigned long long tmp = (unsigned long long)size;\
    if(tmp < ((size_t) - 1)){\
        ret = 0;\
    }else {\
        ret = -1;\
    }\
    ret;\
})

#define INM_ALLOC_MEM(size, flag)\
({\
    void *rptr = NULL;\
    if(!CHECK_OVERFLOW(size)) {\
        rptr = kmem_alloc(size, flag);\
    }\
    rptr;\
})

#define INM_FREE_MEM(p, size, flag)	kmem_free(p, size);

#ifdef _VSNAP_DEBUG_
#define INM_ALLOC(size, flag)						\
	inm_verify_alloc(size, flag, __FUNCTION__, __LINE__)
#define INM_ALLOC_WITH_INFO(size, flag, func, line)                     \
        inm_verify_alloc(size, flag, func, line)
#else
#define INM_ALLOC(size, flag)		INM_ALLOC_MEM(size, flag)
#endif


/*
 * wrapper around kfree for debugging
 * if double freeing then BUG_ON
 */
#ifdef _VSNAP_DEBUG_

#define VERIFY_DOUBLE_FREE(ptrptr, size, F, L, D)				\
	do {								\
		if (!(*ptrptr)) { 						\
			ERR("extra kfree");					\
			ASSERT(1);						\
		} else { 							\
			inm_verify_for_mem_leak_kfree(*ptrptr, size, 0,	\
							F, L, D);	\
			*ptrptr = NULL; 					\
		}								\
	} while(0)

#define INM_FREE(pp, size)	VERIFY_DOUBLE_FREE(pp, size, __FUNCTION__, __LINE__, 0)

#define INM_FREE_VERBOSE(pp, size)      \
        VERIFY_DOUBLE_FREE(pp, size, __FUNCTION__, __LINE__, 1)

#define INM_FREE_WITH_INFO(pp, size, func, line)                \
        VERIFY_DOUBLE_FREE(pp, size, func, line, 0)

#else

#define INM_FREE(pp, size)		do {				     \
						INM_FREE_MEM(*pp, size, 0);  \
						*pp = NULL;		     \
					} while(0)
#endif

#define INM_FREE_PINNED(pp, size)	INM_FREE(pp, size)

#define INM_MIN(a,b)		((a) < (b))?(a):(b)

#endif	/* _INMAGE_COMMON_H */

