
/*****************************************************************************
 * vsnap_kernel_helpers.h
 *
 * Copyright (c) 2007 InMage
 * This file contains proprietary and confidential information and remains
 * the unpublished property of InMage. Use, disclosure, or reproduction is
 * prohibited except as permitted by express written license agreement with
 * Inmage
 *
 * Author:              InMage Systems
 *                      http://www.inmage.net
 * Creation date:       Wed, Feb 28, 2007
 * Description:         Declarations of kernel helper routines.
 *
 * Revision:            $Revision: 1.16 $
 * Checked in by:       $Author: lamuppan $
 * Last Modified:       $Date: 2015/02/24 03:29:23 $
 *
 * $Log: vsnap_kernel_helpers.h,v $
 * Revision 1.16  2015/02/24 03:29:23  lamuppan
 * Bug #1619650, 1619648, 1619642, 1619640, 1619637, 1619636, 1619635: Fix for policheck
 * bugs for Filter and Vsnap drivers. Please refer to bugs in TFS for more details.
 *
 * Revision 1.15  2014/11/17 12:59:48  lamuppan
 * TFS Bug #1057085: Fixed the issue by fixing the STR_COPY function. Please
 * go through the bug for more details.
 *
 * Revision 1.14  2014/10/10 08:04:12  praweenk
 * Bug ID:917800
 * Description: Safe API changes for filter and VSNAP driver cross platform.
 *
 * Revision 1.13  2014/07/16 07:11:58  vegogine
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
 * Revision 1.11.36.1  2014/05/06 08:56:44  chvirani
 * 29837, 29838, 29852: segmap changes, endian changes, crc changes
 * remove unused code, comments cleanup,
 *
 * Revision 1.11  2012/08/31 07:14:33  hpadidakul
 * Fixing the build issue on Solaris and AIX, issue is regresstion of bug 22914 checkin.
 * Unit tested by: Hari
 *
 * Revision 1.10  2011/01/13 06:25:26  chvirani
 * 14878: Changes to support AIX.
 *
 * Revision 1.9  2010/10/19 04:51:28  chvirani
 * 14174: Remove all references to raw device as we no longer require it
 *
 * Revision 1.8  2010/10/01 10:06:58  chvirani
 * 13869: Full disk support
 *
 * Revision 1.7  2010/07/19 05:37:57  chvirani
 * 13206: memmove not defined in sol 8/9. so define it as bcopy() for all sol
 *
 * Revision 1.6  2010/07/05 07:13:07  chvirani
 * 12934: Allocate bio_data only for I/O being processed.
 *             Limit max parallel I/O for reduced mem usage.
 *
 * Revision 1.5  2010/06/09 10:10:04  chvirani
 * Support for sparse retention
 *
 * Revision 1.4  2010/03/10 15:15:54  hpadidakul
 *
 * Vsnap Solaris changes porting from Linux.
 * Code reviewed by: Chirag.
 *
 * Smokianide test done by: Sahoo.
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
 * Revision 1.3  2009/07/03 07:35:38  chvirani
 * 8920: zfs support and proc error reporting
 *
 * Revision 1.2  2009/05/19 13:57:36  chvirani
 * Solaris 9 support. Map mem* functions to b* functions, remove devfs_clean()
 * Proc changes.
 *
 * Revision 1.1  2009/04/28 12:36:16  cswarna
 * Adding these files as part of vsnap driver code re-org
 *
 * Revision 1.5  2008/03/25 12:59:52  praveenkr
 * code cleanup for Solaris
 *
 * Revision 1.4  2007/12/04 12:53:38  praveenkr
 * fix for the following defects -
 *  4483 - Under low memory condition, vsnap driver suspended forever
 * 	4484 - vsnaps & dataprotection not responding
 * 	4485 - vsnaps utilizing more than 90% CPU
 *
 * Revision 1.3  2007/09/14 13:11:16  sudhakarp
 * Merge from DR_SCOUT_4-0_DEV/DR_SCOUT_4-1_BETA2 to HEAD ( Onbehalf of RP).
 *
 * Revision 1.1.2.9  2007/07/26 13:08:33  praveenkr
 * fix for 3788: vsnap of diff target showing same data
 * replaced strncmp with strcmp
 *
 * Revision 1.1.2.8  2007/07/24 12:58:30  praveenkr
 * virtual volume (volpack) implementation
 *
 * Revision 1.1.2.7  2007/07/17 13:53:25  praveenkr
 * clean-up - unix-style naming, tab-spacing = 8, 78 col
 *
 * Revision 1.1.2.6  2007/05/25 12:54:17  praveenkr
 * deleted unused functions - left behind during porting
 * DELETE_FILE, GET_FILE_SIZE, MEM_COPY
 *
 * Revision 1.1.2.5  2007/05/25 09:45:44  praveenkr
 * added comments & partial clean-up of code
 *
 * Revision 1.1.2.4  2007/04/24 12:49:47  praveenkr
 * page cache implementation(write)
 *
 * Revision 1.1.2.2  2007/04/17 15:36:30  praveenkr
 * #2944: 4.0_rhl4target-target is crashed when writting the data to R/W vsnap.
 * Sync-ed most of the data types keeping Trunk Windows code as bench-mark.
 * Did minimal cleaning of the code too.
 *
 * Revision 1.1.2.1  2007/04/11 19:49:37  gregzav
 *   initial release of the new make system
 *   note that some files have been moved from Linux to linux (linux os is
 *   case sensitive and we had some using lower l and some using upper L for
 *   linux) this is to make things consitent also needed to move cdpdefs.cpp
 *   under config added the initial brocade work that had been done.
 *
 *****************************************************************************/

#ifndef _INMAGE_VSNAP_KERNEL_HELPERS_H
#define _INMAGE_VSNAP_KERNEL_HELPERS_H

#include "common.h"

/*
 * OS Specific types
 */

extern struct inm_sol_disk;

typedef struct inm_sol_sync {
	kmutex_t 	is_mutex;
	kcondvar_t	is_cv;
} inm_sol_sync_t;

/*
 * End OS Specific types
 */

#define INM_PAGE_SIZE 	4096

/* File open flags */
#define INM_RDONLY 	FREAD
#define INM_RDWR 	FREAD | FWRITE | FSYNC
#define INM_CREAT 	FCREAT
#define INM_LYR		0
#define INM_LARGEFILE	O_LARGEFILE

#define CALC_DEV_T(major, minor)	makedevice(major, minor)

/* IPC related structure & functions declarations */
typedef kmutex_t			inm_mutex_t;
#define INM_INIT_MUTEX(m)		mutex_init(m, NULL, MUTEX_DEFAULT, NULL)
#define INM_ACQ_MUTEX(m)	 	mutex_enter(m)
#define INM_REL_MUTEX(m)	 	mutex_exit(m)
#define INM_INIT_MUTEX_LOCKED(m) 	INM_INIT_MUTEX(m); \
					INM_ACQ_MUTEX(m)
#define INM_ACQ_MUTEX_INTERRUPTIBLE(m)  mutex_enter(m)

typedef krwlock_t			inm_rwlock_t;
#define INM_INIT_LOCK(x)		rw_init(x, NULL, RW_DEFAULT, NULL)
#define INM_READ_LOCK(x)		rw_enter(x, RW_READER)
#define INM_READ_UNLOCK(x)		rw_exit(x)
#define INM_WRITE_LOCK(x)		rw_enter(x, RW_WRITER)
#define INM_WRITE_UNLOCK(x)		rw_exit(x)

typedef kmutex_t				inm_spinlock_t;
#define INM_INIT_SPINLOCK(m)			mutex_init(m, NULL, MUTEX_SPIN, NULL)
#define INM_ACQ_SPINLOCK(m)			mutex_enter(m)
#define INM_REL_SPINLOCK(m)			mutex_exit(m)
#define INM_ACQ_SPINLOCK_IRQ(m, x)		mutex_enter(m)
#define INM_REL_SPINLOCK_IRQ(m, x)		mutex_exit(m)
#define INM_ACQ_SPINLOCK_IRQSAVE(m, f)		mutex_enter(m)
#define INM_REL_SPINLOCK_IRQRESTORE(m, f)	mutex_exit(m)

typedef unsigned int			inm_atomic_t;
#define INM_ATOMIC_SET(var, val)        (*((inm_atomic_t *)var) = (val))
#define INM_ATOMIC_INC(var)             atomic_add_32((inm_u32_t *)var, 1)
#define INM_ATOMIC_DEC(var)             atomic_add_32((inm_u32_t *)var, -1)
#define INM_ATOMIC_READ(var)            atomic_add_32_nv((inm_u32_t *)var, 0)
#define INM_ATOMIC_DEC_AND_TEST(var)    (atomic_add_32_nv((inm_u32_t *)var, -1) == 0)
#define INM_ATOMIC_INC_AND_TEST(var)    (atomic_add_32_nv((inm_u32_t *)var, 1) == 0)
#define INM_ATOMIC_DEC_RETURN(var, ret) (ret = atomic_add_32_nv((inm_u32_t *)var, -1))

typedef inm_sol_sync_t			inm_ipc_t;

#define INM_INIT_IPC(is)     		                       \
	do {                                                   \
        	INM_INIT_MUTEX(&((is)->is_mutex));             \
	        cv_init(&((is)->is_cv), NULL, CV_DRIVER, NULL);\
	}while (0)

#define INM_INIT_SLEEP(is)              INM_ACQ_MUTEX(&((is)->is_mutex))

#define INM_SLEEP(is)          		                                   \
	do {                                                               \
        	ASSERT(!mutex_owned(&((is)->is_mutex))); /* Remove this */ \
	        cv_wait(&((is)->is_cv), &((is)->is_mutex));                \
	}while (0)

#define INM_FINI_SLEEP(is)              INM_REL_MUTEX(&((is)->is_mutex))

#define INM_WAKEUP(is) 		                   \
	do {                                       \
        	INM_ACQ_MUTEX(&((is)->is_mutex));  \
	        cv_signal(&((is)->is_cv));         \
        	INM_REL_MUTEX(&((is)->is_mutex));  \
	}while (0)

/*
 * Bit Ops
 */
#define INM_SET_BIT(bit, flags)		(*(flags) = *(flags) | (bit))
#define INM_TEST_BIT(bit, flags)	((*(flags) & bit ) == (bit))
#define INM_CLEAR_BIT(bit, flags)	(*(flags) = *(flags) & ~(bit))

/* mem related*/
/* kmem_alloc flags */
#define GFP_KERNEL 	KM_SLEEP
#define GFP_NOIO 	KM_SLEEP

/* Platform specific definitions */
typedef struct buf		inm_block_io_list_t;
typedef struct buf		inm_block_io_t;
typedef struct inm_sol_disk	inm_disk_t;
typedef struct taskq_t		inm_queue_t;
typedef offset_t		inm_offset_t;
typedef intptr_t		inm_ioctl_arg_t;
typedef major_t			inm_major_t;
typedef minor_t			inm_minor_t;
typedef char			inm_page_t;
typedef dev_t			inm_dev_t;
typedef char			inm_interrupt_t;
/*
 * alloc_page and free page should be inlined for
 * non debug driver. For debug, since we have large routines
 * with lots of messages, compiler complains, so define them
 * as a regular function
 */
inm_page_t *alloc_page(inm_u32_t );
void __free_page(inm_page_t *);

/*
 * Solaris 9 and earlier does not support
 * mem* operations. #define to corresponding
 * b* operations
 */
/* NOTE: Order of source and destination is reverse in bcopy */
#define memmove(dst, src, size)		bcopy(src, dst, size)
#define memzero(ptr, size)		bzero(ptr, size)
#define memcmp(ptr1, ptr2, size)	bcmp(ptr1, ptr2, size)

/*
 * End OS Specific section
 */

/*
 * This is generic declaration, definition - no changes required
 * below this for porting
 */
/* page cache related structure & fns declarations */
typedef struct inm_vdev_page_data_tag {
	inm_page_t      *pd_bvec_page;	/* page/buffer associated with I/O */
	inm_u32_t	pd_bvec_offset;	/* offset within the page */
	inm_offset_t	pd_disk_offset;	/* disk block/offset to perfom IO on */
	inm_u64_t	pd_file_offset;	/* file involved? then offset within */
	inm_u32_t	pd_size;	/* size of data to copy */
} inm_vdev_page_data_t;

/*
 * Hacks for memory alignment
 */

#ifdef _MEM_ALIGN_
#define INM_ASSIGN(DEST, SRC, TYPE) \
	memcpy_s((DEST), sizeof(TYPE), (SRC), sizeof(TYPE));
#else
#define INM_ASSIGN(DEST, SRC, TYPE) \
	*((TYPE *)(DEST)) = *(SRC);
#endif

/* string related fns declarations */
#define STRING_PRINTF(x)	snprintf x;
void STR_COPY(char *dest, char *src, inm_32_t len);
void STRING_CAT(char *str1, inm_32_t dlen, char *str2, inm_32_t len);

/* FILE OPS */
inm_32_t STRING_MEM(char *);
inm_32_t INM_OPEN_FILE(char *, inm_u32_t, void **);
inm_32_t INM_READ_FILE(void *, void *, inm_u64_t, inm_u32_t, inm_u32_t *);
inm_32_t inm_vdev_read_page(void *, inm_vdev_page_data_t *);
inm_32_t INM_WRITE_FILE(void *, void *, inm_u64_t, inm_u32_t, inm_u32_t *);
inm_32_t inm_vdev_write_page(void *, inm_vdev_page_data_t *);
inm_32_t INM_SEEK_FILE_END(void *, inm_u64_t *);
void INM_CLOSE_FILE(void *, inm_32_t mode);

/*
 * Copying to and from user space
 * and page to virtual addresss
 */
void inm_copy_buf_to_page(inm_page_t *page, void *buf, inm_u32_t len);
void inm_copy_page_to_buf(void *buf, inm_page_t *page, inm_u32_t len);
inm_32_t inm_copy_from_user(void *kerptr, void __user *userptr, size_t size);
inm_32_t inm_copy_to_user(void __user *userptr, void *kerptr, size_t size);
void inm_bzero_page(inm_page_t *page, inm_u32_t offset, inm_u32_t len);

/* do_div() for solaris */
inm_64_t divide(inm_64_t *, inm_64_t);
#define do_div(a,b)	divide((inm_64_t *)&(a), (inm_64_t)b)

/* For endianness conversion */
void memrev(char *, size_t);
inm_u16_t memrev_16(inm_u16_t );
inm_u32_t memrev_32(inm_u32_t x);
inm_u64_t memrev_64(inm_u64_t x);

#endif /* _INMAGE_VSNAP_KERNEL_HELPERS_H */
