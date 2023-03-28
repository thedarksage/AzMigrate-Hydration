#ifndef _DISK_BTREE_CACHE_H
#define _DISK_BTREE_CACHE_H

#include "common.h"
#include "disk_btree_helpers.h"
#include "vsnap_common.h"
#include "vsnap.h"
#include "vsnap_kernel.h"

/* MACROS */
#define BT_CACHE(BT)		(BT)->bt_cache
#define BT_CACHE_SLOT(BT, IDX)	(BT_CACHE(BT))[IDX]
#define BT_ROOT(BT)		(BT)->bt_root_node

#define BT_NODE_CACHED(NODE)	\
			*((inm_32_t *)((char *)(NODE) - BT_NODE_PAD_SIZE))

/* Caching routines */
void bt_destroy_cache(inm_bt_t *);
inm_32_t bt_init_cache(inm_bt_t *);
void bt_update_cache(inm_bt_t *);
void * bt_get_from_cache(inm_bt_t *, void *, inm_32_t);
void bt_invalidate_cache(inm_bt_t *);

#ifdef _VSNAP_DEBUG_
inm_32_t inm_validate_btree_cache(inm_vdev_t *);
#endif

#endif /* _DISK_BTREE_CACHE_H */
