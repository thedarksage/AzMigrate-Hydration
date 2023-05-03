
/*****************************************************************************
 * disk_btree_core.c
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
 * Description:         Core BTree routines
 *
 * Revision:            $Revision: 1.6 $
 * Checked in by:       $Author: lamuppan $
 * Last Modified:       $Date: 2015/02/24 03:29:23 $
 *
 * $Log: disk_btree_core.c,v $
 * Revision 1.6  2015/02/24 03:29:23  lamuppan
 * Bug #1619650, 1619648, 1619642, 1619640, 1619637, 1619636, 1619635: Fix for policheck
 * bugs for Filter and Vsnap drivers. Please refer to bugs in TFS for more details.
 *
 * Revision 1.5  2014/10/10 08:04:05  praweenk
 * Bug ID:917800
 * Description: Safe API changes for filter and VSNAP driver cross platform.
 *
 * Revision 1.4  2011/01/11 05:42:35  chvirani
 * 14377: Changes to support linux kernel 2.6.32 upwards
 *
 * Revision 1.3  2010/03/10 12:08:43  chvirani
 * Multithreaded Driver
 *
 * Revision 1.2  2009/05/19 13:58:37  chvirani
 * Solaris 9 support. Change memset to memzero()
 *
 * Revision 1.1  2009/04/28 12:34:38  cswarna
 * Adding these files as part of vsnap driver code re-org
 *
 * Revision 1.7  2008/08/12 22:27:23  praveenkr
 * fix for #5404: Decoupled the mount point away from creation of vsnap
 * Moved the decision on the device name away from driver into the CX
 * & 'cdpcli'
 *
 * Revision 1.6  2008/04/01 13:04:09  praveenkr
 * added memory debugging support (for debug build only)
 *
 * Revision 1.5  2008/03/25 12:59:52  praveenkr
 * code cleanup for Solaris
 *
 * Revision 1.4  2007/12/04 12:53:38  praveenkr
 * fix for the following defects -
 * 	4483 - Under low memory condition, vsnap driver suspended forever
 * 	4484 - vsnaps & dataprotection not responding
 * 	4485 - vsnaps utilizing more than 90% CPU
 *
 * Revision 1.3  2007/09/14 13:11:16  sudhakarp
 * Merge from DR_SCOUT_4-0_DEV/DR_SCOUT_4-1_BETA2 to HEAD ( Onbehalf of RP).
 *
 * Revision 1.1.2.10  2007/08/21 15:22:23  praveenkr
 * 1. replaced all kfrees with a wrapper dr_vdev_kfree, which will force null
 *    the ptr, to easily capture mem leaks/mem corruption
 * 2. added comments
 *
 * Revision 1.1.2.9  2007/07/24 12:58:30  praveenkr
 * virtual volume (volpack) implementation
 *
 * Revision 1.1.2.8  2007/07/17 13:53:25  praveenkr
 * clean-up - unix-style naming, tab-spacing = 8, 78 col
 *
 * Revision 1.1.2.7  2007/05/25 13:04:50  praveenkr
 * deleted unused functions - left behind during porting
 * DiskBtreeInitBtreeMapFile
 *
 * Revision 1.1.2.6  2007/05/25 12:52:25  praveenkr
 * deleted unused functions - left behind during porting
 * DiskBtreeCopyRootNode, DiskBtreeCtr, DiskBtreeDtr,
 * DiskBtreeInitFromDiskFileName
 *
 * Revision 1.1.2.5  2007/05/25 09:45:43  praveenkr
 * added comments & partial clean-up of code
 *
 * Revision 1.1.2.4  2007/05/22 11:50:45  praveenkr
 * removed a unnecessary debug print
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

#include "disk_btree_core.h"
#include "vsnap_common.h"
#include "vsnap_kernel.h"
#include "vsnap_kernel_helpers.h"
#include "disk_btree_cache.h"

/* to facilitate ident */
static const char rcsid[] = "$Id: disk_btree_core.c,v 1.6 2015/02/24 03:29:23 lamuppan Exp $";

#ifdef _MEM_ALIGN_
/*
 * Assert that we have enough space to pad in btree node
 * Currently only helpful for memory alignment code
 */
static void
bt_compile_time_check()
{
	COMPILE_TIME_ASSERT( BT_NODE_PAD_SIZE <= BT_NODE_SPACE_FREE );
	return;
}

#endif

#ifdef _VSNAP_DEBUG_

void
bt_print_btree_info(void)
{
	DBG("Node Allocation Size = %d", (int)(BT_NODE_ALLOC_SIZE));
	DBG("Node Size In Use = %d", (int)(BT_NODE_SPACE_USED));
	DBG("Node Space That Can Be Padded = %d", (int)(BT_NODE_SPACE_FREE));
	DBG("Node Pad Size = %d", (int)(BT_NODE_PAD_SIZE));
	DBG("Node Size That Can Be Used = %d", (int)(BT_NODE_SIZE));
	DBG("Sizeof Of Key = %d", (int)(sizeof(inm_bt_key_t)));
	DBG("Max Keys = %d", (int)(INM_MAX_KEYS(sizeof(inm_bt_key_t))));

}

#endif

/* In debug mode, mark the allocated nodes with info about the caller */
void *
#ifdef _VSNAP_DEBUG_
bt_alloc_node(const char func[], inm_32_t line)
#else
bt_alloc_node(void)
#endif
{
	char *node_ptr;

#ifdef _VSNAP_DEBUG_
	node_ptr = INM_ALLOC_WITH_INFO(BT_NODE_ALLOC_SIZE, GFP_NOIO,
				       func, line);
#else
	node_ptr = INM_ALLOC(BT_NODE_ALLOC_SIZE, GFP_NOIO);
#endif

	if ( node_ptr ) {
		DEV_DBG("Got %p, sending %p", node_ptr, (char *)node_ptr +
							BT_NODE_PAD_SIZE);
		node_ptr = node_ptr + BT_NODE_PAD_SIZE;
		BT_NODE_CACHED(node_ptr) = 0;
	} else {
		DEV_DBG("Cannot allocate node");
	}

	return node_ptr;
}


/*
 * In debug mode, mark the freed nodes with function/line
 * of the caller
 */
void
#ifdef _VSNAP_DEBUG_
bt_free_node(void **node_ptr, const char func[], inm_32_t line)
#else
bt_free_node(void **node_ptr)
#endif
{
	char *mem_ptr = *node_ptr;

	if ( !BT_NODE_CACHED(mem_ptr) ) {
		mem_ptr = mem_ptr - BT_NODE_PAD_SIZE;
		DEV_DBG("Got %p, freeing %p", *node_ptr, mem_ptr);

		#ifdef _VSNAP_DEBUG_
		INM_FREE_WITH_INFO(&mem_ptr, BT_NODE_ALLOC_SIZE, func, line);
		#else
		INM_FREE(&mem_ptr, BT_NODE_ALLOC_SIZE);
		#endif
	}

	#ifdef _VSNAP_DEBUG_
		*node_ptr = NULL;
	#endif
}


/*
 * get_free_file_offset
 *
 * DESC
 *      seek to the end of the file & return the offset
 */
static inm_32_t
get_free_file_offset(inm_bt_t *btree, inm_u64_t *file_offset)
{
	inm_32_t retval = 0;

	retval = INM_SEEK_FILE_END(btree->bt_map_filp, file_offset);
	if (retval < 0) {
		ERR("SEEK FILE failed");
	}

	return retval;
}

/*
 * bt_write_to_file
 *
 * DESC
 *      wrapper to write to the file
 *
 * TODO
 *      this logic works only as far as len < PAGE_SIZE bcoz as of now
 *      dr_vs_page_???? works only in units of PAGE_SIZE
 */
static inm_32_t
bt_write_to_file(void *hdle, void *buf, inm_u64_t off, inm_u32_t len, inm_u32_t pad)
{
	inm_32_t		retval = 0;
	inm_vdev_page_data_t	*pd = NULL;

	pd = INM_ALLOC(sizeof(inm_vdev_page_data_t), GFP_NOIO);
	if (!pd) {
		ERR("no mem.");
		retval = -ENOMEM;
		goto out;
	}
	pd->pd_bvec_page = alloc_page(GFP_NOIO);
	if (!pd->pd_bvec_page) {
		ERR("no mem.");
		retval = -ENOMEM;
		goto fail;
	}
	pd->pd_bvec_offset = 0;
	pd->pd_disk_offset = 0;
	pd->pd_file_offset = off;
	pd->pd_size = len + pad;

	/*
	 * copy the data from the buffer into the allocated page
	 */
	inm_copy_buf_to_page(pd->pd_bvec_page, buf, len);

	/*
	 * zero the pad towards the end
	 */
	if ( pad )
		inm_bzero_page(pd->pd_bvec_page, len, pad);

	retval = inm_vdev_write_page(hdle, pd);
	if (retval < 0) {
		ERR("vdev write failed");
	}

	__free_page(pd->pd_bvec_page);

  fail:
	INM_FREE(&pd, sizeof(inm_vdev_page_data_t));
  out:
	return retval;
}

/*
 * bt_write
 *
 * DESC
 *      write into the map file
 */
static inm_32_t
bt_write(inm_bt_t *btree, inm_u64_t offset, void *page, inm_u32_t size, inm_u32_t pad)
{
	inm_32_t	retval = 0;

	ASSERT((!btree->bt_map_filp) || (!page));

	retval = bt_write_to_file(btree->bt_map_filp, page, offset, size, pad);
	if (retval < 0) {
		ERR("writef failed");
	}

	return retval;
}

/*
 * bt_split_node
 *
 * DESC
 *      Split the Child node, which is full.
 *
 * ALGO
 *      1. create a new node
 *      2. move the right half of the child (full) node into this new node
 *      3. insert an entry (key) in the parent with left pointer pointing to
 *         orig child and the right pointer to new node
 */
static enum BTREE_STATUS
bt_split_node(inm_bt_t *btree, void *parent_node, inm_u64_t parent_node_offset,
	      inm_32_t index, void *child_node, inm_u64_t child_node_offset,
	      inm_bt_key_t *new_key)
{
	void			*new_node = NULL;
	inm_32_t		i = 0;
	inm_32_t		ret = 0;
	inm_u64_t		new_offset = 0;
	enum BTREE_STATUS	retval = BTREE_SUCCESS;

	/*
	 * To prevent cache corruption, we need to make copies of
	 * the nodes which we can restore to in case of a failure
	 */

	DEV_DBG("create a new node");
	new_node = BT_ALLOC_NODE();
	if (!new_node) {
		ERR("no mem.");
		retval = BTREE_FAIL;
		goto out;
	}
	memzero(new_node, BT_NODE_SIZE);
	INM_SET_NUM_KEYS(new_node, btree->bt_min_keys);

	/*
	 * copy half the keys (left-half) from the old full node to new node
	 */
	DEV_DBG("copy left-half from the old node to the new node");
	for (i = 0; i < btree->bt_min_keys; i++) {
		if (memcpy_s(INM_NODE_PTR(btree, i, new_node),
		       INM_MIN(sizeof(inm_bt_key_t),
			       (size_t)btree->bt_key_size),
		       INM_NODE_PTR(btree, i + btree->bt_min_keys + 1,
				    child_node),
		       INM_MIN(sizeof(inm_bt_key_t),
			       (size_t)btree->bt_key_size))) {

			retval = BTREE_FAIL;
			goto fail;
		}
	}

	/*
	 * if the fullnode is not a leaf, then copy the child pointers too.
	 */
	if (0 != INM_CHILD_VAL(0, child_node)) {
		DEV_DBG("full node not a leaf so copying the child ptrs");
		for (i = 0; i < (btree->bt_min_keys + 1); i++)
			(*(inm_u64_t *)INM_CHILD_PTR(i, new_node)) =
			    (*(inm_u64_t *)INM_CHILD_PTR(
				i + btree->bt_min_keys + 1, child_node));
	}

	/*
	 * reset number of nodes in the previously full node.
	 */
	INM_SET_NUM_KEYS(child_node, btree->bt_min_keys);

	/*
	 * adjust the child pointers in the parent node where new entry is
	 * going to be added.
	 */
	DEV_DBG("right shift the child pointers to accomodate the new entry");
	for(i = INM_GET_NUM_KEYS(parent_node);
	    ((INM_GET_NUM_KEYS(parent_node) != 0) && (i >= (index + 1)));i--) {
		(*(inm_u64_t *)INM_CHILD_PTR(i + 1, parent_node)) =
		    (*(inm_u64_t *)INM_CHILD_PTR(i, parent_node));
	}

	/*
	 * get a free file offset from map file to insert the new node
	 */
	DEV_DBG("get a fee map file offset to insert the new node");
	ret = get_free_file_offset(btree, &new_offset);
	if (ret < 0) {
		ERR("free map file failed");
		retval = BTREE_FAIL;
		goto fail;
	}

	/*
	 * set the child pointer of parent to newnode
	 */
	DEV_DBG("set the child ptr of the parent to point to the new node");
	(*(inm_u64_t *)INM_CHILD_PTR(index + 1, parent_node)) = new_offset;

	/*
	 * right shift the entries on the parent node to accomodate the key
	 * for the new node
	 */
	DEV_DBG("now right shift the nodes to accomodate the new node");
	for(i = INM_GET_NUM_KEYS(parent_node);
	    ((INM_GET_NUM_KEYS(parent_node) != 0) && (i >= index)); i--) {
		if (memcpy_s(INM_NODE_PTR(btree, i, parent_node),
		       INM_MIN(sizeof(inm_bt_key_t),
			       (size_t)btree->bt_key_size),
		       INM_NODE_PTR(btree, i - 1, parent_node),
		       INM_MIN(sizeof(inm_bt_key_t),
			       (size_t)btree->bt_key_size))) {

			retval = BTREE_FAIL;
			goto fail;
		}
	}

	/*
	 * copy the new key into the parent node
	 */
	DEV_DBG("copy the new key into the parent node");
	if (memcpy_s(INM_NODE_PTR(btree, index, parent_node),
	       INM_MIN(sizeof(inm_bt_key_t),(size_t)btree->bt_key_size),
	       INM_NODE_PTR(btree, btree->bt_min_keys, child_node),
	       INM_MIN(sizeof(inm_bt_key_t),(size_t)btree->bt_key_size))) {

		retval = BTREE_FAIL;
		goto fail;
	}

	/*
	 * increment the # of keys in the parent
	 */
	INM_INCR_NUM_KEYS(parent_node);

	/*
	 * flush all the data, v.i.z. parent, child & new nodes, to disk
	 */
	DEV_DBG("flush all nodes");
	ret = bt_write(btree, parent_node_offset, parent_node, BT_NODE_SIZE, BT_NODE_PAD_SIZE) ||
	    bt_write(btree, child_node_offset, child_node, BT_NODE_SIZE, BT_NODE_PAD_SIZE) ||
	    bt_write(btree, new_offset, new_node, BT_NODE_SIZE, BT_NODE_PAD_SIZE);

	/*
	 * NOTE: on failure bt_write returns negative value.
	 * 	(a || b) = 0	when a = b = 0
	 *	(a || b) = 1	when a <= 0 or b <= 0
	 * hence the value checked here is either for 0/1 and not for
	 * less than zero
	 */
	if ( ret ) {
		ERR("btree_write failed");
		retval = BTREE_FAIL;
	}

  fail:
	BT_FREE_NODE(&new_node);

  out:
	return retval;
}

/*
 * inm_vs_bt_insert
 *
 * DESC
 *      insert a new element into the btree
 *
 * ALGO
 *      1. if root node full
 *      2.   allocate a new root
 *      3.   set the old root as child of new root
 *      4.   write the new root
 *      5.   split the old root node
 *      6.   update the persistant header with new root info
 *      7.   insert the element using this new root
 *      8. else insert the element with the old root
 */
enum BTREE_STATUS
inm_vs_bt_insert(inm_bt_t *btree, inm_bt_key_t *new_key, inm_u32_t over_write_flag)
{
	void			*old_root = NULL;
	void			*new_root = NULL;
	inm_32_t		ret = 0;
	inm_u64_t		new_root_offset = 0;
	enum BTREE_STATUS	retval = BTREE_SUCCESS;

	/*
	 * If root node is full then split the node before insertion.
	 */
	if (btree->bt_max_keys == INM_GET_NUM_KEYS(btree->bt_root_node)) {

		bt_invalidate_cache(btree);

		DEV_DBG("root node full so split the node");
		old_root = btree->bt_root_node;	/* store the old root */

		/*
		 * allocate a new root node and set the node information
		 */
		new_root = BT_ALLOC_NODE();
		if (!new_root) {
			ERR("no mem.");
			retval = BTREE_FAIL;
			goto out;
		}

		memzero(new_root, BT_NODE_SIZE);
		INM_SET_NUM_KEYS(new_root, 0);
		*(inm_u64_t *)INM_CHILD_PTR(0, new_root) =
		    btree->bt_root_offset;

		/*
		 * get a free file offset within the btree map where the
		 * new root node can be written
		 */
		DEV_DBG("get a free file offset to insert the new root node");
		ret = get_free_file_offset(btree, &new_root_offset);
		if (ret < 0) {
			ERR("free map file failed");
			retval = BTREE_FAIL;
			goto fail;
		}

		/*
		 * write the new root into the map file
		 */
		DEV_DBG("write the new root node into the file");
		ret = bt_write(btree, new_root_offset, new_root, BT_NODE_SIZE,
			       BT_NODE_PAD_SIZE);
		if (ret < 0) {
			ERR("btree write failed");
			retval = BTREE_FAIL;
			goto fail;
		}

		/*
		 * split the node; the old_root becomes the left child of
		 * new root node and a new node is created and added as the
		 * right child of the new root node
		 */
		DEV_DBG("now split the old root node");
		retval = bt_split_node(btree, new_root, new_root_offset, 0,
				       old_root, btree->bt_root_offset,
				       new_key);
		if (BTREE_FAIL == retval) {
			ERR("split node failed");
			goto fail;
		}

		/*
		 * update the header with new root node details & write to
		 * the file
		 */
		DEV_DBG("update the header with the new root info");
		btree->bt_hdr->hdr_root_offset = new_root_offset;
		DBG("Root Offset = %llu\n", btree->bt_hdr->hdr_root_offset);
		DBG("Max Keys = %d\n", btree->bt_hdr->hdr_max_keys);
		DBG("Min Keys = %d\n", btree->bt_hdr->hdr_min_keys);
		DBG("Key Size = %d\n", btree->bt_hdr->hdr_key_size);
		DBG("Node Size = %u\n", btree->bt_hdr->hdr_node_size);
		ret = bt_write(btree, (inm_u64_t) 0, btree->bt_hdr,
			       BTREE_HEADER_SIZE, BT_HDR_PAD_SIZE);
		if (ret < 0) {
			ERR("btree write failed");
			retval = BTREE_FAIL;
			goto fail;
		}

		btree->bt_root_offset = new_root_offset;

		BT_NODE_CACHED(new_root) = 1;
		btree->bt_root_node = new_root;

		BT_NODE_CACHED(old_root) = 0;
		BT_FREE_NODE(&old_root);
	}

	/*
	 * insert the new key using this new root
	 */
	DEV_DBG("inser the new key into the new root");
	retval = inm_vs_bt_insert_nonfull(btree, btree->bt_root_node,
					  btree->bt_root_offset,
					  new_key, over_write_flag);
	if (retval < 0) {
		ERR("insert nonfull failed");
	}

  out:
	return retval;

  fail:
	BT_FREE_NODE(&new_root);
	goto out;
}

/*
 * read_from_file
 *
 * DESC
 *      wrapper to read from the file
 *
 * TODO
 *      this logic works only as far as len < PAGE_SIZE bcoz as of now
 *      dr_vs_page_???? works only in units of PAGE_SIZE
 */
static inm_32_t
read_from_file(void *hdle, void *buf, inm_u64_t off, inm_u32_t len)
{
	inm_32_t		retval = 0;
	inm_vdev_page_data_t	*pd = NULL;

	pd = INM_ALLOC(sizeof(inm_vdev_page_data_t), GFP_NOIO);
	if (!pd) {
		ERR("no mem.");
		retval = -ENOMEM;
		goto out;
	}

	/*
	 * allocate a temp page which will be used for reading from the file
	 */
	pd->pd_bvec_page = alloc_page(GFP_NOIO);
	if (!pd->pd_bvec_page) {
		ERR("no mem.");
		retval = -ENOMEM;
		goto fail;
	}
	pd->pd_bvec_offset = 0;
	pd->pd_disk_offset = 0;
	pd->pd_file_offset = off;
	pd->pd_size = len;
	DEV_DBG("read %d bytes @ %llu", len, off);

	retval = inm_vdev_read_page(hdle, pd);
        if (retval < 0) {
                ERR("read failed.");
        } else {
		inm_copy_page_to_buf(buf,pd->pd_bvec_page, len);
        }

	__free_page(pd->pd_bvec_page);

  fail:
	INM_FREE(&pd, sizeof(inm_vdev_page_data_t));
  out:
	return retval;
}

/*
 * bt_read
 *
 * DESC
 *      read from the map file
 */
inm_32_t
bt_read(inm_bt_t *btree, inm_u64_t offset, void *page, inm_u32_t size)
{
	inm_32_t retval = 0;

	ASSERT((!btree->bt_map_filp) || (!page));
	retval = read_from_file(btree->bt_map_filp, page, offset, size);
	if (retval < 0) {
		ERR("read from file failed");
	}

	return retval;
}

/*
 * bt_alloc_and_read_node
 *
 * DESC
 *      query the cache. In case its not there, allocate a new node
 *	and read node from map file. In debug mode, mark the node with
 *	caller info
 */
inm_32_t
#ifdef _VSNAP_DEBUG_
bt_alloc_and_read_node(inm_bt_t *btree, void *parent, inm_32_t idx,
		       void **child, const char func[], inm_32_t line)
#else
bt_alloc_and_read_node(inm_bt_t *btree, void *parent,
		       inm_32_t idx, void **child)
#endif
{
	inm_32_t retval = 0;
	void *node = NULL;

	node = bt_get_from_cache(btree, parent, idx);
	if ( node == NULL ) {
		DEV_DBG("Not found in cache");

#ifdef _VSNAP_DEBUG_
		node = bt_alloc_node(func, line);
#else
		node = bt_alloc_node();
#endif

		if ( node ) {
			retval = bt_read(btree, INM_CHILD_VAL(idx, parent),
					 node, BT_NODE_SIZE);
			if ( retval < 0 ) {
				ERR("Could not read the child %d of %p", idx,
				    parent);
				goto err;
			}
		}
	} else {
		DEV_DBG("Found in cache");
	}

	*child = node;

out:
	return retval;

err:
	BT_FREE_NODE(&node);
	goto out;
}

/*
 * inm_vs_bt_lookup
 *
 * DESC
 *      (recursively) search for an overlap for the given (offset,size) pair
 *
 * ALGO
 *      1. for the given node, read the keys in linear fashion
 *      2.    compare the keys against the search keys
 *      3.    if found copy into o/p param
 *      4.    if current key < required key then pick next key
 *      5.    if current key > required key then probably in the child
 *      6.       retrieve the child node & recursively call btree_search
 */
enum BTREE_STATUS
inm_vs_bt_lookup(inm_bt_t *btree, void *node, inm_bt_key_t *search_key,
		 inm_bt_key_t *match_key, void **match_node)
{
	void			*child_node = NULL;
	inm_32_t		ret = 0;
	inm_32_t		low;
	inm_32_t		high;
	inm_32_t		mid;
	inm_bt_key_t		*cur_key = NULL;
	enum BTREE_STATUS	retval = BTREE_FAIL;
	inm_32_t		alloc_node = 0;

	do {
		DEV_DBG("Node = %p", node);
		if ( !INM_GET_NUM_KEYS(node) ) {
			/*
			 * not an error but no key so failure
			 */
			DEV_DBG("no keys in the node to search");
			break;
		}

		low = -1;
		high = INM_GET_NUM_KEYS(node);

		/*
		* Search thro the node using binary search ...
		* ...node-key = search-key - key found
		* ...not found - high would be pointing to immediate next higher entry
		* 		  get the left child of high and continue search.
		* 		  In case high = num_keys and since index starts
		*		  from zero, this automatically translates to right
		*		  child of the last key.
		*/

		while ( 1 ) {
			mid = (high - low) / 2;
			if ( 0 == mid )
				break;

			mid += low;
			cur_key = (inm_bt_key_t *)INM_NODE_PTR(btree,mid,node);

			if (INM_CMP_LESS(cur_key, search_key)) {
				/* cur key off < search key off but no overlap */
				low = mid;
			} else if (INM_CMP_GREATER(cur_key, search_key)) {
				/* cur key off > search key off but no overlap */
				high = mid;
			} else {
				DEV_DBG("match");
				if (memcpy_s(match_key, INM_MIN(sizeof(inm_bt_key_t),
				       (size_t)btree->bt_key_size),
				       cur_key, INM_MIN(sizeof(inm_bt_key_t),
				       (size_t)btree->bt_key_size))) {

					goto out;
				}

				retval = BTREE_SUCCESS;
				*match_node = node;
				goto out;
			}
		}

		/*
		* if here and no child then search key not found
		*/
		if (0 == INM_CHILD_VAL(0, node)) {
			DEV_DBG("searched the entire tree & failed");
			if ( alloc_node ) {
				DEV_DBG("Freeing child");
				BT_FREE_NODE(&node);
			}
			break;
		} else {
			DEV_DBG("read the child node");
			ret = BT_ALLOC_AND_READ_NODE(btree, node, high,
						     &child_node);
			if ( alloc_node ) {
				DEV_DBG("Freeing Node");
				BT_FREE_NODE(&node);
			} else {
				alloc_node = 1;
			}

			if (ret < 0) {
				ERR("Cannot read child node");
				break;
			}

			DEV_DBG("Child = %p", child_node);
			node = child_node;
		}
	} while(1);

out:
	DEV_DBG("===");
	return retval;
}


/*
 * bt_move_insert_key
 *
 * DESC
 *      insert a key in the node @ index posn
 */
static void
bt_move_insert_key(inm_bt_t *btree, void *node, inm_bt_key_t key,
		   inm_32_t index)
{
	inm_bt_key_t temp;

	while (index <= INM_GET_NUM_KEYS(node)) {
		memcpy_s(&temp, INM_MIN(sizeof(inm_bt_key_t),
			       (size_t) btree->bt_key_size),
		       INM_NODE_PTR(btree, index, node),
		       INM_MIN(sizeof(inm_bt_key_t),
			       (size_t) btree->bt_key_size));
		memcpy_s(INM_NODE_PTR(btree, index, node),
		       INM_MIN(sizeof(inm_bt_key_t),
			       (size_t) btree->bt_key_size),
		       &key, INM_MIN(sizeof(inm_bt_key_t),
			       (size_t) btree->bt_key_size));
		key = temp;
		index++;
	}
}

/*
 * bt_insert_non_leaf_node
 *
 * DESC
 *      insert a non-leaf node into the btree
 */
static enum BTREE_STATUS
bt_insert_non_leaf_node(inm_bt_t *btree, void *node, inm_u64_t node_offset,
			inm_bt_key_t *new_key, inm_u32_t over_write_flag)
{
	void			*child = NULL;
	inm_32_t		ret = 0;
	inm_32_t		index = 0;
	inm_u64_t		child_offset = 0;
	inm_bt_key_t		*node_key;
	enum BTREE_STATUS	 retval = BTREE_SUCCESS;

	index = INM_GET_NUM_KEYS(node) - 1;
	while (index >= 0) {
		node_key = (inm_bt_key_t *)INM_NODE_PTR(btree, index, node);

		/*
		 * check for overlap and position the new_key based on its
		 * offset and the offset of other keys in the node
		 */
		if (INM_CMP_LESS(new_key, node_key)) {
			/* new-key off < node-key off & no overlap*/
			DEV_DBG("new-key < node-key");
			index--;
		} else if (INM_CMP_GREATER(new_key, node_key)) {
			/* node-key off < new-ke off & no overlap */
			DEV_DBG("node-key < new-key");
			break;
		} else { /* overlap */
			DEV_DBG("overlap");
			ASSERT(!btree->bt_callback);

			/*
			 * this callback function would gauge the overlap
			 * and re-arrange the (offset,len) tuples into
			 * non-overlapping list; with the overlap, the data
			 * would be over-written only when the write happens
			 * on the VSNAP
			 */
			ret = btree->bt_callback((void *)node_key,
						 (void *)new_key,
						 btree->bt_callback_param,
						 over_write_flag);
			if (ret < 0) {
				ERR("callback failed");
				retval = BTREE_FAIL;
				goto out;
			}

			if (over_write_flag) {
				ret = bt_write(btree, node_offset, node,
					       BT_NODE_SIZE, BT_NODE_PAD_SIZE);
				if (ret < 0) {
					ERR("write failed");
					retval = BTREE_FAIL;
				} else
					retval= BTREE_SUCCESS_MODIFIED;
				goto out;
			}
			retval = BTREE_SUCCESS;
			goto out;
		}
	}

	/*
	 * index can be negative @ this point, if we did not find
	 * a key, which is greater than the search-key. then we need
	 * to insert into the child @ 0th index. similarly,
	 * if index = 'k' then the insert should be @ 'k+1' child.
	 */
	DEV_DBG("index - %d", index);
	index++;
	child_offset = INM_CHILD_VAL(index, node);
	ret = BT_ALLOC_AND_READ_NODE(btree, node, index, &child);
	if ( ret < 0 ) {
		ERR("Cannot read child node");
		retval = BTREE_FAIL;
		goto out;
	}

	/*
	 * if the child if full the split the node
	 */
	if (btree->bt_max_keys == INM_GET_NUM_KEYS(child)) {
		DEV_DBG("child node full.");
		retval = bt_split_node(btree, node, node_offset, index, child,
				       child_offset, new_key);

		/* We will re-read the child, so free it now */
		BT_FREE_NODE(&child);

		if ( node == btree->bt_root_node )
			bt_invalidate_cache(btree);

		if (BTREE_FAIL == retval) {
			ERR("split node failed");
			goto out;
		}

		/*
		 * check the node-key @ index, if less then insert at
		 * the next child if same then call "callback" to
		 * seperately insert the overlaping portion &
		 * non-overlaping portion
		 */
		node_key = (inm_bt_key_t *)INM_NODE_PTR(btree, index, node);
		if (INM_CMP_LESS(node_key, new_key)) {
			/* node-key off < new-key & no over lap */
			DEV_DBG("node-key < new key");
			index++;
		} else if (INM_CMP_EQUAL(node_key, new_key)) {
			/* over lap */
			DEV_DBG("over lap");
			ASSERT(!btree->bt_callback);
			ret =  btree->bt_callback((void *)node_key,
						  (void *) new_key,
						  btree->bt_callback_param,
						  over_write_flag);
			if (ret < 0) {
				ERR("callback failed");
				retval = BTREE_FAIL;
				goto out;
			}
			retval = BTREE_SUCCESS_MODIFIED;
			if (over_write_flag) {
				ret = bt_write(btree, node_offset, node,
					       BT_NODE_SIZE, BT_NODE_PAD_SIZE);
				if (ret < 0) {
					ERR("write failed");
					retval = BTREE_FAIL;
				}
			}
			goto out;
		}

		/*
		 * If its the new node after split, we are sure cache does
		 * not have it, so we are safe
		 */
		ret = BT_ALLOC_AND_READ_NODE(btree, node, index, &child);
		if ( ret < 0 ) {
			ERR("Cannot read child node");
			retval = BTREE_FAIL;
			goto out;
		}
		child_offset = INM_CHILD_VAL(index, node);
	}

	DEV_DBG("child node not full; insert newkey into this node");
	retval = inm_vs_bt_insert_nonfull(btree, child, child_offset,
					  new_key, over_write_flag);
	if ( retval == BTREE_FAIL ) {
		ERR("insert nonfull failed");
	}

	BT_FREE_NODE(&child);

  out:
	return retval;
}

/*
 * run through the entire list of keys available on 'THIS' node
 * until -
 *	1. you find an index where this new key be inserted
 *	   and still maintain the sorting order of the list
 *	2. there is an overlap; if there was any then break
 *	   them into overlapping & non-overlapping parts and
 *	   insert them back into the offset_split list.
 *
 * at the caller, the process of write goes in a loop until
 * this offset_split list is empty; if an over-lap is found
 * then break and re-visit the (offset,len) tuple again.
 */
static enum BTREE_STATUS
bt_insert_leaf_node(inm_bt_t *btree, void *node, inm_u64_t node_offset,
		    inm_bt_key_t *new_key, inm_u32_t over_write_flag)
{
	inm_32_t		ret = 0;
	inm_32_t		index = 0;
	inm_bt_key_t		*node_key;
	enum BTREE_STATUS	 retval = BTREE_SUCCESS;

	while (index < INM_GET_NUM_KEYS(node)) {
		node_key = (inm_bt_key_t *)INM_NODE_PTR(btree, index, node);

		/*
		 * check for overlap and position the new_key based on its
		 * offset and the offset of other keys in the node
		 */
		if (INM_CMP_LESS(new_key, node_key)) {
			/* new_key off < node_key off and no overlap */
			DEV_DBG("newkey < nodekey.");
			break;
		} else if (INM_CMP_GREATER(new_key, node_key)) {
			/* node_key off < new_key off and no overlap */
			DEV_DBG("newkey > nodekey");
			index++;
		} else {
			/* overlap */
			DEV_DBG("overlap");
			ASSERT(!btree->bt_callback);

			/*
			 * this callback function would gauge the overlap
			 * and re-arrange the (offset,len) tuples into
			 * non-overlapping list; with the overlap, the data
			 * would be over-written only when the write happens
			 * on the VSNAP
			 */
			ret = btree->bt_callback((void *)node_key,
						 (void *)new_key,
						 btree->bt_callback_param,
						 over_write_flag);
			if (ret < 0) {
				ERR("callback failed");
				retval = BTREE_FAIL;
				goto out;
			}

			if (over_write_flag) {
				ret = bt_write(btree, node_offset, node,
					       BT_NODE_SIZE, BT_NODE_PAD_SIZE);
				if (ret < 0) {
					ERR("write failed");
					retval = BTREE_FAIL;
				} else {
					retval = BTREE_SUCCESS_MODIFIED;
				}
				goto out;
			}
			retval = BTREE_SUCCESS;
			goto out;
		}
	}

	/*
	 * new_key's offset is greater than all the current keys in the node
	 * insert the new key @ index-th posn and update the # of keys
	 * in the node and write to disk
	 */
	DEV_DBG("found the index - %d", index);
	bt_move_insert_key(btree, node, *new_key, index);
	INM_INCR_NUM_KEYS(node);
	ret = bt_write(btree, node_offset, node, BT_NODE_SIZE,
		       BT_NODE_PAD_SIZE);
	if (ret < 0) {
		ERR("btree write failed");
		retval = BTREE_FAIL;
		goto out;
	}
	retval = BTREE_SUCCESS_MODIFIED;

  out:
	return retval;
}


/*
 * inm_vs_bt_insert_nonfull
 *
 * DESC
 *      insert a new_key in node which is non-full
 *
 * ALGO
 *      1. if an overlapping key is found in the node then break new key into
 *         non-overlapping portion + over-lapping portion (call callback) and
 *         insert (or update) each of the portions seperately.
 *         a. if the node is a child node then insert by comparing the newkey
 *            from the begining of the node
 *         b. if the node is a not a child node then insert by comparing the
 *            newkey from the end of the node
 */
enum BTREE_STATUS
inm_vs_bt_insert_nonfull(inm_bt_t *btree, void *node, inm_u64_t node_offset,
			 inm_bt_key_t *new_key, inm_u32_t over_write_flag)
{
	enum BTREE_STATUS	 retval = BTREE_SUCCESS;

	if (!INM_CHILD_VAL(0, node)) {
		DEV_DBG("leaf node");
		retval = bt_insert_leaf_node(btree, node, node_offset, new_key,
					     over_write_flag);
		if (retval < 0) {
			ERR("insert leaf node failed");
		}
	} else {
		DEV_DBG("non leaf node");
		retval = bt_insert_non_leaf_node(btree, node, node_offset,
						 new_key, over_write_flag);
		if (retval < 0) {
			ERR("insert non leaf node failed");
		}
	}

	return retval;
}

/*
 * inm_bt_ctr
 *
 * DESC
 *	btree constructor
 *      initialize a btree for a given key_size
 */
inm_bt_t *
inm_bt_ctr(inm_u32_t key_size)
{
	inm_bt_t	*btree = NULL;
	inm_u32_t	order = 0;

	btree = INM_ALLOC(sizeof(inm_bt_t), GFP_NOIO);
	if (!btree) {
		ERR("no mem.");
		goto out;
	}
	memzero(btree, sizeof(inm_bt_t));	/* NULLs all the ptrs */

	/*
	 * set the min, max keys & also the keysize for the btree
	 */
	btree->bt_max_keys = INM_MAX_KEYS(key_size);
	if (0 == (btree->bt_max_keys % 2)) {
		btree->bt_max_keys -= 1;
	}
	order = (btree->bt_max_keys + 1) / 2;
	btree->bt_min_keys = order - 1;
	btree->bt_key_size = key_size;
	btree->bt_cache = NULL;
	INM_INIT_MUTEX(&btree->bt_cache_mutex);

  out:
	return btree;
}

/*
 * inm_bt_dtr
 *
 * DESC
 *	btree destructor
 */
void
inm_bt_dtr(inm_bt_t *btree)
{
	if ( btree->bt_cache )
		bt_destroy_cache(btree);

	if (btree->bt_root_node) {
		BT_NODE_CACHED(btree->bt_root_node) = 0;
		BT_FREE_NODE(&btree->bt_root_node);
	}

	if (btree->bt_hdr)
		INM_FREE(&btree->bt_hdr, BTREE_HEADER_SIZE);

	btree->bt_map_filp = NULL;
}

/*
 * read_bt_hdr_from_map
 *
 * DESC
 *      retrieve the header from the map file
 */
static inm_32_t
read_bt_hdr_from_map(inm_bt_t *btree)
{
	inm_32_t retval = 0;

	ASSERT(!btree->bt_map_filp);
	if (!btree->bt_hdr) {
		btree->bt_hdr = INM_ALLOC(BTREE_HEADER_SIZE, GFP_NOIO);
		if (!btree->bt_hdr) {
			ERR("no mem.");
			retval = -ENOMEM;
			goto out;
		}
	} else {
		DEV_DBG("header has already been read.");
		goto out;
	}

	retval = read_from_file(btree->bt_map_filp, btree->bt_hdr,
				(inm_u64_t)0, BTREE_HEADER_SIZE);
        if (retval < 0) {
		ERR("read failed.");
		goto read_failed;
	}

	DEV_DBG("Root Offset = %llu\n", btree->bt_hdr->hdr_root_offset);
	DEV_DBG("Max Keys = %d\n", btree->bt_hdr->hdr_max_keys);
	DEV_DBG("Min Keys = %d\n", btree->bt_hdr->hdr_min_keys);
	DEV_DBG("Key Size = %d\n", btree->bt_hdr->hdr_key_size);
	DEV_DBG("Node Size = %u\n", btree->bt_hdr->hdr_node_size);

  out:
	return retval;

  read_failed:
	INM_FREE(&btree->bt_hdr, BTREE_HEADER_SIZE);
	goto out;
}

/*
 * read_bt_root_node_from_map
 *
 * DESC
 *      retrieve the root node from the map file
 */
static inm_32_t
read_bt_root_node_from_map(inm_bt_t *btree)
{
	inm_32_t retval = 0;

	ASSERT(!btree->bt_map_filp);

	btree->bt_root_node = BT_ALLOC_NODE();
	if (!btree->bt_root_node) {
		ERR("no mem.");
		retval = -ENOMEM;
		goto out;
	}

        retval = read_from_file(btree->bt_map_filp, btree->bt_root_node,
				btree->bt_hdr->hdr_root_offset,	BT_NODE_SIZE);
	if (retval < 0) {
		ERR("read failed.");
		goto read_failed;
	}

	BT_NODE_CACHED(btree->bt_root_node) = 1;

  out:
	return retval;

  read_failed:
	BT_FREE_NODE(&btree->bt_root_node);
	goto out;
}

/*
 * inm_vs_init_bt_from_map
 *
 * DESC
 *      initialize a btree from the given map file handle
 */
inm_32_t
inm_vs_init_bt_from_map(inm_bt_t *btree, void *btree_map_filp)
{
	inm_32_t retval = 0;

	btree->bt_map_filp = btree_map_filp;
	DEV_DBG("read bt hdr from the map file");
        retval = read_bt_hdr_from_map(btree);
	if (retval < 0) {
		ERR("read_bt_hdr_from_map failed.");
		goto out;
	}

	btree->bt_max_keys = btree->bt_hdr->hdr_max_keys;
	btree->bt_min_keys = btree->bt_hdr->hdr_min_keys;
	btree->bt_key_size = btree->bt_hdr->hdr_key_size;
	btree->bt_root_offset = btree->bt_hdr->hdr_root_offset;

	/*
	 * Ideally this should have gone to ctr() to complement destroy_cache()
	 * in dtr() but we have seen cases where on disk max_keys differ.
	 * So init_cache() after reading the header.
	 */
	retval = bt_init_cache(btree);
	if( retval < 0 ) {
		ERR("Cannot initialise cache");
		goto cache_init_failed;
	}

	DEV_DBG("read bt root node from the map file");
        retval = read_bt_root_node_from_map(btree);
	if (retval < 0) {
		ERR("read_bt_root_node_from_map failed.");
		goto read_root_node_failed;
	}
out:
	return retval;

read_root_node_failed:
	bt_destroy_cache(btree);

cache_init_failed:
	INM_FREE(&btree->bt_hdr, BTREE_HEADER_SIZE);
	goto out;
}

/*
 * inm_vs_bt_write_hdr
 *
 * DESC
 *      write the header into the btree map file
 */
inm_32_t
inm_vs_bt_write_hdr(inm_bt_t *btree, void *btree_hdr)
{
	inm_32_t retval = 0;

	if (memcpy_s(btree->bt_hdr, BTREE_HEADER_SIZE,
				btree_hdr, BTREE_HEADER_SIZE)) {
		retval = -EFAULT;
		goto out;
	}

	retval = bt_write(btree, 0, btree->bt_hdr, BTREE_HEADER_SIZE, BT_HDR_PAD_SIZE);
	if (retval < 0) {
		ERR("btree write failed");
	}

out:
	return retval;
}


/*
 * inm_bt_copy_hdr
 *
 * DESC
 *      retreive the header from the btree struct
 */
inm_32_t
inm_bt_copy_hdr(inm_bt_t *btree, void *btree_hdr)
{
	inm_32_t retval = 0;

	ASSERT(!btree->bt_map_filp);
	retval = read_bt_hdr_from_map(btree);
	if (retval < 0) {
		ERR("get header failed");
		goto out;
	}

	if (memcpy_s(btree_hdr, BTREE_HEADER_SIZE, btree->bt_hdr, BTREE_HEADER_SIZE)) {
		retval = -EFAULT;
		goto out;
	}

  out:
	return retval;
}
