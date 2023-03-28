#include "common.h"
#include "vsnap_kernel_helpers.h"
#include "vsnap.h"
#include "vdev_control_common.h"
#include "vdev_disk_helpers.h"

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
inm_vs_blk_open(inm_vdev_t *vdev, inm_32_t otype, inm_minor_t minor,
		inm_32_t oflag)
{

	inm_32_t idx;
	inm_32_t retval = 0;
	unsigned long *flags = &vdev->vd_flags;

	DEV_DBG("Open Called: Current RefCnt = %d", vdev->vd_refcnt);

	if ( vdev ) {

		if( INM_MINOR_IS_PARTITION(vdev, minor) ) {
			idx = INM_PART_IDX(vdev->vd_index, minor);

			if(  INM_PART_SIZE(vdev, idx) == 0 &&
			     !(oflag & (FNDELAY | FNONBLOCK)) ) {
				DBG("Size 0 partition");
				retval = -EIO;
				goto out;
			}

			flags = &(INM_PART_FLAG(vdev, idx));
		}

		INM_ACQ_SPINLOCK(&inm_vs_minor_list_lock);

		if ( INM_TEST_BIT(VSNAP_FLAGS_FREEING, &vdev->vd_flags) == 0 ) {
			switch( otype ) {
				case OTYP_BLK:
				if ( !INM_TEST_BIT(VDEV_BLK_OPEN, flags) ) {
					INM_SET_BIT(VDEV_BLK_OPEN, flags);
					vdev->vd_refcnt++;
				}
				break;

				case OTYP_CHR:
				if ( !INM_TEST_BIT(VDEV_CHR_OPEN, flags) ) {
					INM_SET_BIT(VDEV_CHR_OPEN, flags);
					vdev->vd_refcnt++;
				}
				break;

				case OTYP_LYR:
				vdev->vd_refcnt++;
				break;
			}
		} else {
			 vdev = NULL;
		}

		INM_REL_SPINLOCK(&inm_vs_minor_list_lock);
	}

	if ( !vdev ) {
		INFO("Device is stale");
	}

	retval = vdev ? 0 : -ENXIO;

out:
	return retval;
}

/*
 * inm_vs_blk_close
 *
 * DESC
 *      handle the close call on the device
 */
inm_32_t
inm_vs_blk_close(inm_vdev_t *vdev, inm_32_t otype, inm_minor_t minor)
{
	inm_32_t retval = 0;
	unsigned long *flags = &vdev->vd_flags;
	inm_32_t delete = 0;
	inm_32_t idx = 0;

	DEV_DBG("Close Called: Current RefCnt = %d", vdev->vd_refcnt);

	if( INM_MINOR_IS_PARTITION(vdev, minor) ) {
		idx = INM_PART_IDX(vdev->vd_index, minor);
		flags = &(INM_PART_FLAG(vdev, idx));
	}

	INM_ACQ_SPINLOCK (&inm_vs_minor_list_lock);

	if ( vdev ) {
		--vdev->vd_refcnt;
		switch( otype ) {
			case OTYP_BLK:
				INM_CLEAR_BIT(VDEV_BLK_OPEN, flags);
				DBG("Close: OTYP_BLK on %d", minor);
				break;

			case OTYP_CHR:
				INM_CLEAR_BIT(VDEV_CHR_OPEN, flags);
				break;

			case OTYP_LYR:
				break;
		}

		if ( ( 1 == vdev->vd_refcnt ) &&
		      ( INM_TEST_BIT(VSNAP_FLAGS_FREEING, &vdev->vd_flags) ) )
				delete = 1;
	}

	INM_REL_SPINLOCK(&inm_vs_minor_list_lock);

	if( delete ) {
		INFO ("Stale device %s", INM_VDEV_NAME(vdev));
		if ( inm_delete_vsnap_bydevid(vdev) )
			ERR ("Unable to delete vsnap device");
	}

        retval = vdev ? 0 : -ENXIO;

out:
	return retval;
}

/*
 * inm_vv_blk_open
 *
 * DESC
 *      handle the open call on this device
 *      (if the device is being removed then VSNAP_FLAGS_FREEING is set
 *       in the flags)
 */
inm_32_t
inm_vv_blk_open(inm_vdev_t *vdev, inm_32_t otype, inm_minor_t minor,
		inm_32_t flag)
{
	DEV_DBG("Open Called: Current RefCnt = %d", vdev->vd_refcnt);

	INM_ACQ_SPINLOCK(&inm_vv_minor_list_lock);
	if ( vdev ) {
		if ( INM_TEST_BIT(VSNAP_FLAGS_FREEING, &vdev->vd_flags) == 0 ) {
			switch( otype ) {
				case OTYP_BLK:
				if ( !INM_TEST_BIT(VDEV_BLK_OPEN,
						&vdev->vd_flags) ) {
					INM_SET_BIT(VDEV_BLK_OPEN,
						&vdev->vd_flags);
					vdev->vd_refcnt++;
				}
				break;

				case OTYP_CHR:
				if ( !INM_TEST_BIT(VDEV_CHR_OPEN,
						&vdev->vd_flags) ) {
					INM_SET_BIT(VDEV_CHR_OPEN,
						&vdev->vd_flags);
					vdev->vd_refcnt++;
				}
				break;

				case OTYP_LYR:
				vdev->vd_refcnt++;
				break;
			}
		} else
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
inm_vv_blk_close(inm_vdev_t *vdev, inm_32_t otype, inm_minor_t minor)
{
	DEV_DBG("Close Called: Current RefCnt = %d", vdev->vd_refcnt);

	INM_ACQ_SPINLOCK(&inm_vv_minor_list_lock);
	if ( vdev ) {
		--vdev->vd_refcnt;
		switch( otype ) {
			case OTYP_BLK:
				INM_CLEAR_BIT(VDEV_BLK_OPEN, &vdev->vd_flags);
				break;

			case OTYP_CHR:
				INM_CLEAR_BIT(VDEV_CHR_OPEN, &vdev->vd_flags);
				break;

			case OTYP_LYR:
				break;
		}
	}
	INM_REL_SPINLOCK(&inm_vv_minor_list_lock);
	return vdev ? 0 : -ENXIO;
}
