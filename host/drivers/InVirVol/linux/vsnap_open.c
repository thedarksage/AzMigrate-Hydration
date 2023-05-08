#include "common.h"
#include "vsnap_kernel_helpers.h"
#include "vsnap.h"
#include "vdev_control_common.h"

/* extern definitions */
extern inm_spinlock_t inm_vs_minor_list_lock;
extern inm_spinlock_t inm_vv_minor_list_lock;

/*
 * inm_vs_blk_open
 *
 * DESC
 *      handle the open call on this device
 *      (if the device is being removed then VSNAP_FLAGS_FREEING is set
 *       in the flags)
 */
inm_32_t
inm_vs_blk_open_common(inm_vdev_t *vdev)
{
	INM_ACQ_SPINLOCK(&inm_vs_minor_list_lock);
	if ( vdev ) {
		if ( INM_TEST_BIT(VSNAP_FLAGS_FREEING, &vdev->vd_flags) == 0 ){
			vdev->vd_refcnt++;
		} else {
			 vdev = NULL;
		}
	}
	INM_REL_SPINLOCK(&inm_vs_minor_list_lock);

	if ( !vdev ) {
		INFO("Device is stale");
	}

	return vdev ? 0 : -ENXIO;
}

/*
 * inm_vs_blk_close
 *
 * DESC
 *      handle the close call on the device
 */
inm_32_t
inm_vs_blk_close_common(inm_vdev_t *vdev)
{
	INM_ACQ_SPINLOCK (&inm_vs_minor_list_lock);
	--vdev->vd_refcnt;
	if ( ( 1 == vdev->vd_refcnt ) &&
		( INM_TEST_BIT(VSNAP_FLAGS_FREEING, &vdev->vd_flags) ) ) {
			INM_REL_SPINLOCK (&inm_vs_minor_list_lock);
			INFO ("Stale device %s", INM_VDEV_NAME(vdev));
			if ( inm_delete_vsnap_bydevid (vdev) )
				ERR ("Unable to delete vsnap device");
			goto out;
	}
	INM_REL_SPINLOCK(&inm_vs_minor_list_lock);
out:
        return 0;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,32)
inm_32_t
inm_vs_blk_open(struct inode *inode, struct file *file)
{
	return inm_vs_blk_open_common(inode->i_bdev->bd_disk->private_data);
}

inm_32_t
inm_vs_blk_close(struct inode *inode, struct file *file)
{
	return inm_vs_blk_close_common(inode->i_bdev->bd_disk->private_data);
}

#else
inm_32_t
inm_vs_blk_open(struct block_device *bdev, fmode_t mode)
{
	return inm_vs_blk_open_common(bdev->bd_disk->private_data);
}

inm_32_t
inm_vs_blk_close(struct gendisk *disk, fmode_t mode)
{
	return inm_vs_blk_close_common(disk->private_data);
}

#endif


/*
 * inm_vv_blk_open
 *
 * DESC
 *      handle the open call on this device
 *      (if the device is being removed then VSNAP_FLAGS_FREEING is set
 *       in the flags)
 */
inm_32_t
inm_vv_blk_open_common(inm_vdev_t *vdev)
{
	INM_ACQ_SPINLOCK(&inm_vv_minor_list_lock);
	if ( vdev ) {
		if ( INM_TEST_BIT(VSNAP_FLAGS_FREEING, &vdev->vd_flags) == 0 )
			vdev->vd_refcnt++;
		else
			vdev = NULL;
	}
	INM_REL_SPINLOCK(&inm_vv_minor_list_lock);
	return vdev ? 0 : -ENXIO;
}

/*
 * inm_vv_blk_close
 *
 * DESC
 *      handle the close call on the device
 */
inm_32_t
inm_vv_blk_close_common(inm_vdev_t *vdev)
{
	INM_ACQ_SPINLOCK(&inm_vv_minor_list_lock);
	--vdev->vd_refcnt;
	INM_REL_SPINLOCK(&inm_vv_minor_list_lock);
	return 0;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,32)
inm_32_t
inm_vv_blk_open(struct inode *inode, struct file *file)
{
	return inm_vv_blk_open_common(inode->i_bdev->bd_disk->private_data);
}

inm_32_t
inm_vv_blk_close(struct inode *inode, struct file *file)
{
	return inm_vv_blk_close_common(inode->i_bdev->bd_disk->private_data);
}

#else
inm_32_t
inm_vv_blk_open(struct block_device *bdev, fmode_t mode)
{
	return inm_vv_blk_open_common(bdev->bd_disk->private_data);
}

inm_32_t
inm_vv_blk_close(struct gendisk *disk, fmode_t mode)
{
	return inm_vv_blk_close_common(disk->private_data);
}

#endif
