
/*****************************************************************************
 * vsnap_kernel.h
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
 * Description:         Data Structures used in kernel mode by vsnap module.
 *
 * Revision:            $Revision: 1.10 $
 * Checked in by:       $Author: lamuppan $
 * Last Modified:       $Date: 2015/02/24 03:29:23 $
 *
 * $Log: vsnap_kernel.h,v $
 * Revision 1.10  2015/02/24 03:29:23  lamuppan
 * Bug #1619650, 1619648, 1619642, 1619640, 1619637, 1619636, 1619635: Fix for policheck
 * bugs for Filter and Vsnap drivers. Please refer to bugs in TFS for more details.
 *
 * Revision 1.9  2014/03/14 12:49:28  lamuppan
 * Changes for lists, kernel helpers.
 *
 * Revision 1.8  2010/10/19 04:52:29  chvirani
 * 14174: Raw device no longer required
 *
 * Revision 1.7  2010/10/01 09:03:22  chvirani
 * 13869: Full disk support
 *
 * Revision 1.6  2010/06/17 12:45:15  chvirani
 * 12543: Sparse - Do not insert pvt file into cache. Do not update header during initial population.
 *
 * Revision 1.5  2010/06/10 14:21:31  chvirani
 * vsinfo.vsi_flags is now unsigned long to work with test/set_bit
 *
 * Revision 1.4  2010/06/09 10:10:04  chvirani
 * Support for sparse retention
 *
 * Revision 1.3  2010/03/31 13:23:11  chvirani
 * o X: Convert disk offsets to inm_offset_t
 * o X: Increase/Decrease mem usage count inside lock
 *
 * Revision 1.2  2010/03/10 12:08:43  chvirani
 * Multithreaded Driver
 *
 * Revision 1.1  2009/04/28 12:34:38  cswarna
 * Adding these files as part of vsnap driver code re-org
 *
 * Revision 1.6  2008/08/12 22:27:23  praveenkr
 * fix for #5404: Decoupled the mount point away from creation of vsnap
 * Moved the decision on the device name away from driver into the CX
 * & 'cdpcli'
 *
 * Revision 1.5  2008/03/25 12:59:52  praveenkr
 * code cleanup for Solaris
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
 * Revision 1.1.2.13  2007/08/21 15:25:12  praveenkr
 * added more comments
 *
 * Revision 1.1.2.12  2007/08/02 07:40:55  praveenkr
 * fix for 3829 - volpack is also listed when listing vsnaps from cdpcli
 * maintaining a seperate list for volpacks
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
 * Revision 1.1.2.4  2007/04/24 12:49:47  praveenkr
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

#ifndef _VSNAP_KERNEL_H
#define _VSNAP_KERNEL_H

#include "common.h"
#include "vsnap_kernel_helpers.h"
#include "disk_btree_core.h"
#include "vsnap_common.h"
#include "vdev_thread_common.h"

/* Retention data filenames don't exceed more than 50 bytes. */
#define MAX_RETENTION_DATA_FILENAME_LENGTH	50
#define MAX_FILENAME_LENGTH			2048

/*
 * Subtree macros
 */
typedef struct inm_subtree {
	single_list_entry_t	st_next;
	void			*st_root;
	inm_u32_t		st_ref_count;
} inm_subtree_t;

#define ST_ROOT_ENTRY(HEAD)	((inm_subtree_t *)((HEAD)->sl_next))

#define ST_ROOT(LIST_HEAD)	ST_ROOT_ENTRY(LIST_HEAD)->st_root

#define ST_REF_COUNT(LIST_HEAD)	ST_ROOT_ENTRY(LIST_HEAD)->st_ref_count

#define ST_REF_INC(LIST_HEAD)	ST_ROOT_ENTRY(LIST_HEAD)->st_ref_count++

#define ST_REF_DEC(LIST_HEAD)	ST_ROOT_ENTRY(LIST_HEAD)->st_ref_count--

/*
 * This structure holds information about offset/length of the parent volume
 * (target) that is undergoing a change. From dataprotection, this info is
 * received by invirvol driver through update ioctl. Or if parent itself is a
 * virtual volume(nested vsnaps), then invirvol directly gets these changes
 * through Write API.
 *
 * Members:
 * 	pc_next			Head for the list of offsets that are
 *				undergoing a change on the parent volume.
 * 	pc_mutex		Mutex Lock for read threads to pend on.
 * 	pc_offset/pc_len	offset/len that needs to be protected.
 */
struct inm_vs_parent_change_tag {
	inm_list_head_t		pc_next;
	inm_mutex_t		pc_mutex;
	inm_u64_t		pc_offset;
	inm_u32_t		pc_len;
};
typedef struct inm_vs_parent_change_tag inm_vs_parent_change_t;

/*
 * This is a singly linked list of target volumes. target volume specific
 * information is stored here. Each structure in turn contains linked list of
 * inm_vs_info_t structure associated with the same target volume.
 *
 * Members:
 * 	pl_next			Pointer to next target volume in the list.
 *	pl_devpath		Pointer to Target Volume Name.
 * 	pl_retention_path	Directory where retention files are stored.
 *  	pl_parent_change_lock	Read-Write Lock to protect the offset List
 * 	pl_parent_change_list	Head for the offset list.
 *  	pl_vsnap_list		Head pointer to the first vsnap mounted for
 * 				this target volume.
 */
struct inm_vs_parent_list_tag {
	inm_list_head_t			pl_next;
	char				*pl_devpath;
	inm_32_t			pl_sect_size;
	char				*pl_retention_path;
	inm_rwlock_t			*pl_parent_change_lock;
	inm_list_head_t			pl_parent_change_list;
	inm_list_head_t			pl_vsnap_list;
	inm_rwlock_t			pl_target_io_done;
	inm_u32_t			pl_num_vsnaps;
};
typedef struct inm_vs_parent_list_tag inm_vs_parent_list_t;

#define VS_CDP		1
#define VS_SPARSE	2

/* Sparse time stamp */
#define IntervalsPerSecond 10000000
#define SecondsPerDay 86400
#define SecondsPerHour	3600
#define SecondsPerMinute 60
#define EpochDayOfWeek 1
#define DaysPerWeek 7
#define DaysPerFourHundredYears (365 * 400 + 97)
#define DaysPerNormalCentury (365 * 100 + 24)
#define DaysPerNormalFourYears (365 * 4 + 1)
#define SPARSE_BASE_YEAR	2009

/* For sparse retention */
#define SPARSE_END_TIME_OFFSET		7
#define SPARSE_END_TIME_LEN		12
#define SPARSE_FILE_TYPE_OFFSET		20
#define SPARSE_FILE_TYPE_LEN		1
#define SPARSE_START_TIME_OFFSET	22
#define SPARSE_START_TIME_LEN		12
#define SPARSE_START_TIME_LEN_NOBC	8 /*start time without bookmark */
#define SPARSE_MAX_SEQ_NUM_OFFSET	35
#define SPARSE_MAX_SEQ_NUM_LEN		7

#define SPARSE_FID_SPECIAL		0xDEADBEEF
#define SPARSE_ENDTIME_SPECIAL		999999999999ULL

typedef inm_u64_t fid_key_t;

#define MAX_FID_KEYS	1024	/* Should be multiple of 1024 */

#define MAX_KEYS_PER_PAGE 	(inm_u32_t)(INM_PAGE_SIZE / sizeof(fid_key_t))
#define MAX_FID_PER_PAGE	(inm_u32_t)(INM_PAGE_SIZE / sizeof(inm_u32_t))
#define INM_LAST_KEY(array)	array[MAX_KEYS_PER_PAGE - 1]

#define NUM_KEY_PAGES	MAX_FID_KEYS/MAX_KEYS_PER_PAGE
#define NUM_FID_PAGES	MAX_FID_KEYS/MAX_FID_PER_PAGE

#define ARRAY_1D_TO_2D(pa, idx, MAX_PER_PAGE)	\
	(pa)[(idx) / MAX_PER_PAGE][(idx) % MAX_PER_PAGE]


enum FID_STATUS {
	FID_FAIL,              	/* Lookup and insert failed */
	FID_SUCCESS,		/* Lookup succeeded, file present in fidfile */
	FID_SUCCESS_MODIFIED,	/* Lookup failed, file successfully inserted */
	FID_NOT_REQD		/* Indicates update is not required, only sparse */
};

/*
 * This structure holds vsnap specific information, some are passed from user
 * program and some others are used for internal purpose. One vsnap structure
 * per virtual volume is created.
 *
 * Members:
 *	vsi_next		pointer to next vsnap info structure for the
 *				same target volume.
 *	vsi_data_log_path	valid for writeable vsnaps, where private data
 *				files are stored.
 *	vsi_btree_filp		cached btree map file's handle.
 *	vsi_btree_root_offset	cached root node offset of the btree map.
 *	vsi_fid_table_filp	cached fid table's handle. This file contains
 *				the translation between file_id & filename.
 *	vsi_next_fid		next/current fid to be used for newer data
 *				files; updated in update ioctl path. this is
 *				used as both cache (i.e., translates to
 * 				'vsi_last_datafile') & 	also as the next
 * 				available free fid, based on the value of
 *				'vsi_next_fid_used'
 *	vsi_next_fid_used	tells whether the 'vsi_next_fid' entry refers
 *				to the next available fid or refers to the fid
 *				pointing to 'vsi_last_datafile.'
 *	vsi_snapshot_id		unique Id associated with this vsnap.
 *	vsi_parent_list		back ptr to parent volume structure.
 *	vsi_last_datafile	filename of the most recent data file in the
 *				retention log.
 *	vsi_recovery_time	recovery time associated with this vsnap.
 *				(if 0 in case of PIT)
 */
struct inm_vs_info_tag {
	inm_list_head_t			vsi_next;
	char				*vsi_mnt_path;
	char				*vsi_data_log_path;
	inm_rwlock_t			*vsi_btree_lock;
	void				*vsi_btree_filp;
	inm_u64_t			vsi_btree_root_offset;
	void				*vsi_fid_table_filp;
	inm_u32_t			vsi_next_fid;
	inm_u32_t			vsi_next_fid_used;
	inm_u32_t			vsi_write_fid;
	inm_u32_t			vsi_writefile_suffix;
	inm_u64_t			vsi_snapshot_id;
	inm_vs_parent_list_t		*vsi_parent_list;
	char				*vsi_last_datafile;
	inm_u64_t			vsi_recovery_time;
	enum VSNAP_VOLUME_ACCESS_TYPE	vsi_access_type;
	inm_32_t			vsi_is_map_in_retention_path;
	inm_32_t			vsi_is_tracking_enabled;
	inm_u32_t			vsi_tracking_id;
	void				*vsi_writefile_filp;
	void				*vsi_target_filp;
	inm_bt_t			*vsi_btree;
	/* For Sparse retention */
	fid_key_t			*vsi_key_pages[NUM_KEY_PAGES];
	inm_u32_t			*vsi_fid_pages[NUM_FID_PAGES];
	inm_u64_t			vsi_rec_time_yymmddhh;
	inm_u64_t			vsi_sparse_end_time;
	unsigned long			vsi_flags;
	inm_u32_t			vsi_sparse_num_keys;
	inm_u32_t			vsi_forward_fid;
	inm_u32_t			vsi_backward_fid;
	inm_u32_t			vsi_last_datafid;

	enum FID_STATUS (*vsi_get_file_fid)(struct inm_vs_info_tag *,
					    char *,inm_u32_t *,inm_32_t *);
	void (*vsi_set_file_fid)(struct inm_vs_info_tag *,
				 inm_u32_t, inm_32_t);
	inm_32_t (*vsi_populate_fid)(struct inm_vs_info_tag *,
				     inm_vs_hdr_info_t *);
};
typedef struct inm_vs_info_tag inm_vs_info_t;

/*
 * represents a single volpack/virtual-volume device.
 */
typedef struct inm_vv_info_tag {
	void	*vvi_sparse_file_filp;	/* cached sparse file ptr */
	char	*vvi_sparse_file_path;	/* cached sparse file path */
} inm_vv_info_t;

/*
 * this is the structure used to store the data pages related to update ioctl
 */
typedef struct inm_vs_update_page_list {
	inm_list_head_t		up_next;
	inm_page_t		*up_data_page;
	inm_u32_t		up_len;
} inm_vs_update_page_list_t;

/* Used by threads for updates */
/*
 * This structure is shared by multiple threads and contains common data
 * used to update multiple vsnaps concurrently
 */
typedef struct update_data_common {
	inm_list_head_t		*udc_page_list_head;
	inm_ipc_t		udc_update_ipc;
	inm_atomic_t		udc_updates_inflight;
	inm_u64_t		udc_vol_offset;
	inm_u64_t		udc_file_offset;
	char			*udc_data_filename;
	inm_u32_t		udc_len;
	inm_u32_t 		udc_cow;
	inm_32_t		udc_error;
} update_data_common_t;

/* Per vsnap update data, basically points to vsnap and common data */
typedef struct update_data {
	inm_vs_info_t 		*ud_vs_info;
	update_data_common_t 	*ud_common;
} update_data_t;

/* Extern declarations */
extern inm_list_head_t 		*parent_vollist_head;
extern inm_rwlock_t 		*parent_vollist_lock;
extern inm_list_head_t        	*inm_vs_list_head;      /* list of vsnaps created */
extern inm_rwlock_t     	inm_vs_list_rwsem;      /* held while processing vsnap
                                                   related ioctls */
extern inm_list_head_t        	*inm_vv_list_head;      /* list of vvols created */
extern inm_rwlock_t     	inm_vv_list_rwsem;      /* held while processing
                                                   volpack related ioctls */
extern inm_spinlock_t		inm_vs_minor_list_lock;
extern inm_spinlock_t		inm_vv_minor_list_lock;

extern inm_major_t		inm_vs_major;
extern inm_major_t		inm_vv_major;


inm_32_t inm_vs_update_for_parent(inm_u64_t, inm_u32_t, inm_u64_t, char *,
				  char *, inm_u32_t);
inm_32_t inm_vs_insert_offset_split(single_list_entry_t*, inm_u64_t, inm_u32_t,
				    inm_u32_t, inm_u64_t, inm_u32_t);

inm_32_t inm_vs_read_from_vsnap(void *, inm_offset_t, inm_u32_t, void *);
inm_32_t inm_vs_write_to_vsnap(inm_vdev_page_data_t, void *);
inm_32_t inm_vs_init_vsnap_info(inm_vs_mdata_t *, inm_u32_t, inm_64_t *,
				void **);
void inm_vs_uninit_vsnap_info(void *);
inm_32_t inm_vs_init(void);
void inm_vs_exit(void);
inm_32_t inm_vdev_init(void);
void inm_vdev_exit(void);


inm_u32_t inm_vs_num_of_context_for_parent(inm_vs_parent_list_t *);
inm_vs_parent_list_t * inm_vs_parent_for_ret_path(char *, size_t);

inm_32_t read_from_volpack(void *, inm_offset_t, inm_u32_t, void *);
inm_32_t write_to_volpack(inm_vdev_page_data_t , void *);

inm_32_t read_from_this_fid(inm_vdev_page_data_t *, inm_u32_t, inm_vs_info_t *);

void inm_vs_update_vsnap(vsnap_task_t *);
inm_32_t read_from_parent_volume(void *, inm_vdev_page_data_t *);

#ifdef _VSNAP_DEBUG_
void inm_vs_lookup_file_fid_sparse(inm_vs_info_t *, char *);
#endif

#endif
