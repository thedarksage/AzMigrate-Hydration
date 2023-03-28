#ifndef _INMAGE_COMMON_H
#define _INMAGE_COMMON_H

#include <sys/types.h>
#include <sys/errno.h>
#include <sys/malloc.h>
#include <sys/syslog.h>
#include <sys/lock_def.h>
#include <sys/buf.h>
#include <sys/uio.h>
#include <sys/device.h>
#include <sys/atomic_op.h>
#include <sys/lock_alloc.h>
#include <sys/thread.h>
#include <sys/sleep.h>
#include <sys/vnode.h>
#include <sys/fp_io.h>
#include <sys/proc.h>
#include <sys/uprintf.h>
#include <sys/errids.h>
#include <sys/ioctl.h>
#include <sys/devinfo.h>
#include <sys/sysmacros.h>
#include <sys/time.h>
#include <sys/adspace.h>
#include <sys/vmuser.h>

/* inmage datatypes */
typedef char			inm_8_t;
typedef unsigned char		inm_u8_t;
typedef short			inm_16_t;
typedef unsigned short		inm_u16_t;
typedef int			inm_32_t;
typedef unsigned int		inm_u32_t;
typedef long long		inm_64_t;
typedef unsigned long long	inm_u64_t;

#ifndef __user
#define __user
#endif

#include "linux_list.h"
#include "vdev_mem_debug.h"
#include "vdev_logger.h"
#include "safecapismajor.h"

#undef INLINE
#ifdef _VSNAP_DEBUG_
#define INLINE
#else
#define INLINE inline
#endif

#undef UPRINT
#define UPRINT(format, arg...) 	uprintf(format "\n", ## arg)

#undef DBG
#ifdef _VSNAP_DEBUG_
#define DBG(format, arg...)						       \
	do {								       \
		INM_ACQ_MUTEX(&logstr_mutex);				       \
		sprintf(__logstr__, "%llu: VDEV[%s:%d]:(DBG) " format "\n",    \
			(inm_u64_t)time, __FUNCTION__, __LINE__, ## arg);      \
		inm_log(INM_LOG_DBG, __logstr__);				       \
	} while(0)

#else
#define DBG(format, arg...)	/* Do nothing */
#endif

#undef DEV_DBG
#ifdef _VSNAP_DEV_DEBUG_
#define DEV_DBG(format, arg...)						       \
	do {								       \
		INM_ACQ_MUTEX(&logstr_mutex);				       \
		sprintf(__logstr__, "%llu: VDEV[%s:%d]:(TEST) " format "\n",   \
			(inm_u64_t)time, __FUNCTION__, __LINE__, ## arg);      \
		inm_log(INM_LOG_DEV_DBG, __logstr__);				       \
	} while(0)
#else
#define DEV_DBG(format, arg...)	/* Do nothing */
#endif

#undef INFO
#define INFO(format, arg...)						       \
	do {								       \
		INM_ACQ_MUTEX(&logstr_mutex);				       \
		sprintf(__logstr__, "%llu: VDEV[%s:%d]:(INFO) " format "\n",   \
			(inm_u64_t)time, __FUNCTION__, __LINE__, ## arg);      \
		inm_log(INM_LOG_INFO, __logstr__);				       \
	} while(0)


#undef ERR
#define ERR(format, arg...)						       \
	do {								       \
		INM_ACQ_MUTEX(&logstr_mutex);				       \
		sprintf(__logstr__, "%llu: VDEV[%s:%d]:(ERR) " format "\n",    \
			(inm_u64_t)time, __FUNCTION__, __LINE__, ## arg);      \
		inm_log(INM_LOG_ERR, __logstr__);				       \
	} while(0)

#undef ALERT
#ifdef _VSNAP_DEBUG_
#define ALERT()								       \
	do {							       	       \
		ERR("Vsnap Driver Bug[%s:%d]", __FUNCTION__, __LINE__);        \
		inm_sync_log();						       \
		INM_ACQ_MUTEX(&logstr_mutex);				       \
		sprintf(__logstr__, "Vsnap Driver Bug[%s:%d]",		       \
			__FUNCTION__, __LINE__);      			       \
		panic(__logstr__);					       \
	} while(0)

#else
#define ALERT()		ERR("Vsnap Driver Bug[%s:%d]", __FUNCTION__, __LINE__)
#endif

/* wrapper around BUG_ON for release build */
#undef ASSERT
#define ASSERT(EXPR)			\
	{				\
		if (EXPR) {		\
			ALERT(); 	\
		}			\
	}

/*
 * wrapper around xmalloc for porting
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

#define INM_ALLOC_MEM(size, heap)\
({\
    void *rptr = NULL;\
    if(!CHECK_OVERFLOW(size)) {\
        rptr = (void *)xmalloc(size, 0, heap);\
    }\
    rptr;\
})

#define INM_FREE_MEM(p, size, heap)		xmfree(p, heap);

#ifdef _VSNAP_DEBUG_
#define INM_ALLOC(size, heap)						\
	inm_verify_alloc(size, heap, __FUNCTION__, __LINE__)
#define INM_ALLOC_WITH_INFO(size, heap, func, line)                     \
        inm_verify_alloc(size, heap, func, line)
#else
#define INM_ALLOC(size, heap)			INM_ALLOC_MEM(size, heap)
#endif


/*
 * wrapper around kfree for debugging
 * if double freeing then BUG_ON
 */
#ifdef _VSNAP_DEBUG_

#define VERIFY_DOUBLE_FREE(ptrptr, size, heap, F, L, D)			\
	do {								\
		if (!(*ptrptr)) { 					\
			ERR("extra kfree");				\
			ASSERT(1);					\
		} else { 						\
			inm_verify_for_mem_leak_kfree(*ptrptr, size,	\
							heap, F, L, D);	\
			*ptrptr = NULL; 				\
		}							\
	} while(0)

#define INM_FREE(pp, size)						\
	VERIFY_DOUBLE_FREE(pp, size, kernel_heap, __FUNCTION__, __LINE__, 0)

#define INM_FREE_PINNED(pp, size)					\
	VERIFY_DOUBLE_FREE(pp, size, pinned_heap, __FUNCTION__, __LINE__, 0)

#define INM_FREE_VERBOSE(pp, size)      				\
        VERIFY_DOUBLE_FREE(pp, size, kernel_heap, __FUNCTION__, __LINE__, 1)

#define INM_FREE_WITH_INFO(pp, size, func, line)		\
        VERIFY_DOUBLE_FREE(pp, size, kernel_heap, func, line, 0)

#else

#define INM_FREE(pp, size)						\
			do {						\
				INM_FREE_MEM(*pp, size, kernel_heap);	\
				*pp = NULL;				\
			} while(0)

#define INM_FREE_PINNED(pp, size)					\
			do {						\
				INM_FREE_MEM(*pp, size, pinned_heap);	\
				*pp = NULL;				\
			} while(0)

#endif

#define INM_MIN(a,b)		((a) < (b))?(a):(b)

#endif	/* _INMAGE_COMMON_H */

