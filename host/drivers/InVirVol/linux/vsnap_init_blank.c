
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
 * Revision:            $Revision: 1.1 $
 * Checked in by:       $Author: chvirani $
 * Last Modified:       $Date: 2016/03/30 11:26:46 $
 *
 * $Log: vsnap_init_blank.c,v $
 * Revision 1.1  2016/03/30 11:26:46  chvirani
 * 6727007: Change vsnap driver into a blank driver
 *
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

#include <linux/module.h>

#undef INFO
#define INFO(format, arg...) 						\
	printk(KERN_INFO "VDEV[%s:%d]:(INF) " format "\n",		\
		__FUNCTION__, __LINE__, ## arg)

int __init
inm_vdev_driver_init(void)
{
	int retval = 0;

	INFO("successfully loaded module.Built on " \
	     __DATE__ " [ "__TIME__ " ]");

	return retval;
}

void __exit
inm_vdev_driver_exit(void)
{
	INFO("successfully unloaded vsnap module");
}

module_init(inm_vdev_driver_init);
module_exit(inm_vdev_driver_exit);

/* set module info */
MODULE_AUTHOR("InMage Systems Pvt Ltd");
MODULE_DESCRIPTION("InMage virtual volume module");
MODULE_LICENSE("Proprietary");
MODULE_VERSION(__DATE__ " [ " __TIME__ " ]");
