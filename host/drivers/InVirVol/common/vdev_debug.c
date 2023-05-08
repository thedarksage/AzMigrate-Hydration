#include "vsnap.h"
#include "vdev_helper.h"
#include "vsnap_kernel_helpers.h"
#include "vsnap_kernel.h"
#include "vdev_mem_debug.h"
#include "disk_btree_cache.h"
#include "vdev_debug.h"

#ifdef _VSNAP_DEBUG_

/*
 * Argument extraction helpers
 */

/* Get numeric values from strings ... user makes sure no overflow */
inm_u64_t
inm_strtoull(char *str)
{
	inm_u64_t retval = 0;

	while ( *str ) {
		retval *= 10;
		retval += *str - '0';
		str++;
	}

	return retval;

}

inm_u32_t
inm_strtoui(char *str)
{
	inm_u32_t retval = 0;

	while ( *str ) {
		retval *= 10;
		retval += *str - '0';
		str++;
	}

	return retval;

}

/*
 * Returns pointer to argmuent argidx in the arg stream
 */
void *
get_actual_arg_ptr(void *arg, inm_32_t argidx)
{
	size_t	str_len;
	char *retptr = arg;

	DBG("Getting argument %d in stream at %p", argidx, arg);
	/* first arg is at 0 and so on ... */
	argidx--;

	while( argidx ) {
		if (!(str_len = inm_extract_two_bytes((inm_u16_t *)retptr))) {
			ERR("not able to extract the initial bytes");
			retptr = NULL;
			goto out;
		}

		retptr += str_len + sizeof(inm_u16_t);

		argidx--;
	}

out:
	return retptr;
}

/*
 * Returns a valid string extracted from the arguments
 */
static char *
inm_get_arg(void *arg, inm_u32_t argidx)
{
	inm_u16_t __user 	*len;
	size_t 			str_len;
	inm_32_t		buf_size = 0;
	char			*string = NULL;
	char			*buf;

	if( !arg )
		return NULL;

	arg = get_actual_arg_ptr(arg, argidx);
	if( !arg )
		return NULL;

	/* get the string length */
	len = (inm_u16_t *)arg;
	if (!(str_len = inm_extract_two_bytes(len))) {
		ERR("not able to extract the initial bytes");
		goto out;
	}

	/*
	 * Read the string into memory. This may or may not be NULL terminated.
	 * inm_retreive_string should be called to get a proper string
	 */
	buf_size = sizeof (*len) + str_len;
	DEV_DBG("buf size - %d", buf_size);
	if ( !(buf = inm_retreive_buf((inm_ioctl_arg_t)arg, buf_size)) ) {
		ERR("not able retrieve buffer");
		goto out;
	}

	string = inm_retreive_string(buf + sizeof (*len), str_len);
	if ( !string ) {
		ERR("no mem");
		goto retrieve_string_failed;
	}

retrieve_string_failed:
	INM_FREE(&buf, buf_size);

out:
	return string;
}

/*
 * Returns vdev of the given vsnap/volpack path
 * params:
 *	arg: 	arg ptr
 *	type:	vsnap/volpack
 * 	argidx:	argument index in stream
 */
static inm_vdev_t *
inm_get_vdev(void *arg, inm_u32_t type, inm_u32_t argidx)
{
	inm_vdev_t 	*vdev = NULL;
	char 		*input_path;
	inm_vdev_list_t		*dev_entry = NULL;
	inm_list_head_t	*ptr = NULL;
	inm_list_head_t	*tmp = NULL;
	inm_list_head_t	*head;

	input_path = inm_get_arg(arg, argidx);
	if( !input_path ) {
		ERR("Cannot get device name");
		goto out;
	}

	if( type == VSNAP_DEVICE )
		head = inm_vs_list_head;
	else
		head = inm_vv_list_head;

	DEV_DBG("search through the list");
	inm_list_for_each_safe(ptr, tmp, head) {
		dev_entry = inm_list_entry(ptr, inm_vdev_list_t, vdl_next);
		if (!strcmp(dev_entry->vdl_dev_path, input_path)) {
			DBG("found an entry.");
			vdev = (inm_vdev_t *)dev_entry->vdl_vdev;
		}
	}

	INM_FREE(&input_path, STRING_MEM(input_path));

out:
	return vdev;

}

/*
 * Debug routines
 */
static inm_32_t
validate_btree_cache(inm_u32_t num_args, void *arg)
{
	inm_vdev_t *vdev;
	inm_32_t retval = 0;

	if( num_args != 1 ) {
		ERR("Invalid num of args");
		retval = -EINVAL;
		goto out;
	}

	vdev = inm_get_vdev(arg, VSNAP_DEVICE, 1);
	if( !vdev ) {
		ERR("Cannot get vdev");
		goto out;
	}

	inm_validate_btree_cache(vdev);
out:
	return retval;
}

static inm_32_t
lookup_sparse_cache(inm_u32_t num_args, void *arg)
{
	inm_32_t retval = 0;
	char *file_name;
	inm_vdev_t *vdev;
	inm_vs_info_t *vs_info;
	void *next_arg;

	if( num_args != 2 ) {
		ERR("Invalid number of args");
		retval = -EINVAL;
		goto out;
	}

	vdev = inm_get_vdev(arg, VSNAP_DEVICE, 1);
	if( !vdev ) {
		ERR("Cannot get vdev");
		retval = -EINVAL;
		goto out;
	}

	file_name = inm_get_arg(arg, 2);
	if( !file_name ) {
		ERR("Cannot get file name");
		retval = -EINVAL;
		goto out;
	}

	vs_info = (inm_vs_info_t *)vdev->vd_private;
	inm_vs_lookup_file_fid_sparse(vs_info, file_name);

	INM_FREE(&file_name, STRING_MEM(file_name));

out:
	return retval;
}

static inm_32_t
inm_vdev_proc_data(inm_u32_t num_args, void *arg)
{
	inm_vdev_t *vdev;
	inm_32_t retval = 0;

	if( num_args == 1 ) {
		DBG("Get proc info for device");
		vdev = inm_get_vdev(arg, VSNAP_DEVICE, 1);
		if( !vdev ) {
			vdev = inm_get_vdev(arg, VV_DEVICE, 1);
			if( !vdev ) {
				ERR("Cannot get vdev");
				retval = -ENODEV;
				goto out;
			}
		}
	} else {
		if( !num_args ) {
			vdev = NULL;
		} else {
			ERR("Invalid num of args");
			retval = -EINVAL;
			goto out;
		}
	}

	print_proc_data(vdev);

out:
	return retval;
}

inm_32_t
inm_vsnap_debug_data(inm_ioctl_arg_t arg)
{
	struct vdev_debug dbg;
	inm_32_t retval = 0;

	inm_copy_from_user(&dbg, (void __user *)arg, sizeof(struct vdev_debug));

	DBG("Command = %d", dbg.flag);

	switch(dbg.flag) {
		case VDEV_DBG_MEM:	DBG("Mem List");
					dump_memory_list();
					break;

		case VDEV_DBG_BT_CACHE: DBG("Validate Btree Cache");
					retval =
					      validate_btree_cache(dbg.num_args,
								   dbg.arg);
					break;

		case VDEV_DBG_SPARSE_LOOKUP:
					DBG("Lookup Sparse Cache");
					retval =
					       lookup_sparse_cache(dbg.num_args,
								   dbg.arg);
					break;

		case VDEV_DBG_PROC_DATA:
					DBG("Proc Data");
					retval =
						inm_vdev_proc_data(dbg.num_args,
								   dbg.arg);
					break;
	}

	return retval;

}

#endif
