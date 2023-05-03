#include "involflt.h"
#include "involflt-common.h"
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
#include "db_routines.h"
#include "data-mode.h"
#include "driver-context.h"
#include "metadata-mode.h"
#include "statechange.h"
#include "driver-context.h"
#include "filter.h"
#include "filter_lun.h"
#include "sysfs.h"
#include "tunable_params.h"
#include <linux/uio.h>
#include <linux/ctype.h>
#include <linux/seq_file.h>
#include <asm/kmap_types.h>
#include "file-io.h"
#include "target-context.h"


extern driver_context_t *driver_ctx;
extern inm_s32_t data_file_mode;
int inm_exec_write(emd_io_t *io);
int inm_exec_vacp_write(emd_handle_t handle,  char * vacp_tag_data, loff_t data_len);
void inm_exec_pt_write_cancel(emd_dev_t *handle, inm_u64_t lba_start, 
				inm_u64_t io_len);

#define CMDDT 0x02
#define EVPD    0x01


#define INMAGE_EMD_VENDOR "InMage  " /* Vendor ID Len should be minimum of 8 bytes */
#define INMAGE_EMD_REV "0001"

#define READ_CAP_LEN 8
#define READ_CAP16_LEN 12

/*
 * Since we can't control backstorage device's reordering, we have to always
 * report unrestricted reordering.
 */
#define DEF_TST			SCST_CONTR_MODE_SEP_TASK_SETS
#define DEF_QUEUE_ALG		SCST_CONTR_MODE_QUEUE_ALG_UNRESTRICTED_REORDER
#define DEF_SWP			0
#define DEF_TAS			0

void update_cur_dat_pg(change_node_t *, data_page_t *, int);
data_page_t *get_cur_data_pg(change_node_t *node, inm_s32_t *offset);

int inm_prepare_write(emd_handle_t handle, inm_u64_t lba_start, inm_u64_t bufflen)
{
	target_context_t *vcptr = (target_context_t *)handle;
	target_volume_ctx_t *tvcptr = (target_volume_ctx_t *)vcptr->tc_priv;
	loff_t loff;

	INM_BUG_ON(lba_start < (vcptr->tc_dev_startoff/tvcptr->bsize));
	lba_start -= (vcptr->tc_dev_startoff/tvcptr->bsize);
        loff = (loff_t)(lba_start * tvcptr->bsize);

	if (INM_UNLIKELY(loff < 0) || INM_UNLIKELY(bufflen < 0) ||
	    INM_UNLIKELY((loff + bufflen) > (tvcptr->nblocks * tvcptr->bsize)))
	{
		printk(KERN_ALERT "%s: Access beyond the end of the device "
			  "(%lld of %lld, len %Ld)\n", vcptr?vcptr->tc_guid:" ", (inm_u64_t)loff,
			  (inm_u64_t)(tvcptr->nblocks * tvcptr->bsize),
			  (inm_u64_t)bufflen);
		queue_worker_routine_for_set_volume_out_of_sync(vcptr,
			  ERROR_TO_REG_OUT_OF_BOUND_IO, -EIO);

		return -EIO;
        }
	return 0;
}


inm_get_capacity(emd_handle_t handle, emd_dev_cap_t *cap)
{
	target_context_t *vcptr = (target_context_t *)handle;
	target_volume_ctx_t *tvcptr = (target_volume_ctx_t *)vcptr->tc_priv;

	cap->nblocks = tvcptr->nblocks;
	cap->bsize = tvcptr->bsize;
	cap->startoff = vcptr->tc_dev_startoff;
}

const char* inm_get_AT_path(emd_handle_t handle, int *flags)
{
	target_context_t *vcptr = (target_context_t *)handle;
	target_volume_ctx_t *tvcptr = (target_volume_ctx_t *)vcptr->tc_priv;

	return tvcptr->pt_guid;
}


target_context_t *get_tgt_ctxt_from_uuid_nowait_fabric(char *uuid)
{
    struct inm_list_head *ptr;
    target_context_t *tgt_ctxt = NULL;

    INM_DOWN_READ(&(driver_ctx->tgt_list_sem));
    for(ptr = driver_ctx->tgt_list.next; ptr != &(driver_ctx->tgt_list);
       ptr = ptr->next, tgt_ctxt = NULL) {
        tgt_ctxt = inm_list_entry(ptr, target_context_t, tc_list);
        if (tgt_ctxt->tc_dev_type == FILTER_DEV_FABRIC_LUN) {
           if (strcmp(tgt_ctxt->tc_guid, uuid) == 0) {
                get_tgt_ctxt(tgt_ctxt);
                break;
           }
        }
    }

    INM_UP_READ(&(driver_ctx->tgt_list_sem));

    return tgt_ctxt;
}

void
inm_detach(emd_handle_t handle)
{

	target_context_t *vcptr = (target_context_t *)handle;
	target_volume_ctx_t *tvcptr = (target_volume_ctx_t *)vcptr->tc_priv;

	if (INM_ATOMIC_READ(&tvcptr->remote_volume_refcnt) != 0)
	{
		printk(KERN_ALERT "Vdev %p is being detached with non-zero (%d) remote volume references\n", vcptr, INM_ATOMIC_READ(&tvcptr->remote_volume_refcnt));
	}

	put_tgt_ctxt(vcptr);
	
	return;
}

emd_handle_t
inm_attach(char *dev_name)
{
    target_context_t *vcptr = NULL;

    printk(KERN_ALERT "inm_attach(): name = %s \n", dev_name);
    

    /* This is the only non-blocking function for target context lookup. */
    vcptr = get_tgt_ctxt_from_uuid_nowait_fabric((char *)dev_name);
    
    if (vcptr == NULL)
    {
        printk(KERN_ALERT "In inm_attach Device %s not found \n", dev_name);
        goto out;
    }

    printk(KERN_ALERT "inm_attach() end: vcptr = %p \n", vcptr);


    out:

        return (emd_handle_t)vcptr;
}

struct emd_dev_type  inm_dev_type = {         \
	get_capacity : inm_get_capacity,      \
	get_path : inm_get_AT_path,           \
	prepare_write: inm_prepare_write,     \
	attach : inm_attach,                  \
	detach : inm_detach,                  \
	exec_write: inm_exec_write,           \
	exec_vacp_write: inm_exec_vacp_write, \
	exec_io_cancel: inm_exec_pt_write_cancel, \
};


inm_s32_t 
__init register_filter_target()
{
	return emd_register_virtual_dev_driver(&inm_dev_type);
    
}

inm_s32_t 
__exit unregister_filter_target()
{
	emd_unregister_virtual_dev_driver(&inm_dev_type);
	return 0;
}


target_volume_ctx_t *
alloc_target_volume_context(void)
{
    target_volume_ctx_t *tvcptr;
    
    tvcptr = (target_volume_ctx_t*)INM_KMALLOC(sizeof(*tvcptr), INM_KM_SLEEP, INM_KERNEL_HEAP);
    if (IS_ERR(tvcptr)) {
        return tvcptr;
    }

    INM_MEM_ZERO(tvcptr, sizeof(*tvcptr));
    INM_ATOMIC_SET(&tvcptr->remote_volume_refcnt, 0);
    INM_INIT_LIST_HEAD(&tvcptr->init_list);
    tvcptr->flags |= TARGET_VOLUME_DIRECT_IO;  // Bydefault we set DIRECT.
    return tvcptr;
}

static void
update_initiator_timestamp(target_volume_ctx_t* tvcptr,
                 char *iname)
{
    struct inm_list_head *curp = NULL;
    struct inm_list_head *nxtp = NULL;
    initiator_node_t *initp = NULL;
    int found = 0;

    /* Sunil:
     * Look up "cmd->sess->initiator_wwpn" (could be pwwn or iSCSI iqn name) in the
     * list associated with this replication pair i.e. given target-context. If this
     * pwwn entry is present in the list then update its timestamp, else insert this
     * pwwn entry in the list and set its timestamp.
     * As an optimization, we could check if this entry is among the first 10/20 entries
     * in the list. If no, we could move this entry towards the start of the list.
     * This would help in doing faster look-ups assuming that same node would do further
     * IOs at that time-instance.
     *
     * Free "tvcptr->init_list" when destroying target-context => destruction of target_volume_ctx_t
     * Need to do "INM_INIT_LIST_HEAD" on tvcptr->init_list => creation of target_volume_ctx_t
     */

    inm_list_for_each_safe(curp, nxtp, &tvcptr->init_list) {
        initp = inm_list_entry(curp, initiator_node_t, init_list);
        if ((strlen(iname) == strlen(initp->initiator_wwpn)) &&
            strcmp(iname, initp->initiator_wwpn) == 0) {
                found = 1;
                get_time_stamp(&(initp->timestamp));
                break;
        }
    }

    if (found == 0) {
        // Insert this initiator into the list
        initiator_node_t *initp = INM_KMALLOC(sizeof(initiator_node_t), INM_KM_NOSLEEP, INM_KERNEL_HEAP); // This is WRITE operation and we are in interrupt/atomic ctxt.
        if (!initp) {
            info("initiator node is null");
            return;
        }
        initp->initiator_wwpn = INM_KMALLOC(strlen(iname) + 1, INM_KM_NOSLEEP, INM_KERNEL_HEAP);
        if (!initp->initiator_wwpn) {
            info("initiator node wwpn is null");
            return;
        }

        strncpy_s(initp->initiator_wwpn, strlen(iname) + 1, iname, strlen(iname));

        initp->initiator_wwpn[strlen(iname)] = '\0';
        get_time_stamp(&(initp->timestamp));
        printk(KERN_ERR "First Insertion: Last Host IO TimeStamp: Initiator Name/wwpn = %s, TimeStamp = %lld, Lun = %s\n",
                tvcptr->initiator_name, initp->timestamp, tvcptr->vcptr->tc_guid);

        inm_list_add_tail(&initp->init_list, &tvcptr->init_list);
    }

    return;
}

void inm_heartbeat(target_context_t *tgt_ctxt)
{
        target_volume_ctx_t *tvcptr  = (target_volume_ctx_t *)tgt_ctxt->tc_priv;

        /* Record latest timestamp of the initiator */
        volume_lock(tgt_ctxt);
//        update_initiator_timestamp(tvcptr, cmd);
        volume_unlock(tgt_ctxt);

	return;
}

int inm_exec_vacp_write(emd_handle_t handle,  char * vacp_tag_data, loff_t data_len)
{
	target_context_t *vcptr = (target_context_t *)handle;
	tag_info_t *tag_info = NULL;
	inm_s32_t num_tags = 0;
	tag_volinfo_t tag_volinfop;


	if (vcptr == NULL || vcptr->tc_flags & VCF_FILTERING_STOPPED)
	{
		printk(KERN_ALERT "emd_exec_vacp_write: FILTERING_STOPPED\n");
		goto done;
	}

	if (vcptr->tc_cur_wostate != ecWriteOrderStateData) {
		dbg("Volume is not in Write Order State Data");
		INM_ATOMIC_INC(&(vcptr->tc_stats.num_tags_dropped));
		goto done;
	}

	num_tags = data_len/sizeof(tag_info_t);
	tag_info = cnvt_stream2tag_info((tag_info_t *)vacp_tag_data, num_tags);
	if (!tag_info){
		err("allocation failed for tag_info");
		goto done;
	}
	tag_volinfop.ctxt = vcptr;
	add_tags(&tag_volinfop, tag_info, num_tags, NULL, 0);
	dbg("emd_exec_vacp_write: data_len: %lld\n", data_len);

done:
	return 0;
}


int inm_exec_write(emd_io_t *io)
{
	target_context_t *vcptr = (target_context_t *)io->eio_dev_handle;
	target_volume_ctx_t *tvcptr  = (target_volume_ctx_t *)vcptr->tc_priv;
	loff_t loff, data_len;
	inm_s32_t iv_count = 0;
	write_metadata_t wmd;
	inm_wdata_t wdata = {0};
	target_context_t *tgt_ctxt = vcptr;
	inm_u32_t idx = 0;
	inm_u64_t logical_lba_start = io->eio_start;

	if(vcptr){
		INM_BUG_ON(logical_lba_start < (vcptr->tc_dev_startoff/tvcptr->bsize));
		logical_lba_start -= (vcptr->tc_dev_startoff/tvcptr->bsize);
	}
	loff = (loff_t)(logical_lba_start * tvcptr->bsize);
	data_len = io->eio_len;

	/* Track the write the initiator has issued */
	volume_lock(tgt_ctxt);
	update_initiator(tvcptr, io->eio_iname);
	update_initiator_timestamp(tvcptr, io->eio_iname);
	volume_unlock(tgt_ctxt);

	if (vcptr == NULL || vcptr->tc_flags & VCF_FILTERING_STOPPED)
	{
		//TODO: Hari: need to put the involflt related debug log.
		printk("(FILTERING_STOPPED) WRITE:  lba_start %Ld, loff %Ld, data_len %Ld",logical_lba_start, (inm_u64_t)loff, (inm_u64_t)data_len); 
		goto done;
	}

	//TODO: Handle ORDERED writes

	wmd.offset = (inm_u64_t)(logical_lba_start * tvcptr->bsize);
	wmd.length = data_len;


	idx = inm_comp_io_bkt_idx(data_len);
	INM_ATOMIC_INC(&tgt_ctxt->tc_stats.io_pat_writes[idx]);

	wdata.wd_iovp = io->eio_iovp;
	wdata.wd_iovcnt = io->eio_iovcnt;
	wdata.wd_cplen = wmd.length;
	wdata.wd_copy_wd_to_datapgs = copy_iovec_data_to_data_pages;
	wdata.wd_chg_node = NULL;
	wdata.wd_meta_page = NULL;

	INM_BUG_ON_TMP(tgt_ctxt);
	volume_lock(tgt_ctxt);
	tgt_ctxt->tc_stats.tc_write_io_rcvd_bytes += data_len;
	tgt_ctxt->tc_stats.tc_write_io_rcvd++;
	

        if(!(tgt_ctxt->tc_flags & VCF_FILTERING_STOPPED)) {
        //TODO: gathering IO statistics

        switch(tgt_ctxt->tc_cur_mode) {
        case FLT_MODE_DATA:
		save_data_in_data_mode(vcptr, &wmd, &wdata);
		/* save data function failed here */

		break;
    
        default:
		if (save_data_in_metadata_mode(vcptr, &wmd, &wdata)) {
		/* saving meta data in memory has failed here*/
			if(IS_DBG_ENABLED(inm_verbosity, (INM_IDEBUG | INM_IDEBUG_META))){
				info("Save meta data function failed:...\n");
				info("TO DO list exist\n");
			}
		}
		break;

        }

        /* TODOs: wake up service thread, once the target state crosses
         * the specified threshold levels */

	if (!tgt_ctxt->tc_bp->volume_bitmap &&
	    !(tgt_ctxt->tc_flags & VCF_OPEN_BITMAP_REQUESTED) &&
	    can_open_bitmap_file(tgt_ctxt, FALSE)) {
	    // TODOs: Checking for volume online for cluster volumes
	    request_service_thread_to_open_bitmap(tgt_ctxt);
	}
    }
    volume_unlock(tgt_ctxt);
    if(should_wakeup_s2(tgt_ctxt))
        INM_WAKEUP_INTERRUPTIBLE(&tgt_ctxt->tc_waitq);


done:        
        return 0;
}

void inm_exec_pt_write_cancel(emd_dev_t *handle, inm_u64_t lba_start, 
				inm_u64_t io_len)
{
	target_context_t *vcptr = (target_context_t *)handle;
	target_volume_ctx_t *tvcptr  = (target_volume_ctx_t *)vcptr->tc_priv;
	target_context_t *tgt_ctxt = vcptr;
	loff_t loff;
	inm_u64_t data_len;
	flt_mode cur_mode;
	write_metadata_t wmd;
	inm_u64_t logical_lba_start = lba_start;

	logical_lba_start -= (vcptr->tc_dev_startoff / tvcptr->bsize);
	loff = (loff_t)(logical_lba_start * tvcptr->bsize);
	data_len = (io_len * tvcptr->bsize);


        if (vcptr == NULL || vcptr->tc_flags & VCF_FILTERING_STOPPED){
                printk("(FILTERING_STOPPED) WRITE:  lba_start %Ld, loff %Ld, data_len %Ld",logical_lba_start, (inm_u64_t)loff, (inm_u64_t)data_len);
                goto done;
        }

	printk("IO Cancel CDB:  lba_start %Ld, loff %Ld, data_len %Ld for %s",logical_lba_start, (inm_u64_t)loff, (inm_u64_t)data_len, vcptr->tc_guid);

	wmd.offset = (inm_u64_t)(logical_lba_start * tvcptr->bsize);
	wmd.length = data_len;

	INM_ATOMIC_INC(&tgt_ctxt->tc_stats.tc_write_cancel);

	volume_lock(tgt_ctxt);
	tgt_ctxt->tc_stats.tc_write_cancel_rcvd_bytes += data_len;
	cur_mode = tgt_ctxt->tc_cur_mode;
	set_tgt_ctxt_filtering_mode(tgt_ctxt, FLT_MODE_METADATA, FALSE);
        if(ecWriteOrderStateData == tgt_ctxt->tc_cur_wostate)
            set_tgt_ctxt_wostate(tgt_ctxt, ecWriteOrderStateMetadata, FALSE,
                                 ecWOSChangeReasonUnInitialized);

	if(!(tgt_ctxt->tc_flags & VCF_FILTERING_STOPPED)) {
		if (save_data_in_metadata_mode(vcptr, &wmd, NULL)) {
			/* saving meta data in memory has failed here*/
			if(IS_DBG_ENABLED(inm_verbosity, (INM_IDEBUG | INM_IDEBUG_META))){
				info("Save meta data function failed:...\n");
				info("TO DO list exist\n");
			}
		}
	}

	set_tgt_ctxt_filtering_mode(tgt_ctxt, cur_mode, FALSE);

	if (!tgt_ctxt->tc_bp->volume_bitmap &&
		!(tgt_ctxt->tc_flags & VCF_OPEN_BITMAP_REQUESTED) &&
		can_open_bitmap_file(tgt_ctxt, FALSE)) {
		// TODOs: Checking for volume online for cluster volumes
		request_service_thread_to_open_bitmap(tgt_ctxt);
	}

	volume_unlock(tgt_ctxt);

	if(should_wakeup_s2(tgt_ctxt))
		INM_WAKEUP_INTERRUPTIBLE(&tgt_ctxt->tc_waitq);

done:
	return;
}

inm_s32_t 
filter_lun_create(char* uuid, inm_u64_t nblks, inm_u32_t bsize, inm_u64_t startoff)
{
    inm_dev_extinfo_t *dev_infop = NULL;
    target_context_t	*ctx;
	int				ret = 0;
    inm_block_device_t  *src_dev;

    if (strlen(uuid) > GUID_SIZE_IN_CHARS) {
        return -EINVAL;
    }

    dev_infop = (inm_dev_extinfo_t *) INM_KMALLOC(sizeof(*dev_infop), INM_KM_SLEEP, INM_KERNEL_HEAP);
    if (!dev_infop) {
        return -ENOMEM;
    }
    INM_MEM_ZERO(dev_infop, sizeof(*dev_infop));
    if (strcpy_s(dev_infop->d_guid, INM_GUID_LEN_MAX, uuid)) {
	kfree(dev_infop);
	return -INM_EFAULT;
    }

    dev_infop->d_type = FILTER_DEV_FABRIC_LUN;
    dev_infop->d_bsize = bsize ? bsize : 512;
    dev_infop->d_nblks = nblks ? nblks : (MEGABYTES * 100 );
    if (strncpy_s(dev_infop->d_src_scsi_id, INM_GUID_LEN_MAX, dev_infop->d_guid, INM_GUID_LEN_MAX)) {
	kfree(dev_infop);
	return -INM_EFAULT;
    }

    dev_infop->d_src_scsi_id[INM_GUID_LEN_MAX-1] = '\0';
    dev_infop->d_startoff = startoff;
    /* Validity checks against bad info from CX */
    src_dev = NULL;
    src_dev = open_by_dev_path(dev_infop->d_guid, 0);
    if (src_dev) {
        close_bdev(src_dev, FMODE_READ);
        ret = INM_EINVAL;
        err("Device:%s incorrectly sent as source device for AT LUN",
            dev_infop->d_guid);
        goto exit;
    }
    ret = do_volume_stacking(dev_infop);
    if (ret) {
        goto exit;
    }

    ctx = get_tgt_ctxt_from_uuid(dev_infop->d_guid);
    if (ctx) {
        inm_s32_t flt_on = FALSE;

        volume_lock(ctx);

        if (is_target_filtering_disabled(ctx)) {
            ctx->tc_flags &= ~VCF_FILTERING_STOPPED;
            ctx->tc_flags |= VCF_IGNORE_BITMAP_CREATION;
            flt_on = TRUE;
        }

	/* Request the service thread to open the bitmap file, by which
	 * the filtering mode & write order state would change and helps
	 * in issuing tags for a disk (belonging to a volume group) with
	 * no I/Os.
	 *
	 * Refer to Bug #15565 for more details.
	 */
	if (!ctx->tc_bp->volume_bitmap &&
	    !(ctx->tc_flags & VCF_OPEN_BITMAP_REQUESTED) &&
	    can_open_bitmap_file(ctx, FALSE) &&
	    (!driver_ctx->sys_shutdown)){
		request_service_thread_to_open_bitmap(ctx);
	}

        volume_unlock(ctx);
        if (flt_on) {
            set_int_vol_attr(ctx, VolumeFilteringDisabled, 0);
        }
	ctx->tc_dev_startoff = startoff;
        put_tgt_ctxt(ctx);
        ret = 0;
    } else {
        ret = -EINVAL;
    }

exit:
    kfree(dev_infop);

    return ret;
}

int
emd_file_create(target_volume_ctx_t *vtgtctx_ptr, char *name)
{
        struct file *fp;
	char file_name[128];
	loff_t loff, err;
	int retval;
	mm_segment_t    fs;
	inm_u8_t	*buf;

	sprintf(file_name, "/var/scst/AT/%s", name);

        fp = filp_open(file_name, O_LARGEFILE | O_RDWR| O_CREAT, 0600);
	if (IS_ERR(fp)) {
		return  -EINVAL;
	}
	loff = vtgtctx_ptr->bsize * (vtgtctx_ptr->nblocks - 1);
        fs = get_fs();
        set_fs(KERNEL_DS);
	if (fp->f_op->llseek)
		err = fp->f_op->llseek(fp, loff, 0/*SEEK_SET*/);
	else
		err = default_llseek(fp, loff, 0/*SEEK_SET*/);
	if (err != loff) {
		printk("lseek trouble %Ld != %Ld",
			(long long unsigned int)err, (long long unsigned int)loff);
		retval = -EINVAL;
		goto out;
	}
	buf = INM_KMALLOC(vtgtctx_ptr->bsize, INM_KM_SLEEP, INM_KERNEL_HEAP);
	if (buf == NULL) {
		retval = -ENOMEM;
		goto out;
	}
	INM_MEM_ZERO(buf, vtgtctx_ptr->bsize);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,7,0)
        retval = vfs_write(fp, buf, vtgtctx_ptr->bsize, &loff);
#else
        retval = fp->f_op->write(fp, buf, vtgtctx_ptr->bsize, &loff);
#endif

        if ((retval <= 0) || (retval != vtgtctx_ptr->bsize)) {
                printk("write failed with error 0x%x", (int32_t)retval);
                retval = -EIO;
		goto free_mem;
        }
	retval = 0;
free_mem:
	kfree(buf);
out:
        set_fs(fs);
	filp_close(fp, NULL);

	return retval;
}


int
fabric_volume_init(target_context_t *ctx, inm_dev_info_t *dev_info)
{
    target_volume_ctx_t *vtgtctx_ptr = ctx->tc_priv;
    inm_s32_t error = 0;
    
    vtgtctx_ptr = alloc_target_volume_context();
    if ( IS_ERR(vtgtctx_ptr) )
    {
        printk(KERN_ALERT "Out of memory, unable to allocate memory for tgt_volume_context_t.\n");
        return -ENOMEM;
    }

    vtgtctx_ptr->bsize = dev_info->d_bsize;
    vtgtctx_ptr->nblocks = dev_info->d_nblks;
    vtgtctx_ptr->vcptr = ctx;
    ctx->tc_priv = vtgtctx_ptr;
    vtgtctx_ptr->virt_id = emd_register_virtual_device(ctx->tc_guid);
    if (vtgtctx_ptr->virt_id <= 0) {
        printk(KERN_ALERT "Unable to register virtual device %s with EMD\n", \
               ctx->tc_guid);
        error = vtgtctx_ptr->virt_id;
        if ( vtgtctx_ptr->virt_id == 0) {
            kfree(vtgtctx_ptr);
            return -EINVAL;
        }
     }
    return 0;
}

int
fabric_volume_deinit(target_context_t *ctx)
{
    target_volume_ctx_t *vtgtctx_ptr = ctx->tc_priv;
    struct inm_list_head *curp = NULL;
    struct inm_list_head *nxtp = NULL;
    initiator_node_t *initp = NULL;

    
    if(vtgtctx_ptr) {
        /* Free vtgtctx_ptr->init_list */
        inm_list_for_each_safe(curp, nxtp, &vtgtctx_ptr->init_list) {
            initp = inm_list_entry(curp, initiator_node_t, init_list);
            if (initp->initiator_wwpn != NULL) {
                INM_KFREE(initp->initiator_wwpn, strlen(initp->initiator_wwpn) + 1, INM_KERNEL_HEAP);
                initp->initiator_wwpn = NULL;
            }
            inm_list_del(&initp->init_list);
            INM_KFREE(initp, sizeof(initiator_node_t), INM_KERNEL_HEAP);
            initp = NULL;
        }

        kfree(vtgtctx_ptr);
        ctx->tc_priv = NULL;
    }
    return 0; 
}

int
filter_lun_delete(char* uuid)
{
    target_context_t *tgtctx_ptr = NULL;
    target_volume_ctx_t *vtgtctx_ptr = NULL;
    char *bitmap_filename = NULL;
    inm_s32_t error = 0;
    inm_block_device_t  *src_dev;


    /* Validity checks against bad info from CX */
    src_dev = NULL;
    src_dev = open_by_dev_path(uuid, 0);
    if (src_dev) {
        close_bdev(src_dev, FMODE_READ);
        error = INM_EINVAL;
        err("Device:%s incorrectly sent as source device for AT LUN error:%d",
            uuid, error);
        return error;
    }

    bitmap_filename = (char *)INM_KMALLOC(NAME_MAX + 1, INM_KM_SLEEP, INM_KERNEL_HEAP);
    if (!bitmap_filename) {
        info("Failed to allocate memory for Virtual Device with name %s\n", uuid);
    }
    printk("Removing Virtual Device %s\n", uuid);

    INM_DOWN_WRITE(&(driver_ctx->tgt_list_sem));
    tgtctx_ptr = get_tgt_ctxt_from_uuid_locked(uuid);
    if ( tgtctx_ptr == NULL )
    {
        INM_UP_WRITE(&(driver_ctx->tgt_list_sem));
        printk("Virtual Device with name %s does not exist\n", uuid);
        if (bitmap_filename) {
            kfree(bitmap_filename);
        }
        return -EINVAL;
    }

    volume_lock(tgtctx_ptr);
    tgtctx_ptr->tc_flags |= VCF_VOLUME_DELETING;
    tgtctx_ptr->tc_filtering_disable_required = 1;
    volume_unlock(tgtctx_ptr);
    INM_UP_WRITE(&driver_ctx->tgt_list_sem);

    vtgtctx_ptr = tgtctx_ptr->tc_priv;
    printk("Unregistering the virtual device %s with virt_id 0x%x\n",
           tgtctx_ptr->tc_guid, vtgtctx_ptr->virt_id);
    emd_unregister_virtual_device(vtgtctx_ptr->virt_id);
     printk("Check leak removed vdev %s vcptr %p \n", tgtctx_ptr->tc_guid, vtgtctx_ptr->vcptr);
    vtgtctx_ptr->vcptr = NULL;
    get_tgt_ctxt(tgtctx_ptr);
    tgt_ctx_force_soft_remove(tgtctx_ptr);
    inm_unlink(tgtctx_ptr->tc_bp->bitmap_file_name,
               tgtctx_ptr->tc_bp->bitmap_dir_name);
    put_tgt_ctxt(tgtctx_ptr);

    return error;
}

int
get_at_lun_last_write_vi(char* uuid, char *initiator_name)
{
	inm_s32_t error = 0;

    target_context_t *tgtctx_ptr = NULL;
    target_volume_ctx_t *vtgtctx_ptr = NULL;

    tgtctx_ptr = get_tgt_ctxt_from_uuid_nowait(uuid);

    if ( tgtctx_ptr == NULL )
    {
        printk(KERN_ALERT "Virtual Device with name %s does not exist\n", uuid);
        error=-EINVAL;
        goto out;
    }
	
    vtgtctx_ptr = tgtctx_ptr->tc_priv;

	volume_lock(tgtctx_ptr);
    if (strncpy_s(initiator_name, MAX_INITIATOR_NAME_LEN, vtgtctx_ptr->initiator_name, MAX_INITIATOR_NAME_LEN - 1)) {
	volume_unlock(tgtctx_ptr);
	put_tgt_ctxt(tgtctx_ptr);
	error = -INM_EFAULT;
	goto out;
    }
	initiator_name[MAX_INITIATOR_NAME_LEN] = '\0';
	volume_unlock(tgtctx_ptr);
    put_tgt_ctxt(tgtctx_ptr); 

out:
	return error;
}

int
get_lun_query_data(inm_u32_t avail_count, inm_u32_t* lun_count, LunData* lun_data_ptr)
{

	struct inm_list_head* ptr;
	target_context_t *tgt_ctx_ptr = NULL;
	target_volume_ctx_t *vtgt_ctx_ptr = NULL;
	LunData lun_data;
	inm_s32_t error = 0;

	printk("\n Availiability Count %u, Lun Count %u\n", avail_count, *lun_count);
	INM_DOWN_READ(&((driver_ctx->tgt_list_sem)));
	for  (ptr = driver_ctx->tgt_list.next; ptr != &(driver_ctx->tgt_list) ;) {
		
		if ( *lun_count < avail_count )
		{
			tgt_ctx_ptr = inm_list_entry(ptr, target_context_t, tc_list);
			if(tgt_ctx_ptr->tc_flags & (VCF_VOLUME_CREATING | VCF_VOLUME_DELETING)){
			    tgt_ctx_ptr = NULL;
			    continue;
			}

			vtgt_ctx_ptr = tgt_ctx_ptr->tc_priv;
			if (strcpy_s(lun_data.uuid, GUID_SIZE_IN_CHARS+1, tgt_ctx_ptr->tc_guid)) {
				error = -INM_EFAULT;
				break;
			}

			/* We create only type of lun as of now */
			lun_data.lun_type = FILTER_DEV_FABRIC_LUN;
	
			printk(" Lun Name : %s\n", lun_data.uuid); 	
			if ( INM_COPYOUT(lun_data_ptr, &lun_data,  sizeof(LunData) )) {
				err("INM_COPYOUT failed ");
				error = -EFAULT;
				break;
			}	
			lun_data_ptr++;
			(*lun_count)++; /* One on One mapping between target context and volume context */
		}
		ptr = ptr->next;
   }
	
   INM_UP_READ(&(driver_ctx->tgt_list_sem));
   return error;	
}

int
register_bypass_target()
{
  inm_s32_t err = 1;
  return err;
}

int
unregister_bypass_target()
{
  inm_s32_t err = 1;
  return err;
}

void
copy_iovec_data_to_data_pages(inm_wdata_t *wdatap, struct inm_list_head *listhdp)
{
    data_page_t *pg;
    char *src,*dst;
    inm_s32_t index;
    inm_s32_t to_copy, iov_rem, pg_rem, pg_offset, iov_off;
    inm_s32_t bytes_res_node;
    enum km_type km;
    change_node_t *node = inm_list_entry(listhdp, change_node_t, next);
    ssize_t total_len = wdatap->wd_cplen;
    struct iovec *iovp = (struct iovec *) wdatap->wd_iovp;

    BUG_ON(!wdatap->wd_iovcnt ||!total_len);
    if (in_softirq())
        km = KM_SOFTIRQ1;
    else
        km = KM_USER1;
    INM_BUG_ON(inm_list_empty(&node->data_pg_head));
    bytes_res_node = node->data_free;
    pg = get_cur_data_pg(node, (inm_s32_t *)&pg_offset);
    dst = (char *)INM_KMAP_ATOMIC(pg->page, km);
    pg_rem = (PAGE_SIZE - pg_offset);

    for (index = 0; index < wdatap->wd_iovcnt; index++) {
        src = (char *)(iovp[index].iov_base);
        iov_off = 0;
        iov_rem = iovp[index].iov_len;

        while (iov_rem) {
        	to_copy = MIN(iov_rem, pg_rem);
	        to_copy = MIN(to_copy, bytes_res_node);
    	    memcpy_s((char *)(dst + pg_offset), to_copy, (char *)(src + iov_off), to_copy);
    	    iov_rem -= to_copy;
        	iov_off += to_copy;
	        pg_rem -= to_copy;
    	    pg_offset += to_copy;
        	total_len -= to_copy;
	        bytes_res_node -= to_copy;
	        BUG_ON(total_len < 0);
    	    BUG_ON(iov_rem < 0);
        	BUG_ON(pg_offset > PAGE_SIZE);

			if (!total_len)
				break;
			if (!pg_rem || !bytes_res_node) {
				INM_KUNMAP_ATOMIC(dst, km);
				if (!bytes_res_node) {
	   				/* update offsets for current page in change node structure */
		   			update_cur_dat_pg(node, pg, pg_offset);
        	        INM_BUG_ON(!(node->flags & KDIRTY_BLOCK_FLAG_SPLIT_CHANGE_MASK));
					INM_BUG_ON(inm_list_empty(&node->data_pg_head));
	        	    /*
	                 * Valid case for split io, node needs to be changed. Use next
    	             * change node from change_node_list if current one is full
					 */
            	    node = inm_list_entry(node->next.next, change_node_t, next);
                	INM_BUG_ON(!node);
	                INM_BUG_ON(!(node->flags & KDIRTY_BLOCK_FLAG_SPLIT_CHANGE_MASK));
    	            /* Reset destination page offset values */
        	        pg = get_cur_data_pg(node, &pg_offset);
            	    pg_rem = (PAGE_SIZE - pg_offset);
                	bytes_res_node = node->data_free;
	    			INM_BUG_ON(inm_list_empty(&node->data_pg_head));
				}
				else {
            		pg = get_next_data_page(pg->next.next, &pg_rem,
											&pg_offset, node);
					INM_BUG_ON(inm_list_empty(&node->data_pg_head));
				}
				INM_BUG_ON(pg == NULL);
				dst = INM_KMAP_ATOMIC(pg->page, km);
			}
        }
        INM_BUG_ON(iov_rem != 0);
		INM_BUG_ON(total_len < 0);
        if (total_len == 0)
            break;
    }
	update_cur_dat_pg(node, pg, pg_offset);
    INM_BUG_ON(inm_list_empty(&node->data_pg_head));
	INM_BUG_ON(total_len);
	INM_KUNMAP_ATOMIC(dst, km);
}

/*
 * inm_validate_fabric_vol()
 * @tcp : target context ptr
 * @dip : device info ptr
 * notes : this is a specific fn, and validates the 
 * 			dev specific attributes
 */
int
inm_validate_fabric_vol(target_context_t *tcp,
			const inm_dev_info_t const *dip)
{
	target_volume_ctx_t *vtgtctxp = tcp->tc_priv;
	inm_s32_t ret = -EINVAL;

    volume_lock(tcp);
	if (!vtgtctxp) {
		goto exit;
	}

	if ((vtgtctxp->bsize != dip->d_bsize) ||
		(vtgtctxp->nblocks != dip->d_nblks)) {
		ret = -EEXIST;
		goto exit;
	} 

	ret = 0;

exit:
	volume_unlock(tcp);

	return ret;
}

int
process_at_lun_create(struct file *filp, void __user *arg)
{
    inm_s32_t error = 0;

    LUN_CREATE_INPUT *lun_data;

    if(!INM_ACCESS_OK(VERIFY_READ, (void __user*)arg, sizeof(LUN_CREATE_INPUT)))
    {
        err(" Read access violation for LUN_CREATE_INPUT");
        return -EFAULT;
    }
    lun_data = (LUN_CREATE_INPUT*) INM_KMALLOC(sizeof(LUN_CREATE_INPUT),
                                           INM_KM_SLEEP, INM_KERNEL_HEAP);

    if (!lun_data)
    {
        err("INM_KMALLOC failed for LUN_CREATE_INPUT");
        return -ENOMEM;
    }

	INM_MEM_ZERO(lun_data, sizeof(LUN_CREATE_INPUT));
    if ( INM_COPYIN(lun_data, arg, sizeof(LUN_CREATE_INPUT)) )
    {
        err("INM_COPYIN failed");
        kfree(lun_data);
        return -EFAULT;
    }


    switch(lun_data->lunType)
    {
        case FILTER_DEV_FABRIC_LUN:
            error = filter_lun_create(lun_data->uuid, lun_data->lunSize,
				      lun_data->blockSize, lun_data->lunStartOff);
            break;

        case FILTER_DEV_FABRIC_VSNAP:
            printk("To be implemented\n");
            break;

        case FILTER_DEV_FABRIC_RESYNC:
            printk("To be implemented\n");
            break;
        default:
            error = -EINVAL;
    }
    kfree(lun_data);
    return error;
}

int
process_at_lun_delete(struct file *filp, void __user *arg)
{
    inm_s32_t error = 0;

    LUN_DELETE_INPUT *lun_data;

    if(!INM_ACCESS_OK(VERIFY_READ, (void __user*)arg, sizeof(LUN_DELETE_INPUT)))
    {
        err(" Read access violation for LUN_DELETE_INPUT");
        return -EFAULT;
    }

    lun_data = (LUN_DELETE_INPUT*) INM_KMALLOC(sizeof(LUN_DELETE_INPUT), INM_KM_SLEEP, INM_KERNEL_HEAP);

    if ( lun_data == NULL )
    {
        err("INM_KMALLOC failed for LUN_DELETE_INPUT");
        return -ENOMEM;
    }

	INM_MEM_ZERO(lun_data, sizeof(LUN_DELETE_INPUT));
    if ( INM_COPYIN(lun_data, arg, sizeof(LUN_DELETE_INPUT)) )
    {
        err("INM_COPYIN failed");
        kfree(lun_data);
        return -EFAULT;
    }

    switch(lun_data->lunType)
    {
        case FILTER_DEV_FABRIC_LUN:
            error = filter_lun_delete(lun_data->uuid);
            break;

        case FILTER_DEV_FABRIC_VSNAP:
            printk("To be implemented\n");
            break;

        case FILTER_DEV_FABRIC_RESYNC:
            printk("To be implemented\n");
            break;

        default:
            error = -EINVAL;
    }

   kfree(lun_data);
   return error;
}

int
process_at_lun_last_write_vi(struct file* filp, void __user *arg)
{
    inm_s32_t error = 0;
    AT_LUN_LAST_WRITE_VI *last_write_vi;

    if ( !INM_ACCESS_OK(VERIFY_READ, (void __user*)arg, sizeof(AT_LUN_LAST_WRITE_VI))) {
        err( " Read Access Violation for AT_LUN_LAST_WRITE_VI");
        return -EFAULT;
    }

    last_write_vi = (AT_LUN_LAST_WRITE_VI*) INM_KMALLOC(sizeof(AT_LUN_LAST_WRITE_VI), INM_KM_SLEEP, INM_KERNEL_HEAP);

    if ( last_write_vi == NULL ) {
        err("INM_KMALLOC failed for AT_LUN_LAST_WRITE_VI");
        return -ENOMEM;
    }

	INM_MEM_ZERO(last_write_vi, sizeof(AT_LUN_LAST_WRITE_VI));
    if ( INM_COPYIN( last_write_vi, arg, sizeof(AT_LUN_LAST_WRITE_VI))) {
        err("INM_COPYIN failed");
        kfree(last_write_vi);
        return -EFAULT;
    }

    get_time_stamp(&(last_write_vi->timestamp));
    error = get_at_lun_last_write_vi( last_write_vi->uuid, last_write_vi->initiator_name);
    if ( INM_COPYOUT(arg, last_write_vi, sizeof(AT_LUN_LAST_WRITE_VI))) {
        err("INM_COPYOUT failed");
        kfree(last_write_vi);
        return -EFAULT;
    }

    kfree(last_write_vi);
    return error;
}


int
process_at_lun_last_host_io_timestamp(struct file* filp, void __user *arg)
{
    inm_s32_t error = 0;
    AT_LUN_LAST_HOST_IO_TIMESTAMP *last_host_io_timestamp = NULL;
    int wwpn_count = 0;

    if (!INM_ACCESS_OK(VERIFY_READ, (void __user*)arg, sizeof(AT_LUN_LAST_HOST_IO_TIMESTAMP))) {
        err( " Read Access Violation for AT_LUN_LAST_HOST_IO_TIMESTAMP");
        return -EFAULT;
    }

    last_host_io_timestamp = (AT_LUN_LAST_HOST_IO_TIMESTAMP*) INM_KMALLOC(sizeof(AT_LUN_LAST_HOST_IO_TIMESTAMP), INM_KM_SLEEP, INM_KERNEL_HEAP);

    if (last_host_io_timestamp == NULL) {
        err("INM_KMALLOC failed for AT_LUN_LAST_HOST_IO_TIMESTAMP");
        return -ENOMEM;
    }

    INM_MEM_ZERO(last_host_io_timestamp, sizeof(AT_LUN_LAST_HOST_IO_TIMESTAMP));
    if (INM_COPYIN(last_host_io_timestamp, arg, sizeof(AT_LUN_LAST_HOST_IO_TIMESTAMP))) {
        err("INM_COPYIN failed");
        kfree(last_host_io_timestamp);
        return -EFAULT;
    }

    wwpn_count = last_host_io_timestamp->wwpn_count;
    kfree(last_host_io_timestamp);

    last_host_io_timestamp = (AT_LUN_LAST_HOST_IO_TIMESTAMP*) INM_KMALLOC(sizeof(AT_LUN_LAST_HOST_IO_TIMESTAMP) +
                                                                (sizeof (WwpnData) * (wwpn_count - 1)), INM_KM_SLEEP, INM_KERNEL_HEAP);
    if (last_host_io_timestamp == NULL) {
        err("INM_KMALLOC failed for AT_LUN_LAST_HOST_IO_TIMESTAMP");
        return -ENOMEM;
    }
    if (INM_COPYIN(last_host_io_timestamp, arg, sizeof(AT_LUN_LAST_HOST_IO_TIMESTAMP) + (sizeof (WwpnData) * (wwpn_count - 1)))) {
        err("INM_COPYIN failed");
        kfree(last_host_io_timestamp);
        return -EFAULT;
    }

    error = get_at_lun_last_host_io_timestamp(last_host_io_timestamp);
    if (INM_COPYOUT(arg, last_host_io_timestamp, sizeof(AT_LUN_LAST_HOST_IO_TIMESTAMP))) {
        err("INM_COPYOUT failed");
        kfree(last_host_io_timestamp);
        return -EFAULT;
    }

    kfree(last_host_io_timestamp);
    return error;
}

int
get_at_lun_last_host_io_timestamp(AT_LUN_LAST_HOST_IO_TIMESTAMP *last_host_io_timestamp)
{
    inm_s32_t error = 0;
    target_context_t *tgtctx_ptr = NULL;
    target_volume_ctx_t *vtgtctx_ptr = NULL;
    unsigned long long last_io_timestamp = 0;
    int found = 0;
    initiator_node_t *initp = NULL;
    struct inm_list_head *curp = NULL;
    struct inm_list_head *nxtp = NULL;
    int i = 0;

    tgtctx_ptr = get_tgt_ctxt_from_uuid_nowait(last_host_io_timestamp->uuid);

    if (tgtctx_ptr == NULL)
    {
        printk(KERN_ALERT "Virtual Device with name %s does not exist\n", last_host_io_timestamp->uuid);
        error=-EINVAL;
        goto out;
    }

    vtgtctx_ptr = tgtctx_ptr->tc_priv;
    volume_lock(tgtctx_ptr);

    inm_list_for_each_safe(curp, nxtp, &vtgtctx_ptr->init_list) {
        initp = inm_list_entry(curp, initiator_node_t, init_list);
        for (i = 0; i < last_host_io_timestamp->wwpn_count; i++) {
            if ((strlen(last_host_io_timestamp->wwpn_data[i].wwpn) == strlen(initp->initiator_wwpn)) &&
                strcmp(last_host_io_timestamp->wwpn_data[i].wwpn, initp->initiator_wwpn) == 0) {
                    if (found == 0) {
                        last_io_timestamp = initp->timestamp;
                        found = 1;
                    }
                    if (found == 1 && last_io_timestamp < initp->timestamp) {
                        last_io_timestamp = initp->timestamp;
                    }
                    break;
            }
        }
    }
    last_host_io_timestamp->timestamp = last_io_timestamp;

    volume_unlock(tgtctx_ptr);
    put_tgt_ctxt(tgtctx_ptr);

out:
    return error;
}


int
process_at_lun_query(struct file* flip, void __user *arg)
{
    inm_s32_t error = 0;

    LUN_QUERY_DATA query_data;
    LUN_QUERY_DATA *query_data_ptr;
    LunData *lun_data_ptr;
    inm_u32_t lun_count = 0;

    if ( !INM_ACCESS_OK(VERIFY_READ, (void __user*)arg, sizeof(LUN_QUERY_DATA))) {
        err( " Read Access Violation for AT_LUN_LAST_WRITE_VI");
        return -EFAULT;
    }
    if ( INM_COPYIN( &query_data, arg, sizeof( LUN_QUERY_DATA ))) {
        err("INM_COPYIN failed");
        return -EFAULT;
    }

    query_data_ptr = (LUN_QUERY_DATA*) arg;
    lun_data_ptr = &(query_data_ptr->lun_data[0]);
    error = get_lun_query_data(query_data.count, &lun_count, lun_data_ptr);

    if ( !error )
    {
        if ( INM_COPYOUT( (void*) &(query_data_ptr->lun_count), &lun_count, sizeof(inm_u32_t) ) )
        {
            err("INM_COPYOUT failed");
            error = -EFAULT;
        }
    }
    return error;
}
