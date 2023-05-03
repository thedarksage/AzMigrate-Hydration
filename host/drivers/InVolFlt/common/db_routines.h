/*
 * Copyright (C) 2005 InMage
 * This file contains proprietary and confidential information and
 * remains the unpublished property of InMage. Use, disclosure,
 * or reproduction is prohibited except as permitted by express
 * written license aggreement with InMage.
 *
 * File       : db_routines.h
 *
 * Description: This file contains data mode implementation of the
 *              filter driver.
 *
 * Functions defined are
 *
 *
 */

inm_s32_t set_volume_out_of_sync(target_context_t *vcptr,
			   inm_u64_t out_of_sync_error_code,
			   inm_s32_t status_to_log);

void set_volume_out_of_sync_worker_routine(wqentry_t *wqe);
//void set_volume_out_of_sync_worker_routine(void *context);

inm_s32_t queue_worker_routine_for_set_volume_out_of_sync(target_context_t *vcptr,
						    int64_t out_of_sync_error_code, inm_s32_t status);

inm_s32_t stop_filtering_device(target_context_t *vcptr, inm_s32_t lock_acquired, volume_bitmap_t **vbmap_ptr);
void add_resync_required_flag(UDIRTY_BLOCK_V2 *udb, target_context_t *vcptr);
void reset_volume_out_of_sync(target_context_t *vcptr);
