#ifndef _INM_SUBR_V1_H
#define _INM_SUBR_V1_H


int validate_ts_v1(UDIRTY_BLOCK *udbp);
int replicate_v1(char * src_volp, char *tgt_volp, char *lun_name);
int process_tags_v1(UDIRTY_BLOCK *udbp);
int apply_dblk_v1(int tgt_fd, SVD_DIRTY_BLOCK *dblkp, void *datap);
int process_dm_stream_v1(int tgt_fd, UDIRTY_BLOCK *udbp);
int process_dfm_v1(int tgt_fd, UDIRTY_BLOCK *udbp);
int process_md_v1(int src_fd, int tgt_fd, UDIRTY_BLOCK *udbp);
int process_stream_tag_v1(inm_addr_t *addr);
int get_db_trans_v1(char *src_volp, char *tgt_volp, int commit_required);

#endif
