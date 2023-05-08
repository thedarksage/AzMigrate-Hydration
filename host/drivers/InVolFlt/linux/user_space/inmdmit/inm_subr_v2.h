#ifndef _INM_SUBR_V2_H
#define _INM_SUBR_V2_H


int validate_ts_v2(UDIRTY_BLOCK_V2 *udbp);
int replicate_v2(char * src_volp, char *tgt, char *lun_name);
int process_tags_v2(UDIRTY_BLOCK_V2 *udbp, char **tag, int *tag_len);
int apply_dblk_v2(int tgt_fd, SVD_DIRTY_BLOCK_V2 *dblkp, void *datap);
int process_dm_stream_v2(int tgt_fd, UDIRTY_BLOCK_V2 *udbp);
int process_dfm_v2(int tgt_fd, UDIRTY_BLOCK_V2 *udbp);
int process_md_v2(int src_fd, int tgt_fd, UDIRTY_BLOCK_V2 *udbp);
int process_stream_tag_v2(inm_addr_t *addr);
int get_db_trans_v2(char *src_volp, char *tgt_volp, int commit_required);

#endif
