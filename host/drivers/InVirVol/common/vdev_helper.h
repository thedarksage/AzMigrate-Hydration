#ifndef _VDEV_HELPER_H_
#define _VDEV_HELPER_H_

#include "vsnap.h"

/* list of devices exported by this driver; both vsnap & vv */
typedef struct inm_vdev_list_tag {
        inm_list_head_t        	vdl_next;
        char                    *vdl_file_path;
        char                    *vdl_dev_path;
        inm_vdev_t              *vdl_vdev;
} inm_vdev_list_t;

/*
 * inm_retreive_string
 *
 * DESC
 *      similar to strcpy, just more careful about '\0'
 * 	and return pointer to allocated memory to caller
 */
static inline char *
inm_retreive_string(char *src, size_t length)
{
	size_t		alloc_len = 0;
	char		*dest;

	alloc_len = length;
	/*
	 * If '\0' not in the buffer, allocate additional
	 * byte and put it there ... ugly hack for backward
	 * compatibility
	 */
	if ( src[length - 1] != '\0' )
		alloc_len++;

	dest = INM_ALLOC(alloc_len, GFP_KERNEL);
	if ( !dest ) {
		ERR("no mem %d.", (int)(alloc_len));
		goto out;
	}

	if (memcpy_s(dest, alloc_len - 1, src, alloc_len - 1)) {
		INM_FREE(&dest, alloc_len);
		goto out;
	}

	dest[alloc_len - 1] = '\0';

out:
	return dest;
}

/*
 * WARNING: For copy to/from kernel, linux returns 0 on success &
 * number of bytes remaining for copy while solaris returns 0 or -1
 * so check for 0 for success or failure
 */

/*
 * inm_extract_two_bytes
 *
 * DESC
 *      extract the 1st two bytes from the user buffer
 */
static inline size_t
inm_extract_two_bytes(inm_u16_t __user *len)
{
        char ch_array[10];      /* ToDo - why 10?? */

        if ( inm_copy_from_user(ch_array, len, sizeof(*len)) ) {
                ERR("err while copy_from_user.");
                return 0;
        }

        return *((inm_u16_t *)ch_array);
}

/*
 * inm_retreive_buf
 *
 * DESC
 *      given the buf_size retreive the entire user space input buffer
 */
static inline char*
inm_retreive_buf(inm_ioctl_arg_t arg, inm_32_t buf_size)
{
        char *ker_buf = NULL;
        char __user *user_buf = NULL;

        user_buf = (char *)arg;

	ker_buf = INM_ALLOC((size_t)buf_size, GFP_KERNEL);
        if ( !ker_buf ) {
                ERR("no mem %d.", buf_size);
                goto out;
        }

	if ( inm_copy_from_user(ker_buf, user_buf, (size_t)buf_size) ) {
                ERR("err while copy_from_user %p, size %d",
		    (void *)arg, buf_size);
                INM_FREE(&ker_buf, (size_t)buf_size);
                ker_buf = NULL;
        }

  out:
        return ker_buf;
}

static inline int
STR_COPY_S(char *tgt, size_t tgtmax, const char *src, size_t len)
{
	const char *overlap_sensor;
	int ret;
	int src_high_mem = 0;
	char *tgt_orig = tgt;

	/* validate argument */
	if(tgt == NULL || tgtmax == 0 || len == 0)
		return 1;

	if (src == NULL) {
		ret = 2;
		goto out;
	}

	if (tgt < src) {
		overlap_sensor = src;
		src_high_mem = 1;
	} else {
		overlap_sensor = tgt;
	}

	do {
		if(!len)
			return 0;

		if (!tgtmax) {
			ret = 3;
			goto out;
		}

		if (src_high_mem) {
			if(tgt == overlap_sensor) {
				ret = 4;
				goto out;
			}
		} else {
			if (src == overlap_sensor) {
				ret = 4;
				goto out;
			}
		}

		--tgtmax;
		--len;
	} while(*tgt++ = *src++);

	return 0;

out:
    return ret;
}

#endif
