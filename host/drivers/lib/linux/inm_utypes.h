/************************************************************************
 * Copyright 2005 InMage Systems.                                       *
 * This file contains proprietary and confidential information and      *
 * remains the unpublished property of InMage. Use, disclosure,         *
 * or reproduction is prohibited except as permitted by express         *
 * written license aggreement with InMage.                              *
 *                                                                      *
 * File : 
 * Desc :                                                               *
 ************************************************************************/

#ifndef INM_UTYPES_H
#define	INM_UTYPES_H 

#include <linux/wait.h>
#include <linux/completion.h>
#include <linux/list.h>
#include <linux/kobject.h>
#include "inm_types.h"
#include "inm_list.h"

typedef struct cdev             inm_cdev_t;
typedef dev_t                   inm_dev_t;
typedef pid_t                   inm_pid_t;
typedef unsigned long		inm_addr_t;


typedef struct sysinfo		inm_sysinfo_t;
typedef struct file 		inm_filehandle_t;
typedef struct sysinfo 		inm_meminfo_t;
typedef struct bio		inm_buf_t;

struct drv_open{
        const struct block_device_operations *orig_dev_ops;
        struct block_device_operations mod_dev_ops;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,32)
        int (*orig_drv_open)(struct block_device *bdev, fmode_t mode);
#else
        int (*orig_drv_open)(struct inode *inode, struct file *filp);
#endif
        inm_atomic_t    nr_in_flight_ops;
        int status;
};

typedef struct drv_open inm_dev_open_t;

typedef struct _inm_dc_at_lun_info{
        inm_spinlock_t dc_at_lun_list_spn;
        struct inm_list_head dc_at_lun_list;
        inm_dev_open_t dc_at_drv_info;
        inm_u32_t dc_at_lun_info_flag;
}inm_dc_at_lun_info_t;

#endif /* INM_UTYPES_H */
