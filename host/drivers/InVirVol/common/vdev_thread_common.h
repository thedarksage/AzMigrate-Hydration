#ifndef _VDEV_THREAD_COMMON_H
#define _VDEV_THREAD_COMMON_H

#include "vsnap.h"
#include "vsnap_kernel_helpers.h"

#define INM_VS_NUM_THREADS		72
#define INM_VV_NUM_THREADS  		16
/* Set this to the greater of INM_VS_NUM_THREADS and INM_VV_NUM_THREADS */
#define INM_MAX_NUM_THREADS		INM_VS_NUM_THREADS

#define INM_TASK_BTREE_LOOKUP  		1
#define INM_TASK_RETENTION_IO  		2
#define INM_TASK_WRITE   		3
#define INM_TASK_VOLPACK_IO		4
#define INM_TASK_UPDATE_VSNAP  		5
#define INM_TASK_KILL_THREAD		255

typedef struct vsnap_task {
	inm_list_head_t		vst_task;	/* list of tasks */
	void 			*vst_task_data;	/* data required for the task */
	inm_u32_t		vst_task_id;
} vsnap_task_t;

#include "vdev_thread.h"

typedef struct thread_pool {
	inm_list_head_t		tp_task_list_head;	/* list of tasks - maintain a queue */
	inm_spinlock_t		tp_lock; 		/* for task list operations */
	inm_wait_queue_head_t	tp_wait_queue;		/* for threads to wait on */
	inm_u64_t		tp_num_tasks;		/* num of tasks		*/
} thread_pool_t;

#define INM_WORKER_NAME_SIZE	20

typedef inm_32_t (*thread_func_t)(void *);

typedef struct thread_pool_info {
	inm_rwlock_t	tpi_rwlock;
	inm_atomic_t 	tpi_num_threads;
	thread_pool_t	*tpi_thread_pool;
	thread_func_t	tpi_func;
	char		tpi_wname[INM_WORKER_NAME_SIZE];
} thread_pool_info_t;

#define ADD_TASK_TO_POOL(POOL,TASK) 					    \
	  do {			    					    \
		INM_ACQ_SPINLOCK(&(POOL)->tp_lock); 			    \
		inm_list_add_tail(&TASK->vst_task, &(POOL)->tp_task_list_head); \
		(POOL)->tp_num_tasks++; 				    \
		INM_REL_SPINLOCK(&(POOL)->tp_lock); 			    \
		INM_WAKEUP_WORKER(&(POOL)->tp_wait_queue);		    \
	  } while(0)

/*
 * ADD_TASK_TO_POOL_HEAD is not currently used
 */
#define ADD_TASK_TO_POOL_HEAD(POOL,TASK)				    \
	  do {								    \
		INM_ACQ_SPINLOCK(&(POOL)->tp_lock); 			    \
		inm_list_add(&TASK->vst_task, &(POOL)->tp_task_list_head);      \
		(POOL)->tp_num_tasks++; 				    \
		INM_REL_SPINLOCK(&(POOL)->tp_lock); 			    \
		INM_WAKEUP_WORKER(&(POOL)->tp_wait_queue);		    \
	  } while(0)
/*
 * There is a lack of synchronisation between a thread woken and
 * thread preparing to wait to execute new tasks
 * so check for numtasks after acquiring spinlock
 */
#define GET_TASK_FROM_POOL(POOL, TASK) 					       \
	  do {								       \
		INM_ACQ_SPINLOCK(&(POOL)->tp_lock); 			       \
		if ( !(POOL)->tp_num_tasks ) {				       \
			TASK = NULL; 					       \
		} else {						       \
			TASK = (vsnap_task_t *)(POOL)->tp_task_list_head.next; \
			inm_list_del((POOL)->tp_task_list_head.next); 	       \
			(POOL)->tp_num_tasks--; 			       \
		} 							       \
		INM_REL_SPINLOCK(&(POOL)->tp_lock);			       \
	  } while (0)


#define	WAIT_FOR_NEXT_TASK(POOL)	WAIT(POOL)

/* Kill Structures */ 
typedef struct vsnap_task_quit {
	struct vsnap_task	vstq_task;
	inm_ipc_t		    vstq_ipc;
	void	            *vstq_process;
}vsnap_task_quit_t;

inm_32_t init_thread_pool(void);
void uninit_thread_pool(void);
void inm_kill_thread(thread_pool_info_t *);
inm_32_t vs_ops_worker_thread(void *);
inm_32_t vv_ops_worker_thread(void *);
inm_32_t vs_update_worker_thread(void *);
/* Needs to be defined for each platform */
inm_32_t inm_thread_create(thread_pool_info_t *);

#endif

