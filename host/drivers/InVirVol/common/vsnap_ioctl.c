#include "vsnap.h"
#include "vsnap_kernel.h"
#include "vsnap_common.h"
#include "vdev_helper.h"
#include "VsnapShared.h"
#include "vdev_proc_common.h"
#include "vdev_control_common.h"
#include "vsnap_kernel_helpers.h"
#include "disk_btree_cache.h"
#include "vdev_logger.h"

#ifdef _VSNAP_DEBUG_
#include "vdev_debug.h"
#endif

/*
 * create_vsnapdev
 *
 * DESC
 *      process the mount ioctl
 *      the i/p buffer takes the following format -
 *      -------------------------------------------------------------
 *      | device path | device | mount data | space for | space for |
 *      |    len      |  path  |   struct   |  minor #  |  dev_t #  |
 *      -------------------------------------------------------------
 *
 * ALGO
 *      1. retrieve the mount point len & mount point path
 *      2. allocate a vsnap device
 *      3. process the mount_data struct
 *      4. allocate & maintain this instance in the vsnap_dev list
 *      5. return the allocated minor # and calculated dev_t #
 */
static inm_32_t
create_vsnapdev(inm_ioctl_arg_t arg)
{
	char			*buf = NULL;
	char			*dev_path = NULL;
	size_t			dev_path_len;
	inm_32_t		buf_size = 0;
	inm_32_t		retval = 0;
	inm_minor_t		minor = 0;
	inm_32_t		offset = 0;
	inm_vdev_t		*vdev = NULL;
	inm_vs_mdata_t		*mount_info = NULL;
	inm_vdev_list_t		*vdev_list = NULL;
	inm_vdev_list_t		*devpath_entry = NULL;
	inm_u16_t __user	*len;
	dev_t			dev;
	inm_list_head_t		*ptr = NULL;

	len = (inm_u16_t *)arg;
	if (!(dev_path_len = inm_extract_two_bytes(len))) {
		ERR("not able to retrieve the mnt len");
		retval = -EFAULT;
		goto copy_from_user_failed;
	}
	offset = sizeof(*len) + dev_path_len;		/* 16b + dev path len*/
	buf_size = offset + sizeof(inm_vs_mdata_t);	/* + mount data */
	buf_size += sizeof(inm_32_t) + sizeof(dev_t);	/* + minor + dev_t */
	DEV_DBG("buf size - %d", buf_size);
	if (!(buf = inm_retreive_buf(arg, buf_size))) {
		ERR("not able retrieve buffer");
		retval = -EFAULT;
		goto copy_from_user_failed;
	}

	/*
	 * alloc mem for mountpoint list & insert the current entry
	 * this can be allocted after the lvsnap but if mem not avail then
	 * unmount has to be performed. so why not before & avoid later work?
	 */
	vdev_list = INM_ALLOC(sizeof(inm_vdev_list_t), GFP_KERNEL);
	if (!vdev_list) {
		ERR("no mem %d.", (int)(sizeof(inm_vdev_list_t)));
		retval = -ENOMEM;
		goto idlist_alloc_failed;
	}

	dev_path = buf + sizeof(*len);
	vdev_list->vdl_dev_path = inm_retreive_string(dev_path, dev_path_len);
	if ( !vdev_list->vdl_dev_path ) {
		ERR("Could not retrieve dev path");
		retval = -ENOMEM;
		goto idlist_uniqueid_alloc_failed;
	}

	/*
	 * retrieve the mount data information from the user space buf
	 */
	mount_info = INM_ALLOC(sizeof(inm_vs_mdata_t), GFP_KERNEL);
	if (!mount_info) {
		ERR("no mem.");
		retval = -ENOMEM;
		goto vmountinfo_alloc_failed;
	}

	if (memcpy_s(mount_info, sizeof(inm_vs_mdata_t), buf + offset, sizeof(inm_vs_mdata_t))) {
		retval = -EFAULT;
		goto prev_entry_failed;
	}

	DBG("call for create @ dev-path[%s]", vdev_list->vdl_dev_path);

	/*
	 * verify if this is a part of the v-snap device
	 */
	inm_list_for_each(ptr, inm_vs_list_head) {
		devpath_entry = inm_list_entry(ptr, inm_vdev_list_t, vdl_next);
		if (!strcmp(devpath_entry->vdl_dev_path, dev_path)) {
			ERR("found an previous entry");
			retval = -EEXIST;
			goto prev_entry_failed;
		}
	}

	/*
	 * create a virtual device (not yet a vsnap device) and populate the
	 * necessary fields enough to hook into the kernel. after this call
	 * an disk is already present in the kernel list.
	 */
	DEV_DBG("alloc a virtual device instance");
	retval = inm_vs_alloc_dev(&vdev, &minor, vdev_list->vdl_dev_path);
	if (retval < 0) {
		ERR("alloc dev failed.");
		goto alloc_dev_failed;
	}
	DBG("created a device %s", vdev_list->vdl_dev_path);

	vdev->vd_flags = 0;
	INM_SET_BIT(VSNAP_DEVICE, &vdev->vd_flags);

	/*
	 * prepare to return the vsnap device related info (minor & dev_t)
	 * to the user space, i.e., populate the user space buffer.
	 */
	offset = sizeof(*len) + dev_path_len + sizeof(inm_vs_mdata_t);
	INM_ASSIGN(buf + offset, &minor, inm_minor_t);
	offset += sizeof(inm_minor_t);
	dev = CALC_DEV_T(inm_vs_major, minor);
	INM_ASSIGN(buf + offset, &dev, dev_t);
	inm_copy_to_user((void __user *)arg, (void *)buf, buf_size);
 	DBG("Returning major[%d] minor[%d] dev_t[%llu]", inm_vs_major, minor,
	    (inm_u64_t)dev);

	/*
	 * initialize the virtual device based on the info from the user space
	 */
	DEV_DBG("initialize the vsnap device");
	retval = inm_vs_init_dev(vdev, mount_info);
	if (retval < 0) {
		ERR("inm_vs_init_dev failed.");
		goto init_dev_failed;
	} else {
		/*
		 * fill the vdev_list struct & add to the list
		 */
		vdev_list->vdl_vdev = vdev;
		inm_list_add_tail(&vdev_list->vdl_next, inm_vs_list_head);
		DBG("created %s", vdev_list->vdl_dev_path);
		DBG("vdev = %p", vdev);
	}

init_dev_failed:
	if ( retval < 0 )
		INM_FREE_PINNED(&vdev, sizeof(inm_vdev_t));

alloc_dev_failed:
prev_entry_failed:
	INM_FREE(&mount_info, sizeof(inm_vs_mdata_t));

vmountinfo_alloc_failed:
	if (retval < 0)
		INM_FREE(&vdev_list->vdl_dev_path, STRING_MEM(vdev_list->vdl_dev_path));

idlist_uniqueid_alloc_failed:
	if (retval < 0)
		INM_FREE(&vdev_list, sizeof(inm_vdev_list_t));

idlist_alloc_failed:
	INM_FREE(&buf, buf_size);

copy_from_user_failed:
	return retval;
}

/*
 * delete_this_dev
 *
 * DESC
 *      remove this vsnap device struct
 */
static inm_32_t
delete_this_dev(inm_vdev_t *vdev)
{
	inm_32_t retval = 0;

	/* FIXME: Not holding inm_vs_minor_list_lock lock to test flags and refcnt */
	ASSERT(!INM_TEST_BIT(VSNAP_FLAGS_FREEING, &vdev->vd_flags));
	ASSERT(!(vdev->vd_refcnt == 1));

	vdev->vd_refcnt--;
	DEV_DBG("uninit vsnap device");
	retval = inm_vs_uninit_dev(vdev, NULL);
	if (retval) {
		ERR("clear vsnap failed.");
		goto clear_vsnap_failed;
	}
	DEV_DBG("free the device");
	retval = inm_vs_free_dev(vdev);
	if (retval < 0) {
		ERR("free dev failed.");
	}

  clear_vsnap_failed:
	return retval;
}

/*
 * inm_delete_vsnap_bydevid
 *
 * DESC
 *      Delete vsnap structure and remove from the list.
 *      This routine assumes vd_refcnt is  equal to one and
 *      vd_flags is set to VSNAP_FLAGS_FREEING.
 *      This routine may sleep, don't call this routine holding spinlock.
 */
inm_32_t
inm_delete_vsnap_bydevid(inm_vdev_t *vdev)
{
	inm_32_t                retval = 0;
	inm_vdev_list_t         *dev_entry = NULL;
	inm_list_head_t        	*ptr = NULL;
	inm_list_head_t        	*tmp = NULL;


	DEV_DBG("search through the list");
	retval = -ENOENT;

	INM_WRITE_LOCK(&inm_vs_list_rwsem);

	inm_list_for_each_safe (ptr, tmp, inm_vs_list_head) {
		dev_entry = inm_list_entry (ptr, inm_vdev_list_t, vdl_next);
		if (dev_entry->vdl_vdev == vdev) {

			DBG("Deleting %s",dev_entry->vdl_dev_path);
			retval = delete_this_dev (dev_entry->vdl_vdev);
			if (retval) {
				ERR ("remove vsnap failed.");
				goto remove_vsnap_failed;
			}
			inm_list_del (ptr);

			if (dev_entry->vdl_dev_path) {
				INM_FREE (&dev_entry->vdl_dev_path,
					STRING_MEM(dev_entry->vdl_dev_path));
			}
			INM_FREE (&dev_entry, sizeof(inm_vdev_list_t));
			break;
		}
	}

remove_vsnap_failed:
	INM_WRITE_UNLOCK(&inm_vs_list_rwsem);
	return retval;
}

/*
 * delete_vsnap_byname
 *
 * DESC
 *      process the unmount ioctl
 *      the i/p buffer has the following format -
 *      ---------------------------------------------
 *      | mount point/dev path | mount point/device |
 *      |        length        |        path        |
 *      ---------------------------------------------
 *      the i/p buffer currently takes both devpath & mount point path
 *
 * ALGO
 *      1. retrieve the len and the mount point (or device) path
 *      2. run thro' the list of vsnaps and retrieve the vdev struct
 *      3. release the the minor number, gendisk struct & finally the dev
 */
static inm_32_t
delete_vsnap_byname (inm_ioctl_arg_t arg, unsigned long flags)
{
	char			*buf = NULL;
	char			*input_path = NULL;     /* input could be a
							   mount point or even
							   a device path */
	size_t			 mnt_len;
	inm_32_t		retval = 0;
	inm_32_t                buf_size = 0;
	inm_vdev_list_t         *dev_entry = NULL;
	inm_vdev_t              *vdev = NULL;
	inm_u16_t __user        *len;
	inm_list_head_t        	*ptr = NULL;
	inm_list_head_t        	*tmp = NULL;

	len = (inm_u16_t *)arg;
	if (!(mnt_len = inm_extract_two_bytes(len))) {
		ERR("not able to extract the initial bytes");
		retval = -EFAULT;
		goto copy_from_user_failed;
	}
        buf_size = sizeof (*len) + mnt_len;
        DEV_DBG("buf size - %d", buf_size);
        if (!(buf = inm_retreive_buf(arg, buf_size))) {
                ERR("not able retrieve buffer");
                retval = -EFAULT;
                goto copy_from_user_failed;
        }

	input_path = inm_retreive_string(buf + sizeof (*len), mnt_len);
	if ( !input_path ) {
                ERR("no mem");
                retval = -ENOMEM;
                goto uniqueid_alloc_failed;
        }

	DEV_DBG("call for unmount - %s", input_path);

        /*
         * search through the list with mount_point as key & if match found
         * then remove the vsnap instance and then remove the entry
         * the list.
         * as fix for 3830 - the input could be either a mnt path or a dev
         * path. so the search criteria is not either of the paths.
         * This order is important avoid potential bugs
         */
        DEV_DBG("search through the list");
        retval = -ENOENT;
        inm_list_for_each_safe(ptr, tmp, inm_vs_list_head) {
                dev_entry = inm_list_entry(ptr, inm_vdev_list_t, vdl_next);
                if (!strcmp(dev_entry->vdl_dev_path, input_path)) {
                        DBG("found an entry.");
                        vdev = dev_entry->vdl_vdev;
                        INM_ACQ_SPINLOCK(&inm_vs_minor_list_lock);
                        if (vdev->vd_refcnt > 1) {
                                if ( INM_TEST_BIT(INM_VSNAP_DELETE_CANFAIL, &flags) == 0 ) {
                                        /*
                                         * Bug 6456: In NOFAIL we return success and later in
                                         * close to this device will detele the vsnap structure.
                                         * this flag will indicatethe device need to be deteled.
                                         * this flag is tested when the open and IO's are
                                         * attempted on this device.
                                         */
                                        INM_SET_BIT(VSNAP_FLAGS_FREEING, &vdev->vd_flags);
                                        retval = 0;
                                } else {
                                        retval = -EBUSY;
                                } // if-else(test_bit(INM_VSNAP_DELETE_CANFAIL,

                                INM_REL_SPINLOCK(&inm_vs_minor_list_lock);
                        } else {
                                /*
                                 * set flag to indicate the device is going down.
                                 * this flag is tested when the open is attempted on this device.
                                 * this avoids the racing condition between open & delete
                                 */
                                INM_SET_BIT(VSNAP_FLAGS_FREEING, &vdev->vd_flags);
                                INM_REL_SPINLOCK(&inm_vs_minor_list_lock);

                                retval = delete_this_dev(vdev);
                                if (retval) {
                                        ERR("remove vsnap failed.");
                                        goto remove_vsnap_failed;
                                }
                                inm_list_del(ptr);
                                DBG("remove vsnap %s", dev_entry->vdl_dev_path);
                                if (dev_entry->vdl_dev_path) {
                                        INM_FREE(&dev_entry->vdl_dev_path,
					  STRING_MEM(dev_entry->vdl_dev_path));
                                }
                                INM_FREE(&dev_entry, sizeof(inm_vdev_list_t));
                        } // end of if-else (vdev->vd_refcnt > 1)

			break;
                }
        } // inm_list_for_each_safe(ptr, tmp, inm_vs_list_head

  remove_vsnap_failed:
        INM_FREE(&input_path, STRING_MEM(input_path));

  uniqueid_alloc_failed:
        INM_FREE(&buf, buf_size);

  copy_from_user_failed:
        return retval;
}

/*
 * verify_mnt_path
 *
 * DESC
 *      verify if for the given mount point a vsnap device exists
 *      -----------------------------
 *      | mount point | mount point |
 *      |     len     |    path     |
 *      -----------------------------
 */
static inm_32_t
verify_mnt_path(inm_ioctl_arg_t arg)
{
	char			*buf = NULL;
	char			*mnt_path = NULL;
	size_t			mnt_len;
	inm_32_t		retval = -EINVAL;
	inm_32_t		buf_size = 0;
	inm_vdev_list_t		*dev_entry = NULL;
	inm_u16_t __user	*len;
	inm_list_head_t		*ptr = NULL;

	len = (inm_u16_t *)arg;
	if (!(mnt_len = inm_extract_two_bytes(len))) {
		retval = -EFAULT;
		goto copy_from_user_failed;
	}
	buf_size = sizeof (*len) + mnt_len;
	DEV_DBG("buf_size - %d", buf_size);
	if (!(buf = inm_retreive_buf(arg, buf_size))) {
		ERR("not able retrieve buffer");
		retval = -EFAULT;
		goto copy_from_user_failed;
	}

	mnt_path = inm_retreive_string(buf + sizeof (*len), mnt_len);
	if ( !mnt_path ) {
		ERR("no mem");
		retval = -ENOMEM;
		goto uniqueid_alloc_failed;
	}
	DBG("verify virtual vol for %s", mnt_path);

	/*
	 * search through the list with mount_point as key & if match found
	 * then remove the vsnap instance and then remove the entry
	 * the list. This order is important avoid potential bugs
	 */
	inm_list_for_each(ptr, inm_vs_list_head) {
		dev_entry = inm_list_entry(ptr, inm_vdev_list_t, vdl_next);
		if (!strcmp(dev_entry->vdl_dev_path, mnt_path)) {
			DBG("found");
			retval = 0;
			break;
		}
	}
	INM_FREE(&mnt_path, STRING_MEM(mnt_path));

  uniqueid_alloc_failed:
	INM_FREE(&buf, buf_size);

  copy_from_user_failed:
	return retval;
}

/*
 * verify_dev_path
 *
 * DESC
 *      verify if the given dev path is a valid vsnap device path
 *      ---------------------
 *      | dev path | device |
 *      |   len    |  path  |
 *      ---------------------
 */
static inm_32_t
verify_dev_path(inm_ioctl_arg_t arg)
{
	char			*buf = NULL;
	char			*dev_path = NULL;
	size_t			devpath_len;
	inm_32_t		retval = -EINVAL;
	inm_32_t		buf_size = 0;
	inm_vdev_list_t		*devpath_entry = NULL;
	inm_u16_t __user	*len;
	inm_list_head_t		*ptr = NULL;

	len = (inm_u16_t *)arg;
	if (!(devpath_len = inm_extract_two_bytes(len))) {
		retval = -EFAULT;
		goto copy_from_user_failed;
	}
	buf_size = sizeof(*len) + devpath_len;
	DEV_DBG("buf_size - %d", buf_size);
	if (!(buf = inm_retreive_buf(arg, buf_size))) {
		ERR("not able retrieve buffer");
		retval = -EFAULT;
		goto copy_from_user_failed;
	}

	dev_path = inm_retreive_string(buf + sizeof (*len), devpath_len);
	if (!dev_path) {
		ERR("no mem");
		retval = -ENOMEM;
		goto uniqueid_alloc_failed;
	}
	DBG("verify dev path for %s", dev_path);

	/*
	 * search through the list with dev_path as key & if match found
	 * then remove the vsnap instance and then remove the entry
	 * the list. This order is important avoid potential bugs
	 */
	inm_list_for_each(ptr, inm_vs_list_head) {
		devpath_entry = inm_list_entry(ptr, inm_vdev_list_t, vdl_next);
		if (!strcmp(devpath_entry->vdl_dev_path, dev_path)) {
			DBG("found");
			retval = 0;
			break;
		}
	}
	INM_FREE(&dev_path, STRING_MEM(dev_path));

  uniqueid_alloc_failed:
	INM_FREE(&buf, buf_size);

  copy_from_user_failed:
	return retval;
}

/*
 * update_map_for_all_vsnaps
 *
 * DESC
 *      process the update vsnap ioctl
 *      this is invoked when the parent volume has had some writes. the
 *      changes need to be recorded into this vsnap too as the writes
 *      could lead to 2 scenarios -
 *         1. if the retention was enabled then the write will have a CoW
 *            on the ret logs and those details need to be updated in our
 *            btree map and file id table (if needed).
 *         2. if the retention was not enabled then the changes induced by
 *            the write needs to be copied into our write-data file and the
 *            btree map need to be properly updated.
 */
static inm_32_t
update_map_for_all_vsnaps(inm_ioctl_arg_t arg)
{
	char		*buf = NULL;
	inm_32_t	retval = 0;
	inm_32_t	buf_size = 0;
	char __user	*temp = NULL;
	update_input_t	*update_info = NULL;

	buf_size = sizeof(update_input_t);
	temp = (char __user *)arg;

	buf = INM_ALLOC((size_t)buf_size, GFP_KERNEL);
	if (!buf) {
		ERR("no mem.");
		retval = -ENOMEM;
		goto out;
	}

	retval = inm_copy_from_user((void *)buf, (void __user *)temp, buf_size);
	if ( retval < 0 ) {
		ERR("err while copying from user space.");
		retval = -EFAULT;
		goto out;
	}

	update_info = (update_input_t *)buf;
	retval = inm_vs_update_for_parent(update_info->VolOffset,
					  update_info->Length,
					  update_info->FileOffset,
					  update_info->DataFileName,
					  update_info->ParentVolumeName,
					  update_info->Cow);
	if (retval < 0) {
		ERR("update failed.");
	}

out:
	INM_FREE(&buf, buf_size);
	return retval;
}

/*
 * num_of_context_for_ret_path
 *
 * DESC
 *      process the get total # of vsnaps per ret path ioctl
 *      the i/p buffer takes the following format -
 *         --------------------------------
 *         | ret path | retention | total |
 *         |   len    |    path   |  size |
 *         --------------------------------
 *
 *      this ioctl is invoked primarily before the ret path is to be purged
 *      and the purpose of this ioctl is to return the # of vsnaps that are
 *      associated with the given retpath and needs to be purged when the ret
 *      path is being purged.
 *      also typically immediately after this ioct the get_all_context ioctl
 *      should be invoked.
 *
 * ALGO
 *      1. retrieve the ret path length and ret path
 *      2. run thro' the parent volume list & pick the vsnaps related to the
 *          the given ret path
 *      3. calculate & return the total size of contexts of all vsnaps
 */
static inm_32_t
num_of_context_for_ret_path(inm_ioctl_arg_t arg)
{
	char			*buf = NULL;
	char			*ret_path = NULL;
	size_t			ret_path_len = 0;
	inm_32_t		retval = 0;
	inm_32_t		buf_size = 0;
	inm_u32_t		total_size = 0;
	inm_u16_t __user	*len = NULL;
	inm_vs_parent_list_t	*parent_entry = NULL;

	len = (inm_u16_t *)arg;
	if (!(ret_path_len = inm_extract_two_bytes(len))) {
		retval = -EFAULT;
		goto copy_from_user_failed;
	}
	buf_size = sizeof(*len) + ret_path_len + sizeof(inm_u32_t);
	DEV_DBG("buf size - %d", buf_size);
	if (!(buf = inm_retreive_buf(arg, buf_size))) {
		ERR("not able retrieve buf");
		retval = -EFAULT;
		goto copy_from_user_failed;
	}
	ret_path = inm_retreive_string(buf + sizeof (*len), ret_path_len);
	if (!ret_path) {
		ERR("no mem");
		retval = -ENOMEM;
		goto retpath_alloc_failed;
	}
	DBG("ret path - %s", ret_path);

	/*
	 * retrieve the parent volume info for the given retention path
	 */
	parent_entry = inm_vs_parent_for_ret_path(ret_path, ret_path_len);
	if (!parent_entry) {
		total_size = 0;
		DBG("no volumes for this ret_path.");
	} else {
		total_size = inm_vs_num_of_context_for_parent(parent_entry);
		DBG("total size required for all context - %d", total_size);
	}

	/*
	 * total_size remains zero when no vsnap is created
	 */
	*(inm_u32_t *)(buf + sizeof (*len) + ret_path_len) = total_size;
	inm_copy_to_user((void __user *)arg, (void *)buf, buf_size);
	INM_FREE(&ret_path, STRING_MEM(ret_path));

  retpath_alloc_failed:
	INM_FREE(&buf, buf_size);

  copy_from_user_failed:
	return retval;
}

/*
 * fill_context
 *
 * DESC
 *      fill the vol_context from the vsnap info
 */
static void
fill_context(inm_vs_context_t *vol_context_info, inm_vs_info_t *vsnap_info)
{
	DBG("vol_context_info - %p", vol_context_info);
	DBG("vsnap_info - %p", vsnap_info);
	memzero(vol_context_info, sizeof(inm_vs_context_t));
	DBG("\tsnapshot_id - %lld", vsnap_info->vsi_snapshot_id);
	vol_context_info->vc_snapshot_id = vsnap_info->vsi_snapshot_id;
	DBG("\trecovery_time - %lld", vsnap_info->vsi_recovery_time);
	vol_context_info->vc_recovery_time = vsnap_info->vsi_recovery_time;
	DBG("\taccess_type - %d", vsnap_info->vsi_access_type);
	vol_context_info->vc_access_type = vsnap_info->vsi_access_type;
	DBG("\ttracking - %d", vsnap_info->vsi_is_tracking_enabled);
	vol_context_info->vc_is_tracking_enabled =
	    vsnap_info->vsi_is_tracking_enabled;
	DBG("parent_list - %p", vsnap_info->vsi_parent_list);
	DBG("\tdev_path - %s", vsnap_info->vsi_parent_list->pl_devpath);
	strcpy_s(vol_context_info->vc_parent_dev_path, 2048,
	       vsnap_info->vsi_parent_list->pl_devpath);
	DBG("\tlog_path - %s", vsnap_info->vsi_data_log_path);
	strcpy_s(vol_context_info->vc_data_log_path, 2048,
	       vsnap_info->vsi_data_log_path);
	DBG("\tret_path - %s", vsnap_info->vsi_parent_list->pl_retention_path);
	strcpy_s(vol_context_info->vc_retention_path, 2048,
	       vsnap_info->vsi_parent_list->pl_retention_path);
	DBG("\tmnt_path - %s", vsnap_info->vsi_mnt_path);
	strcpy_s(vol_context_info->vc_mnt_path, 2048, vsnap_info->vsi_mnt_path);
}

static inline void
dump_context(inm_vs_context_t *vs_context)
{
	ERR("snapshot id     - %lld", vs_context->vc_snapshot_id);
	ERR("mount point     - %s", vs_context->vc_mnt_path);
	ERR("recovery time   - %lld", vs_context->vc_recovery_time);
	ERR("retention path  - %s", vs_context->vc_retention_path);
	ERR("priv data path  - %s", vs_context->vc_data_log_path);
	ERR("parent vol name - %s", vs_context->vc_parent_dev_path);
}

/*
 * all_context_for_ret_path
 *
 * DESC
 *      process the get many vsnap context at a time given the ret path
 *      the i/p buffer has the following format -
 *         -------------------------------------------------- ..... ------
 *         | ret path | retention | context of | context of |       t of |
 *         |   len    |    path   |   vsnap0   |   vsnap1   |       pn   |
 *         -------------------------------------------------- ..... ------
 *
 *      ideally before this ioctl is invoked the get_total_size ioctl should
 *      have been invoked and the total # of vsnaps avail until then should
 *      be used to allocate the buffer
 *
 *      ToDo: what will happen if in between the two ioctls more vsnaps are
 *            created??
 *
 * ALGO
 *      1. retrieve the ret path length and ret path
 *      2. run thro' the parent volume list & pick the vsnaps related to the
 *          the given ret path
 *      3. return the context of eery vsnap in the parent list
 */
static inm_32_t
all_context_for_ret_path(inm_ioctl_arg_t arg)
{
	char			*buf = NULL;
	char			*ret_path = NULL;
	char			in_char[4];
	size_t			ret_path_len = 0;
	inm_32_t		retval = 0;
	inm_32_t		buf_size = 0;
	inm_32_t		buf_temp_offset = 0;
	inm_u32_t		count = 0;
	inm_u32_t		num_of_vsnaps = 0;
	char __user		*buffer_temp = NULL;
	inm_vs_info_t		*vsnap = NULL;
	inm_u16_t __user	*len = NULL;
	inm_vs_context_t	*vol_context_info = NULL;
	inm_list_head_t		*ptr = NULL;
	inm_list_head_t		*nextptr = NULL;
	inm_vs_parent_list_t	*parent_entry = NULL;

	len = (inm_u16_t *)arg;
	if (!(ret_path_len = inm_extract_two_bytes(len))) {
		retval = -EFAULT;
		goto copy_from_user_failed;
	}
	buf_size = sizeof(*len);	/* retention path len */
	buf_size += ret_path_len;	/* retention path */
	buf_size += sizeof(inm_u32_t);	/* # of context in/out */
	DEV_DBG("buf-size - %d", buf_size);
	if (!(buf = inm_retreive_buf(arg, buf_size))) {
		ERR("not able retrieve buf");
		retval = -EFAULT;
		goto copy_from_user_failed;
	}
	ret_path = inm_retreive_string(buf + sizeof (*len), ret_path_len);
	if (!ret_path) {
		ERR("no mem");
		retval = -ENOMEM;
		goto retpath_alloc_failed;
	}
	DBG("ret path - %s", ret_path);
	num_of_vsnaps = *(uint32_t *)(buf + sizeof(*len) + ret_path_len);
	DBG("# of vsnaps - %d", num_of_vsnaps);

	/*
	 * retrieve the list of parents for this retention path
	 */
	parent_entry = inm_vs_parent_for_ret_path(ret_path, ret_path_len);
	if (!parent_entry) {
		ERR("no volumes for %s.", ret_path);
		retval = -EINVAL;
		goto no_vsnaps_for_ret_path;
	}
	vol_context_info = INM_ALLOC(sizeof(inm_vs_context_t), GFP_KERNEL);
	if (!vol_context_info) {
		ERR ("no mem");
		retval = -ENOMEM;
		goto vol_context_alloc_failed;
	}
	buf_temp_offset += sizeof(*len) + ret_path_len + sizeof(inm_u32_t);
	buffer_temp = (char __user *)arg;

	/*
	 * run through the list of all the vsnaps for this parent
	 * and return the context of all the vsnaps;
	 * also return the # of vsnaps filled if the available # is less than
	 * requrested; otherwise, return the # of vsnaps available in the
	 * system
	 */
	inm_list_for_each_safe(ptr, nextptr, &parent_entry->pl_vsnap_list) {

		/*
		 * count stats @ 0 and 'num_of_vsnaps' starts @ 1 (like math)
		 * i.e., if num_of_vsnaps is 1 then the # of vsnaps it expects
		 * is _only_one_. so, ++count will align count with
		 * num_of_vsnaps and to start @ 1.
		 */
		if ((++count) > num_of_vsnaps) {
			DBG("too many so filling up only %d contexts",
			    num_of_vsnaps);
			break;
		}

		vsnap = inm_list_entry(ptr, inm_vs_info_t, vsi_next);
		DBG("filling up for %s", vsnap->vsi_mnt_path);
		fill_context(vol_context_info, vsnap);
		if (inm_copy_to_user((void __user *)(buffer_temp + buf_temp_offset),
				 (void *)vol_context_info,
				 sizeof(inm_vs_context_t))) {
			ERR("error while copying to user space");
			retval = -EFAULT;
			break;
		}
		buf_temp_offset +=  sizeof(inm_vs_context_t);
	}
	INM_FREE(&vol_context_info, sizeof(inm_vs_context_t));

	/*
	 * if the acutal # of vsnaps in the system can be more than the
	 * requested 'num_of_vsnaps' then return the total number of vsnaps
	 * available in the system (at the present)
	 */
	if (count > num_of_vsnaps) {
		count = inm_vs_num_of_context_for_parent(parent_entry)/
		    sizeof(inm_vs_context_t);
	}
	DBG("# of vsnaps - %d", count);
	*(inm_u32_t *)(in_char) = count;
	inm_copy_to_user((void __user *)(buffer_temp + sizeof(*len) + ret_path_len), (void *)in_char, 4);

  no_vsnaps_for_ret_path:
  vol_context_alloc_failed:
	INM_FREE(&ret_path, STRING_MEM(ret_path));

  retpath_alloc_failed:
	INM_FREE(&buf, buf_size);

  copy_from_user_failed:
	return retval;
}

/*
 * context_for_one_vsnap
 *
 * DESC
 *      retrieve the context of one vsnap.
 *      the i/p buffer has the following format -
 *      -----------------------------------------------------------
 *      | mount point/dev path | mount point/device |  context of |
 *      |        length        |        path        |     vsnap   |
 *      -----------------------------------------------------------
 *      the i/p buffer currently takes both devpath & mount point path
 *
 * ALGO
 *      1. retrieve the devpath (or mountpoint)
 *      2. run the parent volume list and pick the corresponding
 *         vsnap device struct
 *      3. retreive the vsnap context from vsnap info & return the same
 */
static inm_32_t
context_for_one_vsnap(inm_ioctl_arg_t arg)
{
	char			*buf = NULL;
	char			*dev_path = NULL;
	size_t			dev_path_len = 0;
	inm_32_t		retval = 0;
	inm_32_t		buf_size = 0;
	inm_vdev_list_t		*vdev_list = NULL;
	inm_list_head_t		*ptr = NULL;
	inm_u16_t __user	*len = NULL;
	inm_vs_context_t	*vol_context_info = NULL;

	len = (inm_u16_t *)arg;
	if (!(dev_path_len = inm_extract_two_bytes(len))) {
		retval = -EFAULT;
		goto copy_from_user_failed;
	}
	buf_size = sizeof(*len) + dev_path_len + sizeof(inm_vs_context_t);
	DEV_DBG("buf size - %d", buf_size);
	if (!(buf = inm_retreive_buf(arg, buf_size))) {
		ERR("not able retrieve buf");
		retval = -EFAULT;
		goto copy_from_user_failed;
	}
	dev_path = inm_retreive_string(buf + sizeof(*len), dev_path_len);
	if (!dev_path) {
		ERR("no mem");
		retval = -ENOMEM;
		goto devpath_alloc_failed;
	}
	DBG("vol context for %s", dev_path);
	vol_context_info = INM_ALLOC(sizeof(inm_vs_context_t), GFP_KERNEL);
	if (!vol_context_info) {
		ERR("no mem");
		retval = -ENOMEM;
		goto vol_context_alloc_failed;
	}

	/*
	 * search through the list with dev_path as key & if match found
	 * then remove the vsnap instance and then remove the entry
	 * the list. This order is important avoid potential bugs
	 */
	retval = -ENOTDIR;
	inm_list_for_each(ptr, inm_vs_list_head) {
		vdev_list = inm_list_entry(ptr, inm_vdev_list_t, vdl_next);
		if (!strcmp(vdev_list->vdl_dev_path, dev_path)) {
			DBG("found");
			fill_context(vol_context_info,
				     (inm_vs_info_t*)
				     vdev_list->vdl_vdev->vd_private);
			if (memcpy_s(buf + sizeof(*len) + dev_path_len,
				sizeof(inm_vs_context_t), vol_context_info,
				sizeof(inm_vs_context_t))) {
				retval = -EFAULT;
				break;
			}

			retval = 0;
			inm_copy_to_user((void __user *)arg, (void *)buf, buf_size);
			break;
		}
	}
	INM_FREE(&vol_context_info, sizeof(inm_vs_context_t));

  vol_context_alloc_failed:
	INM_FREE(&dev_path, STRING_MEM(dev_path));

  devpath_alloc_failed:
	INM_FREE(&buf, buf_size);

  copy_from_user_failed:
	return retval;
}

/*
 * inm_vv_create_volpack
 *
 * DESC
 *      process the mount ioctl
 *      the i/p buffer takes the following format -
 *      -------------------------------------------------------
 *      | Total | Dev path | Dev path | file path | file path |
 *      |  len  |   len    |          |   len     |           |
 *      -------------------------------------------------------
 *
 * ALGO
 *      1. retrieve the dev path (if any) and the sparse file path
 *      2. validate the sparsefile
 *      3. allocate & initialize a volpack device & return the path
 */
inm_32_t
inm_vv_create_volpack(inm_ioctl_arg_t arg)
{
	char			*buf= NULL;
	char			*dev_path = NULL;
	char			*file_name = NULL;
	void			*temp_file_handle = NULL;
	inm_32_t		retval = 0;
	inm_32_t		name_len = 0;
	inm_32_t		devpath_len = 0;
	inm_32_t		filename_off = 0;
	inm_32_t		devpath_off = 0;
	inm_32_t		buf_size = 0;
	inm_vdev_t		*vdev = NULL;
	struct file		*sparse_file = NULL;
	inm_vdev_list_t		*vdev_list = NULL;
	inm_u16_t __user	*len = (inm_u16_t *)arg;

	if (!(buf_size = inm_extract_two_bytes(len))) {
		retval = -EFAULT;
		goto out;
	}
	buf_size += INM_VOLPACK_NAME_SIZE; /* ToDo: temp value needs to be changed */
	DEV_DBG("buf size - %d", buf_size);
	if (!(buf = inm_retreive_buf(arg, buf_size))) {
		ERR("not able to retrieve buffer");
		retval = -EFAULT;
		goto out;
	}
	INM_ASSIGN(&devpath_len, buf + sizeof(*len), inm_32_t);
	devpath_off = sizeof(*len) + sizeof(inm_32_t);

	/*
	 * upon reboot the devpath is given. we need to re-create this
	 * device and associate the sparse file (again)
	 */
	if (devpath_len) {
		dev_path = inm_retreive_string(buf + devpath_off, devpath_len);
		if (!dev_path) {
			ERR("no mem for %d", devpath_len);
			retval = -ENOMEM;
			goto devpath_alloc_failed;
		}
		DBG("dev path given - %s", dev_path);
	}

	filename_off = devpath_off + devpath_len;
	INM_ASSIGN(&name_len, buf + filename_off, inm_32_t);
	DEV_DBG("name_len = %d", name_len);

	file_name = inm_retreive_string((buf + filename_off + sizeof(inm_32_t)), name_len);
	if ( !file_name ){
		ERR("no mem.");
		retval = -ENOMEM;
		goto filename_alloc_failed;
	}

	DBG("open the sparse file - %s", file_name);
	retval = INM_OPEN_FILE(file_name, INM_RDWR, &temp_file_handle);
	if (retval != 0) {
		ERR("not able to open file [%s]", file_name);
		retval = -EFAULT;
		goto fget_failed;
	}
	sparse_file = (struct file *)temp_file_handle;	/* work around */

	/*
	 * alloc the vdev list entry & parallely start filling in
	 */
	if(!(vdev_list = INM_ALLOC(sizeof(inm_vdev_list_t), GFP_KERNEL))) {
		ERR("no mem");
		retval = -ENOMEM;
		goto vdev_list_alloc_failed;
	}

	vdev_list->vdl_file_path = inm_retreive_string(file_name, name_len);
	if ( !vdev_list->vdl_file_path ) {
		ERR("no mem");
		retval = -ENOMEM;
		goto vdev_mountpoint_alloc_failed;
	}

	/*
	 * allocate space for storing the device name
	 */
	vdev_list->vdl_dev_path = INM_ALLOC(INM_VOLPACK_NAME_SIZE + 1, GFP_KERNEL);
	if (!vdev_list->vdl_dev_path) {
		ERR("no mem");
		retval = -ENOMEM;
		goto vdev_dev_path_alloc_failed;
	}
	if (dev_path) {
		if (memcpy_s(vdev_list->vdl_dev_path, INM_VOLPACK_NAME_SIZE + 1, dev_path,
							strlen(dev_path))) {
			retval = -EFAULT;
			goto alloc_dev_failed;
		}

		vdev_list->vdl_dev_path[strlen(dev_path)] = '\0';
	} else {
		memzero(vdev_list->vdl_dev_path, INM_VOLPACK_NAME_SIZE);
	}

	/*
	 * allocate the device and hook into the kernel
	 */
	DEV_DBG("alloc a virtual device sturcture");
	retval = vv_alloc_dev(&vdev, vdev_list->vdl_dev_path);
	if (retval < 0) {
		ERR("not able to alloc dev");
		retval = -EINVAL;
		goto alloc_dev_failed;
	}
	DBG("created the device %s", vdev_list->vdl_dev_path);

	vdev->vd_flags = 0;
	INM_SET_BIT(VV_DEVICE, &vdev->vd_flags);

	/*
	 * return the device path to the user
	 */
	DEV_DBG("copy back to user space");
	if (memcpy_s(buf + (buf_size - INM_VOLPACK_NAME_SIZE),
			INM_VOLPACK_NAME_SIZE, vdev_list->vdl_dev_path,
			INM_VOLPACK_NAME_SIZE)) {
		retval = -EFAULT;
		vv_free_dev(vdev);
		goto vdev_dev_path_alloc_failed;
	}

	inm_copy_to_user((void __user *)arg, (void *)buf, buf_size);

	/*
	 * create the device & initialize vdev structure
	 */
	DEV_DBG("initialize the volpack device");
	retval = vv_init_dev(vdev, sparse_file, file_name, dev_path);
	if (retval) {
		ERR("vv_init_dev failed");
		vv_free_dev(vdev);
	} else {
		vdev_list->vdl_vdev = vdev;
		inm_list_add_tail(&vdev_list->vdl_next, inm_vv_list_head);
		DBG("created %s with sparse file %s",vdev_list->vdl_dev_path,
		     vdev_list->vdl_file_path);
		retval = 0; /* safe */
	}

  vdev_dev_path_alloc_failed:
	if (retval)
		INM_FREE(&vdev_list->vdl_file_path,
			STRING_MEM(vdev_list->vdl_file_path));

  alloc_dev_failed:
	if (retval)
		INM_FREE(&vdev_list->vdl_dev_path, INM_VOLPACK_NAME_SIZE + 1);

  vdev_mountpoint_alloc_failed:
	if (retval)
		INM_FREE(&vdev_list, sizeof(inm_vdev_list_t));

  vdev_list_alloc_failed:
  fget_failed:
	INM_FREE(&file_name, STRING_MEM(file_name));

  filename_alloc_failed:
	if (dev_path)
		INM_FREE(&dev_path, STRING_MEM(dev_path));

  devpath_alloc_failed:
	INM_FREE(&buf, buf_size);

  out:
	return retval;
}

/*
 * delete_vv_dev
 *
 * DESC
 *      remove the volpack device struct
 */
inm_32_t
delete_vv_dev(inm_vdev_t *vdev)
{
	inm_32_t retval = 0;

	DEV_DBG("uninit the volpack device");
	retval = vv_uninit_dev(vdev);
	if (retval) {
		ERR("clear volpack failed");
		goto out;
	}

	DEV_DBG("free the device structs");
	retval = vv_free_dev(vdev);
	if (retval < 0) {
		ERR("free dev failed");
	}

  out:
	return retval;
}
/*
 * inm_vv_delete_volpack
 *
 * DESC
 *      process the unmount ioctl
 *      the i/p buffer has the following format -
 *         --------------------
 *         | file name | file |
 *         |  length   | path |
 *         --------------------
 *
 * ALGO
 *      1. retrieve the len and the file path
 *      2. run thro' the list of volpack and retrieve the vdev struct
 *      3. release the the minor number, gendisk struct & finally the dev
 */
inm_32_t
inm_vv_delete_volpack(inm_ioctl_arg_t arg)
{
	char			*dev_path = NULL;
	char			*buf = NULL;
	inm_32_t		retval = 0;
	inm_32_t		buf_size = 0;
	inm_32_t		devpath_len;
	inm_vdev_list_t		*vdev_list = NULL;
	inm_u16_t __user	*len;
	inm_list_head_t		*ptr = NULL;
	inm_list_head_t		*tmp = NULL;

	len = (inm_u16_t *)arg;
	if (!(devpath_len = inm_extract_two_bytes(len))) {
		retval = -EFAULT;
		goto copy_from_user_failed;
	}
	buf_size = sizeof (*len) + devpath_len;
	DEV_DBG("buf size - %d", buf_size);
	if (!(buf = inm_retreive_buf(arg, buf_size))) {
		ERR("not able retrieve buffer");
		retval = -EFAULT;
		goto copy_from_user_failed;
	}

	dev_path = inm_retreive_string(buf + sizeof (*len), devpath_len);
	if (!dev_path) {
		ERR("no mem");
		retval = -ENOMEM;
		goto uniqueid_alloc_failed;
	}
	DBG("call for unmount @ devpath[%s]", dev_path);

	/*
	 * search through the list with dev_path as key & if match found
	 * then remove the volpack instance and then remove the entry
	 * the list. This order is important avoid potential bugs
	 */
	retval = -ENODEV;
	inm_list_for_each_safe(ptr, tmp, inm_vv_list_head) {
		vdev_list = inm_list_entry(ptr, inm_vdev_list_t, vdl_next);
		if (!strcmp(vdev_list->vdl_dev_path, dev_path)) {
			DBG("found an entry");
			retval = delete_vv_dev(vdev_list->vdl_vdev);
			if (retval) {
				ERR("remove volpack failed.");
				goto remove_vv_failed;
			}
			inm_list_del(ptr);
			DBG("remove volpack %s with sparse file %s",
			     vdev_list->vdl_dev_path, vdev_list->vdl_file_path);
			if (vdev_list->vdl_file_path)
				INM_FREE(&vdev_list->vdl_file_path,
						STRING_MEM(vdev_list->vdl_file_path));
			if (vdev_list->vdl_dev_path)
				INM_FREE(&vdev_list->vdl_dev_path,
						INM_VOLPACK_NAME_SIZE + 1);
			INM_FREE(&vdev_list, sizeof(inm_vdev_list_t));
			vdev_list = NULL;
		}
	}

  remove_vv_failed:
	INM_FREE(&dev_path, STRING_MEM(dev_path));

  uniqueid_alloc_failed:
	INM_FREE(&buf, buf_size);

  copy_from_user_failed:
	return retval;
}

/*
 * Generic call to get vdev for a given device name. the format for input is
 * 2 bytes name length followed by the actual name.
 */
static inm_32_t
inm_get_proc_data(inm_ioctl_arg_t arg)
{
	char			*buf = NULL;
	char			*input_path = NULL;
	size_t			mnt_len;
	inm_32_t		retval = 0;
	inm_32_t		buf_size = 0;
	inm_vdev_list_t		*dev_entry = NULL;
	inm_u16_t __user	*len;
	inm_list_head_t		*ptr = NULL;
	inm_list_head_t		*tmp = NULL;
	inm_vdev_t		*vdev = NULL;
	inm_interrupt_t		intr;

	if( arg ) {
		len = (inm_u16_t *)arg;
		if (!(mnt_len = inm_extract_two_bytes(len))) {
			ERR("not able to extract the initial bytes");
			retval = -EFAULT;
			goto copy_from_user_failed;
		}

		buf_size = sizeof (*len) + mnt_len;
		DEV_DBG("buf size - %d", buf_size);
		if (!(buf = inm_retreive_buf(arg, buf_size))) {
			ERR("not able retrieve buffer");
			retval = -EFAULT;
			goto copy_from_user_failed;
		}

		input_path = inm_retreive_string(buf + sizeof (*len), mnt_len);
		if (!input_path) {
			ERR("no mem");
			retval = -ENOMEM;
			goto retrieve_string_failed;
		}

		DEV_DBG("search through the list");
		retval = -ENOENT;
		inm_list_for_each_safe(ptr, tmp, inm_vs_list_head) {
			dev_entry = inm_list_entry(ptr, inm_vdev_list_t, vdl_next);
			if (!strcmp(dev_entry->vdl_dev_path, input_path)) {
				DBG("found an entry.");
				vdev = (inm_vdev_t *)dev_entry->vdl_vdev;
				retval = 0;
			}
		}

		INM_FREE(&input_path, STRING_MEM(input_path));

retrieve_string_failed:
		INM_FREE(&buf, buf_size);

	}

copy_from_user_failed:
	if( !retval )
		print_proc_data(vdev);

	return retval;
}

/*
 * IOCTL Handler
 * NULL arg, if not handled by system should be tackled by us
 */
inm_32_t
inm_vdev_ioctl_handler(inm_32_t cmd, inm_ioctl_arg_t arg)
{
	inm_32_t retval = 0;
	unsigned long flags = 0; /* unsigned long used to be
				  * compatible with test_bit  & set_bit*/

	switch(cmd) {
		case IOCTL_INMAGE_ADD_VIRTUAL_VOLUME_FROM_RETENTION_LOG:
			INM_WRITE_LOCK(&inm_vs_list_rwsem);
			retval = create_vsnapdev(arg);
			IOCTL_STATS(cmd, retval);
			INM_WRITE_UNLOCK(&inm_vs_list_rwsem);
			break;

		case IOCTL_INMAGE_REMOVE_VIRTUAL_VOLUME_FROM_RETENTION_LOG:
			INM_SET_BIT(INM_VSNAP_DELETE_CANFAIL, &flags);
			INM_WRITE_LOCK(&inm_vs_list_rwsem);
			retval = delete_vsnap_byname (arg, flags);
			IOCTL_STATS(cmd, retval);
			INM_WRITE_UNLOCK(&inm_vs_list_rwsem);
			break;

		case IOCTL_INMAGE_REMOVE_VIRTUAL_VOLUME_FROM_RETENTION_LOG_NOFAIL:
			INM_CLEAR_BIT(INM_VSNAP_DELETE_CANFAIL, &flags);
			INM_WRITE_LOCK(&inm_vs_list_rwsem);
			retval = delete_vsnap_byname (arg, flags);
			IOCTL_STATS(cmd, retval);
			INM_WRITE_UNLOCK(&inm_vs_list_rwsem);
			break;

		case IOCTL_INMAGE_GET_VOLUME_SYMLINK_FOR_RETENTION_POINT:
			INM_WRITE_LOCK(&inm_vs_list_rwsem);
			retval = verify_mnt_path(arg);
			IOCTL_STATS(cmd, retval);
			INM_WRITE_UNLOCK(&inm_vs_list_rwsem);
			break;

		case IOCTL_INMAGE_GET_VOLUME_DEVICE_ENTRY:
			INM_WRITE_LOCK(&inm_vs_list_rwsem);
			retval = verify_dev_path(arg);
			IOCTL_STATS(cmd, retval);
			INM_WRITE_UNLOCK(&inm_vs_list_rwsem);
			break;

		case IOCTL_INMAGE_UPDATE_VSNAP_VOLUME:
			retval = update_map_for_all_vsnaps(arg);
			IOCTL_STATS(cmd, retval);
			break;

		case IOCTL_INMAGE_GET_VOL_CONTEXT:
			INM_WRITE_LOCK(&inm_vs_list_rwsem);
			retval = num_of_context_for_ret_path(arg);
			IOCTL_STATS(cmd, retval);
			INM_WRITE_UNLOCK(&inm_vs_list_rwsem);
			break;

		case IOCTL_INMAGE_GET_VOLUME_INFORMATION:
			INM_WRITE_LOCK(&inm_vs_list_rwsem);
			retval = all_context_for_ret_path(arg);
			IOCTL_STATS(cmd, retval);
			INM_WRITE_UNLOCK(&inm_vs_list_rwsem);
			break;

		case IOCTL_INMAGE_GET_VOLUME_CONTEXT:
			INM_WRITE_LOCK(&inm_vs_list_rwsem);
			retval = context_for_one_vsnap(arg);
			IOCTL_STATS(cmd, retval);
			INM_WRITE_UNLOCK(&inm_vs_list_rwsem);
			break;

		case IOCTL_INMAGE_ADD_VIRTUAL_VOLUME_FROM_FILE:
			INM_WRITE_LOCK(&inm_vv_list_rwsem);
			retval = inm_vv_create_volpack(arg);
			IOCTL_STATS(cmd, retval);
			INM_WRITE_UNLOCK(&inm_vv_list_rwsem);
			break;

		case IOCTL_INMAGE_REMOVE_VIRTUAL_VOLUME_FROM_FILE:
			INM_WRITE_LOCK(&inm_vv_list_rwsem);
			retval = inm_vv_delete_volpack(arg);
			IOCTL_STATS(cmd, retval);
			INM_WRITE_UNLOCK(&inm_vv_list_rwsem);
			break;

		case IOCTL_INMAGE_DUMP_DATA:
			retval = inm_get_proc_data(arg);
			break;

#ifdef _VSNAP_DEBUG_
		case IOCTL_DUMP_VSNAP_DEBUG_DATA:
			DBG("Dump Data");
			inm_vsnap_debug_data(arg);
			break;
#endif

		default:ERR("Wrong input - %d", (inm_32_t)cmd);
			retval = -EINVAL;
			break;
	}

	DEV_DBG("retval for ioctl %d = %d", cmd, retval);
	return retval;
}
