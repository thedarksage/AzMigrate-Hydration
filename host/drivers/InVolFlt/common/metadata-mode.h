/*
 * Copyright (c) 2005 InMage
 * This file contains proprietary and confidential information and
 * remains the unpublished property of InMage. Use, disclosure,
 * or reproduction is prohibited except as permitted by express
 * written license aggreement with InMage.
 *
 * File       : metadata-mode.h
 *
 * Description: Meta Data mode operations.
 */
#ifndef LINVOLFLT_METADATA_MODE_H
#define LINVOLFLT_METADATA_MODE_H
typedef struct _write_metadata_tag
{
    inm_u64_t offset;
    inm_u32_t length;

}write_metadata_t;

inm_u32_t split_change_into_chg_node(target_context_t *vcptr, write_metadata_t *wmd,
		inm_s32_t data_source, struct inm_list_head *split_chg_list_hd, inm_wdata_t *wdatap);
inm_s32_t add_metadata(target_context_t *vcptr, struct _change_node *chg_node, write_metadata_t *wmd,
				inm_s32_t data_source, inm_wdata_t *wdatap);
inm_s32_t save_data_in_metadata_mode(target_context_t *, write_metadata_t *, inm_wdata_t *);

inm_s32_t add_tag_in_non_stream_mode(tag_volinfo_t *, tag_info_t *,
                                     int, tag_guid_t *, inm_s32_t, 
                                     int commit_pending, tag_history_t *);
#endif
