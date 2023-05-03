
/*****************************************************************************
 * vsnap_init.c
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
 * Description:         Init routines' definition for Vsnap
 *
 * Revision:            $Revision: 1.11 $
 * Checked in by:       $Author: lamuppan $
 * Last Modified:       $Date: 2015/02/24 03:29:23 $
 *
 * $Log: vsnap_init.c,v $
 * Revision 1.11  2015/02/24 03:29:23  lamuppan
 * Bug #1619650, 1619648, 1619642, 1619640, 1619637, 1619636, 1619635: Fix for policheck
 * bugs for Filter and Vsnap drivers. Please refer to bugs in TFS for more details.
 *
 * Revision 1.10  2012/05/25 07:04:46  hpadidakul
 * 21610: Changes to support SLES11-SP2-32/64 UA.
 *
 * Unit test by: Hari
 *
 * Revision 1.9  2010/03/10 12:21:40  chvirani
 * Multithreaded driver - linux
 *
 * Revision 1.8  2009/08/19 11:56:13  chvirani
 * Use splice/page_cache calls to support newer kernels
 *
 * Revision 1.7  2009/04/28 12:54:49  cswarna
 * These files were changed during vsnap driver code re-org
 *
 * Revision 1.6  2008/08/12 22:27:23  praveenkr
 * fix for #5404: Decoupled the mount point away from creation of vsnap
 * Moved the decision on the device name away from driver into the CX
 * & 'cdpcli'
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
 * Revision 1.1.2.10  2007/08/21 15:18:20  praveenkr
 * 1. replaced all kfrees with a wrapper dr_vdev_kfree, which will force null
 *    the ptr, to easily capture mem leaks/mem corruption
 * 2. added comments
 *
 * Revision 1.1.2.9  2007/08/02 07:40:55  praveenkr
 * fix for 3829 - volpack is also listed when listing vsnaps from cdpcli
 * maintaining a seperate list for volpacks
 *
 * Revision 1.1.2.8  2007/07/26 13:11:58  praveenkr
 * inserted a version tag for the driver using date
 *
 * Revision 1.1.2.7  2007/07/24 12:58:30  praveenkr
 * virtual volume (volpack) implementation
 *
 * Revision 1.1.2.6  2007/07/17 13:53:25  praveenkr
 * clean-up - unix-style naming, tab-spacing = 8, 78 col
 *
 * Revision 1.1.2.5  2007/07/12 11:39:21  praveenkr
 * removed devfs usage; fix for RH5 build failure
 *
 * Revision 1.1.2.4  2007/06/27 09:26:04  praveenkr
 * fix for 3531 - target OS crash
 *     - added locks on minor_num_list and UniqueIDList to handle re-entrancy.
 *     - added more debug prints for better debugging during crash
 *
 * Revision 1.1.2.3  2007/05/25 09:45:44  praveenkr
 * added comments & partial clean-up of code
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

#include "linux/miscdevice.h"
#include "common.h"
#include "vsnap_kernel.h"
#include "vdev_helper.h"
#include "vsnap.h"
#include "vsnap_ioctl.h"
#include "vdev_thread_common.h"

/* to facilitate ident */
static const char rcsid[] = "$Id: vsnap_init.c,v 1.11 2015/02/24 03:29:23 lamuppan Exp $";

extern inm_major_t inm_vs_major;
extern inm_major_t inm_vv_major;

#if LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,35)
inm_32_t
inm_vdev_ioctl(struct inode *inode, struct file *file, inm_u32_t command,
	       unsigned long arg)
#else
inm_32_t
inm_vdev_ioctl(struct file *file, inm_u32_t command, unsigned long arg)
#endif

{
	return inm_vdev_ioctl_handler((inm_32_t)command, arg);
}

/* vsnap controller file ops */
static struct file_operations inm_vdev_fops = {
	.owner = THIS_MODULE,
#if LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,35)
	.ioctl = inm_vdev_ioctl,
#else
	.unlocked_ioctl = inm_vdev_ioctl,
#endif
};

/* vsnap controller is registered as misc device to save a major # */
#define INM_VDEV_CONTROLLER	"vsnapcontrol"
static struct miscdevice inm_vdev_misc_device = {
	.minor =	MISC_DYNAMIC_MINOR,
	.name =		INM_VDEV_CONTROLLER,
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,18)
	.devfs_name =	INM_VDEV_CONTROLLER,
#endif
	.fops =		&inm_vdev_fops
};

/**
 * inm_vdev_driver_init
 *
 * DESC
 *      initialize the vdev driver
 *
 * ALGO
 *      1. register a major # for vsnap dev & volpack dev respectively
 *      2. initialize the global structures thro' inm_vs_init
 *      3. register the control device as a misc device
 *      4. initialize the list head, semaphore & lock for vsnap devices and
 *          volpack devices respectively
 **/
inm_32_t __init
inm_vdev_driver_init(void)
{
	inm_32_t retval = 0;

	/*
	 * initialize the proc interface very early bcoz we are tracking
	 * the memory usage for debug builds
	 */
	DEV_DBG("initialize vdev data structures");
	retval = inm_vdev_init();
	if ( retval != 0 ) {
		ERR("vdev init failed");
		goto out;
	}

	/*
	 * register a major # for vsnap devices
	 */
	DEV_DBG("register a major number for vsnap devices");
	inm_vs_major = register_blkdev(0, INM_VSNAP_BLKDEV);
	if (inm_vs_major <= 0) {
		ERR("register_blkdev failed.");
		retval = -EIO;
		goto vs_register_failed;
	}
	DBG("vsnap major # - %d", inm_vs_major);

	/*
	 * register a major # for volpack devices
	 */
	DEV_DBG("register a major for volpak devices");
	inm_vv_major = register_blkdev(0, INM_VV_BLKDEV);
	if (inm_vv_major <= 0) {
		ERR("register_blkdev failed for volpack");
		retval = -EIO;
		goto vv_register_failed;
	}
	DBG("volpack major # - %d", inm_vv_major);

	/*
	 * register the control device "vsnapcontroller" as a misc device
	 * so as to preserve a major #
	 */
	DEV_DBG("register a vdev controller as misc device");
	if (misc_register(&inm_vdev_misc_device)) {
		ERR("misc_register failed.");
		retval = -EIO;
		goto misc_register_failed;
	}

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,18)
	devfs_mk_dir(INM_VSNAP_BLKDEV);
#endif

	retval = init_thread_pool();
	if ( retval < 0 ) {
		ERR("Cannot initialise thread pool");
		goto tpool_init_failed;
	}

	INFO("successfully loaded module.Built on " \
	     __DATE__ " [ "__TIME__ " ]");

  out:
	return retval;

  tpool_init_failed:
	misc_deregister(&inm_vdev_misc_device);
	
  misc_register_failed:
	unregister_blkdev(inm_vv_major, INM_VV_BLKDEV);

  vv_register_failed:
	unregister_blkdev(inm_vs_major, INM_VSNAP_BLKDEV);

  vs_register_failed:
	inm_vdev_exit();
	goto out;
}

/**
 * inm_vdev_driver_exit
 *
 * DESC
 *      clean up
 **/
void __exit
inm_vdev_driver_exit(void)
{
	uninit_thread_pool();
	
	inm_vdev_exit();
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,18)
	devfs_remove(INM_VSNAP_BLKDEV);
#endif
	if (misc_deregister(&inm_vdev_misc_device) < 0)
		ERR("misc_deregister failed.");

	unregister_blkdev(inm_vv_major, INM_VV_BLKDEV);
	unregister_blkdev(inm_vs_major,INM_VSNAP_BLKDEV);
	
	INFO("successfully unloaded vsnap module");
}

module_init(inm_vdev_driver_init);
module_exit(inm_vdev_driver_exit);

/* set module info */
MODULE_AUTHOR("InMage Systems Pvt Ltd");
MODULE_DESCRIPTION("InMage virtual volume module");
MODULE_LICENSE("Proprietary");
MODULE_VERSION(__DATE__ " [ " __TIME__ " ]");
