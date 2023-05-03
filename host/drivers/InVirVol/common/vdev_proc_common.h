#ifndef _VDEV_PROC_COMMON_H_
#define _VDEV_PROC_COMMON_H_

#include "common.h"
#include "vdev_proc.h"
#include "vdev_mem_debug.h"
#include "vsnap_kernel_helpers.h"

/*
 * This structure is used to maintain the usage/call statistics of every
 * ioctl defined
 */
struct ioctl_stats_tag {
	inm_u32_t	is_create_vsnap_ioctl;
	inm_u32_t	is_delete_vsnap_ioctl;
	inm_u32_t	is_verify_mount_point_ioctl;
	inm_u32_t	is_verify_dev_path_ioctl;
	inm_u64_t	is_update_map_ioctl;
	inm_u32_t	is_num_of_context_per_ret_path_ioctl;
	inm_u32_t	is_many_context_ioctl;
	inm_u32_t	is_one_context_ioctl;
	inm_u32_t	is_create_vv_ioctl;
	inm_u32_t	is_delete_vv_ioctl;
	inm_u32_t	is_delete_vsnap_force_ioctl;
#ifdef _VSNAP_DEBUG_
	char		*is_failure_log;
	inm_u32_t	is_error;
#endif
};

extern struct ioctl_stats_tag inm_proc_global;

/* for easy access of the nested fields */
#define REF_IOCTL_STAT(field)	inm_proc_global.field

#ifdef _VSNAP_DEBUG_
#define INM_PROC_LOG_SIZE	2000
#define INM_PROC_PER_LOG_SIZE	50
#define INM_MAX_LOG		( INM_PROC_LOG_SIZE / INM_PROC_PER_LOG_SIZE )
#define INM_PROC_LOG(n)		( inm_proc_global.is_failure_log + 	\
				(((n - 1) % INM_MAX_LOG ) * 		\
				INM_PROC_PER_LOG_SIZE) )
#endif

inm_32_t proc_read_global_common(char *);
inm_32_t proc_read_vs_context_common(char *, void *);
inm_32_t proc_read_vv_context_common(char *, void *);
inm_32_t inm_vdev_init_proc_common(void);
void inm_vdev_exit_proc_common(void);
void inc_ioctl_stat(inm_32_t);
void print_proc_data(void *);

#ifdef _VSNAP_DEBUG_
void detail_ioctl_stat(inm_32_t ioctl, inm_32_t retval);
#define IOCTL_STATS(ioctl, retval)	detail_ioctl_stat(ioctl, retval)
#else /*_VSNAP_DEBUG_*/
#define IOCTL_STATS(ioctl, retval)	inc_ioctl_stat(ioctl)
#endif /*_VSNAP_DEBUG_*/

/* following functions are expected out of all OS */
int inm_vdev_init_proc(void);
void inm_vdev_exit_proc(void);

#endif
