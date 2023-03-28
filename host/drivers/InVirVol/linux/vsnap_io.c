
/*****************************************************************************
 * vsnap_io.c
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
 * Description:         IO handling routines
 *
 * Revision:            $Revision: 1.17 $
 * Checked in by:       $Author: lamuppan $
 * Last Modified:       $Date: 2015/02/24 03:29:23 $
 *
 * $Log: vsnap_io.c,v $
 * Revision 1.17  2015/02/24 03:29:23  lamuppan
 * Bug #1619650, 1619648, 1619642, 1619640, 1619637, 1619636, 1619635: Fix for policheck
 * bugs for Filter and Vsnap drivers. Please refer to bugs in TFS for more details.
 *
 * Revision 1.16  2014/10/10 08:04:09  praweenk
 * Bug ID:917800
 * Description: Safe API changes for filter and VSNAP driver cross platform.
 *
 * Revision 1.15  2011/01/13 06:27:51  chvirani
 * 14878: Changes to support AIX.
 *
 * Revision 1.14  2010/06/21 11:25:01  chvirani
 * 10967: Handle requests beyond device boundary on solaris
 *
 * Revision 1.13  2010/03/31 13:24:01  chvirani
 * o X: Convert disk offsets to inm_offset_t
 *
 * Revision 1.12  2010/03/10 12:21:40  chvirani
 * Multithreaded driver - linux
 *
 * Revision 1.11  2009/08/19 11:56:13  chvirani
 * Use splice/page_cache calls to support newer kernels
 *
 * Revision 1.10  2009/04/28 12:54:49  cswarna
 * These files were changed during vsnap driver code re-org
 *
 * Revision 1.9  2008/12/24 14:06:20  hpadidakul
 *
 * Change to the Vsnap driver as a part of fix.
 *
 * (1) New IOCLT introduced "IOCTL_INMAGE_REMOVE_VIRTUAL_VOLUME_FROM_RETENTION_LOG_NOFAIL  11"
 *
 * (2) New routine inm_delete_vsnap_bydevid() added.
 *
 * (3) delete_vsnapdev() is changed to delete_vsnap_byname().
 *
 * (4) ref count is assigned to one at creation of vsnap(inm_vs_init_dev()).
 *
 *
 * (5) VSNAP_FLAGS_FREEING check added at routine inm_vdev_make_request(), to fail IO's if device is stale.
 *
 * (6) ref count check added at routine inm_vs_blk_close(), which will call inm_delete_vsnap_bydevid() if ref count is one.
 *
 * (7) INM_VSNAP_DELETE_CANFAIL flag added.
 *
 * Modified files: VsnapShared.h  vsnap_control.c vsnap_control.h vsnap_io.c vsnap_ioctl.c vsnap_open.c.
 *
 * Reviewed by: Sridhara and Chirag.
 *
 * Smokianide test done by: Janardhana Reddy Kota.
 *
 * Revision 1.8  2008/08/12 22:27:23  praveenkr
 * fix for #5404: Decoupled the mount point away from creation of vsnap
 * Moved the decision on the device name away from driver into the CX
 * & 'cdpcli'
 *
 * Revision 1.7  2008/07/04 11:24:52  praveenkr
 * Fix for #5199: RO vsnaps for reiserfs are failing during mount.
 *
 * Revision 1.6  2008/03/25 12:59:52  praveenkr
 * code cleanup for Solaris
 *
 * Revision 1.5  2007/12/04 13:27:08  praveenkr
 * removed development printks
 *
 * Revision 1.4  2007/12/04 12:53:38  praveenkr
 * fix for the following defects -
 *  4483 - Under low memory condition, vsnap driver suspended forever
 * 	4484 - vsnaps & dataprotection not responding
 * 	4485 - vsnaps utilizing more than 90% CPU
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
 * Revision 1.1.2.5  2007/05/25 09:45:44  praveenkr
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
 * Revision 1.1.2.1  2007/04/11 19:49:37  gregzav
 *   initial release of the new make system
 *   note that some files have been moved from Linux to linux (linux os is
 *   case sensitive and we had some using lower l and some using upper L for
 *   linux) this is to make things consitent also needed to move cdpdefs.cpp
 *   under config added the initial brocade work that had been done.
 *
 *****************************************************************************/

#include "common.h"
#include "vsnap_kernel_helpers.h"
#include "vsnap_io.h"

/* to facilitate ident */
static const char rcsid[] = "$Id: vsnap_io.c,v 1.17 2015/02/24 03:29:23 lamuppan Exp $";

extern thread_pool_t	*vs_ops_pool;

void inline
inm_bio_endio(inm_block_io_t *bio, inm_32_t error, bio_data_t *bio_data)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24)
	bio_endio(bio, bio->bi_size, error);
#else
	bio_endio(bio, error);
#endif
}

/*
 * inm_vs_read_from_retention:  Called once target io as well as btree lookup
 *				& target io completes. bio_data->bid_ret_data
 *				points to lookup results. Use the results to
 *				generate retention io tasks.
 */

void
inm_vs_read_from_retention(vsnap_task_t *task)
{
	vsnap_task_t		*io_task = NULL;
	struct bio		*bio = NULL;
	bio_data_t		*bio_data = NULL;
	ret_io_t		*ret_io = NULL;
	inm_u32_t		num_reads = 0;
	inm_vs_offset_split_t	*offset_split = NULL;
	inm_u64_t		*offset = NULL;
	inm_u64_t		start_offset = 0;
	inm_32_t		low, high, mid;
	struct bio_vec 		*bvec = NULL;
	struct bio_vec 		*tmp_bvec = NULL;
	inm_u32_t		i;
	single_list_entry_t     *ptr;
	inm_u32_t		num_vecs;
	single_list_entry_t	offset_split_list_head;

	bio = (struct bio *)task->vst_task_data;
	bio_data = (bio_data_t *)bio->bi_private;

	/*
	 * Since the bio can be freed asynchronously by another thread,
	 * make copies of the data required
	 */
	num_vecs = bio->bi_vcnt;
	memcpy_s(&offset_split_list_head, sizeof(single_list_entry_t),
		&(bio_data->bid_ret_data.lrd_ret_data), sizeof(single_list_entry_t));

	/*
	 * Set the number of retention reads to be performed
	 * If not required, end io
	 */
	INM_LIST_NUM_NODES(&offset_split_list_head, ptr, num_reads);
	if ( !num_reads ) {
		ASSERT(1);
		inm_vdev_endio(task);
		goto done;
	}

	DEV_DBG("%d reads from retention", num_reads);
	INM_ATOMIC_SET(&bio_data->bid_status, num_reads);

	/*
	 * Allocate and read array of offsets for each bvec
	 */
	offset = INM_ALLOC(sizeof(inm_u64_t) * num_vecs, GFP_NOIO);
	if ( !offset ) {
		ERR("No Mem");
		bio_data->bid_error = -ENOMEM;
		goto offset_array_alloc_failed;
	}

	start_offset = (inm_u64_t)BIO_OFFSET(bio);

	bio_for_each_segment(bvec, bio, i) {
		offset[i] = start_offset;
		start_offset += bvec->bv_len;
		DEV_DBG("bvec[%d]->offset = %u, bvec[%d]->len = %u", i,
			bvec->bv_offset, i, bvec->bv_len);
		DEV_DBG("offset[%d] = %llu", i, offset[i]);
	}

	/* Read from retention */
	while ( !INM_LIST_IS_EMPTY(&offset_split_list_head) ) {

		offset_split = (inm_vs_offset_split_t *)
		    INM_LIST_POP_ENTRY(&offset_split_list_head);

		low = -1;
		high = bio->bi_vcnt;
		bvec = NULL;

		DEV_DBG("Get bvec for offset = %llu", offset_split->os_offset);
		/*
		 * Use binary search to get the appropriate bvec
		 */
		while ( 1 ) {
			mid = (high - low) / 2;
			if ( 0 == mid )
				break;

			mid += low;

			tmp_bvec = bio_iovec_idx(bio, mid);

			if ( offset_split->os_offset < offset[mid] )
				high = mid;
			else if ( offset_split->os_offset >= offset[mid] +
				  tmp_bvec->bv_len )
				low = mid;
			else {
				bvec = tmp_bvec;
				break;
			}
		}

		if ( !bvec ) {
			ERR("Could not find bvec for offset %llu",
			    offset_split->os_offset);
			bio_for_each_segment(bvec, bio, i) {
				ERR("offset[%d] = %llu", i, offset[i]);
			}
			bio_data->bid_error = -EINVAL;
			BUG_ON(!bvec);
			goto bvec_not_found;
		}

		/*
		 * We now have the bvec we need to copy from retention
		 */

		io_task = INM_ALLOC(sizeof(vsnap_task_t), GFP_NOIO);
		if ( !io_task ) {
			ERR("No mem for ret task");
			bio_data->bid_error = -ENOMEM;
			goto io_task_alloc_failed;
		}

		ret_io = INM_ALLOC(sizeof(ret_io_t), GFP_NOIO);
		if ( !ret_io ) {
			ERR("No mem for retention io");
			bio_data->bid_error = -ENOMEM;
			goto ret_io_alloc_failed;
		}

		ret_io->ret_bio = bio;
		ret_io->ret_fid = offset_split->os_fid;

		ret_io->ret_pd.pd_bvec_page = bvec->bv_page;
		ret_io->ret_pd.pd_bvec_offset = bvec->bv_offset +
				(offset_split->os_offset - offset[mid]);
		ret_io->ret_pd.pd_file_offset = offset_split->os_file_offset;

		/*
		 * Single tuple may span over multiple bvecs
		 */
		if( offset_split->os_len <= bvec->bv_offset +
					    bvec->bv_len -
					    ret_io->ret_pd.pd_bvec_offset ) {

			ret_io->ret_pd.pd_size = offset_split->os_len;
			DEV_DBG("Free offset_split at %p", offset_split);
			INM_FREE(&offset_split, sizeof(inm_vs_offset_split_t));

		} else {

			DBG("split bvec");
			ret_io->ret_pd.pd_size = bvec->bv_offset +
						 bvec->bv_len -
						 ret_io->ret_pd.pd_bvec_offset;

			/*
			 * If it spans over multiple bvecs, modify offset_split
			 * with new offset and push it into the list so that it
			 * is processed in next iteration
			 */
			offset_split->os_file_offset +=
				ret_io->ret_pd.pd_size;

			offset_split->os_offset += ret_io->ret_pd.pd_size;
			offset_split->os_len -= ret_io->ret_pd.pd_size;

			INM_LIST_PUSH_ENTRY(&offset_split_list_head,
					    (single_list_entry_t*)offset_split);
			/*
			 * Increase num_reads and bid_status so that we wait for
			 * this new i/o to complete as well
			 */
			num_reads++;
			atomic_inc(&bio_data->bid_status);

		}

		DEV_DBG("Found bvec = %d, offset = %u, len = %u", mid,
			ret_io->ret_pd.pd_bvec_offset, ret_io->ret_pd.pd_size);

		io_task->vst_task_data = (void *)ret_io;
		io_task->vst_task_id = INM_TASK_RETENTION_IO;

		ADD_TASK_TO_POOL_HEAD(vs_ops_pool, io_task);

		num_reads--;
	}

out:
	INM_FREE(&offset, sizeof(inm_u64_t) * num_vecs);

done:
	return;


ret_io_alloc_failed:
	INM_FREE(&io_task, sizeof(vsnap_task_t));

io_task_alloc_failed:
bvec_not_found:
	INM_FREE(&offset_split, sizeof(inm_vs_offset_split_t));
	num_reads--;

offset_array_alloc_failed:
	while ( num_reads ) {
		offset_split = (inm_vs_offset_split_t *)
		    INM_LIST_POP_ENTRY(&offset_split_list_head);
		INM_FREE(&offset_split, sizeof(inm_vs_offset_split_t));

		/*
		 * If retention ios already fired have completed
		 * end_io. If not, endio is taken care of by on retention io
		 * completion.
		 */
		if ( INM_ATOMIC_DEC_AND_TEST(&bio_data->bid_status) )
			inm_vdev_endio(task);

		num_reads--;
	}
	goto out;

}

inm_32_t
read_from_target_volume(vsnap_task_t *task)
{
	inm_32_t 	retval = 0;
	struct bio	*bio = (struct bio *)task->vst_task_data;
	inm_offset_t	pos = BIO_OFFSET(bio);
	bio_data_t	*bio_data = (bio_data_t *)bio->bi_private;
	inm_vdev_t 	*vdev = bio_data->bid_vdev;
	inm_vs_info_t	*vs_info = (inm_vs_info_t *)vdev->vd_private;
	struct bio_vec 	*bvec;
	inm_vdev_page_data_t page_data;
	inm_32_t 	i;

        bio_for_each_segment(bvec, bio, i) {
		/* fill the page_data from bvec */
		page_data.pd_bvec_page = bvec->bv_page;
		page_data.pd_bvec_offset = bvec->bv_offset;
		page_data.pd_size = bvec->bv_len;
		page_data.pd_disk_offset = 0;
		page_data.pd_file_offset = (inm_u64_t)pos;
		DEV_DBG("Reading offset = %llu, len = %u bytes",
			page_data.pd_file_offset, page_data.pd_size);
		retval =
		    read_from_parent_volume(vs_info->vsi_target_filp,
					    &page_data);
		if ( retval < 0 ) {
			/* error code */
			break;
		}

		pos += bvec->bv_len;
        }

	return retval;
}

inline inm_32_t
read_from_this_fileid(inm_block_io_t *bio, bio_data_t *bio_data,
		      ret_io_t *ret_io)
{
	return read_from_this_fid(&ret_io->ret_pd, ret_io->ret_fid,
				((inm_vdev_t *)bio_data->bid_vdev)->vd_private);

}

/*
 * process_for_write
 *
 * DESC
 *      create a page_data structure to pass on the bvec->page until the final
 *      io processing by invoking the underlying device's write fn ptr
 */
static inline inm_32_t
process_for_write(inm_vdev_t *vdev, struct bio_vec *bvec, inm_offset_t pos)
{
	inm_vdev_page_data_t page_data;

	/* fill the page_data from bvec */
	page_data.pd_bvec_page = bvec->bv_page;
	page_data.pd_bvec_offset = bvec->bv_offset;
	page_data.pd_size = bvec->bv_len;
	page_data.pd_disk_offset = pos;
	page_data.pd_file_offset = 0;
	DEV_DBG("calling vd_rw_ops->write for %d bytes @ %lld",
		page_data.pd_size, page_data.pd_disk_offset);
	return vdev->vd_rw_ops.write(page_data, vdev->vd_private);
}

/*
 * handle_bio_for_write_to_vv
 *
 * DESC
 *      loop for each bvec from the given bio & process the write
 *      to virtual volume
 */
inm_32_t
handle_bio_for_write_to_vv(inm_vdev_t *vdev, struct bio *bio, inm_offset_t pos)
{
	inm_32_t i;
	inm_32_t retval = 0;
	struct bio_vec *bvec;

        bio_for_each_segment(bvec, bio, i) {
                retval = process_for_write(vdev, bvec, pos);
                if (retval < 0)
                        break;
                pos += bvec->bv_len;
        }
        return retval;
}

/*
 * process_for_read
 *
 * DESC
 *      create a page_data structure to pass on the bvec->page until the final
 *      io processing by invoking the underlying device's read fn ptr
 */
static inline inm_32_t
process_for_read(inm_vdev_t *vdev, struct bio_vec *bvec, inm_offset_t pos)
{
	inm_vdev_page_data_t page_data;

	/* fill the page_data from bvec */
	page_data.pd_bvec_page = bvec->bv_page;
	page_data.pd_bvec_offset = bvec->bv_offset;
	page_data.pd_size = bvec->bv_len;
	page_data.pd_disk_offset = pos;
	page_data.pd_file_offset = 0;
	DEV_DBG("calling vd_rw_ops->read for %d bytes @ %lld",
		page_data.pd_size, page_data.pd_disk_offset);
	return vdev->vd_rw_ops.read(vdev->vd_private, pos, bvec->bv_len,
				    &page_data);
}

/*
 * handle_bio_for_read
 *
 * DESC
 *      loop for each bvec from the given bio & process the read
 */
inm_32_t
handle_bio_for_read_from_vv(inm_vdev_t *vdev, struct bio *bio, inm_offset_t pos)
{
	inm_32_t i;
	inm_32_t retval = 0;
	struct bio_vec *bvec;

        bio_for_each_segment(bvec, bio, i) {
                retval = process_for_read(vdev, bvec, pos);
                if (retval < 0)
                        break;
                pos += bvec->bv_len;
        }

        return retval;
}

/*
 * handle_bio_for_write
 *
 * DESC
 *      loop for each bvec from the given bio & process the write
 */
inm_32_t
handle_bio_for_write_to_vsnap(vsnap_task_t *task)
{
        inm_32_t i;
        inm_32_t retval = 0;
        inm_block_io_t	*bio = (inm_block_io_t *)task->vst_task_data;
        bio_data_t      *bio_data = (bio_data_t *)BIO_PRIVATE(bio);
        inm_offset_t    pos = BIO_OFFSET(bio);
        inm_vdev_t      *vdev = bio_data->bid_vdev;
        inm_vs_info_t   *vs_info = (inm_vs_info_t *)vdev->vd_private;
        struct bio_vec  *bvec;

        INM_WRITE_LOCK(vs_info->vsi_btree_lock);

        bio_for_each_segment(bvec, bio, i) {
                retval = process_for_write(vdev, bvec, pos);
                if ( retval < 0 )
                        break;
                pos += bvec->bv_len;
        }

	INM_WRITE_UNLOCK(vs_info->vsi_btree_lock);

	return retval;
}

inm_32_t
inm_vs_make_request(request_queue_t *q, inm_block_io_t *bio)
{
	return inm_vs_generic_make_request(q->queuedata, bio);
}

inm_32_t
inm_vv_make_request(request_queue_t *q, inm_block_io_t *bio)
{
	return inm_vv_generic_make_request(q->queuedata, bio);
}
