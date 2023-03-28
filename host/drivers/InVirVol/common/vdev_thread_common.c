#include "vdev_thread_common.h"
#include "vsnap_io_common.h"
#include "vsnap_kernel.h"

thread_pool_t		*vs_ops_pool;
thread_pool_info_t	*vs_ops_pool_info;

thread_pool_t		*vs_update_pool;
thread_pool_info_t	*vs_update_pool_info;

thread_pool_t		*vv_ops_pool;
thread_pool_info_t	*vv_ops_pool_info;

inm_32_t
vs_ops_worker_thread(void *data)
{
	inm_32_t		retval;
	vsnap_task_t		*task;
	vsnap_task_quit_t 	*quit_task;

	thread_pool_info_t 	*tp_info = data;
#ifdef _VSNAP_DEV_DEBUG_
	inm_32_t		thread_id =
				INM_ATOMIC_READ(&tp_info->tpi_num_threads);
#endif

	/* In AIX, the ulimit for these threads (processes) should
	 * be raised to infinity. The default value for file size is
	 * 1GB. So in order to process reads/writes beyond 1GB, this
	 * function will raise the file size limit to infinity.
	 *
	 * Refer to the bug #15591 for more details.
	 */
	inm_set_ulimit_to_infinity();

	DEV_DBG("Started new thread");
	INM_READ_LOCK(&tp_info->tpi_rwlock);
	INM_ATOMIC_INC(&tp_info->tpi_num_threads);

	for (;;) {

		wait:
		retval = WAIT_FOR_NEXT_TASK(tp_info->tpi_thread_pool);
		if ( retval < 0 ) {
			ERR("Cannot init wait on lookup pool");
			return retval;
		}

		/*
	 	* Its here means there is a task that needs processing
	 	*/
		GET_TASK_FROM_POOL(tp_info->tpi_thread_pool, task);

		if (!task )
			goto wait;

		switch( task->vst_task_id ) {
			case INM_TASK_BTREE_LOOKUP:
					DEV_DBG("%d: Handle BTree Lookup",
						thread_id);
					handle_bio_for_read_from_vsnap(task);
					break;

			case INM_TASK_RETENTION_IO:
					DEV_DBG("%d: Reading from retention",
						thread_id);
					inm_vs_read_from_retention_file(task);
					break;

			case INM_TASK_WRITE:
					DEV_DBG("%d: Write", thread_id);
					handle_bio_for_vsnap_write_common(task);
					break;

			case INM_TASK_KILL_THREAD:
					DEV_DBG("%d: Killing thread", thread_id);
					INM_ATOMIC_DEC(&tp_info->tpi_num_threads);
                    /* Put relevant data needed for kill */
					quit_task = task->vst_task_data;
                    INM_SET_KILL_INFO(quit_task); 
					INM_WAKEUP(&quit_task->vstq_ipc);
					goto out;

			default:
				DBG("Unknown task");
				ALERT();
		}
	}

out:
	return 0;

}

inm_32_t
vv_ops_worker_thread(void *data)
{
	inm_32_t		retval;
	vsnap_task_t		*task;
	vsnap_task_quit_t 	*quit_task;
	thread_pool_info_t 	*tp_info = data;
#ifdef _VSNAP_DEV_DEBUG_
	inm_32_t		thread_id =
				INM_ATOMIC_READ(&tp_info->tpi_num_threads);
#endif

	/* In AIX, the ulimit for these threads (processes) should
	 * be raised to infinity. The default value for file size is
	 * 1GB. So in order to process reads/writes beyond 1GB, this
	 * function will raise the file size limit to infinity.
	 *
	 * Refer to the bug #15591 for more details.
	 */
	inm_set_ulimit_to_infinity();

	DEV_DBG("Started new thread");
	INM_READ_LOCK(&tp_info->tpi_rwlock);
	INM_ATOMIC_INC(&tp_info->tpi_num_threads);

	for (;;) {

		wait:
		retval = WAIT_FOR_NEXT_TASK(tp_info->tpi_thread_pool);
		if ( retval < 0 ) {
			ERR("Cannot init wait on lookup pool");
			return retval;
		}

		/*
	 	* Its here means there is a task that needs processing
	 	*/
		GET_TASK_FROM_POOL(tp_info->tpi_thread_pool, task);

		if (!task )
			goto wait;

		switch( task->vst_task_id ) {
			case INM_TASK_VOLPACK_IO:
					DEV_DBG("%d: Volpack I/O", thread_id);
					inm_vv_do_volpack_io(task);
					break;

			case INM_TASK_KILL_THREAD:
					DEV_DBG("Killing thread");
					INM_ATOMIC_DEC(&tp_info->tpi_num_threads);
                    /* Put relevant data needed for kill */
					quit_task = task->vst_task_data;
                    INM_SET_KILL_INFO(quit_task); 
					INM_WAKEUP(&quit_task->vstq_ipc);
					goto out;

			default:
				DBG("Unknown task");
				ALERT();
		}
	}

out:
	return 0;

}

inm_32_t
vs_update_worker_thread(void *data)
{
	inm_32_t		retval;
	vsnap_task_t		*task;
	vsnap_task_quit_t	*quit_task;
	thread_pool_info_t 	*tp_info = data;
#ifdef _VSNAP_DEV_DEBUG_
	inm_32_t		thread_id =
				INM_ATOMIC_READ(&tp_info->tpi_num_threads);
#endif

	DEV_DBG("Started new thread");
	INM_READ_LOCK(&tp_info->tpi_rwlock);
	INM_ATOMIC_INC(&tp_info->tpi_num_threads);

	for (;;) {

		wait:
		retval = WAIT_FOR_NEXT_TASK(tp_info->tpi_thread_pool);
		if ( retval < 0 ) {
			ERR("Cannot init wait on lookup pool");
			return retval;
		}

		/*
	 	* Its here means there is a task that needs processing
	 	*/
		GET_TASK_FROM_POOL(tp_info->tpi_thread_pool, task);

		if (!task )
			goto wait;

		switch( task->vst_task_id ) {

			case INM_TASK_UPDATE_VSNAP:
					DEV_DBG("%d: Updating vsnap", thread_id);
					inm_vs_update_vsnap(task);
					DEV_DBG("%d: Update Done", thread_id);
					break;

			case INM_TASK_KILL_THREAD:
					DEV_DBG("Killing thread");
					INM_ATOMIC_DEC(&tp_info->tpi_num_threads);
                    /* Put relevant data needed for kill */
					quit_task = task->vst_task_data;
                    INM_SET_KILL_INFO(quit_task); 
					INM_WAKEUP(&quit_task->vstq_ipc);
					goto out;

			default:
				DBG("Unknown task");
				ALERT();
		}
	}

out:
	return 0;

}

static thread_pool_info_t *
init_one_thread_pool(inm_u32_t num_threads, thread_func_t worker_thread_func,
		     char *worker_name)
{
	thread_pool_t		*pool = NULL;
	thread_pool_info_t	*pool_info = NULL;
	inm_32_t		fail_threads = 0;
	inm_32_t		i;

	DEV_DBG("Init Thread Pool");
	DBG("Creating %u %s threads", num_threads, worker_name);

	pool = INM_ALLOC(sizeof(thread_pool_t), GFP_KERNEL);
	if ( !pool )
		goto out;

	pool_info = INM_ALLOC(sizeof(thread_pool_info_t), GFP_KERNEL);
	if ( !pool_info ) {
		INM_FREE(&pool, sizeof(thread_pool_t));
		goto out;
	}

	pool_info->tpi_thread_pool = pool;
	INM_ATOMIC_SET(&pool_info->tpi_num_threads, 0);
	INM_INIT_LOCK(&pool_info->tpi_rwlock);
	pool_info->tpi_func = worker_thread_func;
	if (strncpy_s(pool_info->tpi_wname, INM_WORKER_NAME_SIZE, worker_name,
						INM_WORKER_NAME_SIZE)) {
		INM_FREE(&pool, sizeof(thread_pool_t));
		INM_FREE(&pool_info, sizeof(thread_pool_info_t));
		goto out;
	}

	INM_INIT_WAITQUEUE_HEAD(&pool->tp_wait_queue);
	INM_INIT_SPINLOCK(&pool->tp_lock);
	INM_INIT_LIST_HEAD(&pool->tp_task_list_head);
	pool->tp_num_tasks = 0;

	for( i = 0; i < num_threads; i++ ) {
		if ( inm_thread_create(pool_info) < 0 )
			fail_threads++;
	}

	if ( num_threads && fail_threads == num_threads ) {
		ERR("Could not create worker threads");
		/* retval is already set since last create failed */
		INM_FREE(&pool, sizeof(thread_pool_t));
		INM_FREE(&pool_info, sizeof(thread_pool_info_t));
		pool_info = NULL;
	} else {
		/* If some threads are created ... operate in degraded mode */
		if ( fail_threads ) {
			ERR("Could not created required num of threads"
			    " ... performance may be degraded");
		}
	}

out:
	return pool_info;
}


inm_32_t
init_thread_pool(void)
{
	inm_32_t	retval = 0;

	DEV_DBG("Init Thread Pool");

	/* Update Pool - Create remaining threads later */
	vs_update_pool_info = init_one_thread_pool(1,
						   vs_update_worker_thread,
						   "inm_vs_update");
	if ( !vs_update_pool_info ) {
		retval = -ENOMEM;
		goto out;
	}
	vs_update_pool = vs_update_pool_info->tpi_thread_pool;

	/* Vsnap pool */
	vs_ops_pool_info = init_one_thread_pool(INM_VS_NUM_THREADS,
						vs_ops_worker_thread,
						"inm_vs_worker");
	if ( !vs_ops_pool_info ) {
		retval = -ENOMEM;
		goto out;
	}
	vs_ops_pool = vs_ops_pool_info->tpi_thread_pool;

	/* Volpack Pool */
	vv_ops_pool_info = init_one_thread_pool(INM_VV_NUM_THREADS,
						vv_ops_worker_thread,
						"inm_vv_worker");
	if ( !vv_ops_pool_info ) {
		retval = -ENOMEM;
		goto out;
	}
	vv_ops_pool = vv_ops_pool_info->tpi_thread_pool;

out:
	return retval;

}

void
inm_kill_thread(thread_pool_info_t *tp_info)
{
	thread_pool_t		*tpool = tp_info->tpi_thread_pool;
	vsnap_task_t		*task = NULL;
	vsnap_task_quit_t	*quit_task = NULL;

	quit_task = INM_ALLOC(sizeof(vsnap_task_quit_t), GFP_KERNEL);
	if ( !quit_task ) {
		ERR("No mem for task ... cant delete thread");
		return;
	}

	INM_INIT_IPC(&quit_task->vstq_ipc);
	
    task = &quit_task->vstq_task;
	task->vst_task_id = INM_TASK_KILL_THREAD;
    task->vst_task_data = quit_task;

    /* Send the kill task and wait for the thread to die */ 
	INM_INIT_SLEEP(&quit_task->vstq_ipc);
    ADD_TASK_TO_POOL(tpool, task); 
	INM_SLEEP(&quit_task->vstq_ipc);

    /* The thread has now exited */
	INM_FINI_SLEEP(&quit_task->vstq_ipc);

	INM_KTHREAD_STOP(quit_task->vstq_process);
	
    INM_READ_UNLOCK(&tp_info->tpi_rwlock);
	INM_FREE(&quit_task, sizeof(vsnap_task_quit_t));
}

void
uninit_one_thread_pool(thread_pool_info_t *pool_info)
{
	inm_32_t 	i = 0;
	inm_32_t	num_threads;
	thread_pool_t	*pool = pool_info->tpi_thread_pool;

	num_threads = INM_ATOMIC_READ(&pool_info->tpi_num_threads);

	DBG("Destroy %d %s threads", num_threads, pool_info->tpi_wname);
	for ( i = 0; i < num_threads; i++ ) {
		inm_kill_thread(pool_info);
	}

	/* Wait for all thread to die */
	INM_WRITE_LOCK(&pool_info->tpi_rwlock);
	INM_WRITE_UNLOCK(&pool_info->tpi_rwlock);

	DEV_DBG("Cleanup Resources");
	INM_FREE(&pool, sizeof(thread_pool_t));
	INM_FREE(&pool_info, sizeof(thread_pool_info_t));
}

void
uninit_thread_pool(void)
{
	uninit_one_thread_pool(vs_update_pool_info);
	uninit_one_thread_pool(vs_ops_pool_info);
	uninit_one_thread_pool(vv_ops_pool_info);
}
