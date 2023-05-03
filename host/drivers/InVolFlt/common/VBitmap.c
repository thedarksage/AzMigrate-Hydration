/*
 * Copyright (C) 2005 InMage
 * This file contains proprietary and confidential information and
 * remains the unpublished property of InMage. Use, disclosure,
 * or reproduction is prohibited except as permitted by express
 * written license aggreement with InMage.
 *
 * File       : VBitmap.c
 *
 * Description: This file contains data mode implementation of the
 *              filter driver.
 *
 * Functions defined are
 *
 *     *open_bitmap_file
 *     close_bitmap_file
 *     wake_service_thread
 *     request_service_thread_to_open_bitmap
 *     bitmap_write_worker_routine
 *     get_volume_bitmap_granularity
 *     *allocate_volume_bitmap
 *     dealloc_volume_bitmap
 *     get_volume_bitmap
 *     put_volume_bitmap
 *     *allocate_bitmap_work_item
 *     cleanup_bitmap_work_item
 *     get_bitmap_work_item
 *     put_bitmap_work_item
 *     wait_for_all_writes_to_complete
 *     queue_worker_routine_for_start_bitmap_read
 *     start_bitmap_read_worker_routine
 *     read_bitmap_completion_callback
 *     read_bitmap_completion_worker_routine
 *     read_bitmap_completion
 *     continue_bitmap_read
 *     continue_bitmap_read_worker_routine
 *     write_bitmap_completion_callback
 *     write_bitmap_completion
 *     queue_worker_routine_for_continue_bitmap_read
 */

#include "involflt.h"
#include "involflt-common.h"
#include "data-mode.h"
#include "utils.h"
#include "change-node.h"
#include "filestream.h"
#include "iobuffer.h"
#include "filestream_segment_mapper.h"
#include "segmented_bitmap.h"
#include "bitmap_api.h"
#include "VBitmap.h"
#include "work_queue.h"
#include "data-file-mode.h"
#include "target-context.h"
#include "driver-context.h"
#include "metadata-mode.h"
#include "db_routines.h"
#include "file-io.h"
#include "tunable_params.h"
#include "telemetry.h"

extern driver_context_t *driver_ctx;

#define update_target_context_stats(vcptr)						\
do{												                \
	vcptr->tc_bp->num_changes_queued_for_writing += vcptr->tc_pending_changes;\
	vcptr->tc_bp->num_of_times_bitmap_written++;                \
	vcptr->tc_bp->num_byte_changes_queued_for_writing += vcptr->tc_bytes_pending_changes;\
	vcptr->tc_pending_changes = 0;								\
	vcptr->tc_pending_md_changes = 0;							\
	vcptr->tc_bytes_pending_md_changes = 0;						\
	vcptr->tc_bytes_pending_changes = 0;				    	\
	vcptr->tc_bytes_pending_changes = 0;							\
												\
	vcptr->tc_pending_wostate_data_changes = 0;						\
	vcptr->tc_pending_wostate_md_changes = 0;						\
	vcptr->tc_pending_wostate_bm_changes = 0;						\
	vcptr->tc_pending_wostate_rbm_changes = 0;						\
}while(0)

/* OpenBitmapFile */
/**
 * FUNCTION NAME: open_bitmap_file
 * 
 * DESCRIPTION : This functions is a wrapper function, which allocates all the
 *     bitmap data structures, and opens the bitmap file
 * INPUT PARAMETERS : 
 *               vcptr - ptr to target_context
 *              status -
 *              
 * 
 * OUTPUT PARAMETERS : status - 
 * NOTES
 * 
 * return value :    volume_bitmap_t ptr - for success
 *             NULL - for invalid inputs
 <non-zero value> - for failure
 * 
 **/

volume_bitmap_t *open_bitmap_file(target_context_t *vcptr, inm_s32_t *status)
{
    volume_bitmap_t *vol_bitmap = NULL;
    bitmap_api_t     *bmap_api = NULL;
    inm_u64_t     bitmap_granularity = 0; /* need to initialize */
    inm_u64_t     bitmap_granularity_bmfile = 0; /* need to initialize */
    inm_s32_t         vol_in_sync = TRUE;
    inm_s32_t         inmage_open_status = 0;

if(IS_DBG_ENABLED(inm_verbosity, (INM_IDEBUG | INM_IDEBUG_BMAP))){
    info("entered");
}

    /* TODOs: combo check, filter device object */
    if (!vcptr)
        return NULL;

if(IS_DBG_ENABLED(inm_verbosity, (INM_IDEBUG | INM_IDEBUG_BMAP))){
    info("entered - target volume %p (%s)\n", vcptr, vcptr->tc_guid);
}
    if (is_rootfs_ro()) {
        err("Root filesystem is RO. Retry bitmap open later..");
        return NULL;
    }
    /* TODOs: check for VCF_GUID_OBTAINED */
    vol_bitmap = allocate_volume_bitmap();
    if (!vol_bitmap) {
        err("allocation of vol_bitmap failed \n");
        return NULL;
    }

    /* We are storing volume size in target context 
     * during creation of target context */ 
    bitmap_granularity_bmfile = get_bmfile_granularity(vcptr);
    
    *status = get_volume_bitmap_granularity(vcptr, &bitmap_granularity);
    if (bitmap_granularity_bmfile &&
        bitmap_granularity_bmfile != bitmap_granularity &&
        bitmap_granularity_bmfile % INM_SECTOR_SIZE == 0) {
        bitmap_granularity = bitmap_granularity_bmfile;
        info("Upgrade for %s: retaining older bitmap granularity %llu",
             vcptr->tc_bp->bitmap_file_name, bitmap_granularity_bmfile);
    }

    if (*status != 0 || !bitmap_granularity)
        goto cleanup_and_return_error;

    vol_bitmap->segment_cache_limit = MAX_BITMAP_SEGMENT_BUFFERS;

    bmap_api = bitmap_api_ctr();
    if (bmap_api) {
        inmage_open_status = 0;
        vol_bitmap->bitmap_api = bmap_api;

        INM_DOWN(&vcptr->tc_sem);
        *status = bitmap_api_open(bmap_api,
				  vcptr,
				  bitmap_granularity,
				  vcptr->tc_bp->bitmap_offset,
				  inm_dev_size_get(vcptr),
				  vcptr->tc_guid,
				  vol_bitmap->segment_cache_limit,
				  &inmage_open_status);
        INM_UP(&vcptr->tc_sem);

        if (*status == 0) { /* if success */
            if (vcptr->tc_bp->num_bitmap_open_errors)
                log_bitmap_open_success_event(vcptr);
            if (inmage_open_status != 0) {
                // DS allocated, hdr may/not contain data
                dbg("bitmap data structures allocated err = %x\n", inmage_open_status);
            }
        } else {
            if (inmage_open_status != 0) {
                /* TODOs: Log error message */
                info("Error %x in opening bitmap file for volume %s\n", inmage_open_status, vcptr->tc_guid);
                /* Logging error into InMageFltLogError() */
            }
            goto cleanup_and_return_error;
        }
        if (bmap_api->io_bitmap_header) {
            *status = is_volume_in_sync(bmap_api, &vol_in_sync,
					&inmage_open_status); 
            if (*status != 0) { /* 0 means success */
                err("bitmap file open failed");
                goto cleanup_and_return_error;
            }

            if (vol_in_sync == FALSE) {
                /* volume is not in sync */
		info("resync triggered");
                set_volume_out_of_sync(vcptr, inmage_open_status,
				       inmage_open_status);

                /* TODOs: cleanup data files from previous boot*/
                /* CDataFile::DeleteDataFilesInDirectory() */
                bmap_api->volume_insync = TRUE;
            }
        }

        /* TODOs : VCF_VOLUME_ON_CLUS_DISK - HUGE CODE*/

    } else {
        err("failed to allocate bitmap_api");
        goto cleanup_and_return_error;
    }
    if (bmap_api->fs) {
        /* unset ignore bitmap creation flag. */
        volume_lock(vcptr);
        vcptr->tc_flags &= ~VCF_IGNORE_BITMAP_CREATION;
        volume_unlock(vcptr);
        vol_bitmap->eVBitmapState = ecVBitmapStateOpened;
        set_tgt_ctxt_wostate(vcptr, ecWriteOrderStateBitmap, FALSE,
                                     ecWOSChangeReasonUnInitialized);
    } else {
        vol_bitmap->eVBitmapState = ecVBitmapStateClosed;
        set_tgt_ctxt_wostate(vcptr, ecWriteOrderStateRawBitmap, FALSE,
                                     ecWOSChangeReasonUnInitialized);
    }
    vcptr->tc_stats.st_wostate_switch_time = INM_GET_CURR_TIME_IN_SEC;

    /* TODOs: statistics information
       vcptr->tick_count_at_last_flt_mode_change = 
       KeQueryPerformanceCounter();
    */

    /* reference the volume context */
    get_tgt_ctxt(vcptr);
    vol_bitmap->volume_context = vcptr;

    /* vol_bitmap->volume_GUID is only used as string in logging messages */
    memcpy_s(vol_bitmap->volume_GUID, sizeof(vol_bitmap->volume_GUID),
			 vcptr->tc_guid, sizeof(vcptr->tc_guid));

if(IS_DBG_ENABLED(inm_verbosity, (INM_IDEBUG | INM_IDEBUG_BMAP))){
    info("leaving");
}
    return vol_bitmap;

cleanup_and_return_error:

    if (vol_bitmap->bitmap_api) {
	bitmap_api_dtr(vol_bitmap->bitmap_api);
	vol_bitmap->bitmap_api = NULL;
	bmap_api = NULL;
    }
    if (vol_bitmap) {
        put_volume_bitmap(vol_bitmap);
        vol_bitmap = NULL;
    }

if(IS_DBG_ENABLED(inm_verbosity, (INM_IDEBUG | INM_IDEBUG_BMAP))){
    info("leaving");
}
    return NULL;
}

/* CloseBitmapFile */

/**
 * FUNCTION NAME: close_bitmap_file
 * 
 * DESCRIPTION : This function is a wrapper function that closes the bitmapfile
 *         and deallocates some of the bitmap data structures
 *        On success, it frees bitmap_api, volume bitmap ptrs
 * INPUT PARAMETERS : vbmap -> ptr to volume bitmap 
 *               clear_bitmap -> flag to indicate whether to clear bits
 *                      in bitmap or not
 *              
 * 
 * OUTPUT PARAMETERS :
 * NOTES
 * 
 * return value :    closes the bitmap file      - for success
 *             returns without successful operation- for invalid inputs
 * 
 **/
 
void close_bitmap_file(volume_bitmap_t *vbmap, inm_s32_t clear_bitmap) {
    inm_s32_t wait_for_notification = FALSE;
    inm_s32_t set_bits_work_item_list_empty = FALSE;
    unsigned long lock_flag = 0;
    
if(IS_DBG_ENABLED(inm_verbosity, (INM_IDEBUG | INM_IDEBUG_BMAP))){
    info("entered");
}
    
    if (!vbmap)
        return; //-EINVAL;

    /*
     * If clear_bitmap is true, set the skip_writes flag so
     * that any bitmap writes queued turn to no-ops as we
     * will eventually clear all the bits. Since this is an
     * advisory flag and ONLY SET HERE, no need for locking.
     */
    INM_DOWN(&vbmap->sem);
    if (clear_bitmap)
        vbmap->bitmap_skip_writes = 1;
    INM_UP(&vbmap->sem);

    do {
        if (wait_for_notification) {
            INM_WAIT_FOR_COMPLETION(&vbmap->set_bits_work_item_list_empty_notification);
            dbg("waiting on set_bits_work_item_list_empty_notification");
            dbg("for volume (%s)", vbmap->volume_GUID);
            wait_for_notification = FALSE;
        }

        INM_DOWN(&vbmap->sem);

        INM_SPIN_LOCK_IRQSAVE(&vbmap->lock, lock_flag);
        set_bits_work_item_list_empty = inm_list_empty(&vbmap->set_bits_work_item_list);
        INM_SPIN_UNLOCK_IRQRESTORE(&vbmap->lock, lock_flag);

        if (set_bits_work_item_list_empty) {
            /* set the state of the bitmap to close */
            vbmap->eVBitmapState = ecVBitmapStateClosed;

            if (vbmap->bitmap_api) {
                inm_s32_t _rc = 0;

                if (clear_bitmap) {
                    vbmap->bitmap_skip_writes = 0;
                    INM_UP(&vbmap->sem);
                    bitmap_api_clear_all_bits(vbmap->bitmap_api);
                    INM_DOWN(&vbmap->sem);
                }

                INM_UP(&vbmap->sem);
                //update_bmaphdr_file(vbmap->bitmap_api);
                if (bitmap_api_close(vbmap->bitmap_api, &_rc)) {
		    target_context_t *vcp = vbmap->volume_context;

		    if (vcp && vcp->tc_pending_changes) {
		        set_volume_out_of_sync(vcp, ERROR_TO_REG_BITMAP_OPEN_FAIL_CHANGES_LOST, 0);
		    }
		}
                INM_DOWN(&vbmap->sem);
                bitmap_api_dtr(vbmap->bitmap_api);
                vbmap->bitmap_api = NULL;
            }
            if (vbmap->volume_context) {
                put_tgt_ctxt(vbmap->volume_context);
                vbmap->volume_context = NULL;
            }
        } else {
            vbmap->flags |= VOLUME_BITMAP_FLAGS_WAITING_FOR_SETBITS_WORKITEM_LIST_EMPTY_NOTIFICATION;
            wait_for_notification = TRUE;
        }

        INM_UP(&vbmap->sem);
    } while (wait_for_notification);
    put_volume_bitmap(vbmap);

if(IS_DBG_ENABLED(inm_verbosity, (INM_IDEBUG | INM_IDEBUG_BMAP))){
    info("leaving");
}
    return;
}

/**
 * FUNCTION NAME: wake_service_thread
 * 
 * DESCRIPTION : This function wakesup service thread to open
 *         bitmap file, esp when the driver mode switches from 
 *        metadata mode to bitmap mode.
 *
 * INPUT PARAMETERS : dc_lock_acquired -flag - true/false indicating 
 *                        driver_ctx lock
 *
 *
 *
 * OUTPUT PARAMETERS :
 * NOTES
 *
 * return value :    0      - for success
 *             EINVAL - for invalid inputs
 <non-zero value> - for failure
 * 
 **/

void wake_service_thread(inm_s32_t dc_lock_acquired) {

if(IS_DBG_ENABLED(inm_verbosity, ((INM_IDEBUG | INM_IDEBUG_META) | INM_IDEBUG_BMAP))){
    info("entered");
}
//    if(!dc_lock_acquired)
//        INM_SPIN_LOCK_IRQSAVE(&driver_ctx->lock, lock_flag);

    //set service_thread_wake_event
    INM_ATOMIC_INC(&driver_ctx->service_thread.wakeup_event_raised);
    INM_WAKEUP_INTERRUPTIBLE(&driver_ctx->service_thread.wakeup_event);
    INM_COMPLETE(&driver_ctx->service_thread._new_event_completion);


//    if(!dc_lock_acquired)
//        INM_SPIN_UNLOCK_IRQRESTORE(&driver_ctx->lock, lock_flag);
    dbg("waking up service thread\n");
if(IS_DBG_ENABLED(inm_verbosity, ((INM_IDEBUG | INM_IDEBUG_META) | INM_IDEBUG_BMAP))){
    info("leaving");
}
    return;
}
void request_service_thread_to_open_bitmap(target_context_t *vcptr) {
    inm_s32_t wakeup_service_thread = FALSE;

if(IS_DBG_ENABLED(inm_verbosity, ((INM_IDEBUG | INM_IDEBUG_META) | INM_IDEBUG_BMAP))){
    info("entered");
}

    if (!vcptr)
        return;

    /* acquire vcptr->lock : if it is not acquired in involflt_completion*/
    if(!(vcptr->tc_flags & VCF_OPEN_BITMAP_REQUESTED)) {
        vcptr->tc_flags |= VCF_OPEN_BITMAP_REQUESTED;
        wakeup_service_thread = TRUE;
    }
    /* release vcptr->lock */
    if (wakeup_service_thread)
        wake_service_thread(FALSE);
if(IS_DBG_ENABLED(inm_verbosity, ((INM_IDEBUG | INM_IDEBUG_META) | INM_IDEBUG_BMAP))){
    info("leaving");
}
    return;
}


/* TODOs: Functions related to bitmap write */

/**
 * FUNCTION NAME: bitmap_write_worker_routine
 * 
 * DESCRIPTION : This function is a wrapper for set bits operation, 
 *         It is called by worker routine.
 * 
 * INPUT PARAMETERS : wqe - ptr to work queue entry
 *               
 *              
 *              
 * 
 * OUTPUT PARAMETERS :
 * NOTES
 * 
 * return value :    0      - for success
 *             EINVAL - for invalid inputs
 <non-zero value> - for failure
 *
 **/

void bitmap_write_worker_routine(wqentry_t *wqe) {
    bitmap_work_item_t *bmap_witem = NULL;
    volume_bitmap_t *vbmap = NULL;
    unsigned long lock_flag = 0;

if(IS_DBG_ENABLED(inm_verbosity, (INM_IDEBUG | INM_IDEBUG_BMAP))){
    info("entered");
}

    bmap_witem = (bitmap_work_item_t *) wqe->context;
    put_work_queue_entry(wqe);

    if (!bmap_witem) {
        info("bmap witem is null");
        return;
    }

    vbmap = bmap_witem->volume_bitmap;
    if (!vbmap) {
        info("volume bitmap is null");
        return;
    }

    get_volume_bitmap(vbmap);

    INM_DOWN(&vbmap->sem);

    if (!vbmap->bitmap_api || vbmap->eVBitmapState == ecVBitmapStateClosed) {
        info("failed state %s, bapi = %p, for volume %s",
	     get_volume_bitmap_state_string(vbmap->eVBitmapState),
	     vbmap->bitmap_api,
	     vbmap->volume_GUID);
        // further set bit operations are ignored
        if (bmap_witem->eBitmapWorkItem == ecBitmapWorkItemSetBits) {
            INM_SPIN_LOCK_IRQSAVE(&vbmap->lock, lock_flag);
            inm_list_del(&bmap_witem->list_entry);

            if ((vbmap->flags & VOLUME_BITMAP_FLAGS_WAITING_FOR_SETBITS_WORKITEM_LIST_EMPTY_NOTIFICATION) &&
                inm_list_empty(&vbmap->set_bits_work_item_list)) {
                vbmap->flags &= ~VOLUME_BITMAP_FLAGS_WAITING_FOR_SETBITS_WORKITEM_LIST_EMPTY_NOTIFICATION;
                INM_COMPLETE(&vbmap->set_bits_work_item_list_empty_notification);
            }

            INM_SPIN_UNLOCK_IRQRESTORE(&vbmap->lock, lock_flag);
        } else {
            /* required to optimize inm_list_del is calling from 
             * if and else places... and else part doesn't have
             * protection
             */
            inm_list_del(&bmap_witem->list_entry);
        }
        volume_lock(vbmap->volume_context);
        vbmap->volume_context->tc_flags &= ~VCF_VOLUME_IN_BMAP_WRITE;
        volume_unlock(vbmap->volume_context);

        put_bitmap_work_item(bmap_witem);
    } else {
        bmap_witem->bit_runs.context1 = bmap_witem;
        bmap_witem->bit_runs.completion_callback =
	    write_bitmap_completion_callback;
        INM_UP(&vbmap->sem);
        bitmap_api_setbits(vbmap->bitmap_api, &bmap_witem->bit_runs, 
                           vbmap);
        INM_DOWN(&vbmap->sem);
    }

    INM_UP(&vbmap->sem);
    put_volume_bitmap(vbmap);

if(IS_DBG_ENABLED(inm_verbosity, (INM_IDEBUG | INM_IDEBUG_BMAP))){
    info("leaving");
}
    return;
}

/* TODOs: Functions Related to Bitmap Read */

/**
 * FUNCTION NAME: get_volume_bitmap_granularity
 * 
 * DESCRIPTION : This function returns the bitmap granularity if it set, 
 *    otherwise, it returns the default granularity, which is based on
 *     volume size.
 * 
 * INPUT PARAMETERS : vcptr - ptr to target context
 *               bitmap_granularity - ptr to unsigned long
 *              
 *              
 * 
 * OUTPUT PARAMETERS : bitmap_granularity - ptr to unsigned long
 * NOTES
 * 
 * return value :    0      - for success
 *             -EINVAL - for invalid inputs
 * 
 **/
inm_s32_t get_volume_bitmap_granularity(target_context_t *vcptr,
				  inm_u64_t *bitmap_granularity) {
    inm_s32_t status = 0;

if(IS_DBG_ENABLED(inm_verbosity, (INM_IDEBUG | INM_IDEBUG_BMAP))){
    info("entered");
}
    if (!vcptr || !bitmap_granularity || !inm_dev_size_get(vcptr))
        return -EINVAL;

    *bitmap_granularity = default_granularity_from_volume_size(inm_dev_size_get(vcptr));

    //TODOs: registry reading
if(IS_DBG_ENABLED(inm_verbosity, (INM_IDEBUG | INM_IDEBUG_BMAP))){
    info("leaving with status = %d", status);
}

    return status;
}

/* WaitForAllWritesToComplete */


/***********************************************
 *  Function definitions for volume bitmap      *
 ************************************************/

/**
 * FUNCTION NAME: allocate_volume_bitmap
 * 
 * DESCRIPTION : This function constructs volume bitmap structure
 * 
 * INPUT PARAMETERS : None
 * 
 * OUTPUT PARAMETERS : None
 * NOTES
 * 
 * return value :    volume bitmap ptr      - for success
 *             NULL - for failure
 * 
 **/
volume_bitmap_t * allocate_volume_bitmap(void) {
    volume_bitmap_t *vol_bitmap = NULL;

    vol_bitmap = (volume_bitmap_t *)INM_KMALLOC(sizeof(volume_bitmap_t), INM_KM_SLEEP, INM_PINNED_HEAP);
    if (!vol_bitmap) 
        return NULL;

    INM_MEM_ZERO(vol_bitmap, sizeof(volume_bitmap_t));

    INM_ATOMIC_SET(&vol_bitmap->refcnt, 1);
    //vol_bitmap->eVBitmap_state = BITMAP_STATE_UNINITIALIZED;
    vol_bitmap->eVBitmapState = ecVBitmapStateUnInitialized;

    INM_INIT_LIST_HEAD(&vol_bitmap->list_entry);
    INM_INIT_LIST_HEAD(&vol_bitmap->work_item_list);
    INM_INIT_LIST_HEAD(&vol_bitmap->set_bits_work_item_list);
    
    INM_INIT_COMPLETION(&vol_bitmap->set_bits_work_item_list_empty_notification);
    INM_INIT_SEM(&vol_bitmap->sem);
    INM_INIT_SPIN_LOCK(&vol_bitmap->lock);

    /* TODOs : dc - lock Do we require lock here -- new lock  */
    inm_list_add_tail(&vol_bitmap->list_entry,
                  &driver_ctx->dc_bmap_info.head_for_volume_bitmaps);
    driver_ctx->dc_bmap_info.num_volume_bitmaps++;
    /* release dc lock */
    return vol_bitmap;
}

/**
 * FUNCTION NAME: dealloc_volume_bitmap
 *
 * DESCRIPTION : This function destructs volume bitmap structure
 *
 * INPUT PARAMETERS : vol_bitmap - ptr to volume bitmap
 *
 * OUTPUT PARAMETERS : None
 * NOTES
 *
 * return value :       volume bitmap ptr      - for success
 *                      NULL - for failure
 * 
 **/

void dealloc_volume_bitmap(volume_bitmap_t *vol_bitmap) {
    /* TODOs: assert statements */
    /* TODOs: bitmap_api */
    
    if(!vol_bitmap) {
        info("vol bitmap is null");
        return;
    }
    
    /* acquire dc lock */
    /* TODOs: require lock variable only for bitmaps ?? */
    inm_list_del(&vol_bitmap->list_entry);
    driver_ctx->dc_bmap_info.num_volume_bitmaps--;
    /* release dc lock */


    INM_DESTROY_COMPLETION(&vol_bitmap->set_bits_work_item_list_empty_notification);
    INM_DESTROY_SPIN_LOCK(&vol_bitmap->lock);
    INM_DESTROY_SEM(&vol_bitmap->sem);
    INM_KFREE(vol_bitmap, sizeof(*vol_bitmap), INM_PINNED_HEAP);
    vol_bitmap = NULL;
}

/* increments volume bitmap reference count */
void get_volume_bitmap(volume_bitmap_t *vol_bitmap) {
    INM_ATOMIC_INC(&vol_bitmap->refcnt);
    return;
}

/* decrements volume bitmap reference count */
void put_volume_bitmap(volume_bitmap_t *vol_bitmap) {
    if (INM_ATOMIC_DEC_AND_TEST(&vol_bitmap->refcnt)) 
        dealloc_volume_bitmap(vol_bitmap);
    return;
/*        cleanup_volume_bitmap(vol_bitmap); */
}

/* Function definition for Bitmap work items */
/**
 * FUNCTION NAME: allocate_bitmap_work_item
 * 
 * DESCRIPTION : This function constructs bitmap work item
 * 
 * INPUT PARAMETERS : None
 * 
 * OUTPUT PARAMETERS : None
 * NOTES
 * 
 * return value :    ptr to bitmap work item      - for success
 *             NULL - for invalid inputs
 * 
 **/
bitmap_work_item_t *allocate_bitmap_work_item(inm_u32_t witem_type) {
    bitmap_work_item_t *bm_witem = NULL;

    bm_witem = (bitmap_work_item_t *)
        INM_KMEM_CACHE_ALLOC(driver_ctx->dc_bmap_info.bitmap_work_item_pool,
		INM_KM_SLEEP | INM_KM_NOWARN);
    
    if (!bm_witem)
        return NULL;

    INM_MEM_ZERO(bm_witem, sizeof(bitmap_work_item_t));
    INM_ATOMIC_SET(&bm_witem->refcnt, 1);

    INM_INIT_LIST_HEAD(&bm_witem->list_entry);
    INM_INIT_LIST_HEAD(&bm_witem->bit_runs.meta_page_list);

    bm_witem->volume_bitmap = NULL;

    //bm_witem->bitmap_work_item = BITMAP_WORK_ITEM_UNINITIALIZED;
    bm_witem->eBitmapWorkItem = ecBitmapWorkItemNotInitialized;

    if(WITEM_TYPE_BITMAP_WRITE != witem_type){
	inm_page_t *pgp = NULL;

	pgp = get_page_from_page_pool(0, INM_KM_SLEEP, NULL);
	if(!pgp){
	    put_bitmap_work_item(bm_witem);
	    return NULL;
	}

	inm_list_add_tail(&pgp->entry, &bm_witem->bit_runs.meta_page_list);
	bm_witem->bit_runs.runs = (disk_chg_t *)pgp->cur_pg;
    }

    return bm_witem;
}

/* frees the bitmap work item */
void cleanup_bitmap_work_item(bitmap_work_item_t *bm_witem) {
    inm_page_t *pgp;

    while(!inm_list_empty(&bm_witem->bit_runs.meta_page_list)){
	pgp = inm_list_entry(bm_witem->bit_runs.meta_page_list.next, inm_page_t, entry);
	inm_list_del(&pgp->entry);
	inm_free_metapage(pgp);
    }

    INM_KMEM_CACHE_FREE(driver_ctx->dc_bmap_info.bitmap_work_item_pool, bm_witem);
    return;
}

// reference bitmap_work_item , increment refcnt 
void get_bitmap_work_item(bitmap_work_item_t *bitmap_work_item) {
    INM_ATOMIC_INC(&bitmap_work_item->refcnt);
    return;
}

// dereference bitmap_work_item, decrement refcnt
void put_bitmap_work_item(bitmap_work_item_t *bitmap_work_item) {
    if (INM_ATOMIC_DEC_AND_TEST(&bitmap_work_item->refcnt))
        cleanup_bitmap_work_item(bitmap_work_item);
    return;
}
/**
 * FUNCTION NAME: wait_for_all_writes_to_complete
 * 
 * DESCRIPTION : This function waits till all bitmap work items drained
 *               in set_bitmap_work_item_list
 * 
 * INPUT PARAMETERS : vbmap - ptr to volume bitmap
 *               
 *              
 *              
 * 
 * OUTPUT PARAMETERS :
 * NOTES
 * 
 * return value :      
 *             returns  - for invalid inputs
 * 
 **/
void wait_for_all_writes_to_complete(volume_bitmap_t *vbmap) 
{
    inm_s32_t set_bits_work_item_list_is_empty;
    inm_s32_t wait_for_notification = FALSE;
    unsigned long lock_flag = 0;

if(IS_DBG_ENABLED(inm_verbosity, (INM_IDEBUG | INM_IDEBUG_BMAP))){
    info("entered");
}

    if (!vbmap) {
        info("null vbmap");
        return;
    }

    INM_DOWN(&vbmap->sem);

    INM_SPIN_LOCK_IRQSAVE(&vbmap->lock, lock_flag);

    set_bits_work_item_list_is_empty = inm_list_empty(&vbmap->set_bits_work_item_list);

    INM_SPIN_UNLOCK_IRQRESTORE(&vbmap->lock, lock_flag);

    if (!set_bits_work_item_list_is_empty) {
        dbg("set_bits_work_item_list is not empty");
        vbmap->flags |= VOLUME_BITMAP_FLAGS_WAITING_FOR_SETBITS_WORKITEM_LIST_EMPTY_NOTIFICATION;
        wait_for_notification = TRUE;
    }

    INM_UP(&vbmap->sem);

    if (wait_for_notification) {
        dbg("waiting on set_bits_work_item_list_empty_notification");
        dbg("for volume %s", vbmap->volume_GUID);
        INM_WAIT_FOR_COMPLETION_INTERRUPTIBLE(&vbmap->set_bits_work_item_list_empty_notification);
    }

if(IS_DBG_ENABLED(inm_verbosity, (INM_IDEBUG | INM_IDEBUG_BMAP))){
    info("leaving");
}

    return;
}
/**
 * FUNCTION NAME: queue_worker_routine_for_start_bitmap_read
 * 
 * DESCRIPTION : This function queues work queue entry for bitmap read (start),
 *               It is called by service thread.
 * 
 * INPUT PARAMETERS : vbmap - ptr to volume bitmap
 *               
 * 
 * OUTPUT PARAMETERS :
 * NOTES
 * 
 * return value :    
 *             
 * 
 **/
void queue_worker_routine_for_start_bitmap_read(volume_bitmap_t *vbmap) {
    wqentry_t *wqe = NULL;
    bitmap_work_item_t *bmap_witem = NULL;

if(IS_DBG_ENABLED(inm_verbosity, (INM_IDEBUG | INM_IDEBUG_BMAP))){
    info("entered");
}

    if (!vbmap) {
        info("null vbmap");
        return;
    }

    wqe = alloc_work_queue_entry(INM_KM_SLEEP);
    if (!wqe) {
        info("malloc failed : wqe");
        return;
    }

    bmap_witem = allocate_bitmap_work_item(WITEM_TYPE_START_BITMAP_READ);
    if (!bmap_witem) {
        info("malloc failed : bmap_witem");
        put_work_queue_entry(wqe);
        return;
    }

    // fill bitmap work item 
    get_volume_bitmap(vbmap);
    bmap_witem->volume_bitmap = vbmap;
    bmap_witem->eBitmapWorkItem = ecBitmapWorkItemStartRead;

    // fill work queue entry 
    wqe->witem_type = WITEM_TYPE_START_BITMAP_READ;
    wqe->context = bmap_witem;
    wqe->work_func = start_bitmap_read_worker_routine;
    
    // add work queue entry in work queue
    add_item_to_work_queue(&driver_ctx->wqueue, wqe);

if(IS_DBG_ENABLED(inm_verbosity, (INM_IDEBUG | INM_IDEBUG_BMAP))){
    info("leaving");
}
    return;
}
/**
 * FUNCTION NAME: start_bitmap_read_worker_routine
 * 
 * DESCRIPTION : This function is a wrapper, which extracts bitmap work item
 *         from work queue entry and calls low level bitmap read routines
 *         It is called by worker thread.
 * INPUT PARAMETERS : wqe - ptr to work queue entry
 *               
 *              
 *              
 * 
 * OUTPUT PARAMETERS :
 * NOTES
 * This function queues the read changes to device
 * specific dirty block context. After queuing changes it checks if changes
 * cross low water mark the read is paused. If the changes read are the last
 * set of changes the read state is set to completed. In this function we
 * have to unset the bits successfully read and send a next read.
 * 
 * return value :    
 * 
 **/
void start_bitmap_read_worker_routine(wqentry_t *wqe) {

    bitmap_work_item_t *bmap_witem = NULL;
    volume_bitmap_t *vbmap = NULL;

if(IS_DBG_ENABLED(inm_verbosity, (INM_IDEBUG | INM_IDEBUG_BMAP))){
    info("entered");
}

    if (!wqe) {
        info("null wqe");
        return;
    }

    bmap_witem = (bitmap_work_item_t *) wqe->context;
    put_work_queue_entry(wqe);

    //assert statements
    if (!bmap_witem || !bmap_witem->volume_bitmap) {
        info("null bmap_witem,  or null volume bitmap");
        return;
    }

    get_volume_bitmap(bmap_witem->volume_bitmap);
    vbmap = bmap_witem->volume_bitmap;

    INM_DOWN(&vbmap->sem);
    if ((vbmap->eVBitmapState == ecVBitmapStateReadStarted) &&
        vbmap->bitmap_api) {

        // reading bitmap
        bmap_witem->bit_runs.context1 = bmap_witem; //check again
        bmap_witem->bit_runs.completion_callback = 
	    read_bitmap_completion_callback;
        //inm_list_add_tail(&bmap_witem->list_entry, &vbmap->work_item_list);
        inm_list_add(&bmap_witem->list_entry, &vbmap->work_item_list);
        INM_UP(&vbmap->sem);
        bitmap_api_get_first_runs(vbmap->bitmap_api, &bmap_witem->bit_runs);
        INM_DOWN(&vbmap->sem);
        bmap_witem = NULL;
    }
    INM_UP(&vbmap->sem);

    put_volume_bitmap(vbmap);

    if (bmap_witem)
        put_bitmap_work_item(bmap_witem);

if(IS_DBG_ENABLED(inm_verbosity, (INM_IDEBUG | INM_IDEBUG_BMAP))){
    info("leaving");
}

    return;
}


/** -----------------------------------------------------------------------------
 * Function : ReadBitmapCompletionCallback
 * Parameters : BitRuns - This indicates the changes that have been read
 *              to bit map
 * NOTES :
 * This function is called after read completion. This function can be 
 * called at DISPACH_LEVEL. If the function is called at DISPACH_LEVEL a
 * worker routine is queued to process the completion at PASSIVE_LEVEL
 * If this function is called at PASSIVE_LEVEL, the process function is 
 * called directly with out queuing worker routine.
 *
 *-----------------------------------------------------------------------*/
/**
 * FUNCTION NAME: read_bitmap_completion_callback
 * 
 * DESCRIPTION : This function is called by worker thread 
 *        
 * 
 * INPUT PARAMETERS : bit_runs - ptr to bitruns (holds changes read from bitmap)
 *               
 * OUTPUT PARAMETERS :
 * NOTES
 * 
 * return value :    
 *             
 * 
 **/
void read_bitmap_completion_callback(bitruns_t *bit_runs) {
    wqentry_t *wqe = NULL;

if(IS_DBG_ENABLED(inm_verbosity, (INM_IDEBUG | INM_IDEBUG_BMAP))){
    info("entered");
}
    if (!bit_runs) {
        info("null bit_runs");
        return;
    }
    // Do not call completion directly here, This could lead to recursive call and stack overflow.
    //Currently completion is called synchronously.

    wqe = alloc_work_queue_entry(INM_KM_SLEEP);
    if (!wqe) {
        info("malloc failed : wqe ");
        return;
    }
    
    wqe->context = bit_runs->context1;
    wqe->work_func = read_bitmap_completion_worker_routine;

    add_item_to_work_queue(&driver_ctx->wqueue, wqe);

if(IS_DBG_ENABLED(inm_verbosity, (INM_IDEBUG | INM_IDEBUG_BMAP))){
    info("leaving");
}

    return;
}

/* -----------------------------------------------------------------------------
 * Function : ReadBitMapCompletionWorkerRoutine
 * Parameters : BitRuns - This indicates the changes that have been read
 *              to bit map
 * NOTES :
 * This function is worker routine and queued if completion routine is called
 * at DISPATCH_LEVEL. This function calls the ReadBitmapCompletion funciton
 *
 * ------------------------------------------------------------------------------ */
/**
 * FUNCTION NAME: read_bitmap_completion_worker_routine
 * 
 * DESCRIPTION :
 * 
 * INPUT PARAMETERS : 
 *               
 *              
 *              
 * 
 * OUTPUT PARAMETERS :
 * NOTES
 * 
 * return value :    0      - for success
 *             EINVAL - for invalid inputs
 <non-zero value> - for failure
 * 
 **/
void read_bitmap_completion_worker_routine(wqentry_t *wqe) {
    bitmap_work_item_t *bmap_witem = NULL;

if(IS_DBG_ENABLED(inm_verbosity, (INM_IDEBUG | INM_IDEBUG_BMAP))){
    info("entered");
}
    //assert for IRQ level

    // Holding reference to VolumeContext so that BitMap File is not closed before the writes are completed.
    bmap_witem = (bitmap_work_item_t *) wqe->context;
    put_work_queue_entry(wqe);

    read_bitmap_completion(bmap_witem);

if(IS_DBG_ENABLED(inm_verbosity, (INM_IDEBUG | INM_IDEBUG_BMAP))){
    info("leaving");
}

    return;
}


static inm_s32_t
split_change_into_chg_node_in_bitmap_wostate(target_context_t *ctx,
    struct inm_list_head *node_hd, write_metadata_t *wmd, etWriteOrderState wostate)
{

    inm_u64_t max_data_sz_per_chg_node =
        driver_ctx->tunable_params.max_data_size_per_non_data_mode_drty_blk;

    inm_u64_t remaining_length = wmd->length;
    struct inm_list_head split_chg_list_hd;
    inm_u64_t  byte_offset = wmd->offset;
    inm_u64_t nr_splits = 0;
    change_node_t *chg_node = NULL;
    inm_u64_t chg_len = 0;
    disk_chg_t *chg = NULL;

if(IS_DBG_ENABLED(inm_verbosity, (INM_IDEBUG | INM_IDEBUG_BMAP))){
    info("entered");
}

    INM_INIT_LIST_HEAD(&split_chg_list_hd);

    while(remaining_length) {
	chg_node = inm_alloc_change_node(NULL, INM_KM_SLEEP);
	if (!chg_node) {
	    info("change node is null");
	    return -ENOMEM;
	}
	init_change_node(chg_node, 0, INM_KM_SLEEP, NULL);
        ref_chg_node(chg_node);
	chg_node->type = NODE_SRC_METADATA;
	chg_node->wostate = wostate;
	inm_list_add_tail(&chg_node->next, &split_chg_list_hd);
	nr_splits++;

	chg_len = min(max_data_sz_per_chg_node, remaining_length);

	chg = (disk_chg_t *)((char *)chg_node->changes.cur_md_pgp +
			     (sizeof(disk_chg_t) * chg_node->changes.change_idx));

	chg->offset = byte_offset;
	chg->length = chg_len;
	chg->time_delta = 0;
	chg->seqno_delta = 0;

	chg_node->changes.change_idx++;
	chg_node->changes.bytes_changes += chg_len;
	chg_node->seq_id_for_split_io = nr_splits;
	chg_node->flags |= KDIRTY_BLOCK_FLAG_PART_OF_SPLIT_CHANGE;


	byte_offset += chg_len;
	remaining_length -= chg_len;
    }
    if (!nr_splits)
	return 0;

    chg_node = inm_list_entry(split_chg_list_hd.next, change_node_t, next);
    chg_node->flags |= KDIRTY_BLOCK_FLAG_START_OF_SPLIT_CHANGE;
    chg_node->flags &= ~KDIRTY_BLOCK_FLAG_PART_OF_SPLIT_CHANGE;

    chg_node = inm_list_entry(split_chg_list_hd.prev, change_node_t, next);
    chg_node->flags |= KDIRTY_BLOCK_FLAG_END_OF_SPLIT_CHANGE;
    chg_node->flags &= ~KDIRTY_BLOCK_FLAG_PART_OF_SPLIT_CHANGE;

    inm_list_splice_at_tail(&split_chg_list_hd, node_hd);

if(IS_DBG_ENABLED(inm_verbosity, (INM_IDEBUG | INM_IDEBUG_BMAP))){
    info("leaving nr splits = %llu\n", nr_splits);
}
    return nr_splits;
}

static inm_s32_t
add_metadata_bitmap_wostate(target_context_t *ctx, struct inm_list_head *node_hd,
				            change_node_t **change_node, write_metadata_t *wmd)
{

    inm_u32_t chg_sz = wmd->length;
    inm_u32_t max_data_sz_per_chg_node =
        driver_ctx->tunable_params.max_data_size_per_non_data_mode_drty_blk;
    inm_u32_t nr_splits = 0;
    disk_chg_t *chg = NULL;
    inm_u64_t avail_space = 0;
    change_node_t *tchg_node = *change_node;


if(IS_DBG_ENABLED(inm_verbosity, (INM_IDEBUG | INM_IDEBUG_BMAP))){
    info("entered");
}

    if (max_data_sz_per_chg_node < chg_sz) {
        *change_node = NULL;
        return split_change_into_chg_node_in_bitmap_wostate(ctx, node_hd, wmd,
                                                         ecWriteOrderStateBitmap);
    }

    if (tchg_node && tchg_node->changes.change_idx <
	(MAX_CHANGE_INFOS_PER_PAGE)) {
	avail_space = 
	    max_data_sz_per_chg_node - tchg_node->changes.bytes_changes;
	if ((avail_space < wmd->length) ||
	    (tchg_node->changes.change_idx >= MAX_CHANGE_INFOS_PER_PAGE)) 
	    tchg_node = NULL;
    }

    if (!tchg_node) {
        tchg_node = inm_alloc_change_node(NULL, INM_KM_SLEEP);
        if (!tchg_node) {
	    info("change node is null");
	    return -ENOMEM;
        }

        init_change_node(tchg_node, 0, INM_KM_SLEEP, NULL);
        ref_chg_node(tchg_node);
        tchg_node->type = NODE_SRC_METADATA;
	tchg_node->wostate = ecWriteOrderStateBitmap;
        inm_list_add_tail(&tchg_node->next, node_hd);
        *change_node = tchg_node;
    }

    chg = (disk_chg_t *)((char *)tchg_node->changes.cur_md_pgp +
			 (sizeof(disk_chg_t) * tchg_node->changes.change_idx));

    chg->offset = wmd->offset;
    chg->length = wmd->length;
    /* the time deltas are updated in completion routine */
    chg->time_delta = 0;
    chg->seqno_delta = 0;

    tchg_node->changes.change_idx++;
    tchg_node->changes.bytes_changes += wmd->length;

    nr_splits++;

if(IS_DBG_ENABLED(inm_verbosity, (INM_IDEBUG | INM_IDEBUG_BMAP))){
    info("leaving");
}

    return nr_splits;

}

/* ----------------------------------------------------------------------------
 * Function : ReadBitmapCompletion
 * Parameters : BitRuns - This indicates the changes that have been read
 *              to bit map
 * NOTES :
 * This function is called after read completion. This function is 
 * called at PASSIVE_LEVEL. This function queues the read changes to device
 * specific dirty block context. After queuing changes it checks if changes
 * cross low water mark the read is paused. If the changes read are the last
 * set of changes the read state is set to completed. In this function we
 * have to unset the bits successfully read and send a next read.
 * 
 * To avoid copying of the bitruns, we would use the previous read DB_WORK_ITEM for
 * clearing bits and allocate a new one for read if required.
 *
 * ------------------------------------------------------------------------------ */
/**
 * FUNCTION NAME: read_bitmap_completion
 * 
 * DESCRIPTION : This function is called by worker thread. This function 
 *         writes all bitrun changes into metadata change node, and
 *        clears corresponding bits in bitmap.
 * INPUT PARAMETERS : bmap_witem - ptr to bitmap work item.
 *               
 *              
 *              
 * 
 * OUTPUT PARAMETERS :
 * NOTES
 * 
 * When bitmap reads are ignored when bitmap is in write state/closed, 
 * 
 * return value :    
 *             
             
 * 
 **/
void read_bitmap_completion(bitmap_work_item_t *bmap_witem)
{
    volume_bitmap_t *vbmap = NULL;
    target_context_t *vcptr = NULL;
    change_node_t *change_node = NULL;
    struct inm_list_head node_hd;  /* change node list for bitmap change nodes */
    inm_s32_t clear_bits_read = 0, cont_bitmap_read = 0;
    inm_u64_t vc_nr_bytes_changes_read_from_bitmap = 0;
    inm_s32_t vc_split_changes_returned = 0;
    inm_s32_t vc_total_changes_pending = 0;
    struct inm_list_head *ptr = NULL, *nextptr = NULL;
    inm_u32_t i = 0;
    inm_u32_t tdelta = 0, sdelta = 0;
    inm_u64_t  time = 0, nr_seq = 0;
    unsigned char delay_bitmap_read = 0;

    // bmap_witem is later used for clear bits
    // PAGED_CODE(); (linux equivalent ??)
    //assert statement

if(IS_DBG_ENABLED(inm_verbosity, (INM_IDEBUG | INM_IDEBUG_BMAP))){
    info("entered");
}

    INM_INIT_LIST_HEAD(&node_hd);
    if (!bmap_witem || !bmap_witem->volume_bitmap) {
        info("null bmap_witem or volume bitmap");
        return;
    }
    
    // let us see if this bitmap is still operational
    get_volume_bitmap(bmap_witem->volume_bitmap);
    vbmap = bmap_witem->volume_bitmap;

    INM_DOWN(&vbmap->sem);

    //check if bitmap is still in read state ??
    if ((vbmap->eVBitmapState != ecVBitmapStateReadStarted) ||
        !vbmap->bitmap_api || !vbmap->volume_context ||
        (vbmap->volume_context->tc_flags & VCF_CV_FS_UNMOUNTED)) {
        // bitmap is closed, or bitmap is moved to write state,
        // Ignore this read 
        inm_list_del(&bmap_witem->list_entry);
        put_bitmap_work_item(bmap_witem);
        info("ignoring bitmap read - state = %s, bapi = %p, vcptr = %p",
	     get_volume_bitmap_state_string(vbmap->eVBitmapState),
	     vbmap->bitmap_api, vbmap->volume_context);

    } else {
        vcptr = vbmap->volume_context;
        do {
            //error processing
	    /* CASE 1: Error in reading bitmap
	     *        soln: set volume resync required flag
	     **/
	    if (bmap_witem->bit_runs.final_status && 
		(bmap_witem->bit_runs.final_status != EAGAIN)) {
		//    EAGAIN = STATUS_MORE_PROCESSING_REQUIRED 

		vbmap->eVBitmapState = ecVBitmapStateReadError;
		vcptr->tc_bp->num_bitmap_read_errors++; //TODOs: make it atomic
		set_volume_out_of_sync(vcptr, ERROR_TO_REG_BITMAP_READ_ERROR,
				       bmap_witem->bit_runs.final_status);
		info("received error %x setting bitmap state of volume %s to ecVBitmap error", bmap_witem->bit_runs.final_status, vbmap->volume_GUID);
		break;
	    }
        
            /* CASE 2: if changes are not added to volume context's
	     *        change list, then add here
	     *     TODOs: split large change into multiple changes
	     **/
	    // Read succeeded, check if any changes are returned
	    if (!bmap_witem->bit_runs.final_status &&
		!bmap_witem->bit_runs.nbr_runs) {
            
		vbmap->eVBitmapState = ecVBitmapStateReadCompleted;
		dbg("bitmap read of volume %s is completed", vbmap->volume_GUID);
		break;
	    }

	    for (i = 0; i < bmap_witem->bit_runs.nbr_runs; i++) {
		write_metadata_t wmd;
		wmd.offset = bmap_witem->bit_runs.runs[i].offset;
		wmd.length = bmap_witem->bit_runs.runs[i].length;
		vc_split_changes_returned =
		    add_metadata_bitmap_wostate(vcptr, &node_hd, &change_node, &wmd);
		if (vc_split_changes_returned <= 0) 
		    break;

		vc_nr_bytes_changes_read_from_bitmap +=
		    bmap_witem->bit_runs.runs[i].length;
		vc_total_changes_pending += vc_split_changes_returned;

		/*
		  if (vc_split_changes_returned > 1)
		  dirty_blk = NULL;
		  else 
		*/
	    }

	    volume_lock(vcptr);
	    inm_list_for_each_safe(ptr, nextptr, &node_hd) {
		unsigned short idx = 0;
		unsigned long lock_flag = 0;

		change_node = inm_list_entry(ptr, change_node_t, next);
		change_node->vcptr = vcptr;
		vcptr->tc_nr_cns++;
		change_node->transaction_id = 0;

		get_time_stamp_tag(&change_node->changes.start_ts);

		if (change_node->flags & KDIRTY_BLOCK_FLAG_SPLIT_CHANGE_MASK) {
		    /* maintaining single time stamp, and seq # for split ios */
		    if (change_node->seq_id_for_split_io == 1) {
		        time = change_node->changes.start_ts.TimeInHundNanoSecondsFromJan1601;
		        nr_seq = change_node->changes.start_ts.ullSequenceNumber;
		    } else {
			change_node->changes.start_ts.TimeInHundNanoSecondsFromJan1601 = time;
			change_node->changes.start_ts.ullSequenceNumber = nr_seq;
		    }
		    change_node->changes.end_ts.TimeInHundNanoSecondsFromJan1601 = time;
		    change_node->changes.end_ts.ullSequenceNumber = nr_seq;
		} else {
		    /* per-io time stamp changes , adds time stamp for each change*/
		    INM_SPIN_LOCK_IRQSAVE(&driver_ctx->time_stamp_lock, lock_flag);
		    sdelta = (driver_ctx->last_time_stamp_seqno - 
			  change_node->changes.start_ts.ullSequenceNumber);
		    driver_ctx->last_time_stamp_seqno += change_node->changes.change_idx;
		    tdelta = driver_ctx->last_time_stamp - 
			change_node->changes.start_ts.TimeInHundNanoSecondsFromJan1601;
		    INM_SPIN_UNLOCK_IRQRESTORE(&driver_ctx->time_stamp_lock, lock_flag);

		    change_node->changes.end_ts.ullSequenceNumber =
				(change_node->changes.start_ts.ullSequenceNumber +
				 sdelta + change_node->changes.change_idx);
		    change_node->changes.end_ts.TimeInHundNanoSecondsFromJan1601 =
			(change_node->changes.start_ts.TimeInHundNanoSecondsFromJan1601 +
			 tdelta);

		    for (idx = 0; idx < change_node->changes.change_idx; idx++) {
			disk_chg_t *dcp = (disk_chg_t *)
				((char *)change_node->changes.cur_md_pgp + 
						(sizeof(disk_chg_t) * idx));
			sdelta++;
			dcp->seqno_delta = sdelta;
			dcp->time_delta = tdelta;
if(IS_DBG_ENABLED(inm_verbosity, (INM_IDEBUG | INM_IDEBUG_BMAP))){
			print_chg_info(change_node, idx);
}
		    } 
		}

		vcptr->tc_pending_changes += change_node->changes.change_idx;
		vcptr->tc_pending_md_changes += change_node->changes.change_idx;
		vcptr->tc_bytes_pending_md_changes += 
                                            change_node->changes.bytes_changes;
		vcptr->tc_bytes_pending_changes += change_node->changes.bytes_changes;
		vcptr->tc_cnode_pgs++;
		add_changes_to_pending_changes(vcptr, change_node->wostate, change_node->changes.change_idx);
            
		dbg("nr of changes in chgnode (%p) = %d nr bytes changes = %d\n",
		    change_node, change_node->changes.change_idx,
		    change_node->changes.bytes_changes);
	    }

    	/*
    	 * when services stops (i) there is active data mode change
    	 * (ii) currently not on non write order change node list and
         * After this situation service starts, so bitmap will be read into
         * memory, before pushing bitmap change nodes into target context
         * change node list *ensure* that active data mode change is added to
         * the tc_nwo_dmode_list
         */
        if (vcptr->tc_cur_node && (vcptr->tc_optimize_performance &
            PERF_OPT_DRAIN_PREF_DATA_MODE_CHANGES_IN_NWO)) {
            change_node_t * chg_node_tp = vcptr->tc_cur_node;
            INM_BUG_ON(!chg_node_tp);
            if ((chg_node_tp->type == NODE_SRC_DATA ||
                 chg_node_tp->type == NODE_SRC_DATAFILE) &&
                chg_node_tp->wostate != ecWriteOrderStateData) {
                close_change_node(chg_node_tp, IN_BMAP_READ_PATH);
                inm_list_add_tail(&chg_node_tp->nwo_dmode_next,
                                  &vcptr->tc_nwo_dmode_list);
                if (vcptr->tc_optimize_performance & PERF_OPT_DEBUG_DATA_DRAIN) {
                    dbg("Appending chg:%p to tgt_ctxt:%p next:%p prev:%p mode:%d",
                    chg_node_tp, vcptr,chg_node_tp->nwo_dmode_next.next,
                    chg_node_tp->nwo_dmode_next.prev,
                    chg_node_tp->type);                  
                }
            }
        }
	    // insert node_head into tgt_ctxt->node_head
	    inm_list_splice_at_tail(&node_hd, &vcptr->tc_node_head);

	    vcptr->tc_bp->num_changes_read_from_bitmap += vc_total_changes_pending;
	    vcptr->tc_bp->num_byte_changes_read_from_bitmap +=
		vc_nr_bytes_changes_read_from_bitmap;
	    vcptr->tc_bp->num_of_times_bitmap_read++;
	    vcptr->tc_cur_node = NULL;
	    delay_bitmap_read = vcptr->tc_flags & (VCF_VOLUME_IN_BMAP_WRITE |
                                               VCF_VOLUME_IN_GET_DB);
	    volume_unlock(vcptr);

	    // if # of split changes are zero, then previous loop breaks
	    if (i < bmap_witem->bit_runs.nbr_runs) {
		vbmap->eVBitmapState = ecVBitmapStateOpened;
		break;
	    }

	    clear_bits_read = TRUE;

	    if (!bmap_witem->bit_runs.final_status) { // success
		// this is the last read
		vbmap->eVBitmapState = ecVBitmapStateReadCompleted;
		dbg("bitmap read for volume %s is completed",
		    vbmap->volume_GUID);
	    } else if (driver_ctx->tunable_params.db_low_water_mark_while_service_running &&
		       (vcptr->tc_pending_changes >= driver_ctx->tunable_params.db_low_water_mark_while_service_running)) {
		//bmap_witem->bit_runs.final_status is STATUS_MORE_PROCESSING_REQUIRED
		// if this is not a final read, and reached the lower water mark pause the reads.
		vbmap->eVBitmapState = ecVBitmapStateReadPaused;
		dbg("bitmap read for volume %s is paused",
		    vbmap->volume_GUID);
	    } else
		    if (!delay_bitmap_read) 
	    	    cont_bitmap_read = TRUE;
    		else  {
		        dbg("bitmap read for volume %s is paused for racing writes",
                    vbmap->volume_GUID);
    		    vbmap->eVBitmapState = ecVBitmapStateReadPaused;
    		}
        } while (0); /* TODOs : remove this loop */

        if (cont_bitmap_read) {
            bitmap_work_item_t *bwi_for_continuing_read = NULL;

            /* continue reading bitmap */
            bwi_for_continuing_read = allocate_bitmap_work_item(WITEM_TYPE_CONTINUE_BITMAP_READ);
            if (!bwi_for_continuing_read) {
                vbmap->eVBitmapState = ecVBitmapStateReadPaused;
                info("malloc failed : bitmap work item");
            } else {

                bwi_for_continuing_read->eBitmapWorkItem =
                    ecBitmapWorkItemContinueRead;
                get_volume_bitmap(vbmap);
                bwi_for_continuing_read->volume_bitmap = vbmap;
                continue_bitmap_read(bwi_for_continuing_read, (int)TRUE);
            }
        }

        if (clear_bits_read) {
            // assert statements
            bmap_witem->eBitmapWorkItem = ecBitmapWorkItemClearBits;
            bmap_witem->bit_runs.completion_callback =
                write_bitmap_completion_callback;
            INM_UP(&vbmap->sem);
            bitmap_api_clearbits(vbmap->bitmap_api, &bmap_witem->bit_runs);
            INM_DOWN(&vbmap->sem);
        } else {
            inm_list_del(&bmap_witem->list_entry);
            put_bitmap_work_item(bmap_witem);
        }

        if (vcptr && (vbmap->eVBitmapState == ecVBitmapStateReadCompleted)) {
            // check for switching to data filtering mode 
            //info("ecVBitmapStateReadCompleted ... :)");    
            /* TODOs: remove comments once testing is finished
	     */
            volume_lock(vcptr);
            if (is_data_filtering_enabled_for_this_volume(vcptr) &&
		(driver_ctx->service_state == SERVICE_RUNNING) &&
                can_switch_to_data_filtering_mode(vcptr)) {

                set_tgt_ctxt_filtering_mode(vcptr, FLT_MODE_DATA, TRUE);
            } else {
                set_tgt_ctxt_filtering_mode(vcptr, FLT_MODE_METADATA, FALSE);
            }

	    if (is_data_filtering_enabled_for_this_volume(vcptr) &&
		(driver_ctx->service_state == SERVICE_RUNNING) &&
		can_switch_to_data_wostate(vcptr)){
 
		// switch to data write order state
		set_tgt_ctxt_wostate(vcptr, ecWriteOrderStateData, FALSE, 
                             ecWOSChangeReasonUnInitialized);
		dbg("switched to data write order state\n");
            } else if ((driver_ctx->service_state == SERVICE_RUNNING) &&
		!vcptr->tc_pending_wostate_bm_changes && !vcptr->tc_pending_wostate_rbm_changes) {
		set_tgt_ctxt_wostate(vcptr, ecWriteOrderStateMetadata, FALSE,
                             ecWOSChangeReasonMDChanges);
		dbg("switched to metadata write order state\n");
	    }

            volume_unlock(vcptr);
        }

        /* notify service to drain the changes read from bitmap
	   INM_SPIN_LOCK_IRQSAVE(&vcptr->lock, lock_flag);
	   INM_WAKEUP_INTERRUPTIBLE(&vcptr->waitq);
	   INM_SPIN_UNLOCK_IRQRESTORE(&vcptr->lock, lock_flag);
	*/
	if(should_wakeup_s2(vcptr))
	    INM_WAKEUP_INTERRUPTIBLE(&vcptr->tc_waitq);
	
    }
    INM_UP(&vbmap->sem);
    put_volume_bitmap(vbmap);

if(IS_DBG_ENABLED(inm_verbosity, (INM_IDEBUG | INM_IDEBUG_BMAP))){
    info("leaving");
}

    return;
}
/**
 * FUNCTION NAME: continue_bitmap_read
 * 
 * DESCRIPTION :
 * 
 * INPUT PARAMETERS : bmap_witem - ptr to bitmap work item
 *               mutex_acquired - flag
 *              
 *              
 * 
 * OUTPUT PARAMETERS : bmap_witem->bit_runs
 * NOTES
 * 
 * return value :    0      - for success
 *             EINVAL - for invalid inputs
 <non-zero value> - for failure
 * 
 **/
void continue_bitmap_read(bitmap_work_item_t *bmap_witem, inm_s32_t mutex_acquired) {
    volume_bitmap_t *vbmap = NULL;

if(IS_DBG_ENABLED(inm_verbosity, (INM_IDEBUG | INM_IDEBUG_BMAP))){
    info("entered");
}

    if (!bmap_witem) {
        info("null bmap_witem");
        return;
    }

    vbmap = bmap_witem->volume_bitmap;
    if (!vbmap) {
        info("vbmap is null");
        return;
    }

    get_volume_bitmap(vbmap);

    if (!mutex_acquired)
        INM_DOWN(&vbmap->sem);
    
    if ((vbmap->eVBitmapState != ecVBitmapStateReadStarted) ||
	!vbmap->bitmap_api)
        put_bitmap_work_item(bmap_witem); // ignore read
    else {
        bmap_witem->bit_runs.context1 = bmap_witem;
        bmap_witem->bit_runs.completion_callback =
            read_bitmap_completion_callback;
        inm_list_add(&bmap_witem->list_entry, &vbmap->work_item_list);
        INM_UP(&vbmap->sem);
        bitmap_api_get_next_runs(vbmap->bitmap_api, &bmap_witem->bit_runs);
        INM_DOWN(&vbmap->sem);
    }

    if (!mutex_acquired)
        INM_UP(&vbmap->sem);

    put_volume_bitmap(vbmap);

if(IS_DBG_ENABLED(inm_verbosity, (INM_IDEBUG | INM_IDEBUG_BMAP))){
    info("leaving");
}

    return;
}
/**
 * FUNCTION NAME: continue_bitmap_read_worker_routine
 * 
 * DESCRIPTION : This function extracts bmap work item from work queue entry.
 * 
 * INPUT PARAMETERS : wqe - ptr to work queue entry
 *               
 *              
 *              
 * 
 * OUTPUT PARAMETERS :
 * NOTES
 * 
 * return value :    0      - for success
 *             EINVAL - for invalid inputs
 <non-zero value> - for failure
 * 
 **/
void continue_bitmap_read_worker_routine(wqentry_t *wqe) {
    bitmap_work_item_t *bmap_witem = NULL;

if(IS_DBG_ENABLED(inm_verbosity, (INM_IDEBUG | INM_IDEBUG_BMAP))){
    info("entered");
}

    if (!wqe) {
        info("wqe is null");
        return;
    }
    
    bmap_witem = (bitmap_work_item_t *) wqe->context;
    put_work_queue_entry(wqe);

    //assert(!bmap_witem);
    
    if (!bmap_witem) {
        info("bmap witem is null");
        return;
    }

    continue_bitmap_read(bmap_witem, FALSE);

if(IS_DBG_ENABLED(inm_verbosity, (INM_IDEBUG | INM_IDEBUG_BMAP))){
    info("leaving");
}

    return;
}
/* ---------------------------------------------------------------------------
 * Function : WriteBitmapCompletionCallback
 * Parameters : BitRuns - This indicates the changes that have been writen
 *              to bit map
 * NOTES :
 * This function is called after write completion. This function can be 
 * called at DISPACH_LEVEL. If called at DISPATCH_LEVEL this function queues worker 
 * routine to continue post write operation at PASSIVE_LEVEL. 
 *
 * --------------------------------------------------------------------------*/
/**
 * FUNCTION NAME: write_bitmap_completion_callback
 * 
 * DESCRIPTION :
 * 
 * INPUT PARAMETERS : bit_runs - ptr to bitruns
 *               
 *              
 *              
 * 
 * OUTPUT PARAMETERS :
 * NOTES
 * 
 * return value :    0      - for success
 *             EINVAL - for invalid inputs
 <non-zero value> - for failure
 * 
 **/
void write_bitmap_completion_callback(bitruns_t *bit_runs) {
    wqentry_t *wqe = NULL;
    bitmap_work_item_t *bmap_witem = NULL;

if(IS_DBG_ENABLED(inm_verbosity, (INM_IDEBUG | INM_IDEBUG_BMAP))){
    info("entered");
}

    if (!bit_runs) {
        info("bit_runs is null");
	return;
    }

    bmap_witem = (bitmap_work_item_t *)bit_runs->context1;
    if (INM_IN_INTERRUPT()) {
        /* DISPATCH LEVEL */
        wqe = alloc_work_queue_entry(INM_KM_NOSLEEP);
        if (!wqe) {
	    info("malloc failed:  wqe");
	    return;
        }
        wqe->context = bmap_witem;
        wqe->work_func = write_bitmap_completion_worker_routine;

        add_item_to_work_queue(&driver_ctx->wqueue, wqe);
    } else {
        /*  PASSIVE level */
        write_bitmap_completion(bmap_witem);
    }

if(IS_DBG_ENABLED(inm_verbosity, (INM_IDEBUG | INM_IDEBUG_BMAP))){
    info("leaving");
}

    return;
}

/*---------------------------------------------------------------------------
 * Function Name : WriteBitMapCompletion
 * Parameters :
 *      pWorkQueueEntry : pointer to work queue entry, Context member has 
 *              pointer to BitmapWorkItem. BitmapWorkItem.BitRuns has write 
 *              meta data/changes we requested to write in bit map.
 * NOTES :
 * This function is called at PASSIVE_LEVEL. This function is worker routine
 * called by worker thread at PASSIVE_LEVEL. This worker routine is scheduled
 * for execution by write completion routine.
 *
 * -----------------------------------------------------------------------------*/
/**
 * FUNCTION NAME: write_bitmap_completion
 * 
 * DESCRIPTION :
 * 
 * INPUT PARAMETERS : bmap_witem - ptr to bitmap work item
 *               
 *              
 *              
 * 
 * OUTPUT PARAMETERS :
 * NOTES
 * 
 * return value :    0      - for success
 *             EINVAL - for invalid inputs
 <non-zero value> - for failure
 * 
 **/
void write_bitmap_completion(bitmap_work_item_t *bmap_witem) {
    volume_bitmap_t *vbmap = NULL;
    target_context_t *vcptr = NULL;

if(IS_DBG_ENABLED(inm_verbosity, (INM_IDEBUG | INM_IDEBUG_BMAP))){
    info("entered");
}

    /* PAGED_CODE() */
    // assert statements
    if (!bmap_witem) {
        info("bmap_witem is null");
	return;
    }

    vbmap = bmap_witem->volume_bitmap;

    INM_DOWN(&vbmap->sem);
    vcptr = vbmap->volume_context;

    if (bmap_witem->eBitmapWorkItem == ecBitmapWorkItemSetBits) {
        volume_lock(vcptr);
        vcptr->tc_flags &= ~VCF_VOLUME_IN_BMAP_WRITE;
        volume_unlock(vcptr);

        if (bmap_witem->bit_runs.final_status) { // != 0
            info("setbits witem failed with status %x for volume %s",
		 bmap_witem->bit_runs.final_status, vbmap->volume_GUID);

            if ((vbmap->eVBitmapState != ecVBitmapStateClosed) && vcptr) {
                vcptr->tc_bp->num_bitmap_write_errors++; // make it atomic
                set_volume_out_of_sync(vcptr, ERROR_TO_REG_BITMAP_WRITE_ERROR,
				       bmap_witem->bit_runs.final_status);
                info("setting volume out of sync due to write error");
            }
        } else {
            if ((vbmap->eVBitmapState != ecVBitmapStateClosed) && vcptr) {
                volume_lock(vcptr);
                vcptr->tc_bp->num_changes_queued_for_writing -= bmap_witem->changes;
                vcptr->tc_bp->num_changes_written_to_bitmap += bmap_witem->changes;
                vcptr->tc_bp->num_byte_changes_queued_for_writing -=
                    bmap_witem->nr_bytes_changed_data;
                vcptr->tc_bp->num_byte_changes_written_to_bitmap +=
                    bmap_witem->nr_bytes_changed_data;
                volume_unlock(vcptr);
            } 
        }
    } else {
        // assert statement 
        if (vcptr && !(vcptr->tc_flags & VCF_CV_FS_UNMOUNTED) &&
            bmap_witem->bit_runs.final_status) {
            vcptr->tc_bp->num_bitmap_clear_errors++; //make it atomic
        }
    }

    // for all the cases, it is required to remove this entry from list.
    if (bmap_witem->eBitmapWorkItem == ecBitmapWorkItemSetBits) {
	unsigned long lock_flag = 0;
        INM_SPIN_LOCK_IRQSAVE(&vbmap->lock, lock_flag);
        //assert statement
        inm_list_del(&bmap_witem->list_entry);

        if ((vbmap->flags & VOLUME_BITMAP_FLAGS_WAITING_FOR_SETBITS_WORKITEM_LIST_EMPTY_NOTIFICATION) && inm_list_empty(&vbmap->set_bits_work_item_list)) {
            vbmap->flags &= ~VOLUME_BITMAP_FLAGS_WAITING_FOR_SETBITS_WORKITEM_LIST_EMPTY_NOTIFICATION;
            // &vbmap->set_bits_work_item_list_empty_notification
            INM_COMPLETE(&vbmap->set_bits_work_item_list_empty_notification);
        }
        INM_SPIN_UNLOCK_IRQRESTORE(&vbmap->lock, lock_flag);

    } else {
	// assert statements
        inm_list_del(&bmap_witem->list_entry);
    }
    INM_UP(&vbmap->sem);

    put_bitmap_work_item(bmap_witem);

if(IS_DBG_ENABLED(inm_verbosity, (INM_IDEBUG | INM_IDEBUG_BMAP))){
    info("leaving");
}

    return;
}

/*---------------------------------------------------------------------------
 * Function Name : write_bitmap_completion_worker_routine
 * Parameters :
 *      pWorkQueueEntry : pointer to work queue entry, Context member has
 *              pointer to BitRuns. BitRuns has the write meta data/changes
 *              we requested to write in bit map.
 * NOTES :
 * This function is called at PASSIVE_LEVEL. This function is worker routine
 * called by worker thread at PASSIVE_LEVEL. This worker routine is scheduled
 * for execution by write completion routine.
 *
 *      This function removes queued write work items in device specific dirty
 * block context and derefences the dirty block context.
 *
 *-----------------------------------------------------------------------------
 */
// This function is called at PASSIVE_LEVEL

void
write_bitmap_completion_worker_routine(wqentry_t *wqep)
{
    bitmap_work_item_t *bwip = NULL;

    bwip = (bitmap_work_item_t *) wqep->context;
    put_work_queue_entry(wqep);
    write_bitmap_completion(bwip);
}


/**
 * FUNCTION NAME: queue_worker_routine_for_continue_bitmap_read
 * 
 * DESCRIPTION : This function queues work queue entry for bitmap read
 *         called by service thread.
 * 
 * INPUT PARAMETERS : vbmap - ptr to volume bitmap
 *               
 *              
 *              
 * 
 * OUTPUT PARAMETERS :
 * NOTES
 * 
 * return value :    0      - for success
 *             EINVAL - for invalid inputs
 <non-zero value> - for failure
 * 
 **/
void queue_worker_routine_for_continue_bitmap_read(volume_bitmap_t *vbmap) {
    wqentry_t *wqe = NULL;
    bitmap_work_item_t *bmap_witem = NULL;

if(IS_DBG_ENABLED(inm_verbosity, (INM_IDEBUG | INM_IDEBUG_BMAP))){
    info("entered");
}

    if (!vbmap) {
        info("null vbmap");
        return;
    }

    wqe = alloc_work_queue_entry(INM_KM_SLEEP);
    if (!wqe) {
        info("malloc failed : wqe");
        return;
    }

    bmap_witem = allocate_bitmap_work_item(WITEM_TYPE_CONTINUE_BITMAP_READ);
    if (!bmap_witem) {
        info("malloc failed : bmap_witem");
	put_work_queue_entry(wqe);
	return;
    }

    // fill bitmap work item
    bmap_witem->eBitmapWorkItem = ecBitmapWorkItemContinueRead;
    get_volume_bitmap(vbmap);
    bmap_witem->volume_bitmap = vbmap;

    //fill work queue entry
    wqe->witem_type = WITEM_TYPE_CONTINUE_BITMAP_READ;
    wqe->context = bmap_witem;
    wqe->work_func = continue_bitmap_read_worker_routine;

    add_item_to_work_queue(&driver_ctx->wqueue, wqe);

if(IS_DBG_ENABLED(inm_verbosity, (INM_IDEBUG | INM_IDEBUG_BMAP))){
    info("leaving");
}

    return;
}


const char *
get_volume_bitmap_state_string(etVBitmapState bmap_state)
{
    switch (bmap_state) {
    case ecVBitmapStateUnInitialized:
	return "ecVBitmapStateUnInitialized";
	break;
    case ecVBitmapStateOpened:
	return "ecVBitmapStateOpened";
	break;
    case ecVBitmapStateReadStarted:
	return "ecVBitmapStateReadStarted";
	break;
    case ecVBitmapStateReadPaused:
	return "ecVBitmapStateReadPaused";
	break;
    case ecVBitmapStateReadCompleted:
	return "ecVBitmapStateReadCompleted";
	break;
    case ecVBitmapStateAddingChanges:
	return "ecVBitmapStateAddingChanges";
	break;
    case ecVBitmapStateClosed:
	return "ecVBitmapStateClosed";
	break;
    case ecVBitmapStateReadError:
	return "ecVBitmapStateReadError";
	break;
    case ecVBitmapStateInternalError:
	return "ecVBitmapStateInternalError";
	break;
    default:
	return "ecVBitmapStateUnknown";
	break;
    }
}

void close_bitmap_file_on_tgt_ctx_deletion(volume_bitmap_t *vbmap,
				target_context_t *vcptr)
{
    close_bitmap_file(vbmap, TRUE);
    vcptr->tc_bp->volume_bitmap = NULL;
}

//------------------------------------
/**
 * FUNCTION NAME:
 * 
 * DESCRIPTION :
 * 
 * INPUT PARAMETERS : wqeptr=ptr to work qeueue entry
 *               
 *              
 *              
 * 
 * OUTPUT PARAMETERS : void
 * NOTES
 * 
 * return value :    0      - for success
 *             EINVAL - for invalid inputs
 <non-zero value> - for failure
 * 
 **/

void process_vcontext_work_items(wqentry_t *wqeptr)
{
    target_context_t *vcptr = NULL;    
    volume_bitmap_t *vbmap = NULL;
    inm_u32_t ret = 0;
    struct inm_list_head change_node_list;
    wqentry_t *wqep;
    bitmap_work_item_t *bwip;
    struct inm_list_head *curp = NULL, *nxtp = NULL;
    change_node_t *cnp = NULL;

if(IS_DBG_ENABLED(inm_verbosity, (INM_IDEBUG | INM_IDEBUG_BMAP))){
    info("entered");
}

    if (!wqeptr) {
        info("invalid wqeptr");
        return;
    }

    vcptr = (target_context_t *)wqeptr->context;
    if (!vcptr) {
        info("invalid vcptr");
        return;
    }

    volume_lock(vcptr);
    if(vcptr->tc_bp->volume_bitmap) {
        get_volume_bitmap(vcptr->tc_bp->volume_bitmap);
        vbmap = vcptr->tc_bp->volume_bitmap;
    }
    volume_unlock(vcptr);
        

    if ( (vbmap) && (vcptr->tc_cur_wostate == ecWriteOrderStateRawBitmap)) {
        INM_DOWN(&vbmap->sem);
        ret = move_rawbitmap_to_bmap(vcptr, FALSE);
        INM_UP(&vbmap->sem);

        if (vcptr->tc_prev_wostate == ecWriteOrderStateUnInitialized && ret < 0) {// last chance changes are zero
            info("move_rawbitmap_to_bmap operation failed \n");
        }
    }

    dbg("volume:%s pending changes = %lld work_item:%u\n", 
        vcptr->tc_guid,vcptr->tc_pending_changes, wqeptr->witem_type);
    switch(wqeptr->witem_type)
    {
    case WITEM_TYPE_OPEN_BITMAP:
	if (vcptr->tc_flags & VCF_VOLUME_STACKED_PARTIALLY)
		return; 

        /* TODOs : temporary code, need to be removed once lock issue fixed */
        if ((driver_ctx->sys_shutdown) &&
            (inm_dev_id_get(vcptr) == driver_ctx->root_dev))
	    break;

        if (!vbmap) {
            inm_s32_t status = 0;

if(IS_DBG_ENABLED(inm_verbosity, (INM_IDEBUG | INM_IDEBUG_BMAP))){
            info(" opening bitmap for volume %s\n", vcptr->tc_guid);
}

            vbmap = open_bitmap_file(vcptr, &status);
            volume_lock(vcptr);
            if (status || !vbmap) {
                info("open bitmap for volume:%s has failed = %d", vcptr->tc_guid, status);
                // TODOs: set error for open bitmap, so that further bitmap opens are 
                // not done
                set_bitmap_open_error(vcptr, TRUE, status);
                //vbmap->eVBitmapState = ecVBitmapStateClosed;
            } else {
                vcptr->tc_flags &= ~VCF_OPEN_BITMAP_REQUESTED;

               /* There is a lack of synchronisation between this function and do_stop_filtering and
                * is as follows:
                *
                * Before assigning vbmap to vcptr->tc_bp->volume_bitmap, if do_stop_filtering
                * get executed completely, no one is responsible for closing the bitmap file.
                *
                * So if the target is undergone for deletion, do the do_stop_filtering task
                * here itself.
                */
                if (vbmap) {
                   if (vcptr->tc_flags & VCF_VOLUME_DELETING) {
                       volume_unlock(vcptr);
		               close_bitmap_file_on_tgt_ctx_deletion(vbmap, vcptr);
		               vbmap = NULL;
                       volume_lock(vcptr);
                   } else {
                       get_volume_bitmap(vbmap);
                       vcptr->tc_bp->volume_bitmap = vbmap;
                   }
                }
            }
            volume_unlock(vcptr);
        }
        break;

    case WITEM_TYPE_BITMAP_WRITE:
        if (vbmap) {

if(IS_DBG_ENABLED(inm_verbosity, (INM_IDEBUG | INM_IDEBUG_BMAP))){
            info("writing to bitmap for volume %s\n", vcptr->tc_guid);
}
	    if ((vcptr->tc_cur_wostate == ecWriteOrderStateRawBitmap) &&
		(is_bmaphdr_loaded(vbmap) == FALSE)) {
		set_volume_out_of_sync(vcptr, ERROR_TO_REG_BITMAP_OPEN_FAIL_CHANGES_LOST, 0);
		break;
	    }

	    wqep = alloc_work_queue_entry(INM_KM_SLEEP);
	    if(!wqep){
		ret = INM_ENOMEM;
		break;
	    }

	    bwip = allocate_bitmap_work_item(WITEM_TYPE_BITMAP_WRITE);
	    if(!bwip){
		put_work_queue_entry(wqep);
		ret = INM_ENOMEM;
		break;
	    }

        INM_DOWN(&vbmap->sem);
        volume_lock(vcptr);

        vbmap->eVBitmapState = ecVBitmapStateAddingChanges;

	    if (inm_list_empty(&vcptr->tc_node_head)) {
		volume_unlock(vcptr);
		put_work_queue_entry(wqep);
		put_bitmap_work_item(bwip);
		goto busy_wait;
	    }

	    vcptr->tc_flags |= VCF_VOLUME_IN_BMAP_WRITE;
	    /* Reset changes related stats here */
	    update_target_context_stats(vcptr);

        /* Before writing to bitmap, remove data mode node from
         * non write order list
         */
        inm_list_for_each_safe(curp, nxtp, &vcptr->tc_nwo_dmode_list) {
            inm_list_del_init(curp);
        }
	    vcptr->tc_cur_node = NULL;
	    list_change_head(&change_node_list, &vcptr->tc_node_head);
	    INM_INIT_LIST_HEAD(&vcptr->tc_node_head);
        if (vcptr->tc_pending_confirm && !(vcptr->tc_pending_confirm->flags & CHANGE_NODE_ORPHANED)) {
            cnp = vcptr->tc_pending_confirm;
            inm_list_del_init(&cnp->next);
            inm_list_add_tail(&cnp->next, &vcptr->tc_node_head);
            if ((vcptr->tc_optimize_performance &
                 PERF_OPT_DRAIN_PREF_DATA_MODE_CHANGES_IN_NWO) &&
                (cnp->type == NODE_SRC_DATA || cnp->type == NODE_SRC_DATAFILE) &&
                (cnp->wostate != ecWriteOrderStateData)) {
                inm_list_add_tail(&cnp->nwo_dmode_next, &vcptr->tc_nwo_dmode_list);
                if (vcptr->tc_optimize_performance & PERF_OPT_DEBUG_DATA_DRAIN) {
                dbg("Appending chg:%p to tgt_ctxt:%p next:%p prev:%p mode:%d",
                    cnp, vcptr, cnp->nwo_dmode_next.next, cnp->nwo_dmode_next.prev,
                    cnp->type);
                }
            }
            vcptr->tc_pending_changes = cnp->changes.change_idx;
            if (cnp->type == NODE_SRC_METADATA) {
                vcptr->tc_pending_md_changes = cnp->changes.change_idx;
                vcptr->tc_bytes_pending_md_changes = cnp->changes.bytes_changes;
            }
            vcptr->tc_bytes_pending_changes = cnp->changes.bytes_changes;
            add_changes_to_pending_changes(vcptr, cnp->wostate, cnp->changes.change_idx);
    
            vcptr->tc_bp->num_changes_queued_for_writing -= cnp->changes.change_idx;
            vcptr->tc_bp->num_byte_changes_queued_for_writing -=
                cnp->changes.bytes_changes;
        }

        if ((vcptr->tc_cur_wostate != ecWriteOrderStateBitmap) &&
            (vcptr->tc_cur_wostate != ecWriteOrderStateRawBitmap)) {
            set_tgt_ctxt_wostate(vcptr, ecWriteOrderStateBitmap, FALSE,
                                 ecWOSChangeReasonBitmapChanges);
        }
	    volume_unlock(vcptr);
	    ret = queue_worker_routine_for_bitmap_write(vcptr, wqeptr->extra1, vbmap,
						&change_node_list, wqep, bwip);
busy_wait:
	    volume_lock(vcptr);
            vcptr->tc_bp->bmap_busy_wait = FALSE;
            volume_unlock(vcptr);
            INM_UP(&vbmap->sem); // TODOs: this should be fast mutex
            if (ret < 0) {
                err("bmap write op failed for volume %s [err code = %d]",
                    vcptr->tc_guid, ret);
            }
        } else {
            set_bitmap_open_fail_due_to_loss_of_changes(vcptr, FALSE);
        }
        break;
    case WITEM_TYPE_START_BITMAP_READ:
        /* reading bitmap in raw mode is not allowed */
        if (vbmap && (vcptr->tc_cur_wostate != ecWriteOrderStateRawBitmap)) {

if(IS_DBG_ENABLED(inm_verbosity, (INM_IDEBUG | INM_IDEBUG_BMAP))){
            info("starting read from bitmap for volume %s\n",
                 vcptr->tc_guid);
}

            INM_DOWN(&vbmap->sem);
            if ((vbmap->eVBitmapState == ecVBitmapStateOpened) ||
                (vbmap->eVBitmapState == ecVBitmapStateAddingChanges)){

                vbmap->eVBitmapState = ecVBitmapStateReadStarted;
                queue_worker_routine_for_start_bitmap_read(vbmap);
            }
            INM_UP(&vbmap->sem);
        } else {
            err("volume is in raw bitmap write order state/null vbmap(vc-wostate = %d\n)",
		vcptr->tc_cur_wostate);
        }
        break;

    case WITEM_TYPE_CONTINUE_BITMAP_READ:
        if (vbmap) {

if(IS_DBG_ENABLED(inm_verbosity, (INM_IDEBUG | INM_IDEBUG_BMAP))){
            info("starting read from bitmap for volume %s\n",
                 vcptr->tc_guid);
}

            INM_DOWN(&vbmap->sem);
            if ((vbmap->eVBitmapState == ecVBitmapStateReadStarted) ||
                (vbmap->eVBitmapState == ecVBitmapStateReadPaused)) {

                vbmap->eVBitmapState = ecVBitmapStateReadStarted;
                queue_worker_routine_for_continue_bitmap_read(vbmap);
            }
            INM_UP(&vbmap->sem);
	} else {
	    err("null vol_bitmap for volume = %s\n", vcptr->tc_guid);
	}
	break;

    case WITEM_TYPE_VOLUME_UNLOAD:            

	dbg("Processing VOLUME UNLOAD work item \n");            
	if (!inm_list_empty(&vcptr->tc_node_head)) {
	    if (!vbmap) {
		inm_s32_t status = 0;
		vbmap = open_bitmap_file(vcptr, &status);
	    }
	}

	if(vbmap){
	    wqep = alloc_work_queue_entry(INM_KM_SLEEP);
   	    if(!wqep){
		ret = INM_ENOMEM;
		break;
	    }

	    bwip = allocate_bitmap_work_item(WITEM_TYPE_BITMAP_WRITE);
	    if(!bwip){
		put_work_queue_entry(wqep);
		ret = INM_ENOMEM;
		break;
	    }

	    volume_lock(vcptr);

	    vcptr->tc_flags |= VCF_VOLUME_IN_BMAP_WRITE;
        vbmap->eVBitmapState = ecVBitmapStateAddingChanges;

	    if (inm_list_empty(&vcptr->tc_node_head)) {
		volume_unlock(vcptr);
		put_work_queue_entry(wqep);
		put_bitmap_work_item(bwip);
		goto bitmap_close;
	    }

	    /* Reset changes related stats here */
	    update_target_context_stats(vcptr);

        /* Before writing to bitmap, remove data mode node from
         * non write order list
         */
        inm_list_for_each_safe(curp, nxtp, &vcptr->tc_nwo_dmode_list) {
            inm_list_del_init(curp);
        }

	    vcptr->tc_cur_node = NULL;
	    list_change_head(&change_node_list, &vcptr->tc_node_head);
	    INM_INIT_LIST_HEAD(&vcptr->tc_node_head);
        if (vcptr->tc_pending_confirm && !(vcptr->tc_pending_confirm->flags & CHANGE_NODE_ORPHANED)) {
            cnp = vcptr->tc_pending_confirm;
            inm_list_del_init(&cnp->next);
            inm_list_add_tail(&cnp->next, &vcptr->tc_node_head);
            if ((vcptr->tc_optimize_performance &
                 PERF_OPT_DRAIN_PREF_DATA_MODE_CHANGES_IN_NWO) &&
                (cnp->type == NODE_SRC_DATA || cnp->type == NODE_SRC_DATAFILE) &&
                (cnp->wostate != ecWriteOrderStateData)) {
                inm_list_add_tail(&cnp->nwo_dmode_next, &vcptr->tc_nwo_dmode_list);
                if (vcptr->tc_optimize_performance & PERF_OPT_DEBUG_DATA_DRAIN) {
                    dbg("Appending chg:%p to tgt_ctxt:%p", cnp, vcptr);
                }
            }
            vcptr->tc_pending_changes = cnp->changes.change_idx;
            if (cnp->type == NODE_SRC_METADATA) {
                vcptr->tc_pending_md_changes = cnp->changes.change_idx;
                vcptr->tc_bytes_pending_md_changes = cnp->changes.bytes_changes;
            }
            vcptr->tc_bytes_pending_changes = cnp->changes.bytes_changes;
            add_changes_to_pending_changes(vcptr, cnp->wostate, cnp->changes.change_idx);
    
            vcptr->tc_bp->num_changes_queued_for_writing -= cnp->changes.change_idx;
            vcptr->tc_bp->num_byte_changes_queued_for_writing -=
                cnp->changes.bytes_changes;
        }

	    if ((vcptr->tc_cur_wostate != ecWriteOrderStateBitmap) &&
            (vcptr->tc_cur_wostate != ecWriteOrderStateRawBitmap)) {
            set_tgt_ctxt_wostate(vcptr, ecWriteOrderStateBitmap, FALSE,
                                 ecWOSChangeReasonBitmapChanges);
	    }
	    volume_unlock(vcptr);
	    ret = queue_worker_routine_for_bitmap_write(vcptr, 0, vbmap,
						&change_node_list, wqep, bwip);
	    if (ret < 0) {
	        err("bmap write op failed for volume %s [ err code = %d ]",
	            vcptr->tc_guid, ret);
	    }
	}

bitmap_close:
	///driver_ctx->data_file_writer_manager->free_volume_writer(vcptr);
	flush_and_close_bitmap_file(vcptr);

	break;
            
    default:
	dbg("Unknow work-item type - 0x%x \n", wqeptr->witem_type);
	break;            
    }

    //put_volume_context(vcptr); //derefence wqeptr->context
    put_tgt_ctxt(vcptr);//derefence wqeptr->context ... doing it in the caling function 
    //  put_work_queue_entry(wqeptr); //dereference wqeptr

    if (vbmap)
        put_volume_bitmap(vbmap);

    return;
}

inm_s32_t add_vc_workitem_to_list(inm_u32_t witem_type, target_context_t *vcptr,
                            inm_u32_t extra1, inm_u8_t open_bitmap,
                            struct inm_list_head *lhptr)
{
    inm_s32_t success = 1;
    wqentry_t *wqe = NULL;

if(IS_DBG_ENABLED(inm_verbosity, (INM_IDEBUG | INM_IDEBUG_BMAP))){
    info("entered");
}

    /* TODOs: check for drive letter */
    /* in linux we don't have driver letter */

    if (witem_type != WITEM_TYPE_OPEN_BITMAP) {
        if (!vcptr->tc_bp->volume_bitmap &&
            !(vcptr->tc_flags & VCF_OPEN_BITMAP_REQUESTED) &&
            open_bitmap) {

            dbg("Queuing open bitmap for volume = %p\n", vcptr);
            wqe = alloc_work_queue_entry(INM_KM_NOSLEEP);
            if (!wqe) {
                info("Failed to allocate work queue entry for bitmap open\n");
                return !success;
            }

            vcptr->tc_flags |= VCF_OPEN_BITMAP_REQUESTED;
            wqe->witem_type = WITEM_TYPE_OPEN_BITMAP;
            get_tgt_ctxt(vcptr);
            wqe->context = vcptr;
            wqe->extra1 = 0;
            inm_list_add_tail(&wqe->list_entry, lhptr);
            wqe = NULL;
        }
    }

    wqe = alloc_work_queue_entry(INM_KM_NOSLEEP);
    if (wqe == NULL)
        return !success;

    wqe->witem_type = witem_type;
    //wqe->context = get_volume_context(vcptr);
    get_tgt_ctxt(vcptr);
    wqe->context = vcptr;
    wqe->extra1 = extra1;    /* TODOs: additional info */
    inm_list_add_tail(&wqe->list_entry, lhptr);

if(IS_DBG_ENABLED(inm_verbosity, (INM_IDEBUG | INM_IDEBUG_BMAP))){
    info("leaving");
}

    return success;
}

/**
 * FUNCTION NAME:
 * 
 * DESCRIPTION :
 * 
 * INPUT PARAMETERS : 
 *               
 *              
 *              
 * 
 * OUTPUT PARAMETERS :
 * NOTES
 * 
 * return value :    0      - for success
 *             EINVAL - for invalid inputs
 <non-zero value> - for failure
 * 
 **/
void
fill_bitmap_filename_in_volume_context(target_context_t *vcptr)
{

if(IS_DBG_ENABLED(inm_verbosity, (INM_IDEBUG | INM_IDEBUG_BMAP))){
    info("entered");
}

    if (!vcptr)
        return;

    snprintf(vcptr->tc_bp->bitmap_file_name, 
            sizeof(vcptr->tc_bp->bitmap_file_name), "%s/%s%s%s", 
            INM_BMAP_DEFAULT_DIR_DEPRECATED,
            LOG_FILE_NAME_PREFIX, vcptr->tc_pname, LOG_FILE_NAME_SUFFIX);

    if (INM_BMAP_ALLOW_DEPRECATED(vcptr->tc_bp->bitmap_file_name)) {
        snprintf(vcptr->tc_bp->bitmap_dir_name, 
                sizeof(vcptr->tc_bp->bitmap_dir_name), "%s",
                INM_BMAP_DEFAULT_DIR_DEPRECATED);

        info("Using deprecated file %s", vcptr->tc_bp->bitmap_file_name);
    } else {
        snprintf(vcptr->tc_bp->bitmap_dir_name, 
                sizeof(vcptr->tc_bp->bitmap_dir_name), "%s/%s",
                PERSISTENT_DIR, vcptr->tc_pname); 

        snprintf(vcptr->tc_bp->bitmap_file_name, 
                sizeof(vcptr->tc_bp->bitmap_file_name), "%s/%s%s%s", 
                vcptr->tc_bp->bitmap_dir_name,
                LOG_FILE_NAME_PREFIX, vcptr->tc_pname, LOG_FILE_NAME_SUFFIX);
    }

if(IS_DBG_ENABLED(inm_verbosity, (INM_IDEBUG | INM_IDEBUG_BMAP))){
        info("bitmap_file_name:%s",vcptr->tc_bp->bitmap_file_name);
}


if(IS_DBG_ENABLED(inm_verbosity, (INM_IDEBUG | INM_IDEBUG_BMAP))){
    info("leaving");
}

}
/**
 * FUNCTION NAME:
 * 
 * DESCRIPTION :
 * 
 * INPUT PARAMETERS : 
 *               
 *              
 *              
 * 
 * OUTPUT PARAMETERS :
 * NOTES
 * 
 * return value :    0      - for success
 *             EINVAL - for invalid inputs
 <non-zero value> - for failure
 * 
 **/
void set_bitmap_open_fail_due_to_loss_of_changes(target_context_t *vcptr, inm_s32_t lock_acquired) {

if(IS_DBG_ENABLED(inm_verbosity, (INM_IDEBUG | INM_IDEBUG_BMAP))){
    info("entered");
}

    if (!vcptr) {
        return;
    }
    
    /* TODOs: measure time */
    if (!lock_acquired)
        volume_lock(vcptr);

    /* As we are losing changes, it is required to set resync required
     * and also disable filtering temporarily for this volume
     **/
    vcptr->tc_flags |= VCF_OPEN_BITMAP_FAILED;
    vcptr->tc_flags |= VCF_FILTERING_STOPPED;

    telemetry_set_dbs(&vcptr->tc_tel.tt_blend, DBS_FILTERING_STOPPED_BY_KERNEL);

    stop_filtering_device(vcptr, TRUE, NULL);
    dbg("stop filtering device .. functionality yet to implement ");

    if(!lock_acquired)
        volume_unlock(vcptr);

    set_volume_out_of_sync(vcptr, ERROR_TO_REG_BITMAP_OPEN_FAIL_CHANGES_LOST,
                           0);
    
if(IS_DBG_ENABLED(inm_verbosity, (INM_IDEBUG | INM_IDEBUG_BMAP))){
    info("leaving");
}

    return;
}
/**
 * FUNCTION NAME:
 * 
 * DESCRIPTION :
 * 
 * INPUT PARAMETERS : 
 *               
 *              
 *              
 * 
 * OUTPUT PARAMETERS :
 * NOTES
 * 
 * return value :    0      - for success
 *             EINVAL - for invalid inputs
 <non-zero value> - for failure
 * 
 **/
void set_bitmap_open_error(target_context_t *vcptr, inm_s32_t lock_acquired,
                           inm_s32_t status)
{
    
    inm_s32_t fail_further_opens = FALSE;
    inm_s32_t out_of_sync = FALSE;
    
if(IS_DBG_ENABLED(inm_verbosity, (INM_IDEBUG | INM_IDEBUG_BMAP))){
    info("entered");
}

    if(!vcptr)
	return;

    /* TODOs: time issues */
    if (!lock_acquired)
        volume_lock(vcptr);

    //vcptr->stats.num_bitmap_open_errors++;
    vcptr->tc_bp->num_bitmap_open_errors++;

    /* TODOs: handle time issues */
    //if (vcptr->stats.num_bitmap_open_errors > MAX_BITMAP_OPEN_ERRORS_TO_STOP_FILTERING)
    if (vcptr->tc_bp->num_bitmap_open_errors >
	MAX_BITMAP_OPEN_ERRORS_TO_STOP_FILTERING)
	fail_further_opens = TRUE;

    if (fail_further_opens) {
        vcptr->tc_flags |= VCF_OPEN_BITMAP_FAILED;
        vcptr->tc_flags |= VCF_FILTERING_STOPPED;

        telemetry_set_dbs(&vcptr->tc_tel.tt_blend, DBS_FILTERING_STOPPED_BY_KERNEL);
        out_of_sync = TRUE;
        stop_filtering_device(vcptr, TRUE, NULL);
        info("bitmap open error on volume %s\n", vcptr->tc_guid);
    }

    if (!lock_acquired)
        volume_unlock(vcptr);

    if (out_of_sync)
        set_volume_out_of_sync(vcptr, ERROR_TO_REG_BITMAP_OPEN_ERROR, status);

    return;
}
/**
 * FUNCTION NAME:
 * 
 * DESCRIPTION :
 * 
 * INPUT PARAMETERS : 
 *               
 *              
 *              
 * 
 * OUTPUT PARAMETERS :
 * NOTES
 * 
 * return value :    0      - for success
 *             EINVAL - for invalid inputs
 <non-zero value> - for failure
 * 
 **/
inm_s32_t can_open_bitmap_file(target_context_t *vcptr, inm_s32_t lose_changes)
{

if(IS_DBG_ENABLED(inm_verbosity, (INM_IDEBUG | INM_IDEBUG_BMAP))){
    info("entered");
}

    if (!vcptr) /* TODOs: check validity of i/p params */
        return -EINVAL;

    if (vcptr->tc_flags & (VCF_FILTERING_STOPPED | VCF_OPEN_BITMAP_FAILED)) {
        return FALSE;
    }

	if (vcptr->tc_flags & VCF_VOLUME_STACKED_PARTIALLY) {
		return FALSE;
	}

    if (lose_changes || !vcptr->tc_bp->num_bitmap_open_errors) {
        return TRUE;
    }

    /* TODOs: handle time issues and checks */
    return FALSE;
//    return TRUE;
}

/*-----------------------------------------------------------------------------
 * Function Name : QueueWorkerRoutineForBitmapWrite
 * Parameters :
 *      pVolumeContext : This parameter is referenced and stored in the 
 *                            work item.
 *      ulNumDirtyChanges   : if this value is zero all the dirty changes are 
 *                            copied. If this value is non zero, minimum of 
 *                            ulNumDirtyChanges are copied.
 * Retruns :
 *      BOOLEAN             : TRUE if succeded in queing work item for write
 *                            FALSE if failed in queing work item
 * NOTES :
 * This function is called at DISPATCH_LEVEL. This function is called with 
 * Spinlock to VolumeContext held.
 *
 *      This function removes changes from volume context and queues set bits
 * work item for each dirty block. These work items are later removed and
 * processed by Work queue
 *      
 *
 *-------------------------------------------------------------------------
 */
/**
 * FUNCTION NAME: queue_worker_routine_for_bitmap_write
 * 
 * DESCRIPTION : This function queues all the change nodes to write into bitmap,
 *     and queues the change nodes for cleanup.
 * 
 * INPUT PARAMETERS : vcptr - ptr to target context.
 *               nr_dirty_changes -
 *              
 *              
 * 
 * OUTPUT PARAMETERS :
 * NOTES
 * 
 * return value :    0      - for success
 *             EINVAL - for invalid inputs
 <non-zero value> - for failure
 * 
 **/
inm_s32_t queue_worker_routine_for_bitmap_write(target_context_t *vcptr,
	inm_u64_t  nr_dirty_changes, volume_bitmap_t *vbmap,
	struct inm_list_head *change_node_list, wqentry_t *wqep, bitmap_work_item_t *bwip)
{
    inm_u64_t  nr_changes_copied = 0;
    unsigned long lock_flag = 0;
    inm_s32_t ret = -1;

if(IS_DBG_ENABLED(inm_verbosity, (INM_IDEBUG | INM_IDEBUG_BMAP))){
    info("entered");
}

    while(!inm_list_empty(change_node_list)) {
        change_node_t *change_node = NULL;
        inm_u32_t nr_chgs = 0;
        unsigned short rem;

        // check whether it exceeded the limit
        if (nr_dirty_changes && nr_changes_copied >= nr_dirty_changes)
            break;

        change_node = inm_list_entry(change_node_list->next, change_node_t, next);
        inm_list_del(&change_node->next);

        // tag-change nodes should be dropped
        if (change_node->type == NODE_SRC_TAGS) {

            telemetry_log_tag_history(change_node, vcptr, 
                                      ecTagStatusDropped, 
                                      ecBitmapWrite, ecMsgTagDropped);
            
            INM_ATOMIC_INC(&change_node->vcptr->tc_stats.num_tags_dropped);
            if (change_node->tag_guid) {
                change_node->tag_guid->status[change_node->tag_status_idx] = STATUS_DROPPED;
                INM_WAKEUP_INTERRUPTIBLE(&change_node->tag_guid->wq);
                change_node->tag_guid = NULL;
            }

            if (change_node->flags & CHANGE_NODE_FAILBACK_TAG) {
                info("The failback tag is dropped for disk %s, dirty block = %p",
                          vcptr->tc_guid, change_node);
                set_tag_drain_notify_status(vcptr, TAG_STATUS_DROPPED,
                                                   DEVICE_STATUS_NON_WRITE_ORDER_STATE);
            }

            commit_change_node(change_node);
        } else {
if(IS_DBG_ENABLED(inm_verbosity, (INM_IDEBUG | INM_IDEBUG_BMAP))){
            info("removed change node %p from vcptr %p\n", change_node, vcptr);
}
	    rem = change_node->changes.change_idx;

            while (!inm_list_empty(&change_node->changes.md_pg_list)) {
                inm_page_t *pgp = NULL;

                nr_chgs = min((inm_u32_t)rem, (inm_u32_t)MAX_CHANGE_INFOS_PER_PAGE);

                pgp = inm_list_entry(change_node->changes.md_pg_list.next,inm_page_t, entry);
                inm_list_del(&pgp->entry);
                pgp->nr_chgs = nr_chgs;
                inm_list_add_tail(&pgp->entry, &bwip->bit_runs.meta_page_list);
                rem -= nr_chgs;
                if (!rem)
                    break;
            }

            bwip->changes += change_node->changes.change_idx;
            bwip->bit_runs.nbr_runs += change_node->changes.change_idx;
            bwip->nr_bytes_changed_data += change_node->changes.bytes_changes;

            commit_change_node(change_node);
        }
    }

    // if bitmap file is alreadyc opened. We can directly queue work 
    // item to worker queue. if it is not opened lets insert to list and
    // will be processed later.
    if(bwip->changes){
	bwip->eBitmapWorkItem = ecBitmapWorkItemSetBits;
	get_volume_bitmap(vbmap);
	bwip->volume_bitmap = vbmap;
	wqep->context = bwip;
	wqep->witem_type = WITEM_TYPE_BITMAP_WRITE;
	wqep->work_func = bitmap_write_worker_routine;
	INM_SPIN_LOCK_IRQSAVE(&vbmap->lock, lock_flag);
	inm_list_add(&bwip->list_entry, &vbmap->set_bits_work_item_list);
	INM_SPIN_UNLOCK_IRQRESTORE(&vbmap->lock, lock_flag);
	//add wqe to work queue
	add_item_to_work_queue(&driver_ctx->wqueue, wqep);
    }else{
	put_work_queue_entry(wqep);
	put_bitmap_work_item(bwip);
    }

    ret = 0;

if(IS_DBG_ENABLED(inm_verbosity, (INM_IDEBUG | INM_IDEBUG_BMAP))){
    info("leaving ret value true");
}

    return ret;
}

/**
 * FUNCTION NAME: flush_and_close_bitmap_file
 * 
 * DESCRIPTION : Flushes all the changes volume context, and closes it bitmap.
 * 
 * INPUT PARAMETERS : vcptr - ptr to target_context
 *               
 *              
 *              
 * 
 * OUTPUT PARAMETERS :
 * NOTES
 * 
 * return value :
 *     
 * 
 **/
void flush_and_close_bitmap_file(target_context_t *vcptr)
{
    volume_bitmap_t *vbmap = NULL;

if(IS_DBG_ENABLED(inm_verbosity, (INM_IDEBUG | INM_IDEBUG_BMAP))){
    info("entered");
}

    if (!vcptr) {
        info("null vcptr");
        return;
    }

    inmage_flt_save_all_changes(vcptr, TRUE, INM_NO_OP);
    volume_lock(vcptr);
    vbmap = vcptr->tc_bp->volume_bitmap;
    vcptr->tc_bp->volume_bitmap = NULL;
    volume_unlock(vcptr);

    if (vbmap)
        close_bitmap_file(vbmap, FALSE);

if(IS_DBG_ENABLED(inm_verbosity, (INM_IDEBUG | INM_IDEBUG_BMAP))){
    info("leaving");
}

    return;
}
/**
 * FUNCTION NAME:
 * 
 * DESCRIPTION :
 * 
 * INPUT PARAMETERS : 
 *               
 *              
 *              
 * 
 * OUTPUT PARAMETERS :
 * NOTES
 * 
 * return value :    0      - for success
 *             EINVAL - for invalid inputs
 <non-zero value> - for failure
 * 
 **/

int
inmage_flt_save_all_changes(target_context_t *vcptr, inm_s32_t wait_required,
                            inm_s32_t op_type)
{
    struct inm_list_head lh;
    wqentry_t *wqe = NULL;

if(IS_DBG_ENABLED(inm_verbosity, (INM_IDEBUG | INM_IDEBUG_BMAP))){
    info("entered");
}

    if (!vcptr)
        return -EINVAL;

    if (vcptr->tc_flags & VCF_BITMAP_WRITE_DISABLED)
        return 0;

    /* TODOs: VOLUME is offline here .... needs to be handled */

    /* TODOs: processing */

    INM_INIT_LIST_HEAD(&lh);
    volume_lock(vcptr);

    // device is being shutdown soon, write all pending changes to bitmap

    if (!inm_list_empty(&vcptr->tc_node_head)) {
        inm_s32_t wi = WITEM_TYPE_UNINITIALIZED;
        inm_s32_t open_bmap = FALSE;

        switch (op_type) {
        case INM_NO_OP:
        case INM_STOP_FILTERING:
        case INM_SYSTEM_SHUTDOWN:
        case INM_UNSTACK:
            wi = WITEM_TYPE_BITMAP_WRITE;
            break;

        default:
            dbg("unknown operation type\n");
            break;
	}

	if (wi != WITEM_TYPE_UNINITIALIZED)
	    add_vc_workitem_to_list(wi, vcptr, 0, open_bmap, &lh);
    }
    volume_unlock(vcptr);

    if(!inm_list_empty(&lh)) {
        struct inm_list_head *ptr = NULL, *nextptr = NULL;

        inm_list_for_each_safe(ptr, nextptr, &lh) {
            wqe = inm_list_entry(ptr, wqentry_t, list_entry);
            inm_list_del(&wqe->list_entry);
            process_vcontext_work_items(wqe);
            put_work_queue_entry(wqe);
        }
    }

    if (wait_required)
        wait_for_all_writes_to_complete(vcptr->tc_bp->volume_bitmap);

if(IS_DBG_ENABLED(inm_verbosity, (INM_IDEBUG | INM_IDEBUG_BMAP))){
    info("leaving");
}

    return 0;
}

/**
 * FUNCTION NAME:
 * 
 * DESCRIPTION :
 * 
 * INPUT PARAMETERS : 
 *               
 *              
 *              
 * 
 * OUTPUT PARAMETERS :
 * NOTES
 * 
 * return value :    0      - for success
 *             EINVAL - for invalid inputs
 <non-zero value> - for failure
 * 
 **/
void log_bitmap_open_success_event(target_context_t *vcptr)
{
    //char buffer_for_retries[20]= {0};
    //char buffer_for_seconds[20] = {0};
    //inm_s32_t status = 0;

if(IS_DBG_ENABLED(inm_verbosity, (INM_IDEBUG | INM_IDEBUG_BMAP))){
    info("entered");
}

    /* TODOs: time related issues */

    if (!vcptr || vcptr->tc_bp->num_bitmap_open_errors)
        return;

    dbg("TODOs: This functionality yet to implement \n");

if(IS_DBG_ENABLED(inm_verbosity, (INM_IDEBUG | INM_IDEBUG_BMAP))){
    info("leaving");
}

    return;
}

/* move raw bitmap mode to bitmap mode 
 * return value +ve for last chance changes that can fit into the bmap hdr
 * -ve for failure
 * 0 for success
 */

int
move_rawbitmap_to_bmap(target_context_t *vcptr, inm_s32_t force)
{
    bitmap_api_t *bapi = NULL;
    inm_s32_t _rc = -1;
    inm_s32_t status;

    if (driver_ctx->sys_shutdown)
        return -1;

    INM_BUG_ON(!vcptr->tc_bp->volume_bitmap->bitmap_api);
    bapi = vcptr->tc_bp->volume_bitmap->bitmap_api;
    //check if we aleady loaded the header into memory
    // if loaded , flush it
    _rc = bitmap_api_open_bitmap_stream(bapi, vcptr, &status);
    dbg("bmap(%s) open trial one status = %d\n", bapi->bitmap_filename, _rc);
    //TODOs: chk for status values 
    if (!_rc)
        goto success;

    if (bapi->bitmap_header.un.header.header_size) {
        inm_s32_t max_nr_lcwrites = 31*64;
        inm_s32_t rem = max_nr_lcwrites - vcptr->tc_pending_changes;

        if (rem < 0) {
            bapi->bitmap_header.un.header.changes_lost++;
            //set resync required flag
        }
    } else {
        // bitmap has not opened yet
        if (force && vcptr->tc_pending_changes > 0) {
            //is service shutdown??
            //create bitmap file in root, since it is a raw volume

            /* this should never happen for file stored on root volume */
            if (vcptr->tc_bp->bitmap_file_name[0] == '/')
                return _rc;

            _rc = bitmap_api_open_bitmap_stream(bapi, vcptr, &status);
            if (_rc) {
                info("bitmap_api_open_bitmap_stream failed = %d\n", _rc);
                // if this is the case set resync_required 
            }
        }
    }

success:
    if (_rc == 0) { //successfully opened the bitmap file
        inm_s32_t vol_in_sync = TRUE;
        inm_s32_t ret = 0;

        INM_BUG_ON(!bapi->fs);
        if (bapi->fs) {
            vcptr->tc_bp->volume_bitmap->eVBitmapState = ecVBitmapStateOpened;
            set_tgt_ctxt_wostate(vcptr, ecWriteOrderStateBitmap, FALSE,
                                     ecWOSChangeReasonUnInitialized);
        }
        bapi->bitmap_file_state = BITMAP_FILE_STATE_OPENED;

        /* bmap hdr is freshly loaded (first time) */
        ret = is_volume_in_sync(bapi, &vol_in_sync, &status);
        if (vol_in_sync == FALSE) {
	    /* volume is not in sync */
            set_volume_out_of_sync(vcptr, status, status);

            /* TODOs: cleanup data files from previous boot*/
            /* CDataFile::DeleteDataFilesInDirectory() */
            bapi->volume_insync = TRUE;
        }

        /* unset ignore bitmap creation flag. */
        volume_lock(vcptr);
        vcptr->tc_flags &= ~VCF_IGNORE_BITMAP_CREATION;
        volume_unlock(vcptr);
    }
    return _rc;
}

