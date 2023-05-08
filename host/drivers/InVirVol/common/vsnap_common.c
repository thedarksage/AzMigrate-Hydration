
/*****************************************************************************
 * vsnap_common.c
 *
 * Copyright (c) 2007 InMage
 * This file contains proprietary and confidential information and remains
 * the unpublished property of InMage. Use, disclosure, or reproduction is
 * prohibited except as permitted by express written license agreement with
 * Inmage
 *
 * Author:              InMage Systems
 *                      http://www.inmage.net
 * Creation date:       Wed, Feb 28, 2007
 * Description:         Common vsnap definitions
 *
 * Revision:            $Revision: 1.7 $
 * Checked in by:       $Author: praweenk $
 * Last Modified:       $Date: 2014/10/10 08:04:07 $
 *
 * $Log: vsnap_common.c,v $
 * Revision 1.7  2014/10/10 08:04:07  praweenk
 * Bug ID:917800
 * Description: Safe API changes for filter and VSNAP driver cross platform.
 *
 * Revision 1.6  2013/12/20 06:03:07  lamuppan
 * Bug #27488: In inm_vs_call_back, we are copying field by field for the keys
 * and in SOlaris-11-x86, movdqu instruction is generated where the system was
 * crashing. Instead used memecpy to avoid the generation of movdqu.
 *
 * Instead of passing odd boundary for reading the FID file, passing a newly
 * allocated one.
 *
 * Revision 1.5  2011/02/02 11:37:37  chvirani
 * Convert DBG() to DEV_DBG() to prevent excessive logging
 *
 * Revision 1.4  2010/10/13 08:09:12  chvirani
 * remove memory unaligned access to btree key
 *
 * Revision 1.3  2010/10/13 06:40:09  chvirani
 * remove memory unaligned access in btree code.
 *
 * Revision 1.2  2010/06/09 10:10:04  chvirani
 * Support for sparse retention
 *
 * Revision 1.1  2009/04/28 12:34:38  cswarna
 * Adding these files as part of vsnap driver code re-org
 *
 * Revision 1.5  2008/08/12 22:27:23  praveenkr
 * fix for #5404: Decoupled the mount point away from creation of vsnap
 * Moved the decision on the device name away from driver into the CX
 * & 'cdpcli'
 *
 * Revision 1.4  2008/03/25 12:59:52  praveenkr
 * code cleanup for Solaris
 *
 * Revision 1.3  2007/09/14 13:11:16  sudhakarp
 * Merge from DR_SCOUT_4-0_DEV/DR_SCOUT_4-1_BETA2 to HEAD ( Onbehalf of RP).
 *
 * Revision 1.1.2.8  2007/08/21 15:25:12  praveenkr
 * added more comments
 *
 * Revision 1.1.2.7  2007/07/24 12:58:30  praveenkr
 * virtual volume (volpack) implementation
 *
 * Revision 1.1.2.6  2007/07/17 13:53:25  praveenkr
 * clean-up - unix-style naming, tab-spacing = 8, 78 col
 *
 * Revision 1.1.2.5  2007/05/25 09:45:43  praveenkr
 * added comments & partial clean-up of code
 *
 * Revision 1.1.2.4  2007/04/24 12:49:46  praveenkr
 * page cache implementation(write)
 *
 * Revision 1.1.2.2  2007/04/17 15:36:30  praveenkr
 * #2944: 4.0_rhl4target-target is crashed when writting the data to R/W vsnap.
 * Sync-ed most of the data types keeping Trunk Windows code as bench-mark.
 * Did minimal cleaning of the code too.
 *
 * Revision 1.1.2.1  2007/04/11 19:49:36  gregzav
 *   initial release of the new make system
 *   note that some files have been moved from Linux to linux (linux os is
 *   case sensitive and we had some using lower l and some using upper L for
 *   linux) this is to make things consitent also needed to move cdpdefs.cpp
 *   under config added the initial brocade work that had been done.
 *
 *****************************************************************************/

#include "vsnap_common.h"
#include "vsnap_kernel_helpers.h"
#include "vsnap_kernel.h"

/* to facilitate ident */
static const char rcsid[] = "$Id: vsnap_common.c,v 1.7 2014/10/10 08:04:07 praweenk Exp $";

#ifdef _MEM_ALIGN_
/* Memory alignment safe comparison of keys */
inm_32_t
compare_less(inm_bt_key_t *lhs, inm_bt_key_t *rhs)
{
	inm_u64_t lhs_offset;
	inm_u64_t rhs_offset;

	INM_ASSIGN(&lhs_offset, &(lhs->key_offset), inm_u64_t);
	INM_ASSIGN(&rhs_offset, &(rhs->key_offset), inm_u64_t);

	return (lhs_offset + lhs->key_len - 1) < rhs_offset ? 1 : 0;

}

inm_32_t
compare_greater(inm_bt_key_t *lhs, inm_bt_key_t *rhs)
{
	inm_u64_t lhs_offset;
	inm_u64_t rhs_offset;

	INM_ASSIGN(&lhs_offset, &(lhs->key_offset), inm_u64_t);
	INM_ASSIGN(&rhs_offset, &(rhs->key_offset), inm_u64_t);

	return (rhs_offset + rhs->key_len - 1) < lhs_offset ? 1 : 0;
}

#endif

/*
 * compare_equal
 *
 * DESC
 *      Overload "==" operator for inm_bt_key_t structure.
 */
inm_32_t
compare_equal(inm_bt_key_t *lhs, inm_bt_key_t *rhs)
{
	inm_u64_t lhs_offset;
	inm_u64_t rhs_offset;
	inm_u64_t lhs_end;
	inm_u64_t rhs_end;

	INM_ASSIGN(&lhs_offset, &(lhs->key_offset), inm_u64_t);
	INM_ASSIGN(&rhs_offset, &(rhs->key_offset), inm_u64_t);

	lhs_end = lhs_offset + lhs->key_len - 1;
	rhs_end = rhs_offset + rhs->key_len - 1;

	if (((lhs_offset >= rhs_offset) && (lhs_offset <= rhs_end)) ||
	    ((lhs_end >= rhs_offset) && (lhs_end <= rhs_end)) ||
	    ((rhs_offset >= lhs_offset) && (rhs_offset <= lhs_end)) ||
	    ((rhs_end >= lhs_offset) && (rhs_end <= lhs_end)))
		return 1;
	else
		return 0;
}

/*
 * inm_vs_callback
 *
 * DESC
 *      This callback is called by btree module during insertions in case of
 *      keys matching. First key is the node key and the second key is the new
 *      key to be inserted. Third parameter the is the pointer to the singly
 *      linked list of entries to be inserted in the btree map. User/Kernel
 *      mode components will initialize the call back before performing
 *      insertions.
 *
 * ALGO
 */
inm_32_t
inm_vs_callback(void *key1, void *key2, void *param, inm_u32_t over_write_flag)
{
	single_list_entry_t* list_head = (single_list_entry_t*)param;
	inm_bt_key_t new_key;
	inm_bt_key_t node_key;
	inm_u64_t new_key_end;
	inm_u64_t node_key_end;
	inm_32_t retval = 0;
	inm_u32_t tracking_id;

	DEV_DBG("Copying keys to local key");
	memcpy_s(&new_key, sizeof(inm_bt_key_t), key2, sizeof(inm_bt_key_t));
	memcpy_s(&node_key, sizeof(inm_bt_key_t), key1, sizeof(inm_bt_key_t));

	new_key_end = new_key.key_offset + new_key.key_len - 1;
	node_key_end =  node_key.key_offset + node_key.key_len - 1;


	if ((new_key.key_offset >= node_key.key_offset) &&
	    (new_key_end <= node_key_end)) {
		if (!over_write_flag) {
			goto out;
		} else {
			if (node_key_end > new_key_end) {
				retval =
				    inm_vs_insert_offset_split(list_head,
							       new_key_end + 1, (inm_u32_t)(node_key_end - new_key_end),
							       node_key.key_fid, node_key.key_file_offset + new_key_end -
							       node_key.key_offset + 1, node_key.key_tracking_id);
				if (retval < 0) {
					ERR("insert offset failed");
					goto out;
				}
			}
			if (new_key.key_offset > node_key.key_offset) {
				retval = inm_vs_insert_offset_split(list_head,
								    node_key.key_offset, (inm_u32_t)(new_key.key_offset - node_key.key_offset),
								    node_key.key_fid, node_key.key_file_offset, node_key.key_tracking_id);
				if (retval < 0) {
					ERR("insert offset failed");
					goto out;
				}
			}
			tracking_id = node_key.key_tracking_id;
			memcpy_s(&node_key, sizeof(inm_bt_key_t), &new_key, sizeof(inm_bt_key_t));
			node_key.key_tracking_id = tracking_id | new_key.key_tracking_id;
		}
	} else if ((new_key.key_offset < node_key.key_offset)	&&
		   (new_key_end >= node_key.key_offset) &&
		   (new_key_end <= node_key_end)) {
		if (!over_write_flag) {
			retval = inm_vs_insert_offset_split(list_head, new_key.key_offset, (inm_u32_t)(node_key.key_offset - new_key.key_offset), new_key.key_fid, new_key.key_file_offset, new_key.key_tracking_id);
			if (retval < 0) {
				ERR("insert offset failed");
			}
			goto out;
		} else {
			if (node_key_end > new_key_end) {
				retval = inm_vs_insert_offset_split(list_head, new_key_end + 1, (inm_u32_t)(node_key_end - new_key_end), node_key.key_fid, node_key.key_file_offset + (new_key_end - node_key.key_offset+1), node_key.key_tracking_id);
				if (retval < 0) {
					ERR("insert offset failed");
					goto out;
				}
			}
			if (new_key_end > node_key.key_offset) {
				retval = inm_vs_insert_offset_split(list_head, new_key.key_offset, (inm_u32_t)(node_key.key_offset - new_key.key_offset), new_key.key_fid, new_key.key_file_offset, new_key.key_tracking_id);
				if (retval < 0) {
					ERR("insert offset failed");
					goto out;
				}

				node_key.key_len = (inm_u32_t)(new_key_end -
								node_key.key_offset+1);
				node_key.key_fid = new_key.key_fid;
				node_key.key_file_offset = new_key.key_file_offset +
				    node_key.key_offset - new_key.key_offset;
			} else {
				retval = inm_vs_insert_offset_split(list_head, new_key.key_offset, (inm_u32_t)(node_key.key_offset - new_key.key_offset), new_key.key_fid, new_key.key_file_offset, new_key.key_tracking_id);
				if (retval < 0) {
					ERR("insert offset failed");
					goto out;
				}

				node_key.key_len = 1;
				node_key.key_fid = new_key.key_fid;
				node_key.key_file_offset = new_key.key_file_offset +
				    new_key_end - new_key.key_offset;
			}
			node_key.key_tracking_id |= new_key.key_tracking_id;
			goto out;
		}
	} else if ((new_key.key_offset >= node_key.key_offset) &&
		   (new_key.key_offset <= node_key_end) &&
		   (new_key_end > node_key_end)) {
		if (!over_write_flag) {
			retval = inm_vs_insert_offset_split(list_head, node_key_end + 1, (inm_u32_t)(new_key_end - node_key_end), new_key.key_fid, new_key.key_file_offset + (node_key_end - new_key.key_offset + 1), new_key.key_tracking_id);
			if (retval < 0) {
				ERR("insert offset failed");
			}
			goto out;
		} else {
			if (new_key.key_offset > node_key.key_offset) {
				retval = inm_vs_insert_offset_split(list_head, node_key.key_offset, (inm_u32_t)(new_key.key_offset - node_key.key_offset), node_key.key_fid, node_key.key_file_offset, node_key.key_tracking_id);
				if (retval < 0) {
					ERR("insert offset failed");
					goto out;
				}
			}

			tracking_id = node_key.key_tracking_id;

			if (new_key.key_offset < node_key_end) {
				retval = inm_vs_insert_offset_split(list_head, node_key_end + 1, (inm_u32_t)(new_key_end - node_key_end), new_key.key_fid, new_key.key_file_offset+ (node_key_end - new_key.key_offset + 1), new_key.key_tracking_id);
				if (retval < 0) {
					ERR("insert offset");
					goto out;
				}
				memcpy_s(&node_key, sizeof(inm_bt_key_t), &new_key, sizeof(inm_bt_key_t));
				node_key.key_len = (inm_u32_t)(node_key_end -
								new_key.key_offset+1);
			} else {
				retval = inm_vs_insert_offset_split(list_head, new_key.key_offset + 1, (inm_u32_t)(new_key.key_len - 1), new_key.key_fid, new_key.key_file_offset + 1, new_key.key_tracking_id);
				if (retval < 0) {
					ERR("insert offset failed");
					goto out;
				}
				memcpy_s(&node_key, sizeof(inm_bt_key_t), &new_key, sizeof(inm_bt_key_t));
				node_key.key_len = 1;
			}
			node_key.key_tracking_id = tracking_id | new_key.key_tracking_id;
			goto out;
		}
	} else if ((new_key.key_offset < node_key.key_offset)	&&
		   (new_key_end > node_key_end)) {
		if (!over_write_flag) {
			retval = inm_vs_insert_offset_split(list_head, new_key.key_offset, (inm_u32_t)(node_key.key_offset - new_key.key_offset), new_key.key_fid, new_key.key_file_offset, new_key.key_tracking_id);
			if (retval < 0) {
				ERR("insert offset failed");
				goto out;
			}
			retval = inm_vs_insert_offset_split(list_head, node_key_end + 1, (inm_u32_t)(new_key_end - node_key_end), new_key.key_fid, new_key.key_file_offset + (node_key_end - new_key.key_offset + 1), new_key.key_tracking_id);
			if (retval < 0) {
				ERR("insert offset failed");
			}
			goto out;
		} else {
			retval = inm_vs_insert_offset_split(list_head, new_key.key_offset, (inm_u32_t)(node_key.key_offset - new_key.key_offset), new_key.key_fid, new_key.key_file_offset, new_key.key_tracking_id);
			if (retval < 0) {
				ERR("insert offset failed");
				goto out;
			}
			retval = inm_vs_insert_offset_split(list_head, node_key_end + 1, (inm_u32_t)(new_key_end - node_key_end), new_key.key_fid, new_key.key_file_offset + (node_key_end - new_key.key_offset + 1), new_key.key_tracking_id);
			if (retval < 0) {
				ERR("insert offset failed");
				goto out;
			}
			node_key.key_fid = new_key.key_fid;
			node_key.key_file_offset = new_key.key_file_offset +
			    node_key.key_offset - new_key.key_offset;
			node_key.key_tracking_id |= new_key.key_tracking_id;
		}
	}

  out:
	if( over_write_flag ) {
		DEV_DBG("Overwriting node key");
		memcpy_s(key1, sizeof(inm_bt_key_t), &node_key, sizeof(inm_bt_key_t));
	}

	return retval;
}

/*
 * pop_entry_from_list
 *
 * DESC
 *	pop an entry from the top of the list
 */
single_list_entry_t*
pop_entry_from_list(single_list_entry_t* list_entry_head)
{
	single_list_entry_t* first_entry = NULL;

	first_entry = list_entry_head->sl_next;
	if (first_entry != NULL) {
		list_entry_head->sl_next = first_entry->sl_next;
	}

	return first_entry;
}


/*
 * inm_strtoktoull
 *
 * DESC
 *      From a given string, extract numerical tokens described by
 *	vectors and convert to u64 number.
 *
 * ARGS:
 *	str: Base String
 *
 *	vector_size: number of vectors in vector array
 *
 *	vector: (start, max_len) vectors describing tokens in string.
 *		start - start of token. Is relative to end of last token.
 *		max_len - signifies the number of significant digits(chars)
 *			  to consider to for u64 num computation. beyond
 *			  max_len, the chars are not used for computation
 *			  but we traverse until the end of token.
 *
 * Returns:
 *	u64 number derived using string tokens
 */
#define isdigit(n)	((n) >= '0' && (n) <= '9')

inm_u64_t
inm_strtoktoull(char *str, int vector_size, int vector[][2])
{
	inm_32_t 	i = 0;
	inm_u64_t	retval = 0;
	inm_32_t	tok_size = 0;
	char		*tok_start = str;
	inm_32_t	j;

	for ( i = 0; i < vector_size; i++ ) {
		tok_start = str + vector[i][0];
		tok_size = vector[i][1];
		j = 0;

		while( isdigit(*tok_start) && j < tok_size ) {
				retval = retval * 10 + ( *tok_start - '0');
				j++;
				tok_start++;
		}
	}

	return retval;
}
