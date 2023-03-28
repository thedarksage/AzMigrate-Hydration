#ifndef _INMAGE_COMMON_H
#define _INMAGE_COMMON_H

#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/errno.h>
#include <linux/mm.h>
#include <asm/page.h>
#include <asm/mman.h>
#include <asm/atomic.h>
#include <linux/kernel.h>
#include <asm/uaccess.h>
#include <linux/err.h>
#include <linux/spinlock.h>
#include <linux/workqueue.h>
#include <linux/completion.h>
#include <linux/wait.h>
#include <linux/rwsem.h>
#include <linux/device-mapper.h>
#include <linux/moduleparam.h>
#include <linux/stat.h>
#include <linux/sched.h>
#include <linux/limits.h>
#include <linux/jiffies.h>
#include <linux/time.h>
#include <linux/file.h>
#include <linux/errno.h>
#include <linux/major.h>
#include <linux/blkdev.h>
#include <linux/blkpg.h>
#include <linux/swap.h>
#include <linux/slab.h>
#include <linux/suspend.h>
#include <linux/string.h>
#include <linux/writeback.h>
#include <linux/buffer_head.h>
#include <linux/version.h>

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,18)
#include <linux/devfs_fs_kernel.h>
#include <linux/config.h>
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24)
#include <asm/semaphore.h>
#endif

#if LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,35)
#include <linux/smp_lock.h>
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,24)
#include <linux/splice.h>
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,8,13)
#include <linux/kthread.h>
#endif

/* extern data types */
extern struct ioctl_stats_tag inm_proc_global;

/* inmage datatypes */
typedef char			inm_8_t;
typedef unsigned char		inm_u8_t;
typedef short			inm_16_t;
typedef unsigned short		inm_u16_t;
typedef int			inm_32_t;
typedef unsigned int		inm_u32_t;
typedef long long		inm_64_t;
typedef unsigned long long	inm_u64_t;

#include "vdev_mem_debug.h"
#include "linux_list.h"
#include "safecapismajor.h"

#define INLINE inline

#undef UPRINT
#define UPRINT(format, arg...) \
	printk(format "\n", ## arg)

#undef DBG
#ifdef _VSNAP_DEBUG_
#define DBG(format, arg...)						\
	printk(KERN_DEBUG "VDEV[%s:%d]:(DBG) " format "\n",		\
		__FUNCTION__, __LINE__, ## arg)
#else
#define DBG(format, arg...)	/* Do nothing */
#endif

#undef DEV_DBG
#ifdef _VSNAP_DEV_DEBUG_
#define DEV_DBG(format, arg...)						\
	printk(KERN_DEBUG "VDEV[%s:%d]:(TEST) " format "\n",		\
		__FUNCTION__, __LINE__, ## arg)
#else
#define DEV_DBG(format, arg...)	/* Do nothing */
#endif

#undef INFO
#define INFO(format, arg...) 						\
	printk(KERN_INFO "VDEV[%s:%d]:(INF) " format "\n",		\
		__FUNCTION__, __LINE__, ## arg)

#undef ERR
#define ERR(format, arg...) 						\
	printk(KERN_ERR "VDEV[%s:%d]:(ERR) " format "\n", 		\
		__FUNCTION__, __LINE__, ## arg)

#undef ALERT
#define ALERT()		 						\
	printk(KERN_ALERT "VDEV[%s:%d]:(BUG)\n", __FUNCTION__, __LINE__)

/* wrapper around BUG_ON for release build */
#undef ASSERT
#ifdef _VSNAP_DEBUG_
#define ASSERT(EXPR)	BUG_ON(EXPR)
#else
#define ASSERT(EXPR)							\
	{								\
		if (EXPR)						\
			ALERT();					\
	}
#endif

/*
 * wrapper around kmalloc for porting
 * ToDo - enable debugging
 */

#define CHECK_OVERFLOW(size)\
({\
    int ret;\
    unsigned long long tmp = (unsigned long long)size;\
    if(tmp < ((size_t) - 1)){\
        ret = 0;\
    }else {\
        ret = -1;\
    }\
    ret;\
})

#define INM_ALLOC_MEM(size, flag)\
({\
   void *rptr = NULL;\
   if(!CHECK_OVERFLOW(size)) {\
       rptr = kmalloc(size, flag);\
   }\
   rptr;\
})

#define INM_FREE_MEM(p, size, flag)	kfree(p)

#ifdef _VSNAP_DEBUG_
#define INM_ALLOC(size, flag)						\
	inm_verify_alloc(size, flag, __FUNCTION__, __LINE__)
#define INM_ALLOC_WITH_INFO(size, flag, func, line)						\
	inm_verify_alloc(size, flag, func, line)
#else
#define INM_ALLOC(size, flag)		INM_ALLOC_MEM(size, flag)
#endif


/*
 * wrapper around kfree for debugging
 * if double freeing then BUG_ON
 */
#ifdef _VSNAP_DEBUG_
#define VERIFY_DOUBLE_FREE(ptrptr, size, F, L, D) 			\
	do {								\
		if (!(*ptrptr)) {					\
			ERR("extra kfree");				\
			ASSERT(1);					\
		} else { 						\
			inm_verify_for_mem_leak_kfree(*ptrptr, size,	\
							0, F, L, D);	\
			*ptrptr = NULL;					\
		}							\
	} while(0)
#define INM_FREE(pp, size)		\
	VERIFY_DOUBLE_FREE(pp, size, __FUNCTION__, __LINE__, 0)

#define INM_FREE_PINNED(pp, size)					\
	VERIFY_DOUBLE_FREE(pp, size, __FUNCTION__, __LINE__, 0)

#define INM_FREE_VERBOSE(pp, size)	\
	VERIFY_DOUBLE_FREE(pp, size, __FUNCTION__, __LINE__, 1)

#define INM_FREE_WITH_INFO(pp, size, func, line)			\
	VERIFY_DOUBLE_FREE(pp, size, func, line, 0)

#else
#define INM_FREE(pp, size)						\
				do {					\
					INM_FREE_MEM(*pp, size, 0);	\
					*pp = NULL;			\
				} while(0)

#define INM_FREE_PINNED(pp, size)	INM_FREE(pp, size)

#endif

#define INM_MIN(a,b)		((a) < (b))?(a):(b)

#endif	/* _INMAGE_COMMON_H */

