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

#ifndef INM_TYPES_H
#define	INM_TYPES_H 

#ifdef __KERNEL__
#include <linux/types.h>
#else
#include <stdint.h>
#endif
#include <linux/version.h>


/* signed types */
typedef long long               inm_sll64_t;
typedef int64_t                 inm_s64_t;
typedef int32_t                 inm_s32_t;
typedef int16_t                 inm_s16_t;
typedef int8_t                  inm_s8_t;
typedef char                    inm_schar;

/* unsigned types */
typedef unsigned long long      inm_ull64_t;
typedef uint64_t                inm_u64_t;
typedef uint32_t                inm_u32_t;
typedef uint16_t                inm_u16_t;
typedef uint8_t                 inm_u8_t;
typedef unsigned char           inm_uchar;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,24)
typedef void                    inm_iodone_t;
#else
typedef int			inm_iodone_t;
#endif

#endif /* INM_TYPES_H */
