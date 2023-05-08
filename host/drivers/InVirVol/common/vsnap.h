
/*****************************************************************************
 * vsnap.h
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
 * Description:         Vsnap device related declarations
 *
 * Revision:            $Revision: 1.8 $
 * Checked in by:       $Author: chvirani $
 * Last Modified:       $Date: 2011/01/13 06:34:52 $
 *
 * $Log: vsnap.h,v $
 * Revision 1.8  2011/01/13 06:34:52  chvirani
 * 14878: Changes to support AIX.
 *
 * Revision 1.7  2010/10/01 09:03:22  chvirani
 * 13869: Full disk support
 *
 * Revision 1.6  2010/06/21 11:21:10  chvirani
 * 10967: Handle boundary conditions on solaris raw device
 *
 * Revision 1.5  2010/03/31 13:23:11  chvirani
 * o X: Convert disk offsets to inm_offset_t
 * o X: Increase/Decrease mem usage count inside lock
 *
 * Revision 1.4  2010/03/10 12:08:43  chvirani
 * Multithreaded Driver
 *
 * Revision 1.3  2009/07/03 06:52:04  chvirani
 * 8920: ZFS support and error reporting in solaris proc
 *
 * Revision 1.2  2009/05/04 13:00:41  chvirani
 * EFI label support for solaris
 *
 * Revision 1.1  2009/04/28 12:34:38  cswarna
 * Adding these files as part of vsnap driver code re-org
 *
 * Revision 1.5  2008/08/12 22:27:23  praveenkr
 * fix for #5404: Decoupled the mount point away from creation of vsnap
 * Moved the decision on the device name away from driver into the CX
 * & 'cdpcli'
 *
 * Revision 1.4  2008/03/25 12:59:52  praveenkr
 * code cleanup for Solaris
 *
 * Revision 1.3  2007/09/14 13:11:16  sudhakarp
 * Merge from DR_SCOUT_4-0_DEV/DR_SCOUT_4-1_BETA2 to HEAD ( Onbehalf of RP).
 *
 * Revision 1.1.2.8  2007/08/21 15:25:12  praveenkr
 * added more comments
 *
 * Revision 1.1.2.7  2007/07/24 12:58:30  praveenkr
 * virtual volume (volpack) implementation
 *
 * Revision 1.1.2.6  2007/07/17 13:53:25  praveenkr
 * clean-up - unix-style naming, tab-spacing = 8, 78 col
 *
 * Revision 1.1.2.5  2007/06/29 20:07:44  praveenkr
 * one more fix for 3531: target os crash (suse 10)
 * included a flag to avoid racing between open and delete vsnaps
 *
 * Revision 1.1.2.4  2007/06/29 17:24:45  praveenkr
 * fix for 3531 - target os crash (suse 10)
 * removed the usage & reference of struct block_device from within
 * the vsnap_device_t structure.
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

#ifndef _INMAGE_LVSNAP_H
#define _INMAGE_LVSNAP_H

#include "common.h"
#include "vsnap_kernel_helpers.h"
#include "vdev_proc_common.h"

/* vsnap controller is registered as misc device to save a major # */
#define INM_VDEV_CONTROLLER	"vsnapcontrol"

/* names the block devices will acquired, followed by minor # */
#define INM_VSNAP_BLKDEV         "vsnap"
#define INM_VV_BLKDEV            "volpack"

/*
 * this structure is the set of function pointers that handle the rw ops
 * corresponding to the policy defined for that device
 */
struct inm_vdev_rw_ops {
	inm_32_t (*read)(void *, inm_offset_t, inm_u32_t, void *);
	inm_32_t (*write)(inm_vdev_page_data_t, void *);
};

/*
 * virtual device structure - represents a virtual device
 * this structure is a hook up into the kernel and is common for both vsnap
 * devices and volpack devices
 */
typedef struct inm_vdev_tag {
	inm_minor_t		vd_index;       /* Contains the minor */
	unsigned long		vd_flags;	/* unsigned long used to be
						   compatible with test_bit */
	inm_spinlock_t		vd_lock;
	inm_block_io_list_t	*vd_bio;
	inm_block_io_list_t	*vd_biotail;
	inm_ipc_t		vd_sem;		/* used while creating/deleting
						   worker thread */
	inm_atomic_t		vd_io_pending;
	inm_32_t		vd_refcnt;
	inm_u64_t		vd_size;
	inm_queue_t		*vd_request_queue;
						/* request queue for IO reqs */
	inm_disk_t		*vd_disk;
	void			*vd_private;	/* holds the info structures */
	struct inm_vdev_rw_ops	vd_rw_ops;
	inm_vdev_proc_entry_t	*vd_proc_entry;
	inm_atomic_t		vd_io_inflight;
	inm_u32_t		vd_io_complete;
	inm_32_t		vd_write_in_progress;
} inm_vdev_t;

/* MACROs defined to be used with vd_flags */
#define VSNAP_FLAGS_READ_ONLY		1
#define VSNAP_FLAGS_FREEING		2	/* used to indicate that
						   the vsnap is tearing down */
#define VSNAP_DEVICE			4
#define VV_DEVICE			8

/*
 * These are not used for linux
 * We define it here so that we do not have
 * multiple definitions for the same flag
 */
#define VDEV_FULL_DISK			32
#define VDEV_HAS_VTOC			64
#define VDEV_HAS_EFI			128
#define VDEV_HAS_MBR			256
#define VDEV_HAS_PARTITIONS		512
#define VDEV_BLK_OPEN			1024
#define VDEV_CHR_OPEN			2048
#define VDEV_LYR_OPEN			4096

#define INM_VDEV_SIZE(vdev)		(vdev)->vd_size

#endif /* _INMAGE_LVSNAP_H */
