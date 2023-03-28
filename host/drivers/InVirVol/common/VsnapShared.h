
/*****************************************************************************
 * VsnapShared.h
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
 * Description:         Data structures shared b/w User-space & Kernel module
 *
 * Revision:            $Revision: 1.7 $
 * Checked in by:       $Author: hpadidakul $
 * Last Modified:       $Date: 2013/07/04 10:59:09 $
 *
 * $Log: VsnapShared.h,v $
 * Revision 1.7  2013/07/04 10:59:09  hpadidakul
 * 25475: invalid ioclt number.
 *
 * fix is to use _IOWR of sys/ioclt.h
 *
 * Unit tested by: Harikrishnan on Linux and Solaris.
 *
 * Revision 1.6  2011/01/13 06:34:52  chvirani
 * 14878: Changes to support AIX.
 *
 * Revision 1.5  2011/01/11 05:42:35  chvirani
 * 14377: Changes to support linux kernel 2.6.32 upwards
 *
 * Revision 1.4  2010/10/01 09:03:22  chvirani
 * 13869: Full disk support
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
 * Revision 1.5  2008/03/26 07:21:35  praveenkr
 * fix for 5130 - 4.2.0 and 4.1.0 VX build failure on all Linux platforms
 *
 * Revision 1.4  2008/03/25 12:59:52  praveenkr
 * code cleanup for Solaris
 *
 * Revision 1.3  2007/09/14 13:11:16  sudhakarp
 * Merge from DR_SCOUT_4-0_DEV/DR_SCOUT_4-1_BETA2 to HEAD ( Onbehalf of RP).
 *
 * Revision 1.1.2.6  2007/07/24 12:58:30  praveenkr
 * virtual volume (volpack) implementation
 *
 * Revision 1.1.2.5  2007/05/25 09:45:43  praveenkr
 * added comments & partial clean-up of code
 *
 * Revision 1.1.2.4  2007/05/23 15:03:04  praveenkr
 * Fix for 3240, 3245, 3266:
 * implemented IOCTL_INMAGE_GET_VOLUME_CONTEXT to retrieve context for any
 * vsnap
 *
 * Revision 1.1.2.3  2007/05/16 14:45:50  praveenkr
 * Fix for 2984:
 * IOCTLs to retrieve the context info of all vsnaps for a retention path
 *
 * Revision 1.1.2.2  2007/04/17 15:36:30  praveenkr
 * #2944: 4.0_rhl4target-target is crashed when writting the data to R/W vsnap.
 *
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
 * Revision 1.1.2.5  2007/04/05 13:53:22  kush
 * BUG 2925 - Inserted a new IOCTL to verify the virtual volume
 *
 *****************************************************************************/

#ifndef __VSNAPSHARED_H__
#define __VSNAPSHARED_H__
#define INM_VDEV_IOCTL_MAGIC	0xfc

#ifndef SV_WINDOWS
#if defined (SV_LINUX) || defined (_LINUX_)
#include <linux/version.h>
/* For Linux we are using _IOWR and third argument "char[8]" is just to make _IOWR & compiler happy
 * better approach is to have structure for each ioctl */
#define INM_IOCTL_RW_CODE(x)      _IOWR(INM_VDEV_IOCTL_MAGIC, (x), char[8])
#define INM_IOCTL_W_CODE(x)       _IOW(INM_VDEV_IOCTL_MAGIC, (x), char[8])
#define INM_IOCTL_R_CODE(x)       _IOR(INM_VDEV_IOCTL_MAGIC, (x), char[8])
#else
#define INM_IOCTL_RW_CODE(x)      (INM_VDEV_IOCTL_MAGIC << 8 | (x))
#define INM_IOCTL_W_CODE(x)       (INM_VDEV_IOCTL_MAGIC << 8 | (x))
#define INM_IOCTL_R_CODE(x)       (INM_VDEV_IOCTL_MAGIC << 8 | (x))
#endif


#define IOCTL_INMAGE_ADD_VIRTUAL_VOLUME_FROM_RETENTION_LOG            INM_IOCTL_RW_CODE(1)
#define IOCTL_INMAGE_REMOVE_VIRTUAL_VOLUME_FROM_RETENTION_LOG         INM_IOCTL_R_CODE(2)
#define IOCTL_INMAGE_GET_VOLUME_SYMLINK_FOR_RETENTION_POINT           INM_IOCTL_R_CODE(3)
#define IOCTL_INMAGE_GET_VOLUME_DEVICE_ENTRY                          INM_IOCTL_R_CODE(4)
#define IOCTL_INMAGE_UPDATE_VSNAP_VOLUME                              INM_IOCTL_R_CODE(5)
#define IOCTL_INMAGE_GET_VOL_CONTEXT                                  INM_IOCTL_RW_CODE(6)
#define IOCTL_INMAGE_GET_VOLUME_INFORMATION                           INM_IOCTL_RW_CODE(7)
#define IOCTL_INMAGE_GET_VOLUME_CONTEXT                               INM_IOCTL_RW_CODE(8)
#define IOCTL_INMAGE_ADD_VIRTUAL_VOLUME_FROM_FILE                     INM_IOCTL_RW_CODE(9)
#define IOCTL_INMAGE_REMOVE_VIRTUAL_VOLUME_FROM_FILE                  INM_IOCTL_R_CODE(10)
#define IOCTL_INMAGE_REMOVE_VIRTUAL_VOLUME_FROM_RETENTION_LOG_NOFAIL  INM_IOCTL_R_CODE(11)
#define IOCTL_INMAGE_DUMP_DATA					      INM_IOCTL_RW_CODE(254)
#define IOCTL_DUMP_VSNAP_DEBUG_DATA				      INM_IOCTL_RW_CODE(255)
#endif

/* Flags  used in IOCTL driver */
#define INM_VSNAP_DELETE_CANFAIL    0x1

typedef struct _UPDATE_VSNAP_VOLUME_INPUT {
	int64_t		VolOffset;
	int32_t		Length;
	int32_t		Cow;
	int64_t		FileOffset;
	char		DataFileName[50];
	char		ParentVolumeName[2048];
} UPDATE_VSNAP_VOLUME_INPUT, *PUPDATE_VSNAP_VOLUME_INPUT;

/*
 * this file is shared between user space & kernel space.
 * the user space uses this file for the sturct - UPDATE_VSNAP_VOLUME_INPUT
 * so redefining for our convenience
 */
typedef UPDATE_VSNAP_VOLUME_INPUT update_input_t;

#endif
