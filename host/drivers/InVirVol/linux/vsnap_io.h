
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
 * Revision:            $Revision: 1.9 $
 * Checked in by:       $Author: chvirani $
 * Last Modified:       $Date: 2010/10/01 09:05:00 $
 *
 * $Log: vsnap_io.h,v $
 * Revision 1.9  2010/10/01 09:05:00  chvirani
 * 13869: Full disk support
 *
 * Revision 1.8  2010/06/21 11:25:01  chvirani
 * 10967: Handle requests beyond device boundary on solaris
 *
 * Revision 1.7  2010/03/31 13:24:01  chvirani
 * o X: Convert disk offsets to inm_offset_t
 *
 * Revision 1.6  2010/03/10 12:21:40  chvirani
 * Multithreaded driver - linux
 *
 * Revision 1.5  2009/04/28 12:54:49  cswarna
 * These files were changed during vsnap driver code re-org
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

#ifndef _INMAGE_VSNAP_IO_H
#define _INMAGE_VSNAP_IO_H

#include "vsnap_io_common.h"

#define INM_HANDLE_END_OF_DEVICE(vdev, bio)
#define INM_DEVICE_MAX_OFFSET(vdev, bio)	INM_VDEV_SIZE(vdev)

/* Macros to access bio fields */
#define BIO_TYPE(BIO, RW)	*RW = bio_rw(bio) == WRITE ? WRITE : READ;
#define BIO_PRIVATE(bio)	(bio)->bi_private
#define BIO_SECTOR(bio)		(bio)->bi_sector
#define BIO_OFFSET(bio)		((inm_offset_t)((bio)->bi_sector) << 9)
#define BIO_SIZE(bio)		(bio)->bi_size
#define BIO_NEXT(bio)		(bio)->bi_next

inm_32_t inm_vs_make_request(request_queue_t *, struct bio *);
inm_32_t inm_vv_make_request(request_queue_t *, struct bio *);

#endif
