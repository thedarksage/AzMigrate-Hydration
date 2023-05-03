#include "vsnap_io.h"
#include "vsnap_io_common.h"
#include "vdev_thread_common.h"

extern void		*vs_listp;
extern void		*vv_listp;
extern dev_info_t	*vdev_dip;
extern thread_pool_t    *vs_ops_pool;

void INLINE
inm_bio_endio(inm_block_io_t *bio, inm_32_t error, bio_data_t *bio_data)
{
        inm_minor_t     minor = getminor(bio->b_edev);
        inm_vdev_t      **vdevpp;
        inm_vdev_t      *vdev;
        inm_32_t        part_idx = -1;

        /*
         * Restore partition I/O mapping for vsnaps
         */
        if ( INM_IS_VSNAP_DEVICE(minor) ) {
                vdevpp = (inm_vdev_t **)ddi_get_soft_state(vs_listp,
                                        INM_SOL_VS_SOFT_STATE_IDX(minor));

                vdev = *vdevpp;

                /*
                 * If the I/O comes on a partition, map the
                 * partition offset to vsnap offset
                 */
                if ( INM_MINOR_IS_PARTITION(vdev, minor) ) {
                        part_idx = INM_PART_IDX(vdev->vd_index, minor);
                        bio->b_lblkno -= INM_PART_START_BLOCK(vdev, part_idx);
                }
        }

	if ((bio_data != NULL) && INM_TEST_BIT(INM_BUF_REMAPPED, &bio_data->bid_flags))
		bp_mapout(bio);

	error = -error;  // Make it +ve as we will get -ve value on error.

	bioerror(bio, error);
	biodone(bio);
}

INLINE inm_u64_t
inm_get_parition_offset(inm_vdev_t *vdev, inm_block_io_t *bio)
{
	inm_minor_t 	minor = getminor(bio->b_edev);
	inm_32_t	partidx;
	inm_u64_t	max_offset;

	if ( INM_IS_VSNAP_DEVICE(minor) ) {
		if( INM_MINOR_IS_PARTITION(vdev, minor) ) {
			partidx = INM_PART_IDX(vdev->vd_index, minor);
			max_offset = ( INM_PART_SIZE(vdev, partidx) +
				    INM_PART_START_BLOCK(vdev,partidx) ) *
				    DEV_BSIZE;
		} else {
			max_offset = INM_VDEV_VIRTUAL_SIZE(vdev);
		}
	} else {
		max_offset = INM_VDEV_SIZE(vdev);
	}

	return max_offset;
}

/*
 * Solaris has peculiar requirements handling i/o beyond device size.
 * For offset <= size of device:
 *	READ: 	a success should be returned with resid field set to number
 *		of bytes we cannot fulfill for reads.
 *
 *	WRITE: 	if offset == size of device, return error else return success
 *		with resid field set appropriately
 *
 * For offset > size of device
 *	Return error. No need to set the resid field.
 */
void
inm_handle_end_of_device(inm_vdev_t *vdev, inm_block_io_t *bio)
{
	bio_data_t *bio_data = (bio_data_t *)BIO_PRIVATE(bio);
	inm_32_t	rw;
	inm_u64_t	max_offset;

	DBG("Handle EOD");
	BIO_TYPE(bio, &rw);

	max_offset = INM_DEVICE_MAX_OFFSET(vdev, bio);

	if( BIO_OFFSET(bio) == max_offset ) {
		if( rw == READ ) {
			DBG("Read On Boundary");
			BIO_RESID(bio) = BIO_SIZE(bio);
			bio_data->bid_error = 0;
		} else {
			DBG("Write on boundary, returning error");
			bio_data->bid_error = -ENXIO;
		}
	} else if (BIO_OFFSET(bio) > max_offset ) {
		DBG("Beyond Disk Boundary, returning error");
		bio_data->bid_error = -ENXIO;
	} else {
		/* partial block, return success */
		DBG("Partial block");
		/*
		 * The I/O may have come for an partial block.
		 * If there was an error processing the valid partial
		 * block, we propagate that error and not mark resid
		 */
		if( !bio_data->bid_error )
			BIO_RESID(bio) = BIO_SIZE(bio) - bio_data->bid_size;
	}

	return;
}

inm_32_t
read_from_target_volume(vsnap_task_t *task)
{
        inm_32_t        retval = 0;
        struct buf      *bp = (struct buf*)task->vst_task_data;
        inm_offset_t    pos = BIO_OFFSET(bp);
        bio_data_t      *bio_data = (bio_data_t *)BIO_PRIVATE(bp);
        inm_vdev_t      *vdev = bio_data->bid_vdev;
        inm_vs_info_t   *vs_info = (inm_vs_info_t *)vdev->vd_private;
        inm_vdev_page_data_t page_data;

	if ( !(bp->b_flags & B_REMAPPED) ) {
		bp_mapin(bp);
		INM_SET_BIT(INM_BUF_REMAPPED, &bio_data->bid_flags);
        }

	if( pos < INM_VDEV_SIZE(vdev) ) {
		page_data.pd_bvec_page = bp->b_un.b_addr;
		page_data.pd_bvec_offset = 0;
		page_data.pd_size = bio_data->bid_size;
		page_data.pd_disk_offset = 0;
		page_data.pd_file_offset = pos;

		DEV_DBG("Reading offset = %llu, len = %u bytes",
			page_data.pd_file_offset, page_data.pd_size);
		retval = read_from_parent_volume(vs_info->vsi_target_filp,
						 &page_data);
	} else {
		DBG("I/O in virtual area, return zero filled data");
		memzero(bp->b_un.b_addr, bio_data->bid_size);
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
 * inm_vs_read_from_retention:  Called once target io as well as btree lookup
 *                              & target io completes. bio_data->bid_ret_data
 *                              points to lookup results. Use the results to
 *                              generate retention io tasks.
 */

void
inm_vs_read_from_retention(vsnap_task_t *task)
{
        vsnap_task_t            *io_task = NULL;
        inm_block_io_t		*bio = NULL;
        bio_data_t              *bio_data = NULL;
        ret_io_t                *ret_io = NULL;
        inm_u32_t               num_reads = 0;
        inm_vs_offset_split_t   *offset_split = NULL;
        inm_u64_t               *offset = NULL;
        inm_u64_t               start_offset = 0;
        inm_u32_t               i;
        single_list_entry_t     *ptr;
        single_list_entry_t     offset_split_list_head;

        bio = (inm_block_io_t*)task->vst_task_data;
        bio_data = (bio_data_t *)BIO_PRIVATE(bio);

        /*
         * Since the bio can be freed asynchronously by another thread,
         * make copies of the data required
         */
        memcpy_s(&offset_split_list_head, sizeof(single_list_entry_t),
		&(bio_data->bid_ret_data.lrd_ret_data), sizeof(single_list_entry_t));

        /*
         * Set the number of retention reads to be performed
         * If not required, end io
         */
        INM_LIST_NUM_NODES(&offset_split_list_head, ptr, num_reads);
        if ( !num_reads ) {
                inm_vdev_endio(task);
                goto out;
        }
        INM_ATOMIC_SET(&bio_data->bid_status, num_reads);

        start_offset = (inm_u64_t)BIO_OFFSET(bio);

        if ( !(bio->b_flags & B_REMAPPED) ) {
                bp_mapin(bio);
		INM_SET_BIT(INM_BUF_REMAPPED, &bio_data->bid_flags);
        }

        /* Read from retention */
        while ( !INM_LIST_IS_EMPTY(&offset_split_list_head) ) {

                offset_split = (inm_vs_offset_split_t *)
                    INM_LIST_POP_ENTRY(&offset_split_list_head);


                DEV_DBG("Get bvec for offset = %llu", offset_split->os_offset);

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

                ret_io->ret_pd.pd_bvec_page = bio->b_un.b_addr;
                ret_io->ret_pd.pd_bvec_offset =
					offset_split->os_offset - start_offset;
                ret_io->ret_pd.pd_file_offset = offset_split->os_file_offset;
		ret_io->ret_pd.pd_size = offset_split->os_len;


                io_task->vst_task_data = (void *)ret_io;
                io_task->vst_task_id = INM_TASK_RETENTION_IO;

                ADD_TASK_TO_POOL_HEAD(vs_ops_pool, io_task);

		INM_FREE(&offset_split, sizeof(inm_vs_offset_split_t));
                num_reads--;
        }

out:
        return;


ret_io_alloc_failed:
        INM_FREE(&io_task, sizeof(vsnap_task_t));

io_task_alloc_failed:
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

/* Good stuff starts here */
/*
 * process_for_read
 *
 * DESC
 *      create a page_data structure to pass on the bvec->page until the final
 *      io processing by invoking the underlying device's read fn ptr
 */
static INLINE inm_32_t
process_for_write(inm_vdev_t *vdev, struct buf *bp, inm_offset_t pos)
{
	inm_vdev_page_data_t 	page_data;
	inm_32_t		retval = 0;
	bio_data_t		*bio_data = (bio_data_t *)BIO_PRIVATE(bp);

	DEV_DBG("Writing %u bytes", bio_data->bid_size);
	/* fill the page_data from buf */
	page_data.pd_bvec_page = bp->b_un.b_addr;
	/*
	 * We are dealing with virtual addresses
	 * so offset will always be 0
	 */
	page_data.pd_bvec_offset = 0;
	page_data.pd_size = bio_data->bid_size;
	page_data.pd_disk_offset = pos;
	page_data.pd_file_offset = 0;
	DEV_DBG("calling vd_rw_ops->write for %d bytes @ %lld",
		page_data.pd_size, page_data.pd_disk_offset);
 	retval = vdev->vd_rw_ops.write(page_data, vdev->vd_private);

	return retval;
}

/*
 * handle_bio_for_read
 *
 * DESC
 *      loop for each bvec from the given bio & process the read
 */
inm_32_t
handle_bio_for_write_to_vv(inm_vdev_t *vdev, inm_block_io_t *bio,
			   inm_offset_t pos)
{
	inm_32_t retval = 0;
	bio_data_t      *bio_data = BIO_PRIVATE(bio);

	if ( !(bio->b_flags & B_REMAPPED) ) {
		bp_mapin(bio);
		INM_SET_BIT(INM_BUF_REMAPPED, &bio_data->bid_flags);
	}

	DEV_DBG("Bio of size = %u", bio_data->bid_size);
	retval = process_for_write(vdev, bio, pos);

	return retval;
}


/*
 * handle_bio_for_read
 *
 * DESC
 *      loop for each bvec from the given bio & process the read
 */
inm_32_t
handle_bio_for_write_to_vsnap(vsnap_task_t *task)
{
	inm_block_io_t  *bio = task->vst_task_data;
        bio_data_t      *bio_data = (bio_data_t *)BIO_PRIVATE(bio);
        inm_offset_t    pos = BIO_OFFSET(bio);
        inm_vdev_t      *vdev = bio_data->bid_vdev;
	inm_vs_info_t	*vs_info = vdev->vd_private;
	inm_32_t	retval = 0;

	if ( !(bio->b_flags & B_REMAPPED) ) {
		bp_mapin(bio);
		INM_SET_BIT(INM_BUF_REMAPPED, &bio_data->bid_flags);
	}

	DEV_DBG("Bio of size = %u", bio_data->bid_size);

        INM_WRITE_LOCK(vs_info->vsi_btree_lock);
	retval = process_for_write(vdev, bio, pos);
	INM_WRITE_UNLOCK(vs_info->vsi_btree_lock);

	return retval;
}
/*
 * process_for_read
 *
 * DESC
 *      create a page_data structure to pass on the bvec->page until the final
 *      io processing by invoking the underlying device's read fn ptr
 */
static INLINE inm_32_t
process_for_read(inm_vdev_t *vdev, struct buf *bp, inm_offset_t pos)
{
	inm_vdev_page_data_t 	page_data;
	inm_32_t		retval = 0;
	bio_data_t		*bio_data = BIO_PRIVATE(bp);

	DEV_DBG("Reading %u bytes", bio_data->bid_size);
	/* fill the page_data from buf */
	page_data.pd_bvec_page = bp->b_un.b_addr;
	/*
	 * We are dealing with virtual addresses
	 * so offset will always be 0
	 */
	page_data.pd_bvec_offset = 0;
	page_data.pd_size = bio_data->bid_size;
	page_data.pd_disk_offset = pos;
	page_data.pd_file_offset = 0;
	DEV_DBG("calling vd_rw_ops->read for %d bytes @ %lld",
		page_data.pd_size, page_data.pd_disk_offset);
	retval = vdev->vd_rw_ops.read(vdev->vd_private, pos, bio_data->bid_size,
				      &page_data);

	return retval;
}

/*
 * handle_bio_for_read
 *
 * DESC
 *      loop for each bvec from the given bio & process the read
 */
inm_32_t
handle_bio_for_read_from_vv(inm_vdev_t *vdev, inm_block_io_t *bio,
			    inm_offset_t pos)
{
	inm_32_t retval = 0;
        bio_data_t      *bio_data = BIO_PRIVATE(bio);

	if ( !(bio->b_flags & B_REMAPPED) ) {
		bp_mapin(bio);
		INM_SET_BIT(INM_BUF_REMAPPED, &bio_data->bid_flags);
	}

	DEV_DBG("Bio of size = %u", bio_data->bid_size);
	retval = process_for_read(vdev, bio, pos);

	return retval;
}


inm_32_t
vdev_strategy(inm_block_io_t *bio)
{
	inm_minor_t	minor = getminor(bio->b_edev);
	inm_vdev_t	**vdevpp = NULL;
	inm_vdev_t	*vdev = NULL;
	inm_32_t	part_idx = -1;

	if ( minor == 0 ) {
		bioerror(bio, EIO);
	        biodone(bio);
		return 0;
	}

	if ( INM_IS_VSNAP_DEVICE(minor) )
		vdevpp = (inm_vdev_t **)ddi_get_soft_state(vs_listp,
					INM_SOL_VS_SOFT_STATE_IDX(minor));
	else
		vdevpp = (inm_vdev_t **)ddi_get_soft_state(vv_listp,
					INM_SOL_VV_SOFT_STATE_IDX(minor));

	ASSERT(!vdevpp);
	vdev = *vdevpp;
	ASSERT(!vdev);

	if ( INM_IS_VSNAP_DEVICE(minor) ) {
                /*
                 * If the I/O comes on a partition, map the
                 * partition offset to vsnap offset
                 */
		if ( INM_MINOR_IS_PARTITION(vdev, minor) ) {
			part_idx = INM_PART_IDX(vdev->vd_index, minor);

			DEV_DBG("Partition Offset %ll = Vsnap Offset %ll",
                                bio->b_lblkno, bio->b_lblkno +
                                INM_PART_START_BLOCK(vdev, part_idx));
			bio->b_lblkno += INM_PART_START_BLOCK(vdev, part_idx);
		}

		return inm_vs_generic_make_request(vdev, bio);
	} else {
		return inm_vv_generic_make_request(vdev, bio);
	}
}
/*ARGSUSED2*/
inm_32_t
vdev_read(dev_t dev, struct uio *uio, struct cred *credp)
{
	if (getminor(dev) == 0)
		return (EINVAL);
	return (physio(vdev_strategy, NULL, dev, B_READ, minphys, uio));

}

/*ARGSUSED2*/
inm_32_t
vdev_write(dev_t dev, struct uio *uio, struct cred *credp)
{
	if (getminor(dev) == 0)
		return (EINVAL);
	return (physio(vdev_strategy, NULL, dev, B_WRITE, minphys, uio));

}

/*ARGSUSED2*/
inm_32_t
vdev_aread(dev_t dev, struct aio_req *aio, struct cred *credp)
{
	if (getminor(dev) == 0)
		return (EINVAL);
	return (aphysio(vdev_strategy, anocancel, dev, B_READ, minphys, aio));
}

/*ARGSUSED2*/
inm_32_t
vdev_awrite(dev_t dev, struct aio_req *aio, struct cred *credp)
{
	if (getminor(dev) == 0)
		return (EINVAL);
	return (aphysio(vdev_strategy, anocancel, dev, B_WRITE, minphys, aio));
}

