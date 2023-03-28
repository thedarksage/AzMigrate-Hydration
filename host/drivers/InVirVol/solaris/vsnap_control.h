
/*****************************************************************************
 * vsnap_control.h
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
 * Description:         Declaration of Vsnap control routines
 *
 * Revision:            $Revision: 1.3 $
 * Checked in by:       $Author: chvirani $
 * Last Modified:       $Date: 2010/10/01 10:06:58 $
 *
 * $Log: vsnap_control.h,v $
 * Revision 1.3  2010/10/01 10:06:58  chvirani
 * 13869: Full disk support
 *
 * Revision 1.2  2010/03/10 15:15:54  hpadidakul
 *
 * Vsnap Solaris changes porting from Linux.
 * Code reviewed by: Chirag.
 *
 * Smokianide test done by: Sahoo.
 *
 * Revision 1.1  2009/12/04 10:47:56  chvirani
 * phase 1 - final
 *
 * Revision 1.1  2009/11/18 13:38:06  chvirani
 * multithreaded
 *
 * Revision 1.1  2009/04/28 12:36:15  cswarna
 * Adding these files as part of vsnap driver code re-org
 *
 * Revision 1.6  2008/12/24 14:06:20  hpadidakul
 *
 * Change to the Vsnap driver as a part of fix.
 *
 * (1) New IOCLT introduced "IOCTL_INMAGE_REMOVE_VIRTUAL_VOLUME_FROM_RETENTION_LOG_NOFAIL  11"
 *
 * (2) New routine inm_delete_vsnap_bydevid() added.
 *
 * (3) delete_vsnapdev() is changed to delete_vsnap_byname().
 *
 * (4) ref count is assigned to one at creation of vsnap(inm_vs_init_dev()).
 *
 *
 * (5) VSNAP_FLAGS_FREEING check added at routine inm_vdev_make_request(), to fail IO's if device is stale.
 *
 * (6) ref count check added at routine inm_vs_blk_close(), which will call inm_delete_vsnap_bydevid() if ref count is one.
 *
 * (7) INM_VSNAP_DELETE_CANFAIL flag added.
 *
 * Modified files: VsnapShared.h  vsnap_control.c vsnap_control.h vsnap_io.c vsnap_ioctl.c vsnap_open.c.
 *
 * Reviewed by: Sridhara and Chirag.
 *
 * Smokianide test done by: Janardhana Reddy Kota.
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
 * Revision 1.1.2.5  2007/07/24 12:58:30  praveenkr
 * virtual volume (volpack) implementation
 *
 * Revision 1.1.2.4  2007/07/17 13:53:25  praveenkr
 * clean-up - unix-style naming, tab-spacing = 8, 78 col
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

#ifndef _INMAGE_VSNAP_CONTROL_H
#define _INMAGE_VSNAP_CONTROL_H

#include "common.h"
#include "vsnap.h"
#include "VsnapShared.h"
#include "vsnap_common.h"

/* Minor Macros */
#define INM_VSNAP_MINOR_START	0
#define INM_VSNAP_NUM_DEVICES	( NDKMAP + FD_NUMPART + 1 + 1 )
#define INM_MAX_VSNAP		(( L_MAXMIN32 >> 1 ) / INM_VSNAP_NUM_DEVICES )

/* Solaris specific */
#define INM_SOL_VS_SOFT_STATE_IDX(minor)	( minor )

#endif /* _INMAGE_VSNAP_CONTROL_H */
