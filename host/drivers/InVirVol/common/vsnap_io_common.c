
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
 * Revision:            $Revision: 1.13 $
 * Checked in by:       $Author: lamuppan $
 * Last Modified:       $Date: 2015/02/24 03:29:23 $
 *
 * $Log: vsnap_io_common.c,v $
 * Revision 1.13  2015/02/24 03:29:23  lamuppan
 * Bug #1619650, 1619648, 1619642, 1619640, 1619637, 1619636, 1619635: Fix for policheck
 * bugs for Filter and Vsnap drivers. Please refer to bugs in TFS for more details.
 *
 * Revision 1.12  2012/08/31 07:14:32  hpadidakul
 * Fixing the build issue on Solaris and AIX, issue is regresstion of bug 22914 checkin.
 * Unit tested by: Hari
 *
 * Revision 1.11  2012/08/30 08:53:23  hpadidakul
 * bug 22914: On SLES9-SP3 vsnap driver is not loading
 *
 * Unit tested by: Hari.
 * Please refer http://bugzilla/show_bug.cgi?id=22914 for more information.
 *
 * Revision 1.10  2011/01/13 06:34:52  chvirani
 * 14878: Changes to support AIX.
 *
 * Revision 1.9  2011/01/07 11:08:46  chvirani
 * 14811: Fix synchronisation problem between two threads to start queueing
 *
 * Revision 1.8  2010/12/28 07:39:49  chvirani
 * 14749: Handle return value from read_from_this_fid()
 *
 * Revision 1.7  2010/10/01 09:03:22  chvirani
 * 13869: Full disk support
 *
 * Revision 1.6  2010/07/21 10:13:36  chvirani
 * 13305: Free btree lookup results if any iteration of btree
 * lookup or target read fails.
 *
 * Revision 1.5  2010/07/05 07:34:28  chvirani
 * DBG to DEV_DBG
 *
 * Revision 1.4  2010/07/05 07:13:07  chvirani
 * 12934: Allocate bio_data only for I/O being processed.
 *             Limit max parallel I/O for reduced mem usage.
 *
 * Revision 1.3  2010/06/21 11:21:10  chvirani
 * 10967: Handle boundary conditions on solaris raw device
 *
 * Revision 1.2  2010/03/31 13:23:11  chvirani
 * o X: Convert disk offsets to inm_offset_t
 * o X: Increase/Decrease mem usage count inside lock
 *
 * Revision 1.1  2010/03/10 12:09:33  chvirani
 * initial code for multithreaded driver
 *
 * Revision 1.9  2010/02/25 13:28:12  chvirani
 * Pass bio_data->bid_flags to bio_endio() for use in solaris
 *
 * Revision 1.8  2010/02/24 10:36:21  chvirani
 * convert linux specific calls to generic calls
 *
 * Revision 1.7  2010/02/16 13:41:51  chvirani
 * start queued io within the thread instead of a separate task
 *
 * Revision 1.6  2010/02/16 13:09:55  chvirani
 * working phase 1 driver - fix for read suspension forever, write suspension forever, DI
 *
 * Revision 1.5  2010/01/21 11:33:54  chvirani
 * fix btree lookup code
 *
 * Revision 1.4  2010/01/13 14:52:49  chvirani
 * working phase 1 - fixed random io bug
 *
 * Revision 1.3  2009/12/18 10:53:05  chvirani
 * Read from target volume correctly
 *
 * Revision 1.2  2009/12/14 10:48:27  chvirani
 * go through FS cache
 *
 * Revision 1.1  2009/12/04 10:47:55  chvirani
 * phase 1 - final
 *
 * Revision 1.3  2009/12/04 10:22:34  chvirani
 * common threading functions
 *
 * Revision 1.2  2009/11/24 06:54:11  chvirani
 * working multithreaded
 *
 * Revision 1.1  2009/11/18 13:38:05  chvirani
 * multithreaded
 *
 * Revision 1.1  2009/07/28 08:34:50  chvirani
 * Working code for multithreaded io
 *
 * Revision 1.1  2008/12/30 18:21:16  chvirani
 * Multithreaded pool changes, caching and other improvements - unfinished
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
 * 	4483 - Under low memory condition, vsnap driver suspended forever
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

#include "vsnap_io_common.h"
#include "vsnap_io.h"
#include "vsnap.h"
#include "vsnap_control.h"
#include "vsnap_kernel_helpers.h"
#include "vsnap_kernel.h"

/* to facilitate ident */
static const char rcsid[] = "$Id: vsnap_io.c,v 1.1 2009/07/28 08:34:50 chvirani \
Exp $";

extern thread_pool_t	*vs_ops_pool;
extern thread_pool_t	*vv_ops_pool;
typedef void (*queue_method)(inm_vdev_t *, inm_block_io_t *);

/* Common code across Vsnap and volpack */

static void check_and_start_queued_io(inm_vdev_t *, inm_32_t );

void
io_print_info(void)
{
	DBG("Max Active BIOs = %d", INM_MAX_ACTIVE_BIO);
	DBG("Min Active BIOs = %d", INM_MIN_ACTIVE_BIO);
}

/*
 * Error processing for add_bio_to_pool.
 */
static inline void
add_bio_to_pool_error(inm_vdev_t *vdev, inm_block_io_t *bio, inm_32_t error)
{
	inm_32_t	rw = 0;

	BIO_TYPE(bio, &rw);
	/* End the bio with sent error */
	inm_bio_endio(bio, error, 0);

	/*
	 * Now start any queued I/O. check_and_start_queued_io will only start
	 * queueing new I/Os when all current inflight I/O have been processed
	 */
	check_and_start_queued_io(vdev, rw);
}


/*
 * Common function to put a serviceable bio into the task pool when a bio is
 * deemed to be eligible to be serviced. Can be called in two paths
 *
 * Queue function or End of all serviceable I/O
 */
static inm_32_t
add_bio_to_pool(inm_vdev_t * vdev, inm_block_io_t *bio, thread_pool_t *pool,
		inm_u32_t task_id)
{
	bio_data_t		*bio_data = NULL;
	vsnap_task_t		*task = NULL;
	inm_32_t		retval = 0;
	inm_u64_t		max_offset;

	/*
	 * Vsnap read i/o path starts here
	 */

	task = INM_ALLOC(sizeof(vsnap_task_t), GFP_NOIO);
	if ( !task ) {
		ERR("Cannot allocate mem for lookup task");
		goto task_alloc_failed;
	}

	bio_data = INM_ALLOC(sizeof(bio_data_t), GFP_NOIO);
	if ( !bio_data ) {
		ERR("Could not allocate mem for bio_data");
		goto bio_data_alloc_failed;
	}

	INM_ATOMIC_SET(&bio_data->bid_status, 0);
	bio_data->bid_error = 0;
	bio_data->bid_vdev = vdev;
	bio_data->bid_flags = 0;
	bio_data->bid_bi_private = BIO_PRIVATE(bio);
	BIO_PRIVATE(bio) = (void *)bio_data;
	bio_data->bid_task = task;

	max_offset = INM_DEVICE_MAX_OFFSET(vdev, bio);

	if( ( BIO_OFFSET(bio) + BIO_SIZE(bio) ) > max_offset ) {
		if( BIO_OFFSET(bio) >= max_offset ) {
			DEV_DBG("Requested block beyond disk size");
			bio_data->bid_size = 0;
		} else {
			DEV_DBG("Request extends beyond disk size");
			bio_data->bid_size = max_offset - BIO_OFFSET(bio);
		}

		DBG("Offset = %llu, length = %llu", (inm_u64_t)BIO_OFFSET(bio),
		    (inm_u64_t)BIO_SIZE(bio));
		DBG("New request size = %llu", (inm_u64_t)bio_data->bid_size);
	} else {
		bio_data->bid_size = BIO_SIZE(bio);
	}

	task->vst_task_data = (void *)bio;
	task->vst_task_id = task_id;

	ADD_TASK_TO_POOL(pool, task);

out:
	return retval;

bio_data_alloc_failed:
	INM_FREE(&task, sizeof(vsnap_task_t));

task_alloc_failed:
	add_bio_to_pool_error(vdev, bio, -ENOMEM);
	retval = -ENOMEM;
	goto out;

}

/*
 * start_queued_io_vsnap:
 * Algo:
 *	o Check the first BIO type
 *	o if write and no io inflight, mark write in progress
 *	o remove the BIO from queue
 *	o Queue the BIO into the wait queue
 *	o If read, repeat with next BIO
 *	o If write, get out
 */

void
inm_vs_start_queued_io(inm_vdev_t *vdev, inm_u32_t inflight)
{
	inm_32_t	rw;
	inm_block_io_t  *bio;
	inm_u32_t	task_id;
	inm_32_t	retval;
	inm_u32_t	io_completed;
	inm_u32_t	pending = 0;
	inm_u32_t	reloop = 1;
	inm_interrupt_t	intr;

	while( reloop ) {

		io_completed = 0;
		rw = READ;
		bio = NULL;
		task_id = INM_TASK_BTREE_LOOKUP;
		retval = 0;
		pending = 0;

		/*
		 * TODO: just queue all the runnable bios in a pvt list
		 * and release spinlock and then start the bio tasks
		 */
		while( rw == READ && inflight < INM_MAX_ACTIVE_BIO ) {

			INM_ACQ_SPINLOCK_IRQ(&vdev->vd_lock, intr);

			bio = vdev->vd_bio;
			if ( !bio ) {
				INM_REL_SPINLOCK_IRQ(&vdev->vd_lock, intr);
				break;
			}

			/*
			 * If this is a write request and if there I/O inflight
			 * or there are BIO before that are going to be
			 * queued, we end the loop there.
			 */
			BIO_TYPE(bio, &rw);
			if ( rw == WRITE ) {
				if ( !inflight ) {
					vdev->vd_write_in_progress = 1;
					task_id = INM_TASK_WRITE;
				} else {
					INM_REL_SPINLOCK_IRQ(&vdev->vd_lock,
							     intr);
					break;
				}
			}

			/*
			 * If its the last bio, mark the queue empty
			 */
			if ( bio == vdev->vd_biotail )
				vdev->vd_biotail = NULL;

			vdev->vd_bio = BIO_NEXT(bio);
			BIO_NEXT(bio) = NULL;

			INM_REL_SPINLOCK_IRQ(&vdev->vd_lock, intr);

			retval = add_bio_to_pool(vdev, bio, vs_ops_pool,
						 task_id);

			/*
			 * increment only if successful start. This will lead
			 * continue queueing the reminaing BIOs.
			 */
			if( retval == 0 )
				inflight++;
			else {
			       /* add_bio_to_pool_error() called
				* check_and_start_queued_io() which would have
				* decremented the inflight count.
				*/
				INM_ATOMIC_INC(&vdev->vd_io_inflight);
				ERR("Failed to add bio to queue");
				/*
				 * If it was WRITE, we do not want to stop
				 * so mark rw = READ to loop again
				 */
				rw = READ;
				retval = 0;
			}
		}

		/*
		 * Restore sanity to vd_io_inflight count. Need to take
		 * I/Os that may have completed while we were queueing
		 * new BIOs into account as well.
		 */
		INM_ACQ_SPINLOCK_IRQ(&vdev->vd_lock, intr);
		/* diff from original inflated value gives us io_completed */
		io_completed = INM_ATOMIC_READ(&vdev->vd_io_inflight);
		io_completed = INM_INFLATED_INFLIGHT - io_completed;

		inflight = inflight - io_completed;
		pending = INM_ATOMIC_READ(&vdev->vd_io_pending);
		/* Pending also includes inflight I/Os */
		pending = pending - inflight;

		/*
		 * There is a possibility that we need to queue more
		 * BIOs as all the BIO already inflight when we were queueing
		 * new ones or the new ones may have already been processed by
		 * now and check_and_start_queued_io() will not start another
		 * queue because of the inflated vd_in_inflight values.
		 * NOTE: rw == WRITE if a write is in progress or if the first
		 * BIO in the vsnap local queue is a write. In either scenario,
		 * inflight may be <= INM_MIN_ACTIVE_BIO
		 */
		if( !pending || /* Nothing pending or */
		    ( inflight && /* Some BIOs inflight and */
		    ( rw == WRITE || /* In case of write, only one BIO or */
		    inflight > INM_MIN_ACTIVE_BIO ) ) ) { /*READS > MIN_ACTIVE*/
			INM_ATOMIC_SET(&vdev->vd_io_inflight, inflight);
			reloop = 0;
		} else {
			INM_ATOMIC_SET(&vdev->vd_io_inflight,
				       INM_INFLATED_INFLIGHT);
		}

		INM_REL_SPINLOCK_IRQ(&vdev->vd_lock, intr);

#ifdef _VSNAP_DEBUG_
		if( pending < 0 ) {
			ERR("Complete = %u, Inflight = %u, Pending = %u",
			    io_completed, inflight, pending + inflight);

			ASSERT(1);
		}
#endif
	}

	return;
}

/*
 * start_queued_io_vsnap:
 * Algo: Queue BIO until max inflight limit reached
 *
 */
void
inm_vv_start_queued_io(inm_vdev_t *vdev, inm_u32_t inflight)
{
	inm_block_io_t  *bio;
	inm_32_t	retval;
	inm_u32_t	io_completed;
	inm_u32_t	pending = 0;
	inm_u32_t	reloop = 1;
	inm_interrupt_t	intr;

	while( reloop ) {

		io_completed = 0;
		bio = NULL;

		/*
		 * TODO: just queue all the runnable bios in a pvt list
		 * and release spinlock and then start the bio tasks
		 */
		while( inflight < INM_MAX_ACTIVE_BIO ) {

			INM_ACQ_SPINLOCK_IRQ(&vdev->vd_lock, intr);

			bio = vdev->vd_bio;
			if ( !bio ) {
				INM_REL_SPINLOCK_IRQ(&vdev->vd_lock, intr);
				break;
			}

			/*
			 * If its the last bio, mark the queue empty
			 */
			if ( bio == vdev->vd_biotail )
				vdev->vd_biotail = NULL;

			vdev->vd_bio = BIO_NEXT(bio);
			BIO_NEXT(bio) = NULL;

			INM_REL_SPINLOCK_IRQ(&vdev->vd_lock, intr);

			retval = add_bio_to_pool(vdev, bio, vv_ops_pool,
						 INM_TASK_VOLPACK_IO);

			/*
			 * increment only if successful start. This will lead
			 * continue queueing the reminaing BIOs.
			 */
			if( retval == 0 )
				inflight++;
			else {
			       /* add_bio_to_pool_error() called
				* check_and_start_queued_io() which would have
				* decremented the inflight count.
				*/
				INM_ATOMIC_INC(&vdev->vd_io_inflight);
				ERR("Failed to add bio to queue");
				retval = 0;
			}
		}

		/*
		 * Restore sanity to vd_io_inflight count. Need to take
		 * I/Os that may have completed while we were queueing
		 * new BIOs into account as well.
		 */
		INM_ACQ_SPINLOCK_IRQ(&vdev->vd_lock, intr);
		/* diff from original inflated value gives us io_completed */
		io_completed = INM_ATOMIC_READ(&vdev->vd_io_inflight);
		io_completed = INM_INFLATED_INFLIGHT - io_completed;

		inflight = inflight - io_completed;
		pending = INM_ATOMIC_READ(&vdev->vd_io_pending);
		/* Pending also includes inflight I/Os */
		pending = pending - inflight;

		/*
		 * There is a possibility that we need to queue more
		 * BIOs as all the BIO already inflight when we were queueing
		 * new ones or the new ones may have already been processed by
		 * now and check_and_start_queued_io() will not start another
		 * queue because of the inflated vd_in_inflight values.
		 */
		if( !pending || inflight > INM_MIN_ACTIVE_BIO ) {
			INM_ATOMIC_SET(&vdev->vd_io_inflight, inflight);
			reloop = 0;
		} else {
			INM_ATOMIC_SET(&vdev->vd_io_inflight,
				       INM_INFLATED_INFLIGHT);
		}

		INM_REL_SPINLOCK_IRQ(&vdev->vd_lock, intr);

#ifdef _VSNAP_DEBUG_
		if( pending < 0 ) {
			ERR("Complete = %u, Inflight = %u, Pending = %u",
			    io_completed, inflight, pending + inflight);

			ASSERT(1);
		}
#endif
	}

	return;

}


static void
check_and_start_queued_io(inm_vdev_t *vdev, inm_32_t rw)
{
	inm_32_t	delete_flag = 0;
	inm_u32_t       pending, inflight;
	inm_32_t	queue = 0;
	inm_interrupt_t	intr;

	INM_ACQ_SPINLOCK_IRQ(&vdev->vd_lock, intr);
	INM_ATOMIC_DEC_RETURN(&vdev->vd_io_pending, pending);
	INM_ATOMIC_DEC_RETURN(&vdev->vd_io_inflight, inflight);

	if ( !inflight || inflight == INM_MIN_ACTIVE_BIO ) {
		/*
		 * If there is anything pending on vsnap local queue,
		 * we call start_queued_io() which will queue bios from
		 * local to global queue. pending is inclusive of inflight
		 * I/O so is always >= inflight.
		 */
		if( pending != inflight ) { /* pending > inflight */
			/*
			 * Artificially inflate the inflight I/O num. It
			 * prevents another check_and_start_queued_io from
			 * starting another start_queued_io in parallel to us.
			 * This also helps with add_bio_to_pool_error which
			 * calls us which in turn calls start_queued_io()
			 * which may lead to an stack overflow in case
			 * subsequent add_bio_to_pool() fails.
			 */

			INM_ATOMIC_SET(&vdev->vd_io_inflight,
				       INM_INFLATED_INFLIGHT);
			queue = 1;
		} else { /* pending == inflight, nothing in local queue */
			/*
			 * No need to start queued I/O.
			 * if pending == inflight == INM_MIN_ACTIVE_BIO, we
			 * indicate there is nothing pending so that
			 * start_queued_io() is not called
			 */
			if( !inflight ) { /* nothing pending and inflight */
				if( INM_TEST_BIT(VSNAP_FLAGS_FREEING,
						 &vdev->vd_flags) )
					delete_flag = 1;
			}
		}
	}

	vdev->vd_io_complete++;

	if( rw == WRITE ) /* For volpack, this is never set */
		vdev->vd_write_in_progress = 0;

	INM_REL_SPINLOCK_IRQ(&vdev->vd_lock, intr);

	if ( queue ) { /* pending > inflight */
		DEV_DBG("%u tasks pending, %u inflight", pending, inflight);
		ASSERT(pending < inflight);
		if( INM_TEST_BIT(VSNAP_DEVICE, &vdev->vd_flags ) )
			inm_vs_start_queued_io(vdev, inflight);
		else
			inm_vv_start_queued_io(vdev, inflight);

	} else {
		if ( delete_flag ) {
			/*
			 * Here, there are  no pending/inflight I/Os
			 * and vsnap is marked for deletion, wakeup
			 * the delete thread.
			 */
			INM_WAKEUP(&vdev->vd_sem);
		}
	}
}

/*
 * inm_vdev_endio:      restores original values of bio
 *			and calls bio_endio to signal end of bio.
 *			Error, if any, indicated by bio_data->bid_error
 */
/*
 * inm_vdev_endio:      restores original values of bio
 *			and calls bio_endio to signal end of bio.
 *			Error, if any, indicated by bio_data->bid_error
 *			Btree R/W sync should happen here to maintain
 *			R/W order
 *
 * Algo:
 *	o If BIO is write, mark write in progress as done (0) so
 *	  subsequent I/O can proceed
 *	o bio_end_io()
 *
 * TODO: error code for solaris has to be positive
 */
void
inm_vdev_endio(vsnap_task_t *task)
{
	inm_block_io_t  *bio = (inm_block_io_t *)task->vst_task_data;
	bio_data_t      *bio_data = (bio_data_t *)BIO_PRIVATE(bio);
	inm_vdev_t      *vdev = bio_data->bid_vdev;
	inm_32_t	rw;

	BIO_TYPE(bio, &rw);

	/* Restore original bio size and private data */
	if( BIO_SIZE(bio) != bio_data->bid_size ) {
		INM_HANDLE_END_OF_DEVICE(vdev, bio);
	}

	/* restore original bio data and end bio */
	BIO_PRIVATE(bio) = bio_data->bid_bi_private;

	inm_bio_endio(bio, bio_data->bid_error, bio_data);

	INM_FREE(&bio_data, sizeof(bio_data_t));
	INM_FREE(&task, sizeof(vsnap_task_t));

	check_and_start_queued_io(vdev, rw);
}




/*
 * add_bio_for_write:   Modifies the bio with driver data and places
 *			it into the target queue
 */
static void
add_bio_for_write(inm_vdev_t * vdev, inm_block_io_t *bio)
{
	inm_u32_t	        bio_queued = 0;
	inm_interrupt_t	intr;

	/*
	 * If there is any inflight i/o, just queue this write bio
	 */
	INM_ACQ_SPINLOCK_IRQ(&vdev->vd_lock, intr);

	/* We are processing one write at a time. So no need to check
	 * for INM_MAX_ACTIVE_BIO.
	 */
	if ( vdev->vd_biotail ||
	     INM_ATOMIC_READ(&vdev->vd_io_inflight) ) {
		DEV_DBG("Queue write");
		if ( vdev->vd_biotail ) {
        		BIO_NEXT(vdev->vd_biotail) = bio;
	                vdev->vd_biotail = bio;
		} else {
        		vdev->vd_bio = vdev->vd_biotail = bio;
		}

		bio_queued = 1;
	} else {
		INM_ATOMIC_INC(&vdev->vd_io_inflight);
		vdev->vd_write_in_progress = 1;
	}

	INM_REL_SPINLOCK_IRQ(&vdev->vd_lock, intr);

	if ( !bio_queued )
		add_bio_to_pool(vdev, bio, vs_ops_pool, INM_TASK_WRITE);

}


/*
 * add_bio_to_target_queue:     Modifies the bio with driver data and places
 *				it into the target queue
 */
static void
add_bio_for_read(inm_vdev_t * vdev, inm_block_io_t *bio)
{
	inm_u32_t	        bio_queued = 0;
	inm_interrupt_t	intr;

	/*
	 * If there already are bios waiting in queue or there
	 * is a write in progress and its NOT update ioctl
	 * then place ourselves in the vsnap queue
	 */
	INM_ACQ_SPINLOCK_IRQ(&vdev->vd_lock, intr);

	if ( vdev->vd_biotail  ||
	     vdev->vd_write_in_progress ||
	     INM_ATOMIC_READ(&vdev->vd_io_inflight) == INM_MAX_ACTIVE_BIO ) {
		if ( vdev->vd_biotail ) {
			BIO_NEXT(vdev->vd_biotail) = bio;
			vdev->vd_biotail = bio;
		} else {
			vdev->vd_bio = vdev->vd_biotail = bio;
		}

		bio_queued = 1;
	} else {
		INM_ATOMIC_INC(&vdev->vd_io_inflight);
	}

	INM_REL_SPINLOCK_IRQ(&vdev->vd_lock, intr);

	if ( !bio_queued )
		add_bio_to_pool(vdev, bio, vs_ops_pool, INM_TASK_BTREE_LOOKUP);

}

/*
 * add_bio_to_target_queue:     Modifies the bio with driver data and places
 *				it into the target queue
 */
static void
add_bio_for_volpack(inm_vdev_t * vdev, inm_block_io_t *bio)
{
	inm_u32_t	        bio_queued = 0;
	inm_interrupt_t	intr;

	/*
	 * If there already are bios waiting in queue or there
	 * is a write in progress and its NOT update ioctl
	 * then place ourselves in the vsnap queue
	 */
	INM_ACQ_SPINLOCK_IRQ(&vdev->vd_lock, intr);

	if ( vdev->vd_biotail ||
	     INM_ATOMIC_READ(&vdev->vd_io_inflight) == INM_MAX_ACTIVE_BIO ) {
		if ( vdev->vd_biotail ) {
			BIO_NEXT(vdev->vd_biotail) = bio;
			vdev->vd_biotail = bio;
		} else {
			vdev->vd_bio = vdev->vd_biotail = bio;
		}

		bio_queued = 1;
	} else {
		INM_ATOMIC_INC(&vdev->vd_io_inflight);
	}

	INM_REL_SPINLOCK_IRQ(&vdev->vd_lock, intr);

	if ( !bio_queued )
		add_bio_to_pool(vdev, bio, vv_ops_pool, INM_TASK_VOLPACK_IO);

}

/*
 * inm_vs_make_request
 *
 * DESC
 *      validate the status of the device & add the bio to the queue
 *      for each io request, try and acquire the io_mutex so that any
 *      uninit request sleeps on it and is awoken when io_pending
 *      drops to zero
 */
inm_32_t
inm_vs_generic_make_request(inm_vdev_t *vdev, inm_block_io_t *bio)
{
	inm_32_t	rw;
	inm_u32_t	delete_flag = 0;
	queue_method	queue_bio = NULL;
	inm_interrupt_t	intr;

	ASSERT(!vdev);

        INM_ACQ_SPINLOCK_IRQ(&vdev->vd_lock, intr);
        INM_ATOMIC_INC(&vdev->vd_io_pending);
	if ( INM_TEST_BIT(VSNAP_FLAGS_FREEING, &vdev->vd_flags) )
		delete_flag = 1;
        INM_REL_SPINLOCK_IRQ(&vdev->vd_lock, intr);

	if ( delete_flag )
		goto err;

	BIO_NEXT(bio) = NULL;
	BIO_TYPE(bio, &rw);

	switch(rw) {
	    case WRITE:
		DEV_DBG("write");
		queue_bio = add_bio_for_write;
		break;

	    case READ:
		DEV_DBG("read");
		queue_bio = add_bio_for_read;
		break;
	}

	DEV_DBG("add %p bio to queue", bio);
	queue_bio(vdev, bio);

  out:
	return 0;

  err:
	ERR("Cannot process I/O");
	INM_ACQ_SPINLOCK_IRQ(&vdev->vd_lock, intr);

	/*
	 * If we are here, it means delete flag has been set.
	 * If there are pending BIOs, unset the delete flag
	 */
	if ( !INM_ATOMIC_DEC_AND_TEST(&vdev->vd_io_pending) ) {
		delete_flag = 0;
	}

	INM_REL_SPINLOCK_IRQ(&vdev->vd_lock, intr);

	inm_bio_endio(bio, -EIO, 0);

	if ( delete_flag )
		INM_WAKEUP(&vdev->vd_sem);

	goto out;
}

/*
 * inm_vv_make_request
 *
 * DESC
 *      validate the status of the device & add the bio to the queue
 *      for each io request, try and acquire the io_mutex so that any
 *      uninit request sleeps on it and is awoken when io_pending
 *      drops to zero
 */
inm_32_t
inm_vv_generic_make_request(inm_vdev_t *vdev, inm_block_io_t *bio)
{
	inm_u32_t	delete_flag = 0;
	inm_interrupt_t	intr;

	ASSERT(!vdev);

        INM_ACQ_SPINLOCK_IRQ(&vdev->vd_lock, intr);
        INM_ATOMIC_INC(&vdev->vd_io_pending);
	if ( INM_TEST_BIT(VSNAP_FLAGS_FREEING, &vdev->vd_flags) )
		delete_flag = 1;

	INM_REL_SPINLOCK_IRQ(&vdev->vd_lock, intr);

	if( delete_flag )
		goto err;

	BIO_NEXT(bio) = NULL;

	DEV_DBG("add %p bio to queue", bio);
	add_bio_for_volpack(vdev, bio);

  out:
	return 0;

  err:
	ERR("Cannot process I/O");
	INM_ACQ_SPINLOCK_IRQ(&vdev->vd_lock, intr);

	/*
	 * If we are here, it means delete flag has been set.
	 * If there are pending BIOs, unset the delete flag
	 */
	if ( !INM_ATOMIC_DEC_AND_TEST(&vdev->vd_io_pending) ) {
		delete_flag = 0;
	}

	INM_REL_SPINLOCK_IRQ(&vdev->vd_lock, intr);

	inm_bio_endio(bio, -EIO, 0);

	if ( delete_flag )
		INM_WAKEUP(&vdev->vd_sem);

	goto out;
}

/* Volpack Handlers */
/*
 * handle_this_bio
 *
 * DESC
 *      process the bio & invoke the bio_endio on completion of the processing
 */
void
inm_vv_do_volpack_io(vsnap_task_t *task)
{
	inm_block_io_t  *bio = task->vst_task_data;
	bio_data_t	*bio_data = (bio_data_t *)BIO_PRIVATE(bio);
        inm_offset_t	pos = BIO_OFFSET(bio);
	inm_32_t	rw;

	if( bio_data->bid_size != 0 ) {
		BIO_TYPE(bio, &rw);
		if ( rw == WRITE) {
			bio_data->bid_error =
				handle_bio_for_write_to_vv(bio_data->bid_vdev,
							   bio, pos);
		} else {
			bio_data->bid_error =
				handle_bio_for_read_from_vv(bio_data->bid_vdev,
							    bio, pos);
		}
	}

	inm_vdev_endio(task);
}

void
handle_bio_for_vsnap_write_common(vsnap_task_t *task)
{
	inm_block_io_t  *bio = task->vst_task_data;
	bio_data_t	*bio_data = (bio_data_t *)BIO_PRIVATE(bio);

	if( bio_data->bid_size != 0 )
		bio_data->bid_error = handle_bio_for_write_to_vsnap(task);

	inm_vdev_endio(task);
}

/* Vsnap Handlers */
/*
 * inm_vs_read_from_retention_file:     This is called by the worker thread_pool
 *					for actual retention io.
 */
void
inm_vs_read_from_retention_file(vsnap_task_t *task)
{
	ret_io_t	*ret_io;
	inm_block_io_t  *bio;
	struct bio_data *bio_data;
	inm_32_t	retval = 0;

	ret_io = (ret_io_t *)task->vst_task_data;
	bio = ret_io->ret_bio;
	bio_data = (bio_data_t *)BIO_PRIVATE(bio);

	/*
	 * Free up io sub task. Main task associated with this bio
	 * will be freed in end_bio
	 */
	INM_FREE(&task, sizeof(vsnap_task_t));

	DEV_DBG("reading from fid = %d", ret_io->ret_fid);
	retval = read_from_this_fileid(bio, bio_data, ret_io);
	/*
	 * In case of error, just set bio_data->bid_error
	 * which is taken care of in end_bio
	 */
	if ( retval < 0 ) {
		ERR("Retention IO error");
		bio_data->bid_error = retval;
	}

	INM_FREE(&ret_io, sizeof(ret_io_t));

	/*
	 * If all associated io tasks are over i.e. bid_status == 0
	 * call end_io for this bio
	 */
	if ( INM_ATOMIC_DEC_AND_TEST(&bio_data->bid_status) ) {
		DEV_DBG("All IOs done ... end bio");

		/*
		 * Get the original task associated with this bio to call end_io
		 */
		task = bio_data->bid_task;
		inm_vdev_endio(task);
	}
}


/*
 * handle_bio_for_read :	lookup entire bio range. Results for lookup are
 *				stored in bio_data->bid_ret_data. At end of
 *				lookup, if target io is already done, go for
 *				retention processing.
 */
void
handle_bio_for_read_from_vsnap(vsnap_task_t *task)
{
	inm_32_t 	retval = 0;
	inm_block_io_t	*bio = (inm_block_io_t*)task->vst_task_data;
	bio_data_t	*bio_data = (bio_data_t*)BIO_PRIVATE(bio);
	inm_offset_t	disk_offset = BIO_OFFSET(bio);
	inm_vdev_t 	*vdev = bio_data->bid_vdev;
	inm_vs_info_t	*vs_info = (inm_vs_info_t *)vdev->vd_private;
	inm_vs_parent_list_t *parent = vs_info->vsi_parent_list;
	inm_vs_offset_split_t	*offset_split = NULL;
	single_list_entry_t	*found_keys_list_head;

	if( bio_data->bid_size == 0 ) {
		inm_vdev_endio(task);
		goto out;
	}

	INM_READ_LOCK(&parent->pl_target_io_done);

	LOOKUP_RET_DATA_INIT(&bio_data->bid_ret_data);
	found_keys_list_head = &(bio_data->bid_ret_data.lrd_ret_data);

	INM_READ_LOCK(vs_info->vsi_btree_lock);

	retval = vdev->vd_rw_ops.read(vdev->vd_private, disk_offset,
				      bio_data->bid_size,
				      &bio_data->bid_ret_data);

	INM_READ_UNLOCK(vs_info->vsi_btree_lock);

	if( retval < 0 ) {
		ERR("Error in lookup");
		INM_READ_UNLOCK(&parent->pl_target_io_done);
		ASSERT(!INM_LIST_IS_EMPTY(found_keys_list_head));
		goto error;
	} else {
		if ( bio_data->bid_ret_data.lrd_target_read ) {
			retval = read_from_target_volume(task);
			DEV_DBG("Read from target device");
		} else {
			DEV_DBG("No need for target device");
		}

		INM_READ_UNLOCK(&parent->pl_target_io_done);

		if( retval < 0 ) {
			ERR("Cannot read from target device");
			goto error;
		}

		if ( INM_LIST_IS_EMPTY(&(bio_data->bid_ret_data.lrd_ret_data)) )
			inm_vdev_endio(task);
		else
			inm_vs_read_from_retention(task);
	}

out:
	return;

error:
	/* Free the keys returned by lookup */
	DBG("Freeing lookup results");
	while (!INM_LIST_IS_EMPTY(found_keys_list_head)) {
		offset_split = (inm_vs_offset_split_t *)
		INM_LIST_POP_ENTRY(found_keys_list_head);
		DBG("Free %p", offset_split);
		INM_FREE(&offset_split, sizeof(inm_vs_offset_split_t));
	}

	bio_data->bid_error = retval;
	inm_vdev_endio(task);

}

