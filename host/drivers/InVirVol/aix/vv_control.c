#include "vsnap.h"
#include "vsnap_common.h"
#include "vdev_control_common.h"
#include "vsnap_kernel.h"
#include "vsnap_io.h"

extern void *vv_listp;
extern struct inm_vdev_rw_ops vv_rw_ops;
extern inm_u32_t module_refcount;

/*
 * Volpack creation is tricky in solaris. We share major between
 * vsnap and volpack devices. As such, we divide the
 * minors between the two. 0 to n/2 goes to vsnap while n/2 + 1
 * to n goes to volpack. We however maintain different soft state
 * arrays for both of them. The minor in our minor list corresponds
 * to index of volpack in soft state array
 */

inm_32_t
vv_alloc_dev(inm_vdev_t **vvdev, char *dev_path)
{
	inm_32_t	minor = -1;
	inm_32_t	retval = 0;
	inm_vdev_t	*vdev = NULL;

	/*
	 * if the path was provided then the minor number will be extracted
	 * out of the path else will be allocated a new one
	 */
	if (dev_path[0]) {
		DBG("dev path given - %s", dev_path);
		minor =  inm_atoi(dev_path + strlen(INM_VOLPACK_NAME_PREFIX));
		retval = insert_into_minor_list(minor);
		if (retval < 0) {
			ERR("the minor number %d already available", minor);
			goto no_minor_available;
		}
	} else {
		if ((minor = get_free_vv_minor()) < 0) {
			ERR("not able to alloc minor number");
			retval = minor;		/* error value super imposed
						   on the return value */
			goto no_minor_available;
		}

		/*
		 * TODO: the devices can be located anywhere, so cannot
		 * hard-code this, i suppose!
		 */
		sprintf(dev_path, INM_VOLPACK_NAME_PREFIX"%d", minor);
	}

	/*
	 * initialize the vdev fields
	 */
	if (!(vdev = INM_ALLOC(sizeof(inm_vdev_t), GFP_PINNED))) {
		ERR("no mem");
		retval = -ENOMEM;
		goto vdev_alloc_failed;
	}
	memzero(vdev, sizeof(inm_vdev_t));

	vdev->vd_index = minor;
	INM_INIT_SPINLOCK(&vdev->vd_lock);
	INM_INIT_IPC(&vdev->vd_sem);

	/* Platform dependent code starts here */
	/*
	 * Allocate the disk structure
	 */
	DEV_DBG("allocate a new disk instance");
	vdev->vd_disk = INM_ALLOC(sizeof(inm_disk_t), GFP_KERNEL);
	if ( !vdev->vd_disk ) {
		ERR("alloc_disk failed.");
		retval = -ENOMEM;
		goto alloc_disk_failed;
	}

	/*
         * we are restricted upto LEN_DKL_VVOL chars for volpack name
         */
	snprintf(INM_VDEV_NAME(vdev), INM_DISK_NAME_SIZE, "%s",
		 dev_path + strlen(INM_VOLPACK_PATH));

	/*
	 * store the vdev in soft state
	 */

	retval = inm_set_soft_state(vv_listp, INM_SOL_VV_SOFT_STATE_IDX(minor),
				    vdev);
	if ( retval < 0 ) {
		ERR("Set soft state failed");
		goto set_soft_state_failed;
	}

	/*
	 * return the partially filled vdev;
	 * rest will be filled inside vv_init_dev
	 */
	*vvdev = vdev;
	module_refcount++;

out:
	return retval;	/* if success '0' falls through */

set_soft_state_failed:
	INM_FREE(&vdev->vd_disk, sizeof(inm_disk_t));

alloc_disk_failed:
	INM_FREE_PINNED(&vdev, sizeof(inm_vdev_t));

vdev_alloc_failed:
	put_vv_minor(minor);	/* return value is useless so not captured */

no_minor_available:
	*vvdev = NULL;
	goto out;
}

/*
 * vv_init_dev
 *
 * DESC
 *      initialize a volpack device
 *
 */
inm_32_t
vv_init_dev(inm_vdev_t *vdev, void *vnode_ptr, char *file_name,
	    char *dev_path)
{
	inm_u64_t	size = 0;
	inm_32_t	retval = 0;
	inm_vv_info_t	*vv_info = NULL;
	inm_offset_t	sector_aligned_size;

	vv_info = INM_ALLOC(sizeof(inm_vv_info_t), GFP_KERNEL);
	if (!vv_info) {
		ERR("no mem.");
		retval = -ENOMEM;
		goto fail;
	}
	vv_info->vvi_sparse_file_filp = vnode_ptr;
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
		goto seek_file_end_failed;
	}

	vdev->vd_private = vv_info;

	vdev->vd_bio = vdev->vd_biotail = NULL;

	/* Platform dependent code starts here */
	/*
	 * we use the INM_SEEK_END
	 * which will give us the size of the file ... a cleaner approach
	 */
	retval = INM_SEEK_FILE_END(vnode_ptr, &size);
	if ( retval < 0 ) {
		ERR("Cannot get size of sparse file");
		goto seek_file_end_failed;
	}

	vdev->vd_rw_ops = vv_rw_ops;

	/* Platform dependent code starts here */
	sector_aligned_size = ( size / DEV_BSIZE ) * DEV_BSIZE;
	if ( sector_aligned_size != size ) {
		ERR("size = %llu, sector aligned size = %llu", size,
		    sector_aligned_size);
		retval = -EFBIG;
		goto size_mismatch;
	}

	INM_VDEV_SIZE(vdev) = size;

out:
	return retval;

size_mismatch:
seek_file_end_failed:
	INM_FREE(&vv_info->vvi_sparse_file_path, MAX_FILENAME_LENGTH);

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
        inm_32_t	retval = 0;
	inm_minor_t	minor = vdev->vd_index;

        retval = put_vv_minor(minor);
        if (retval < 0) {
                ERR("put minor failed");
                goto out;
        }

	inm_set_soft_state(vv_listp, INM_SOL_VV_SOFT_STATE_IDX(minor), NULL);
        DEV_DBG("Free the disk instance");
	INM_FREE(&vdev->vd_disk, sizeof(inm_disk_t));
        INM_FREE_PINNED(&vdev, sizeof(inm_vdev_t));
	module_refcount--;

out:
        return retval;
}



/*
 * vv_uninit_dev
 *
 * DESC
 *      de-initalize a volpack device
 * ALGO
 *      1. if the volpack is being used fail the request
 *      2. if no IOs pending then cleanly exit the worker thread
 */
inm_32_t
vv_uninit_dev(inm_vdev_t *vdev)
{
       	inm_32_t	retval = 0;
	inm_vv_info_t	*vv_info = vdev->vd_private;
	inm_u32_t pending = 0;
	inm_interrupt_t	intr;

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

	INM_INIT_SLEEP(&vdev->vd_sem);
       	INM_ACQ_SPINLOCK_IRQ(&vdev->vd_lock, intr);
	pending = INM_ATOMIC_READ(&vdev->vd_io_pending);
        INM_REL_SPINLOCK_IRQ(&vdev->vd_lock, intr);

        /*
         * when the worker thread is going down this semaphore is used
         * to indicate that its down & out; so wait till the worker thread
         * has completed its tear-down
         */
	if ( pending ) {
		INM_SLEEP(&vdev->vd_sem);
	}
	INM_FINI_SLEEP(&vdev->vd_sem);

	DEV_DBG("close the sparse file");
	INM_CLOSE_FILE(vv_info->vvi_sparse_file_filp, INM_RDWR);
	INM_FREE(&vv_info->vvi_sparse_file_path, MAX_FILENAME_LENGTH);
	INM_FREE(&vv_info, sizeof(inm_vv_info_t));
        vdev->vd_private = NULL;

out:
        return retval; /* It should never fail */
}
