#include "common.h"
#include "vdev_thread_common.h"

#include <sys/time.h>
#include <sys/resource.h>

#ifdef _VSNAP_DEBUG_
#define INLINE
#else
#define INLINE inline
#endif

/* Thread pool related definitions */
static inm_32_t
thread_entry_point(inm_32_t flag, void *data, inm_32_t datalen)
{
	thread_pool_info_t *tp_info = *(thread_pool_info_t **)data;

	tp_info->tpi_func(tp_info);
	return 0;
}

INLINE inm_32_t
inm_thread_create(thread_pool_info_t *tp_info)
{
	pid_t pid;
	inm_32_t retval = 0;

	DEV_DBG("Thread Pool @ %p", tp_info);
	pid = creatp();
	if ( pid == -1 ) {
		retval = -ENOMEM;
		goto out;
	}

	retval = initp(pid, thread_entry_point, (char *)&tp_info,
		       sizeof(thread_pool_info_t *), tp_info->tpi_wname);
	if( retval )
		retval = -retval; 

out:
	return retval;

}


inm_32_t
__inm_wait(thread_pool_t* pool)
{
	inm_32_t  ret = 0;
        INM_INIT_SLEEP(&(pool)->tp_wait_queue);
        if ((pool)->tp_num_tasks == 0) {
                /* wait when condition value is false */
                INM_SLEEP(&(pool)->tp_wait_queue);
        }
        INM_FINI_SLEEP(&(pool)->tp_wait_queue);
       return ret;
}

void
inm_set_ulimit_to_infinity()
{
	struct rlimit64 rlp;

	rlp.rlim_cur = RLIM64_INFINITY;
	rlp.rlim_max = RLIM64_INFINITY;
	setrlimit(RLIMIT_FSIZE, &rlp);
}

/* End thread pool */
