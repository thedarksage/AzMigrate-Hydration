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

#ifndef __INM_UTYPES_H_
#define	__INM_UTYPES_H_

#include "inm_types.h"
#include <sys/vnode.h>

struct inm_meminfo {
    inm_u64_t   totalram;
};

typedef int                     inm_cdev_t;
typedef dev_t                   inm_dev_t;
typedef pid_t                   inm_pid_t;
typedef caddr_t                 inm_addr_t;
typedef inm_u32_t		        inm_sysinfo_t;
typedef vnode_t			        inm_filehandle_t;	
typedef struct inm_meminfo      inm_meminfo_t;
typedef int			inm_iodone_t;
typedef struct buf              inm_buf_t;

#endif /* __INM_UTYPES_H_ */
