
/*****************************************************************************
 * vdev_proc.c
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
 * Revision:            $Revision: 1.10 $
 * Checked in by:       $Author: praweenk $
 * Last Modified:       $Date: 2014/10/10 08:04:08 $
 *
 * $Log: vdev_proc.c,v $
 * Revision 1.10  2014/10/10 08:04:08  praweenk
 * Bug ID:917800
 * Description: Safe API changes for filter and VSNAP driver cross platform.
 *
 * Revision 1.9  2011/01/13 06:27:51  chvirani
 * 14878: Changes to support AIX.
 *
 * Revision 1.8  2011/01/11 05:43:27  chvirani
 * 14377: Changes to support linux kernel 2.6.32 upwards
 *
 * Revision 1.7  2010/06/17 12:46:33  chvirani
 * 12543: Sparse - Print additional sparse related info.
 *
 * Revision 1.6  2010/06/09 10:10:04  chvirani
 * Support for sparse retention
 *
 * Revision 1.5  2010/03/10 12:21:40  chvirani
 * Multithreaded driver - linux
 *
 * Revision 1.4  2009/05/19 13:59:14  chvirani
 * Change memset() to memzero() to support Solaris 9
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

#include "common.h"
#include "vsnap.h"
#include "vsnap_kernel.h"
#include "VsnapShared.h"

/* to facilitate ident */
static const char rcsid[] = "$Id:";

/* global & static variables */
struct proc_dir_entry *inm_vdev_dir;			/* invirvol directory */
static struct proc_dir_entry *inm_vdev_proc_global;	/* global entry */

/*
 * proc_read_global
 *
 * DESC
 *	to process the read requests for all the /proc/invirvol/global
 */
static inm_32_t
proc_read_global(char *page, char **start, off_t off, inm_32_t count,
		 inm_32_t *eof, void *data)
{
	return proc_read_global_common(page);
}


/*
 * proc_read_context
 *
 * DESC
 *	common fn to process all the read requests for all the
 *	/proc/invirvol/~/context
 */
static inm_32_t
proc_read_context(char *page, char **start, off_t off, inm_32_t count,
		  inm_32_t *eof, void *data)
{
	inm_vdev_t *vdev = data;

	if ( INM_TEST_BIT(VSNAP_DEVICE, &vdev->vd_flags) )
		return proc_read_vs_context_common(page, vdev);
	else
		return proc_read_vv_context_common(page, vdev);
}

/*
 * list of common/mandatory proc entries
 */
static proc_list_t proc_list[] = {
	{"global", proc_read_global},
	{"context", proc_read_context},
};

/*
 * inm_vdev_init_proc
 *
 * DESC
 *	initialize the proc interface
 */
inm_32_t
inm_vdev_init_proc(void)
{
	inm_32_t retval = 0;

	/*
	 * create a directory under the proc interface
	 */
	DEV_DBG("creating the base dir");
	inm_vdev_dir = CREATE_PROC_DIR(INM_VDEV_PROC_MODULE_NAME, NULL);
	if (!inm_vdev_dir) {
		retval = -ENOMEM;
		goto out;
	}

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,32)
	inm_vdev_dir->owner = THIS_MODULE;
#endif

	/*
	 * create a global entry only for debug driver
	 */
	DEV_DBG("creating global proc-entry");
	inm_vdev_proc_global = CREATE_RO_PROC_ENTRY(GLOBAL, inm_vdev_dir, NULL);
	if (!inm_vdev_proc_global) {
		retval = -ENOMEM;
		goto global_fail;
	}

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,32)
	inm_vdev_proc_global->owner = THIS_MODULE;
#endif

	retval = inm_vdev_init_proc_common();
	if( retval < 0 ) {
		ERR("Cannot initialise proc");
		goto proc_init_failed;
	}

  out:
	return retval;

  proc_init_failed:
	DELETE_PROC_ENTRY(GLOBAL, inm_vdev_dir);

  global_fail:
	remove_proc_entry(INM_VDEV_PROC_MODULE_NAME, NULL);
	goto out;
}

/*
 * inm_vdev_exit_proc
 *
 * DESC
 *	clean up the proc interface
 */
void
inm_vdev_exit_proc(void)
{
	inm_vdev_exit_proc_common();
	DEV_DBG("deleting the global entry");
	DELETE_PROC_ENTRY(GLOBAL, inm_vdev_dir);
	DEV_DBG("deleting the invirvol dir");
	remove_proc_entry(INM_VDEV_PROC_MODULE_NAME, NULL);
}

/*
 * inm_vdev_create_proc_entry
 *
 * DESC
 *	create a proc entry of the given name
 */
inm_32_t
inm_vdev_create_proc_entry(char *name, inm_vdev_t *vdev)
{
	char			*temp_name = name;
	inm_32_t		retval = 0;
	inm_vdev_proc_entry_t	*temp_entry;

	DEV_DBG("allocating space for proc entry");
	temp_entry = INM_ALLOC(sizeof(inm_vdev_proc_entry_t), GFP_KERNEL);
	if (!temp_entry) {
		ERR("no memeory");
		retval = -ENOMEM;
		goto out;
	}

	temp_name += 3;		/* bcoz dev name is vs/??? */
	temp_entry->pe_name = INM_ALLOC(strlen(temp_name) + 1, GFP_KERNEL);
	if (!temp_entry->pe_name) {
		ERR("no mem for str[%s]", temp_name);
		retval=-ENOMEM;
		goto pe_name_failed;
	}

	if (strcpy_s(temp_entry->pe_name, strlen(temp_name) + 1, temp_name)) {
		retval = -EFAULT;
		goto pe_dir_failed;
	}

	DEV_DBG("pe_name - %s", temp_entry->pe_name);

	DEV_DBG("creating a proc dir with %s name", temp_entry->pe_name);
	temp_entry->pe_dir = CREATE_PROC_DIR(temp_entry->pe_name, inm_vdev_dir);
	if (!temp_entry->pe_dir) {
		ERR("no mem while allocating for %s", name);
		retval = -ENOMEM;
		goto pe_dir_failed;
	}

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,32)
	temp_entry->pe_dir->owner = THIS_MODULE;
#endif

	DEV_DBG("creating proc entry for context");
	temp_entry->pe_context = CREATE_RO_PROC_ENTRY(CONTEXT,
						      temp_entry->pe_dir,
						      vdev);
	if (!temp_entry->pe_context) {
		ERR("no mem while allocating for context");
		retval = -ENOMEM;
		goto pe_context_failed;
	}

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,32)
	temp_entry->pe_context->owner = THIS_MODULE;
#endif

	vdev->vd_proc_entry = temp_entry;

  out:
	return retval;

  pe_context_failed:
	remove_proc_entry(temp_entry->pe_name, inm_vdev_dir);

  pe_dir_failed:
	INM_FREE(&temp_entry->pe_name, STRING_MEM(temp_entry->pe_name));

  pe_name_failed:
	INM_FREE(&temp_entry, sizeof(inm_vdev_proc_entry_t));
	goto out;
}

/*
 * inm_vdev_del_proc_entry
 *
 * DESC
 *	remove the proc entry
 */
void
inm_vdev_del_proc_entry(inm_vdev_t *vdev)
{
	inm_vdev_proc_entry_t   *temp_entry;

	temp_entry = vdev->vd_proc_entry;
	DEV_DBG("removing proc entry - context");
	DELETE_PROC_ENTRY(CONTEXT, temp_entry->pe_dir);
	DEV_DBG("removing proc dir with name - %s", temp_entry->pe_name);
	remove_proc_entry(temp_entry->pe_name, inm_vdev_dir);
	DEV_DBG("freeing %s", temp_entry->pe_name);
	INM_FREE(&temp_entry->pe_name, STRING_MEM(temp_entry->pe_name));
	DEV_DBG("freeing temp_entry");
	INM_FREE(&temp_entry, sizeof(inm_vdev_proc_entry_t));
}
