#ifndef _VDEV_THREAD_H
#define _VDEV_THREAD_H

#include "common.h"

#define WAIT(POOL) 							\
	wait_event_interruptible_exclusive((POOL)->tp_wait_queue, 	\
					   (POOL)->tp_num_tasks > 0)
#define INM_WAKEUP_WORKER(qptr)		wake_up(qptr)
#define INM_INIT_WAITQUEUE_HEAD(qptr)	init_waitqueue_head(qptr)

typedef wait_queue_head_t	inm_wait_queue_head_t;

#define inm_set_ulimit_to_infinity()

#define INM_SET_KILL_INFO(task)  (task)->vstq_process = (void *)current

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,8,13)
#define INM_KTHREAD_STOP(task)		kthread_stop(task)
#else
#define INM_KTHREAD_STOP(task)
#endif


#endif


