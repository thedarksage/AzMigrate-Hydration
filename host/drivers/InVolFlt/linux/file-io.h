/************************************************************************
 * Copyright 2005 InMage Systems.                                       *
 * This file contains proprietary and confidential information and      *
 * remains the unpublished property of InMage. Use, disclosure,         *
 * or reproduction is prohibited except as permitted by express         *
 * written license aggreement with InMage.                              *
 *                                                                      *
 * File : 
 * Desc :                                                               *
 ************************************************************************/

#ifndef _INM_FILE_IO_H
#define _INM_FILE_IO_H

#include "involflt-common.h"
#include "involflt.h"

/* code to handle recursive writes */
enum {
	INM_ORG_ADDR_SPACE_OPS = 0,
	INM_DUP_ADDR_SPACE_OPS = 1,
	INM_MAX_ADDR_OPS = 2, /* array size */
};
inma_ops_t *inm_alloc_inma_ops(void);
void inm_free_inma_ops(inma_ops_t *inma_opsp);
inma_ops_t *inm_get_inmaops_from_aops(const inm_address_space_operations_t *a_opsp,
				      inm_u32_t lookup_flag);
inm_s32_t inm_prepare_tohandle_recursive_writes(struct inode *inodep);
void inm_restore_org_addr_space_ops(struct inode *inodep);

inm_s32_t flt_open_file(const char *, inm_u32_t, void **);
inm_s32_t flt_read_file (void *, void *, inm_u64_t , inm_u32_t ,inm_u32_t *);
inm_s32_t flt_write_file(void *, void *, inm_u64_t, inm_u32_t,
		   inm_u32_t *);
int32_t flt_seek_file(void *, long long, inm_s64_t *, inm_u32_t);
void flt_close_file (void *);
long flt_mkdir(const char *, int);
long inm_unlink(const char *, char *);
inm_s32_t inm_unlink_symlink(const char *, char *);
long flt_rmdir(const char *);
inm_s32_t flt_get_file_size(void *, loff_t*);
inm_s32_t read_full_file(char *, char *, inm_u32_t , inm_u32_t *);
inm_s32_t write_full_file(char *, void *, inm_s32_t , inm_u32_t *);
inm_s32_t write_to_file(char *, void *, inm_s32_t , inm_u32_t *);
inm_s32_t flt_open_data_file(const char *, inm_u32_t, void **);
int file_exists(char *filename);
struct dentry *inm_lookup_create(struct nameidata *nd, inm_s32_t is_dir);

#endif /* _INM_FILE_IO_H */
