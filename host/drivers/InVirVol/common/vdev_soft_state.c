#include "vdev_soft_state.h"

/* This is an implementation similar to soft states in solaris.
 * This is a much simpler implementation where soft state can only
 * be a pointer to the needed data structure.
 *
 * Note: Although the soft state structures are in pinned heap, the entities
 * they point to are not. Its responsibility of callers to pin data as per
 * their requrements.
 */

/*
 * inm_soft_state_init: This function is called to intialise a soft state array.
 *			A soft state array is used to store and retrieve
 *			related pointers.
 */
inm_32_t
inm_soft_state_init(void **ss_ptr)
{
	inm_32_t retval = 0;
	inm_soft_state_t *ptr;

	ptr = INM_ALLOC(sizeof(inm_soft_state_t), GFP_PINNED);
	if( !ptr ) {
		ERR("Cannot allocate soft state structure");
		retval = -ENOMEM;
		goto out;
	}

	ptr->ss_array = NULL;
	ptr->ss_num_items = 0;
	INM_INIT_SPINLOCK(&ptr->ss_spinlock);
	INM_INIT_MUTEX(&ptr->ss_mutex);

	*ss_ptr = ptr;

out:
	return retval;
}

/*
 * inm_set_soft_state: 	This function is called to store a pointer in soft
 *			state array. It can only be called in process context.
 *
 */;
inm_32_t
inm_set_soft_state(void *ssp, inm_32_t idx, void *value)
{
	inm_u32_t num_items = 0;
	void *ptr = NULL;
	void *org_ptr = NULL;
	inm_u32_t org_num_items = 0;
	inm_32_t retval = 0;
	inm_soft_state_t *ss_ptr = ssp;

	INM_ACQ_MUTEX(&ss_ptr->ss_mutex);

	if( ss_ptr->ss_num_items <= idx ) {
		DBG("Allocating new batch of soft states");
		num_items = ((( idx >> 3 ) + 1 ) << 3); /* Allocate chunk of 8 */

		DEV_DBG("New num soft states = %u", num_items);

		ptr = INM_ALLOC(num_items * sizeof(void *), GFP_PINNED);
		if( !ptr ) {
			INM_REL_MUTEX(&ss_ptr->ss_mutex);
			ERR("zalloc soft state failed");
			retval = -ENOMEM;
			goto out;
		}

		memzero(ptr, num_items * sizeof(void *));

		/* copy the original soft states */
		if (memcpy_s(ptr, num_items * sizeof(void *), ss_ptr->ss_array,
					ss_ptr->ss_num_items * sizeof(void *))) {

			INM_REL_MUTEX(&ss_ptr->ss_mutex);
			INM_FREE(&ptr, num_items * sizeof(void *));
			retval = -EFAULT;
			goto out;
		}

		/* take spin lock to protect against get_soft_state() */
		INM_ACQ_SPINLOCK(&ss_ptr->ss_spinlock);
		org_num_items = ss_ptr->ss_num_items;
		ss_ptr->ss_num_items = num_items;
		org_ptr = ss_ptr->ss_array;
		ss_ptr->ss_array = ptr;
		INM_REL_SPINLOCK(&ss_ptr->ss_spinlock);

	}

	if( ss_ptr->ss_array[idx] && value ) {
		ERR("Overwriting soft state ... might be a bug");
		ASSERT(1);
	}

	ss_ptr->ss_array[idx] = value;

	INM_REL_MUTEX(&ss_ptr->ss_mutex);

	if( org_ptr )
		INM_FREE_PINNED(&org_ptr, org_num_items * sizeof(void *));

out:
	return retval;
}

/*
 * inm_set_soft_state: 	This function is called to retrieve the pointer stored
 *			in soft state array. It can be called in process
 *			or interrupt context.
 */
void *
inm_get_soft_state(void *ssp, inm_32_t idx)
{
	void *retptr = NULL;
	inm_soft_state_t *ss_ptr = ssp;

	if( idx >= 0 && idx < ss_ptr->ss_num_items ) {
		INM_ACQ_SPINLOCK(&ss_ptr->ss_spinlock);
		retptr = ss_ptr->ss_array[idx];
		INM_REL_SPINLOCK(&ss_ptr->ss_spinlock);
	}

	DEV_DBG("Returning soft state %p", retptr);

	return retptr;
}

/*
 * inm_soft_state_destroy: Called to destroy all soft state related data
 */
void
inm_soft_state_destroy(void **ss_pp)
{
	inm_soft_state_t *ss_ptr = *ss_pp;

	DEV_DBG("Freeing %u soft states", ss_ptr->ss_num_items);
	if( ss_ptr->ss_num_items ) {
		INM_FREE_PINNED(&ss_ptr->ss_array,
				ss_ptr->ss_num_items * sizeof(void *));
	}

	INM_FREE_PINNED(&ss_ptr, sizeof(inm_soft_state_t));

	*ss_pp = NULL;
}
