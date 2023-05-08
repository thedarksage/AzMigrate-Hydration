/* (C)Copy Right Information */

#include "involflt.h"
#include "involflt-common.h"
#include "data-mode.h"
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
#include <linux/list.h>
#include "involflt_debug.h" 
#include "driver-context.h"


void print_driver_context(driver_context_t *dc) {
    struct inm_list_head *iter = NULL;
    target_context_t *tc_entry = NULL;
    data_page_t *entry = NULL;

    if (dc) {
	// * TODOs: Displaying device information 
	print_dbginfo("Device information \n");
	print_dbginfo("Service State = %d \n", dc->service_state);
	print_dbginfo("# of protected volumes = %d \n", dc->total_prot_volumes);

	// *  printing all the target contexts in driver context

	for(iter = dc->tgt_list.next; iter != &(dc->tgt_list);
	    iter = iter->next) {
	    tc_entry = inm_list_entry(iter, target_context_t, tc_list);
	    print_target_context(tc_entry);
	}

	print_dbginfo("flt dev info\n");
	print_dbginfo("flt cdev info\n");
	print_dbginfo("Module owner name  = %s\n", dc->flt_cdev.owner->name);
/*
  print_dbginfo("Module version     = %s\n", dc->flt_cdev.owner->version);
  print_dbginfo("Module src version = %s\n",
  dc->flt_cdev.owner->src_version);
  print_dbginfo("Module arguments   = %s\n", dc->flt_cdev.owner->src_version);
*/


	// TODOs: Displaying the data filtering context
	print_dbginfo("pages info \n");
	print_dbginfo("pages allocated    = %d\n",
		      dc->data_flt_ctx.pages_allocated);
	print_dbginfo("pages free    = %d\n",
		      dc->data_flt_ctx.pages_free);
    
	for (iter = dc->data_flt_ctx.data_pages_head.next;
	     iter != &(dc->data_flt_ctx.data_pages_head);) {
	    entry = inm_list_entry (iter, data_page_t,     next);
	    iter = iter->next;
	    print_dbginfo("page address     = 0x%p\n", entry);
	}

    }
}


void print_target_context(target_context_t *tgt_ctxt)
{
    print_dbginfo("entered \n");
    print_dbginfo("--------------------------------------------");
    if (!tgt_ctxt) { 
        print_dbginfo("\nInvalid target context\n");
        return;
    }
    print_dbginfo("\nTarget Information \n");
    print_dbginfo("TARGET           = %p\n", tgt_ctxt);
    //print_dbginfo("volume id #      = %s\n", tgt_ctxt->uuid);
    print_dbginfo("Major            = %d\n"
		  "Minor            = %d\n", MAJOR(inm_dev_id_get(tgt_ctxt)),
		  MINOR(inm_dev_id_get(tgt_ctxt)));
    print_dbginfo("DM device specific information \n");
    //print_dm_device(tgt_ctxt->dev);
    //print_dbginfo("Next Target name = %s\n", tgt_ctxt->next_tgt);

    if ( tgt_ctxt->tc_flags & VCF_FILTERING_STOPPED )
        print_dbginfo("Filtering status= STOPPED\n");
       
    if (tgt_ctxt->tc_flags & VCF_READ_ONLY)
        print_dbginfo("Filtering status= READ ONLY\n");
 
    print_dbginfo("Filtering mode   = ");
    switch (tgt_ctxt->tc_cur_mode) {
    case FLT_MODE_DATA:
        print_dbginfo("DATA MODE(%x)\n",(int32_t)tgt_ctxt->tc_cur_mode);
        break;

    case FLT_MODE_METADATA:
        print_dbginfo("META DATA MODE(%x)\n", (int32_t)tgt_ctxt->tc_cur_mode);
        break;

    default:
        print_dbginfo("UNKNOWN (%x)\n", (int32_t)tgt_ctxt->tc_cur_mode);
        break;
    }

//    print_dbginfo("Reference count \t = %d\n", (int32_t)tgt_ctxt->tc_ref_cnt);
    print_dbginfo("Pending changes  = %d\n", (int32_t)tgt_ctxt->tc_pending_changes);
    print_dbginfo("pending changes (bytes) = %d\n",
		  (int32_t)tgt_ctxt->tc_bytes_pending_changes);
    print_dbginfo("Transaction id   = %d\n", (int32_t)tgt_ctxt->tc_transaction_id);

    if (tgt_ctxt->tc_cur_node) {
        print_dbginfo("Current change node information \n");
        print_change_node(tgt_ctxt->tc_cur_node);
    }

    if ( tgt_ctxt->tc_pending_confirm) {
        print_dbginfo("Pending Transaction/dirty blk information \n\n");
        print_change_node(tgt_ctxt->tc_pending_confirm);
    }

    print_dbginfo("STATISTICS\n");
    print_dbginfo("# of malloc fails = %u\n",
		  tgt_ctxt->tc_stats.num_malloc_fails);
}

void print_change_node(change_node_t *change_node)
{
    struct inm_list_head *ptr;
    struct inm_list_head *head = &change_node->data_pg_head;
    data_page_t *entry;
    inm_s32_t count = 0;
  
    if (!change_node)
	print_dbginfo("Invalid change node \n");
    else {
	print_dbginfo("NODE             = ");
	switch (change_node->type) {
	case NODE_SRC_DATA:
	    print_dbginfo("DATA\n");
	    break;
	case NODE_SRC_METADATA:
	    print_dbginfo("META DATA\n");
        /* fall through */
	case NODE_SRC_TAGS:
	    print_dbginfo("TAGS\n");
	    break;
	case NODE_SRC_DATAFILE:
	    print_dbginfo("DATA FILE\n");
	    break;
	default:
	    print_dbginfo("UNKNOWN\n");
	}

	print_dbginfo("transaction id   = %d\n", (int32_t)change_node->transaction_id);
	print_dbginfo("mapped address   = %x\n", (int32_t)change_node->mapped_address);

    
	print_disk_change_head(&change_node->changes);

	if (head) {
	    print_dbginfo("pages info \n");
	    count = 0;
	    for (ptr = head; ptr != head;) {
		entry = inm_list_entry (ptr, data_page_t, next);
		ptr = ptr->next;
		print_dbginfo("page address     = 0x%p\n", entry);
		count++;
	    }
	}
	print_dbginfo("total # pages    = %d\n", count);
    }
}

void print_disk_change_head(disk_chg_head_t *disk_change_hd)
{
    inm_s32_t _index = 0;
    if (!disk_change_hd)
	print_dbginfo("Invalid disk changes \n");
    else {
	print_dbginfo("disk chang head  = %p\n", disk_change_hd);
	print_dbginfo("First time stamp = %lld\n",
		      disk_change_hd->start_ts.TimeInHundNanoSecondsFromJan1601);
	print_dbginfo("Laste time stamp = %lld\n",
		      disk_change_hd->end_ts.TimeInHundNanoSecondsFromJan1601);

	print_dbginfo("# changes (bytes)= %d \n",
		      (int32_t)disk_change_hd->bytes_changes);
    
	if (!disk_change_hd->change_idx) {
	    print_dbginfo("Empty\n");
	}
    
	print_dbginfo("DISK CHANGE INFORMATION\n");
	print_dbginfo("# of disk changes = %d \n", disk_change_hd->change_idx);
	for (_index = 0; _index < disk_change_hd->change_idx
		 && _index < MAX_CHANGE_INFOS_PER_PAGE-1; _index++) {
	    print_dbginfo("DISK CHANGE #    = %d\n", _index);
	    print_disk_change((disk_chg_t *) (&disk_change_hd->cur_md_pgp)[_index]);
	    /* TODOs: Displaying the page addresses */
	    /* if one wants to display the page addresses then need to have 
	     * global buff list access ... i.e. global context info
	     * buf_idx is the index where the change starts from
	     **/
	}
    }
}




void print_disk_change(disk_chg_t *disk_change) {
    if (disk_change) {
      
	print_dbginfo("offset           = %x\n", (int32_t)disk_change->offset);
	print_dbginfo("length           = %d\n", (int32_t)disk_change->length);

    } else
	print_dbginfo("Invalid disk change\n");
}

/*void print_dm_device(struct dm_dev *dev) {
  if (dev) {
  print_dbginfo("Device name      = %s\n", dev->name);
  print_dbginfo("Mode             = %d\n", dev->mode);
  } else
  print_dbginfo("Invalid device object\n");
  }*/

void print_bio(struct bio * _bio) {
    if (!_bio) {
        print_dbginfo("OFFSET           = %lld\n", (long long)INM_BUF_SECTOR(_bio));
        print_dbginfo("LEN              = %x\n", INM_BUF_COUNT(_bio));
    }
}
void print_dm_bio_info(dm_bio_info_t *dm_bio_info) {
    if (dm_bio_info){
        print_dbginfo("DM BIO INFO\n");
        print_dbginfo("SECTOR           = %x\n",
		      (int32_t)dm_bio_info->bi_sector);
        print_dbginfo("SIZE             = %d\n",
		      (int32_t)dm_bio_info->bi_size);
        print_dbginfo("INDEX            = %x\n",
		      (int32_t)dm_bio_info->bi_idx);
        print_dbginfo("FLAGS            = %x\n",
		      (int32_t)dm_bio_info->bi_flags);
    } else 
	print_dbginfo("Invalid dm_bio_info variable\n");
}
