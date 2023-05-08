#include "common.h"
#include "vsnap.h"
#include "vsnap_kernel.h"
#include "vsnap_io.h"
#include "vsnap_ioctl.h"
#include "vsnap_common.h"
#include "vdev_mem_debug.h"
#include "vdev_control_common.h"
#include "vsnap_open.h"
#include "disk_btree_core.h"
#include "vdev_disk_helpers.h"
#include "vdev_helper.h"

void		*vs_listp;
void		*vv_listp;
dev_info_t	*vdev_dip;
inm_u32_t	module_refcount = 0;
inm_spinlock_t	blk_open_spinlock;
inm_u16_t	blk_open;

#define INM_VDEV_BLK_CONTROLLER "vblkcontrol"

/*ARGSUSED*/
static int
vdev_open(dev_t *devp, int flag, int otyp, struct cred *credp)
{
	inm_minor_t	minor;
	inm_vdev_t	**vdevpp;
	inm_vdev_t	*vdev;
	inm_32_t	retval = 0;

	minor = getminor(*devp);

	/* Do we need to track the opens on control device? */
	if ( minor == 0 ) {
		DEV_DBG("Open on the control device");
		if ( otyp == OTYP_BLK ) {
			DEV_DBG("Open on the block control device");
			INM_ACQ_SPINLOCK(&blk_open_spinlock);
			if ( !blk_open )
				blk_open++;
			else
				retval = EBUSY;
			INM_REL_SPINLOCK(&blk_open_spinlock);
		}
		return retval;

	}

	if ( INM_IS_VSNAP_DEVICE(minor) ) {
		DEV_DBG("Open on vsnap%d", INM_SOL_VS_SOFT_STATE_IDX(minor));
		vdevpp = (inm_vdev_t **)ddi_get_soft_state(vs_listp,
					INM_SOL_VS_SOFT_STATE_IDX(minor));

		if ( vdevpp == NULL ) {
			ERR("Vsnap major = %d, minor = %d is stale",
							getmajor(*devp), minor);
			retval = -ENXIO;
			goto out;
		}

		vdev = *vdevpp;
		retval = inm_vs_blk_open(vdev, otyp, minor, flag);
	} else {
		DEV_DBG("Open on volpack%d", INM_SOL_VV_SOFT_STATE_IDX(minor));
		vdevpp = (inm_vdev_t **)ddi_get_soft_state(vv_listp,
					INM_SOL_VV_SOFT_STATE_IDX(minor));

		if ( vdevpp == NULL ) {
			ERR("Volpack major = %d, minor = %d is stale",
							getmajor(*devp), minor);
			retval = -ENXIO;
			goto out;
		}

		vdev = *vdevpp;
		retval = inm_vv_blk_open(vdev, otyp, minor, flag);
	}

out:
	return (int)(-retval);
}

static int
vdev_close(dev_t dev, int flag, int otyp, struct cred *credp)
{
	inm_minor_t	minor;
	inm_vdev_t	**vdevpp;
	inm_vdev_t	*vdev;
	inm_32_t	retval = 0;

	minor = getminor(dev);

	/* Do we need to track the open/close on control device? */
	if ( minor == 0 ) {
		DEV_DBG("Close on the control device");
 		if ( otyp == OTYP_BLK ) {
			DEV_DBG("Close on the block control device");
			INM_ACQ_SPINLOCK(&blk_open_spinlock);
			ASSERT(!blk_open);
			blk_open--;
			INM_REL_SPINLOCK(&blk_open_spinlock);
		}
		return retval;
	}

	if ( INM_IS_VSNAP_DEVICE(minor) ) {
		DEV_DBG("Close on vsnap%d", INM_SOL_VS_SOFT_STATE_IDX(minor));
		vdevpp = (inm_vdev_t **)ddi_get_soft_state(vs_listp,
					INM_SOL_VS_SOFT_STATE_IDX(minor));

		if ( vdevpp == NULL ) {
			ERR("Vsnap major = %d, minor = %d is stale",
							getmajor(dev), minor);
			retval = -ENXIO;
			goto out;
		}


		vdev = *vdevpp;
		retval = inm_vs_blk_close(vdev, otyp, minor);
	} else {
		DEV_DBG("Close on volpack%d", INM_SOL_VV_SOFT_STATE_IDX(minor));
		vdevpp = (inm_vdev_t **)ddi_get_soft_state(vv_listp,
					INM_SOL_VV_SOFT_STATE_IDX(minor));

		if ( vdevpp == NULL ) {
			ERR("Volpack major = %d, minor = %d is stale",
							getmajor(dev), minor);
			retval = -ENXIO;
			goto out;
		}

		vdev = *vdevpp;
		retval = inm_vv_blk_close(vdev, otyp, minor);
	}

out:
	return (int)(-retval);
}

/*
 * Create the control device
 */
static int
vdev_attach(dev_info_t *dip, ddi_attach_cmd_t cmd)
{
	int	error;
	char 	*name;

	if (cmd != DDI_ATTACH)
		return (DDI_FAILURE);

	error = ddi_soft_state_zalloc(vs_listp, 0);
	if (error == DDI_FAILURE) {
		ERR("ddi_soft_state_zalloc failed");
		return (DDI_FAILURE);
	}

	error = ddi_create_minor_node(dip, INM_VDEV_CONTROLLER, S_IFCHR, 0,
		DDI_PSEUDO, NULL);
	if (error == DDI_FAILURE) {
		ERR("ddi_create_minor_node failed");
		ddi_soft_state_free(vs_listp, 0);
		return (DDI_FAILURE);
	}

	error = ddi_create_minor_node(dip, INM_VDEV_BLK_CONTROLLER, S_IFBLK, 0,
		DDI_PSEUDO, NULL);
	if (error == DDI_FAILURE) {
		ERR("ddi_create_minor_node failed");
		ddi_remove_minor_node(dip, NULL);
		ddi_soft_state_free(vs_listp, 0);
		return (DDI_FAILURE);
	}

	if (ddi_prop_create(DDI_DEV_T_NONE, dip, DDI_PROP_CANSLEEP,
	    DDI_KERNEL_IOCTL, NULL, 0) != DDI_PROP_SUCCESS) {
		ERR("ddi_prop_create failed");
		ddi_remove_minor_node(dip, NULL);
		ddi_soft_state_free(vs_listp, 0);
		return (DDI_FAILURE);
	}

	vdev_dip = dip;

	name = (char *)ddi_driver_name(vdev_dip);
	inm_vs_major = ddi_name_to_major(name);
	inm_vv_major = inm_vs_major;

	DEV_DBG("Name = %s, Major = %d", name, inm_vs_major);

	ddi_report_dev(dip);
	return (DDI_SUCCESS);
}

static int
vdev_detach(dev_info_t *dip, ddi_detach_cmd_t cmd)
{
	if (cmd != DDI_DETACH)
		return (DDI_FAILURE);

	if ( module_refcount ) {
		INFO("Remove All Virtual Snapshots/Volumes before \
							unloading driver");
		return (DDI_FAILURE);
	}

	ddi_remove_minor_node(dip, NULL);
	ddi_prop_remove_all(dip);
	ddi_soft_state_free(vs_listp, 0);

	return (DDI_SUCCESS);
}

static int
vdev_info(dev_info_t *dip, ddi_info_cmd_t infocmd, void *arg, void **result)
{
	switch (infocmd) {

		case DDI_INFO_DEVT2DEVINFO:
			*result = vdev_dip;
			return (DDI_SUCCESS);

		case DDI_INFO_DEVT2INSTANCE:
			*result = 0;
			return (DDI_SUCCESS);
        }

        return (DDI_FAILURE);

}

/*
 * IOCTLS for vsnap devices
 * Pass it down to target device and modify the data returned
 */
static inm_32_t
inm_vdev_ioctl(dev_t dev, int cmd, intptr_t arg, int flag, cred_t *credp,
	       int *rvalp)
{
	inm_minor_t	minor;
	inm_32_t error = 0;
	inm_vdev_t **vdev;
	inm_disk_t *vdisk = NULL;

	minor = getminor(dev);

	if ( minor == 0 ) {
		error = inm_vdev_ioctl_handler(cmd, arg);
		goto out;
	}

	DEV_DBG("Ioctl recieved on vdev with minor = %d", minor);

	if ( INM_IS_VSNAP_DEVICE(minor) )  {
		vdev = (inm_vdev_t **)ddi_get_soft_state(vs_listp,
					INM_SOL_VS_SOFT_STATE_IDX(minor));
		if ( !vdev ) {
			ERR("Cannot find vdev for minor %d in vs_listp", minor);
			error = -ENXIO;
			goto out;
		}
	} else {
		vdev = (inm_vdev_t **)ddi_get_soft_state(vv_listp,
					INM_SOL_VV_SOFT_STATE_IDX(minor));
		if ( !vdev ) {
			ERR("Cannot find vdev for minor %d in vv_listp", minor);
			error = -ENXIO;
			goto out;
		}
	}

	DEV_DBG("vdev = 0x%lx", *vdev);

	/*
         * This section is for ioctls handled by each vsnap device
         * like disk geometry. This is system specific ioctls
	 * so need not use inm* calls
         */

	switch (cmd) {

		case DKIOCGVTOC:
			DEV_DBG("Got DKIOCGVTOC");
			error = inm_get_vdev_vtoc(*vdev, arg, flag, minor);
			break;

		case DKIOCINFO:
			DEV_DBG("Got DKIOCINFO");
			error = inm_get_vdev_iocinfo(*vdev, arg, flag, minor);
			break;

		case DKIOCG_VIRTGEOM:
		case DKIOCG_PHYGEOM:
		case DKIOCGGEOM:
			DEV_DBG("Got DKIOCGGEOM");
			error = inm_get_vdev_geom(*vdev, arg, flag);
			break;

		case DKIOCGMEDIAINFO:
			DEV_DBG("Got DKIOCGMEDIAINFO");
			error = inm_get_vdev_minfo(*vdev, arg, flag);
			break;

		case DKIOCSTATE:
			DEV_DBG("Got DKIOCSTATE");
			error = inm_get_vdev_state(*vdev, arg, flag);
			break;

#ifndef _SOLARIS_8_
		case DKIOCGETEFI:
			DEV_DBG("Got DKIOCGETEFI");
			error = inm_get_vdev_efi(*vdev, arg, flag, minor);
			break;
#endif

		case DKIOCGMBOOT:
			DEV_DBG("Got DKIOCGMBOOT");
			error = inm_get_vdev_mboot(*vdev, arg, flag, minor);
			break;

		case USCSICMD:
			DEV_DBG("Got USCSICMD");
			error = inm_handle_scsi_request(*vdev, arg, flag,minor);
			break;

		default:
			DBG("unknown ioctl %d(0x%x)", cmd, cmd);
			error = -ENOTTY;
			break;
	}

out:

#ifdef _VSNAP_DEBUG_
	ASSERT(error > 0 );
	if ( error )
		DBG("Ioctl %d(0x%x) error = %d", cmd, cmd, error);
#endif

	/*
	 * Solaris expects positive errors
	 */
	error = -error;
	return error;
}

static struct cb_ops vdev_cb_ops = {
	vdev_open,		/* open */
	vdev_close,		/* close */
	vdev_strategy,		/* strategy */
	nodev,			/* print */
	nodev,			/* dump */
	vdev_read,		/* read */
	vdev_write,		/* write */
	inm_vdev_ioctl,		/* ioctl */
	nodev,			/* devmap */
	nodev,			/* mmap */
	nodev,			/* segmap */
	nochpoll,		/* poll */
	ddi_prop_op,		/* prop_op */
	0,			/* streamtab  */
	D_64BIT | D_NEW | D_MP,	/* Driver compatibility flag */
	CB_REV,
	vdev_aread,
	vdev_awrite
};

static struct dev_ops vdev_ops = {
	DEVO_REV,		/* devo_rev, */
	0,			/* refcnt  */
	vdev_info,		/* info */
	nulldev,		/* identify */
	nulldev,		/* probe */
	vdev_attach,		/* attach */
	vdev_detach,		/* detach */
	nodev,			/* reset */
	&vdev_cb_ops,		/* driver operations */
	NULL,			/* no bus operations */
	NULL			/* power */
	/*NULL			quiesce */
};

static struct modldrv modldrv = {
	&mod_driverops,
	"Inmage VSnap Driver",
	&vdev_ops
};

static struct modlinkage modlinkage = {
	MODREV_1,
	&modldrv,
	NULL
};

int
_init(void)
{
	int error;

	error = inm_vdev_init();
	if ( error ) {
		ERR("vdev init failed");
		error = -error;
		goto vdev_init_failed;
	}

	/* OS specific initialisation */
	/*
         * We use minor 0 for the control device
         * mark it used
         */
	blk_open = 0;
	INM_INIT_SPINLOCK(&blk_open_spinlock);

	error = get_free_vs_minor();
	if( error ) {
		ERR("Expecting minor 0 for control device, got %d", error);
		goto get_free_vs_minor_failed;

	}

	error = ddi_soft_state_init(&vs_listp,
	    sizeof (inm_vdev_t *), 0);
	if (error) {
		ERR("ddi_vs_soft_state_init failed");
		goto ddi_vs_soft_state_init_failed;
	}

	error = ddi_soft_state_init(&vv_listp,
	    sizeof (inm_vdev_t *), 0);
	if (error) {
		ERR("ddi_vv_soft_state_init failed");
		goto ddi_vv_soft_state_init_failed;
	}

	error = mod_install(&modlinkage);
	if ( error ) {
		ERR("mod_install failed", error);
		goto mod_install_failed;
	}
        error = init_thread_pool();
	if ( error ) {
		ERR("Cannot initialise thread pool");
		error = -error;
		goto tpool_init_failed;
	}

	INFO("Successfully loaded module.Built on " \
             __DATE__ " [ "__TIME__ " ]");

out:
	return (error);

tpool_init_failed:
	mod_remove(&modlinkage);
mod_install_failed:
	ddi_soft_state_fini(&vv_listp);

ddi_vv_soft_state_init_failed:
	ddi_soft_state_fini(&vs_listp);

ddi_vs_soft_state_init_failed:
	put_vs_minor((inm_minor_t)0);

get_free_vs_minor_failed:
	inm_vdev_exit();

vdev_init_failed:
	goto out;
}

int
_fini(void)
{
	int	error;

	if ( module_refcount ) {
		return (DDI_FAILURE);
	}
	uninit_thread_pool();

	error = mod_remove(&modlinkage);
	if (error) {
		ERR("mod_remove failed");
		return (error);
	}

	put_vs_minor((inm_minor_t)0);
	inm_vdev_exit();

	ddi_soft_state_fini(&vv_listp);
	ddi_soft_state_fini(&vs_listp);

	INFO("Successfully unloaded vsnap module");

	return (error);
}

int
_info(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}
