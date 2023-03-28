
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
 * Revision:            $Revision: 1.25 $
 * Checked in by:       $Author: lamuppan $
 * Last Modified:       $Date: 2015/02/24 03:29:23 $
 *
 * $Log: vsnap_kernel_helpers.h,v $
 * Revision 1.25  2015/02/24 03:29:23  lamuppan
 * Bug #1619650, 1619648, 1619642, 1619640, 1619637, 1619636, 1619635: Fix for policheck
 * bugs for Filter and Vsnap drivers. Please refer to bugs in TFS for more details.
 *
 * Revision 1.24  2014/11/17 12:59:48  lamuppan
 * TFS Bug #1057085: Fixed the issue by fixing the STR_COPY function. Please
 * go through the bug for more details.
 *
 * Revision 1.23  2014/10/10 08:04:09  praweenk
 * Bug ID:917800
 * Description: Safe API changes for filter and VSNAP driver cross platform.
 *
 * Revision 1.22  2014/07/16 07:11:57  vegogine
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
 * Revision 1.20.2.1  2014/05/06 09:35:38  chvirani
 * 29837, 29838, 29852: segmap changes, endian changes, crc changes
 * remove unused code, comments cleanup,
 *
 * Revision 1.20  2014/03/14 12:49:29  lamuppan
 * Changes for lists, kernel helpers.
 *
 * Revision 1.19  2012/08/31 07:14:32  hpadidakul
 * Fixing the build issue on Solaris and AIX, issue is regresstion of bug 22914 checkin.
 * Unit tested by: Hari
 *
 * Revision 1.18  2012/05/25 07:04:47  hpadidakul
 * 21610: Changes to support SLES11-SP2-32/64 UA.
 *
 * Unit test by: Hari
 *
 * Revision 1.17  2011/01/13 06:27:51  chvirani
 * 14878: Changes to support AIX.
 *
 * Revision 1.16  2010/10/19 04:52:02  chvirani
 * 14174: Raw device no longer required
 *
 * Revision 1.15  2010/10/01 09:05:00  chvirani
 * 13869: Full disk support
 *
 * Revision 1.14  2010/07/22 05:47:35  chvirani
 * define list_first_entry if not already defined
 *
 * Revision 1.13  2010/07/05 07:13:07  chvirani
 * 12934: Allocate bio_data only for I/O being processed.
 *             Limit max parallel I/O for reduced mem usage.
 *
 * Revision 1.12  2010/04/30 10:40:33  hpadidakul
 * Changes is to support RHEL5.5, to use write_begin & write_end operations.
 *
 * Revision 1.11  2010/03/10 12:21:40  chvirani
 * Multithreaded driver - linux
 *
 * Revision 1.10  2010/02/08 10:52:34  hpadidakul
 * RHEL5U4 support, found a bug where vsnap creaction can fail on other Filesystem.
 * Code reviewed by: Chirag.
 * Unit test done by: Harikrishnan
 *
 * Revision 1.9  2010/01/23 06:16:13  hpadidakul
 * The code changes is to support vsnap driver on RHEL 5U4.
 * Fix is to use _begin & write_end function for doing write opration.
 *
 * Smokianide test done by: Brahamaprakash.
 *
 * Revision 1.8  2009/08/19 11:56:13  chvirani
 * Use splice/page_cache calls to support newer kernels
 *
 * Revision 1.7  2009/05/19 13:59:14  chvirani
 * Change memset() to memzero() to support Solaris 9
 *
 * Revision 1.6  2009/04/28 12:54:49  cswarna
 * These files were changed during vsnap driver code re-org
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

#define INM_PAGE_SIZE 	PAGE_SIZE

/* File open flags */
#define INM_RDONLY 	O_RDONLY
#define INM_RDWR 	O_RDWR
#define INM_CREAT 	O_CREAT
#define INM_LARGEFILE	O_LARGEFILE
#define INM_LYR		0

#define CALC_DEV_T(ma, mi)	(mi & 0xff) | (ma << 8) | ((mi & ~0xff) << 12)

/* IPC related structure & functions declarations */
typedef struct semaphore		inm_mutex_t;
#if LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,35)
#define INM_INIT_MUTEX(m)		init_MUTEX(m)
#define INM_INIT_MUTEX_LOCKED(m) 	init_MUTEX_LOCKED(m);
#else
#define INM_INIT_MUTEX(m)               sema_init((struct semaphore*)m, 1)
#define INM_INIT_MUTEX_LOCKED(m)        sema_init((struct semaphore*)m, 0)
#endif
#define INM_ACQ_MUTEX(m)	 	down(m)
#define INM_REL_MUTEX(m)	 	up(m)
#define INM_ACQ_MUTEX_INTERRUPTIBLE(m)	down_interruptible(m);

typedef struct rw_semaphore		inm_rwlock_t;
#define INM_INIT_LOCK(x)		init_rwsem(x)
#define INM_READ_LOCK(x)		down_read(x)
#define INM_READ_UNLOCK(x)		up_read(x)
#define INM_WRITE_LOCK(x)		down_write(x)
#define INM_WRITE_UNLOCK(x)		up_write(x)

typedef spinlock_t				inm_spinlock_t;
#define INM_INIT_SPINLOCK(m)			spin_lock_init(m);
#define INM_ACQ_SPINLOCK(m)			spin_lock(m)
#define INM_REL_SPINLOCK(m)			spin_unlock(m)
#define INM_ACQ_SPINLOCK_IRQ(m, x)		spin_lock_irq(m)
#define INM_REL_SPINLOCK_IRQ(m, x)		spin_unlock_irq(m)
#define INM_ACQ_SPINLOCK_IRQSAVE(m, f)		spin_lock_irqsave(m,f)
#define INM_REL_SPINLOCK_IRQRESTORE(m, f)	spin_unlock_irqrestore(m,f)

typedef atomic_t			inm_atomic_t;
#define INM_ATOMIC_SET(var, val)        atomic_set(var, val)
#define INM_ATOMIC_INC(var)             atomic_inc(var)
#define INM_ATOMIC_DEC(var)             atomic_dec(var)
#define INM_ATOMIC_READ(var)            atomic_read(var)
#define INM_ATOMIC_DEC_AND_TEST(var)    atomic_dec_and_test(var)
#define INM_ATOMIC_INC_AND_TEST(var)    atomic_inc_and_test(var)
#if ((LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,5)))
#define INM_ATOMIC_DEC_RETURN(var, ret)	atomic_dec(var) \
					ret = atomic_read(var)
#else
#define INM_ATOMIC_DEC_RETURN(var, ret)	(ret = atomic_dec_return(var))
#endif

typedef inm_mutex_t			inm_ipc_t;
#define INM_INIT_IPC(is)		INM_INIT_MUTEX(is)
#define INM_INIT_SLEEP(is)		INM_ACQ_MUTEX(is)
#define INM_SLEEP(is)			INM_ACQ_MUTEX(is)
#define INM_FINI_SLEEP(is)		INM_REL_MUTEX(is)
#define INM_WAKEUP(is)			INM_REL_MUTEX(is)

/*
 * Bit Ops
 */
#define INM_SET_BIT(bit, flags)		(*(flags) = *(flags) | (bit))
#define INM_TEST_BIT(bit, flags)	((*(flags) & bit ) == (bit))
#define INM_CLEAR_BIT(bit, flags)	(*(flags) = *(flags) & ~(bit))

#define INM_VDEV_NAME(vdev)		(vdev)->vd_disk->disk_name

/* Platform specific definitions */

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,27)
typedef struct request_queue request_queue_t;
#endif

#define OS_RELEASE_VERSION(a,b) (((a) << 8) + (b))

/* On RHEL 5.4 and 5.5, we have address_space_operations_ext to incorporate
 * write_begin & write_end operation, similar to 2.6.24 and above kernels.
 */
#if (defined(RHEL_MAJOR) && (RHEL_MAJOR == 5) &&  \
     (OS_RELEASE_VERSION(RHEL_MAJOR, RHEL_MINOR)>= OS_RELEASE_VERSION(5, 4)))
typedef const struct address_space_operations_ext inm_address_space_operations_t;
#else
typedef const struct address_space_operations inm_address_space_operations_t;
#endif

typedef struct bio		inm_block_io_list_t;
typedef struct bio		inm_block_io_t;
typedef struct gendisk		inm_disk_t;
typedef request_queue_t		inm_queue_t;
typedef loff_t			inm_offset_t;
typedef unsigned long		inm_ioctl_arg_t;
typedef inm_32_t		inm_major_t;
typedef inm_32_t		inm_minor_t;
typedef struct page		inm_page_t;
typedef dev_t			inm_dev_t;
typedef unsigned long		inm_interrupt_t;

#define memzero(ptr, size)	memset(ptr, 0, size)

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
	memcpy_s((DEST), sizeof(TYPE), (SRC), sizeof(TYPE));
#else
#define INM_ASSIGN(DEST, SRC, TYPE) \
	*((TYPE *)(DEST)) = *(TYPE *)(SRC);
#endif


/* string related fns declarations */
#define STRING_PRINTF(x)	snprintf x;
void STR_COPY(char *dest, char *src, inm_32_t len);
void STRING_CAT(char *str1, inm_32_t dlen, char *str2, inm_32_t len);

/* FILE OPS */
inm_32_t STRING_MEM(char *);
inm_32_t INM_OPEN_FILE(const char *, inm_u32_t, void **);
inm_32_t INM_READ_FILE(void *, void *, inm_u64_t, inm_u32_t, inm_u32_t *);
inm_32_t inm_vdev_read_page(void *, inm_vdev_page_data_t *);
inm_32_t INM_WRITE_FILE(void *, void *, inm_u64_t, inm_u32_t, inm_u32_t *);
inm_32_t inm_vdev_write_page(void *, inm_vdev_page_data_t *);
inm_32_t INM_SEEK_FILE_END(void *, inm_u64_t *);
void INM_CLOSE_FILE(void *, inm_32_t);

/*
 * Copying to and from user space
 * and page to virtual addresss
 */
void inm_copy_buf_to_page(inm_page_t *page, void *buf, inm_u32_t len);
void inm_copy_page_to_buf(void *buf, inm_page_t *page, inm_u32_t len);
inm_32_t inm_copy_from_user(void *kerptr, void __user *userptr, size_t size);
inm_32_t inm_copy_to_user(void __user *userptr, void *kerptr, size_t size);
void inm_bzero_page(inm_page_t *page, inm_u32_t offset, inm_u32_t len);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,8,13)
#define	INM_KMAP_ATOMIC(page, idx)	kmap_atomic(page)
#define	INM_KUNMAP_ATOMIC(vaddr, idx)	kunmap_atomic(vaddr)
#else
#define	INM_KMAP_ATOMIC(page, idx)	kmap_atomic(page, idx)
#define	INM_KUNMAP_ATOMIC(vaddr, idx)	kunmap_atomic(vaddr, idx)
#endif

#endif /* _INMAGE_VSNAP_KERNEL_HELPERS_H */
