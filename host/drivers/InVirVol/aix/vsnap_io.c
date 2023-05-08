#include "vsnap_io.h"
#include "vsnap_io_common.h"
#include "vdev_thread_common.h"

extern void		*vs_listp;
extern void		*vv_listp;
extern thread_pool_t    *vs_ops_pool;

/* IO init() and exit()
 * Specific to AIX. When strategy is called in interrupt context, queue the
 * BIOs chained here and wake associated thread. Thread will process the bios
 * and go through normal queue process. 0xDEADBEEF marks the thread for death.
 * Since this code is AIX only, not using generic calls in all places.
 */
bio_chain_t *bio_chain;

void INLINE
inm_bio_endio(inm_block_io_t *bio, inm_32_t error, bio_data_t *bio_data)
{
	if( error ) {
		INM_SET_BIT(B_ERROR, &bio->b_flags);
		bio->b_resid = BIO_SIZE(bio);
		if ( getpid != -1 )
			ERR("Returning I/O Error %d", error);
		bio->b_error = (char)-error;
	}

	iodone(bio);
}


/*
 * AIX has peculiar requirements handling i/o beyond device size, similar to
 * Solaris. if the read starts at end of device, no error is returned and
 * resid == BIO_SIZE(). For all other requests staring at or beyond end of
 * device, error needs to be returned. For a partial request beyond end of
 * device, the resid field needs to be set appropriately.
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
        struct buf      *bio = (struct buf*)task->vst_task_data;
        inm_offset_t    pos = BIO_OFFSET(bio);
        bio_data_t      *bio_data = (bio_data_t *)BIO_PRIVATE(bio);
        inm_vdev_t      *vdev = bio_data->bid_vdev;
        inm_vs_info_t   *vs_info = (inm_vs_info_t *)vdev->vd_private;
        inm_vdev_page_data_t page_data;

	xm_mapin(&bio->b_xmemd, bio->b_baddr, bio_data->bid_size,
		 &page_data.pd_bvec_page);
        page_data.pd_bvec_offset = 0;
        page_data.pd_size = bio_data->bid_size;
        page_data.pd_disk_offset = 0;
        page_data.pd_file_offset = pos;


        DEV_DBG("Reading offset = %llu, len = %u bytes",
                 page_data.pd_file_offset, page_data.pd_size);
        retval = read_from_parent_volume(vs_info->vsi_target_filp,
                                         &page_data);

	xm_det(page_data.pd_bvec_page, &bio->b_xmemd);

out:
	return retval;
}

inm_32_t
read_from_this_fileid(inm_block_io_t *bio, bio_data_t *bio_data,
		      ret_io_t *ret_io)
{
	inm_vs_info_t   *vs_info;
	inm_32_t retval = 0;
	caddr_t	oldaddr = NULL;

	vs_info = ((inm_vdev_t *)bio_data->bid_vdev)->vd_private;

	oldaddr = ret_io->ret_pd.pd_bvec_page;
	xm_mapin(&bio->b_xmemd, bio->b_baddr, bio_data->bid_size,
		 &ret_io->ret_pd.pd_bvec_page);


	retval = read_from_this_fid(&ret_io->ret_pd, ret_io->ret_fid, vs_info);
	if( retval < 0 )
		ERR("Retention I/O error");

	xm_det(ret_io->ret_pd.pd_bvec_page, &bio->b_xmemd);
	ret_io->ret_pd.pd_bvec_page = oldaddr;

out:
	return retval;
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
        inm_u64_t               start_offset = 0;
        single_list_entry_t     *ptr;
        single_list_entry_t     offset_split_list_head;
	inm_32_t		error = 0;

        bio = (inm_block_io_t*)task->vst_task_data;
        bio_data = (bio_data_t *)BIO_PRIVATE(bio);

        /*
         * Since the bio can be freed asynchronously by another thread,
         * make copies of the data required
         */
        memcpy_s(&offset_split_list_head, sizeof(single_list_entry_t),
		&(bio_data->bid_ret_data.lrd_ret_data),
		sizeof(single_list_entry_t));

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

                ret_io->ret_pd.pd_bvec_page = bio->b_baddr;
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
process_for_write(inm_vdev_t *vdev, struct buf *bio, inm_offset_t pos)
{
	inm_vdev_page_data_t 	page_data;
	inm_32_t		retval = 0;
	bio_data_t		*bio_data = (bio_data_t *)BIO_PRIVATE(bio);



	DEV_DBG("Writing %u bytes", bio_data->bid_size);
	/* fill the page_data from buf */
	xm_mapin(&bio->b_xmemd, bio->b_baddr, bio_data->bid_size,
		 &page_data.pd_bvec_page);
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

	xm_det(page_data.pd_bvec_page, &bio->b_xmemd);

out:
	return retval;
}

/*
 * handle_bio_for_read
 *
 * DESC
 *      loop for each bvec from the given bio & process the read
 */
INLINE inm_32_t
handle_bio_for_write_to_vv(inm_vdev_t *vdev, inm_block_io_t *bio,
			   inm_offset_t pos)
{
	return process_for_write(vdev, bio, pos);
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
process_for_read(inm_vdev_t *vdev, struct buf *bio, inm_offset_t pos)
{
	inm_vdev_page_data_t 	page_data;
	inm_32_t		retval = 0;
	bio_data_t		*bio_data = (bio_data_t *)BIO_PRIVATE(bio);

	DEV_DBG("Reading %u bytes", bio_data->bid_size);
	/* fill the page_data from buf */
	xm_mapin(&bio->b_xmemd, bio->b_baddr, bio_data->bid_size,
		 &page_data.pd_bvec_page);
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

	xm_det(page_data.pd_bvec_page, &bio->b_xmemd);

out:
	return retval;
}

/*
 * handle_bio_for_read
 *
 * DESC
 *      loop for each bvec from the given bio & process the read
 */
INLINE inm_32_t
handle_bio_for_read_from_vv(inm_vdev_t *vdev, inm_block_io_t *bio,
			    inm_offset_t pos)
{
	return process_for_read(vdev, bio, pos);
}

inline inm_32_t
inm_vdev_queue_buf_chain(inm_vdev_t *vdev, inm_block_io_t *bio,
			 inm_32_t vsnap_device)
{
	inm_block_io_t *bio_next;
	inm_32_t	retval = 0;

	do {
		bio_next = BIO_NEXT(bio);
		BIO_RESID(bio) = 0;
		BIO_NEXT(bio) = NULL;

		if( !vdev ) { /* minor 0 control device - fail I/O */
			inm_bio_endio(bio, -EIO, 0);
		} else {
			if( vsnap_device )
				retval |=
					inm_vs_generic_make_request(vdev, bio);
			else
				retval |=
					inm_vv_generic_make_request(vdev, bio);
		}

		bio = bio_next;

	} while(bio);

	return retval;
}


/*
 * We can offload buf processing to io_queue_thread but we will be
 * bottlenecked because of single thread processing bufs of all devices
 * As such, we offload only when called in interrupt context
 */
inm_32_t
vdev_strategy(inm_block_io_t *bio)
{
	inm_minor_t	minor = minor_num(bio->b_dev);
	inm_vdev_t	*vdev = NULL;
	inm_interrupt_t	intr;
	inm_32_t	retval = 0;
	inm_block_io_t	*bufp;
	static inm_32_t faulty_call = 0;


	if ( minor != 0 ) {
		if ( INM_IS_VSNAP_DEVICE(minor) )
			vdev = inm_get_soft_state(vs_listp,
					INM_SOL_VS_SOFT_STATE_IDX(minor));
		else
			vdev = inm_get_soft_state(vv_listp,
					INM_SOL_VV_SOFT_STATE_IDX(minor));
	} else {
		vdev = NULL;
	}

	ASSERT(getpid() != -1 && vdev == NULL);

	if( !faulty_call && getpid() != -1 && vdev == NULL ) {
		faulty_call = 1;
		INFO("I/O from %d is not intended for any vdev", getpid());
	}

	INM_ACQ_SPINLOCK_IRQ(&bio_chain->bc_spinlock, intr);

	if ( bio_chain->bc_driver_unloading ) {
		INM_REL_SPINLOCK_IRQ(&bio_chain->bc_spinlock, intr);

		while ( bio ) {
			bufp = BIO_NEXT(bio);
			BIO_NEXT(bio) = NULL;
			inm_bio_endio(bio, -EIO, 0);
			bio = bufp;
		}

		return retval;
	}

	if( getpid() == -1 || intr != INTBASE ) {

#ifdef _VSNAP_DEBUG_
	if ( intr != INTIODONE )
		panic("vdev_strategy: need to disbale higher level interruts");
#endif

		if( vdev ) {
			/*
			 * We identify buf chains based on minor. For each buf
			 * chain, we inflate the pending count to protect vdev
			 * against deletion. If the last buf chain belonged to
			 * the same vdev, we do not inc pending as it is
			 * processed as a single larger chain
			 */
			if( minor != bio_chain->bc_prev_minor ) {
				INM_ACQ_SPINLOCK_IRQ(&vdev->vd_lock, intr);
				INM_ATOMIC_INC(&vdev->vd_io_pending);
				INM_REL_SPINLOCK_IRQ(&vdev->vd_lock, intr);
				bio_chain->bc_prev_minor = minor;
			}
		}

		/* populate append ptr with head of new buf chain */
		*(bio_chain->bc_append) = bio;
		bufp = bio;

		/* Point append ptr to av_fwd of last buf in chain */
		while( BIO_NEXT(bufp) )
			bufp = BIO_NEXT(bufp);

		bio_chain->bc_append = &(BIO_NEXT(bufp));

		INM_REL_SPINLOCK_IRQ(&bio_chain->bc_spinlock, intr);
		e_wakeup_one(&bio_chain->bc_event);
	} else {
		INM_REL_SPINLOCK_IRQ(&bio_chain->bc_spinlock, intr);
		retval =
		inm_vdev_queue_buf_chain(vdev, bio, INM_IS_VSNAP_DEVICE(minor));
	}

	return retval;
}

/* called by uphysio, do nothing */
inm_32_t
vdev_null(inm_block_io_t *bio, void *data)
{
	return 0;
}

/*ARGSUSED2*/
inm_32_t
vdev_read(dev_t dev, struct uio *uio, chan_t chan, int ext)
{
	if (minor_num(dev) == 0)
		return (EINVAL);

	return uphysio(uio, B_READ, 1, dev, vdev_strategy, vdev_null, NULL);
}

/*ARGSUSED2*/
inm_32_t
vdev_write(dev_t dev, struct uio *uio, chan_t chan, int ext)
{
	if (minor_num(dev) == 0)
		return (EINVAL);

	return uphysio(uio, B_WRITE, 1, dev, vdev_strategy, vdev_null, NULL);

}


static inm_32_t
vdev_queue_io(inm_32_t flag, void *data, inm_32_t datalen)
{
	inm_block_io_t *bio;
	inm_block_io_t *bio_chain_next;
	inm_block_io_t *bio_last_in_chain;
	inm_minor_t	minor;
	inm_vdev_t	*vdev;
	inm_interrupt_t	intr;
	inm_32_t	delete_flag = 0;
	inm_32_t	retval = 0;

	INM_WAKEUP(&bio_chain->bc_ipc);

	while(1) {
		INM_ACQ_SPINLOCK_IRQ(&bio_chain->bc_spinlock, intr);

		if( !bio_chain->bc_head ) {
			e_sleep_thread(&bio_chain->bc_event,
				       &bio_chain->bc_spinlock,
				       LOCK_HANDLER);
		}

		bio = bio_chain->bc_head;

		/* Reset the queue */
		bio_chain->bc_head = NULL;
		bio_chain->bc_append = &bio_chain->bc_head;
		bio_chain->bc_prev_minor = -1;

		INM_REL_SPINLOCK_IRQ(&bio_chain->bc_spinlock, intr);

		/*
		 * Find the maximum contiguous chain of buf with same minor
		 * and send it for processing. Decrease the pending count we
		 * had incremented in strategy.
		 */
		while( bio ) {
			/*
			 * find the start of next chain(different minor) as
			 * BIO_NEXT() will be modified and keep it locally
			 */
			minor = minor_num(bio->b_dev);
			bio_last_in_chain = bio;
			bio_chain_next = BIO_NEXT(bio);

			while( bio_chain_next &&
			       minor == minor_num(bio_chain_next->b_dev) ) {
				bio_last_in_chain = bio_chain_next;
				bio_chain_next = BIO_NEXT(bio_chain_next);
			}

			/*
			 * bio now has start of a buf chain while
			 * bio_chain_next now points to next chain
			 * in the list while bio_last_in_chain points
			 * to last buf for this device chain. Mark the
			 * next of last buf as NULL to remove association
			 * with buf chain of another device. Then get the
			 * *list_rw_sem lock to prevent deletion of dev.
			 * Decrement the pending count we had incremented
			 * while queueing this buf chain in interrupt path.
			 * Then get the vdev and send the buf chain for
			 * processing
			 */

			BIO_NEXT(bio_last_in_chain) = NULL;

			if ( minor != 0 ) {
				if ( INM_IS_VSNAP_DEVICE(minor) ) {
					vdev = inm_get_soft_state(vs_listp,
					      INM_SOL_VS_SOFT_STATE_IDX(minor));
					INM_READ_LOCK(&inm_vs_list_rwsem);

				} else {
					vdev = inm_get_soft_state(vv_listp,
					      INM_SOL_VV_SOFT_STATE_IDX(minor));
					INM_READ_LOCK(&inm_vv_list_rwsem);
				}

				ASSERT(vdev == NULL);

				if ( vdev ) {
					INM_ACQ_SPINLOCK_IRQ(&vdev->vd_lock, intr);
					INM_ATOMIC_DEC(&vdev->vd_io_pending);
					INM_REL_SPINLOCK_IRQ(&vdev->vd_lock, intr);
				}

			} else {
				vdev = NULL;
			}

			retval = inm_vdev_queue_buf_chain(vdev, bio,
						    INM_IS_VSNAP_DEVICE(minor));
			if( retval < 0 )
				ERR("Error processing I/O");

			if ( minor != 0 ) {
				if ( INM_IS_VSNAP_DEVICE(minor) )
					INM_READ_UNLOCK(&inm_vs_list_rwsem);
				else
					INM_READ_UNLOCK(&inm_vv_list_rwsem);
			}

			/* Loop and process chain for another dev */
			bio = bio_chain_next;
		}

		INM_ACQ_SPINLOCK_IRQ(&bio_chain->bc_spinlock, intr);

		if( bio_chain->bc_head == NULL && bio_chain->bc_driver_unloading ) {
			INM_REL_SPINLOCK_IRQ(&bio_chain->bc_spinlock, intr);
			DBG("Its time to die");
			break;
		}

		INM_REL_SPINLOCK_IRQ(&bio_chain->bc_spinlock, intr);
	}

	/* Thread dying */
	INM_WAKEUP(&bio_chain->bc_ipc);

	return 0;
}

inm_32_t
vdev_io_init()
{
	pid_t pid;
	inm_32_t retval = 0;

	bio_chain = INM_ALLOC(sizeof(bio_chain_t), GFP_PINNED);
	if( !bio_chain ) {
		return -ENOMEM;
		goto out;
	}

	/* Initialise bio chain */
	bio_chain->bc_head = NULL;
	bio_chain->bc_append = &bio_chain->bc_head;
	bio_chain->bc_prev_minor = -1;
	INM_INIT_IPC(&bio_chain->bc_ipc);
	INM_INIT_SPINLOCK(&bio_chain->bc_spinlock);
	bio_chain->bc_event = EVENT_NULL;
	bio_chain->bc_driver_unloading = 0;

	INM_INIT_SLEEP(&bio_chain->bc_ipc);

	pid = creatp();
	if ( pid == -1 ) {
		retval = -ENOMEM;
		goto creatp_failed;
	}

	retval = initp(pid, vdev_queue_io, NULL, 0, "io_queue_thread");
	if( retval < 0 ) {
		ERR("Could not create io queue thread");
		goto initp_failed;
	}

	/* Wait for I/O thread to be created */
	INM_SLEEP(&bio_chain->bc_ipc);
	INM_FINI_SLEEP(&bio_chain->bc_ipc);

out:
	return retval;

initp_failed:
creatp_failed:
	INM_FREE_PINNED(&bio_chain, sizeof(bio_chain_t));
	goto out;
}

void
vdev_io_exit()
{
	inm_interrupt_t intr;

	INM_INIT_SLEEP(&bio_chain->bc_ipc);

	e_wakeup_one(&bio_chain->bc_event);

	/* Wait for I/O thread to be die */
	INM_SLEEP(&bio_chain->bc_ipc);
	INM_FINI_SLEEP(&bio_chain->bc_ipc);

	INM_FREE_PINNED(&bio_chain, sizeof(bio_chain_t));
}

