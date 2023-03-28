#ifndef _INMAGE_FILTER_TARGET_H_
#define _INMAGE_FILTER_TARGET_H_

#include <linux/types.h>
#include "scst.h"
#include "scst_dev_handler.h"


typedef char                    inm_8_t;
typedef unsigned char           inm_u8_t;
typedef short                   inm_16_t;
typedef unsigned short          inm_u16_t;
typedef int                     inm_32_t;
typedef unsigned int            inm_u32_t;
typedef long long               inm_64_t;
typedef unsigned long long      inm_u64_t;

#define INLINE  inline


#define VOLUME_FILTERING_DISABLED_ATTR "VolumeFilteringDisabled"
#define AT_LUN_TYPE "LunType"
#define AT_BLOCK_SIZE "BlockSize"
#define INM_SECTORS_PER         	63
#define INM_DEF_SECTORS             	56
#define INM_DEF_HEADS               	255
#define INM_MODES_ENSE_BUF_SZ       	256
#define INM_DBD                         0x08
#define INM_DPOFUA                      0x10
#define INM_BYTE                        8


#define INM_MAX_UUID_LEN          128

#define WRITE_CANCEL_CDB                0xC0
#define WRITE_CANCEL_CDB_LEN            16
#define VACP_CDB                        0xC2
#define VACP_CDB_LEN                    6
#define HEARTBEAT_CDB                   0xC5
#define HEARTBEAT_CDB_LEN               6


#define INM_MEM_ZERO(addr, size)   memset(addr, 0, size)
#define INM_MEM_CMP(addr_src, addr_tgt, size)  memcmp(addr_src, addr_tgt, size)


void emd_exec_inquiry(struct scst_cmd *cmd);
void emd_exec_mode_sense(struct scst_cmd *cmd);
void emd_exec_mode_select(struct scst_cmd *cmd);
void emd_exec_read_capacity(struct scst_cmd *cmd);
void emd_exec_read_capacity16(struct scst_cmd *cmd);
void emd_exec_read(struct scst_cmd *cmd, inm_u64_t lba_start);
void emd_exec_write(struct scst_cmd *cmd, inm_u64_t lba_start);
void emd_exec_pt_write_cancel(struct scst_cmd *cmd, inm_u64_t lba_start, inm_u64_t io_len);
inm_32_t emd_task_mgmt_fn(struct scst_mgmt_cmd *mcmd,struct scst_tgt_dev *tgt_dev);
inm_32_t emd_proc_help_read(char *buffer, char **start, off_t offset, inm_32_t length, inm_32_t *eof, void *data);
inm_32_t emd_proc_info(char *buffer, char **start, off_t offset, inm_32_t length, inm_32_t *eof, struct scst_dev_type *dev_type,inm_32_t inout);
inm_32_t emd_read_proc(struct seq_file *seq, struct scst_dev_type *dev_type);
inm_32_t emd_write_proc(char *buffer, char **start, off_t offset, inm_32_t length, inm_32_t *eof, struct scst_dev_type *dev_type);
inm_32_t emd_help_info_show(struct seq_file *seq, void *v);
void emd_exec_vacp_write(struct scst_cmd *cmd);
void emd_exec_heartbeat(struct scst_cmd *cmd);

#endif
