/*
 * Copyright (C) 2005 InMage
 * This file contains proprietary and confidential information and
 * remains the unpublished property of InMage. Use, disclosure,
 * or reproduction is prohibited except as permitted by express
 * written license aggreement with InMage.
 *
 * File       : filestream_segment_mapper.h
 *
 * Description: This file contains data mode implementation of the
 *              filter driver.
 *
 * Functions defined are
 *
 *
 */

#ifndef _INMAGE_FILESTREAM_SEGMENT_MAPPER_H
#define _INMAGE_FILESTREAM_SEGMENT_MAPPER_H

#include "involflt-common.h"

#define BITMAP_FILE_SEGMENT_SIZE (0x1000)

#define MAX_BITMAP_SEGMENT_BUFFERS 0x41 //65 segments

//bitmap operation
#define BITMAP_OP_SETBITS 0
#define BITMAP_OP_CLEARBITS 1
#define BITMAP_OP_INVERTBITS 2

struct _bitmap_api_tag; /* typedef'ed to bitmap_api_t */
struct _volume_bitmap;

typedef struct _fstream_segment_mapper_tag
{
    struct _bitmap_api_tag *bapi;
    inm_u32_t cache_size;
    inm_atomic_t refcnt;
    /* index for buffer cache pages */
    unsigned char **buffer_cache_index;
    struct inm_list_head segment_list;
    inm_u32_t nr_free_buffers;
    inm_u32_t nr_cache_hits;
    inm_u32_t nr_cache_miss;
    inm_u32_t segment_size;
    inm_u64_t starting_offset;

}fstream_segment_mapper_t;

fstream_segment_mapper_t *fstream_segment_mapper_ctr(void);
void fstream_segment_mapper_dtr(fstream_segment_mapper_t *fssm);
fstream_segment_mapper_t *fstream_segment_mapper_get(fstream_segment_mapper_t *fssm);
void fstream_segment_mapper_put(fstream_segment_mapper_t *fssm);
inm_s32_t fstream_segment_mapper_attach(fstream_segment_mapper_t *fssm, struct _bitmap_api_tag *bapi, inm_u64_t offset, inm_u64_t min_file_size, inm_u32_t segment_cache_limit);
inm_s32_t fstream_segment_mapper_detach(fstream_segment_mapper_t *fssm);
inm_s32_t fstream_segment_mapper_read_and_lock(fstream_segment_mapper_t *fssm,
          inm_u64_t offset, unsigned char **return_iobuf_ptr, inm_u32_t *return_seg_size);
inm_s32_t fstream_segment_mapper_unlock_and_mark_dirty(fstream_segment_mapper_t * fssm, inm_u64_t offset);
inm_s32_t fstream_segment_mapper_unlock(fstream_segment_mapper_t * fssm, inm_u64_t offset);
inm_s32_t fstream_segment_mapper_flush(fstream_segment_mapper_t * fssm, inm_u64_t offset);
inm_s32_t fstream_segment_mapper_sync_flush_all(fstream_segment_mapper_t *fssm);
#endif /* _INMAGE_FILESTREAM_SEGMENT_MAPPER_H */

