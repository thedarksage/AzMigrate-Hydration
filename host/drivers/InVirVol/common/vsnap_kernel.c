
/*****************************************************************************
 * vsnap_kernel.c
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
 * Description:         Main File containing the actual Vsnap routines
 *
 * Revision:            $Revision: 1.23 $
 * Checked in by:       $Author: lamuppan $
 * Last Modified:       $Date: 2015/02/24 03:29:23 $
 *
 * $Log: vsnap_kernel.c,v $
 * Revision 1.23  2015/02/24 03:29:23  lamuppan
 * Bug #1619650, 1619648, 1619642, 1619640, 1619637, 1619636, 1619635: Fix for policheck
 * bugs for Filter and Vsnap drivers. Please refer to bugs in TFS for more details.
 *
 * Revision 1.22  2014/11/17 12:59:48  lamuppan
 * TFS Bug #1057085: Fixed the issue by fixing the STR_COPY function. Please
 * go through the bug for more details.
 *
 * Revision 1.21  2014/03/14 12:49:28  lamuppan
 * Changes for lists, kernel helpers.
 *
 * Revision 1.20  2013/12/20 06:03:07  lamuppan
 * Bug #27488: In inm_vs_call_back, we are copying field by field for the keys
 * and in SOlaris-11-x86, movdqu instruction is generated where the system was
 * crashing. Instead used memecpy to avoid the generation of movdqu.
 *
 * Instead of passing odd boundary for reading the FID file, passing a newly
 * allocated one.
 *
 * Revision 1.19  2011/01/18 09:34:24  chvirani
 * chage DBG to DEV_DBG to prevent syslog issues if write happens on RO vsnap
 *
 * Revision 1.18  2011/01/13 06:34:52  chvirani
 * 14878: Changes to support AIX.
 *
 * Revision 1.17  2011/01/11 05:42:35  chvirani
 * 14377: Changes to support linux kernel 2.6.32 upwards
 *
 * Revision 1.16  2010/10/19 04:52:29  chvirani
 * 14174: Raw device no longer required
 *
 * Revision 1.15  2010/10/12 11:26:39  chvirani
 * Close raw device on deletion
 *
 * Revision 1.14  2010/10/01 09:03:22  chvirani
 * 13869: Full disk support
 *
 * Revision 1.13  2010/07/21 10:13:36  chvirani
 * 13305: Free btree lookup results if any iteration of btree
 * lookup or target read fails.
 *
 * Revision 1.12  2010/07/05 07:13:07  chvirani
 * 12934: Allocate bio_data only for I/O being processed.
 *             Limit max parallel I/O for reduced mem usage.
 *
 * Revision 1.11  2010/06/17 12:45:14  chvirani
 * 12543: Sparse - Do not insert pvt file into cache. Do not update header during initial population.
 *
 * Revision 1.10  2010/06/10 14:20:21  chvirani
 * Convert 64bit divisions t odo_div(). Corrected u64 assignment.
 *
 * Revision 1.9  2010/06/09 10:10:04  chvirani
 * Support for sparse retention
 *
 * Revision 1.8  2010/04/01 14:15:01  chvirani
 * 11305: Take write lock for vsnaps without retention enabled
 *
 * Revision 1.7  2010/03/31 13:23:11  chvirani
 * o X: Convert disk offsets to inm_offset_t
 * o X: Increase/Decrease mem usage count inside lock
 *
 * Revision 1.6  2010/03/19 12:53:04  hpadidakul
 *
 * Changes is fix build issue in Solaris 8.
 * Changed "inline" to "INLINE" in file common/vsnap_kernel.c & solaris/vsnap_io.c.
 *
 * Revision 1.5  2010/03/10 12:08:43  chvirani
 * Multithreaded Driver
 *
 * Revision 1.4  2009/08/07 13:39:57  chvirani
 * 9451: Open the target device before creating parent struct toprevent access
 *       of freed memory in case of open() failure
 *
 * Revision 1.3  2009/07/20 11:04:01  chvirani
 * 9139: Correct volpack ioctl handling args
 * -> Remove excessive debug logging
 *
 * Revision 1.2  2009/07/09 06:21:23  chvirani
 * Reduce excessive debug logging
 *
 * Revision 1.1  2009/04/28 12:34:38  cswarna
 * Adding these files as part of vsnap driver code re-org
 *
 * Revision 1.12  2008/09/23 13:14:56  chvirani
 * fix for 6512 - Target OS crash after removing the replication pairs
 *
 * Revision 1.11  2008/08/12 22:27:23  praveenkr
 * fix for #5404: Decoupled the mount point away from creation of vsnap
 * Moved the decision on the device name away from driver into the CX
 * & 'cdpcli'
 *
 * Revision 1.10  2008/06/26 20:46:20  praveenkr
 * fix for #5492 - vsnap crash for no-retention replication pair
 * bad/wrong order of freeing of memory
 *
 * Revision 1.9  2008/04/01 13:03:40  praveenkr
 * fix for bug 5133 -
 * 	Target machine getting rebooted after taken scheduled vsnaps
 *
 * Revision 1.8  2008/03/25 12:59:52  praveenkr
 * code cleanup for Solaris
 *
 * Revision 1.7  2008/01/28 11:10:17  praveenkr
 * fixed possible memory corruption while NULL ending strings
 *
 * Revision 1.6  2007/12/04 12:53:38  praveenkr
 * fix for the following defects -
 * 	4483 - Under low memory condition, vsnap driver suspended forever
 * 	4484 - vsnaps & dataprotection not responding
 * 	4485 - vsnaps utilizing more than 90% CPU
 *
 * Revision 1.5  2007/09/21 14:08:07  praveenkr
 * rectified the build break (due to previous checkin)
 *
 * Revision 1.4  2007/09/21 14:04:22  praveenkr
 * fix for 4087 - Target agent got stopped after 3 days on Longterm setup
 *                  End to End RHEL4 U4 32bit
 *
 * earlier, the following mem allocations were not freed:
 *      1. a 2K filename ptr for every vsnap created (dr_vs_mount)
 *      2. a series of 8-byte list_head ptrs created to break the update
 *           buffers into chunks of pages (dr_vs_update_for_parent)
 * (checkin into trunc as by mistake checked into 4-0)
 *
 * Revision 1.3  2007/09/14 13:11:16  sudhakarp
 * Merge from DR_SCOUT_4-0_DEV/DR_SCOUT_4-1_BETA2 to HEAD ( Onbehalf of RP).
 *
 * Revision 1.1.2.20  2007/09/10 10:39:39  praveenkr
 * fix for 4062: dataprotection has crashed
 * fixed overflow while updating the user space buffer
 *
 * Revision 1.1.2.19  2007/09/04 08:30:00  praveenkr
 * fix for 4004: update ioctl failing
 * re-initialized a local variable
 *
 * Revision 1.1.2.18  2007/08/29 10:01:09  praveenkr
 * fix for 4019: Issues related to Virtual Snapshots [Driver Side]
 *
 * Revision 1.1.2.17  2007/08/24 15:32:20  praveenkr
 * fix for 4004
 * was updating errored data in update ioctl path; now breaking i/p data into
 * sequence of pages & updating the vsnap
 *
 * Revision 1.1.2.16  2007/08/22 13:14:37  praveenkr
 * toggled the error return value for dr_vs_write_using_map &
 * dr_vs_read_data_using_map
 *
 * Revision 1.1.2.15  2007/08/21 15:20:56  praveenkr
 * 1. replaced all kfrees with a wrapper dr_vdev_kfree, which will force null
 *    the ptr, to easily capture mem leaks/mem corruption
 * 2. toggled the error retrun value for dr_vs_write_using_map &
 *    dr_vs_read_data_using_map as kernel expects 0 for success & non-zero
 *    on failure
 * 3. added comments
 *
 * Revision 1.1.2.14  2007/08/17 14:26:09  praveenkr
 * fix for 3834: Apply track log is failed, read write drive is corrupted while
 * applying track logs
 * when the ext3 block size is not default(4K) the way page_data.bvec_offset was
 * handled was in err. incidently this same could cause serious consistency &
 * possible crash scenario. fixed the wrong updates on the page_data.bvec_offset
 *
 * Revision 1.1.2.13  2007/08/02 07:40:55  praveenkr
 * fix for 3829 - volpack is also listed when listing vsnaps from cdpcli
 * maintaining a seperate list for volpacks
 *
 * Revision 1.1.2.12  2007/07/26 13:08:33  praveenkr
 * fix for 3788: vsnap of diff target showing same data
 * replaced strncmp with strcmp
 *
 * Revision 1.1.2.11  2007/07/24 12:58:30  praveenkr
 * virtual volume (volpack) implementation
 *
 * Revision 1.1.2.10  2007/07/17 13:53:25  praveenkr
 * clean-up - unix-style naming, tab-spacing = 8, 78 col
 *
 * Revision 1.1.2.9  2007/05/25 13:05:23  praveenkr
 * deleted unused functions - left behind during porting
 * VsnapAlignedRead
 *
 * Revision 1.1.2.8  2007/05/25 12:53:41  praveenkr
 * deleted unused functions - left behind during porting
 * VsnapGetVolumeInformation, VsnapGetVolumeInformationLength,
 * VsnapSetOrResetTracking
 *
 * Revision 1.1.2.7  2007/05/25 09:45:44  praveenkr
 * added comments & partial clean-up of code
 *
 * Revision 1.1.2.6  2007/05/16 14:45:50  praveenkr
 * Fix for 2984:
 * IOCTLs to retrieve the context info of all vsnaps for a retention path
 *
 * Revision 1.1.2.5  2007/05/01 13:01:59  praveenkr
 * #3103: removed the reference to FSData file
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

#include "vsnap_kernel.h"
#include "vsnap_common.h"
#include "vsnap_control.h"
#include "vdev_proc_common.h"
#include "vdev_mem_debug.h"
#include "vdev_thread_common.h"
#include "vsnap_io_common.h"

/* to facilitate ident */
static const char rcsid[] = "$Id: vsnap_kernel.c,v 1.23 2015/02/24 03:29:23 lamuppan Exp $";

inm_list_head_t *parent_vollist_head = NULL;
inm_rwlock_t *parent_vollist_lock = NULL;

inm_list_head_t        	*inm_vs_list_head;      /* list of vsnaps created */
inm_rwlock_t     	inm_vs_list_rwsem;      /* held while processing vsnap
                                                   related ioctls */
inm_list_head_t        	*inm_vv_list_head;      /* list of vvols created */
inm_rwlock_t     	inm_vv_list_rwsem;      /* held while processing
                                                   volpack related ioctls */
inm_spinlock_t		inm_vs_minor_list_lock;
inm_spinlock_t		inm_vv_minor_list_lock;

inm_major_t		inm_vs_major;
inm_major_t		inm_vv_major;

/* vsnap r/w ops */
struct inm_vdev_rw_ops vs_rw_ops = {
        .read = inm_vs_read_from_vsnap,
        .write = inm_vs_write_to_vsnap
};

struct inm_vdev_rw_ops vv_rw_ops = {
	.read = read_from_volpack,
	.write = write_to_volpack
};

#define IS_FID_IN_RETENTION_PATH(FID)					\
	(((FID) & (VSNAP_RETENTION_FILEID_MASK))? 0:1)

extern thread_pool_info_t	*vs_update_pool_info;
extern thread_pool_t		*vs_update_pool;

/* Used for sparse key creation */
int sparse_vector[3][2] = {
			{SPARSE_FILE_TYPE_OFFSET, SPARSE_FILE_TYPE_LEN},
			{SPARSE_START_TIME_OFFSET, SPARSE_START_TIME_LEN},
			{SPARSE_MAX_SEQ_NUM_OFFSET, SPARSE_MAX_SEQ_NUM_LEN},
		};

int sparse_vector_size = 3;

int sparse_end_time_vector[1][2] = {
			{SPARSE_END_TIME_OFFSET, SPARSE_END_TIME_LEN}
		};

int sparse_start_time_vector[1][2] = {
			{SPARSE_START_TIME_OFFSET, SPARSE_START_TIME_LEN_NOBC}
		};

int sparse_time_vector_size = 1;

static inm_u64_t
inm_convert_to_yymmddhh(inm_u64_t nanosecs)
{
	inm_u64_t units = nanosecs;
	inm_u64_t temp;
	inm_u32_t hour = 0;
	inm_u32_t day = 0;
	inm_u32_t month = 0;
	inm_u32_t year = 0;
	inm_u32_t leapCount = 0;
	inm_u32_t yearDay = 0;

	// Time is now in seconds
	do_div(units, IntervalsPerSecond);

	// How many seconds remain after days are subtracted off
	// Note: no leap seconds!

	temp = (inm_u64_t)do_div(units, SecondsPerDay);
	day = (inm_u32_t)units;
	units = temp;

	// Calculate hh:mm:ss from seconds
	do_div(units, SecondsPerHour);
	hour = (inm_u32_t)units;

	// Compute year, month, and day
	leapCount = ( 3 * ((4 * day + 1227) / DaysPerFourHundredYears) + 3 ) / 4;
	day += 28188 + leapCount;
	year = ( 20 * day - 2442 ) / ( 5 * DaysPerNormalFourYears );
	yearDay = day - ( year * DaysPerNormalFourYears ) / 4;
	month = ( 64 * yearDay ) / 1959;

	day = yearDay - (1959 * month) / 64;

	// Resulting month based on March starting the year.
	// Adjust for January and February by adjusting year in tandem.
	if( month < 14 ) {
		month = month - 1;
		year = year + 1524;
	} else {
		month = month - 13;
		year = year + 1525;
	}

	year = year - SPARSE_BASE_YEAR;

	units = (inm_u64_t)(((((year * 100) + month) * 100) + day) * 100) + hour;

	return units;
}

/*
 * bt_uninit
 *
 * DESC
 *      vsnap btree uninitialization and frees the memory.
 */
static void
bt_uninit(inm_bt_t **btree)
{
	inm_bt_dtr(*btree);
	INM_FREE(btree, sizeof(inm_bt_t));
	*btree = NULL;
}

/*
 * init_bt_from_filp
 *
 * DESC
 *      This function allocates and initializes btree instance from a
 *      btree map file handle.
 */
static inm_32_t
init_bt_from_filp(void *bt_map_filp, inm_bt_t **btree)
{
	inm_32_t retval = 0;

	if (!bt_map_filp) {
		ERR("bt_map_filp is NULL");
		retval = -ENXIO;
		goto out;
	}

	DEV_DBG("initialize the btree instance for a given key size");
        (*btree) = inm_bt_ctr(sizeof(inm_bt_key_t));
	DEV_DBG("init the btree for the map file");
        retval = inm_vs_init_bt_from_map(*btree, bt_map_filp);
	if (retval < 0) {
		ERR("btree init from map failed.");
		goto btree_init_failed;
	}

  out:
	return retval;

  btree_init_failed:
	bt_uninit(btree);
	goto out;
}

/*
 * inm_vs_insert_offset_split
 *
 * DESC
 *      allocates one inm_vs_offset_split_t structure and inserts into the
 *      given list_head
 */
inm_32_t
inm_vs_insert_offset_split(single_list_entry_t* list_head, inm_u64_t offset,
			  inm_u32_t len, inm_u32_t file_id,
			  inm_u64_t file_offset, inm_u32_t tracking_id)
{
    	inm_32_t retval = 0;
  	inm_vs_offset_split_t *offset_split = NULL;

	offset_split = INM_ALLOC(sizeof(inm_vs_offset_split_t), GFP_NOIO);
	if (!offset_split) {
		ERR("no mem");
                retval = -ENOMEM;
                goto out;
	}

	offset_split->os_offset = offset;
	offset_split->os_len = len;
	offset_split->os_file_offset = file_offset;
	offset_split->os_fid = file_id;
	offset_split->os_tracking_id = tracking_id;
	INM_LIST_PUSH_ENTRY(list_head, (single_list_entry_t*)offset_split);

  out:
	return retval;
}

/*
 * add_fid_to_table
 *
 * DESC
 *      adds new filename->file id translation information to the FileIdTable
 */
static inm_32_t
add_fid_to_table(inm_vs_info_t *vsnap_info, inm_vs_fid_table_t *fid_table)
{
	inm_32_t retval = 0;
	inm_u32_t bytes_written = 0;
	inm_u64_t distance_moved = 0;

	retval = INM_SEEK_FILE_END(vsnap_info->vsi_fid_table_filp,
				   &distance_moved);
	if (retval < 0) {
		ERR("seek file failed. - %d", retval);
		goto out;
	}

	retval = INM_WRITE_FILE(vsnap_info->vsi_fid_table_filp, fid_table,
				distance_moved, sizeof(inm_vs_fid_table_t),
				&bytes_written);
	if (retval < 0) {
		ERR("write failed - %d", retval);
	}

  out:
	return retval;
}

/*
 * read_from_parent_volume
 *
 * DESC
 *      Generic function used by the Vsnap module to read data from the volume
 *      using Raw Volume Access method
 */
INLINE inm_32_t
read_from_parent_volume(void *file_handle, inm_vdev_page_data_t *page_data)
{
	inm_32_t retval = 0;

	retval = inm_vdev_read_page(file_handle, page_data);

	return retval;
}


/*
 * update_bt_map_for_writes
 *
 * DESC
 *      Update the btree map with the new writes
 *      (called in write path)
 *
 * ALGO
 *      1. initialize the call back
 *      2. initalize a offset-split list, to queue the split keys
 *      3. loop until the list empty
 *      4.   insert each key into the btree (inm_vs_bt_insert)
 */
static inm_32_t
update_bt_map_for_writes(inm_bt_t *btree, inm_bt_key_t *key)
{
	inm_32_t		retval = 0;
	enum BTREE_STATUS	status = BTREE_SUCCESS;
	single_list_entry_t	dirty_block_head;
	inm_vs_offset_split_t	*offset_split = NULL;

	/*
	 * set the call back related info
	 */
	DEV_DBG("set call back info");
	INM_SET_CALLBACK(btree, &inm_vs_callback);
	INM_SET_CALLBACK_PARAM(btree, (void *)&dirty_block_head);

	/*
	 * initialize an offset-split list. to queue new keys that might arise
	 * due to splitting of this key (eg, if this key overlaps with an
	 * existing ones). queue grows thro call_back
	 */
	INM_LIST_INIT(&dirty_block_head);
	retval = inm_vs_insert_offset_split(&dirty_block_head, key->key_offset,
					    key->key_len, key->key_fid,
					    key->key_file_offset,
					    key->key_tracking_id);
	if (retval < 0) {
		ERR("insert offset split failed.");
		goto out;
	}

	/*
	 * loop thro' the list until empty
	 */
	while (1) {
		if (INM_LIST_IS_EMPTY(&dirty_block_head))
			break;

		offset_split = (inm_vs_offset_split_t *)
		    INM_LIST_POP_ENTRY(&dirty_block_head);

		/*
		 * Insertion with overwrite
		 */
		status =
		    inm_vs_bt_insert(btree,
				     (inm_bt_key_t *)&offset_split->os_offset,
				     1);
		if (BTREE_FAIL == status) {
			ERR("inm_vs_bt_insert failed.\n");
			retval = -EIO;
			break;
		}
		INM_FREE(&offset_split, sizeof(inm_vs_offset_split_t));
		offset_split = NULL;	/* in release build there is no
					   double free check */
	}

	while (!INM_LIST_IS_EMPTY(&dirty_block_head)) {
		offset_split = (inm_vs_offset_split_t *)
		    INM_LIST_POP_ENTRY(&dirty_block_head);
		if (offset_split)
			INM_FREE(&offset_split, sizeof(inm_vs_offset_split_t));
	}

  out:
	return retval;
}

/*
 * update_btree_map
 *
 * DESC
 *      Update the btree map with the new writes
 *      (called in the UPDATE IOCTL path)
 * ALGO
 *      1. initialize the call back
 *      2. initalize a offset-split list, to queue the split keys
 *      3. loop until the list empty
 *      4.   insert each key into the btree (inm_vs_bt_insert)
 *      5. if the data was in a new ret-log file then update the fid table
 */
static inm_32_t
update_btree_map(inm_vs_info_t *vsnap_info, inm_u64_t vol_offset,
		 inm_u32_t len, inm_u64_t file_offset, char *file_name,
                 inm_u32_t newfid, inm_32_t fididx)
{
	inm_32_t		retval = 0;
	inm_32_t		insert_fid = 0;
	inm_bt_t		*btree = NULL;
	single_list_entry_t	dirty_block_head;
	enum BTREE_STATUS	status = BTREE_SUCCESS;
	inm_vs_offset_split_t	*offset_split = NULL;

	btree = vsnap_info->vsi_btree;

	DEV_DBG("New FId = %u", newfid);
	DEV_DBG("set the call back & its param");
	INM_SET_CALLBACK(btree, &inm_vs_callback);
	INM_SET_CALLBACK_PARAM(btree, &dirty_block_head);
	INM_LIST_INIT(&dirty_block_head);

	/*
	 * this list is modified (mostly added) by the callback function
	 * whenever an overlap was found while trying to update the btree
	 */
	retval = inm_vs_insert_offset_split(&dirty_block_head, vol_offset, len,
					    newfid, file_offset, 0);
	if (retval < 0) {
		ERR("insert offset split failed");
		goto insert_offset_failed;
	}

	/*
	 * run through the entire set offsets in the offset_split list. insert
	 * the (offset,len) tuple into the btree
	 */
	while (1) {
		if (INM_LIST_IS_EMPTY(&dirty_block_head))
			break;

		/*
		 * pop one entry from the offset split list. this list is
		 * updated by the callback function, called within the insert
		 * code path
		 */
		offset_split = (inm_vs_offset_split_t *)
		    INM_LIST_POP_ENTRY(&dirty_block_head);
		status =
		    inm_vs_bt_insert(btree,
				     (inm_bt_key_t *)&offset_split->os_offset,
				     0);
		if (BTREE_FAIL == status) {
			ERR("inm_vs_bt_insert failed.\n");
			retval = -EIO;
			break; /* not exiting function due to clean up */
		}
		if (status == BTREE_SUCCESS_MODIFIED) {
			/*
			 * a new file has been used and the fid table
			 * has to be updated
			 */
			insert_fid = 1;
		}
		INM_FREE(&offset_split, sizeof(inm_vs_offset_split_t));
		offset_split = NULL;	/* in release build there is no
					   double free check */
	}

	while (!INM_LIST_IS_EMPTY(&dirty_block_head)) { /* clean up */
		offset_split = (inm_vs_offset_split_t *)
		    INM_LIST_POP_ENTRY(&dirty_block_head);
		if (offset_split)
			INM_FREE(&offset_split, sizeof(inm_vs_offset_split_t));
	}

	DEV_DBG("Insert = %d, newfid = %u, vsnap_info->vsi_next_fid = %u,"
		"vsnap_info->vsi_next_fid_used = %u", insert_fid, newfid,
		vsnap_info->vsi_next_fid, vsnap_info->vsi_next_fid_used);

	if ( insert_fid && newfid == vsnap_info->vsi_next_fid &&
	    !vsnap_info->vsi_next_fid_used ) {
		inm_vs_fid_table_t *fid_table;

		DEV_DBG("need to insert the fid into the table");
		fid_table = INM_ALLOC(sizeof(inm_vs_fid_table_t), GFP_NOIO);
		if (fid_table) {
			STR_COPY(fid_table->ft_filename, file_name, 50);
			retval = add_fid_to_table(vsnap_info, fid_table);
			if (retval < 0) {
				ERR("add_fid 2 table failed");
			}
			vsnap_info->vsi_next_fid_used = 1;
			if( vsnap_info->vsi_set_file_fid ) {
				vsnap_info->vsi_set_file_fid(vsnap_info, newfid,
							     fididx);
			}

			INM_FREE(&fid_table, sizeof(inm_vs_fid_table_t));
		} else {
			ERR("no mem");
			retval = -ENOMEM;
		}
	}

	vsnap_info->vsi_btree_root_offset = INM_ROOT_NODE_OFFSET(btree);

insert_offset_failed:
	INM_RESET_CALLBACK(btree);
	INM_RESET_CALLBACK_PARAM(btree);
	return retval;
}

/*
 * alloc_insert_parent
 *
 * DESC
 *      creates new parent vol structure, populates and adds to the global list
 */
static inm_32_t
alloc_insert_parent(char *parent_path, inm_vs_info_t *vs_info,
		    char *retention_path)
{
	inm_32_t		retval = 0;
	inm_vs_parent_list_t	*new_parent_entry = NULL;

	new_parent_entry = INM_ALLOC(sizeof(inm_vs_parent_list_t), GFP_KERNEL);
	if (!new_parent_entry) {
		ERR("no mem.");
		retval = -ENOMEM;
		goto out;
	}
	new_parent_entry->pl_sect_size = 0;
	new_parent_entry->pl_devpath = INM_ALLOC(strlen(parent_path) + 1,
						 GFP_KERNEL);
	if (!new_parent_entry->pl_devpath) {
		ERR("no mem.");
		retval = -ENOMEM;
		goto parent_path_alloc_failed;
	}
	STR_COPY(new_parent_entry->pl_devpath, parent_path,
		    strlen(parent_path) + 1);

	new_parent_entry->pl_retention_path =
	    INM_ALLOC(strlen(retention_path) + 1, GFP_KERNEL);
	if (!new_parent_entry->pl_retention_path) {
		ERR("no mem.");
		retval = -ENOMEM;
		goto ret_path_alloc_failed;
	}
	STR_COPY(new_parent_entry->pl_retention_path, retention_path,
		    strlen(retention_path) + 1);

	new_parent_entry->pl_parent_change_lock =
	    INM_ALLOC(sizeof(inm_rwlock_t), GFP_KERNEL);
	if (!new_parent_entry->pl_parent_change_lock) {
		ERR("no mem.");
		retval = -ENOMEM;
		goto offset_list_lock_alloc_failed;
	}

	INM_INIT_LOCK(new_parent_entry->pl_parent_change_lock);
	INM_INIT_LIST_HEAD(&new_parent_entry->pl_parent_change_list);
	inm_list_add_tail(&new_parent_entry->pl_next, parent_vollist_head);
	INM_INIT_LIST_HEAD(&new_parent_entry->pl_vsnap_list);
	inm_list_add_tail(&vs_info->vsi_next, &new_parent_entry->pl_vsnap_list);
	vs_info->vsi_parent_list = new_parent_entry;

	INM_INIT_LOCK(&new_parent_entry->pl_target_io_done);
	new_parent_entry->pl_num_vsnaps = 1;

  out:
	return retval;

  offset_list_lock_alloc_failed:
	INM_FREE(&new_parent_entry->pl_retention_path,
		 STRING_MEM(new_parent_entry->pl_retention_path));

  ret_path_alloc_failed:
	INM_FREE(&new_parent_entry->pl_devpath,
		 STRING_MEM(new_parent_entry->pl_devpath));

  parent_path_alloc_failed:
	INM_FREE(&new_parent_entry, sizeof(inm_vs_parent_list_t));
	goto out;
}

/*
 * retreive_parent_from_path
 *
 * DESC
 *      searches the parent list structure and returns the structure if one is
 *      found, otherwise returns NULL. The caller of this function should
 *      acquire the parent list lock.
 */
static inm_vs_parent_list_t *
retreive_parent_from_path(char *parent_devpath)
{
	inm_list_head_t		*ptr = NULL;
	inm_list_head_t		*nextptr = NULL;
	inm_vs_parent_list_t	*parent_entry = NULL;

	if (!parent_vollist_head) {
		ERR("error. parent_vollist_head is NULL.");
		goto out;
	}

	inm_list_for_each_safe(ptr, nextptr, parent_vollist_head) {
		parent_entry = inm_list_entry(ptr, inm_vs_parent_list_t, pl_next);
		if (0 == strcmp(parent_entry->pl_devpath, parent_devpath)) {
			return parent_entry;
		}
	}

  out:
	DEV_DBG("not found");
	return NULL;
}

/*
 * insert_into_parent_vsnap_list
 *
 * DESC
 *      adds new vsnap structure to the parent's list of vsnap. if parent's
 * 	list is not yet created then create & insert
 */
static inm_32_t
insert_into_parent_vsnap_list(char *parent_path, inm_vs_info_t *vs_info,
			      char *retention_path)
{
	inm_32_t		retval = 0;
	inm_vs_parent_list_t	*parent_vol = NULL;

	INM_WRITE_LOCK(parent_vollist_lock);
	DEV_DBG("retreive the parent instance given the path");
	parent_vol = retreive_parent_from_path(parent_path);
	if (!parent_vol) {
		DEV_DBG("fresh tgt volume");
		retval = alloc_insert_parent(parent_path, vs_info,
					     retention_path);
		if (retval < 0) {
			ERR("alloc_insert_parent failed.");
			goto out;
		}
	} else {
		DEV_DBG("found an existing parent");
		vs_info->vsi_parent_list = parent_vol;
		inm_list_add_tail(&vs_info->vsi_next, &parent_vol->pl_vsnap_list);
		/* We assume deletes/creates cant go in parallel */
		parent_vol->pl_num_vsnaps++;
	}

  out:
	INM_WRITE_UNLOCK(parent_vollist_lock);
	return retval;
}

/*
 * fid_to_filename
 *
 * DESC
 *      Given file_id, this function returns the filename.
 */
static inm_32_t
fid_to_filename(inm_vs_info_t *vs_info, inm_u32_t file_id, char *file_name)
{
	inm_32_t	retval = 0;
	inm_u64_t	file_offset = 0;

	if (file_id == 0)
		file_offset = 0;
	else
		file_offset = (file_id - 1) * sizeof(inm_vs_fid_table_t);

	/* ToDo - convert to readf */
	retval = INM_READ_FILE(vs_info->vsi_fid_table_filp, file_name,
			       file_offset, sizeof(inm_vs_fid_table_t), NULL);

	return retval;
}

/*
 * read_from_this_fid
 *
 * DESC
 *      opens and reads the data from the retention data file or from
 *      private data file for this vsnap and place it in the appropriate
 *      position in the buffer
 *
 * ALGO
 */
inm_32_t
read_from_this_fid(inm_vdev_page_data_t *page_data, inm_u32_t file_id,
		   inm_vs_info_t *vsnap_info)
{
	char		*file_name = NULL;
	void		*datafile_filp = NULL;
	inm_32_t	close = 1;
	inm_32_t	retval = 0;
	inm_32_t	flags = INM_RDONLY;
	char		*temp_file_name = NULL;

	DEV_DBG("Reading from fid = %u, offset = %lld, size = %d",
		file_id, page_data->pd_file_offset, page_data->pd_size);

	if (IS_FID_IN_RETENTION_PATH(file_id)) {
		file_name = INM_ALLOC(MAX_FILENAME_LENGTH, GFP_NOIO);
		if (!file_name) {
			ERR("no mem");
			retval = -ENOMEM;
			goto out;
		}

		temp_file_name = INM_ALLOC(MAX_FILENAME_LENGTH, GFP_NOIO);
		if (!temp_file_name) {
			ERR("no mem");
			retval = -ENOMEM;
			INM_FREE(&file_name, MAX_FILENAME_LENGTH);
			file_name = NULL;
			goto out;
		}

		if (IS_FID_IN_RETENTION_PATH(file_id)) {
			DEV_DBG("This is obvious");
			STR_COPY(
			    file_name,
			    vsnap_info->vsi_parent_list->pl_retention_path,
			    MAX_FILENAME_LENGTH);
		} else {
			ERR("This cannot happen");
			ASSERT(1);
			STR_COPY(file_name, vsnap_info->vsi_data_log_path,
				 MAX_FILENAME_LENGTH);
		}

		STRING_CAT(file_name, MAX_FILENAME_LENGTH, "/", 1);
		retval =
		    fid_to_filename(vsnap_info,
				    file_id & ~VSNAP_RETENTION_FILEID_MASK,
				    temp_file_name);
		if (retval < 0) {
			ERR("fid_to_filename failed for %u", file_id);
			goto fname_from_fid_failed;
		}

		STRING_CAT(file_name, MAX_FILENAME_LENGTH, temp_file_name,
										sizeof(inm_vs_fid_table_t));

		DEV_DBG("...in retention file %u -> %s", file_id, file_name);
		DEV_DBG("Open the file - %s", file_name);
		retval = INM_OPEN_FILE(file_name, INM_RDONLY, &datafile_filp);
		if (retval != 0) {
			ERR("opening of file %s failed.", file_name);
			retval = -EIO;
			goto open_failed;
		}
	} else {
		DEV_DBG("...in the writefile");
		datafile_filp = vsnap_info->vsi_writefile_filp;
		close = 0;
	}
	retval = inm_vdev_read_page(datafile_filp, page_data);
	if (retval < 0) {
		ERR("reading %d bytes at offset %lld from file id %p failed.",
		    page_data->pd_size, page_data->pd_file_offset,
		    datafile_filp);
		close = 1;
		flags = INM_RDWR;
	}

	if (close)
		INM_CLOSE_FILE(datafile_filp, flags);

  open_failed:
  fname_from_fid_failed:
	if (file_name)
		INM_FREE(&file_name, MAX_FILENAME_LENGTH);

	if (temp_file_name)
		INM_FREE(&temp_file_name, MAX_FILENAME_LENGTH);

  out:
	return retval;
}

/*
 * Decrement the ref_count for the subtree
 * and cleanup once ref_count drops to zero
 */
INLINE static void *
pop_subtree_root(single_list_entry_t *list_head)
{
	inm_subtree_t	*root_entry = NULL;
	void		*root = NULL;

	root = ST_ROOT(list_head);

	ST_REF_DEC(list_head);
	if ( ST_REF_COUNT(list_head) == 0 ) {
		/*
		 * Free the subtree root entry
		 */
		root_entry = (inm_subtree_t *)INM_LIST_POP_ENTRY(list_head);
		INM_FREE(&root_entry, sizeof(inm_subtree_t));
	}

	return root;
}

/*
 * Check if the root at the top of the stack
 * is the one we want to refer to, if so
 * bump the count else push the new root
 * to top of the stack and init use count (st_ref_count)
 */

static inm_32_t
push_subtree_root(single_list_entry_t *list_head, void *root_node)
{
	inm_32_t	retval = 0;
	inm_subtree_t	*root_entry = NULL;

	/*
	 * if subtree root already present, bump the count and return
	 */
	if (  ST_ROOT_ENTRY(list_head) && ST_ROOT(list_head) == root_node ) {
		ST_REF_INC(list_head);
		DEV_DBG("Present in the stack, ref_cnt = %d",
			ST_REF_COUNT(list_head));
		goto out;
	}

	/*
	 * If not already there, create a new entry and push it onto list
	 */
	root_entry = INM_ALLOC(sizeof(inm_subtree_t), GFP_NOIO);
	if ( !root_entry ) {
		retval = -ENOMEM;
		goto out;
	}

	root_entry->st_root = root_node;
	root_entry->st_ref_count = 1;
	INM_LIST_PUSH_ENTRY(list_head, (single_list_entry_t*)root_entry);

	out:
		return retval;
}

/*
 * insert_into_key_list
 *
 * DESC
 *      based on the type of overlap reads the overlapping region 1st and
 *      queues the non-overlapping regions for iterative processing
 *
 * ALGO
 */

static inm_32_t
insert_into_key_list(inm_vs_info_t *vsnap_info,
		single_list_entry_t *found_keys_list_head,
		inm_vs_offset_split_t *offset_split, inm_bt_key_t *key_data,
		single_list_entry_t* offset_split_list_head,
		void *subtree_root, single_list_entry_t *sub_root_list_head)
{
	inm_32_t	retval = 0;
	inm_u32_t	cur_len = 0;
	inm_u64_t	cur_offset = 0;
	inm_u64_t	entry_end = 0;
	inm_u64_t	cur_end = 0;
	inm_bt_key_t    ret_key_data;

	cur_offset = offset_split->os_offset;
	cur_len = offset_split->os_len;
	cur_end = cur_offset + cur_len - 1;
	entry_end = key_data->key_offset + key_data->key_len - 1;

	/*
	 * requested block is contained inside a key_data block
	 * read the entire requested block from the key_data block
	 */
	if ((cur_offset >= key_data->key_offset) && (cur_end <= entry_end)) {

		DEV_DBG("requested (off,len) tuple contained in btree tuple");
		ret_key_data.key_offset = cur_offset;
		ret_key_data.key_len = cur_len;
		ret_key_data.key_file_offset = key_data->key_file_offset +
					( cur_offset - key_data->key_offset );
		ret_key_data.key_fid = key_data->key_fid;
		ret_key_data.key_tracking_id = key_data->key_tracking_id;

	} /* else to continue... */

	/*
	 * requested block covers the key_data block
	 * 1. read the entire key_data block
	 * 2. split and insert the remaining request into the offset_split list
	 * for processing in next iteration
	 */
	else if ((cur_offset < key_data->key_offset) &&
		 (cur_end > entry_end)) {
		DEV_DBG("requested (off,len) tuple covers the btree tuple");

		ret_key_data.key_offset = key_data->key_offset;
		ret_key_data.key_len = key_data->key_len;
		ret_key_data.key_file_offset = key_data->key_file_offset;
		ret_key_data.key_fid = key_data->key_fid;
		ret_key_data.key_tracking_id = key_data->key_tracking_id;

		/*
		 * insert the left remaining portion of request in
		 * offset_split_list
		 */
		DEV_DBG("insert the left non-overlapping portion into list");
		retval =
		    inm_vs_insert_offset_split(offset_split_list_head,
					       cur_offset,
					       (key_data->key_offset -
						cur_offset), 0,
					       offset_split->os_file_offset,0);
		if (retval < 0) {
			ERR("insert offset split failed.");
			goto out;
		}

		retval = push_subtree_root(sub_root_list_head, subtree_root);
		if (retval < 0) {
			ERR("push_subtree_root failed.");
                	goto out;
		}

		/*
		 * insert the right remaining portion of request in
		 * offset_split_list
		 */
		DEV_DBG("insert the right non-overlapping portion into list");
		offset_split->os_file_offset += (entry_end - cur_offset + 1);

		retval =
		    inm_vs_insert_offset_split(offset_split_list_head,
					       entry_end + 1,
					       (cur_end - entry_end), 0,
					       offset_split->os_file_offset,0);
		if (retval < 0) {
			ERR("alloc and insert offset split failed.");
			goto out;
		}

		retval = push_subtree_root(sub_root_list_head, subtree_root);
		if (retval < 0) {
			ERR("push_subtree_root failed.");
                	goto out;
		}

	} /* else to continue */

	/*
	 * requested block left-overlaps with key_data block
	 * 1. read the overlapped portion from the log
	 * 2. split and insert the remaining request into the offset_split list
	 *	 for processing in next iteration
	 */
	else if ((cur_offset < key_data->key_offset) &&
		 (cur_end >= key_data->key_offset) && (cur_end <= entry_end)) {
		DEV_DBG("requested (off,len) tuple left-overlaps with bt");

		ret_key_data.key_offset = key_data->key_offset;
		ret_key_data.key_len = ( cur_offset + cur_len ) -
					key_data->key_offset;
		ret_key_data.key_file_offset = key_data->key_file_offset;
		ret_key_data.key_fid = key_data->key_fid;
		ret_key_data.key_tracking_id = key_data->key_tracking_id;

		/*
		 * insert the left remaining portion of request in
		 * offset_split_list
		 */
		DEV_DBG("insert the non-overlapping into list");
		retval =
		    inm_vs_insert_offset_split(offset_split_list_head,
					       cur_offset,
					       (key_data->key_offset -
						cur_offset), 0,
					       offset_split->os_file_offset,
					       0);
		if (retval < 0) {
			ERR("inm_vs_insert_offset_split failed.");
			goto out;
		}

		retval = push_subtree_root(sub_root_list_head, subtree_root);
		if (retval < 0) {
			ERR("push_subtree_root failed.");
                	goto out;
		}
	} /* else to continue... */

	/*
	 * requested block right-overlaps with key_data block
	 * 1. read the overlapped portion from the log
	 * 2. split and insert the remaining request into the offset_split list
	 * for processing in next iteration
	 */
	else if ((cur_offset >= key_data->key_offset) &&
		 (cur_offset <= entry_end) && (cur_end > entry_end)) {
		DEV_DBG("requested (off,len) tuple right overlaps bt tuple");

		ret_key_data.key_offset = cur_offset;
		ret_key_data.key_len = ( key_data->key_offset +
					key_data->key_len ) - cur_offset;
		ret_key_data.key_file_offset = key_data->key_file_offset +
						(cur_offset -
						 key_data->key_offset);
		ret_key_data.key_fid = key_data->key_fid;
		ret_key_data.key_tracking_id = key_data->key_tracking_id;

		/*
		 * insert the right remaining portion of request in
		 * offset_split_list
		 */
		DEV_DBG("insert the non-overlapped portion into list");
		offset_split->os_file_offset += (entry_end - cur_offset + 1);
		retval =
		    inm_vs_insert_offset_split(offset_split_list_head,
					       entry_end + 1,
					       (cur_end - entry_end), 0,
					       offset_split->os_file_offset,
					       0);
		if (retval < 0) {
			ERR("inm_vs_insert_offset_split failed.");
		}

		retval = push_subtree_root(sub_root_list_head, subtree_root);
		if (retval < 0) {
			ERR("push_subtree_root failed.");
                	goto out;
		}
       	} else {
		ERR("Something is wrong, shouldn't be here");
		ALERT();
		retval = -EINVAL;
		goto out;
	}

	DEV_DBG("Found offset=%llu, len = %u, file offset = %llu",
			ret_key_data.key_offset, ret_key_data.key_len,
			ret_key_data.key_file_offset);
	retval = inm_vs_insert_offset_split(found_keys_list_head,
					    ret_key_data.key_offset,
					    ret_key_data.key_len,
					    ret_key_data.key_fid,
					    ret_key_data.key_file_offset,
					    ret_key_data.key_tracking_id);

	DEV_DBG("found key at %p", found_keys_list_head->sl_next);
  out:
		return retval;
}

/*
 * read_using_map
 *
 * DESC
 *      Iteratively calls bt_lookup for a range of bio
 *
 * ALGO
 *      1. Create inm_vs_offset_split_t structure with page_data struct
 *      2. Add this to the global list of unsatisfied offset/len pair
 *      3. loop until global list is not empty
 *      4.   Search the btree map
 *      5.   if success then add to retention data list in biodata
 */
inm_32_t
read_using_map(inm_vs_info_t *vsnap_info, inm_u64_t disk_offset,
		    inm_u32_t size, lookup_ret_data_t *found_keys)
{
	inm_bt_t		*btree = NULL;
	inm_32_t		retval = 0;
	inm_bt_key_t		*key_entry = NULL;
	single_list_entry_t	offset_split_list_head;
	single_list_entry_t	sub_root_list_head;
	inm_vs_offset_split_t	*offset_split = NULL;
	void 			*subtree_root = NULL;
	void			*new_subtree_root = NULL;
	single_list_entry_t	*found_keys_list_head;
	inm_32_t		delete_subtree_root = 0;

	found_keys_list_head = &found_keys->lrd_ret_data;

	DEV_DBG("Lookup for offset = %llu len = %u", disk_offset, size);

	INM_LIST_INIT(&offset_split_list_head);
	INM_LIST_INIT(&sub_root_list_head);
	INM_LIST_INIT(found_keys_list_head);

	key_entry = INM_ALLOC(sizeof(inm_bt_key_t), GFP_NOIO);
	if (!key_entry) {
		ERR("no mem");
                retval = -ENOMEM;
                goto out;
	}

	/*
	 * insert the given offset & length into the offset list for
	 * processing
	 */
        retval = inm_vs_insert_offset_split(&offset_split_list_head,
                                           disk_offset, size, 0, 0, 0);
	if (retval < 0) {
		ERR("insert offset split failed");
		goto insert_offset_failed;
	}

	/*
	 * push in the root node into the subtree list
	 * as lookup should start from the btree root
	 */
	btree = vsnap_info->vsi_btree;
	retval = push_subtree_root(&sub_root_list_head, btree->bt_root_node);
	if (retval < 0) {
		ERR("push_subtree_root root failed");
                goto push_subtree_root_failed;
	}

	/*
	 * loop until no key_data queued in this list
	 * page_data.bvec_offset is being modified so to retain the
	 * original value we use this temp local var bvec_off
	 */
	while (!INM_LIST_IS_EMPTY(&offset_split_list_head)) {

		/*
		 * pick the lastest entry from the queue
		 */
		offset_split = (inm_vs_offset_split_t *)
		    INM_LIST_POP_ENTRY(&offset_split_list_head);
		if (!offset_split)
			ASSERT(1);

		/*
		 * Get the subtree where you want to start the search.
		 * Mark the flag if the subtree root has to freed in
		 * this iteration. Once we insert_into_key_list(), the
		 * we cannot be sure new subtree roots are not pushed in
		 * So mark the flag here.
		 */
		subtree_root = pop_subtree_root(&sub_root_list_head);
		if ( INM_LIST_IS_EMPTY(&sub_root_list_head) ||
		     subtree_root != ST_ROOT(&sub_root_list_head) )
			delete_subtree_root = 1;
		else
			delete_subtree_root = 0;
		/*
		 * search thro' the btree if the offset is available?
		 * if so, then read from file else from vol
		 */
		retval = inm_vs_bt_lookup(btree, subtree_root,
				     (inm_bt_key_t *)&offset_split->os_offset,
				     (inm_bt_key_t *)key_entry,
				     &new_subtree_root);

		/*
		 * If we no longer need the subtree node
		 * delete it
		 */

		if (retval == BTREE_SUCCESS) {
			DEV_DBG("Found: offset = %llu, len = %u in %p",
				key_entry->key_offset, key_entry->key_len,
				new_subtree_root);
			retval = insert_into_key_list(vsnap_info,
						      found_keys_list_head,
						      offset_split, key_entry,
						      &offset_split_list_head,
						      new_subtree_root,
						      &sub_root_list_head);

			/* If new node is not inserted into the stack,
			 * then delete it now before checking for error.
			 * If new_subtree_root is not going to be used
			 * i.e. we found a complete data in a single tuple
			 * free it as no further search is required
		 	*/
			if ( ( INM_LIST_IS_EMPTY(&sub_root_list_head) ||
			      new_subtree_root != ST_ROOT(&sub_root_list_head) )
			     && new_subtree_root != subtree_root ) {
				DEV_DBG("Found entire key, free %p, sub = %p",
					new_subtree_root, subtree_root);
				BT_FREE_NODE(&new_subtree_root);
			}

			if (retval < 0) {
				ERR("insert key failed");
				goto insert_key_failed;
			}

			new_subtree_root = NULL;

		} else {

			found_keys->lrd_target_read = 1;
			DEV_DBG("Not Found: offset = %llu, len = %u",
				offset_split->os_offset, offset_split->os_len);
		}

		/*
		 * If the node is no longer in use, free it. Recheck and
		 * verify if the subtree root did not get pushed in again
		 */
		if ( ( INM_LIST_IS_EMPTY(&sub_root_list_head) ||
		     subtree_root != ST_ROOT(&sub_root_list_head) ) &&
		     delete_subtree_root ) {
			DEV_DBG("Free Subtree = %p", subtree_root);
			BT_FREE_NODE(&subtree_root);
			subtree_root = NULL;
		}

		INM_FREE(&offset_split, sizeof(inm_vs_offset_split_t));
		offset_split = NULL;	/* in release build there is no
					   double free check */
	}

	INM_FREE(&key_entry, sizeof(inm_bt_key_t));

out:
	return retval;

insert_key_failed:
	INM_FREE(&offset_split, sizeof(inm_vs_offset_split_t));

	/*
	 * Free the subtree if delete_subtree_root is set
	 */
	if ( ( INM_LIST_IS_EMPTY(&sub_root_list_head) ||
	     subtree_root != ST_ROOT(&sub_root_list_head) ) &&
	     delete_subtree_root ) {
		DEV_DBG("Free Subtree = %p", subtree_root);
		BT_FREE_NODE(&subtree_root);
		subtree_root = NULL;
	}

	/* Free the subtree nodes */
	DEV_DBG("Freeing subtree roots");
	while ( !INM_LIST_IS_EMPTY(&sub_root_list_head) ) {
		subtree_root = pop_subtree_root(&sub_root_list_head);
		if ( INM_LIST_IS_EMPTY(&sub_root_list_head) ||
		     subtree_root != ST_ROOT(&sub_root_list_head) ) {
			DEV_DBG("Freeing %p", subtree_root);
			BT_FREE_NODE(&subtree_root);
		}
	}

	/* Free the keys we have in the list in previous iterations */
	DEV_DBG("Freeing previous results");
	while (!INM_LIST_IS_EMPTY(found_keys_list_head)) {
		offset_split = (inm_vs_offset_split_t *)
		INM_LIST_POP_ENTRY(found_keys_list_head);
		DEV_DBG("Free %p", offset_split);
		INM_FREE(&offset_split, sizeof(inm_vs_offset_split_t));
	}


push_subtree_root_failed:
	while (!INM_LIST_IS_EMPTY(&offset_split_list_head)) {
		offset_split = (inm_vs_offset_split_t *)
		    INM_LIST_POP_ENTRY(&offset_split_list_head);
		INM_FREE(&offset_split, sizeof(inm_vs_offset_split_t));
	}

insert_offset_failed:
	INM_FREE(&key_entry, sizeof(inm_bt_key_t));
        goto out;
}

/*
 * update_bt_hdr
 *
 * DESC
 *      update the btree header corresponding to the give vsnap
 */
static inm_32_t
update_bt_hdr(inm_vs_info_t *vsnap_info)
{
	inm_32_t		retval = 0;
	char			*btree_hdr = NULL;
	inm_bt_t		*btree = NULL;
	inm_vs_hdr_info_t	*temp;

	btree = vsnap_info->vsi_btree;

	btree_hdr = INM_ALLOC(BTREE_HEADER_SIZE, GFP_NOIO);
	if (!btree_hdr) {
		ERR("no mem.");
		retval = -ENOMEM;
		goto out;
	}

	temp = (inm_vs_hdr_info_t *)(btree_hdr + BTREE_HEADER_OFFSET_TO_USE);
	DEV_DBG("copy hdr from the btree");
	retval = inm_bt_copy_hdr(btree, btree_hdr);
	if (retval < 0) {
		ERR("copy hdr failed");
		goto copy_hdr_failed;
	}
	temp->hinfo_data_fid = vsnap_info->vsi_next_fid;
	temp->hinfo_write_fid = vsnap_info->vsi_write_fid;
	temp->hinfo_writefile_suffix = vsnap_info->vsi_writefile_suffix;

	/* For sparse */
	temp->hinfo_sparse_backward_fid = vsnap_info->vsi_backward_fid;
	temp->hinfo_sparse_forward_fid = vsnap_info->vsi_forward_fid;

	DEV_DBG("Write Fid = %u", temp->hinfo_write_fid);

	DEV_DBG("write the btree hdr");
	retval = inm_vs_bt_write_hdr(btree, btree_hdr);
	if (retval < 0) {
		ERR("write_hdr failed");
		retval = -EIO;
	}

  copy_hdr_failed:
	INM_FREE(&btree_hdr, BTREE_HEADER_SIZE);

  out:
	return retval;
}

/*
 * set_next_private_fid
 *
 * DESC
 *      sets the write file id for a vsnap volume
 */
static inm_32_t
set_next_private_fid(inm_vs_info_t * vsnap_info)
{
	inm_32_t retval = 0;

	if (!vsnap_info->vsi_next_fid_used) {
		DEV_DBG("using vsi_next_fid");
		vsnap_info->vsi_write_fid = vsnap_info->vsi_next_fid;
	} else {
		DEV_DBG("next fid in use");
		vsnap_info->vsi_write_fid = (vsnap_info->vsi_next_fid + 1);
	}

	/* Dont use special FID */
	if ( vsnap_info->vsi_write_fid == SPARSE_FID_SPECIAL )
		vsnap_info->vsi_write_fid++;

	vsnap_info->vsi_next_fid = vsnap_info->vsi_write_fid + 1;
	vsnap_info->vsi_next_fid_used = 0;

	DBG("Write FID = %u", vsnap_info->vsi_write_fid);

	DEV_DBG("update bt hdr with the next fid");
	retval = update_bt_hdr(vsnap_info);
	if (retval < 0) {
		ERR("update hdr failed");
	}

	return retval;
}

/*
 * set_next_data_fid
 *
 * DESC
 *      sets the read file id (i.e., ret log files) for a vsnap volume
 */
static inm_32_t
set_next_data_fid(inm_vs_info_t *vs_info)
{
	inm_32_t	retval = 0;

	if (vs_info->vsi_next_fid_used) {
		DEV_DBG("current fid is already used");
		vs_info->vsi_next_fid++;
		vs_info->vsi_next_fid_used = 0;

		/* Dont use special FID */
		if ( vs_info->vsi_next_fid == SPARSE_FID_SPECIAL )
			vs_info->vsi_next_fid++;

		DEV_DBG("New FID = %u", vs_info->vsi_next_fid);

		DEV_DBG("update bt hdr with the next fid");
		retval = update_bt_hdr(vs_info);
		if (retval < 0) {
			ERR("update hdr failed");
		}
	}

	return retval;
}


/*
 * seek_and_write
 *
 * DESC
 *      given a file in the private data directory of a virtual volume, this
 *      function writes the data at the specified offset. If the offset
 *      supplied is -1, the this function finds the free offset and writes
 *      the data there. If the file exceeds VSNAP_MAX_DATA_FILE_SIZE, this
 *      function writes data in the new file.
 */
static inm_32_t
seek_and_write(char *file_name, inm_32_t need_file_offset,
	       inm_vdev_page_data_t *page_data, inm_u32_t *nbytes,
	       inm_u32_t *file_id, inm_vs_info_t *vs_info)
{
	void		*temp_filp = vs_info->vsi_writefile_filp;
	inm_32_t	retval = 0;
	inm_u64_t	new_offset = 0;

	/*
	 * if fileoffset is not provided then seek it
	 */
	if (need_file_offset == 1) {
		DEV_DBG("file offset not provided");
		retval = INM_SEEK_FILE_END(temp_filp, &new_offset);
		if (retval < 0) {
			ERR("seek file failed");
			goto out;
		}
		DEV_DBG("new offset %lld", new_offset);
		page_data->pd_file_offset = new_offset;
	}

	retval = inm_vdev_write_page(temp_filp, page_data);
	if (retval < 0) {
		ERR("inm_vdev_write_page failed");
	}

  out:
	return retval;
}

/*
 * write_to_private_file
 *
 * DESC
 *      copies the data to private log for a vsnap volume.
 */
static inm_32_t
write_to_private_file(inm_vdev_page_data_t *page_data, inm_u32_t *file_id,
		      inm_vs_info_t *vs_info, inm_u32_t *nbytes)
{
	char		*file_name = NULL;
	inm_32_t	retval = 0;
	inm_32_t	need_file_offset = 0;

	/*
	 * is it the 1st write?
	 */
	if (vs_info->vsi_write_fid == (inm_u32_t) -1) {
		DEV_DBG("1st write");
		file_name = INM_ALLOC(MAX_FILENAME_LENGTH, GFP_NOIO);
		if (!file_name) {
			ERR("no mem");
			retval = -ENOMEM;
			goto out;
		}

		/*
		 * set a new fileid for this write file
		 */
		DEV_DBG("set a new fid for the write data file");
		retval = set_next_private_fid(vs_info);
		if (retval < 0) {
			ERR("set next private fid failed");
			INM_FREE(&file_name, MAX_FILENAME_LENGTH);
			goto out;
		}

		*file_id = vs_info->vsi_write_fid |VSNAP_RETENTION_FILEID_MASK;

		/*
		 * add the file id to the file id mapping table.
		 */
		STRING_PRINTF((file_name, MAX_FILENAME_LENGTH,
			       "%llu_WriteData%d", vs_info->vsi_snapshot_id,
			       vs_info->vsi_writefile_suffix));
		DEV_DBG("add write fid into the fid table");
		retval = add_fid_to_table(vs_info,
					  (inm_vs_fid_table_t *)file_name);
		if (retval < 0) {
			ERR("add fid 2 table failed");
			INM_FREE(&file_name, MAX_FILENAME_LENGTH);
			goto add_fid_failed;
		}

		/*
		 * new file so get a new file offset
		 */
		need_file_offset = 1;
		retval = seek_and_write(file_name, need_file_offset, page_data,
					nbytes, file_id, vs_info);
		if (retval < 0) {
			ERR("vs write failed");
		}
		INM_FREE(&file_name, MAX_FILENAME_LENGTH);
	} else {
		/*
		 * if fileid does not point to write_file then use the
		 * current write_file
		 */
		DEV_DBG("fid not provided");
		if ((*file_id == (inm_u32_t)-1) ||
		    IS_FID_IN_RETENTION_PATH(*file_id)) {
			*file_id = vs_info->vsi_write_fid
			    | VSNAP_RETENTION_FILEID_MASK;
			need_file_offset = 1; /* get a new file offset */
		}
		retval = seek_and_write(file_name, need_file_offset, page_data,
					nbytes, file_id, vs_info);
		if (retval < 0) {
			ERR("vs write failed");
		}
	}

  add_fid_failed:
  out:
	return retval;
}

static inm_32_t
update_bt_map_for_writes_entry(single_list_entry_t *list_head,
			       inm_vs_offset_split_t *offset_split,
			       inm_vdev_page_data_t page_data,
			       inm_bt_key_t *key_entry,
			       inm_vs_info_t *vs_info, inm_32_t cow,
			       inm_u64_t cur_offset, inm_u32_t cur_len)
{
	inm_32_t		retval = 0;
	inm_u32_t		temp = 0;
	inm_u64_t		cur_end = 0;
	inm_u64_t		entry_end = 0;

	cur_end = cur_offset + cur_len - 1;
	entry_end = key_entry->key_offset + key_entry->key_len - 1;

	/*
	 * requested block is contained in the btree-entry block; write the
	 * entire requested block @ the same block-offset corresponding to
	 * the btree-entry block. then insert this entry in the btree-map
	 */
	if ((cur_offset >= key_entry->key_offset) && (cur_end <= entry_end)) {
		DEV_DBG("requested (off,len) tuple contained in btree tuple");

		/*
		 * file_offset reflects an offset such that the previous
		 * entry's data is replaced
		 */
		page_data.pd_file_offset = key_entry->key_file_offset +
		    (cur_offset - key_entry->key_offset);
		page_data.pd_bvec_offset +=
		    (inm_u32_t)offset_split->os_file_offset;
		page_data.pd_size = cur_len;

		/*
		 * write the data into the write_data file
		 */
		if (!cow) {
			DEV_DBG("write into the private file");
			retval = write_to_private_file(&page_data,
						       &key_entry->key_fid,
						       vs_info, &temp);
			if (retval < 0) {
				ERR("write log failed.");
				goto out;
			}
		}

		/*
		 * a new entry to be inserted in the btree for this write
		 */
		DEV_DBG("set the new key entry");
		key_entry->key_offset = cur_offset;
		key_entry->key_len = cur_len;
		key_entry->key_file_offset = page_data.pd_file_offset;
		key_entry->key_tracking_id = offset_split->os_tracking_id;
	} /* else to continue... */

	/*
	 * requested block covers the existing btree-entry
	 * block
	 *   1. insert the non-overlapping entries into the
	 *   offset-split list for future processing
	 *   2. write the overlapping data @ the same location
	 */
	else if ((cur_offset < key_entry->key_offset) &&
		 (cur_end > entry_end)) {
		DEV_DBG("requested (off,len) tuple covers the btree tuple");

		/*
		 * write the overlapping data into the write_data file
		 */
		page_data.pd_bvec_offset += (inm_u32_t)
		    (offset_split->os_file_offset + (key_entry->key_offset -
						     cur_offset));
		page_data.pd_file_offset = key_entry->key_file_offset;
		page_data.pd_size = key_entry->key_len;

		/*
		 * insert left non-overlap portion
		 */
		DEV_DBG("insert the left un-overlapped tuple into the list");
		retval = inm_vs_insert_offset_split(
		    list_head, cur_offset,
		    (inm_u32_t)(key_entry->key_offset - cur_offset),
		    0, offset_split->os_file_offset,
		    offset_split->os_tracking_id);
		if (retval < 0) {
			ERR("insert offset failed.");
			goto out;
		}

		/*
		 * insert right non-overlap portion
		 */
		DEV_DBG("insert the right un-overlapped tuple into the list");
		offset_split->os_file_offset += (entry_end - cur_offset + 1);
		retval = inm_vs_insert_offset_split(
		    list_head, entry_end + 1,(inm_u32_t) (cur_end - entry_end),
		    0, offset_split->os_file_offset,
		    offset_split->os_tracking_id);
		if (retval < 0) {
			ERR("insert offset failed.");
			goto out;
		}

		if (!cow) {
			DEV_DBG("write data of overlapping tuple into prvate");
			retval = write_to_private_file(&page_data,
						       &key_entry->key_fid,
						       vs_info, &temp);
			if (retval < 0) {
				ERR("write log failed.");
				goto out;
			}
		}

		/*
		 * update the map with offset and length remains same
		 */
		DEV_DBG("set the new key entry");
		key_entry->key_file_offset = page_data.pd_file_offset;
		key_entry->key_tracking_id = offset_split->os_tracking_id;
	} /* else to continue... */

	/*
	 * requested block left-overlaps with key_data block
	 *   1. insert the non-overlapping portion into
	 *   offset-split list for furture processing
	 *   2. write the overlapping portion in the
	 *   write_fileid
	 */
	else if ((cur_offset < key_entry->key_offset) &&
		 (cur_end >= key_entry->key_offset) &&
		 (cur_end <= entry_end)) {
		DEV_DBG("requested (off,len) tuple left-overlaps with bt");

		/*
		 * insert the non-overlapping portion into the offset-split
		 * list
		 */
		DEV_DBG("insert the non-overlapping tuple into the list");
		retval = inm_vs_insert_offset_split(
		    list_head, cur_offset,
		    (inm_u32_t) (key_entry->key_offset - cur_offset),
		    0, offset_split->os_file_offset,
		    offset_split->os_tracking_id);
		if (retval < 0) {
			ERR("insert offset failed.");
			goto out;
		}

		/*
		 * write the overlapping data into the write_data file
		 */
		page_data.pd_bvec_offset += (inm_u32_t)
		    (offset_split->os_file_offset + (key_entry->key_offset -
						     cur_offset));
		page_data.pd_file_offset = key_entry->key_file_offset;
		page_data.pd_size = (inm_u32_t)(cur_end -
						key_entry->key_offset + 1);
		if (!cow) {
			DEV_DBG("write data of overlap tuple into private");
			retval = write_to_private_file(&page_data,
						       &key_entry->key_fid,
						       vs_info, &temp);
			if (retval < 0) {
				ERR("write log failed.");
				goto out;
			}
		}

		/*
		 * insert an entry in the btree-map
		 */
		key_entry->key_len = page_data.pd_size;
		key_entry->key_file_offset = page_data.pd_file_offset;
		key_entry->key_tracking_id = offset_split->os_tracking_id;
	} /* else to continue... */

	/*
	 * requested block right-overlaps with key_data block
	 *   1. insert the non-overlapping portion into
	 *   offset-split list for furture processing
	 *   2. write the overlapping portion in the
	 *   write_fileid
	 */
	else if ((cur_offset >= key_entry->key_offset)
		 && (cur_offset <= entry_end) && (cur_end > entry_end)) {
		DEV_DBG("requested (off,len) tuple right overlaps bt tuple");
		page_data.pd_bvec_offset +=
		    (inm_u32_t)offset_split->os_file_offset;
		page_data.pd_file_offset = key_entry->key_file_offset +
		    (cur_offset - key_entry->key_offset);
		page_data.pd_size = (inm_u32_t)(entry_end - cur_offset + 1);

		/*
		 * insert the non-overlapping offset-len in
		 * the list for future processing
		 */
		DEV_DBG("insert the non-overlapping tuple into the list");
		offset_split->os_file_offset += (entry_end - cur_offset + 1);
		retval = inm_vs_insert_offset_split(
		    list_head, entry_end + 1, (inm_u32_t)(cur_end - entry_end),
		    0, offset_split->os_file_offset,
		    offset_split->os_tracking_id);
		if (retval < 0) {
			ERR("insert offset failed.");
			goto out;
		}
		if (!cow) {
			DEV_DBG("write the data of overlapping tuple in prve");
			retval = write_to_private_file(&page_data,
						       &key_entry->key_fid,
						       vs_info, &temp);
			if (retval < 0) {
				ERR("write log failed.");
				goto out;
			}
		}

		/*
		 * insert an entry in the btree-map
		 */
		DEV_DBG("set the new key entry");
		key_entry->key_offset = cur_offset;
		key_entry->key_len = page_data.pd_size;
		key_entry->key_file_offset = page_data.pd_file_offset;
		key_entry->key_tracking_id = offset_split->os_tracking_id;
	} /* end of final else for btree success */

  out:
	return retval;
}


/*
 * write_using_map
 *
 * DESC
 *      core write API performing the bunch of creating the logs,
 *      map updation, etc...
 *
 * ALGO
 *      1. search the map for offset,length pair
 *      2. if found in the map
 *      3.   if fileid points to retention log, then we have to store this
 *             data as a personal copy for this vsnap in writefile
 *      4.   else fileid points to data file thats personal to this vsnap
 *             then overwrite the old data with new data
 *      5. else
 *      6.   insert the entry in the map
 *      7.   add the data to the personal data file for this vsnap
 */
static inm_32_t
write_using_map(inm_vdev_page_data_t page_data, inm_vs_info_t *vs_info,
		inm_32_t cow)
{
	inm_bt_t		*btree = NULL;
	inm_32_t		retval = 0;
	inm_32_t		should_update = 0;
	inm_u32_t		temp;
	inm_u32_t		cur_len = 0;
	inm_u32_t		bvec_off = 0;
	inm_u32_t		tracking_id = 0;
	inm_u64_t		cur_offset = 0;
	inm_bt_key_t		*key_entry;
	enum BTREE_STATUS	status = BTREE_SUCCESS;
	single_list_entry_t	list_head;
	inm_vs_offset_split_t	*offset_split = NULL;
	void			*subtree_root = NULL;

	btree = vs_info->vsi_btree;
	INM_LIST_INIT(&list_head);

	/*
	 * is trackin is enabled?
	 */
	if (cow) {
		tracking_id = 0;
	} else {
		if (vs_info->vsi_is_tracking_enabled)
			tracking_id = vs_info->vsi_tracking_id;
		else
			tracking_id = 0;
	}
	key_entry = INM_ALLOC(sizeof(inm_bt_key_t), GFP_NOIO);
	if (!key_entry) {
		ERR("no mem");
		retval = -ENOMEM;
		goto key_entry_alloc_failed;
	}

	/*
	 * insert the given offset & len into the offset list for processing
	 */
	retval = inm_vs_insert_offset_split(&list_head,
					    (inm_u64_t)page_data.pd_disk_offset,
					    page_data.pd_size, 0, 0,
					    tracking_id);
	if (retval < 0) {
		ERR("insert offset split failed");
		goto insert_offset_failed;
	}

        /*
	 * loop until no key_data queued in this list
	 * page_data.bvec_offset is being modified so to retain the
	 * original value we use this temp local var bvec_off
	 */
	bvec_off = page_data.pd_bvec_offset;
	while (!INM_LIST_IS_EMPTY(&list_head)) {

		/*
		 * reset the local (loop dependent) vars
		 */
		temp = 0;
		offset_split = NULL;
		page_data.pd_bvec_offset = bvec_off;

		/*
		 * pick the lastest entry from the queue
		 */
		offset_split = (inm_vs_offset_split_t *)
		    INM_LIST_POP_ENTRY(&list_head);
		cur_offset = offset_split->os_offset;
		cur_len = offset_split->os_len;

		/*
		 * search thro' the btree if the offset is available?
		 * if so,then update the btree-map entry else insert a new
		 * entry
		 */
		DEV_DBG("btree lookup for off[%lld]-len[%d]", cur_offset,
			cur_len);
		status =
		    inm_vs_bt_lookup(btree, btree->bt_root_node,
				     (inm_bt_key_t *)&offset_split->os_offset,
				     key_entry, &subtree_root);

		if (status == BTREE_SUCCESS) {
			DEV_DBG("found a match");
			/*
			 * Dont need subtree_root, free it
			 */
			BT_FREE_NODE(&subtree_root);

			retval = update_bt_map_for_writes_entry(&list_head,
								offset_split,
								page_data,
								key_entry,
								vs_info, cow,
								cur_offset,
								cur_len);
			if (retval < 0) {
				ERR("updating the bt map failed");
				break;
			}
			/*
			 * should the btree map be update
			 */
			if (cow) {
				DEV_DBG("btree map already updated");
				should_update = 0;
			} else {
				DEV_DBG("should update the btree map");
				should_update = 1;
			}

		} else {
			/*
			 * no entry in the btree-map. create a new entry and
			 * point the entry to the write-data file
			 */
			DEV_DBG("no match found in btree");

			/*
			 * insert a new entry in the btree-map
			 */
			DEV_DBG("prepare a new key");
			key_entry->key_offset = cur_offset;
			key_entry->key_len = cur_len;
			key_entry->key_fid = -1;
			key_entry->key_tracking_id =
			    offset_split->os_tracking_id;
			page_data.pd_bvec_offset +=
			    (inm_u32_t)offset_split->os_file_offset;

			/*
			 * file_offset will be updated later inside the
			 * write_to_private_file call
			 */
			page_data.pd_file_offset = key_entry->key_file_offset;
			page_data.pd_size = (inm_u32_t) key_entry->key_len;

			/*
			 * write the data into the write file
			 */
			DEV_DBG("write the data into the private file");
			retval = write_to_private_file(&page_data,
						       &key_entry->key_fid,
						       vs_info, &temp);
			if (retval < 0) {
				ERR("write log failed.");
				break;
			}

			/*
			 * page_data.file_offset gets updated inside
			 */
			key_entry->key_file_offset = page_data.pd_file_offset;
			should_update = 1;
		}

		/*
		 * if map should be update then do so
		 */
		if (should_update) {
			DEV_DBG("update the map");
			retval = update_bt_map_for_writes(btree, key_entry);
			if (retval < 0) {
				ERR("write update map failed.");
				break;
			}
		}
		INM_FREE(&offset_split, sizeof(inm_vs_offset_split_t));
		offset_split = NULL;	/* in release build there is no
					   double free check */
	}

	/*
	 * clean up (if failed)
	 */
	if (offset_split) {
		INM_FREE(&offset_split, sizeof(inm_vs_offset_split_t));
	}

	while (!INM_LIST_IS_EMPTY(&list_head)) {
		offset_split = (inm_vs_offset_split_t *)
		    INM_LIST_POP_ENTRY(&list_head);
		if (offset_split)
			INM_FREE(&offset_split, sizeof(inm_vs_offset_split_t));
	}

  insert_offset_failed:
	INM_FREE(&key_entry, sizeof(inm_bt_key_t));

  key_entry_alloc_failed:
  	return retval;
}

/*
 * insert_change_in_parent_list
 *
 * DESC
 *      allocates a new inm_vs_parent_change_t structure, initializes it and
 *	adds to the parent's list of offset's to be in locked state.
 */
static inm_32_t
insert_change_in_parent_list(inm_vs_parent_list_t *parent_vollist,
			     inm_u64_t vol_offset, inm_u32_t len,
			     inm_vs_parent_change_t **parent_change)
{
	inm_32_t		retval = 0;
	inm_vs_parent_change_t	*temp_entry;

	temp_entry = INM_ALLOC(sizeof(inm_vs_parent_change_t), GFP_NOIO);
	if (!temp_entry) {
		ERR("no mem");
		retval = -ENOMEM;
		goto out;
	}

	*parent_change = temp_entry;
	temp_entry->pc_offset = vol_offset;
	temp_entry->pc_len = len;
	INM_INIT_MUTEX(&(temp_entry->pc_mutex));
	INM_WRITE_LOCK(parent_vollist->pl_parent_change_lock);
	inm_list_add_tail(&temp_entry->pc_next,
		      &parent_vollist->pl_parent_change_list);
	INM_WRITE_UNLOCK(parent_vollist->pl_parent_change_lock);

  out:
	return retval;
}

/*
 * remove_change_from_parent_list
 *
 * DESC
 *      removes the offset Lock from the parent structure.
 */
static void
remove_change_from_parent_list(inm_vs_parent_list_t *parent_vollist,
			 inm_vs_parent_change_t *parent_change)
{
	INM_WRITE_LOCK(parent_vollist->pl_parent_change_lock);
	inm_list_del((inm_list_head_t *)parent_change);
	INM_WRITE_UNLOCK(parent_vollist->pl_parent_change_lock);
	INM_FREE(&parent_change, sizeof(inm_vs_parent_change_t));
}

/*
 * For cdp, the retention data is either appended to last retention file
 * or a new file is created. We compare the filename against last file.
 * If new fileis created, get a new fid and use it to populate the btree
 */
static enum FID_STATUS
inm_vs_get_file_fid_cdp(inm_vs_info_t *vs_info, char *file_name, inm_u32_t *fid,
                        inm_32_t *not_used)
{
	inm_32_t retval = 0;
	enum FID_STATUS ret = FID_SUCCESS;

	if ( strncmp(vs_info->vsi_last_datafile, file_name,
		     MAX_RETENTION_DATA_FILENAME_LENGTH) ) {
		DEV_DBG("New File");
		STR_COPY(vs_info->vsi_last_datafile, file_name,
			 MAX_RETENTION_DATA_FILENAME_LENGTH);
		retval = set_next_data_fid(vs_info);
		if (retval < 0) {
			ERR("set next data fileid failed");
			ret = FID_FAIL;
			goto out;
		}

		vs_info->vsi_last_datafid = vs_info->vsi_next_fid;
	}

	*fid = vs_info->vsi_last_datafid;

out:
	return ret;
}

/*
 * Converts the 1D index to 2D index and assigns new fid to existing entry
 */
static void
inm_vs_set_file_fid_sparse(inm_vs_info_t *vs_info, inm_u32_t fid,
			   inm_32_t fididx)
{
	ARRAY_1D_TO_2D(vs_info->vsi_fid_pages, fididx, MAX_FID_PER_PAGE) = fid;

}

/*
 * For sparse, retention data can be copied to any file. We maintain a sorted
 * array of unique keys generated from retention file name. We then perform a
 * binary search on the key array to determine if a new fid is required.
 * This is the heart of key array lookup an insert. Do a binary search on the
 * array. If search fails, insert such that the array is still sorted. The key
 * array is 2D array of pages which is simulated as a 1D array. so fid index
 * (fididx) returned is index within a 1D array of MAX_FID_KEYS.
 *
 * ARGUMENTS:
 * 	vs_info : 	vsnap_info structure
 *
 *	file_name : 	File name to lookup or insert
 *
 *	fid :		If lookup successful, return the fid
 *
 *	fididx :	If lookup failed, index of array where new name inserted
 *
 * RETURN:
 *	KA_SUCCESS :	Search successful, fid arg contains the file's
 * 			correspoding fid
 *
 *	KA_SUCCESS_ : 	Search failed and file successfully inserted
 * 	MODIFIED	into array for future reference
 *
 *	KA_FAIL:	Failure
 */

static enum FID_STATUS
inm_vs_array_lookup_and_insert(inm_vs_info_t *vs_info, char *filename,
			       inm_u32_t *fid, inm_32_t *fididx)
{
	fid_key_t *array;
	inm_u64_t key = 0;
	inm_u64_t endtime = 0;
	inm_u64_t starttime = 0;
	enum FID_STATUS ret = FID_SUCCESS_MODIFIED;
	inm_32_t idx = 0;
	inm_32_t num_full_pages;
	inm_32_t i;
	inm_32_t j;
	inm_32_t k;
	inm_32_t start = 0;

	/*
	 * If start_time is greater than vsnap recovery time, we are sure
	 * an entry exists in btree. We optimmise and do not process this
	 * update. Updates with start time same as the current on are still
	 * used as bookmark may vary.
	 */
	starttime = inm_strtoktoull(filename, sparse_time_vector_size,
				    sparse_start_time_vector);
	if ( starttime > vs_info->vsi_rec_time_yymmddhh ) {
		DEV_DBG("Start Time = %llu", starttime);
		ret = FID_NOT_REQD;
		*fid = SPARSE_FID_SPECIAL;
		*fididx = -1;
		goto out;
	}

	endtime = inm_strtoktoull(filename, sparse_time_vector_size,
				  sparse_end_time_vector);
	DEV_DBG("End Time = %llu", endtime);

	if ( endtime != vs_info->vsi_sparse_end_time ) {
		if( !endtime ) 		 /* insert pvt file */
			return FID_FAIL; /* caller should just continue
					    and not handle the error */

		/* We should not update header while initial population */
		if ( vs_info->vsi_sparse_end_time != SPARSE_ENDTIME_SPECIAL ) {
			DBG("End time changed, updating header");
			vs_info->vsi_sparse_num_keys = 0;

			/* update backward roll fid */
			vs_info->vsi_backward_fid = 0;

			if( vs_info->vsi_next_fid_used ) {
				vs_info->vsi_forward_fid =
						vs_info->vsi_next_fid + 1;
			} else {
				vs_info->vsi_forward_fid =
							vs_info->vsi_next_fid;
			}

			update_bt_hdr(vs_info);
		}

		DBG("Hour changed, %llu to %llu", vs_info->vsi_sparse_end_time,
		    endtime);
		vs_info->vsi_sparse_end_time = endtime;

	}


	key = inm_strtoktoull(filename, sparse_vector_size, sparse_vector);
	DEV_DBG("key = %llu", key);

	num_full_pages = vs_info->vsi_sparse_num_keys / MAX_KEYS_PER_PAGE;
	DEV_DBG("Checking %d pages", num_full_pages);

	/*
	 * The binary search could be much less complex using ARRAY_1D_TO_2D().
	 * However, following approach helps in identifying data for memmove
	 * and is preferred.
	 * NOTE: array is assigned with comma operator.
	 */
	for( idx = 0; array = vs_info->vsi_key_pages[idx], idx < num_full_pages;
	     idx++ ) {
		/*
		 * The keys are in sorted order. In case of multiple
		 * pages, we find the page where the key is located
		 * such that last key in array > present key, else we
		 * check the last page
		 */
		if ( INM_LAST_KEY(array) >= key )
			break;

	}

	/*
	 * In case the array is full and key is the largest key yet,
	 * we would have slipped onto the next non-existent page. In
	 * such cases, the appropriate slot is the last slot in the
	 * array as well as the last page.
	 */
	if( idx >= NUM_KEY_PAGES ) {
		ASSERT( idx != NUM_KEY_PAGES );
		ASSERT( vs_info->vsi_sparse_num_keys != MAX_FID_KEYS );
		DBG("Will have to overwrite the last slot");

		array = vs_info->vsi_key_pages[NUM_KEY_PAGES - 1];
		j = MAX_KEYS_PER_PAGE - 1;

		/* adjust return values */
		*fididx = MAX_FID_KEYS - 1;
		*fid = SPARSE_FID_SPECIAL;

	} else {

		*fididx = idx * MAX_KEYS_PER_PAGE;

		/* Use binary search to find the key */
		/* i = low, j = high, k = mid */
		i = -1;

		if ( idx < num_full_pages )
			j = MAX_KEYS_PER_PAGE;
		else
			j = vs_info->vsi_sparse_num_keys % MAX_KEYS_PER_PAGE;

		while ( 1 ) {
			k = (j - i) / 2;
			if ( 0 == k )
				break;

			k += i;

			if ( array[k] < key )
				i = k;
			else if ( array[k] > key )
				j = k;
			else {
				DEV_DBG("FID present");
				*fididx += k;
				*fid = ARRAY_1D_TO_2D(vs_info->vsi_fid_pages,
						      *fididx,MAX_FID_PER_PAGE);
				ret = FID_SUCCESS;
				goto out;
			}
		}

		/*
		 * If here, it means the key is not in the array and j(high)
		 * points to slot where we need to insert the key.
		 * Mark the fid and fididx return values
		 */

		*fididx += j;
		*fid = SPARSE_FID_SPECIAL;
	}

	DEV_DBG("insert the key at key[%d]", j);
	if( vs_info->vsi_sparse_num_keys != MAX_FID_KEYS ) {

		if ( *fididx < vs_info->vsi_sparse_num_keys ) {
			DEV_DBG("Current key at key[%d] = %llu",
				j, array[j]);
		}

		/*
		 * Make space to insert new key. We shift all keys by one
		 * starting from current page to the last page for space
		 */
		/*
		 * Move keys by one in all pages starting from last page
		 * and move the last key from previous page to top of the
		 * current page. Then move keys in the page where data needs
		 * to be inserted.
		 */
		for( i = num_full_pages; i >= idx; i-- ) {
			/*
			 * Calculate num of elements we want to move to
			 * ensure we dont overflow.
			 */
			if ( i == idx )
				start = j;
			else
				start = 0;

			if ( i == num_full_pages ) { /* Last Page */
				k = vs_info->vsi_sparse_num_keys %
					MAX_KEYS_PER_PAGE;
			} else {
				k = MAX_KEYS_PER_PAGE - 1; /* move n-1 keys */
			}

			k -= start;

			if( k ) {
				DEV_DBG("Moving %d keys in page[%d] from %d to \
					%d", k, i, start, start + 1);

				/* Move elements by 1 */
				memmove(&vs_info->vsi_key_pages[i][start + 1],
					&vs_info->vsi_key_pages[i][start],
					k * sizeof(fid_key_t));
			}

			if ( i != idx ) {
				/* copy last key of previous page to top of
				 * next page
				 */
				DEV_DBG("Copy last key from page %d", i - 1);
				vs_info->vsi_key_pages[i][0] =
				    INM_LAST_KEY(vs_info->vsi_key_pages[i - 1]);
			}
		}

		/* Copy the Key */
		array[j] = key;

		/*
		 * Now move the elements in fid array. No. of fid pages will
		 * be half the number of of key pages.
		 */
		num_full_pages = vs_info->vsi_sparse_num_keys/MAX_FID_PER_PAGE;
		idx = *fididx / MAX_FID_PER_PAGE;
		j = *fididx % MAX_FID_PER_PAGE;

		for( i = num_full_pages; i >= idx; i-- ) {
			/*
			 * Calculate num of elements we want to move to
			 * ensure we dont overflow.
			 */
			if ( i == idx )
				start = j;
			else
				start = 0;

			if ( i == num_full_pages ) { /* Last Page */
				k = vs_info->vsi_sparse_num_keys %
					MAX_FID_PER_PAGE;
			} else {
				k = MAX_FID_PER_PAGE - 1; /* move n-1 keys */
			}

			k -= start;

			if( k ) {

				DEV_DBG("Moving %d fid in page[%d] from %d to \
					%d", k, i, start, start + 1);

				/* Move elements by 1 */
				memmove(&vs_info->vsi_fid_pages[i][start + 1],
					&vs_info->vsi_fid_pages[i][start],
					k * sizeof(inm_u32_t));
			}

			if ( i != idx ) {
				/* copy last key of previous page to top of
				 * next page
				 */
				DEV_DBG("Copy last fid from page %d", i - 1);
				vs_info->vsi_fid_pages[i][0] =
			    vs_info->vsi_fid_pages[i - 1][MAX_FID_PER_PAGE - 1];
			}
		}

		ARRAY_1D_TO_2D(vs_info->vsi_fid_pages, *fididx,
			       MAX_FID_PER_PAGE) = SPARSE_FID_SPECIAL;

		vs_info->vsi_sparse_num_keys++;

	} else {
		DBG("No slot is free, overwrite");
		DBG("Overwriting slot %d, array slot %d, from key %llu"
		     "to key %llu", *fididx, j, array[j], key);
		array[j] = key;
		ARRAY_1D_TO_2D(vs_info->vsi_fid_pages, *fididx,
			       MAX_FID_PER_PAGE) = SPARSE_FID_SPECIAL;
	}

	DEV_DBG("Num Keys = %d", vs_info->vsi_sparse_num_keys);
	ret = FID_SUCCESS_MODIFIED;

out:
	return ret;
}

#ifdef _VSNAP_DEBUG_

void
inm_vs_lookup_file_fid_sparse(inm_vs_info_t *vs_info, char *filename)
{
	enum FID_STATUS ret;
	inm_u32_t fid;
	inm_32_t fididx;
	inm_u64_t endtime;

	if ( !INM_TEST_BIT(VS_SPARSE, &vs_info->vsi_flags) ) {
		UPRINT("Not Sparse Retention");
		goto out;
	}

	/* If end times dont match, dont lookup or cache gets flushed */
	endtime = inm_strtoktoull(filename, sparse_time_vector_size,
				  sparse_end_time_vector);

	if ( endtime != vs_info->vsi_sparse_end_time ) {
		UPRINT("End Time mismatch, Not in the cache");
		goto out;
	}

	ret = inm_vs_array_lookup_and_insert(vs_info, filename, &fid, &fididx);

	switch(ret) {
		case FID_NOT_REQD: 	UPRINT("Start greater, not in cache");
					break;

		case FID_SUCCESS:	if( fid == SPARSE_FID_SPECIAL )
						UPRINT("In cache, no associated"
							"fid");
					else
						UPRINT("Fid = %d", fid);
					break;

		case FID_SUCCESS_MODIFIED:
					UPRINT("Not present, Inserted");
					break;

		case FID_FAIL:		UPRINT("Cache failed");
					break;
	}

out:
	return;
}

#endif

/*
 * Lookup the array. If file was not present i.e. KA_SUCCESS_MODIFIED, send
 * new fid. If file already present, send its corresponding fid returned by
 * lookup/insert call.
 */
static enum FID_STATUS
inm_vs_get_file_fid_sparse(inm_vs_info_t *vs_info, char *file_name,
			   inm_u32_t *fid, inm_32_t *fididx)
{
	inm_32_t retval = 0;
	enum FID_STATUS ret;

	ret = inm_vs_array_lookup_and_insert(vs_info, file_name, fid, fididx);

	/* If the fid returned is special, assume freshly inserted into array */
	if( ret == FID_SUCCESS && *fid == SPARSE_FID_SPECIAL ) {
		DEV_DBG("In cache but not used, assign new fid");
		ret = FID_SUCCESS_MODIFIED;
	}

	switch(ret) {
		case FID_NOT_REQD: 	DEV_DBG("Update not needed");
					break;

		case FID_SUCCESS:	DEV_DBG("Sending old fid = %d", *fid);
					break;

		case FID_SUCCESS_MODIFIED:
					retval = set_next_data_fid(vs_info);
					if ( retval < 0 ) {
						ERR("set data fileid failed");
						ret = FID_FAIL;
						break;
					}

					DEV_DBG("Sending new fid = %d",
						vs_info->vsi_next_fid);
					*fid = vs_info->vsi_next_fid;
					break;

		case FID_FAIL:		break;
	}

	return ret;
}

static inm_32_t
update_map_for_this_vsnap(inm_vs_info_t *vs_info, inm_u64_t vol_offset,
			  inm_u32_t len, inm_u64_t file_offset,
			  char *file_name)
{
	inm_32_t retval = 0;
	enum FID_STATUS ret;
	inm_u32_t fid;
	inm_32_t fidx = -1;

	ret = vs_info->vsi_get_file_fid(vs_info, file_name, &fid, &fidx);
	if ( ret == FID_FAIL ) {
		ERR("get_file_fid failed");
		retval = -EINVAL;
		goto out;
	}

	if ( ret != FID_NOT_REQD ) {
		DEV_DBG("update the map for [%lld, %d]", vol_offset, len);
		retval = update_btree_map(vs_info, vol_offset, len, file_offset,
				  file_name, fid, fidx);
		if (retval < 0) {
			ERR("update map failed");
		}
	} else {
		DEV_DBG("Update Not required");
	}
  out:
	return retval;
}

void
inm_vs_update_vsnap(vsnap_task_t *task)
{
	update_data_t			*upd_data = task->vst_task_data;
	update_data_common_t		*upd_common = upd_data->ud_common;
	inm_list_head_t			*tmp;
	inm_list_head_t			*temp;
	inm_u64_t			offset;
	inm_32_t			retval = 0;
	inm_vdev_page_data_t		page_data;
	inm_vs_update_page_list_t	*update_page = NULL;

	INM_FREE(&task, sizeof(vsnap_task_t));

	INM_WRITE_LOCK(upd_data->ud_vs_info->vsi_btree_lock);
	if (!upd_common->udc_cow) {
		DEV_DBG("retention available");
		retval = update_map_for_this_vsnap(upd_data->ud_vs_info,
						 upd_common->udc_vol_offset,
						 upd_common->udc_len,
						 upd_common->udc_file_offset,
						 upd_common->udc_data_filename);
		if (retval < 0) {
			ERR("update for one vsnap failed - %d",retval);
			ALERT();
			goto out;
		}
	} else {
		DEV_DBG("retention not available");
		offset = upd_common->udc_vol_offset;

		/*
		 * loop thro' each page in the list & cow the data
		 * ToDo - to change the entire rw interface from using
		 * only page to use multiple pages
		 */
		inm_list_for_each_safe(temp, tmp, upd_common->udc_page_list_head) {

			update_page =
			    inm_list_entry(temp, inm_vs_update_page_list_t,
				       up_next);

			page_data.pd_bvec_page = update_page->up_data_page;
			page_data.pd_bvec_offset = 0;
			page_data.pd_disk_offset = (inm_offset_t)offset;
			page_data.pd_file_offset = 0;
			page_data.pd_size = update_page->up_len;

			retval = write_using_map(page_data,
						 upd_data->ud_vs_info, 1);

			if ( retval < 0 )
				break;

			offset += update_page->up_len;

		}

	}

out:
	INM_WRITE_UNLOCK(upd_data->ud_vs_info->vsi_btree_lock);

	if ( retval < 0 ) {
		/*
		 * We dont care for sync with other threads, we
		 * return atleast one return code which might help
		 */
		upd_common->udc_error = retval;
	}

	INM_FREE(&upd_data, sizeof(update_data_t));

	if ( INM_ATOMIC_DEC_AND_TEST(&upd_common->udc_updates_inflight) )
		INM_WAKEUP(&upd_common->udc_update_ipc);

	return;
}

/*
 * inm_vs_update_for_parent
 *
 * DESC
 *      core API to perform btree map updates to the child vsnap volumes
 *      when its parent volume gets modified.
 *      (called when UPDATE IOCTL is invoked)
 *
 * ALGO
 */
inm_32_t
inm_vs_update_for_parent(inm_u64_t vol_offset, inm_u32_t len,
			 inm_u64_t file_offset, char *data_filename,
			 char *parent_devpath, inm_u32_t cow)
{
	inm_32_t			retval = 0;
	inm_u32_t			bytes_read = 0;
	inm_u64_t			offset;
	inm_vs_info_t			*vs_info = NULL;
	inm_list_head_t			*tmp;
	inm_list_head_t			*ptr = NULL;
	inm_list_head_t			*nextptr = NULL;
	inm_list_head_t			*page_list_head;
	inm_vdev_page_data_t		*page_data;
	inm_vs_parent_list_t		*parent_vollist = NULL;
	inm_vs_parent_change_t		*parent_change = NULL;
	inm_vs_update_page_list_t	*update_page = NULL;
	void 				*file_handle = NULL;
	update_data_common_t		*upd_common = NULL;
	update_data_t			*upd_data = NULL;
	vsnap_task_t			*task = NULL;

	if (!parent_vollist_lock) {
		ERR("lock not found.");
		retval = -EINVAL;
		goto out;
	}
	INM_READ_LOCK(parent_vollist_lock);

	DEV_DBG("retrieve a parent entry from given path - %s", parent_devpath);
	parent_vollist = retreive_parent_from_path(parent_devpath);
	if (!parent_vollist) {
		DEV_DBG("no parent");
		goto no_vsnap_label;	/* not a failure & so retval = 0 */
	}
	DEV_DBG("parent structure found");

	/*
	 * create a list of pages and copy the old data from the parent
	 * volume if retention was not enabled
	 */
	page_list_head = INM_ALLOC(sizeof(inm_list_head_t), GFP_NOIO);
	if (!page_list_head) {
		ERR("no mem");
		retval = -ENOMEM;
		goto list_head_alloc_failed;
	}
	INM_INIT_LIST_HEAD(page_list_head);

	/*
	 * copy the old data from the parent volume in the page_data format;
	 * for finally it is this structure that will  be used for IO
	 * transfers & as well as btree updates
	 */
	page_data = INM_ALLOC(sizeof(inm_vdev_page_data_t), GFP_NOIO);
	if (!page_data) {
		ERR("no mem.");
		retval = -ENOMEM;
		goto page_data_alloc_failed;
	}

	/*
	 * if cow was specified then retention is not enabled
	 */
	if (cow) {
		DEV_DBG("no retention");
		offset = vol_offset;

		retval = INM_OPEN_FILE(parent_devpath, INM_RDONLY | INM_LYR,
					&file_handle);
		if ( retval < 0 ) {
			ERR("Cannot open parent %s", parent_devpath);
			goto parent_open_failed;
		}

		/*
		 * break the input data into pages and create a list of them
		 */
		while (len) {
			inm_u32_t read_len = len;

			bytes_read = 0;
			if (read_len >= INM_PAGE_SIZE) {
				read_len = INM_PAGE_SIZE;
			}

			/*
			 * this is done in a loop, so the same page_data
			 * variable is reused. the freeing happens much later.
			 */
			page_data->pd_bvec_page = (inm_page_t *)alloc_page(GFP_NOIO);
			if ( !page_data->pd_bvec_page ) {
				ERR("No mem");
				goto page_alloc_failed;
			}
			page_data->pd_bvec_offset = 0;
			page_data->pd_disk_offset = 0;
			page_data->pd_file_offset = offset;
			page_data->pd_size = read_len;
			DEV_DBG("read (%lld, %d) from vol",offset, read_len);

			retval = read_from_parent_volume(file_handle,
							 page_data);
			if (retval < 0) {
				ERR("update failed while read");
				goto page_list_failed;
			}

			/*
			 * maintain these pages containing the data from the
			 * parent volume in a list; this list will used to
			 * copy the data into the private write file
			 */
			update_page =
			    INM_ALLOC(sizeof(inm_vs_update_page_list_t),
				      GFP_NOIO);
			if ( !update_page ) {
				ERR("No mem");
				goto update_alloc_failed;
			}
			update_page->up_data_page = page_data->pd_bvec_page;
			update_page->up_len = read_len;
			inm_list_add_tail(&update_page->up_next, page_list_head);
			len -= read_len;
			offset += read_len;
		}

		INM_CLOSE_FILE(file_handle, INM_RDONLY | INM_LYR);
	}

	INM_FREE(&page_data, sizeof(inm_vdev_page_data_t));
	page_data = NULL;

	upd_common = INM_ALLOC(sizeof(update_data_common_t), GFP_NOIO);
	if ( !upd_common ) {
		ERR("No mem");
		goto upd_common_alloc_failed;
	}

	upd_common->udc_page_list_head = page_list_head;
	INM_INIT_IPC(&upd_common->udc_update_ipc);
	INM_ATOMIC_SET(&upd_common->udc_updates_inflight,
		       parent_vollist->pl_num_vsnaps);
	upd_common->udc_vol_offset = vol_offset;
	upd_common->udc_file_offset = file_offset;
	upd_common->udc_data_filename = data_filename;
	upd_common->udc_len = len;
	upd_common->udc_cow = cow;
	upd_common->udc_error = 0;

	/*
	 * store the (offset,len) tuple for which this update has been invoked.
	 * this is supposed to be used to delay the read_code_path to delay
	 * processing if there was any overlap between the (off,len) tuple it
	 * was processing and this (off,len) tuple.
	 * but as a fix for 4483/4/5, this delay was removed. now the ToDo is
	 * to use this chain & update the btree map simulatenously.
	 */
	parent_change = NULL;
	DEV_DBG("insert into the parent's offset list");
	retval = insert_change_in_parent_list(parent_vollist, vol_offset, len,
					      &parent_change);
	if (retval < 0) {
		ERR("insert offset info failed");
		goto insert_parent_change_failed;
	}

	DEV_DBG("Updates for %d vsnaps",
	    INM_ATOMIC_READ(&upd_common->udc_updates_inflight));
	/*
	 * pick each of the vsnaps from the parent's list of vsnaps and apply
	 * the changes to its btree. if retention was not enabled (i.e., we
	 * did the CoW) then write the CoW-ed data into the vsnap else
	 * update only the btree map with the new info
	 */
	INM_ACQ_MUTEX(&parent_change->pc_mutex);
	INM_INIT_SLEEP(&upd_common->udc_update_ipc);

	inm_list_for_each_safe(ptr, nextptr, &parent_vollist->pl_vsnap_list) {

		task = INM_ALLOC(sizeof(vsnap_task_t), GFP_NOIO);
		if ( !task ) {
			ERR("task alloc failed");
			goto task_alloc_failed;
		}

		upd_data = INM_ALLOC(sizeof(update_data_t), GFP_NOIO);
		if ( !upd_data ) {
			ERR("upd alloc failed");
			goto upd_data_alloc_failed;
		}

		vs_info = inm_list_entry(ptr, inm_vs_info_t, vsi_next);

		upd_data->ud_vs_info = vs_info;
		upd_data->ud_common = upd_common;

		task->vst_task_id = INM_TASK_UPDATE_VSNAP;
		task->vst_task_data = upd_data;

		ADD_TASK_TO_POOL_HEAD(vs_update_pool, task);
	}

	/* Wait for thread to pick up the task and notify us */
	INM_SLEEP(&upd_common->udc_update_ipc);
	INM_FINI_SLEEP(&upd_common->udc_update_ipc);

	INM_REL_MUTEX(&parent_change->pc_mutex);

	/*
	 * Wait for all target i/o to complete. Once we acquire write lock,
	 * we are sure there are no inflight I/O to target volume
	 */
	INM_WRITE_LOCK(&parent_vollist->pl_target_io_done);
	INM_WRITE_UNLOCK(&parent_vollist->pl_target_io_done);

	DEV_DBG("remove the change from the parent's offset list");
	remove_change_from_parent_list(parent_vollist, parent_change);
	parent_change = NULL;

	retval = upd_common->udc_error;
	DEV_DBG("Updates over, rc = %d", retval);

  insert_parent_change_failed:
	INM_FREE(&upd_common, sizeof(update_data_common_t));

  upd_common_alloc_failed:
	inm_list_for_each_safe(ptr, tmp, page_list_head) {
		update_page = inm_list_entry(ptr, inm_vs_update_page_list_t,
					 up_next);
		__free_page(update_page->up_data_page);
		inm_list_del(ptr);
		INM_FREE(&update_page, sizeof(inm_vs_update_page_list_t));
	}

  parent_open_failed:
	if ( page_data )
		INM_FREE(&page_data, sizeof(inm_vdev_page_data_t));

  page_data_alloc_failed:
	INM_FREE(&page_list_head, sizeof(inm_list_head_t));

  list_head_alloc_failed:
  no_vsnap_label:
	INM_READ_UNLOCK(parent_vollist_lock);

  out:
	return retval;

  upd_data_alloc_failed:

  /*
   * We have done an alloc_page which needs to be freed
   * before we start with the usual cleanup. Also, the device
   * file needs to be closed
   */
  update_alloc_failed:
	INM_FREE(&task, sizeof(vsnap_task_t));

  task_alloc_failed:
	INM_REL_MUTEX(&parent_change->pc_mutex);

  page_list_failed:
	__free_page(page_data->pd_bvec_page);
  page_alloc_failed:
	INM_CLOSE_FILE(file_handle, INM_RDONLY | INM_LYR);
	goto insert_parent_change_failed;


}

/*
 * inm_vs_read_from_vsnap
 *
 * DESC
 *      vsnap volume read API exported to the invirvol driver
 */
inm_32_t
inm_vs_read_from_vsnap(void *vol_context, inm_offset_t disk_offset,
		       inm_u32_t size, void *retention_data_list)
{
	inm_vs_info_t *vs_info = (inm_vs_info_t *)vol_context;

        return read_using_map(vs_info, (inm_u64_t)disk_offset, size,
			      (lookup_ret_data_t *)retention_data_list);
}

/*
 * inm_vs_write_to_vsnap
 *
 * DESC
 *      vsnap volume write API exported to the invirvol driver
 */
inm_32_t
inm_vs_write_to_vsnap(inm_vdev_page_data_t page_data, void *vol_context)
{
	inm_vs_info_t *vs_info = (inm_vs_info_t *)vol_context;

	if (vs_info->vsi_access_type == VSNAP_READ_ONLY) {
		DEV_DBG("No write access.");
		return 0; /* its success never the less */
	}

        return write_using_map(page_data, vs_info, 0);
}

#ifdef _VSNAP_DEBUG_
/*
 * dump_mount_data
 *
 * DESC
 *       dump the entire mount info structre
 *      (for the purpose of debugging)
 */
static INLINE void
dump_mount_data(inm_vs_mdata_t *minfo)
{
	DBG("Mount Data->");
	DBG("\tret path   - %s", minfo->md_retention_path);
	DBG("\tdata path  - %s", minfo->md_data_log_path);
	DBG("\tmnt_path   - %s", minfo->md_mnt_path);
	DBG("\tparent dev - %s", minfo->md_parent_dev_path);
	DBG("\tparent size- %lld", minfo->md_parent_vol_size);
	DBG("\tsnapshot id- %lld", minfo->md_snapshot_id);
	DBG("\tdata fid   - %d", minfo->md_next_fid);
	DBG("\trecovery   - %lld", minfo->md_recovery_time);
	DBG("in ret/priv  - %d", minfo->md_is_map_in_retention_path);
}

#endif

/*
 * vs_info_ctr
 *
 * DESC
 *	constructor for vs_info structure
 */
static inm_vs_info_t*
vs_info_ctr(void)
{
	inm_vs_info_t	*vs_info = NULL;
	inm_32_t i = 0;

	vs_info = INM_ALLOC(sizeof(inm_vs_info_t), GFP_KERNEL);
	if (!vs_info) {
		ERR("no mem.");
		goto out;
	}

	vs_info->vsi_btree_lock = INM_ALLOC(sizeof(inm_rwlock_t), GFP_KERNEL);
	if (!vs_info->vsi_btree_lock) {
		ERR("no mem for lock.");
		INM_FREE(&vs_info, sizeof(inm_vs_info_t));
		vs_info = NULL;
	}

	vs_info->vsi_btree_filp = NULL;
	vs_info->vsi_fid_table_filp = NULL;
	vs_info->vsi_last_datafile = NULL;
	vs_info->vsi_data_log_path = NULL;
	vs_info->vsi_mnt_path = NULL;
	vs_info->vsi_writefile_filp = NULL;
	vs_info->vsi_target_filp = NULL;
	vs_info->vsi_btree = NULL;

	/* For sparse */
	vs_info->vsi_flags = 0;
	vs_info->vsi_rec_time_yymmddhh = 99999999;
	vs_info->vsi_sparse_end_time = SPARSE_ENDTIME_SPECIAL;
	vs_info->vsi_sparse_num_keys = 0;

	for( i = 0; i < NUM_KEY_PAGES; i++ )
		vs_info->vsi_key_pages[i] = NULL;

	for( i = 0; i < NUM_FID_PAGES; i++ )
		vs_info->vsi_fid_pages[i] = NULL;

	vs_info->vsi_get_file_fid = NULL;
	vs_info->vsi_set_file_fid = NULL;
	vs_info->vsi_populate_fid = NULL;

  out:
	return vs_info;
}

/*
 * vs_info_dtr
 *
 * DESC
 *	destructor for vs_info structure
 */
static void
vs_info_dtr(inm_vs_info_t *vs_info)
{
	if (vs_info->vsi_btree_lock)
		INM_FREE(&vs_info->vsi_btree_lock, sizeof(inm_rwlock_t));
	INM_FREE(&vs_info, sizeof(inm_vs_info_t));
}

/*
 * open_write_data_file
 *
 * DESC
 *	open the "write_data" file and populate the field of vs_info
 */
static inm_32_t
open_write_data_file(char path[2048], inm_u64_t snapshot_id,
		     inm_vs_info_t *vs_info)
{
	inm_32_t retval = 0;
	char *file_name = NULL;

	if (path[0] == '\0') {
		ERR("null path.");
		retval = -ENXIO;
		goto out;
	}

	file_name = INM_ALLOC(MAX_FILENAME_LENGTH, GFP_KERNEL);
	if (!file_name) {
		ERR("no mem.");
		retval = -ENOMEM;
		goto out;
	}

	STRING_PRINTF((file_name, MAX_FILENAME_LENGTH, "%s/%llu_WriteData%d",
		       path, snapshot_id, vs_info->vsi_writefile_suffix));
	retval = INM_OPEN_FILE(file_name, INM_RDWR | INM_CREAT,
			   &vs_info->vsi_writefile_filp);
	if (retval != 0) {
		ERR("failed to open %s.", file_name);
	}

	INM_FREE(&file_name, MAX_FILENAME_LENGTH);

  out:
	return retval;
}

/*
 * open_map_files
 *
 * DESC
 *	open the "vsnap_map" file and populate the field of vs_info
 */
static inm_32_t
open_map_files(char path[2048], inm_u64_t snapshot_id, inm_vs_info_t *vs_info)
{
	inm_32_t retval = 0;
	char *file_name = NULL;

	if (path[0] == '\0') {
		ERR("null path.");
		retval = -ENXIO;
		goto out;
	}

	file_name = INM_ALLOC(MAX_FILENAME_LENGTH, GFP_KERNEL);
	if (!file_name) {
		ERR("no mem.");
		retval = -ENOMEM;
		goto out;
	}

	/*
	 * open the btree map file and store the file pointer in vsnap info
	 * structure's btree_filp field
	 */
	STRING_PRINTF((file_name, MAX_FILENAME_LENGTH, "%s/%llu_VsnapMap",
		       path, snapshot_id));
	retval = INM_OPEN_FILE(file_name, INM_RDWR, &vs_info->vsi_btree_filp);
	if (retval != 0) {
		ERR("failed to open %s.", file_name);
		goto error;
	}

	/*
	 * open the fid table file and store the file pointer in vsnap info
	 * structure's fid_table_filp field
	 */
	STRING_PRINTF((file_name, MAX_FILENAME_LENGTH, "%s/%llu_FileIdTable",
		      path, snapshot_id));
	retval = INM_OPEN_FILE(file_name, INM_RDWR,
			       &vs_info->vsi_fid_table_filp);
	if (retval != 0) {
		ERR("failed to open %s.", file_name);
	}

  error:
	INM_FREE(&file_name, MAX_FILENAME_LENGTH);

  out:
	return retval;

}

/*
 * This can be called by cdp retention or when retention is not enabled. If
 * retention is not enabled, we expect vs_info->vsi_next_fid_used == 0 so no
 * harm should be done
 */
static inm_32_t
inm_vs_populate_fid_cdp(inm_vs_info_t *vs_info, inm_vs_hdr_info_t *hd_info)
{
	inm_32_t retval = 0;
	char *file_name = NULL;

	file_name = INM_ALLOC(MAX_RETENTION_DATA_FILENAME_LENGTH, GFP_KERNEL);
	if (!file_name) {
		ERR("no mem.");
		retval = -ENOMEM;
		goto out;
	}

	vs_info->vsi_last_datafile = file_name;

	vs_info->vsi_last_datafile[0] = '\0';

	DEV_DBG("CDP vsnap, populating last_datafile");
	if ( vs_info->vsi_next_fid_used ) {

		vs_info->vsi_last_datafid = vs_info->vsi_next_fid;

		if( vs_info->vsi_next_fid ) {
			fid_to_filename(vs_info, vs_info->vsi_next_fid,
					file_name);

		}
	} else {
		vs_info->vsi_last_datafid = vs_info->vsi_next_fid - 1;
	}

out:
	return retval;
}

static inm_32_t
inm_vs_populate_fid_sparse(inm_vs_info_t *vs_info, inm_vs_hdr_info_t *hd_info)
{
	inm_32_t 		retval = 0;
	inm_32_t 		fididx = -1;
	inm_u32_t		fid = 0;
	inm_u32_t 		i;
	enum FID_STATUS 	ret;
	char			*file_name;

	DBG("Allocating %d pages for keys", NUM_KEY_PAGES);
	for( i = 0; i < NUM_KEY_PAGES; i++ ) {
		vs_info->vsi_key_pages[i] = INM_ALLOC(INM_PAGE_SIZE,GFP_KERNEL);
		if( !vs_info->vsi_key_pages[i] ) {
			retval = -ENOMEM;
			goto alloc_key_array_failed;
		}
	}

	DBG("Allocating %d pages for fid", NUM_FID_PAGES);
	for( i = 0; i < NUM_FID_PAGES; i++ ) {
		vs_info->vsi_fid_pages[i] = INM_ALLOC(INM_PAGE_SIZE,GFP_KERNEL);
		if( !vs_info->vsi_fid_pages[i] ) {
			retval = -ENOMEM;
			goto alloc_fid_array_failed;
		}
	}

	DEV_DBG("Sparse retention, populating key array");
	file_name = INM_ALLOC(MAX_RETENTION_DATA_FILENAME_LENGTH, GFP_KERNEL);
	if (!file_name) {
		ERR("no mem.");
		retval = -ENOMEM;
		goto file_name_alloc_failed;
	}

	/* Roll backward and populate. This is done on initial create of vsnap */
	DBG("Backward Fid = %u", hd_info->hinfo_sparse_backward_fid);
	i = hd_info->hinfo_sparse_backward_fid;
	while( i ) {
		if( i == vs_info->vsi_write_fid ) {
			DBG("Skipping PVT file - should not happen");
			i--;
			continue;
		}

		retval = fid_to_filename(vs_info, i--, file_name);
		DEV_DBG("%u--->%s", i + 1, file_name);

		/* If we fail in reading filename, display an error message
		 * and continue.
		 */
		if ( retval < 0 ) {
			ERR("Cannot read filename in backward roll");
			retval = 0;
			continue;
		}

		ret = inm_vs_array_lookup_and_insert(vs_info, file_name, &fid,
						     &fididx);
		if ( ret == FID_FAIL ) {
			DBG("Cannot insert %s into array", file_name);
			continue;
		}

		/* it may happen that some files with time > start time are
		 * populated by user space as they roll backward. In such a
		 * scenario, we do not insert that file into the cache. set
		 * fid only when insert ito the array succeeded.
		 */
		if ( ret == FID_SUCCESS_MODIFIED ) {
			DBG("Set FID for %s to %u at %d", file_name, i + 1,
			    fididx);

			/* Mark the fid against the filename */
			vs_info->vsi_set_file_fid(vs_info, i + 1, fididx);
		}
	}

	DBG("Inserted %u fid in backward roll",
	    hd_info->hinfo_sparse_backward_fid);

	/* Roll forward and populate. */
	DBG("Forward Fid = %u", hd_info->hinfo_sparse_forward_fid);

	i = hd_info->hinfo_sparse_forward_fid;
	while( 1 ) {

		if( i == vs_info->vsi_write_fid ) {
			DBG("Skipping PVT file");
			i++;
			continue;
		}

		retval = fid_to_filename(vs_info, i++, file_name);
		if ( retval < 0 ) {
			retval = 0;
			break;
		}

		ret = inm_vs_array_lookup_and_insert(vs_info, file_name, &fid,
						     &fididx);
		if ( ret == FID_FAIL ) {
			DBG("Cannot insert %s into array", file_name);
			continue;
		}

		if ( ret == FID_SUCCESS_MODIFIED ) {
			DBG("Set FID for %s to %u at %d", file_name, i - 1,
			    fididx);

			/* Mark the fid against the filename */
			vs_info->vsi_set_file_fid(vs_info, i - 1, fididx);
		}
	}

	DBG("Inserted %u fids in forward roll",
	    i - hd_info->hinfo_sparse_forward_fid);

	INM_FREE(&file_name, MAX_RETENTION_DATA_FILENAME_LENGTH);

	vs_info->vsi_backward_fid = hd_info->hinfo_sparse_backward_fid;
	vs_info->vsi_forward_fid = hd_info->hinfo_sparse_forward_fid;

out:
	return retval;

file_name_alloc_failed:
alloc_fid_array_failed:
	for( i = 0; i < NUM_FID_PAGES; i++ ) {
		if( vs_info->vsi_fid_pages[i] )
			INM_FREE(&vs_info->vsi_fid_pages[i],
				 INM_PAGE_SIZE);
	}

alloc_key_array_failed:
	for( i = 0; i < NUM_KEY_PAGES; i++ ) {
		if( vs_info->vsi_key_pages[i] )
			INM_FREE(&vs_info->vsi_key_pages[i],
				 INM_PAGE_SIZE);
	}


	goto out;
}

/*
 * fill_vs_info_from_bt_hdr
 *
 * DESC
 *      fill information from btree header into vs_info structure
 */
static inm_32_t
fill_vs_info_from_bt_hdr(inm_vs_info_t *vs_info)
{
	char			*btree_hdr = NULL;
	char			*file_name = NULL;
	inm_32_t		retval = 0;
	inm_vs_hdr_info_t	*hdr_info = NULL;
	inm_u64_t		fidfile_offset;

	if (!vs_info) {
		ERR("vs_info is NULL.");
		retval = -ENXIO;
		goto out;
	}
	btree_hdr = INM_ALLOC(BTREE_HEADER_SIZE, GFP_KERNEL);
	if (!btree_hdr) {
		ERR("no mem.");
		retval = -ENOMEM;
		goto out;
	}
	hdr_info = (inm_vs_hdr_info_t *)(btree_hdr +
					 BTREE_HEADER_OFFSET_TO_USE);

	file_name = INM_ALLOC(MAX_RETENTION_DATA_FILENAME_LENGTH, GFP_KERNEL);
	if (!file_name) {
		ERR("no mem.");
		retval = -ENOMEM;
		goto file_name_alloc_failed;
	}

	DEV_DBG("initialize the bt from btree map file");
	retval = init_bt_from_filp(vs_info->vsi_btree_filp,
				   &vs_info->vsi_btree);
	if (retval < 0) {
		ERR("init_bt_from_filp failed");
		goto btree_init_failed;
	}
	DEV_DBG("copy the btree hdr");
	retval = inm_bt_copy_hdr(vs_info->vsi_btree, btree_hdr);
	if (retval < 0) {
		ERR("copy hdr failed");
		goto copy_hdr_failed;
	}
	DBG("Data Fid = %u", hdr_info->hinfo_data_fid);
	DBG("Write FID = %u", hdr_info->hinfo_write_fid);
	DBG("Write File Suffix = %u", hdr_info->hinfo_writefile_suffix);
	DBG("Backward Fid = %u", hdr_info->hinfo_sparse_backward_fid);
	DBG("Forward Fid = %u", hdr_info->hinfo_sparse_forward_fid);

	retval = INM_SEEK_FILE_END(vs_info->vsi_fid_table_filp,&fidfile_offset);
	if( retval < 0 ) {
		INFO("Cannot determine last fid through file size, using"
		     "using old method");
		retval = 0;

		vs_info->vsi_next_fid = hdr_info->hinfo_data_fid;
		DEV_DBG("check if next_fid[%d] is in use", vs_info->vsi_next_fid);

		retval = fid_to_filename(vs_info, vs_info->vsi_next_fid, file_name);
		if (retval < 0) {
			DEV_DBG("next_fid[%d] not in use", vs_info->vsi_next_fid);
			vs_info->vsi_next_fid_used = 0;
			retval = 0; /* reset the return value */
		} else {
			vs_info->vsi_next_fid_used = 1;
		}

	} else {
		/* Cannot use fidfile_offset after do_div, careful */
		do_div(fidfile_offset,
		       (inm_u64_t)MAX_RETENTION_DATA_FILENAME_LENGTH);

		vs_info->vsi_next_fid = (inm_u32_t)fidfile_offset;
		vs_info->vsi_next_fid_used = 1;
	}

	DBG("Next FID = %u, Used = %u", vs_info->vsi_next_fid,
		vs_info->vsi_next_fid_used);

	vs_info->vsi_write_fid = hdr_info->hinfo_write_fid;
	vs_info->vsi_writefile_suffix = hdr_info->hinfo_writefile_suffix;

	retval = vs_info->vsi_populate_fid(vs_info, hdr_info);
	if ( retval < 0 ) {
		ERR("Cannot populate FID data structure");
		goto populate_fid_failed;
	}


  btree_init_failed:
	INM_FREE(&file_name, MAX_RETENTION_DATA_FILENAME_LENGTH);

  file_name_alloc_failed:
	INM_FREE(&btree_hdr, BTREE_HEADER_SIZE);

  out:
	return retval;

  populate_fid_failed:
  copy_hdr_failed:
	bt_uninit(&vs_info->vsi_btree);
	goto btree_init_failed;

}


/*
 * inm_vs_init_vsnap_info
 *
 * DESC
 *      called by the driver during a new mount for recovery or pit vsnap.
 *      initialize the vsnap structure, create a target volume structure if
 *      one doesn't exist already cache the handles of btree map file, file id
 *      to file name conversion file for faster access
 *
 * ALGO
 */
inm_32_t
inm_vs_init_vsnap_info(inm_vs_mdata_t *mdata, inm_u32_t buf_len,
		       inm_64_t *vol_size, void **vdev_private)
{
	inm_32_t	retval = 0;
	inm_vs_info_t	*vs_info = NULL;

	ASSERT(!mdata);
	if (!(vs_info = vs_info_ctr())) {
		ERR("no mem.");
		retval = -ENOMEM;
		goto out;
	}

	if ( mdata->md_sparse == (unsigned char)1 ) {
		DBG("Vsnap with sparse retention");
		INM_SET_BIT(VS_SPARSE, &vs_info->vsi_flags);
		vs_info->vsi_populate_fid = inm_vs_populate_fid_sparse;
		vs_info->vsi_get_file_fid = inm_vs_get_file_fid_sparse;
		vs_info->vsi_set_file_fid = inm_vs_set_file_fid_sparse;
	} else {
		DBG("Vsnap with cdp retention");
		INM_SET_BIT(VS_CDP, &vs_info->vsi_flags);
		vs_info->vsi_populate_fid = inm_vs_populate_fid_cdp;
		vs_info->vsi_get_file_fid = inm_vs_get_file_fid_cdp;
		vs_info->vsi_set_file_fid = NULL;
	}

	vs_info->vsi_rec_time_yymmddhh =
		inm_convert_to_yymmddhh(mdata->md_recovery_time);
	DBG("Recovery Time (yymmddhh) = %llu", vs_info->vsi_rec_time_yymmddhh);

	/*
	 * open the btree map file & fid table file
	 */
	DEV_DBG("open the map & fid_table files");
	if (mdata->md_is_map_in_retention_path) {
		DEV_DBG("\t...from retention path");
		retval = open_map_files(mdata->md_retention_path,
					mdata->md_snapshot_id, vs_info);
	} else { /* files in priv dir */
		DEV_DBG("\t...from data log path");
		retval = open_map_files(mdata->md_data_log_path,
					mdata->md_snapshot_id, vs_info);
	}
	if (retval < 0) {
		ERR("failed to open map files");
		goto err_exit;
	}

	DEV_DBG("fill vsnap info from the btree header");
	retval = fill_vs_info_from_bt_hdr(vs_info);
	if (retval < 0) {
		ERR("fill_vs_info_from_bt_hdr failed.");
		goto err_exit;
	}
	vs_info->vsi_btree_root_offset =mdata->md_bt_root_offset;
	vs_info->vsi_snapshot_id = mdata->md_snapshot_id;
	vs_info->vsi_recovery_time = mdata->md_recovery_time;
	vs_info->vsi_is_map_in_retention_path =
	    mdata->md_is_map_in_retention_path;
	vs_info->vsi_access_type = mdata->md_access_type;
	DEV_DBG("open the writedata file");
	if (mdata->md_is_map_in_retention_path) {
		retval = open_write_data_file(mdata->md_retention_path,
					      mdata->md_snapshot_id, vs_info);
	} else {
		retval = open_write_data_file(mdata->md_data_log_path,
					      mdata->md_snapshot_id, vs_info);
	}
	if (retval < 0) {
		ERR("failed to open writefile.");
		goto err_exit;
	}

	if (mdata->md_is_tracking_enabled) {
		DEV_DBG("tracking enabled");
		vs_info->vsi_is_tracking_enabled = 1;
		vs_info->vsi_tracking_id = 1;
	} else {
		DEV_DBG("tracking not enabled");
		vs_info->vsi_is_tracking_enabled = 0;
		vs_info->vsi_tracking_id = 0;
	}

	if (mdata->md_mnt_path[0] != '\0') {
		vs_info->vsi_mnt_path =
		    INM_ALLOC(strlen(mdata->md_mnt_path) + 1, GFP_KERNEL);
		if (!vs_info->vsi_mnt_path) {
			ERR("no mem");
			retval = -ENOMEM;
			goto err_exit;
		}
		STR_COPY(vs_info->vsi_mnt_path, mdata->md_mnt_path,
			    strlen(mdata->md_mnt_path) + 1);
	}
	if (mdata->md_data_log_path[0] != '\0') {
		vs_info->vsi_data_log_path =
		    INM_ALLOC(strlen(mdata->md_data_log_path) + 1, GFP_KERNEL);
		if (!vs_info->vsi_data_log_path) {
			ERR("no mem");
			retval = -ENOMEM;
			goto err_exit;
		}
		STR_COPY(vs_info->vsi_data_log_path,
			    mdata->md_data_log_path,
			    strlen(mdata->md_data_log_path) + 1);
	}
	INM_INIT_LOCK(vs_info->vsi_btree_lock);
	DEV_DBG("parent vol size - %lld", mdata->md_parent_vol_size);
	*vol_size = mdata->md_parent_vol_size;

	retval = INM_OPEN_FILE(mdata->md_parent_dev_path, INM_RDONLY | INM_LYR,
				&vs_info->vsi_target_filp);
	if (retval != 0) {
		ERR("opening target %s failed.", mdata->md_parent_dev_path);
		goto err_exit;
	}


	/*
	 * init done, add this vsnap instance to the parent's list of vsnaps
	 */
	DEV_DBG("insert into parent's list of vsnap");
	retval = insert_into_parent_vsnap_list(mdata->md_parent_dev_path,
					       vs_info,
					       mdata->md_retention_path);
	if (retval < 0) {
		ERR("insert_into_parent_vsnap_list failed.");
		goto err_exit;
	}

	/*
	 * set parent ptr volume sector size
	 */
	if (!vs_info->vsi_parent_list->pl_sect_size)
		vs_info->vsi_parent_list->pl_sect_size =
		    (mdata->md_parent_sect_size?  mdata-> md_parent_sect_size:
		     DEFAULT_VOLUME_SECTOR_SIZE);

	*vdev_private = vs_info;

	/* We can safely create update thread without worrying about it much */
	if ( inm_thread_create(vs_update_pool_info) < 0 )
		ERR("Cannot create update thread");


  out:
	return retval;

  err_exit:
	if ( vs_info->vsi_target_filp )
		INM_CLOSE_FILE(vs_info->vsi_target_filp, INM_RDONLY | INM_LYR);
	if ( vs_info->vsi_btree )
		bt_uninit(&vs_info->vsi_btree);
	if (vs_info->vsi_btree_filp)
		INM_CLOSE_FILE(vs_info->vsi_btree_filp, INM_RDWR);
	if (vs_info->vsi_fid_table_filp)
		INM_CLOSE_FILE(vs_info->vsi_fid_table_filp, INM_RDWR);
	if (vs_info->vsi_writefile_filp)
		INM_CLOSE_FILE(vs_info->vsi_writefile_filp, INM_RDWR);
	if (vs_info->vsi_data_log_path)
		INM_FREE(&vs_info->vsi_data_log_path,
			 STRING_MEM(vs_info->vsi_data_log_path));
	if (vs_info->vsi_mnt_path)
		INM_FREE(&vs_info->vsi_mnt_path,
			 STRING_MEM(vs_info->vsi_mnt_path));
	vs_info_dtr(vs_info);
	goto out;
}

/*
 * inm_vs_uninit_vsnap_info
 *
 * DESC
 *       called to cleanup vsnap data structure associated with the vsnap
 */
void
inm_vs_uninit_vsnap_info(void *vol_context)
{
	inm_vs_info_t *vs_info = (inm_vs_info_t *)vol_context;
	inm_32_t	i = 0;

	INM_WRITE_LOCK(parent_vollist_lock);
	inm_kill_thread(vs_update_pool_info);
	INM_CLOSE_FILE(vs_info->vsi_target_filp, INM_RDONLY | INM_LYR);
	bt_uninit(&vs_info->vsi_btree);
	INM_CLOSE_FILE(vs_info->vsi_btree_filp, INM_RDWR);
	INM_CLOSE_FILE(vs_info->vsi_fid_table_filp, INM_RDWR);
	if (vs_info->vsi_writefile_filp)
		INM_CLOSE_FILE(vs_info->vsi_writefile_filp, INM_RDWR);
	inm_list_del(&vs_info->vsi_next);
	/* We assume deletes/creates cant go in parallel */
	vs_info->vsi_parent_list->pl_num_vsnaps--;
	if (inm_list_empty(&(vs_info->vsi_parent_list->pl_vsnap_list))) {
		inm_list_del(&(vs_info->vsi_parent_list->pl_next));
		if (vs_info->vsi_parent_list->pl_devpath) {
			INM_FREE(
			    &vs_info->vsi_parent_list->pl_devpath,
			    STRING_MEM(vs_info->vsi_parent_list->pl_devpath));
		}
		if (vs_info->vsi_parent_list->pl_retention_path) {
			INM_FREE(&vs_info->vsi_parent_list->pl_retention_path,
		       STRING_MEM(vs_info->vsi_parent_list->pl_retention_path));
		}
		INM_FREE(&vs_info->vsi_parent_list->pl_parent_change_lock,
			 sizeof(inm_rwlock_t));
		INM_FREE(&vs_info->vsi_parent_list,
			 sizeof(inm_vs_parent_list_t));
	}
	INM_WRITE_UNLOCK(parent_vollist_lock);
	if (vs_info->vsi_mnt_path)
		INM_FREE(&vs_info->vsi_mnt_path,
			 STRING_MEM(vs_info->vsi_mnt_path));
	if ( INM_TEST_BIT(VS_CDP, &vs_info->vsi_flags) ) {
		if (vs_info->vsi_last_datafile) {
			INM_FREE(&vs_info->vsi_last_datafile,
				 MAX_RETENTION_DATA_FILENAME_LENGTH);
		}
	} else {
		for( i = 0; i < NUM_KEY_PAGES; i++ ) {
			if( vs_info->vsi_key_pages[i] )
				INM_FREE(&vs_info->vsi_key_pages[i],
					 INM_PAGE_SIZE);
		}

		for( i = 0; i < NUM_FID_PAGES; i++ ) {
			if( vs_info->vsi_fid_pages[i] )
				INM_FREE(&vs_info->vsi_fid_pages[i],
					 INM_PAGE_SIZE);
		}
	}

	if (vs_info->vsi_data_log_path)
		INM_FREE(&vs_info->vsi_data_log_path,
			 STRING_MEM(vs_info->vsi_data_log_path));
	INM_FREE(&vs_info->vsi_btree_lock, sizeof(inm_rwlock_t));
	INM_FREE(&vs_info, sizeof(inm_vs_info_t));
}

/*
 * inm_vs_init
 *
 * DESC
 *      initialize the global structures
 */
inm_32_t
inm_vs_init()
{
	inm_32_t retval = 0;

	DEV_DBG("allocate parent vollist head");
	parent_vollist_head = INM_ALLOC(sizeof(inm_list_head_t), GFP_KERNEL);
	if (!parent_vollist_head) {
		ERR("no mem");
		retval = -ENOMEM;
		goto out;
	}
	INM_INIT_LIST_HEAD(parent_vollist_head);

	DEV_DBG("allocate parent vollist lock");
	parent_vollist_lock = INM_ALLOC(sizeof(inm_rwlock_t), GFP_KERNEL);
	if (!parent_vollist_lock) {
		ERR("no mem");
		retval = -ENOMEM;
		goto fail;
	}
	INM_INIT_LOCK(parent_vollist_lock);

  out:
	return retval;

  fail:
	INM_FREE(&parent_vollist_head, sizeof(inm_list_head_t));
	goto out;
}

/*
 * inm_vs_exit
 *
 * DESC
 *      called during driver's exit.
 */
void
inm_vs_exit()
{
	INM_FREE(&parent_vollist_head, sizeof(inm_list_head_t));
	INM_FREE(&parent_vollist_lock, sizeof(inm_rwlock_t));
}

/*
 * inm_vs_parent_for_ret_path
 *
 * DESC
 *      retrieve the parent_vol_list(ie list of vsnap) for a given
 *      retention path
 */
inm_vs_parent_list_t *
inm_vs_parent_for_ret_path(char *ret_path, size_t ret_path_len)
{
	inm_list_head_t		*ptr = NULL;
	inm_list_head_t		*nextptr = NULL;
	inm_vs_parent_list_t	*parent_vollist = NULL;

	if (!parent_vollist_head) {
		ERR("parent_vollist_head is NULL.");
		return NULL;
	}
	inm_list_for_each_safe(ptr, nextptr, parent_vollist_head) {
		parent_vollist = inm_list_entry(ptr, inm_vs_parent_list_t,
					    pl_next);
		if (!strcmp(parent_vollist->pl_retention_path, ret_path)) {
			return parent_vollist;
		}
	}
	DBG("not found");
	return NULL;
}

/*
 * inm_vs_num_of_context_for_parent
 *
 * DESC
 *      return the size required to retrieve the context of all vsnap
 *      associated with a given retention path
 */
INLINE inm_u32_t
inm_vs_num_of_context_for_parent(inm_vs_parent_list_t *parent_entry)
{

/*	inm_list_for_each_safe(ptr, next_ptr, &parent_entry->pl_vsnap_list) {
		n_entries++;
	}*/

	return (parent_entry->pl_num_vsnaps * sizeof(inm_vs_context_t));
}

inm_32_t
inm_vdev_init()
{
	int error = 0;

#ifdef _VSNAP_DEBUG_
	inm_mem_init();
#endif

#ifdef _INM_LOGGER_
	error = inm_vdev_init_logger();
	if ( error ) {
		ERR("vdev logger init failed");
		goto vdev_logger_init_failed;
	}
#endif

#ifdef _VSNAP_DEBUG_
	bt_print_btree_info();
	io_print_info();
#endif

	DEV_DBG("initialize proc interface");
	error = inm_vdev_init_proc();
	if ( error ) {
		ERR("no proc");
		goto init_proc_failed;
	}

	error = inm_vs_init();
	if ( error ) {
		ERR("vsnap_init failed");
		goto vs_init_failed;
	}

	/*
	 * initialize vsnap list head & semaphore
	 */
	DEV_DBG("initialize vsnap list head");
	inm_vs_list_head = INM_ALLOC(sizeof(inm_list_head_t), GFP_KERNEL);
	if (!inm_vs_list_head) {
		ERR("no mem");
		error = -ENOMEM;
		goto vs_list_alloc_failed;
	}
	INM_INIT_LIST_HEAD(inm_vs_list_head);
	INM_INIT_LOCK(&inm_vs_list_rwsem);
	INM_INIT_SPINLOCK(&inm_vs_minor_list_lock);

	/*
	 * initialize virtual vol (volpack) list head & semaphore
	 */
	DEV_DBG("initialize volpack list head");
	inm_vv_list_head = INM_ALLOC(sizeof(inm_list_head_t), GFP_KERNEL);
	if (!inm_vv_list_head) {
		ERR("no mem");
		error= -ENOMEM;
		goto vv_list_alloc_failed;
	}

	INM_INIT_LIST_HEAD(inm_vv_list_head);
	INM_INIT_LOCK(&inm_vv_list_rwsem);
	INM_INIT_SPINLOCK(&inm_vv_minor_list_lock);

out:
	return error;

vv_list_alloc_failed:
	INM_FREE(&inm_vs_list_head, sizeof(inm_list_head_t));

vs_list_alloc_failed:
	inm_vs_exit();

vs_init_failed:
	inm_vdev_exit_proc();

init_proc_failed:
#ifdef _INM_LOGGER_
	inm_vdev_fini_logger();

vdev_logger_init_failed:
#endif
#ifdef _VSNAP_DEBUG_
	inm_mem_destroy();
#endif
	goto out;

}

void
inm_vdev_exit()
{
	INM_FREE(&inm_vs_list_head, sizeof(inm_list_head_t));
	INM_FREE(&inm_vv_list_head, sizeof(inm_list_head_t));
	inm_vs_exit();
	inm_vdev_exit_proc();
#ifdef _INM_LOGGER_
	inm_vdev_fini_logger();
#endif
#ifdef _VSNAP_DEBUG_
	inm_mem_destroy();
#endif
}
/*
 * read_from_volpack
 *
 * DESC
 *      handle when a read i/o is performed on any volpack dev
 */
inm_32_t
read_from_volpack(void *vol_context, inm_offset_t disk_offset, inm_u32_t size,
		  void *pd_ptr)
{
	inm_32_t	retval = 0;
	inm_vv_info_t	*vv_info = (inm_vv_info_t *)vol_context;
	inm_vdev_page_data_t *page_data = pd_ptr;

	page_data->pd_file_offset = (inm_u64_t)page_data->pd_disk_offset;
	retval = inm_vdev_read_page(vv_info->vvi_sparse_file_filp, page_data);
	if (retval < 0) {
		ERR("read failed.");
	}

	return retval;
}

/*
 * write_to_volpack
 *
 * DESC
 *      handle when a write i/o is performed on any volpack dev
 */
inm_32_t
write_to_volpack(inm_vdev_page_data_t page_data, void *context)
{
	inm_32_t	retval = 0;
	inm_vv_info_t	*vv_info = (inm_vv_info_t *)context;

	page_data.pd_file_offset = (inm_u64_t)page_data.pd_disk_offset;
	retval = inm_vdev_write_page(vv_info->vvi_sparse_file_filp,
				     &page_data);
	if (retval < 0) {
		ERR("written failed.");
	}

	return retval;
}
