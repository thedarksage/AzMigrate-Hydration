#ifndef _VDEV_DEBUG_H_
#define _VDEV_DEBUG_H_

#include "common.h"

#define VDEV_DBG_MEM 			1
#define VDEV_DBG_BT_CACHE		2
#define VDEV_DBG_SPARSE_LOOKUP		3
#define VDEV_DBG_PROC_DATA		4

struct vdev_debug {
	inm_u32_t 	flag;
	inm_u32_t	num_args;
	void 		*arg;
};

/*
 * Very simple debug info dump interface. The entry point accepts ioctl
 * argument which is of type struct vdev_debug. The flag field specifies
 * the command while the arg file is a memory buffer which comprises of
 * length and string tuples. All values mumeric or alpha is passed as string.
 * It is the job of respective debug routines to extract the info out of the
 * arg based on num/type of args it expects.
 *
 * The following is the expected format of vdev_debug.arg
 *
 *      --------------------------------------------------
 * 	| strlen(1) |    str 1    | strlen(2) |    str 2 .....
 * 	--------------------------------------------------
 */

inm_32_t inm_vsnap_debug_data(inm_ioctl_arg_t);

#endif
