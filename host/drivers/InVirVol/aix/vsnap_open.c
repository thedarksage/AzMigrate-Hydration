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
inm_vs_blk_open(inm_vdev_t *vdev)
{
	if ( vdev ) {
		INM_ACQ_SPINLOCK(&inm_vs_minor_list_lock);
		if ( !INM_TEST_BIT(VSNAP_FLAGS_FREEING, &vdev->vd_flags) ) {
			if( vdev->vd_refcnt == 1 )
				vdev->vd_refcnt++;
		} else {
			vdev = NULL;
		}
		INM_REL_SPINLOCK(&inm_vs_minor_list_lock);
	} else {
		ERR("Device is stale");
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
inm_vs_blk_close(inm_vdev_t *vdev)
{
	inm_32_t delete_flag = 0;

	if ( vdev ) {
		INM_ACQ_SPINLOCK (&inm_vs_minor_list_lock);

		vdev->vd_refcnt--;

		if( INM_TEST_BIT(VSNAP_FLAGS_FREEING, &vdev->vd_flags) )
			delete_flag = 1;

		INM_REL_SPINLOCK(&inm_vs_minor_list_lock);
	}

	if( delete_flag ) {
		INFO ("Deleting Stale device %s", INM_VDEV_NAME(vdev));
		if ( inm_delete_vsnap_bydevid(vdev) )
			ERR ("Unable to delete vsnap device");
	}

        return vdev ? 0 : -ENXIO;
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
inm_vv_blk_open(inm_vdev_t *vdev)
{
	DEV_DBG("Open Called: Current RefCnt = %d", vdev->vd_refcnt);

	if ( vdev ) {
		INM_ACQ_SPINLOCK(&inm_vv_minor_list_lock);
		if( !vdev->vd_refcnt )
			vdev->vd_refcnt++;
		INM_REL_SPINLOCK(&inm_vv_minor_list_lock);
	}

	return vdev ? 0 : -ENXIO;
}


/*
 * inm_vv_blk_close
 *
 * DESC
 *      handle the close call on the device
 */
inm_32_t
inm_vv_blk_close(inm_vdev_t *vdev)
{
	DEV_DBG("Close Called: Current RefCnt = %d", vdev->vd_refcnt);

	if ( vdev ) {
		INM_ACQ_SPINLOCK(&inm_vv_minor_list_lock);
		vdev->vd_refcnt = 0;
		INM_REL_SPINLOCK(&inm_vv_minor_list_lock);
	}

	return vdev ? 0 : -ENXIO;
}
