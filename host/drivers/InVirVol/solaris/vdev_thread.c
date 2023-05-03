#include "common.h"
#include "vdev_thread_common.h"

#ifdef _VSNAP_DEBUG_
#define INLINE
#else
#define INLINE inline
#endif

/* Thread pool related definitions */
void
inm_thread_entry(void *data)
{
 	thread_pool_info_t *thread_pool_info = (thread_pool_info_t *)data;

	thread_pool_info->tpi_func(thread_pool_info);
}

INLINE inm_32_t
inm_thread_create(thread_pool_info_t *thread_pool_info)
{
	DBG("Creating thread");
	thread_create(NULL, 0, inm_thread_entry, thread_pool_info, 0, &p0,
		      TS_RUN, MINCLSYSPRI);
	return 0;
}

inm_32_t
__inm_wait(thread_pool_t* pool)
{      inm_32_t  ret;
        INM_INIT_SLEEP(&(pool)->tp_wait_queue);
        if ((pool)->tp_num_tasks == 0) {
                /* wait when condition value is false */
                INM_SLEEP(&(pool)->tp_wait_queue);
        }
        INM_FINI_SLEEP(&(pool)->tp_wait_queue);
       return ret;
}

/* End thread pool */
