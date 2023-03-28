
/*****************************************************************************
 * vsnap_io.h
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
 * Description:         IO related routines declarations
 *
 * Revision:            $Revision: 1.7 $
 * Checked in by:       $Author: lamuppan $
 * Last Modified:       $Date: 2015/02/24 03:29:23 $
 *
 * $Log: vsnap_io_common.h,v $
 * Revision 1.7  2015/02/24 03:29:23  lamuppan
 * Bug #1619650, 1619648, 1619642, 1619640, 1619637, 1619636, 1619635: Fix for policheck
 * bugs for Filter and Vsnap drivers. Please refer to bugs in TFS for more details.
 *
 * Revision 1.6  2011/01/13 06:34:52  chvirani
 * 14878: Changes to support AIX.
 *
 * Revision 1.5  2010/07/05 07:13:07  chvirani
 * 12934: Allocate bio_data only for I/O being processed.
 *             Limit max parallel I/O for reduced mem usage.
 *
 * Revision 1.4  2010/06/21 11:21:10  chvirani
 * 10967: Handle boundary conditions on solaris raw device
 *
 * Revision 1.3  2010/03/31 13:23:11  chvirani
 * o X: Convert disk offsets to inm_offset_t
 * o X: Increase/Decrease mem usage count inside lock
 *
 * Revision 1.2  2010/03/26 13:01:49  chvirani
 * Move bio_data flag defines to common file
 *
 * Revision 1.1  2010/03/10 12:09:33  chvirani
 * initial code for multithreaded driver
 *
 * Revision 1.7  2010/02/25 13:28:12  chvirani
 * Pass bio_data->bid_flags to bio_endio() for use in solaris
 *
 * Revision 1.6  2010/02/24 10:36:21  chvirani
 * convert linux specific calls to generic calls
 *
 * Revision 1.5  2010/02/16 13:41:51  chvirani
 * start queued io within the thread instead of a separate task
 *
 * Revision 1.4  2010/02/16 13:09:55  chvirani
 * working phase 1 driver - fix for read suspension forever, write suspension forever, DI
 *
 * Revision 1.3  2010/01/13 14:52:49  chvirani
 * working phase 1 - fixed random io bug
 *
 * Revision 1.2  2009/12/14 10:48:27  chvirani
 * go through FS cache
 *
 * Revision 1.1  2009/12/04 10:47:55  chvirani
 * phase 1 - final
 *
 * Revision 1.2  2009/12/04 10:22:34  chvirani
 * common threading functions
 *
 * Revision 1.1  2009/11/18 13:38:05  chvirani
 * multithreaded
 *
 * Revision 1.1  2009/07/28 08:34:50  chvirani
 * Working code for multithreaded io
 *
 * Revision 1.1  2008/12/30 18:23:00  chvirani
 * Multithreaded pool changes, caching and other improvements - unfinished
 *
 * Revision 1.4  2008/03/25 12:59:52  praveenkr
 * code cleanup for Solaris
 *
 * Revision 1.3  2007/09/14 13:11:16  sudhakarp
 * Merge from DR_SCOUT_4-0_DEV/DR_SCOUT_4-1_BETA2 to HEAD ( Onbehalf of RP).
 *
 * Revision 1.1.2.7  2007/07/24 12:58:30  praveenkr
 * virtual volume (volpack) implementation
 *
 * Revision 1.1.2.6  2007/07/17 13:53:25  praveenkr
 * clean-up - unix-style naming, tab-spacing = 8, 78 col
 *
 * Revision 1.1.2.5  2007/05/25 09:45:44  praveenkr
 * added comments & partial clean-up of code
 *
 * Revision 1.1.2.4  2007/04/24 12:49:46  praveenkr
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

#ifndef _INMAGE_VSNAP_IO_COMMON_H
#define _INMAGE_VSNAP_IO_COMMON_H

#include "common.h"
#include "vsnap.h"
#include "vsnap_control.h"
#include "vsnap_kernel.h"
#include "vdev_thread_common.h"

#define INM_MAX_ACTIVE_BIO	INM_MAX_NUM_THREADS * 10
#define INM_MIN_ACTIVE_BIO	INM_MAX_NUM_THREADS * 8
#define INM_INFLATED_INFLIGHT	INM_MAX_ACTIVE_BIO + INM_MIN_ACTIVE_BIO + 1


typedef struct lookup_ret_data {
	single_list_entry_t	lrd_ret_data;
	inm_u32_t		lrd_target_read;
} lookup_ret_data_t;

#define LOOKUP_RET_DATA_INIT(lrd) do {					       \
					INM_LIST_INIT(&((lrd)->lrd_ret_data)); \
					(lrd)->lrd_target_read = 0;	       \
				  } while(0)

typedef struct bio_data {
	lookup_ret_data_t	bid_ret_data;
	inm_atomic_t		bid_status;	/* Rudimentary Thread Sync
						   o Increment on target bio
						     completion and decrement
						     after vsnap_lookup. On
						     zero, start retention io
						   o Number of retention reads.
						     Decrement on completion of
						     each retention i/o.
						     On zero, call endio */
	inm_vdev_t		*bid_vdev;	/* vdev of the vsnap */
	vsnap_task_t		*bid_task;	/* points to the task associated
						   with bio */
	inm_u32_t		bid_flags;
	inm_32_t		bid_error;	/* error code */

	inm_u64_t		bid_size;	/* Size we can fulfill */

	/* Store for original bio data */
	void 			*bid_bi_private;/* Original private data */
} bio_data_t;

// Flags to bid_flags
#define INM_BUF_REMAPPED	1

typedef struct ret_io {
	inm_vdev_page_data_t	ret_pd;
	inm_block_io_t		*ret_bio;
	inm_u32_t		ret_fid;
} ret_io_t;

void handle_bio_for_read_from_vsnap(vsnap_task_t *);
void inm_vs_read_from_retention_file(vsnap_task_t *);
void inm_vv_do_volpack_io(vsnap_task_t *);
void inm_vdev_endio(vsnap_task_t *);
inm_32_t inm_vs_generic_make_request(inm_vdev_t *, inm_block_io_t *);
inm_32_t inm_vv_generic_make_request(inm_vdev_t *, inm_block_io_t *);
void handle_bio_for_vsnap_write_common(vsnap_task_t *);

/* These functions should be defined for each platform */
void inm_bio_endio(inm_block_io_t *, inm_32_t, bio_data_t *);
void inm_vs_read_from_retention(vsnap_task_t *);
inm_32_t read_from_target_volume(vsnap_task_t *);
inm_32_t handle_bio_for_write_to_vv(inm_vdev_t *,inm_block_io_t *,inm_offset_t);
inm_32_t handle_bio_for_read_from_vv(inm_vdev_t *,inm_block_io_t *,inm_offset_t);
inm_32_t handle_bio_for_write_to_vsnap(vsnap_task_t *);
inm_32_t read_from_this_fileid(inm_block_io_t *, bio_data_t *, ret_io_t *);

#ifdef _VSNAP_DEBUG_
void io_print_info(void);
#endif

#endif /* _INMAGE_VSNAP_IO_COMMON_H */
