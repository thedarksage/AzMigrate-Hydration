#include "common.h"
#include "disk_btree_core.h"
#include "disk_btree_cache.h"

#ifdef _VSNAP_DEBUG_
#include "vsnap_kernel.h"
#include "vdev_helper.h"

inm_32_t
inm_validate_btree_cache(inm_vdev_t *vdev)
{
	inm_bt_t *btree = ((inm_vs_info_t *)vdev->vd_private)->vsi_btree;
	void *tmp, *cache_ptr;
	int i = 0;
	int retval = 0;

	if( !INM_TEST_BIT(VSNAP_DEVICE, &vdev->vd_flags) ) {
		ERR("Not a vsnap device");
		return -ENODEV;
	}

	if ( !BT_ROOT(btree) ) {
		INFO("Blank BTREE");
		return 0;
	}

	tmp = BT_ALLOC_NODE();
	if ( !tmp ) {
		ERR("Cannot allocate memory for nodes");
		retval = -ENOMEM;
		goto out;
	}

	INFO("Number of cached nodes = %d", INM_GET_NUM_KEYS(BT_ROOT(btree)) + 1);

	for( i = 0; i <= btree->bt_max_keys; i++ ) {

		if ( i <= INM_GET_NUM_KEYS(BT_ROOT(btree)) ) {
			/*
			 * If the node is in cache, read in pvt mem and
			 * cmp pvt with cache data
			 */
			if ( (cache_ptr = BT_CACHE_SLOT(btree, i)) ) {

				retval = bt_read(btree,
						INM_CHILD_VAL(i,BT_ROOT(btree)),
						tmp, BT_NODE_SIZE);

				if ( retval < 0 ) {
					ERR("Failed to read first level node %d", i);
					goto out;
				}

				if ( memcmp(tmp, cache_ptr, BT_NODE_SIZE) ) {
					ERR("Cache Corrupted");
					retval = -EINVAL;
					goto out;
				}

			} else
				INFO("Node %d not in cache", i);
		} else {
			if ( BT_CACHE_SLOT(btree, i) ) {
				ERR("Node present when should not be");
				retval = -EINVAL;
				goto out;
			}
		}
	}

out:
	if ( tmp )
		BT_FREE_NODE(&tmp);

	return retval;
}

#endif


/*
 * If required, allocate space for node and read it into mem
 * If it fails, mark the slot as NULL
 */
static void
bt_alloc_and_read_cache(inm_bt_t *btree, inm_32_t child_node_idx)
{
	void *tmp = NULL;
	inm_32_t retval = 0;

	tmp = BT_ALLOC_NODE();
	if ( !tmp ) {
		ERR("Cannot allocate memory for nodes");
		goto out;
	}

	DEV_DBG("Reading from offset %llu",
	    INM_CHILD_VAL(child_node_idx, BT_ROOT(btree)));

	retval = bt_read(btree, INM_CHILD_VAL(child_node_idx, BT_ROOT(btree)),
			 tmp, BT_NODE_SIZE);
	if ( retval < 0 ) {
		ERR("Failed to read first level node %d", child_node_idx);
		BT_FREE_NODE(&tmp);
		goto out;
	}

	DEV_DBG("Successfully read level = 1, node = %d", child_node_idx);

	BT_NODE_CACHED(tmp) = 1;
	BT_CACHE_SLOT(btree, child_node_idx) = tmp;
	btree->bt_num_cached_nodes++;

out:
	return;
}

/*
 * Check if the parent node is root, if true, send the cached page
 * the node may not be in cache in which case we try and get
 * the node into cache now. If the new attempt fails as well, return
 * NULL which should be handled by the caller
 */
void *
bt_get_from_cache(inm_bt_t *btree, void *node, inm_32_t child_node_idx)
{
	void *retptr = NULL;

	if ( !BT_ROOT(btree) )
		return NULL;

	DEV_DBG("Request for %d node", child_node_idx);
	if ( node == BT_ROOT(btree) ) {

		INM_ACQ_MUTEX(&btree->bt_cache_mutex);

		/*
		 * Once here, the request has to be valid
		 * but we may not have it in cache
		 */
		retptr = BT_CACHE_SLOT(btree, child_node_idx);

		if ( !retptr ) {
			DEV_DBG("level = 1, node = %d not found in cache",
							child_node_idx);

			/* If not in cache, read it and send cached node */
			bt_alloc_and_read_cache(btree, child_node_idx);
			retptr = BT_CACHE_SLOT(btree, child_node_idx);
#ifdef _VSNAP_DEBUG_
			if ( !retptr ) {
				ERR("Cache failed");
				ALERT();
			}
#endif
		}

		INM_REL_MUTEX(&btree->bt_cache_mutex);
	}

	DEV_DBG("Returning %p", retptr);
	return retptr;
}

/*
 * Invalidates the cache with first level of Btree
 *
 * Should be called in three circumstances:
 *      o Initialising vsnap
 *      o When root has changed i.e. when root is full and a
 *        new root is allocated
 *      o When trying to insert a key into a full cached node
 *        which is then split
 */

void
bt_invalidate_cache(inm_bt_t *btree)
{
	inm_u32_t i = 0;
	void *tmp;

	if ( !BT_ROOT(btree) )
		return;

	DBG("Invalidating cache");

	/*
	 * We do not need this mutex as this will only happen in
	 * write path which is already protected by btree lock
	 * This is just for correctness
	 */

	INM_ACQ_MUTEX(&btree->bt_cache_mutex);

	/*
	 * We just free all the first level nodes as they are now invalid
	 * The new level 1 cached nodes would be populated on need basis
	 * by bt_get_from_cache()
	 */

	for( i = 0; i <= btree->bt_max_keys; i++ ) {
		if ( BT_CACHE_SLOT(btree, i ) ) {
			tmp = BT_CACHE_SLOT(btree, i);

			BT_NODE_CACHED(tmp) = 0;
			BT_FREE_NODE(&tmp);

			BT_CACHE_SLOT(btree, i) = NULL;
			btree->bt_num_cached_nodes--;
		}
	}

	INM_REL_MUTEX(&btree->bt_cache_mutex);
}

void
bt_destroy_cache(inm_bt_t *btree)
{
	inm_u32_t i = 0;
	void *tmp;

	DBG("Releasing Cache");

	if ( BT_CACHE(btree) ) {
		/* Free the cache pages */
		for( i = 0; i <= btree->bt_max_keys; i++ ) {
			if( BT_CACHE_SLOT(btree, i) ) {
				tmp = BT_CACHE_SLOT(btree, i);

				BT_NODE_CACHED(tmp) = 0;
				BT_FREE_NODE(&tmp);

				BT_CACHE_SLOT(btree, i) = NULL;
			}
		}

		INM_FREE(&BT_CACHE(btree),
			 (btree->bt_max_keys + 1) * sizeof(void *));
		BT_CACHE(btree) = NULL;
	}
}




inm_32_t
bt_init_cache(inm_bt_t *btree)
{
	inm_u32_t i = 0;
	inm_32_t retval = 0;

	btree->bt_num_cached_nodes = 0;

	DBG("Initialising Cache");
	BT_CACHE(btree) = INM_ALLOC((btree->bt_max_keys + 1) * sizeof(void *),
				    GFP_KERNEL);
	if ( !BT_CACHE(btree) ) {
		ERR("Cannot allocate memory for cache");
		return -ENOMEM;
	}

	for( i = 0; i <= btree->bt_max_keys; i++ )
		BT_CACHE_SLOT(btree, i) = NULL;

	return retval;
}
