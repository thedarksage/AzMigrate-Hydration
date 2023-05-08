#ifndef _SOFT_STATE_H_
#define _SOFT_STATE_H_

#include "common.h"
#include "vsnap_kernel_helpers.h"

typedef struct soft_state {
	/*
	 * get_state() can be called in interrupt context
	 * spinlock is used for contention between get and
	 * set where set has to replace old array with new
	 */
	inm_spinlock_t	ss_spinlock;
	/* mutex is used for contention between parallel sets */
	inm_mutex_t	ss_mutex;
	void 		**ss_array;
	inm_u32_t 	ss_num_items;
} inm_soft_state_t;

inm_32_t inm_soft_state_init(void **);
inm_32_t inm_set_soft_state(void *, inm_32_t, void *);
void *inm_get_soft_state(void *, inm_32_t);
void inm_soft_state_destroy(void **);

#endif
