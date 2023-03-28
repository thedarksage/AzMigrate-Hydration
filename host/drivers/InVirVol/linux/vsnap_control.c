#include "vsnap.h"
#include "vsnap_common.h"
#include "vdev_control_common.h"
#include "vsnap_kernel.h"
#include "vsnap_io.h"
#include "vdev_proc_common.h"
#include "vsnap_open.h"

extern struct inm_vdev_rw_ops vs_rw_ops;

/* vsnap block device ops */
struct block_device_operations inm_vs_blk_fops = {
        .open = inm_vs_blk_open,
        .release = inm_vs_blk_close,
        .owner = THIS_MODULE,
};

extern inm_32_t inm_vdev_create_proc_entry(char *, inm_vdev_t *);
extern void inm_vdev_del_proc_entry(inm_vdev_t *);


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
inm_vs_alloc_dev(inm_vdev_t **vdev, inm_32_t *vminor, char *dev_path)
{
	char		*temp_dev_path = NULL;
	inm_32_t	retval = 0;
	inm_32_t	minor = -1;
	inm_vdev_t	*temp_dev = NULL;

	DEV_DBG("get a refcount on this module");
	__module_get(THIS_MODULE);
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
	temp_dev->vd_request_queue = blk_alloc_queue(GFP_KERNEL);
	if (!temp_dev->vd_request_queue) {
		ERR("blk_request_queue failed.");
		retval = -ENOMEM;
		goto request_q_alloc_failed;
	}


	/*
	 * initialize the gendisk & its fields. also add the disk to kernel
	 * this disk does not contain partitions and hence the input for
	 * alloc_disk is INM_NO_PARTITIONS (= 1)
	 */
	DEV_DBG("allocate a new gendisk instance");
	temp_dev->vd_disk = alloc_disk(16);
	if (!temp_dev->vd_disk) {
		ERR("alloc_disk failed.");
		retval = -ENOMEM;
		goto alloc_disk_failed;
	}
	temp_dev->vd_disk->major = inm_vs_major;
	temp_dev->vd_disk->first_minor = minor;
	temp_dev->vd_disk->fops = &inm_vs_blk_fops;

	/*
	 * the device name is passed by the vsnaplib
	 * this has the /dev/vs/cx??? or /dev/vs/cli??? format, so stripping
	 * the /dev portion before creating using the add_disk
	 */
	DEV_DBG("allocating the device name");
	temp_dev_path = INM_ALLOC(strlen(dev_path) + 1, GFP_KERNEL);
	if (!temp_dev_path) {
		ERR("no mem");
		goto temp_dev_path_failed;
	}
	memzero(temp_dev_path, strlen(dev_path) + 1);

	if (memcpy_s(temp_dev_path, strlen(dev_path), dev_path, strlen(dev_path))) {
		retval = -EFAULT;
		INM_FREE(&temp_dev_path, STRING_MEM(temp_dev_path));
		goto temp_dev_path_failed;
	}

	temp_dev_path[strlen(dev_path)] = '\0';
	sprintf(temp_dev->vd_disk->disk_name, temp_dev_path + strlen("/dev/"));
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,18)
	sprintf(temp_dev->vd_disk->devfs_name,temp_dev_path + strlen("/dev/"));
#endif
	INM_FREE(&temp_dev_path, STRING_MEM(temp_dev_path));

	/*
	 * store the private_data and request_queue
	 */
	temp_dev->vd_disk->private_data = temp_dev;
	temp_dev->vd_disk->queue = temp_dev->vd_request_queue;

	/*
	 * return the partially filled vdev;
	 * rest will be filled inside inm_vs_init
	 * ToDo: move this vdev allocation to inm_vs_init
	 */


  out:
	return retval;	/* if success '0' falls through */

  temp_dev_path_failed:
	put_disk(temp_dev->vd_disk);

  alloc_disk_failed:
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24)
	blk_put_queue(temp_dev->vd_request_queue);
#else
	blk_cleanup_queue(temp_dev->vd_request_queue);
#endif

  request_q_alloc_failed:
	INM_FREE(&temp_dev, sizeof(inm_vdev_t));

  vdev_alloc_failed:
	put_vs_minor(minor);	/* return value is useless so not captured */

  no_minor_available:
	module_put(THIS_MODULE);
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
	DEV_DBG("ret path	- %s", mdata->md_retention_path);
	DEV_DBG("data log path	- %s", mdata->md_data_log_path);
	DEV_DBG("mount path	- %s", mdata->md_mnt_path);
	DEV_DBG("parent		- %s", mdata->md_parent_dev_path);
	DEV_DBG("parent volsize	- %lld", mdata->md_parent_vol_size);
	DEV_DBG("par sect size	- %d", mdata->md_parent_sect_size);
	DEV_DBG("snapshot id	- %lld", mdata->md_snapshot_id);
	DEV_DBG("root offset 	- %lld", mdata->md_bt_root_offset);
	DEV_DBG("next_fid		- %d", mdata->md_next_fid);
	DEV_DBG("recovery time	- %lld", mdata->md_recovery_time);
	DEV_DBG("access type	- %d", mdata->md_access_type);
	DEV_DBG("in ret path	- %d", mdata->md_is_map_in_retention_path);
	DEV_DBG("tracking		- %d", mdata->md_is_tracking_enabled);
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

	print_mdata(mdata);	/* debug print */
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
	if ((loff_t) (sector_t) size != size) {
		ERR("size not matching.");
		retval = -EFBIG;
		goto size_mismatch;
	}

	size = size >> 9; /* convert bytes to 512 Sectors */
	DEV_DBG("disk size in sectors - %lld", size);
	DEV_DBG("define vsnap make request function");
	blk_queue_make_request(vdev->vd_request_queue, inm_vs_make_request);
	vdev->vd_request_queue->queuedata = vdev;
	DEV_DBG("set the capacity of disk to %lld sectors", size);
	set_capacity(vdev->vd_disk, (sector_t)size);
	vdev->vd_rw_ops = vs_rw_ops;

	/* Store the complete size in bytes */
	INM_VDEV_SIZE(vdev) = size << 9;

	/*
	 * adds this disk into the kernel list.
	 * also this call (asynchronuously) populates the namespace with the
	 * device name specified through the field disk_name.
	 * ToDo - currently this async activity takes more time and so as a
	 * work-around, the device is also created using mkdev from the user
	 * space as the successful return from this IOCTL path. this could lead
	 * to a problem as we do not stop this async creation too!!
	 */
	DEV_DBG("adding the disk - %s", vdev->vd_disk->disk_name);
	add_disk(vdev->vd_disk);

	/*
	 * create a proc interface
	 */
	retval = inm_vdev_create_proc_entry(vdev->vd_disk->disk_name, vdev);
	if (retval) {
		ERR("proc interface failed");
		goto proc_failed;
	}

  out:
	return retval;

  proc_failed:
	DEV_DBG("delete the gendisk instance");
	del_gendisk(vdev->vd_disk);

  size_mismatch:
	DEV_DBG("uninit vsnap info");
	inm_vs_uninit_vsnap_info(vdev->vd_private);

  vsnapmount_failed:
  invalid_mdata:
	DEV_DBG("put/kill the request queue instance");
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24)
	blk_put_queue(vdev->vd_request_queue);
#else
	blk_cleanup_queue(vdev->vd_request_queue);
#endif
	DEV_DBG("recycle minor");
	put_vs_minor(vdev->vd_disk->first_minor);
	vdev->vd_private = NULL;
	DEV_DBG("put/kill the gendisk instance");
	put_disk(vdev->vd_disk);
	module_put(THIS_MODULE);
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

	INM_INIT_SLEEP(&vdev->vd_sem);

	/*
         * Sleep on vd_sem. Is successful,
	 * it means all pending io is complete
	 */

	INM_ACQ_SPINLOCK_IRQ(&vdev->vd_lock, 0);
	pending = INM_ATOMIC_READ(&vdev->vd_io_pending);
	INM_REL_SPINLOCK_IRQ(&vdev->vd_lock, 0);

	/*
	 * since we have already set VSNAP_FREEING flag,
	 * no new io will be queued. If pending is 0, safely delete
	 * the vsnap else wait for I/O to complete
	 */
	if ( pending )
		INM_SLEEP(&vdev->vd_sem);

	INM_FINI_SLEEP(&vdev->vd_sem);

        inm_vs_uninit_vsnap_info(vd_private);
        /* release proc entry */
        inm_vdev_del_proc_entry(vdev);

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
	inm_32_t retval = 0;

	retval = put_vs_minor(vdev->vd_disk->first_minor);
	if (retval < 0) {
		ERR("put minor failed");
		goto out;
	}

	DEV_DBG("delete the gendisk instance");
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
