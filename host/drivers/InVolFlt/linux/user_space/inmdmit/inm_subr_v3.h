#ifndef _INM_SUBR_V3_H
#define _INM_SUBR_V3_H


int validate_ts_v3(UDIRTY_BLOCK_V3 *udbp);
int replicate_v3(char * src_volp, char *tgt_volp, char *lun_name);
int process_tags_v3(UDIRTY_BLOCK_V3 *udbp);
int apply_dblk_v3(int tgt_fd, SVD_DIRTY_BLOCK_V2 *dblkp, void *datap);
int process_dm_stream_v3(int tgt_fd, UDIRTY_BLOCK_V3 *udbp);
int process_dfm_v3(int tgt_fd, UDIRTY_BLOCK_V3 *udbp);
int process_md_v3(int src_fd, int tgt_fd, UDIRTY_BLOCK_V3 *udbp);
int process_stream_tag_v3(inm_addr_t *addr);
int get_db_trans_v3(char *src_volp, char *tgt_volp, int commit_required);

#endif
