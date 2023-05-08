#include "involflt-common.h"
#include "change-node.h"
#include "target-context.h"
#define IDEBUG_METADATA 1
#ifndef LINVOLFLT_DEBUG_H
#define LINVOLFLT_DEBUG_H
#define DEBUG_LEVEL 1

void print_target_context(target_context_t *);
void print_disk_change_head(disk_chg_head_t *);
void print_change_node(change_node_t *);
void print_disk_change(disk_chg_t *);
#if 0
void print_bio(struct bio *);
void print_dm_bio_info(dm_bio_info_t *);
#endif

#endif
