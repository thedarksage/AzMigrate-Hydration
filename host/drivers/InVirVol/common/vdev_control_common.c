#include "common.h"
#include "vsnap_kernel_helpers.h"
#include "vsnap_kernel.h"
#include "vsnap_control.h"
#include "vv_control.h"

/* list of minor numbers in use & used for vsnap devices */
typedef struct inm_vs_minor_list_tag {
	inm_list_head_t		vsm_next;
	inm_minor_t		vsm_minor;
} inm_vs_minor_list_t;

static inm_list_head_t vs_minor_list_head = {
        NULL,           /* initialize to next & prev to NULL */
        NULL            /* because NULL is used as a criteria while
                           initializing the list */
};

/* list of minor numbers used for volpack devices */
typedef struct inm_vv_minor_list_tag {
        inm_list_head_t		vvm_next;
        inm_minor_t		vvm_minor;
} inm_vv_minor_list_t;

static inm_list_head_t vv_minor_list_head = {
        NULL,           /* initialize to next & prev to NULL */
        NULL            /* because NULL is used as a criteria while
                           initializing the list */
};

/*
 * get_free_vs_minor
 *
 * DESC
 *      allocate a free available minor # for vsnap devices
 *      (note: minor #'s list is always in increasing sorted order)
 *
 * ALGO
 *      1. if the list is empty
 *      2.   initialize the list head
 *      3.   reserve & return 0 as the minor #
 *      4. else
 *      5.   reserve & return the 1st free available #
 */
inm_minor_t
get_free_vs_minor(void)
{
	inm_32_t		free_minor = INM_VSNAP_MINOR_START;
	inm_list_head_t		*ptr = NULL;
	inm_list_head_t		*prev = NULL;
	inm_vs_minor_list_t	*temp_entry = NULL;
	inm_vs_minor_list_t	*new_entry = NULL;

	new_entry = INM_ALLOC(sizeof(inm_vs_minor_list_t), GFP_KERNEL);
	if (new_entry == NULL) {
		ERR("no mem");
		return -ENOMEM;
	}

	INM_ACQ_SPINLOCK(&inm_vs_minor_list_lock);

	/*
	 * if the minor list is not initialized then the head.next is NULL
	 * if so then it _HAS_ to be a fresh call, i.e., 1st call to this fn
	 */
	if (!vs_minor_list_head.next) {
		INM_INIT_LIST_HEAD(&vs_minor_list_head);
		prev = &vs_minor_list_head;
		/* free_minor = 0 falls through */
	} else {
		inm_list_for_each(ptr, &vs_minor_list_head) {
			temp_entry = inm_list_entry(ptr, inm_vs_minor_list_t,
						vsm_next);
			if (temp_entry->vsm_minor > free_minor)
				break; /* found a free entry */

			if (temp_entry->vsm_minor == free_minor) {
				free_minor++;
				prev = &(temp_entry->vsm_next);
			}

			if ( free_minor == INM_MAX_VSNAP ) {
				goto max_limit_reached;
			}
		}

		if (!prev) {
			prev = &vs_minor_list_head;
		}
	}

	new_entry->vsm_minor = free_minor;
	inm_list_add(&new_entry->vsm_next, prev);
	INM_REL_SPINLOCK(&inm_vs_minor_list_lock);

	DBG("minor number - %d", free_minor);

out:
	return free_minor * INM_VSNAP_NUM_DEVICES;

max_limit_reached:
	INM_REL_SPINLOCK(&inm_vs_minor_list_lock);
	ERR("Max VSNAP limit reached");
	INM_FREE(&new_entry, sizeof(inm_vs_minor_list_t));
	free_minor = -EMFILE;
	goto out;

}

/*
 * put_vs_minor
 *
 * DESC
 *      release the minor number associated with vsnap devices
 *      (note: minor # list is always in the increasing sorted order)
 */
inm_32_t
put_vs_minor(inm_minor_t minor)
{
	inm_32_t retval = -EINVAL;
	inm_list_head_t *ptr = NULL;
	inm_list_head_t *tmp = NULL;
	inm_vs_minor_list_t *temp_entry = NULL;

	minor = minor / INM_VSNAP_NUM_DEVICES;

	INM_ACQ_SPINLOCK(&inm_vs_minor_list_lock);
	inm_list_for_each_safe(ptr, tmp, &vs_minor_list_head) {
		temp_entry = inm_list_entry(ptr, inm_vs_minor_list_t, vsm_next);
		if (temp_entry->vsm_minor == minor) {
			inm_list_del(ptr);
			retval = 0;
			break;
		}
	}
	INM_REL_SPINLOCK(&inm_vs_minor_list_lock);

	if ( retval == 0 ) {
		DBG("Minor found");
		INM_FREE(&temp_entry, sizeof(inm_vs_minor_list_t));
	} else {
		ERR("minor # not found");
	}

	return retval;	/* if not found then EINVAL will fall through */
}


/*
 * get_free_vv_minor
 *
 * DESC
 *      allocate a free available minor #
 *      (note: minor #'s list is always in increasing sorted order)
 *
 * ALGO
 *      1. if the list is empty
 *      2.   initialize the list head
 *      3.   reserve & return 0 as the minor #
 *      4. else
 *      5.   reserve & return the 1st free available #
 */
inm_32_t
get_free_vv_minor(void)
{
	inm_32_t		free_minor = INM_VOLPACK_MINOR_START;
	inm_list_head_t		*ptr = NULL;
	inm_list_head_t		*prev = NULL;
	inm_vv_minor_list_t	*temp = NULL;
	inm_vv_minor_list_t	*minor_entry;

	minor_entry = INM_ALLOC(sizeof(inm_vv_minor_list_t), GFP_KERNEL);
	if (minor_entry == NULL) {
		ERR("no mem");
		return -ENOMEM;
	}

	INM_ACQ_SPINLOCK(&inm_vv_minor_list_lock);

        /*
	 * if the minor list is not initialized then the head.next is NULL
	 * if so then it _HAS_ to be a fresh call, i.e., 1st call to this fn
	 */
	if (!vv_minor_list_head.next) {
		INM_INIT_LIST_HEAD(&vv_minor_list_head);
		prev = &vv_minor_list_head;
		/* free_minor = 0 falls through */
	} else {
		inm_list_for_each(ptr, &vv_minor_list_head) {
			temp = inm_list_entry(ptr, inm_vv_minor_list_t, vvm_next);
			if ( temp->vvm_minor > free_minor ) {
				break; /* found a free entry */
			}

			if ( temp->vvm_minor == free_minor ) {
				free_minor++;
				prev = &(temp->vvm_next);
			}

			if ( free_minor == INM_MAX_VV ) {
				goto max_limit_reached;
			}
		}
		if (!prev) {
			prev = &vv_minor_list_head;
		}
	}

	minor_entry->vvm_minor = free_minor;
	inm_list_add(&minor_entry->vvm_next, prev);
	INM_REL_SPINLOCK(&inm_vv_minor_list_lock);
	DBG("minor number - %d", free_minor);

out:
	return free_minor;

max_limit_reached:
	INM_REL_SPINLOCK(&inm_vv_minor_list_lock);
	ERR("Max VV limit reached");
	INM_FREE(&minor_entry, sizeof(inm_vv_minor_list_t));
	free_minor = -EMFILE;
	goto out;

}

/*
 * put_vv_minor
 *
 * DESC
 *      release the minor number
 *      (note: minor # list is always in the increasing sorted order)
 */
inm_32_t
put_vv_minor(inm_32_t minor)
{
	inm_32_t		retval = -EINVAL;
	inm_list_head_t		*ptr = NULL;
	inm_list_head_t		*tmp = NULL;
	inm_vv_minor_list_t	*temp = NULL;

	INM_ACQ_SPINLOCK(&inm_vv_minor_list_lock);
	inm_list_for_each_safe(ptr, tmp, &vv_minor_list_head) {
		temp = inm_list_entry(ptr, inm_vv_minor_list_t, vvm_next);
		if (temp->vvm_minor == minor) {
			inm_list_del(ptr);
			retval = 0;
			break;
		}
	}
	INM_REL_SPINLOCK(&inm_vv_minor_list_lock);

	if (retval == 0) {
		INM_FREE(&temp, sizeof(inm_vv_minor_list_t));
	} else {
		ERR("minor # not found");
	}

	return retval;	/* if not found then EINVAL will fall through */
}

/*
 * insert_into_minor_list
 *
 * DESC
 *      insert a minor # into the list
 *      this fn is called when the create ioctl is invoked with a minor #,
 *      which is possible, say, during reboot... mainly to maintain the
 *      persistency of the volpack devices
 *
 *      (note - the list is maintained in an increasing sorted order)
 */
inm_32_t
insert_into_minor_list(inm_32_t minor)
{
	inm_32_t		retval = 0;
	inm_list_head_t		*ptr = NULL;
	inm_list_head_t		*prev = NULL;
	inm_vv_minor_list_t	*temp = NULL;
	inm_vv_minor_list_t	*minor_entry;

	minor_entry = INM_ALLOC(sizeof(inm_vv_minor_list_t), GFP_KERNEL);
	if ( !minor_entry ) {
		ERR("no mem");
		return -ENOMEM;
	}

	INM_ACQ_SPINLOCK(&inm_vv_minor_list_lock);

	/*
	 * if the minor list is not initialized then the head.next is NULL
	 * if so then it _HAS_ to be a fresh call, i.e., 1st call to this fn
	 */
	if (!vv_minor_list_head.next) {
		INM_INIT_LIST_HEAD(&vv_minor_list_head);
		prev = &vv_minor_list_head;
	} else {
		inm_list_for_each(ptr, &vv_minor_list_head) {
			temp = inm_list_entry(ptr, inm_vv_minor_list_t, vvm_next);
			if (temp->vvm_minor > minor) {
				break; /* found a free entry */
			}
			if (temp->vvm_minor == minor) {
				retval = -EINVAL;
				goto error;
			}
		}
		if (!prev) {
			prev = &vv_minor_list_head;
		}
	}
	minor_entry->vvm_minor = minor;
	inm_list_add(&minor_entry->vvm_next, prev);
	INM_REL_SPINLOCK(&inm_vv_minor_list_lock);

out:
	return retval;

error:
	INM_REL_SPINLOCK(&inm_vv_minor_list_lock);
	ERR("minor %d  exists.", minor);
	INM_FREE(&minor_entry, sizeof(inm_vv_minor_list_t));
	goto out;
}

/*
 * inm_atoi
 *
 * DESC
 *      to convert the given the string into int
 *      this is invoked with the minor # is passedas an i/p from the user space
 */
inm_32_t
inm_atoi(char *str)
{
	inm_32_t	len = strlen(str);
	inm_32_t	i = len - 1;
	inm_32_t	k = 1;
	inm_32_t	l = 0;

	DEV_DBG("dev path given - %s", str);
	do {
		inm_32_t j = str[i];

		l += k * (j - 48);
		k *= 10;
	} while(i--);
	DEV_DBG("minor # found was - %d", l);
	return l;
}
