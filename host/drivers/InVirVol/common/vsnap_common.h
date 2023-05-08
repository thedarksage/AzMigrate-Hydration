
/*****************************************************************************
 * vsnap_common.h
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
 * Description:         Common vsnap declarations
 *
 * Revision:            $Revision: 1.6 $
 * Checked in by:       $Author: chvirani $
 * Last Modified:       $Date: 2011/01/13 06:34:52 $
 *
 * $Log: vsnap_common.h,v $
 * Revision 1.6  2011/01/13 06:34:52  chvirani
 * 14878: Changes to support AIX.
 *
 * Revision 1.5  2010/10/13 06:40:09  chvirani
 * remove memory unaligned access in btree code.
 *
 * Revision 1.4  2010/10/01 09:03:22  chvirani
 * 13869: Full disk support
 *
 * Revision 1.3  2010/06/09 10:10:04  chvirani
 * Support for sparse retention
 *
 * Revision 1.2  2010/03/10 12:08:43  chvirani
 * Multithreaded Driver
 *
 * Revision 1.1  2009/04/28 12:34:38  cswarna
 * Adding these files as part of vsnap driver code re-org
 *
 * Revision 1.8  2008/08/12 22:27:23  praveenkr
 * fix for #5404: Decoupled the mount point away from creation of vsnap
 * Moved the decision on the device name away from driver into the CX
 * & 'cdpcli'
 *
 * Revision 1.7  2008/05/07 17:50:49  praveenkr
 * fix for 5247 - applying tracklogs are failing
 *
 * Revision 1.6  2008/05/07 17:05:51  praveenkr
 * fix for the following -
 * 	5247 - apply tracklogs are failing
 * 	5287 - rollback is failing from cdpcli
 * 	5335 - read-only & read-write unhide options are not working
 *
 * Revision 1.5  2008/04/17 10:38:41  praveenkr
 * fix for itanium crash -
 * 8 byte realignment of structures (common to userspace & kernel)
 *
 * Revision 1.4  2008/03/25 12:59:52  praveenkr
 * code cleanup for Solaris
 *
 * Revision 1.3  2007/09/14 13:11:16  sudhakarp
 * Merge from DR_SCOUT_4-0_DEV/DR_SCOUT_4-1_BETA2 to HEAD ( Onbehalf of RP).
 *
 * Revision 1.1.2.7  2007/08/21 15:25:12  praveenkr
 * added more comments
 *
 * Revision 1.1.2.6  2007/07/24 12:58:30  praveenkr
 * virtual volume (volpack) implementation
 *
 * Revision 1.1.2.5  2007/07/17 13:53:25  praveenkr
 * clean-up - unix-style naming, tab-spacing = 8, 78 col
 *
 * Revision 1.1.2.4  2007/05/25 09:45:44  praveenkr
 * added comments & partial clean-up of code
 *
 * Revision 1.1.2.3  2007/04/24 12:49:46  praveenkr
 * page cache implementation(write)
 *
 * Revision 1.1.2.2  2007/04/17 15:36:30  praveenkr
 * #2944: 4.0_rhl4target-target is crashed when writting the data to R/W vsnap.
 * Sync-ed most of the data types keeping Trunk Windows code as bench-mark.
 * Did minimal cleaning of the code too.
 *
 * Revision 1.1.2.1  2007/04/11 19:49:36  gregzav
 *   initial release of the new make system
 *   note that some files have been moved from Linux to linux (linux os is
 *   case sensitive and we had some using lower l and some using upper L for
 *   linux) this is to make things consitent also needed to move cdpdefs.cpp
 *   under config added the initial brocade work that had been done.
 *
 *****************************************************************************/

#ifndef _VSNAP_COMMON_H
#define _VSNAP_COMMON_H

#include "common.h"

#define VSNAP_RETENTION_FILEID_MASK	0x80000000
#define DEFAULT_VOLUME_SECTOR_SIZE	512

#define SET_WRITE_BIT_FOR_FILEID(x)	(x | VSNAP_RETENTION_FILEID_MASK)
#define RESET_WRITE_BIT_FOR_FILEID(x)	(x &= ~VSNAP_RETENTION_FILEID_MASK)

enum VSNAP_VOLUME_ACCESS_TYPE {
	VSNAP_READ_ONLY,
	VSNAP_READ_WRITE,
	VSNAP_READ_WRITE_TRACKING,
	VSNAP_UNKNOWN_ACCESS_TYPE
};
enum VSNAP_CREATION_TYPE {
	VSNAP_POINT_IN_TIME,
	VSNAP_RECOVERY,
	VSNAP_CREATION_UNKNOWN
};

/*
 * structure used to pass vsnap mount information from user program to the
 * invirvol driver.
 *
 * Members:
 *	md_retention_path	path to retention log data files
 *	md_parent_dev_path	replicated Target Vol Name or a virutal vol
 *				this vsnap is pointing to.
 *	md_snapshot_id		unique Id associated with each Vsnap.
 *	md_bt_root_offset	root node offset within the btree map file
 *	md_parent_vol_size	target volume size in bytes.
 *	md_parent_sect_size	sectore size
 *	md_next_fid		the next fid for subsequent data files.
 *	md_recovery_time	recovery time associated with this vsnap.
 */
typedef struct inm_vs_mdata_tag {
	char				md_retention_path[2048];
	char				md_data_log_path[2048];
	char				md_mnt_path[2048];
	char				md_parent_dev_path[2048];
	inm_u64_t			md_parent_vol_size;
	inm_u32_t			md_parent_sect_size;
	inm_u32_t			md_next_fid;
	inm_u64_t			md_snapshot_id;
	inm_u64_t			md_bt_root_offset;
	inm_u64_t			md_recovery_time;
	enum VSNAP_VOLUME_ACCESS_TYPE	md_access_type;
	char				md_is_map_in_retention_path;
	char				md_is_tracking_enabled;
	char				md_sparse;
	char				md_flag; /* alignment */
} inm_vs_mdata_t;

#define MDATA_FLAG_FULL_DISK 	1

/*
 * structure used to retain the context of the vsnap device when queried by
 * the user space.
 */
typedef struct inm_vs_context_tag {
	inm_u64_t			vc_snapshot_id;
	char				vc_mnt_path[2048];
	inm_u64_t			vc_recovery_time;
	char				vc_retention_path[2048];
	char				vc_data_log_path[2048];
	char				vc_parent_dev_path[2048];
	enum VSNAP_VOLUME_ACCESS_TYPE	vc_access_type;
	char				vc_is_tracking_enabled;
	char				vc_reserved[3];	/* alignment */
} inm_vs_context_t;

typedef struct single_list_entry_tag {
	struct single_list_entry_tag	*sl_next;
} single_list_entry_t; //, *single_list_entry_t_p;

/*
 * inline functions & MACROs work on single_list_entry_t
 */
single_list_entry_t* pop_entry_from_list(single_list_entry_t*);
#define INM_LIST_INIT(LH)		(LH)->sl_next = NULL
#define INM_LIST_IS_EMPTY(LH)		((LH)->sl_next == NULL)
#define INM_LIST_POP_ENTRY(LH)		pop_entry_from_list(LH)
#define INM_LIST_PUSH_ENTRY(LH, ENTRY)					\
	do {								\
		(ENTRY)->sl_next = (LH)->sl_next;			\
		(LH)->sl_next = (ENTRY);				\
	} while(0)
#define INM_LIST_NUM_NODES(LH, PTR, COUNT)      \
for( (PTR) = (LH)->sl_next; (PTR); COUNT++, (PTR) = (PTR)->sl_next );

/*
 * structure is used internally while inserting entries in the btree map
 * and also while searching the map.
 */
typedef struct inm_vs_offset_split_tag {
	single_list_entry_t	os_next;
	inm_u64_t		os_offset;
	inm_u64_t		os_file_offset;
	inm_u32_t		os_fid;
	inm_u32_t		os_len;
	inm_u32_t		os_tracking_id;
} inm_vs_offset_split_t;

#ifdef _SOLARIS_
#pragma pack(1)
#else
#ifdef _AIX_
#pragma pack(1)
#else
#pragma pack(push, 1)
#endif
#endif
/*
 * fid table file contains array of file names. for a particular id, the
 * corresponding file name is found using the formula:
 *     (file_id-1)*sizeof(FileIdTable)
 * This avoids searching in this table.
 */
typedef struct inm_vs_fid_table_tag {
	char		ft_filename[50];
} inm_vs_fid_table_t;

/* structure to represent each entry in the node of a Btree map. */
typedef struct inm_bt_key_tag {
	inm_u64_t	key_offset;
	inm_u64_t	key_file_offset;
	inm_u32_t	key_fid;
	inm_u32_t	key_len;
	inm_u32_t	key_tracking_id;
} inm_bt_key_t;

#ifdef _SOLARIS_
#pragma pack ()
#else
#ifdef _AIX_
#pragma pack()
#else
#pragma pack (pop)
#endif
#endif

typedef struct  inm_vs_hdr_info_tag {
	inm_u64_t	hinfo_not_used_now;	/* ToDo - new field included
						   during back-porting from
						   3.5.1 to 4.0 but not
						   ported into linux driver */
	inm_u32_t	hinfo_data_fid;
	inm_u32_t	hinfo_write_fid;
	inm_u32_t	hinfo_writefile_suffix;
	inm_u32_t	hinfo_sparse_backward_fid;
	inm_u32_t	hinfo_sparse_forward_fid;
} inm_vs_hdr_info_t;

/* C hacks for Overloading operators */
#ifdef _MEM_ALIGN_
inm_32_t compare_less(inm_bt_key_t *, inm_bt_key_t *);
inm_32_t compare_greater(inm_bt_key_t *, inm_bt_key_t *);
#endif

inm_32_t compare_equal(inm_bt_key_t *, inm_bt_key_t *);

inm_32_t inm_vs_callback(void *, void *, void *, inm_u32_t);

inm_u64_t inm_strtoktoull(char *str, int vector_size, int vector[][2]);


#endif
