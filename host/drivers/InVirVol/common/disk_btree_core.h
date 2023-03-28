
/*****************************************************************************
 * disk_btree_core.h
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
 * Description:
 *
 * Revision:            $Revision: 1.3 $
 * Checked in by:       $Author: chvirani $
 * Last Modified:       $Date: 2010/10/13 06:40:09 $
 *
 * $Log: disk_btree_core.h,v $
 * Revision 1.3  2010/10/13 06:40:09  chvirani
 * remove memory unaligned access in btree code.
 *
 * Revision 1.2  2010/03/10 12:08:43  chvirani
 * Multithreaded Driver
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
 * Revision 1.1.2.7  2007/07/17 13:53:25  praveenkr
 * clean-up - unix-style naming, tab-spacing = 8, 78 col
 *
 * Revision 1.1.2.6  2007/05/25 13:04:50  praveenkr
 * deleted unused functions - left behind during porting
 * DiskBtreeInitBtreeMapFile
 *
 * Revision 1.1.2.5  2007/05/25 12:52:25  praveenkr
 * deleted unused functions - left behind during porting
 * DiskBtreeCopyRootNode, DiskBtreeCtr, DiskBtreeDtr,
 * DiskBtreeInitFromDiskFileName
 *
 * Revision 1.1.2.4  2007/05/25 09:45:43  praveenkr
 * added comments & partial clean-up of code
 *
 * Revision 1.1.2.3  2007/04/24 12:49:46  praveenkr
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

#ifndef _DISK_BTREE_CORE_H
#define _DISK_BTREE_CORE_H

#include "common.h"
#include "vsnap_common.h"
#include "vsnap_kernel_helpers.h"

#ifdef DISK_PAGE_SIZE
#undef DISK_PAGE_SIZE
#endif

enum BTREE_STATUS {
	BTREE_FAIL,              /* 0 */
	BTREE_SUCCESS,
	BTREE_SUCCESS_MODIFIED
};

/*
 * header thats filled & stored persistently in the btree file
 * maintained for scalability & more importantly for PERSISTENCE.
 * scalability - currently the max & min keys are fixed but this can be
 *               modified based on the requirement
 * persistence - the root-node's offset is stored in here.
 */
typedef struct inm_bt_hdr_tag {
	inm_u64_t	hdr_root_offset;	/* root node (disk) offset */
	inm_u64_t	hdr_rsvd1;		/* unused */
	inm_u64_t	hdr_rsvd2;		/* unused */
	inm_u32_t	hdr_version;		/* disk btree version */
	inm_32_t	hdr_max_keys;		/* max keys per node */
	inm_32_t	hdr_min_keys;		/* min keys per node */
	inm_32_t	hdr_key_size;		/* size of 1 key */
	inm_u32_t	hdr_node_size;		/* size of 1 node */
} inm_bt_hdr_t;

/* fixed sizes that directly reflect the on-disk offset/size */
#define BTREE_HEADER_OFFSET_TO_USE	128
#define BTREE_HEADER_SIZE		512
#define DISK_PAGE_SIZE			4096

/*
 * represents a btree in memory
 * TODO: have many small btrees to improve perf, currently only one btree is
 *       currently used
 */
typedef struct inm_bt_tag {
	inm_32_t	bt_max_keys;		/* max keys per node */
	inm_32_t	bt_min_keys;		/* min keys per node */
	inm_32_t	bt_key_size;		/* size of 1 key */
	void		*bt_root_node;		/* root-node */
	inm_bt_hdr_t	*bt_hdr;		/* pointer to btree header */
	inm_64_t	bt_root_offset;		/* r-node offset within file */
	void		*bt_map_filp;		/* file handle to map file */
	void		*bt_callback_param;	/* params to call back fn */
	inm_32_t (*bt_callback)(void *, void *, void *, inm_u32_t);
						/* this callback is called by
						   btree module during
						   insertions in case of keys
						   matching */
	void		**bt_cache;
	inm_mutex_t	bt_cache_mutex;
	inm_u32_t	bt_num_cached_nodes;
} inm_bt_t;

/*
 * MACROs that concern the node of the btree
 */
#define BT_CHILD_PTR_SIZE	sizeof(inm_64_t)
#define BT_NUM_KEYS_SIZE	sizeof(inm_32_t)

#define BT_NODE_ALLOC_SIZE	DISK_PAGE_SIZE

#define BT_NODE_PAD_SIZE	4

#define BT_NODE_SIZE		( BT_NODE_ALLOC_SIZE - BT_NODE_PAD_SIZE )

#define BT_HDR_PAD_SIZE		0


/*
 * in a node there are 'n' keys & 'n + 1' child pointers. so to calculate 'n'
 * 	BT_NODE_SIZE = BT_NUM_KEYS_SIZE + (n + 1) x (BT_CHILD_PTR_SIZE) +
 *				(n) x (KEY_SZ)
 * =>	(BT_NODE_SIZE - BT_NUM_KEYS_SIZE) = (n + 1) x (BT_CHILD_PTR_SIZE) +
 *				(n) x (KEY_SZ)
 * =>	(BT_NODE_SIZE - BT_NUM_KEYS_SIZE - BT_CHILD_PTR_SIZE) =
 *			(n) x (BT_CHILD_PTR_SIZE) + (n) x (KEY_SZ)
 * =>	(n) = (BT_NODE_SIZE - BT_NUM_KEYS_SIZE - BT_CHILD_PTR_SIZE) /
 *				(KEY_SZ + BT_CHILD_PTR_SIZE)
 */
#define INM_MAX_KEYS(KEY_SZ)					\
	(BT_NODE_ALLOC_SIZE - BT_NUM_KEYS_SIZE - BT_CHILD_PTR_SIZE) / \
	(KEY_SZ + BT_CHILD_PTR_SIZE)

#define INM_ROOT_NODE_OFFSET(BT)	(BT)->bt_root_offset

#define BT_NODE_SPACE_USED \
	BT_NUM_KEYS_SIZE + \
	((INM_MAX_KEYS(sizeof(inm_bt_key_t)) + 1) * (BT_CHILD_PTR_SIZE)) + \
	(INM_MAX_KEYS(sizeof(inm_bt_key_t)) * sizeof(inm_bt_key_t))

#define BT_NODE_SPACE_FREE \
	BT_NODE_ALLOC_SIZE - ( BT_NODE_SPACE_USED )

/*
 * STRUCTURE OF KEY
 *	-------------------------...----------------------------...---------
 *	| # of | child | child | ... |  child   | node | node | ... | node |
 *	| keys |  ptr0 |  ptr1 | ... | ptr(n+1) | ptr0 | ptr1 | ... | ptrn |
 *	-------------------------...----------------------------...---------
 *	| 32b  |	each 64b		|	each 64b	   |
 *	-------------------------...----------------------------...---------
 */
#define INM_GET_NUM_KEYS(NODE)		(*(inm_32_t *)(NODE))
#define INM_SET_NUM_KEYS(NODE, KEYS)	(*(inm_32_t *)(NODE) = (KEYS))
#define INM_INCR_NUM_KEYS(NODE)		(*(inm_32_t *)(NODE) += 1)
#define INM_CHILD_PTR(IDX, NODE)				\
	(inm_u64_t *)((char *)(NODE) + BT_NUM_KEYS_SIZE + 	\
			((IDX) * (BT_CHILD_PTR_SIZE)))
#define INM_CHILD_VAL(IDX, NODE)	(*(INM_CHILD_PTR(IDX, NODE)))
#define INM_NODE_PTR(BT, IDX, NODE)				\
	((char *)(NODE) + BT_NUM_KEYS_SIZE +			\
	(((BT)->bt_max_keys + 1) * (BT_CHILD_PTR_SIZE)) +	\
	((IDX) * (BT)->bt_key_size))

/*
 * over load the comparison operators
 */
#ifdef _MEM_ALIGN_

#define INM_CMP_LESS(LHS, RHS)		compare_less((LHS), (RHS))
#define INM_CMP_GREATER(LHS, RHS)	compare_greater((LHS), (RHS))
#define INM_CMP_EQUAL(LHS, RHS)		compare_equal((LHS), (RHS))

#else

#define INM_CMP_LESS(LHS, RHS)					\
	((((LHS)->key_offset + (LHS)->key_len - 1) <		\
	(RHS)->key_offset)? 1:0)
#define INM_CMP_GREATER(LHS, RHS)				\
	((((RHS)->key_offset + (RHS)->key_len - 1) <		\
	(LHS)->key_offset)? 1:0)
#define INM_CMP_EQUAL(LHS, RHS)		compare_equal((LHS), (RHS))

#endif

/*
 * call back functions
 */
#define INM_SET_CALLBACK(BT, CALLBACK)	(BT)->bt_callback = (CALLBACK)
#define INM_SET_CALLBACK_PARAM(BT, PARAM)			\
	(BT)->bt_callback_param = (PARAM)
#define INM_RESET_CALLBACK(BT)		INM_SET_CALLBACK(BT, NULL)
#define INM_RESET_CALLBACK_PARAM(BT)	INM_SET_CALLBACK_PARAM(BT, NULL)

/* Init & UnInit routines */
inm_bt_t* inm_bt_ctr(inm_u32_t);
void inm_bt_dtr(inm_bt_t *);
inm_32_t inm_vs_init_bt_from_map(inm_bt_t *, void *);

/* Header routines */
inm_32_t inm_bt_copy_hdr(inm_bt_t *, void *);
inm_32_t inm_vs_bt_write_hdr(inm_bt_t *, void *);

/* Btree related routines */
enum BTREE_STATUS inm_vs_bt_insert(inm_bt_t *, inm_bt_key_t *, inm_u32_t);
enum BTREE_STATUS inm_vs_bt_lookup(inm_bt_t *, void *, inm_bt_key_t *,
				   inm_bt_key_t *, void **);
enum BTREE_STATUS inm_vs_bt_insert_nonfull(inm_bt_t *, void *, inm_u64_t,
					   inm_bt_key_t *, inm_u32_t);
inm_32_t bt_read(inm_bt_t *, inm_u64_t , void *, inm_u32_t );

#ifdef _VSNAP_DEBUG_
void bt_print_btree_info(void);
#define BT_ALLOC_NODE() bt_alloc_node(__FUNCTION__, __LINE__)
void * bt_alloc_node(const char[], inm_32_t);
#define BT_FREE_NODE(node) bt_free_node(node, __FUNCTION__, __LINE__)
void bt_free_node(void **node_ptr, const char[], inm_32_t);
#define BT_ALLOC_AND_READ_NODE(b, p, i, c) \
	bt_alloc_and_read_node(b, p, i, c, __FUNCTION__, __LINE__)
inm_32_t bt_alloc_and_read_node(inm_bt_t *, void *, inm_32_t, void **,
				const char[], inm_32_t);
#else
#define BT_ALLOC_NODE() bt_alloc_node()
void * bt_alloc_node(void);
#define BT_FREE_NODE(node) bt_free_node(node)
void bt_free_node(void **node_ptr);
#define BT_ALLOC_AND_READ_NODE(b, p, i, c) bt_alloc_and_read_node(b, p, i, c)
inm_32_t bt_alloc_and_read_node(inm_bt_t *, void *, inm_32_t, void **);
#endif

/*
 * This is ugly way of making sure some conditions are met
 * at compile time
 */
#define COMPILE_TIME_ASSERT(expr)   \
	char constraint[expr]

#endif /* _DISK_BTREE_CORE_H */
