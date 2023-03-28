#ifndef _VDEV_THREAD_H
#define _VDEV_THREAD_H

#include "common.h"
#include "vdev_thread_common.h"

struct thread_pool;

#define INM_WAKEUP_WORKER(qptr)         INM_WAKEUP(qptr)
#define INM_INIT_WAITQUEUE_HEAD(qptr)	INM_INIT_IPC(qptr)

typedef inm_ipc_t	inm_wait_queue_head_t;

inm_32_t __inm_wait(struct thread_pool *);
#define WAIT(POOL)     __inm_wait(POOL)

void inm_set_ulimit_to_infinity();

#define INM_SET_KILL_INFO(task)
#define INM_KTHREAD_STOP(task)

#endif  // _VDEV_THREAD_H

