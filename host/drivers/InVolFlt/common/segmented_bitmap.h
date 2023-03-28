/*
 * Copyright (C) 2005 InMage
 * This file contains proprietary and confidential information and
 * remains the unpublished property of InMage. Use, disclosure,
 * or reproduction is prohibited except as permitted by express
 * written license aggreement with InMage.
 *
 * File       : segmented_bitmap.h
 *
 * Description: This file contains data mode implementation of the
 *              filter driver.
 *
 * Functions defined are
 *
 *
 */

#ifndef _INMAGE_SEGMENTED_BITMAP_H
#define _INMAGE_SEGMENTED_BITMAP_H

#include "involflt-common.h"

typedef struct _segmented_bitmap_tag
{
    fstream_segment_mapper_t *fssm;
    inm_u64_t next_search_offset;
    inm_u64_t bits_in_bitmap;
    inm_atomic_t refcnt;
}segmented_bitmap_t;

struct bmap_bit_stats;
segmented_bitmap_t *segmented_bitmap_ctr(fstream_segment_mapper_t *fssm, inm_u64_t bits_in_bitmap);
void segmented_bitmap_dtr(segmented_bitmap_t *sb);
segmented_bitmap_t *segmented_bitmap_get(segmented_bitmap_t *sb);
void segmented_bitmap_put(segmented_bitmap_t *sb);
inm_s32_t segmented_bitmap_process_bitrun(segmented_bitmap_t *sb, inm_u32_t bitsinrun, inm_u64_t bitoffset, inm_s32_t bitmap_operation);
inm_s32_t segmented_bitmap_set_bitrun(segmented_bitmap_t *sb, inm_u32_t bitsinrun, inm_u64_t bitoffset);
inm_s32_t segmented_bitmap_clear_bitrun(segmented_bitmap_t *sb, inm_u32_t bitsinrun, inm_u64_t bitoffset);
inm_s32_t segmented_bitmap_invert_bitrun(segmented_bitmap_t *sb, inm_u32_t bitsinrun, inm_u64_t bitoffset);
inm_s32_t segmented_bitmap_clear_all_bits(segmented_bitmap_t *sb);
inm_s32_t segmented_bitmap_get_first_bitrun(segmented_bitmap_t *sb, inm_u32_t *bitsinrun, inm_u64_t *bitoffset);
inm_s32_t segmented_bitmap_get_next_bitrun(segmented_bitmap_t *sb, inm_u32_t *bitsinrun, inm_u64_t *bitoffset);
inm_u64_t segmented_bitmap_get_number_of_bits_set(segmented_bitmap_t *sb, struct bmap_bit_stats *);
inm_s32_t segmented_bitmap_sync_flush_all(segmented_bitmap_t *sb);
inm_s32_t get_next_bitmap_bitrun(char *bit_buffer, inm_u64_t adjusted_buffer_size, inm_u32_t *search_bit_offset, inm_u32_t *run_length, inm_u32_t *run_offset);


#endif /* _INMAGE_SEGMENTED_BITMAP_H */
