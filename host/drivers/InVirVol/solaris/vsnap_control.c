#include "vsnap.h"
#include "vsnap_common.h"
#include "vdev_control_common.h"
#include "vsnap_kernel.h"
#include "vsnap_io.h"
#include "vdev_disk_helpers.h"

extern void *vs_listp;
extern dev_info_t *vdev_dip;
extern struct inm_vdev_rw_ops vs_rw_ops;
extern inm_u32_t module_refcount;

/*
 * inm_vs_alloc_dev
 *
 * DESC
 *      allocate & intialize a vsnap device struct
 *
 * ALGO
 *      1. allocate a free minore #
 *      2. allocate a vsnap device struct & init IPCs
 *      3. allocate a gendisk and initialize its fields
 *      4. init the priv data field of gendisk to our vsnap dev struct
 */

inm_32_t
inm_vs_alloc_dev(inm_vdev_t 	**vdev,
		 inm_minor_t 	*vminor,
		 char 		*dev_path)
{
	char		*temp_dev_path = NULL;
	inm_32_t	retval = 0;
	inm_32_t	minor = -1;
	inm_vdev_t	*temp_dev = NULL;
	inm_dev_t	dev;
	inm_vdev_t	**vdev_soft_state;

	DEV_DBG("reserve the next available minor #");
	minor = get_free_vs_minor();	/* never fails */
	if (minor < 0) {
		ERR("no mem.");
		retval = minor;		/* error super imposed on minor */
		goto no_minor_available;
	}
	*vminor = minor;

	temp_dev = INM_ALLOC(sizeof(inm_vdev_t), GFP_KERNEL);
	if (!temp_dev) {
		ERR("no mem.");
		retval = -ENOMEM;
		goto vdev_alloc_failed;
	}
	memzero(temp_dev, sizeof(inm_vdev_t));
	*vdev = temp_dev;

	/*
	 * this index stores the minor #.
	 * also the worker thread that will be created will also hold this
	 * minor # as its reference. in simple words, the worker thread for
	 * the device with minor number 0 is named 'lvsnap0' and so on.
	 */
	temp_dev->vd_index = minor;
	INM_INIT_SPINLOCK(&temp_dev->vd_lock);
	INM_INIT_IPC(&temp_dev->vd_sem);

	/* Platform dependent code starts here */
	/*
	 * Allocate the disk structure
	 */
	DEV_DBG("allocate a new disk instance");
	temp_dev->vd_disk = INM_ALLOC(sizeof(inm_disk_t), GFP_KERNEL);
	if ( !temp_dev->vd_disk ) {
		ERR("alloc_disk failed.");
		retval = -ENOMEM;
		goto alloc_disk_failed;
	}

	retval = inm_alloc_disk_info(temp_dev);
	if ( retval < 0 ) {
		ERR("Alloc disk info failed");
		goto alloc_disk_info_failed;
	}

	DEV_DBG("allocating the device name");
	temp_dev_path = INM_ALLOC(strlen(dev_path) + 1, GFP_KERNEL);
	if (!temp_dev_path) {
		ERR("no mem");
		retval = -ENOMEM;
		goto temp_dev_path_failed;
	}
	memzero(temp_dev_path, strlen(dev_path) + 1);

	if (memcpy_s(temp_dev_path, strlen(dev_path), dev_path, strlen(dev_path))) {
		retval = -EFAULT;
		INM_FREE(&temp_dev_path, STRING_MEM(temp_dev_path));
		goto temp_dev_path_failed;
	}
	temp_dev_path[strlen(dev_path)] = '\0';
	/*
         * we are restricted upto LEN_DKL_VVOL chars for vsnap name
         */
	snprintf(INM_VDEV_NAME(temp_dev), INM_DISK_NAME_SIZE, "%s",
		 temp_dev_path + strlen("/dev/vs/dsk/"));
	INM_FREE(&temp_dev_path, STRING_MEM(temp_dev_path));

	/*
	 * store the vdev in soft state
	 */

	retval = ddi_soft_state_zalloc(vs_listp,
					INM_SOL_VS_SOFT_STATE_IDX(minor));
	if ( retval == DDI_FAILURE ) {
		ERR("Soft state zalloc failed");
		retval = -ENOMEM;
		goto soft_state_zalloc_failed;
	}

	vdev_soft_state = (inm_vdev_t **)ddi_get_soft_state(vs_listp,
					INM_SOL_VS_SOFT_STATE_IDX(minor));
	if ( !vdev_soft_state ) {
		ERR("Get soft state failed");
		retval = -EFAULT;
		goto get_soft_state_failed;
	}

	*vdev_soft_state = temp_dev;

	/*
	 * return the partially filled vdev;
	 * rest will be filled inside inm_vs_init
	 * ToDo: move this vdev allocation to inm_vs_init
	 */


out:
	return retval;	/* if success '0' falls through */

get_soft_state_failed:
	DEV_DBG("ddi_soft_state_free");
	ddi_soft_state_free(vs_listp, INM_SOL_VS_SOFT_STATE_IDX(minor));

soft_state_zalloc_failed:
temp_dev_path_failed:
	inm_free_disk_info(temp_dev);

alloc_disk_info_failed:
	INM_FREE(&temp_dev->vd_disk, sizeof(inm_disk_t));

alloc_disk_failed:
	INM_FREE(&temp_dev, sizeof(inm_vdev_t));

vdev_alloc_failed:
	put_vs_minor(minor);	/* return value is useless so not captured */

no_minor_available:
	*vdev = NULL;
	goto out;
}

/*
 * print_mdata
 *
 * DESC
 *	used for debugging purpose only (DEBUG_DEV)
 */
static void
print_mdata(inm_vs_mdata_t *mdata)
{
	DBG("ret path	- %s", mdata->md_retention_path);
	DBG("data log path	- %s", mdata->md_data_log_path);
	DBG("mount path	- %s", mdata->md_mnt_path);
	DBG("parent		- %s", mdata->md_parent_dev_path);
	DBG("parent volsize	- %lld", mdata->md_parent_vol_size);
	DBG("par sect size	- %d", mdata->md_parent_sect_size);
	DBG("snapshot id	- %lld", mdata->md_snapshot_id);
	DBG("root offset 	- %lld", mdata->md_bt_root_offset);
	DBG("next_fid		- %d", mdata->md_next_fid);
	DBG("recovery time	- %lld", mdata->md_recovery_time);
	DBG("access type	- %d", mdata->md_access_type);
	DBG("in ret path	- %d", mdata->md_is_map_in_retention_path);
	DBG("tracking	- %d", mdata->md_is_tracking_enabled);
	DBG("Sparse - %d", (inm_32_t)mdata->md_sparse);
	DBG("Flag = %d", (inm_32_t)mdata->md_flag);
}

/*
 * inm_vs_init_dev
 *
 * DESC
 *      initialize a vsnap device
 *
 * ALGO
 *      1. validate i/p val & populate structs (inm_vs_init_vsnap_info)
 *      2. populate the vdev struct details using parent vol details
 *         (like size, sector size, etc...)
 *      3. register a make_request fn for this dev
 *      4. set the size of the associated gen_disk
 *      5. create a kernel worker thread to process ios
 */
inm_32_t
inm_vs_init_dev(inm_vdev_t *vdev, inm_vs_mdata_t *mdata)
{
	inm_offset_t	size = 0;
	inm_32_t	retval = 0;
	inm_vs_info_t	*vs_info = NULL;
	inm_offset_t	sector_aligned_size;
	char 		namebuf[VDEV_NAME_SIZE];
	inm_minor_t	minor;
	dev_t		dev = 0;
	inm_vdev_t	**vdev_soft_state;
	inm_32_t	full_disk = 0;

	minor = vdev->vd_index;

	print_mdata(mdata);	/*debug print */
	if (!mdata->md_mnt_path) {
		ERR("mount point is NULL");
		retval = -EINVAL;
		goto invalid_mdata;
	}
	vdev->vd_private = NULL;

	/*
	 * validate the input value and populate internal structures
	 */
	DEV_DBG("initialize vsnap info strcure from the user input");
	retval = inm_vs_init_vsnap_info(mdata, sizeof(inm_vs_mdata_t), &size,
					&vdev->vd_private);
	if (retval < 0) {
		ERR("inm_vs_init_vsnap_info Failed.");
		goto vsnapmount_failed;
	}

	/*
	 * set the read only flag.
	 */
	vs_info = (inm_vs_info_t *)vdev->vd_private;
	if (vs_info->vsi_access_type == VSNAP_READ_ONLY) {
		DBG("read only device");
		INM_SET_BIT(VSNAP_FLAGS_READ_ONLY, &vdev->vd_flags);
	}

	vdev->vd_refcnt = 1;
	vdev->vd_bio = vdev->vd_biotail = NULL;

	/* Platform dependent code starts here */


	/*
	 * Mark the vdev VSNAP_FLAGS_FREEING so we do not entertain I/Os
	 * and open/close calls to the vsnap while they are
	 * being created.
	 */

	INM_SET_BIT(VSNAP_FLAGS_FREEING, &vdev->vd_flags);

	/* Platform dependent code starts here */
	vdev->vd_rw_ops = vs_rw_ops;
	sector_aligned_size = ( size / DEV_BSIZE ) * DEV_BSIZE;
	if ( sector_aligned_size != size ) {
		ERR("size = %llu, sector aligned size = %llu", size,
		    sector_aligned_size);
		retval = -EFBIG;
		goto size_mismatch;
	}

	/* md_flag is char */
	full_disk = (inm_32_t)(mdata->md_flag);
	if( INM_TEST_BIT(MDATA_FLAG_FULL_DISK, &full_disk) )
		INM_SET_BIT(VDEV_FULL_DISK, &vdev->vd_flags);

	DEV_DBG("set the capacity of disk to %lld", size);
	retval = inm_populate_disk_info(vdev, (inm_u64_t)size);
	if ( retval ) {
		ERR("Cannot populate disk info");
		goto populate_disk_info_failed;
	}

        sprintf(namebuf, INM_VSNAP_BLKDEV"%d", minor);
        retval = ddi_create_minor_node(vdev_dip, namebuf, S_IFBLK,
				      minor, DDI_PSEUDO, NULL);
        if ( retval != DDI_SUCCESS ) {
		ERR("Could not create block device");
                retval = -ENXIO;
		goto create_block_device_failed;
        }

	sprintf(namebuf, INM_VSNAP_BLKDEV"%d,raw", minor);
        retval = ddi_create_minor_node(vdev_dip, namebuf, S_IFCHR,
				      minor, DDI_PSEUDO, NULL);
        if ( retval != DDI_SUCCESS ) {
		ERR("Could not create raw device");
                retval = -ENXIO;
		goto create_raw_device_failed;
        }

#ifndef _SOLARIS_8_
	dev = CALC_DEV_T(inm_vs_major, minor);
        if ((ddi_prop_update_int64(dev, vdev_dip,
            SIZE, (inm_u64_t)size)) != DDI_PROP_SUCCESS) {
                retval = EINVAL;
                goto prop_update_size_failed;
        }

        if ((ddi_prop_update_int64(dev, vdev_dip,
            NBLOCKS, (inm_u64_t)size/DEV_BSIZE)) != DDI_PROP_SUCCESS) {
                retval = EINVAL;
                goto prop_update_nblocks_failed;
        }
#endif

	/*
	 * We are now ready to recieve I/O on the devices
	 */
	INM_CLEAR_BIT(VSNAP_FLAGS_FREEING, &vdev->vd_flags);

	module_refcount++;

out:
	return retval;

prop_update_nblocks_failed:
#ifndef _SOLARIS_8_
	(void) ddi_prop_remove(dev, vdev_dip, SIZE);
#endif

prop_update_size_failed:
	sprintf(namebuf, INM_VSNAP_BLKDEV"%d,raw", minor);
	DEV_DBG("Removing %s", namebuf);
        ddi_remove_minor_node(vdev_dip, namebuf);

create_raw_device_failed:
	sprintf(namebuf, INM_VSNAP_BLKDEV"%d", minor);
	DEV_DBG("Removing %s", namebuf);
        ddi_remove_minor_node(vdev_dip, namebuf);
	ndi_devi_offline(vdev_dip, NDI_DEVI_REMOVE);

create_block_device_failed:
	inm_free_disk_info(vdev);

populate_disk_info_failed:
size_mismatch:

	DEV_DBG("uninit vsnap info");
	inm_vs_uninit_vsnap_info(vdev->vd_private);
	vdev->vd_private = NULL;

vsnapmount_failed:
invalid_mdata:
	DEV_DBG("Free soft state");
	vdev_soft_state = (inm_vdev_t **)ddi_get_soft_state(vs_listp,
					INM_SOL_VS_SOFT_STATE_IDX(minor));
	*vdev_soft_state = NULL;
	ddi_soft_state_free(vs_listp, INM_SOL_VS_SOFT_STATE_IDX(minor));
	DEV_DBG("Deleting disk");
	INM_FREE(&vdev->vd_disk, sizeof(inm_disk_t));
	DEV_DBG("recycle minor");
	minor = vdev->vd_index;
	put_vs_minor(minor);
	goto out;
}



/*
 * inm_vs_uninit_dev
 *
 * DESC
 *      de-initalize a vsnap device
 * ALGO
 *      1. if the vsnap is being used fail the request
 *      2. if no IOs pending then cleanly exit the worker thread
 */
inm_32_t
inm_vs_uninit_dev(inm_vdev_t *vdev, void *arg)
{
        void *vd_private = vdev->vd_private;
	inm_u32_t pending = 0;

        ASSERT(!(vdev->vd_refcnt == 0));
        ASSERT (!vd_private);

        /*
         * cleanly exit the worker thread.
         */

        /*
         * Sleep on vd_sem. Is successful,
         * it means all pending io is complete
         */
	DEV_DBG("Lake INM_INIT_SLEEP");

	INM_INIT_SLEEP(&vdev->vd_sem);
       	INM_ACQ_SPINLOCK_IRQ(&vdev->vd_lock, 0);
	pending = INM_ATOMIC_READ(&vdev->vd_io_pending);
        INM_REL_SPINLOCK_IRQ(&vdev->vd_lock, 0);
	DEV_DBG("Check pending IO's");

        /*
         * when the worker thread is going down this semaphore is used
         * to indicate that its down & out; so wait till the worker thread
         * has completed its tear-down
         */
	if ( pending ) {
		INM_SLEEP(&vdev->vd_sem);
	}

	INM_FINI_SLEEP(&vdev->vd_sem);

        inm_vs_uninit_vsnap_info(vd_private);
        vdev->vd_private = NULL;

        return 0; /* It should never fail */
}

/*
 * inm_vs_free_dev
 *
 * DESC
 *      release the allocated vsnap device struct
 */
inm_32_t
inm_vs_free_dev(inm_vdev_t *vdev)
{
        inm_32_t	retval = 0;
	char 		namebuf[100];
	inm_minor_t	minor = vdev->vd_index;
	inm_vdev_t	**vdev_soft_state;
#ifndef _SOLARIS_8_
	dev_t		dev;
#endif

        retval = put_vs_minor(minor);
        if (retval < 0) {
                ERR("put minor failed");
                goto out;
        }

#ifndef _SOLARIS_8_
	dev = makedevice(inm_vs_major, minor);
	(void) ddi_prop_remove(dev, vdev_dip, SIZE);
	(void) ddi_prop_remove(dev, vdev_dip, NBLOCKS);
#endif
	DEV_DBG("delete the gendisk instance");
	sprintf(namebuf, INM_VSNAP_BLKDEV"%d,raw", minor);
	DEV_DBG("Removing the disk - %s", namebuf);
        ddi_remove_minor_node(vdev_dip, namebuf);
        sprintf(namebuf, INM_VSNAP_BLKDEV"%d", minor);
	DEV_DBG("Removing the disk - %s", namebuf);
        ddi_remove_minor_node(vdev_dip, namebuf);
	ndi_devi_offline(vdev_dip, NDI_DEVI_REMOVE);
	vdev_soft_state = (inm_vdev_t **)ddi_get_soft_state(vs_listp,
					INM_SOL_VS_SOFT_STATE_IDX(minor));
	*vdev_soft_state = NULL;
	ddi_soft_state_free(vs_listp, INM_SOL_VS_SOFT_STATE_IDX(minor));
        DEV_DBG("Free the disk instance");
	inm_free_disk_info(vdev);
	INM_FREE(&vdev->vd_disk, sizeof(inm_disk_t));
        INM_FREE(&vdev, sizeof(inm_vdev_t));
	module_refcount--;

  out:
        return retval;
}

