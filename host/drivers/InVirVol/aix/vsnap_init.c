#include "common.h"
#include "vdev_soft_state.h"
#include "vsnap.h"
#include "vsnap_kernel.h"
#include "vsnap_io.h"
#include "vsnap_ioctl.h"
#include "vsnap_common.h"
#include "vdev_mem_debug.h"
#include "vdev_control_common.h"
#include "vsnap_open.h"
#include "disk_btree_core.h"

static inm_u32_t	config_done = 0;
inm_u32_t		module_refcount = 0;
static inm_u32_t	control_open = 0;
void			*vs_listp;
void			*vv_listp;

inm_32_t vdev_config(dev_t, int, register struct uio *);

extern bio_chain_t *bio_chain;

inm_32_t
vdev_open(dev_t devno, ulong devflag, chan_t chan, inm_32_t ext)
{
	inm_vdev_t 	*vdev;
	inm_minor_t	minor = minor_num(devno);
	inm_32_t	retval = 0;

	DEV_DBG("Open on minor = %u", (inm_u32_t)minor);
	if( minor != 0 ) {
		if ( INM_IS_VSNAP_DEVICE(minor) ) {
			vdev = inm_get_soft_state(vs_listp,
					INM_SOL_VS_SOFT_STATE_IDX(minor));
			retval = inm_vs_blk_open(vdev);
		} else {
			vdev = inm_get_soft_state(vv_listp,
					INM_SOL_VV_SOFT_STATE_IDX(minor));
			retval = inm_vv_blk_open(vdev);
		}
	} else {
		control_open = 1;
	}

	DEV_DBG("Return %d", retval);
	return -retval;
}

inm_32_t
vdev_close(dev_t devno, chan_t chan)
{
	inm_vdev_t 	*vdev;
	inm_minor_t	minor = minor_num(devno);
	inm_32_t	retval = 0;

	DEV_DBG("Close on minor = %u", (inm_u32_t)minor);
	if( minor != 0 ) {
		if ( INM_IS_VSNAP_DEVICE(minor) ) {
			vdev = inm_get_soft_state(vs_listp,
					INM_SOL_VS_SOFT_STATE_IDX(minor));
			retval = inm_vs_blk_close(vdev);
		} else {
			vdev = inm_get_soft_state(vv_listp,
					INM_SOL_VV_SOFT_STATE_IDX(minor));
			retval = inm_vv_blk_close(vdev);
		}
	} else {
		control_open = 0;
	}

	return -retval;
}

#define TB	1099511627776LL

inm_32_t
vdev_ioctl(dev_t devno, int cmd, void *arg, ulong devflag, chan_t chan,
		int ext)
{
	inm_32_t 	retval = 0;
	inm_minor_t	minor = minor_num(devno);
	struct devinfo	devi;
	inm_vdev_t	*vdev = NULL;
	inm_interrupt_t intr;

	INM_ACQ_SPINLOCK_IRQ(&bio_chain->bc_spinlock, intr);

        if ( bio_chain->bc_driver_unloading ) {
		INM_REL_SPINLOCK_IRQ(&bio_chain->bc_spinlock, intr);
		ERR("Driver is being unloaded");
		retval = -EINVAL;
		return retval;
	}

        INM_REL_SPINLOCK_IRQ(&bio_chain->bc_spinlock, intr);

	if( minor == 0 ) {
		retval = inm_vdev_ioctl_handler(cmd, arg);
	} else {
		if ( INM_IS_VSNAP_DEVICE(minor) )
			vdev = inm_get_soft_state(vs_listp,
					INM_SOL_VS_SOFT_STATE_IDX(minor));
		else
			vdev = inm_get_soft_state(vv_listp,
					INM_SOL_VV_SOFT_STATE_IDX(minor));

		switch(cmd) {
			case IOCINFO:
				DBG("IOCINFO ioctl");
				if( INM_TEST_BIT(VDEV_FULL_DISK,
						 &vdev->vd_flags) ) {
					devi.devtype = DD_SCDISK;
					devi.devsubtype = DS_PV | DS_SCSI;

					devi.un.scdk64.blksize = DEV_BSIZE;
					devi.un.scdk64.lo_numblks = (inm_32_t)
					    ( INM_VDEV_SIZE(vdev) / DEV_BSIZE );
					devi.un.scdk64.lo_max_request = 262144;
					devi.un.scdk64.segment_size = 0;
					devi.un.scdk64.segment_count = 0;
					devi.un.scdk64.byte_count = 0;
					devi.un.scdk64.hi_numblks = (inm_32_t)
					(INM_VDEV_SIZE(vdev) / DEV_BSIZE >> 32);
					devi.un.scdk64.hi_max_request = 0;
				} else {
					devi.devtype = DD_DISK;
					devi.devsubtype = DS_LV;

					devi.un.dk64.bytpsec = DEV_BSIZE;
					devi.un.dk64.secptrk = 512;
					devi.un.dk64.trkpcyl = 1024;
					devi.un.dk64.lo_numblks = (inm_32_t)
					    ( INM_VDEV_SIZE(vdev) / DEV_BSIZE );
					devi.un.dk64.segment_size = 0;
					devi.un.dk64.segment_count = 0;
					devi.un.dk64.byte_count = 0;
					devi.un.dk64.hi_numblks = (inm_32_t)
					(INM_VDEV_SIZE(vdev) / DEV_BSIZE >> 32);
				}

				devi.flags = DF_FIXED | DF_RAND | DF_LGDSK |
					     DF_FAST;


				if( INM_TEST_BIT(DKERNEL, &devflag) ) {
					if (memcpy_s(arg, sizeof(struct devinfo), &devi,
						       sizeof(struct devinfo))) {
						retval = -EFAULT;
					}
				} else {
					retval = copyout((char *)&devi,
							 (char *)arg,
							 sizeof(struct devinfo));
					/* copyout gives +ve errors */
					if( retval ) {
						retval = -retval;
					}
				}

				break;

			default:
				DBG("Unknown Ioctl %d", cmd);
				retval = -EINVAL;
				break;
		}
	}

	if( retval ) {
		ERR("Returning error %d", retval);
		retval = -retval;
	}

	return retval;
}

struct devsw vdev_devsw = {
	.d_open = vdev_open,
	.d_close = vdev_close,
	.d_read = vdev_read,
	.d_write = vdev_write,
	.d_ioctl = vdev_ioctl,
	.d_strategy = vdev_strategy,
	.d_ttys = NULL,
	.d_select = nodev,
	.d_config = vdev_config,
	.d_print = nodev,
	.d_dump = nodev,
	.d_mpx = nodev,
	.d_revoke = nodev,
	.d_dsdptr = NULL,
	.d_selptr = NULL,
	.d_opts = DEV_MPSAFE | DEV_64BIT
};

/* Initialise Driver */
inm_32_t
vdev_init(dev_t devno)
{
	int error = 0;

	if( config_done == 1 ) {
		DBG("Trying to config again");
		goto out;
	}

	if (error = pincode(vdev_open)) {
		goto out;
	}

	error = inm_vdev_init();
	if ( error ) {
		ERR("vdev init failed");
		goto vdev_init_failed;
	}

	/* OS specific initialisation */
	/*
         * We use minor 0 for the control device
         * mark it used
         */

	inm_vs_major = major_num(devno);
	inm_vv_major = inm_vs_major;

	DBG("Major = %d", inm_vs_major);

	error = get_free_vs_minor();
	ASSERT(error != 0);
	ASSERT(minor(devno) != 0);

        error = init_thread_pool();
	if ( error ) {
		ERR("Cannot initialise thread pool");
		goto tpool_init_failed;
	}

	error = inm_soft_state_init(&vs_listp);
	if( error ) {
		ERR("soft state init vs failed");
		goto vs_soft_state_init_failed;
	}

	error = inm_soft_state_init(&vv_listp);
	if( error ) {
		ERR("soft state init vv failed");
		goto vv_soft_state_init_failed;
	}

	error = vdev_io_init();
	if( error ) {
		ERR("Cannot initialise io structures");
		goto io_init_failed;
	}

	error = devswadd(devno, &vdev_devsw);
	if( error ) {
		ERR("devswadd Failed with %d", error);
		goto dev_sw_add_failed;
	}

	INFO("Successfully loaded module.Built on " \
             __DATE__ " [ "__TIME__ " ]");

	config_done = 1;

out:
	return error;

dev_sw_add_failed:
	vdev_io_exit();

io_init_failed:
	inm_soft_state_destroy(vv_listp);

vv_soft_state_init_failed:
	inm_soft_state_destroy(vs_listp);

vs_soft_state_init_failed:
	uninit_thread_pool();

tpool_init_failed:
	put_vs_minor((inm_minor_t)0);
	inm_vdev_exit();

vdev_init_failed:
	unpincode(vdev_open);
	goto out;
}

/* Destroy data structures, thread pool */
inm_32_t
vdev_fini(dev_t devno)
{
	int		error = 0;
	inm_interrupt_t intr;

	DBG("Call to unload driver");
	if( !config_done ) {
		error = -EINVAL;
		goto out;
	}

	if ( module_refcount || control_open ) {
		UPRINT("Remove all vsnap/volpack devices");
		error = -EBUSY;
		goto out;
	}

	INM_ACQ_SPINLOCK_IRQ(&bio_chain->bc_spinlock, intr);
	bio_chain->bc_driver_unloading = 1;   
	INM_REL_SPINLOCK_IRQ(&bio_chain->bc_spinlock, intr);

	config_done = 0;

	devswdel(devno);
	delay(10 * HZ);

	vdev_io_exit();

	inm_soft_state_destroy(&vv_listp);
	inm_soft_state_destroy(&vs_listp);

	uninit_thread_pool();
	put_vs_minor((inm_minor_t)0);
	INFO("Successfully unloaded vsnap module");
	inm_vdev_exit();
	unpincode(vdev_open);

out:
	return error;
}

/*
* NAME: vdev_config
*
* FUNCTION: vdev_config() initializes the vsnap/volpack device driver.
*
* INPUTS:
*  devno - the major and minor device numbers.
*  cmd   - CFG_INIT if device is being defined.
*        - CFG_TERM if device is being deleted.
*  uiop  - pointer to uio structure containing vdev_config data.
*
* OUTPUTS: none.
*
*/

inm_32_t vdev_config(dev_t devno, int cmd, register struct uio *uiop)
{
	inm_32_t	retval = 0;

	switch (cmd) {
		case CFG_INIT:	retval = vdev_init(devno);
				break;

		case CFG_TERM:	retval = vdev_fini(devno);
				break;

		default: 	retval = -EINVAL;
				break;
	}

	if( retval )
		ERR("Config error %d", -retval);

	return -retval;
}

