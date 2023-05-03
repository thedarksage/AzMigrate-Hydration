
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
 * Revision:            $Revision: 1.7 $
 * Checked in by:       $Author: lamuppan $
 * Last Modified:       $Date: 2015/02/24 03:29:23 $
 *
 * $Log: vsnap_kernel_helpers.h,v $
 * Revision 1.7  2015/02/24 03:29:23  lamuppan
 * Bug #1619650, 1619648, 1619642, 1619640, 1619637, 1619636, 1619635: Fix for policheck
 * bugs for Filter and Vsnap drivers. Please refer to bugs in TFS for more details.
 *
 * Revision 1.6  2014/11/17 12:59:48  lamuppan
 * TFS Bug #1057085: Fixed the issue by fixing the STR_COPY function. Please
 * go through the bug for more details.
 *
 * Revision 1.5  2014/10/10 08:04:05  praweenk
 * Bug ID:917800
 * Description: Safe API changes for filter and VSNAP driver cross platform.
 *
 * Revision 1.4  2014/07/16 07:11:57  vegogine
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
 * Revision 1.2.36.1  2014/05/06 08:56:20  chvirani
 * 29837, 29838, 29852: segmap changes, endian changes, crc changes
 * remove unused code, comments cleanup,
 *
 * Revision 1.2  2012/08/31 07:14:32  hpadidakul
 * Fixing the build issue on Solaris and AIX, issue is regresstion of bug 22914 checkin.
 * Unit tested by: Hari
 *
 * Revision 1.1  2011/01/13 06:19:37  chvirani
 * 14878: AIX kernel driver initial drop
 *
 * Revision 1.3  2011/01/04 10:02:26  chvirani
 * working aix code
 *
 * Revision 1.2  2010/09/09 08:03:06  chvirani
 * Initial AIX support
 *
 * Revision 1.1  2010/08/10 11:00:08  chvirani
 * initial aix support
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
 * 	4483 - Under low memory condition, vsnap driver suspended forever
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

/*
 * End OS Specific types
 */

#define INM_PAGE_SIZE 	4096

/* File open flags */
#define INM_RDONLY 	O_RDONLY
#define INM_RDWR 	O_RDWR
#define INM_CREAT 	O_CREAT
#define INM_LYR		0
#define INM_DIRECT	O_DIRECT
#define INM_LARGEFILE	O_LARGEFILE

#define CALC_DEV_T(major, minor)	makedevno(major, minor)

/* IPC related structure & functions declarations */
typedef Simple_lock			inm_mutex_t;
#define INM_INIT_MUTEX(m)						       \
			do {						       \
				lock_alloc(m, LOCK_ALLOC_PAGED, 0, -1);        \
				simple_lock_init(m);			       \
			} while(0)

#define INM_ACQ_MUTEX(m)	 	simple_lock(m)
#define INM_REL_MUTEX(m)	 	simple_unlock(m)
#define INM_INIT_MUTEX_LOCKED(m)					       \
			do {						       \
				INM_INIT_MUTEX(m); 			       \
				INM_ACQ_MUTEX(m);			       \
			} while(0)					       \

#define INM_ACQ_MUTEX_INTERRUPTIBLE(m)  simple_lock(m)
#define INM_DESTROY_MUTEX(m)		lock_free(m)

typedef Complex_lock			inm_rwlock_t;
#define INM_INIT_LOCK(x)						       \
			do {						       \
				lock_alloc(x, LOCK_ALLOC_PAGED, 0, -1);        \
				lock_init(x, 0);			       \
			} while(0)
#define INM_READ_LOCK(x)		lock_read(x)
#define INM_READ_UNLOCK(x)		lock_done(x)
#define INM_WRITE_LOCK(x)		lock_write(x)
#define INM_WRITE_UNLOCK(x)		lock_done(x)

typedef Simple_lock			inm_spinlock_t;
#define INM_INIT_SPINLOCK(m)						       \
			do {						       \
				lock_alloc(m, LOCK_ALLOC_PIN, 0, -1);	       \
				simple_lock_init(m);\
			} while(0)

#define INM_ACQ_SPINLOCK(m)		simple_lock(m)
#define INM_REL_SPINLOCK(m)		simple_unlock(m)
#define INM_ACQ_SPINLOCK_IRQ(m, i)	i = disable_lock(INTIODONE, m)
#define INM_REL_SPINLOCK_IRQ(m, i)	unlock_enable(i, m)
#define INM_DESTROY_SPINLOCK(m)		lock_free(m)

typedef int				inm_atomic_t;
#define INM_ATOMIC_SET(var, val)        (*var) = (val)
#define INM_ATOMIC_INC(var)		fetch_and_add((var), 1);
#define INM_ATOMIC_DEC(var)             fetch_and_add((var), -1)
#define INM_ATOMIC_READ(var)            fetch_and_add((var), 0)
					/* fetch and add returns orig value */
#define INM_ATOMIC_DEC_AND_TEST(var)    (fetch_and_add((var), -1) == 1)
#define INM_ATOMIC_INC_AND_TEST(var)    (fetch_and_add((var), 1) == -1)
#define INM_ATOMIC_DEC_RETURN(var, ret)	(ret = (fetch_and_add((var), -1) - 1))



typedef struct inm_aix_sync {
	tid_t		is_event;
	inm_mutex_t	is_lock;
} inm_ipc_t;

#define INM_INIT_IPC(is)						       \
			do {						       \
				(is)->is_event = EVENT_NULL;		       \
				INM_INIT_MUTEX(&((is)->is_lock));	       \
			} while(0)

#define INM_INIT_SLEEP(is)             	INM_ACQ_MUTEX(&((is)->is_lock))
#define INM_SLEEP(is) 			e_sleep_thread(&((is)->is_event),      \
						       &(is)->is_lock,         \
						       LOCK_SIMPLE)

#define INM_FINI_SLEEP(is)              INM_REL_MUTEX(&((is)->is_lock))
#define INM_WAKEUP(is)							       \
			do {						       \
				INM_ACQ_MUTEX(&((is)->is_lock));	       \
				e_wakeup_one(&((is)->is_event));	       \
				INM_REL_MUTEX(&((is)->is_lock));	       \
			} while(0)


/*
 * Bit Ops
 */
#define INM_SET_BIT(bit, flags)		(*(flags) = *(flags) | (bit))
#define INM_TEST_BIT(bit, flags)	((*(flags) & bit ) == (bit))
#define INM_CLEAR_BIT(bit, flags)	(*(flags) = *(flags) & ~(bit))

/* mem related*/
/* kmem_alloc flags */
#define GFP_KERNEL 	kernel_heap
#define GFP_NOIO 	kernel_heap
#define GFP_PINNED	pinned_heap

#define VDEV_NAME_SIZE			16
#define INM_VDEV_NAME(vdev)		(vdev)->vd_disk->ad_name

typedef struct aix_disk {
	char 	ad_name[VDEV_NAME_SIZE];
} inm_disk_t;

/* Platform specific definitions */
typedef struct buf		inm_block_io_list_t;
typedef struct buf		inm_block_io_t;
typedef char			inm_queue_t;
typedef offset_t		inm_offset_t;
typedef caddr_t			inm_ioctl_arg_t;
typedef inm_32_t		inm_major_t;
typedef inm_32_t		inm_minor_t;
typedef char			inm_page_t;
typedef dev_t			inm_dev_t;
typedef dev_t			inm_parent_dev_t;
typedef inm_32_t		inm_interrupt_t;

/*
 * alloc_page and free page should be inlined for
 * non debug driver. For debug, since we have large routines
 * with lots of messages, compiler complains, so define them
 * as a regular function
 */
inm_page_t *alloc_page(inm_mem_flag_t);
void __free_page(inm_page_t *);

#define memzero(ptr, size)		bzero(ptr, size)


/*aix specific fixes, prototypes */
#define snprintf(p, n, format, arg...)	sprintf(p, format, ##arg)
void bsdlog (int , const char *, ...);



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
 * Ugly hacks for memory alignment
 */

#ifdef _MEM_ALIGN_
#define INM_ASSIGN(DEST, SRC, TYPE) \
	memcpy_s((DEST), sizeof(TYPE), (SRC), sizeof(TYPE))
#else
#define INM_ASSIGN(DEST, SRC, TYPE) \
	*((TYPE *)(DEST)) = *((TYPE *)(SRC))
#endif

/* string related fns declarations */
#define SPRINTF(p, n, format, arg...)	sprintf(p, format, ##arg)
#define STRING_PRINTF(x)		SPRINTF x
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

#endif /* _INMAGE_VSNAP_KERNEL_HELPERS_H */
