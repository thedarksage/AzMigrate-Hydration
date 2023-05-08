
/*****************************************************************************
 * vdev_proc.h
 *
 * Copyright (c) 2007 InMage
 * This file contains proprietary and confidential information and remains
 * the unpublished property of InMage. Use, disclosure, or reproduction is
 * prohibited except as permitted by express written license agreement with
 * Inmage
 *
 * Author:              InMage Systems
 *                      http://www.inmage.net
 * Creation date:       Sat, Jul 13, 2008
 * Description:         proc interface routines
 *
 * Revision:            $Revision: 1.7 $
 * Checked in by:       $Author: lamuppan $
 * Last Modified:       $Date: 2014/03/14 12:49:28 $
 *
 * $Log: vdev_proc.h,v $
 * Revision 1.7  2014/03/14 12:49:28  lamuppan
 * Changes for lists, kernel helpers.
 *
 * Revision 1.6  2011/01/13 06:27:51  chvirani
 * 14878: Changes to support AIX.
 *
 * Revision 1.5  2010/03/10 12:21:40  chvirani
 * Multithreaded driver - linux
 *
 * Revision 1.4  2009/07/03 07:36:45  chvirani
 * 8920: Support for additional error logging in solaris
 *
 * Revision 1.3  2009/05/04 13:09:08  chvirani
 * suppot for FORCE DELETE ioctl in stats
 *
 * Revision 1.2  2009/04/28 12:54:49  cswarna
 * These files were changed during vsnap driver code re-org
 *
 * Revision 1.1  2008/08/12 22:27:23  praveenkr
 * fix for #5404: Decoupled the mount point away from creation of vsnap
 * Moved the decision on the device name away from driver into the CX
 * & 'cdpcli'
 *
 *****************************************************************************/

#ifndef _VDEV_PROC_H_
#define _VDEV_PROC_H_

#define _ENABLE_PROC_

#include <linux/proc_fs.h>
#include "common.h"
#include "vdev_proc_common.h"

// #include "vsnap.h"

/* extern data types */
extern struct proc_dir_entry	*inm_vdev_dir;

/*
 * /proc/invirvol directory
 */
#define INM_VDEV_PROC_MODULE_NAME	"invirvol"

/*
 * This structure is used to maintain the list of proc entries associated with
 * this device.
 *
 * Members:
 *	pe_name			name of the directory entry
 *	pe_dir			proc structure for this directory
 *	pe_context		proc structure for the context
 */
struct inm_vdev_proc_entry_tag {
	char			*pe_name;
	struct proc_dir_entry	*pe_dir;
	struct proc_dir_entry	*pe_context;
};
typedef struct inm_vdev_proc_entry_tag inm_vdev_proc_entry_t;

/*
 * list of available entries and its related functions
 */
typedef struct proc_list_entries_tag {
	char			*ple_name;
	int32_t			(*ple_read_fn)(char *, char **, off_t, int32_t,
					       int32_t *, void *);
}proc_list_t;

/*
 * list of available indices
 */
#define GLOBAL			0
#define CONTEXT			1

/*
 * abstracted out proc interface calls
 */
#define PROC_DEFAULT_ACL		444
#define CREATE_PROC_DIR(name, parent_dir)				\
	proc_mkdir(name, parent_dir)
#define CREATE_RO_PROC_ENTRY(n, parent_dir, data)			\
	create_proc_read_entry((proc_list[n]).ple_name,			\
			       PROC_DEFAULT_ACL, parent_dir,		\
			       (proc_list[n]).ple_read_fn, data)
#define DELETE_PROC_ENTRY(n, parent_dir)			       	\
	remove_proc_entry((proc_list[n]).ple_name, parent_dir)

#endif /* _VDEV_PROC_H_ */
