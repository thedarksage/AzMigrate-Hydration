#include "vsnap.h"
#include "vsnap_common.h"
#include "vdev_control_common.h"
#include "vsnap_kernel.h"
#include "vsnap_io.h"

extern void *vs_listp;
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

	DEV_DBG("reserve the next available minor #");
	minor = get_free_vs_minor();	/* never fails */
	if (minor < 0) {
		ERR("no mem.");
		retval = minor;		/* error super imposed on minor */
		goto no_minor_available;
	}
	*vminor = minor;

	temp_dev = INM_ALLOC(sizeof(inm_vdev_t), GFP_PINNED);
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

	DEV_DBG("allocating the device name");
	temp_dev_path = INM_ALLOC(strlen(dev_path) + 1, GFP_KERNEL);
	if (!temp_dev_path) {
		ERR("no mem");
		retval = -ENOMEM;
		goto temp_dev_path_failed;
	}
	memzero(temp_dev_path, strlen(dev_path) + 1);
	memcpy_s(temp_dev_path, strlen(dev_path), dev_path, strlen(dev_path));
	temp_dev_path[strlen(dev_path)] = '\0';
	/*
         * we are restricted upto LEN_DKL_VVOL chars for vsnap name
         */
	snprintf(INM_VDEV_NAME(temp_dev), INM_DISK_NAME_SIZE, "%s",
		 temp_dev_path + strlen("/dev/vs/"));
	INM_FREE(&temp_dev_path, STRING_MEM(temp_dev_path));

	retval = inm_set_soft_state(vs_listp, INM_SOL_VS_SOFT_STATE_IDX(minor),
				    temp_dev);
	if ( retval < 0 ) {
		ERR("Get soft state failed");
		retval = -EFAULT;
		goto set_soft_state_failed;
	}

	/*
	 * return the partially filled vdev;
	 * rest will be filled inside inm_vs_init
	 * ToDo: move this vdev allocation to inm_vs_init
	 */


out:
	return retval;	/* if success '0' falls through */

set_soft_state_failed:
temp_dev_path_failed:
	INM_FREE(&temp_dev->vd_disk, sizeof(inm_disk_t));

alloc_disk_failed:
	INM_FREE_PINNED(&temp_dev, sizeof(inm_vdev_t));

vdev_alloc_failed:
	put_vs_minor(minor);	/* return value is useless so not captured */

no_minor_available:
	*vdev = NULL;
	*vminor = -1;
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
	DBG("Sparse - %d", mdata->md_flag);
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
	inm_64_t	size = 0;
	inm_32_t	retval = 0;
	inm_vs_info_t	*vs_info = NULL;
	inm_offset_t	sector_aligned_size;
	inm_minor_t	minor;
	inm_32_t	full_disk;

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

	full_disk = (inm_32_t)(mdata->md_flag);
	if( INM_TEST_BIT(MDATA_FLAG_FULL_DISK, &full_disk) )
		INM_SET_BIT(VDEV_FULL_DISK, &vdev->vd_flags);


	vdev->vd_refcnt = 1;
	vdev->vd_bio = vdev->vd_biotail = NULL;

	vdev->vd_rw_ops = vs_rw_ops;
	sector_aligned_size = ( size / DEV_BSIZE ) * DEV_BSIZE;
	if ( sector_aligned_size != size ) {
		ERR("size = %llu, sector aligned size = %llu", size,
		    sector_aligned_size);
		retval = -EFBIG;
		goto size_mismatch;
	}

	/* Store the complete size in bytes */
	INM_VDEV_SIZE(vdev) = size;

	module_refcount++;

out:
	return retval;

size_mismatch:
	DEV_DBG("uninit vsnap info");
	inm_vs_uninit_vsnap_info(vdev->vd_private);
	vdev->vd_private = NULL;

vsnapmount_failed:
invalid_mdata:
	DEV_DBG("Free soft state");
	inm_set_soft_state(vs_listp, INM_SOL_VS_SOFT_STATE_IDX(minor), NULL);
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
	inm_interrupt_t	intr;

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
       	INM_ACQ_SPINLOCK_IRQ(&vdev->vd_lock, intr);
	pending = INM_ATOMIC_READ(&vdev->vd_io_pending);
        INM_REL_SPINLOCK_IRQ(&vdev->vd_lock, intr);
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
	inm_minor_t	minor = vdev->vd_index;

        retval = put_vs_minor(minor);
        if (retval < 0) {
                ERR("put minor failed");
                goto out;
        }

	inm_set_soft_state(vs_listp, INM_SOL_VS_SOFT_STATE_IDX(minor), NULL);
        DEV_DBG("Free the disk instance");
	INM_FREE(&vdev->vd_disk, sizeof(inm_disk_t));
        INM_FREE_PINNED(&vdev, sizeof(inm_vdev_t));
	module_refcount--;

  out:
        return retval;
}

