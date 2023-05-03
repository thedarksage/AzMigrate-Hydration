#include "common.h"
#include "vdev_thread_common.h"

/* Thread pool related definitions */
inm_32_t
inm_thread_entry(void *data)
{
 	thread_pool_info_t *thread_pool_info = (thread_pool_info_t *)data;

#if LINUX_VERSION_CODE <  KERNEL_VERSION(3,8,13)
	daemonize(thread_pool_info->tpi_wname);
#endif

	return thread_pool_info->tpi_func(thread_pool_info);
}

INLINE inm_32_t
inm_thread_create(thread_pool_info_t *thread_pool_info)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,8,13)
	struct task_struct *task;
#endif

	DBG("Creating thread");
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,8,13)
	task = kthread_run(inm_thread_entry, thread_pool_info, thread_pool_info->tpi_wname);
	if (IS_ERR(task))
		return -1;

	return task->pid;
#else
	return kernel_thread(inm_thread_entry, thread_pool_info, CLONE_KERNEL);
#endif
}

/* End thread pool */
