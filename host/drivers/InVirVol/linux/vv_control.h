 
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
 * Revision:            $Revision: 1.5 $
 * Checked in by:       $Author: cswarna $
 * Last Modified:       $Date: 2009/04/28 12:54:49 $
 *
 * $Log: vv_control.h,v $
 * Revision 1.5  2009/04/28 12:54:49  cswarna
 * These files were changed during vsnap driver code re-org
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

#define INM_VOLPACK_NAME_PREFIX "/dev/volpack"
#define INM_VOLPACK_NAME_SIZE	15

#define INM_VOLPACK_MINOR_START	0
#define INM_MAX_VV		999

#endif /* _VV_CONTROL_H_ */
