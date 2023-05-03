#ifndef _LCW_H
#define _LCW_H

void lcw_move_bitmap_to_raw_mode(target_context_t *tgt_ctxt);
void lcw_flush_changes(void);
inm_s32_t lcw_perform_bitmap_op(char *guid, enum LCW_OP op);
inm_s32_t lcw_map_file_blocks(char *name);

#endif

