#include "scst_utap.h"
#include <linux/uio.h>
#include <linux/ctype.h>
#include <linux/seq_file.h>
#include <asm/kmap_types.h>
#include <linux/list.h>

/* Future roadmap :
 * o   Add support for user space tap to achieve following goals 
 *     1. Provide data tap into user space
 *     2. User space component may write data to generic block device
 *     3. User space component may write data to Cloud storage with
 *        REST APIs. This makes it tap compatabile with any cloud storage
 *     4. SCST IO receiver threading model requires use of non blocking context
 *     5. Various libraries could be leveraged for dedupe,compression etc.
 *        prior to sending out the data
 *     6. Modify vendor CDBs changes to use single CDB either using
 *        (i) payload information (ii) Mode select pages (intrusive, avoid it)
 */

#define CMDDT 0x02
#define EVPD    0x01


#define EMD_NAME "InMageEMD"
#define EMD_PROC_HELP "help"
#define INMAGE_EMD_VENDOR "InMage  "
#define INMAGE_EMD_REV "0002"

#define READ_CAP_LEN 8
#define READ_CAP16_LEN 12

#define INQUIRY_RSP_SIZE 128

#define DEF_TST			SCST_CONTR_MODE_SEP_TASK_SETS
#define DEF_QUEUE_ALG		SCST_CONTR_MODE_QUEUE_ALG_UNRESTRICTED_REORDER
#define DEF_SWP			0
#define DEF_TAS			0

extern struct proc_dir_entry *scst_create_proc_entry(struct proc_dir_entry * root, const char *name, struct scst_proc_data *pdata);

static LIST_HEAD(emd_dev_list);
typedef inm_u64_t emd_handle_t;

typedef struct emd_dev {
        struct list_head	dev_list;
        inm_32_t             	dev_id;
        emd_handle_t    	dev_handle;
        inm_u8_t   	dev_name[256];
}emd_dev_t;

typedef struct emd_io {
        unsigned long   eio_rw;
        void            *eio_iovp;
        inm_u32_t    eio_iovcnt;
        inm_u32_t    eio_len;
        emd_handle_t    eio_dev_handle;
	inm_u64_t eio_start;
	const char *	eio_iname;
}emd_io_t;

typedef struct emd_dev_cap {
        inm_u32_t       bsize;
        inm_u64_t       nblocks;
        inm_u64_t       startoff;
}emd_dev_cap_t;

typedef struct emd_dev_type {
        inm_32_t (*exec)(inm_u8_t *, inm_u8_t *);

        emd_handle_t (*attach)(const char *);

        void (*detach)(emd_handle_t);
        inm_32_t (*get_capacity)(emd_handle_t, emd_dev_cap_t *);
	inm_32_t (*prepare_write)(emd_handle_t, inm_u64_t, inm_u64_t);
        inm_32_t (*exec_write)(emd_io_t *io);
        inm_32_t (*exec_read)(emd_io_t *io);
        inm_32_t (*exec_io_cancel)(emd_handle_t, inm_u64_t,
                                inm_u64_t);
        inm_32_t (*exec_vacp_write)(emd_handle_t, char *, loff_t);
	const char* (*get_path)(emd_handle_t, inm_32_t *);

}emd_dev_type_t;


emd_dev_type_t *handler_dev_type;


char *emd_proc_help_string = "echo \"open|close NAME NBLOCKS\" >/proc/scsi_tgt/InMageEMD/InMageEMD \n";

#ifdef CONFIG_SCST_PROC
static struct scst_proc_data emd_help_proc_data = {
	SCST_DEF_RW_SEQ_OP(NULL)
	.show = emd_help_info_show,
	.data = "InVolFlt EMD Driver"
};
#endif

static struct file *emd_open(char *f_name)
{
	struct file *filp;

	if ((f_name == NULL) || (strlen(f_name) == 0)) {
		printk(KERN_ALERT"File name not given\n");
		return NULL;
	}

	filp = filp_open(f_name, O_LARGEFILE | O_RDONLY, 0600);
	if (IS_ERR(filp)) {
		printk(KERN_ALERT"Not able to open %s\n", f_name);
		filp = NULL;
	}

	return filp;
}

inm_32_t register_filter_target()
{
    return 0;
    
}

inm_32_t 
unregister_filter_target()
{
    return 0;
}

void
emd_detach(struct scst_device *dev)
{
	emd_dev_t		*emd_dev = (emd_dev_t *)dev->dh_priv;

	printk(KERN_ALERT "emd_detach(): name = %s  dev = %p, virt_id = %d \n", dev->virt_name, dev, dev->virt_id);
    
	handler_dev_type->detach(emd_dev->dev_handle);

	dev->dh_priv = NULL;
	
	return;
}

inline inm_32_t
emd_sync_queue_type(enum scst_cmd_queue_type qt)
{
	switch(qt) {
		case SCST_CMD_QUEUE_ORDERED:
		case SCST_CMD_QUEUE_HEAD_OF_QUEUE:
			return 1;
		default:
			return 0;
	}

}

inline inm_32_t
emd_need_pre_sync(enum scst_cmd_queue_type cwqt,
	enum scst_cmd_queue_type lwqt)
{
	if (emd_sync_queue_type(cwqt))
		if (!emd_sync_queue_type(lwqt))
			return 1;
	return 0;
}

inm_32_t
emd_attach_tgt(struct scst_tgt_dev *tgt_dev)
{

   if (tgt_dev->acg_dev && tgt_dev->acg_dev->dev)
   {
    	printk(KERN_ALERT "emd_attach_tgt(): tgt_dev = %p tgt_dev->acg_dev = %p dev = %p \n", tgt_dev, tgt_dev->acg_dev, tgt_dev->acg_dev->dev);
   
    	printk(KERN_ALERT "emd_attach_tgt(): dev->virt_id = %d \n", tgt_dev->acg_dev->dev->virt_id);
	if (tgt_dev->acg_dev->dev->virt_name)
    	printk(KERN_ALERT "emd_attach_tgt(): dev->virt_name = %s \n", tgt_dev->acg_dev->dev->virt_name);
   }
   else
   {
    printk(KERN_ALERT "emd_attach_tgt(): tgt_dev = %p tgt_dev->acg_dev = %p \n", tgt_dev, tgt_dev->acg_dev);
   }
   return 0;
}

void
emd_detach_tgt(struct scst_tgt_dev *tgt_dev)
{

    printk(KERN_ALERT "emd_detach_tgt(): tgt_dev = %p \n", tgt_dev);
    	return;
}

inm_32_t emd_get_block_shift(struct scst_cmd *cmd_scst)
{
	return 9;
}

inm_32_t emd_parse(struct scst_cmd *cmd_scst)
{
	inm_32_t ret = SCST_CMD_STATE_DEFAULT;	

	scst_sbc_generic_parse(cmd_scst, emd_get_block_shift);
	return ret;

}


inm_32_t emd_exec(struct scst_cmd *cmd_scst)
{
	inm_32_t opcode = cmd_scst->cdb[0];
	inm_u8_t *cdb = cmd_scst->cdb;
	inm_u64_t lba_start = 0;
	inm_u64_t io_len = 0;

        if (scst_check_local_events(cmd_scst))
                goto out_done;

	switch(opcode)
	{
		case READ_6:
		case READ_10:
		case READ_12:
		case READ_16:
			if (scst_cmd_atomic(cmd_scst)) {
				TRACE_DBG("%s", "READ can not be done in atomic"
					  " context, requesting thread context");
				return SCST_TGT_RES_NEED_THREAD_CTX;
			}
			break;

		default:
			break;
	}

	cmd_scst->status = 0;
	cmd_scst->msg_status = 0;
	cmd_scst->host_status = DID_OK;
	cmd_scst->driver_status = 0;

	switch(opcode)
	{
		case INQUIRY:
			emd_exec_inquiry(cmd_scst);
			break;
			
		case READ_CAPACITY:
			if (cmd_scst->lun == (inm_u64_t)0)
				goto def;
			emd_exec_read_capacity(cmd_scst);
			break;
			
		case SERVICE_ACTION_IN:
			if (cmd_scst->lun == (inm_u64_t)0)
				goto def;
			if ((cmd_scst->cdb[1] & 0x1f) == SAI_READ_CAPACITY_16)				
				emd_exec_read_capacity16(cmd_scst);
			else
				goto def;
			break;

		case TEST_UNIT_READY:
			break;
		case MODE_SENSE:
		case MODE_SENSE_10:
			emd_exec_mode_sense(cmd_scst);
			break;

		case START_STOP:
		case RESERVE:
		case RESERVE_10:
		case RELEASE:
		case RELEASE_10:
		case VERIFY_6:
		case VERIFY:
		case VERIFY_12:
		case VERIFY_16:
		case SYNCHRONIZE_CACHE:
			if (cmd_scst->lun == (inm_u64_t)0)
				goto def;
				break;

		case READ_6:
			if (cmd_scst->lun == (inm_u64_t)0)
				goto def;
			lba_start = (((cdb[1] & 0x1f) << 16) +  (cdb[2] << 8) +  (cdb[3]));
			goto exec_read;

		case READ_12:
		case READ_10:
			if (cmd_scst->lun == (inm_u64_t)0)
				goto def;
			lba_start |= ((u64)cdb[2]) << 24;
			lba_start |= ((u64)cdb[3]) << 16;
			lba_start |= ((u64)cdb[4]) << 8;
			lba_start |= ((u64)cdb[5]);
			goto exec_read;	             
 	              
		case READ_16:                	
			if (cmd_scst->lun == (inm_u64_t)0)
				goto def; 
			lba_start |= ((u64)cdb[2]) << 56;
			lba_start |= ((u64)cdb[3]) << 48;
			lba_start |= ((u64)cdb[4]) << 40;
			lba_start |= ((u64)cdb[5]) << 32;
			lba_start |= ((u64)cdb[6]) << 24;
			lba_start |= ((u64)cdb[7]) << 16;
			lba_start |= ((u64)cdb[8]) << 8;
			lba_start |= ((u64)cdb[9]);
			goto exec_read;
			
		case WRITE_6:
			if (cmd_scst->lun == (inm_u64_t)0)
				goto def; 
			lba_start = (((cdb[1] & 0x1f) << 16) +  (cdb[2] << 8) +  (cdb[3]));
			goto exec_write;
                       
		case WRITE_10:                       
		case WRITE_12:
		case WRITE_VERIFY:
		case WRITE_VERIFY_12:
			if (cmd_scst->lun == (inm_u64_t)0)
				goto def;
			lba_start |= ((u64)cdb[2]) << 24;
			lba_start |= ((u64)cdb[3]) << 16;
			lba_start |= ((u64)cdb[4]) << 8;
			lba_start |= ((u64)cdb[5]);
			goto exec_write;

		case WRITE_CANCEL_CDB:
			if(cmd_scst->lun == (inm_u64_t)0)
				goto def;
			/* lba start stored in 2-9 (total 8 bytes) */
			lba_start |= ((u64)cdb[2]) << 56;
			lba_start |= ((u64)cdb[3]) << 48;
			lba_start |= ((u64)cdb[4]) << 40;
			lba_start |= ((u64)cdb[5]) << 32;
			lba_start |= ((u64)cdb[6]) << 24;
			lba_start |= ((u64)cdb[7]) << 16;
			lba_start |= ((u64)cdb[8]) << 8;
			lba_start |= ((u64)cdb[9]);

			/* Length stored in 10-13 (total 4 bytes) */
			io_len |= ((u64)cdb[10]) << 24;
			io_len |= ((u64)cdb[11]) << 16;
			io_len |= ((u64)cdb[12]) << 8;
			io_len |= ((u64)cdb[13]);
                        printk(KERN_INFO "WRITE CANCEL CDB recvd by emd handler for %llu %llu\n", 
                                lba_start, io_len);
			emd_exec_pt_write_cancel(cmd_scst, lba_start, io_len);
			break;
		case VACP_CDB:
			if(cmd_scst->lun == (inm_u64_t)0)
				goto def;
			io_len |= ((u64)cdb[1]) << 24;
			io_len |= ((u64)cdb[2]) << 16;
			io_len |= ((u64)cdb[3]) << 8;
			io_len |= ((u64)cdb[4]);
                        printk(KERN_INFO "VACP CDB recvd by emd handler of len %llu\n",
                                io_len);
			emd_exec_vacp_write(cmd_scst);
			break;

		case HEARTBEAT_CDB:
			if(cmd_scst->lun == (inm_u64_t)0)
				goto def;
			printk(KERN_INFO "HEARTBEAT CDB recvd by emd handler\n");
			break;

		case WRITE_16:
		case WRITE_VERIFY_16: 
			if (cmd_scst->lun == (inm_u64_t)0)
				goto def;
			lba_start |= ((u64)cdb[2]) << 56;
			lba_start |= ((u64)cdb[3]) << 48;
			lba_start |= ((u64)cdb[4]) << 40;
			lba_start |= ((u64)cdb[5]) << 32;
			lba_start |= ((u64)cdb[6]) << 24;
			lba_start |= ((u64)cdb[7]) << 16;
			lba_start |= ((u64)cdb[8]) << 8;
			lba_start |= ((u64)cdb[9]);

exec_write:
			emd_exec_write(cmd_scst, lba_start);
			break;

exec_read:
			emd_exec_read(cmd_scst, lba_start);
			break;
def:
		default:
			printk(KERN_ALERT "Invalid opcode = 0x%x \n", opcode);
			scst_set_cmd_error(cmd_scst,  SCST_LOAD_SENSE(scst_sense_invalid_opcode));
			break;
	}
out_done:
	cmd_scst->completed = 1;
	cmd_scst->scst_cmd_done(cmd_scst, SCST_CMD_STATE_DEFAULT, SCST_CONTEXT_SAME);	
	return SCST_EXEC_COMPLETED;
}

inm_32_t emd_inquiry_page80(inm_u8_t *buf, char *tc_guid)
{
	inm_32_t len, i, j;

	len = strlen(tc_guid);

	if ((len != 16) || !isdigit(tc_guid[7])) 
		goto non_digit;

	for (i = 7, j = 0; i < 16; i++) {
		if(!isdigit(tc_guid[i]))
			goto non_digit;
		buf[j] = tc_guid[i];
		j++;
	}

done:
	buf[j] = '\0';
	j++;

	return j;

non_digit:
	for (i = 0, j = 0; i < len; j++) {
		if ((j % 2) == 0) {
			buf[j] = ((tc_guid[i] & 0xf0) >> 4) + '0';
		} else {
			buf[j] = (tc_guid[i] & 0x0f) + '0';
			i++;
		}

		if (buf[j] > '9')
			buf[j] += 7;
	}
	goto done;
}


inm_32_t emd_inquiry_page83(inm_u8_t *buf, char *tc_guid)
{
	inm_32_t len, i, j, rlen = 0;

	buf[0] = 0x2;
	buf[1] = 0x1;
	buf[2] = 0x0;
	memcpy_s(&buf[4], 8, INMAGE_EMD_VENDOR, 8);
	memset(&buf[12], ' ', 16);
	i = strlen(tc_guid);
	i = i < 16 ? i : 16;
	memcpy_s(&buf[12], i, tc_guid, i);
	rlen = buf[3] = 8 + 16;
	rlen += 4;
	buf += rlen;  
	buf[0] = 0x1;
	buf[1] = 0x3;
	buf[2] = 0x0;
	buf[3] = 0x8;

	buf[4] = 0x50;
	buf[5] = 0x02;
	buf[6] = 0x38;
	buf[7] = 0x30;

	len = strlen(tc_guid);

	if ((len != 16) || !isdigit(tc_guid[7])) 
		goto non_digit;

	buf[7] |= (tc_guid[7] - '0') & 0x0f;
	for (i = 8, j = 8; i < 16; i++) {
		if(!isdigit(tc_guid[i]))
			goto non_digit;
		if ((i % 2) == 0) {
			buf[j] = ((tc_guid[i] - '0') << 4) & 0xf0;
		} else {
			buf[j] |= ((tc_guid[i] - '0') & 0x0f);
			j++;
		}
	}

done:
	rlen = rlen + 12;

	return rlen;

non_digit:	
	i = len < 5 ? 0 : len - 5;
	j = 7;
	buf[j] |= tc_guid[i] & 0x0f;
	j++;
	i++;
	for (; i < len; i++, j++) 
	{
		buf[j]=  tc_guid[i];
	}
	goto done;
}

void emd_exec_inquiry(struct scst_cmd *cmd_scst)
{
	emd_dev_t *emd_dev = (emd_dev_t *)cmd_scst->dev->dh_priv;
	inm_u8_t *buf, *address;
	inm_32_t len, length, rlen = 0;

	buf = kmalloc(INQUIRY_RSP_SIZE, scst_cmd_atomic(cmd_scst) ? GFP_ATOMIC : GFP_KERNEL);
	if (buf == NULL)
	{
		printk("In emd_exec_inquiry cannot allocat memroy calling scst_set_busy \n");
		scst_set_busy(cmd_scst);
		goto out;
	}

	INM_MEM_ZERO(buf, INQUIRY_RSP_SIZE);
	length = scst_get_buf_first(cmd_scst, &address);

	if (unlikely(length <= 0))
	{
		printk(KERN_ALERT "scst_get_buf_first failed %d \n", length);
		scst_set_cmd_error(cmd_scst, SCST_LOAD_SENSE(scst_sense_hardw_error));
		goto out_free;
	}

	if (cmd_scst->cdb[1] & CMDDT)
	{
		printk(KERN_ALERT "%s", "INQUIRY: CMDDT is unsupported\n");
		scst_set_cmd_error(cmd_scst, SCST_LOAD_SENSE(scst_sense_invalid_field_in_cdb));
		goto out_put;
	}

	if (cmd_scst->tgt_dev->lun != cmd_scst->lun) {
		buf[0] = 0x60;
		buf[0] |= 0x1F;

		 rlen = 5;
		goto out_check_len;
	} else {
	buf[0] = cmd_scst->dev->handler->type;
	}

	if (cmd_scst->cdb[1] & EVPD)
	{
		switch(cmd_scst->cdb[2])
		{
			case 0x00:
				buf[1] = 0x00;
				buf[3] = 2;
				buf[4] = 0x00;
				buf[5] = 0x83;
				rlen = buf[3] + 4;
				break;
			case 0x80:
	                        buf[1] = 0x80;
				rlen = emd_inquiry_page80(buf + 4, emd_dev->dev_name);
                    	        buf[3] = rlen;
				rlen += 4;
				break;
			case 0x83:
				buf[1] = 0x83;

				rlen = emd_inquiry_page83(buf + 4, emd_dev->dev_name);
				buf[2] = (rlen >> 8) & 0xFF;
				buf[3] = rlen & 0xFF;
				rlen += 4;
				break;
	
			default:
				printk(KERN_ALERT "INQUIRY: Unsupported EVPD page %x\n", cmd_scst->cdb[2]);
			 	scst_set_cmd_error(cmd_scst, 
									SCST_LOAD_SENSE(scst_sense_invalid_field_in_cdb));
				goto out_put;
		}
	}
	else   
    {
        if (cmd_scst->cdb[2] != 0) 
        {
			printk(KERN_ALERT "INQUIRY: Unsupported page %x", cmd_scst->cdb[2]);
			scst_set_cmd_error(cmd_scst,
		    				   SCST_LOAD_SENSE(scst_sense_invalid_field_in_cdb));
			goto out_put;
		}
       
		buf[2] = 4;
		buf[3] = 0x12;
		buf[4] = 31;
		buf[6] = 0; buf[7] = 2;

		memcpy_s(&buf[8], 8, INMAGE_EMD_VENDOR, 8);

		memset(&buf[16], ' ', 16);
		len = strlen(emd_dev->dev_name);
		len = len < 16 ? len : 16;
		memcpy_s(&buf[16], len, emd_dev->dev_name, len);
		
		memcpy_s(&buf[32], 4, INMAGE_EMD_REV, 4);
		rlen = buf[4] + 5;
    }

    BUG_ON(rlen >= INQUIRY_RSP_SIZE);
out_check_len: 
    if (length > rlen)
		length = rlen;
    
	memcpy_s(address, length, buf, length);
    
out_put:
	scst_put_buf(cmd_scst, address);
	if (length < cmd_scst->resp_data_len)
		scst_set_resp_data_len(cmd_scst, length);
    
out_free:
	kfree(buf);

out:
	return;
}

void emd_exec_mode_select(struct scst_cmd *cmd_scst)
{
    return;
}

void emd_exec_read_capacity(struct scst_cmd *cmd_scst)
{
    emd_dev_t *emd_dev = (emd_dev_t*)cmd_scst->dev->dh_priv;
    emd_dev_cap_t cap;
    inm_u32_t blocksize, length;
    inm_u64_t nblocks;
    inm_u8_t *address;
    inm_u8_t buffer[READ_CAP_LEN];


    handler_dev_type->get_capacity(emd_dev->dev_handle, &cap);
    nblocks = cap.nblocks;
    blocksize = cap.bsize;

    INM_MEM_ZERO(buffer, sizeof(buffer));

    if (nblocks >>32)
    {
        buffer[0] = 0xFF;
        buffer[1] = 0xFF;
        buffer[2] = 0xFF;
        buffer[3] = 0xFF;
    }
    else
    {
        buffer[0] = (((nblocks -1 ) >> 24) & 0xFF);
        buffer[1] = (((nblocks -1 ) >> 16) & 0xFF);
        buffer[2] = (((nblocks -1 ) >> 8) & 0xFF);
        buffer[3] = ((nblocks -1 ) & 0xFF);
    }

    buffer[4] = ((blocksize >> 24) & 0xFF);
    buffer[5] = ((blocksize >> 16) & 0xFF);
    buffer[6] = ((blocksize >> 8) & 0xFF);
    buffer[7] = (blocksize & 0xFF);

    length = scst_get_buf_first(cmd_scst, &address);
        if (unlikely(length <= 0)) {
            printk(KERN_ALERT "scst_get_buf_first() failed: %d\n", length);
            scst_set_cmd_error(cmd_scst,
                SCST_LOAD_SENSE(scst_sense_hardw_error));
            goto out;
        }
    
        if (length > READ_CAP_LEN)
            length = READ_CAP_LEN;
        memcpy_s(address, length, buffer, length);
    
        scst_put_buf(cmd_scst, address);
    
        if (length < cmd_scst->resp_data_len)
            scst_set_resp_data_len(cmd_scst, length);
    
    out:

        return;
}

void emd_exec_heartbeat(struct scst_cmd *cmd_scst)
{

	return;
}


void emd_exec_vacp_write(struct scst_cmd *cmd_scst)
{
	emd_dev_t *emd_dev = (emd_dev_t*)cmd_scst->dev->dh_priv;
	loff_t data_len;
	inm_32_t iv_count = 0;
	struct iovec *iv = NULL;
	ssize_t length, full_len;
	inm_u8_t *address;
	char *vacp_tag_data = NULL;
	inm_32_t offset = 0, index = 0, iov_rem = 0;
	char *src = NULL;

	data_len = cmd_scst->bufflen;

	if (unlikely(data_len < 0))
	{
		printk(KERN_ALERT "emd_exec_vacp_write: data_len less than 0. INVALID "
			"(len %Ld)\n", (inm_u64_t)data_len);
		scst_set_cmd_error(cmd_scst, SCST_LOAD_SENSE(
		scst_sense_block_out_range_error));
		goto done;
	}


	iv_count = scst_get_buf_count(cmd_scst);
	iv = kmalloc((sizeof(struct iovec) * iv_count), scst_cmd_atomic(cmd_scst) ? GFP_ATOMIC : GFP_KERNEL);
	if (iv == NULL)
	{
		printk(KERN_ALERT "emd_exec_vacp_write: Could not allocate iovec\n");
		scst_set_busy(cmd_scst);
		goto done;
	}

	INM_MEM_ZERO(iv, sizeof(struct iovec) * iv_count);
	iv_count = 0;
	full_len = 0;
	length = scst_get_buf_first(cmd_scst, &address);
	while (length > 0)
	{
		full_len += length;
		iv[iv_count].iov_base = address;
		iv[iv_count].iov_len = length;
		iv_count++;
		length = scst_get_buf_next(cmd_scst, &address);
	}

	if (unlikely(length < 0))
	{
		printk(KERN_ALERT "emd_exec_vacp_write: scst_get_buf_() failed: %zd\n", length);
		scst_set_cmd_error(cmd_scst,
		SCST_LOAD_SENSE(scst_sense_hardw_error));
		goto free_buf;
	}

	BUG_ON(data_len != full_len);

	vacp_tag_data = kmalloc((sizeof(char) * data_len), scst_cmd_atomic(cmd_scst) ? GFP_ATOMIC : GFP_KERNEL);
	if (vacp_tag_data == NULL)
	{
		printk(KERN_ALERT "emd_exec_vacp_write: Could not allocate vacp_tag_data\n");
		scst_set_busy(cmd_scst);
		goto free_buf;
	}

	offset = 0;
	for (index = 0; index < iv_count; index++) {
		src = (char *)(iv[index].iov_base);
		iov_rem = iv[index].iov_len;
		if (memcpy_s((char *)(vacp_tag_data + offset), iov_rem, (char *) src, iov_rem)) {
			printk(KERN_ALERT "emd_exec_vacp_write: memcpy_s error");
			scst_set_busy(cmd_scst);
			kfree(vacp_tag_data);
			goto free_buf;
		}

		offset += iov_rem;
	}

#ifdef INM_DEBUG
	for (index = 0; index < data_len; index++) {
		printk(KERN_INFO "%c", vacp_tag_data[index]);
	}
#endif
#ifdef INM_DEBUG
	printk(KERN_INFO "emd_exec_vacp_write: data_len: %lld\n", data_len);
#endif

	handler_dev_type->exec_vacp_write(emd_dev->dev_handle, vacp_tag_data, data_len);

	kfree(vacp_tag_data);

free_buf:
	while (iv_count > 0)
	{
		scst_put_buf(cmd_scst, iv[iv_count-1].iov_base);
		iv_count--;
	}
	if (iv)
		kfree(iv);

done:
	return;
}


void emd_exec_read_capacity16(struct scst_cmd *cmd_scst)
{

    emd_dev_t *emd_dev = (emd_dev_t*)cmd_scst->dev->dh_priv;
    emd_dev_cap_t cap;
    inm_u32_t blocksz, length;
    inm_u64_t nblks;
    inm_u8_t *address;
    inm_u8_t buf[READ_CAP16_LEN];

    handler_dev_type->get_capacity(emd_dev->dev_handle, &cap);
    nblks = cap.nblocks;
    blocksz = cap.bsize;

    INM_MEM_ZERO(buf, sizeof(buf));

    buf[0] = nblks >> 56;
    buf[1] = (nblks >> 48) & 0xFF;
    buf[2] = (nblks >> 40) & 0xFF;
    buf[3] = (nblks >> 32) & 0xFF;
    buf[4] = (nblks >> 24) & 0xFF;
    buf[5] = (nblks >> 16) & 0xFF;
    buf[6] = (nblks >> 8) & 0xFF;
    buf[7] = nblks & 0xFF;

    buf[8] = ((blocksz >> 24) & 0xFF);
    buf[9] = ((blocksz >> 16) & 0xFF);
    buf[10] = ((blocksz >> 8) & 0xFF);
    buf[11] = (blocksz & 0xFF);

    length = scst_get_buf_first(cmd_scst, &address);
        if (unlikely(length <= 0)) {
            printk(KERN_ALERT "scst_get_buf_first() failed: %d\n", length);
            scst_set_cmd_error(cmd_scst,
                SCST_LOAD_SENSE(scst_sense_hardw_error));
            goto out;
        }
    
        if (length > READ_CAP16_LEN)
            length = READ_CAP16_LEN;
        memcpy_s(address, length, buf, length);
    
        scst_put_buf(cmd_scst, address);
    
        if (length < cmd_scst->resp_data_len)
            scst_set_resp_data_len(cmd_scst, length);
    
    out:

        return;
}

static void emd_exec_read_file(struct scst_cmd *cmd_scst, loff_t loff, struct file *fp)
{
	mm_segment_t    old_fs;
	inm_u8_t         *address;
	ssize_t length, full_len;
	inm_32_t       i;
	struct iovec    *iv;
	inm_32_t       iv_count = 0;
	loff_t          error;


	iv_count = scst_get_buf_count(cmd_scst);
	iv = kmalloc((sizeof(struct iovec) * iv_count), scst_cmd_atomic(cmd_scst) ? GFP_ATOMIC :  GFP_KERNEL);
	if (iv == NULL)
	{
		printk(KERN_ALERT "Could not allocate iovec\n");
		scst_set_busy(cmd_scst);
		goto out;
	}

	iv_count = 0;
	full_len = 0;
	i = -1;
	length = scst_get_buf_first(cmd_scst, &address);
	while (length > 0) {
		full_len += length;
		i++;
		iv_count++;
		iv[i].iov_base = address;
		iv[i].iov_len = length;
		length = scst_get_buf_next(cmd_scst, &address);
	}
	if (unlikely(length < 0)) {
		PRINT_ERROR("scst_get_buf_() failed: %zd", length);
		scst_set_cmd_error(cmd_scst,
			SCST_LOAD_SENSE(scst_sense_hardw_error));
		goto out_put;
	}
	old_fs = get_fs();
	set_fs(get_ds());

	TRACE_DBG("reading(iv_count %d, full_len %zd)", iv_count, full_len);
	if (fp->f_op->llseek)
		error = fp->f_op->llseek(fp, loff, 0);
	else
		error = default_llseek(fp, loff, 0);
	if (error != loff) {
	PRINT_ERROR("lseek trouble %Ld != %Ld",
		(inm_u64_t)error,
		(inm_u64_t)loff);
                scst_set_cmd_error(cmd_scst, SCST_LOAD_SENSE(scst_sense_hardw_error));
                goto out_set_old_fs;
        }
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 19)
	error = fp->f_op->readv(fp, iv, iv_count, &fp->f_pos);
#else
	error = vfs_readv(fp, (struct iovec __force __user *)iv, iv_count, &fp->f_pos);
#endif

	if ((error < 0) || (error < full_len)) {
		PRINT_ERROR("readv() returned %Ld from %zd",
			(inm_u64_t)error,
			full_len);
		if (error == -EAGAIN)
			scst_set_busy(cmd_scst);
		else {
			scst_set_cmd_error(cmd_scst,
				SCST_LOAD_SENSE(scst_sense_read_error));
		}
		goto out_set_old_fs;
	}

out_set_old_fs:
	set_fs(old_fs);
out_put:
	for (; i >= 0; i--)
		scst_put_buf(cmd_scst, iv[i].iov_base);

out:
	return;
}

void emd_exec_read(struct scst_cmd *cmd_scst, inm_u64_t lba_start)
{
	emd_dev_t *emd_dev = (emd_dev_t*)cmd_scst->dev->dh_priv;
	emd_dev_cap_t	cap;
	struct file     *fp;
	struct block_device *bdev;
	loff_t 		loff;
	inm_32_t 		flags = 0;
	const char	*f_name = NULL;

	f_name = handler_dev_type->get_path(emd_dev->dev_handle, &flags);
	handler_dev_type->get_capacity(emd_dev->dev_handle, &cap);
	BUG_ON(!f_name);
	fp = emd_open(f_name);
	if (fp == NULL) {
		printk(KERN_ALERT"SCST: unable to open AT file \n");
		goto open_fail;
	}

	loff = (loff_t) lba_start * cap.bsize;
	emd_exec_read_file(cmd_scst, loff, fp);

	filp_close(fp, NULL);
out:
return;

block_fail:
	filp_close(fp, NULL);
open_fail:
	scst_set_cmd_error(cmd_scst,
		SCST_LOAD_SENSE(scst_sense_hardw_error));
	goto out;
}

void emd_exec_write(struct scst_cmd *cmd_scst, inm_u64_t lba_start)
{
	emd_dev_t *emd_dev = cmd_scst->dev->dh_priv;
	emd_dev_cap_t cap;
	inm_32_t iv_count = 0;
	struct iovec *iv = NULL;
 	ssize_t length, full_len;
	emd_io_t *eio = NULL;
	inm_u8_t *address;
	inm_u32_t idx = 0;

	if (handler_dev_type->prepare_write(emd_dev->dev_handle, lba_start, cmd_scst->bufflen)) {
		printk(KERN_ALERT "prepare_write failed because of Access beyond the end of the device "
			"(lba_start %lld, len %Ld)\n", lba_start, cmd_scst->bufflen);
		scst_set_cmd_error(cmd_scst, SCST_LOAD_SENSE(scst_sense_block_out_range_error));
		goto done;
	}

	iv_count = scst_get_buf_count(cmd_scst);
	iv = kmalloc((sizeof(struct iovec) * iv_count), scst_cmd_atomic(cmd_scst) ? GFP_ATOMIC : GFP_KERNEL);
	if (iv == NULL)
	{
		printk(KERN_ALERT "In emd_exec_write Could not allocate iovec calling scst_set_busy\n");
		scst_set_busy(cmd_scst);
		goto done;
	}

	INM_MEM_ZERO(iv, sizeof(struct iovec) * iv_count);
	iv_count = 0;
	full_len = 0;
	length = scst_get_buf_first(cmd_scst, &address);
	while (length > 0) 
	{
		full_len += length;
		iv[iv_count].iov_base = address;
		iv[iv_count].iov_len = length;
		iv_count++;
		length = scst_get_buf_next(cmd_scst, &address);
	}
			
	if (unlikely(length < 0)) 
	{
		printk(KERN_ALERT "scst_get_buf_() failed: %zd", length);
		scst_set_cmd_error(cmd_scst,
		    SCST_LOAD_SENSE(scst_sense_hardw_error));
		goto free_buf;
	}
	eio = kmalloc((sizeof(emd_io_t)), scst_cmd_atomic(cmd_scst) ? GFP_ATOMIC : GFP_KERNEL);
	if (eio == NULL) {
		printk(KERN_ALERT "EMD: Alloc failed\n");
		scst_set_cmd_error(cmd_scst,
		    SCST_LOAD_SENSE(scst_sense_hardw_error));
		goto free_buf;
	}

	eio->eio_dev_handle = emd_dev->dev_handle;
	eio->eio_len = full_len;
	eio->eio_iname = cmd_scst->sess->initiator_name;
	eio->eio_iovp = (void *) iv;
	eio->eio_iovcnt = iv_count;
	eio->eio_rw = WRITE;
	eio->eio_start = lba_start;

	if (handler_dev_type->exec_write(eio)) {
		printk(KERN_ALERT"In emd_exec_write: got error from dev handler\n");
		scst_set_cmd_error(cmd_scst,
                    SCST_LOAD_SENSE(scst_sense_hardw_error));
		goto free_eio;
	}

free_eio:
	kfree(eio);
free_buf:
	while (iv_count > 0) 
	{
		scst_put_buf(cmd_scst, iv[iv_count-1].iov_base);
		iv_count--;
	}

	if (iv)
		kfree(iv);
        
done:        
        return;
}

void emd_exec_pt_write_cancel(struct scst_cmd *cmd_scst, inm_u64_t lba_start, 
				inm_u64_t io_len)
{
	emd_dev_t *emd_dev = (emd_dev_t*)cmd_scst->dev->dh_priv;
	emd_dev_cap_t cap;
	loff_t loff, data_len;
	inm_u64_t logical_lba_start = lba_start;

	handler_dev_type->get_capacity(emd_dev->dev_handle, &cap);
	printk("PT I/O Cancellation for LUN = 0x%llx, offset = 0x%llx, length = 0x%llx, block size = 0x%x",
			cmd_scst->lun, lba_start, io_len, cap.bsize);

	logical_lba_start -= (cap.startoff / cap.bsize);
	loff = (loff_t)(logical_lba_start * cap.bsize);
	data_len = (io_len * cap.bsize);

	if (unlikely(loff < 0) || unlikely(data_len < 0) ||
		unlikely((loff + data_len) > (cap.nblocks * cap.bsize))){
		printk(KERN_ALERT "Access beyond the end of the device "
			"(%lld of %lld, len %Ld)\n", (inm_u64_t)loff,
			(inm_u64_t)(cap.nblocks * cap.bsize),
			(inm_u64_t)data_len);

		scst_set_cmd_error(cmd_scst, SCST_LOAD_SENSE(scst_sense_block_out_range_error));
	}
	handler_dev_type->exec_io_cancel(emd_dev->dev_handle, lba_start, io_len);

	return;
}

inm_32_t emd_task_mgmt_fn(struct scst_mgmt_cmd *mcmd_scst,
	struct scst_tgt_dev *tgt_dev)
{
	TRACE_ENTRY();

	if ((SCST_TARGET_RESET == mcmd_scst->fn) || (SCST_LUN_RESET == mcmd_scst->fn)) {
		struct scst_device *dev_scst = tgt_dev->dev;
		dev_scst->tst = DEF_TST;
		dev_scst->queue_alg = DEF_QUEUE_ALG;
		dev_scst->swp = DEF_SWP;
		dev_scst->tas = DEF_TAS;
	}

	TRACE_EXIT();
	return SCST_DEV_TM_NOT_COMPLETED;
}

inm_32_t emd_proc_info(char *buffer, char **start, off_t offset,
inm_32_t length, inm_32_t *eof, struct scst_dev_type *dev_type, inm_32_t inout)
{
if (inout != 0)
	return(emd_write_proc(buffer,start,offset,length, eof, dev_type));	
return 0;	
}


inm_32_t emd_read_proc(struct seq_file *seq, struct scst_dev_type *dev_type)
{
    inm_32_t r = 0;

    seq_printf(seq, "Yet to implement !!!");

    return r;
}


filter_lun_delete(char *name)
{

	return 0;
}

inm_32_t 
filter_lun_create(char* uuid, inm_u64_t nblks, inm_u32_t bsize, inm_u64_t startoff)
{

	return 0;
}


inm_32_t emd_write_proc(char *buffer, char **start, off_t offset, inm_32_t length,
                   inm_32_t *eof, struct scst_dev_type *dev_type)
{
    inm_32_t action;
    inm_32_t ret = 0;
    char *p, *name;
    inm_u64_t nblocks; 
    
    if (!buffer || buffer[0] == '\0') {
         goto out;
    }
    
    p = buffer;
    if (p[strlen(p) - 1] == '\n') {
        p[strlen(p) - 1] = '\0';
	}
	if (!strncmp("close ", p, 6)) {
        p += 6;
		action = 0;
	} else if (!strncmp("open ", p, 5)) {
		p += 5;
		action = 2;
	} else {
		printk(KERN_ALERT "Unknown action \"%s\"", p);
		ret = -EINVAL;
		goto out;
	}

    while (isspace(*p) && *p != '\0') {
		p++;
    }
        
	name = p;
	while (!isspace(*p) && *p != '\0') {
		p++;
    }
	*p++ = '\0';
    
	if (*name == '\0') {
		printk(KERN_ALERT "%s", "Name required");
		ret = -EINVAL;
		goto out;
	}
    else if (strlen(name) >= INM_MAX_UUID_LEN) {
        printk(KERN_ALERT "Name is too long (max %d "
			"characters)", INM_MAX_UUID_LEN-1);
		ret = -EINVAL;
		goto out;
	}
    
    if(action) {
        while (isspace(*p) && *p != '\0') {
            p++;
        }

        if (!isdigit(*p)) {
            ret = -EINVAL;
            goto out;
        } 
    
        nblocks = simple_strtoull(p, (char **)NULL, 0);
        if (!filter_lun_create(name, (inm_u64_t) nblocks, (inm_u32_t)0, (inm_u64_t)0)) {
            return length;
        }
        else {
            return 0;
        }
    }
    else {
        filter_lun_delete(name);
    }
    ret = length;                

out:
    return ret; 
}


inm_32_t emd_help_info_show(struct seq_file *seq, void *v)
{
	char *s = (char*)seq->private;

	seq_printf(seq, "%s", s);

	return 0;
}

inm_32_t emd_proc_help_read(char *buffer, char **start, off_t offset,
				inm_32_t length, inm_32_t *eof, void *data)
{
	inm_32_t r = 0;
	char *s  = (char *)data;

	if (offset < strlen(s))
		r = scnprintf(buffer, length, "%s", &s[offset]);
	
	return r;
}

#ifdef  CONFIG_SCST_PROC
inm_32_t emd_proc_help_build(struct scst_dev_type *dev_type)
{
    inm_32_t r = 0;
    struct proc_dir_entry *p, *root;

    root = scst_proc_get_dev_type_root(dev_type);
	emd_help_proc_data.data = emd_proc_help_string;
	p = scst_create_proc_entry(root, EMD_PROC_HELP,
		&emd_help_proc_data);

    if (p == NULL) 
    {
	printk(KERN_ALERT "Not enough memory to register dev "
	     "handler %s entry %s in /proc",
	      dev_type->name, EMD_PROC_HELP);
	r = -ENOMEM;
	goto out;
    }    

out:
    return r;
}

void emd_proc_help_destroy(struct scst_dev_type *dev_type)
{

	struct proc_dir_entry *root;

	root = scst_proc_get_dev_type_root(dev_type);
	if (root)
		remove_proc_entry(EMD_PROC_HELP, root);

        return;

}
#endif

inm_32_t
emd_attach(struct scst_device *dev)
{
	inm_32_t 		ret = -EINVAL;
	emd_handle_t 		handle;
	emd_dev_t		*emd_dev = NULL;
	struct list_head	*ptr = NULL;

	printk(KERN_ALERT "emd_attach(): name = %s  dev = %p, virt_id = %d \n", dev->virt_name, dev, dev->virt_id);
    
	if (dev->virt_id == 0)
	{
		printk(KERN_ALERT "Invalid Virtual Device\n");
		goto out;
	}


	list_for_each(ptr, &emd_dev_list) {
		emd_dev = list_entry(ptr, emd_dev_t, dev_list);
		if (strcmp(emd_dev->dev_name, dev->virt_name) == 0) {
			ret = 0;
			break;
		}
	}
 
	
	handle = handler_dev_type->attach((char *)dev->virt_name); //get
    
	if (handle == 0) {
		ret = -EINVAL;
		goto out;
	}

	dev->dh_priv = emd_dev;
	emd_dev->dev_handle = handle;

	printk(KERN_ALERT "emd_attach(): dev->dh_priv = %p handle = %lx \n", dev->dh_priv, handle);
out:

        return ret;
}



struct scst_dev_type emulated_disk  = { \
    .name = EMD_NAME, 
    .type = TYPE_DISK,
    .threads_num = 0,
    .parse_atomic = 1,
    .dev_done_atomic = 1,
    .alloc_data_buf_atomic = 1,
    .attach = emd_attach,
    .detach = emd_detach,
    .attach_tgt = emd_attach_tgt,
    .detach_tgt = emd_detach_tgt,
    .parse = emd_parse,
    .exec = emd_exec,
#ifdef	CONFIG_SCST_PROC   
    .read_proc = emd_read_proc,
    .write_proc = emd_write_proc,
#endif
    .task_mgmt_fn = emd_task_mgmt_fn,
};

inm_32_t __init init_emd()
{

    inm_32_t r = 0;

    emulated_disk.module = THIS_MODULE;

    r = scst_register_virtual_dev_driver(&emulated_disk);

    if (r < 0)
        goto out;


#ifdef CONFIG_SCST_PROC
    r = scst_dev_handler_build_std_proc(&emulated_disk);

    if (r < 0)
        goto out_unreg;

    r = emd_proc_help_build(&emulated_disk);
    
    if (r < 0) 
	goto out_destroy_proc;
#endif
    

out:
        return r;

out_destroy_proc:    
#ifdef CONFIG_SCST_PROC
        scst_dev_handler_destroy_std_proc(&emulated_disk);
    
out_unreg:
    
     scst_unregister_virtual_dev_driver(&emulated_disk);
#endif
     goto out; 

}

void __exit exit_emd()
{

#ifdef CONFIG_SCST_PROC
    emd_proc_help_destroy(&emulated_disk);

    scst_dev_handler_destroy_std_proc(&emulated_disk);
#endif
    scst_unregister_virtual_dev_driver(&emulated_disk);

    return;
}



static inm_32_t emd_error_recovery_page(inm_u8_t *page, inm_32_t ctrl)
{
	const inm_u8_t error_recovery_page[] = {0x1, 0xa, 0xc0, 11, 240, 0, 0, 0,
                                              5, 0, 0xff, 0xff};

	memcpy_s(page, sizeof(error_recovery_page), error_recovery_page, sizeof(error_recovery_page));
	if (ctrl == 1)
		memset(page + 2, 0, sizeof(error_recovery_page) - 2);
	return sizeof(error_recovery_page);
}

static inm_32_t emd_disconnect_page(inm_u8_t *page, inm_32_t ctrl)
{
	const inm_u8_t disconnect_page[] = {0x2, 0xe, 128, 128, 0, 10, 0, 0,
                                               0, 0, 0, 0, 0, 0, 0, 0};

	memcpy_s(page, sizeof(disconnect_page), disconnect_page, sizeof(disconnect_page));
	if (ctrl == 1)
		memset(page + 2, 0, sizeof(disconnect_page) - 2);
	return sizeof(disconnect_page);
}

static inm_32_t emd_format_page(inm_u8_t *page, inm_32_t ctrl, emd_dev_cap_t *cap)
{
	const inm_u8_t format_page[] = {0x3, 0x16, 0, 0, 0, 0, 0, 0,
                                           0, 0, 0, 0, 0, 0, 0, 0,
                                           0, 0, 0, 0, 0x40, 0, 0, 0};

	memcpy_s(page, sizeof(format_page), format_page, sizeof(format_page));
	page[10] = (INM_SECTORS_PER >> 8) & 0xff;
	page[11] = INM_SECTORS_PER & 0xff;
	page[12] = (cap->bsize >> 8) & 0xff;
	page[13] = cap->bsize & 0xff;
	if (ctrl == 1)
		memset(page + 2, 0, sizeof(format_page) - 2);
	return sizeof(format_page);
}

static inm_32_t emd_caching_page(inm_u8_t *page, inm_32_t ctrl)
{
	const inm_u8_t caching_page[] = {0x8, 18, 0x10, 0, 0xff, 0xff, 0, 0,
        	        0xff, 0xff, 0xff, 0xff, 0x80, 0x14, 0, 0, 0, 0, 0, 0};

	memcpy_s(page, sizeof(caching_page), caching_page, sizeof(caching_page));
	if (ctrl == 1)
		memset(page + 2, 0, sizeof(caching_page) - 2);
	return sizeof(caching_page);
}

static inm_32_t emd_ctrl_m_page(inm_u8_t *page, inm_32_t ctrl,
                                 struct scst_device *dev)
{
	const inm_u8_t ctrl_m_page[] = {0xa, 0xa, 0, 0, 0, 0, 0, 0,
                                           0, 0, 0x2, 0x4b};

	memcpy_s(page, sizeof(ctrl_m_page), ctrl_m_page, sizeof(ctrl_m_page));
	switch (ctrl) {
		case 0:
			page[2] |= dev->tst << 5;
			page[3] |= dev->queue_alg << 4;
			page[4] |= dev->swp << 3;
			page[5] |= dev->tas << 6;
			break;
		case 1:
			memset(page + 2, 0, sizeof(ctrl_m_page) - 2);
			break;
		case 2:
			page[2] |= DEF_TST << 5;
			page[3] |= DEF_QUEUE_ALG << 4;
			page[4] |= DEF_SWP << 3;
			page[5] |= DEF_TAS << 6;
			break;
		default:
			sBUG();
	}
	return sizeof(ctrl_m_page);
}

static inm_32_t emd_iec_m_page(inm_u8_t *page, inm_32_t ctrl)
{
const inm_u8_t iec_m_page[] = {0x1c, 0xa, 0x08, 0, 0, 0, 0, 0,
                                          0, 0, 0x0, 0x0};
	memcpy_s(page, sizeof(iec_m_page), iec_m_page, sizeof(iec_m_page));
	if (ctrl == 1)
		memset(page + 2, 0, sizeof(iec_m_page) - 2);
	return sizeof(iec_m_page);
}

static inm_32_t emd_rigid_geo_page(inm_u8_t *page, inm_32_t ctrl,
                            emd_dev_cap_t *cap)
{
inm_u8_t geo_m_page[] = {0x04, 0x16, 0, 0, 0, INM_DEF_HEADS, 0, 0,
                                    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                                    0x3a, 0x98/* 15K RPM */, 0, 0};
	inm_32_t ncyl, n, rem;
	inm_u64_t dividend;

	memcpy_s(page, sizeof(geo_m_page), geo_m_page, sizeof(geo_m_page));
	dividend = cap->nblocks;
	rem = do_div(dividend, INM_DEF_HEADS * INM_DEF_SECTORS);
	ncyl = dividend;
	if (rem != 0)
		ncyl++;
	memcpy_s(&n, sizeof(u32), page + 2, sizeof(u32));
	n = n | (cpu_to_be32(ncyl) >> 8);
	memcpy_s(page + 2, sizeof(u32), &n, sizeof(u32));
	if (ctrl == 1)
		memset(page + 2, 0, sizeof(geo_m_page) - 2);
	return sizeof(geo_m_page);
}

void emd_exec_mode_sense(struct scst_cmd *cmd_scst)
{
	emd_dev_t *emd_dev = (emd_dev_t *)cmd_scst->dev->dh_priv;
	emd_dev_cap_t cap;
	inm_32_t buf_len;
	inm_u8_t *addr;
	inm_u8_t *buffer;
	inm_u32_t blocksz;
	inm_u64_t nblks;
	inm_u8_t dbd, type;
	inm_32_t ctrl, pcode, sub_pcode;
	inm_u8_t dev_spec;
	inm_32_t msense, off = 0, len;
	inm_u8_t *bp;

	TRACE_ENTRY();

	buffer = kzalloc(INM_MODES_ENSE_BUF_SZ,
	scst_cmd_atomic(cmd_scst) ? GFP_ATOMIC : GFP_KERNEL);
	if (buffer == NULL) { 
		printk("In emd_exec_mode_sense cannot allocat memroy calling scst_set_busy \n");
		scst_set_busy(cmd_scst);
		goto out;
	}

	handler_dev_type->get_capacity(emd_dev->dev_handle, &cap);
	nblks = cap.nblocks;
	blocksz = cap.bsize;

	type = cmd_scst->dev->handler->type;
	dbd = cmd_scst->cdb[1] & INM_DBD;
	ctrl = (cmd_scst->cdb[2] & 0xc0) >> 6;
	pcode = cmd_scst->cdb[2] & 0x3f;
	sub_pcode = cmd_scst->cdb[3];
	msense = (MODE_SENSE == cmd_scst->cdb[0]);
	dev_spec = INM_DPOFUA;

	buf_len = scst_get_buf_first(cmd_scst, &addr);
	if (unlikely(buf_len <= 0)) {
		if (buf_len < 0) {
			PRINT_ERROR("scst_get_buf_first() failed: %d", buf_len);
			scst_set_cmd_error(cmd_scst,
			SCST_LOAD_SENSE(scst_sense_hardw_error));
		}
		goto out_free;
	}

	if (ctrl == 0x3) {
		TRACE_DBG("%s", "MODE SENSE: Saving values not supported");
		scst_set_cmd_error(cmd_scst,
		SCST_LOAD_SENSE(scst_sense_saving_params_unsup));
		goto out_put;
	}

	if (msense) {
                buffer[1] = type;
                buffer[2] = dev_spec;
                off = 4;
        } else {
                buffer[2] = type;
                buffer[3] = dev_spec;
                off = 8;
        }

        if (sub_pcode != 0) {
	        TRACE_DBG("%s", "MODE SENSE: Only subpage 0 is supported");
        	scst_set_cmd_error(cmd_scst,
	        SCST_LOAD_SENSE(scst_sense_invalid_field_in_cdb));
        	goto out_put;
        }

	if (!dbd) {
		buffer[off - 1] = 0x08;
		if (nblks >> 32) {
		buffer[off + 0] = 0xFF;
		buffer[off + 1] = 0xFF;
		buffer[off + 2] = 0xFF;
		buffer[off + 3] = 0xFF;
	} else {
		buffer[off + 0] = (nblks >> (INM_BYTE * 3)) & 0xFF;
		buffer[off + 1] = (nblks >> (INM_BYTE * 2)) & 0xFF;
		buffer[off + 2] = (nblks >> (INM_BYTE * 1)) & 0xFF;
		buffer[off + 3] = (nblks >> (INM_BYTE * 0)) & 0xFF;
	}
	buffer[off + 4] = 0;
	buffer[off + 5] = (blocksz >> (INM_BYTE * 2)) & 0xFF;
	buffer[off + 6] = (blocksz >> (INM_BYTE * 1)) & 0xFF;
	buffer[off + 7] = (blocksz >> (INM_BYTE * 0)) & 0xFF;

	off += 8;
        }

	bp = buffer + off;

	switch (pcode) {
	case 0x1:
		len = emd_error_recovery_page(bp, ctrl);
		off += len;
	break;
	case 0x2:
		len = emd_disconnect_page(bp, ctrl);
		off += len;
	break;
	case 0x3:
	len = emd_format_page(bp, ctrl, &cap);
	off += len;
	break;
	case 0x4:
	len = emd_rigid_geo_page(bp, ctrl, &cap);
	off += len;
	break;
	case 0x8:
	len = emd_caching_page(bp, ctrl);
	off += len;
	break;
	case 0xa:
	len = emd_ctrl_m_page(bp, ctrl, cmd_scst->dev);
	off += len;
	break;
	case 0x1c:
	len = emd_iec_m_page(bp, ctrl);
	off += len;
	break;
	case 0x3f:
	len = emd_error_recovery_page(bp, ctrl);
	len += emd_disconnect_page(bp + len, ctrl);
	len += emd_format_page(bp + len, ctrl, &cap);
	len += emd_rigid_geo_page(bp + len, ctrl, &cap);
	len += emd_caching_page(bp + len, ctrl);
	len += emd_ctrl_m_page(bp + len, ctrl, cmd_scst->dev);
	len += emd_iec_m_page(bp + len, ctrl);
	off += len;
	break;
	default:
	TRACE_DBG("MODE SENSE: Unsupported page %x", pcode);
	scst_set_cmd_error(cmd_scst,
	SCST_LOAD_SENSE(scst_sense_invalid_field_in_cdb));
	goto out_put;
	}
	if (msense)
		buffer[0] = off - 1;
	else {
		buffer[0] = ((off - 2) >> 8) & 0xff;
		buffer[1] = (off - 2) & 0xff;
	}

	if (off > buf_len)
		off = buf_len;
	memcpy_s(addr, off, buffer, off);

out_put:
	scst_put_buf(cmd_scst, addr);
	if (off < cmd_scst->resp_data_len)
		scst_set_resp_data_len(cmd_scst, off);

out_free:
	kfree(buffer);

out:
	TRACE_EXIT();
	return;
}


inm_32_t 
emd_register_virtual_dev_driver(emd_dev_type_t *dev_type)
{
	handler_dev_type = dev_type;

	return 0;
}
EXPORT_SYMBOL(emd_register_virtual_dev_driver);

inm_32_t 
emd_unregister_virtual_dev_driver(emd_dev_type_t *dev_type)
{
	return 0;
}
EXPORT_SYMBOL(emd_unregister_virtual_dev_driver);

inm_32_t emd_register_virtual_device(char *name)
{
	emd_dev_t *emd_dev = NULL;

	emd_dev = kmalloc(sizeof(emd_dev_t), GFP_KERNEL);
	if (emd_dev == NULL) {
		return -ENOMEM;
	}

	if (strcpy_s(emd_dev->dev_name, 256, name)) {
		kfree(emd_dev);
		return -INM_EFAULT;
	}

	list_add_tail(&emd_dev->dev_list, &emd_dev_list);
	emd_dev->dev_id = scst_register_virtual_device(&emulated_disk, name);
	if (emd_dev->dev_id < 0) {
		list_del(&emd_dev->dev_list);
		kfree(emd_dev);
		goto out;
	}
    
out:
	return emd_dev->dev_id;
}
EXPORT_SYMBOL(emd_register_virtual_device);


inm_32_t emd_unregister_virtual_device(inm_32_t dev_id)
{
	emd_dev_t 		*emd_dev = NULL;
	struct list_head	*ptr = NULL;
	inm_32_t 			ret = 0;

	list_for_each(ptr, &emd_dev_list) {
		emd_dev = list_entry(ptr, emd_dev_t, dev_list);
		if (emd_dev->dev_id == dev_id) {
			ret = 0;
			break;
		}
	}

	scst_unregister_virtual_device(dev_id);
	list_del(&emd_dev->dev_list);
	kfree(emd_dev);

	return emd_dev->dev_id;
}
EXPORT_SYMBOL(emd_unregister_virtual_device);


module_init(init_emd);
module_exit(exit_emd);

MODULE_AUTHOR("InMage Systems Pvt Ltd");
MODULE_DESCRIPTION("InMage user tap driver");
MODULE_LICENSE("GPL");
MODULE_VERSION(__DATE__ " [ " __TIME__ " ]");

