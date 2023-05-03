
/*****************************************************************************
 * vv_control.c
 *
 * Copyright (c) 2007 InMage
 * This file contains proprietary and confidential information and remains
 * the unpublished property of InMage. Use, disclosure, or reproduction is
 * prohibited except as permitted by express written license agreement with
 * Inmage
 *
 * Author:              InMage Systems
 *                      http://www.inmage.net
 * Creation date:       Thu, Jul 19, 2007
 * Description:         virtual volume (volpack) related files
 *
 * Revision:            $Revision: 1.19 $
 * Checked in by:       $Author: lamuppan $
 * Last Modified:       $Date: 2015/02/24 03:29:23 $
 *
 * $Log: vv_control.c,v $
 * Revision 1.19  2015/02/24 03:29:23  lamuppan
 * Bug #1619650, 1619648, 1619642, 1619640, 1619637, 1619636, 1619635: Fix for policheck
 * bugs for Filter and Vsnap drivers. Please refer to bugs in TFS for more details.
 *
 * Revision 1.18  2014/11/17 12:59:48  lamuppan
 * TFS Bug #1057085: Fixed the issue by fixing the STR_COPY function. Please
 * go through the bug for more details.
 *
 * Revision 1.17  2014/10/10 08:04:10  praweenk
 * Bug ID:917800
 * Description: Safe API changes for filter and VSNAP driver cross platform.
 *
 * Revision 1.16  2011/01/13 06:27:51  chvirani
 * 14878: Changes to support AIX.
 *
 * Revision 1.15  2010/10/01 09:05:00  chvirani
 * 13869: Full disk support
 *
 * Revision 1.14  2010/06/21 11:25:01  chvirani
 * 10967: Handle requests beyond device boundary on solaris
 *
 * Revision 1.13  2010/03/10 12:21:40  chvirani
 * Multithreaded driver - linux
 *
 * Revision 1.12  2009/11/18 15:10:51  hpadidakul
 * Bug: 10244
 * Fix for volpack device and to correct typo.
 * Review By:  Chirag.
 * Unit tested By: Hari
 *
 * Revision 1.11  2009/11/18 07:55:12  hpadidakul
 * Bug: 10244
 * Linux Fix for thread create.
 * Code review by: Chirag Virani
 * Unit test done by: Hari
 *
 * Revision 1.10  2009/08/19 11:56:13  chvirani
 * Use splice/page_cache calls to support newer kernels
 *
 * Revision 1.9  2009/05/19 13:59:14  chvirani
 * Change memset() to memzero() to support Solaris 9
 *
 * Revision 1.8  2009/04/28 12:54:49  cswarna
 * These files were changed during vsnap driver code re-org
 *
 * Revision 1.7  2008/09/23 13:13:42  chvirani
 * fix for 6506 - 	RSLite was suspended forever while taking the vsnaps for
 * 		the pair configured with Solaris being initiator
 *
 * Revision 1.6  2008/08/12 22:27:23  praveenkr
 * fix for #5404: Decoupled the mount point away from creation of vsnap
 * Moved the decision on the device name away from driver into the CX
 * & 'cdpcli'
 *
 * Revision 1.5  2008/03/25 12:59:52  praveenkr
 * code cleanup for Solaris
 *
 * Revision 1.4  2008/01/28 11:10:17  praveenkr
 * fixed possible memory corruption while NULL ending strings
 *
 * Revision 1.3  2007/09/14 13:11:16  sudhakarp
 * Merge from DR_SCOUT_4-0_DEV/DR_SCOUT_4-1_BETA2 to HEAD ( Onbehalf of RP).
 *
 * Revision 1.1.2.7  2007/08/21 15:19:38  praveenkr
 * 1. replaced all kfrees with a wrapper dr_vdev_kfree, which will force null
 *    the ptr, to easily capture mem leaks/mem corruption
 * 2. freeing vv_dev->sparse_file_path
 * 3. added comments
 *
 * Revision 1.1.2.6  2007/08/02 07:40:55  praveenkr
 * fix for 3829 - volpack is also listed when listing vsnaps from cdpcli
 * maintaining a seperate list for volpacks
 *
 * Revision 1.1.2.5  2007/07/31 07:32:49  praveenkr
 * fix for 3817 - Unmounting success for non existent volpack device
 * error handling
 *
 * Revision 1.1.2.4  2007/07/26 07:21:43  praveenkr
 * potential bug - replaced the err from 0 to 1 in dr_vv_vol_write
 *
 * Revision 1.1.2.3  2007/07/26 07:15:52  praveenkr
 * fix for 3785: volpack failed in 64bit platform
 * replaced the usage of size_t to int32_t
 *
 * Revision 1.1.2.2  2007/07/25 07:30:48  praveenkr
 * Rh5 build breakage fix
 *
 * Revision 1.1.2.1  2007/07/24 12:58:30  praveenkr
 * virtual volume (volpack) implementation
 *
 *****************************************************************************/

#include "vsnap.h"
#include "vsnap_common.h"
#include "vdev_control_common.h"
#include "vsnap_kernel.h"
#include "vsnap_io.h"
#include "vsnap_open.h"

extern struct inm_vdev_rw_ops vv_rw_ops;

/* vsnap block device ops */
struct block_device_operations inm_vv_blk_fops = {
        .open = inm_vv_blk_open,
        .release = inm_vv_blk_close,
        .owner = THIS_MODULE,
};

/*
 * vv_alloc_dev
 *
 * DESC
 *      allocate & intialize a volpack device struct
 *
 * ALGO
 *      1. allocate a free minor # or, if the minor # is give, then insert
 *          the minor # number list
 *      2. allocate a volpack device struct & init IPCs
 *      3. allocate a gendisk and initialize its fields
 *      4. init the priv data field of gendisk to our volpack dev struct
 */
inm_32_t
vv_alloc_dev(inm_vdev_t **vvdev, char *dev_path)
{
	inm_32_t	minor = -1;
	inm_32_t	retval = 0;
	inm_vdev_t	*vdev = NULL;

	__module_get(THIS_MODULE);

	/*
	 * if the path was provided then the minor number will be extracted
	 * out of the path else will be allocated a new one
	 */
	if (dev_path[0]) {
		/*
		 * ToDo: since "/dev/volpack" is a fixed prefix;
		 * a work around
		 */
		DBG("dev path given - %s", dev_path);
		minor =  inm_atoi(dev_path + 12);
		retval = insert_into_minor_list(minor);
		if (retval < 0) {
			ERR("the minor number %d already available", minor);
			goto fail;
		}
	} else {
		if ((minor = get_free_vv_minor()) < 0) {
			ERR("not able to alloc minor number");
			retval = minor;		/* error value super imposed
						   on the return value */
			goto fail;
		}

		/*
		 * TODO: the devices can be located anywhere, so cannot
		 * hard-code this, i suppose!
		 */
		sprintf(dev_path, "/dev/volpack%d", minor);
	}

	/*
	 * initialize the vdev fields
	 */
	if (!(vdev = INM_ALLOC(sizeof(inm_vdev_t), GFP_KERNEL))) {
		ERR("no mem");
		retval = -ENOMEM;
		goto vdev_alloc_failed;
	}
	memzero(vdev, sizeof(inm_vdev_t));

	/*
	 * this index stores the minor #.
	 * also the worker thread that will be created will also hold this
	 * minor # as its reference. in simple words, the worker thread for
	 * volpack0 is named 'lvolpack0' and so on.
	 */
	vdev->vd_index = minor;
	INM_INIT_SPINLOCK(&vdev->vd_lock);
	INM_INIT_IPC(&vdev->vd_sem);

	/* Platform dependent code starts here */
	/*
	 * create the request queue to which the bios are queued in form
	 * of requests.
	 * for generic devices, that use the generic_make_request fn, the bios
	 * are queued in this request_queue and the low-level-driver yanks the
	 * the request out of this queue and process it. but  we dont use this
	 * request queue anywhere in our processing. we compensate by providing
	 * our own make_request function but the protocol expects us to create
	 * this request function nevertheless
	 */
	DEV_DBG("allocate a request queue");
	if (!(vdev->vd_request_queue = blk_alloc_queue(GFP_KERNEL))) {
		ERR("blk alloc que failed");
		retval = -ENOMEM;
		goto alloc_q_failed;
	}

	/*
	 * initialize the gendisk & its field. Also add the disk to kernel
	 * this disk does not contain partitions and hence the input for
	 * alloc_disk is INM_NO_PARTITIONS (= 1)
	 */
	vdev->vd_disk = alloc_disk(INM_VSNAP_NUM_DEVICES);
	if (!vdev->vd_disk) {
		ERR("alloc_disk failed.");
		retval = -ENOMEM;
		goto alloc_disk_failed;
	}
	vdev->vd_disk->major = inm_vv_major;
	vdev->vd_disk->first_minor = minor;
	vdev->vd_disk->fops = &inm_vv_blk_fops;
	sprintf(vdev->vd_disk->disk_name, "volpack%d", minor);
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,18)
	sprintf(vdev->vd_disk->devfs_name, "volpack%d", minor);
#endif
	vdev->vd_disk->private_data = vdev;
	vdev->vd_disk->queue = vdev->vd_request_queue;

	/*
	 * add the disk into the kernel data structures
	 */
	DEV_DBG("adding the disk - volpack%d", vdev->vd_disk->first_minor);
	add_disk(vdev->vd_disk);

	/*
	 * return the partially filled vdev;
	 * rest will be filled inside inm_vv_init
	 * ToDo: move this vdev allocation to inm_vv_init
	 */
	*vvdev = vdev;

  out:
	return retval;

  alloc_disk_failed:
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24)
	blk_put_queue(vdev->vd_request_queue);
#else
	blk_cleanup_queue(vdev->vd_request_queue);
#endif

  alloc_q_failed:
	INM_FREE(&vdev, sizeof(inm_vdev_t));

  vdev_alloc_failed:
	put_vv_minor(minor);	 /* return value is useless so not captured */

  fail:
	module_put(THIS_MODULE);
	*vvdev = NULL;
	goto out;
}

/*
 * vv_init_dev
 *
 * DESC
 *      initialze the volpack device & volpack info
 *
 * ALGO
 *      1. keep the sparse file open
 *      2. register our make request fn
 *      3. set the size for the gendisk struct
 *      4. create a kernel thread which is a worker thread to handle the i/o
 */
inm_32_t
vv_init_dev(inm_vdev_t *vdev, void *filpv, char *file_name,
	    char *dev_path)
{
	loff_t		size = 0;
	inm_32_t	retval = 0;
	struct file	*filp = (struct file *)filpv;
	struct inode	*temp = filp->f_mapping->host;
	inm_vv_info_t	*vv_info = NULL;

	vv_info = INM_ALLOC(sizeof(inm_vv_info_t), GFP_KERNEL);
	if (!vv_info) {
		ERR("no mem.");
		retval = -ENOMEM;
		goto fail;
	}
	vv_info->vvi_sparse_file_filp = filp;
	vv_info->vvi_sparse_file_path = INM_ALLOC(MAX_FILENAME_LENGTH,
						  GFP_KERNEL);
	if (!vv_info->vvi_sparse_file_path) {
		ERR("no mem");
		retval = -ENOMEM;
		goto file_name_alloc_failed;
	}
	memzero(vv_info->vvi_sparse_file_path, MAX_FILENAME_LENGTH);

    /* vvi_sparse_file_path can be of non-null terminated, so careful
     * in using it further.
     */
	if (memcpy_s(vv_info->vvi_sparse_file_path, MAX_FILENAME_LENGTH,
				file_name, strlen(file_name))) {
		retval = -EFAULT;
		INM_FREE(&vv_info->vvi_sparse_file_path, MAX_FILENAME_LENGTH);
		goto file_name_alloc_failed;
	}

	vdev->vd_private = vv_info;

	/*
	 * initialize the vdev structure
	 */
	vdev->vd_bio = vdev->vd_biotail = NULL;
	DEV_DBG("define vv's own make request fn");
	blk_queue_make_request(vdev->vd_request_queue, inm_vv_make_request);
	vdev->vd_request_queue->queuedata = vdev;
	size = temp->i_size >> 9; /* in sectors */
	DEV_DBG("set the capacity of disk to %lld sectors", size);
	set_capacity(vdev->vd_disk, (sector_t)size);

	INM_VDEV_SIZE(vdev) = size << 9;

	vdev->vd_rw_ops = vv_rw_ops;

  out:
	return retval;

  file_name_alloc_failed:
	INM_FREE(&vv_info, sizeof(inm_vv_info_t));

  fail:
	vdev->vd_private = NULL;
	goto out;
}

/*
 * vv_free_dev
 *
 * DESC
 *      release the allocated volpack device struct
 */
inm_32_t
vv_free_dev(inm_vdev_t *vdev)
{
	inm_32_t retval = 0;

	retval = put_vv_minor(vdev->vd_disk->first_minor);
	if (retval < 0) {
		ERR("put minor failed");
		goto out;
	}

	DEV_DBG("delete the gendisk structure");
	del_gendisk(vdev->vd_disk);
	vdev->vd_disk->private_data = NULL;
	DEV_DBG("put/kill the gendisk instance");
	put_disk(vdev->vd_disk);
	DEV_DBG("put/kill the request queue instance");
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24)
	blk_put_queue(vdev->vd_request_queue);
#else
	blk_cleanup_queue(vdev->vd_request_queue);
#endif
	DEV_DBG("put/release the module");
	module_put(THIS_MODULE);
	INM_FREE(&vdev, sizeof(inm_vdev_t));

  out:
	return retval;
}


/*
 * vv_uninit_dev
 *
 * DESC
 *      release the volpack device
 */
inm_32_t
vv_uninit_dev(inm_vdev_t *vdev)
{
	inm_32_t	retval = 0;
	inm_vv_info_t	*vv_info = vdev->vd_private;
	inm_u32_t pending = 0;

	ASSERT(!vv_info);
	INM_ACQ_SPINLOCK(&inm_vv_minor_list_lock);
	if (vdev->vd_refcnt > 0) {
		INM_REL_SPINLOCK(&inm_vv_minor_list_lock);
		ERR("minor - %d; refcnt - %d", vdev->vd_index,
		    vdev->vd_refcnt);
		retval = -EBUSY;
		goto out;
	}

	/*
	 * set flag to indicate the device is going down.
	 * this flag is tested when the open is attempted on this device.
	 * this avoids the racing condition between open & delete
	 */
	INM_SET_BIT(VSNAP_FLAGS_FREEING, &vdev->vd_flags);
	INM_REL_SPINLOCK(&inm_vv_minor_list_lock);

	/*
	 * wait till the io_pending is zero, i.e., no io request is pending
	 * to be serviced by the device. when no io_pending but if io_mutex
	 * is upped then its tear down
	 */
	INM_INIT_SLEEP(&vdev->vd_sem);
       	INM_ACQ_SPINLOCK_IRQ(&vdev->vd_lock, 0);
	pending = INM_ATOMIC_READ(&vdev->vd_io_pending);
        INM_REL_SPINLOCK_IRQ(&vdev->vd_lock, 0);

	if( pending )
		INM_SLEEP(&vdev->vd_sem);

	INM_FINI_SLEEP(&vdev->vd_sem);

	DEV_DBG("close the sparse file");
	INM_CLOSE_FILE(vv_info->vvi_sparse_file_filp, INM_RDWR);
	INM_FREE(&vv_info->vvi_sparse_file_path, MAX_FILENAME_LENGTH);
	INM_FREE(&vv_info, sizeof(inm_vv_info_t));
        vdev->vd_private = NULL;

out:
        return retval; /* It should never fail */
}
