 
/*****************************************************************************
 * vv_control.h
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
 * Description:         Declaration of volpack control routines
 *
 * Revision:            $Revision: 1.3 $
 * Checked in by:       $Author: hpadidakul $
 * Last Modified:       $Date: 2010/03/10 15:15:54 $
 *
 * $Log: vv_control.h,v $
 * Revision 1.3  2010/03/10 15:15:54  hpadidakul
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
 * Revision 1.2  2009/07/03 07:35:38  chvirani
 * 8920: zfs support and proc error reporting
 *
 * Revision 1.1  2009/04/28 12:36:16  cswarna
 * Adding these files as part of vsnap driver code re-org
 *
 * Revision 1.4  2008/03/25 12:59:52  praveenkr
 * code cleanup for Solaris
 *
 * Revision 1.3  2007/09/14 13:11:16  sudhakarp
 * Merge from DR_SCOUT_4-0_DEV/DR_SCOUT_4-1_BETA2 to HEAD ( Onbehalf of RP).
 *
 * Revision 1.1.2.2  2007/08/21 15:25:12  praveenkr
 * added more comments
 *
 * Revision 1.1.2.1  2007/07/24 12:58:30  praveenkr
 * virtual volume (volpack) implementation
 *
 *****************************************************************************/

#ifndef _VV_CONTROL_H_
#define _VV_CONTROL_H_

#define INM_VOLPACK_PATH 	"/dev/volpack/dsk/"

#define INM_VOLPACK_NAME_PREFIX "/dev/volpack/dsk/volpack"
#define INM_VOLPACK_NAME_SIZE	( MAXPATHLEN - 1 )

#define INM_VOLPACK_MINOR_START	( L_MAXMIN32 >> 1 )
#define INM_MAX_VV		( L_MAXMIN32 >> 1 )

/* Solaris specific */
#define INM_SOL_VV_SOFT_STATE_IDX(minor)	\
				( (minor) - INM_VOLPACK_MINOR_START )

#define INM_IS_VSNAP_DEVICE(minor)	( minor < INM_VOLPACK_MINOR_START ) 

#endif /* _VV_CONTROL_H_ */
