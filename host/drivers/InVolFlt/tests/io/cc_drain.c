#include <string.h>
#include <stdlib.h>
#include "libinclude.h"
#include "liblogger.h"
#include "involflt.h"
#include "svdparse.h"
#include "cc.h"

unsigned long g_nr_disk = 0;
disk_list g_disk_listp[MAX_DISK];
char *base_dir = BASE_DIR;

pthread_mutex_t mutex;
int g_disk_list_index = 0;
int g_done_with_replication_thread = 0;

int SUCCESS = 0;
int FAILED = 1;

void
kill_threads(int signo)
{
    printf("Killing all threads");
    if (signo == SIGTERM)
        g_done_with_replication_thread = 1;
}

int get_disk_list_index(void) {

    int ret = 0;

    pthread_mutex_lock(&mutex);
    if (g_disk_list_index < g_nr_disk) {
        ret = g_disk_list_index;
        ++g_disk_list_index;
    } else {
        err("no disk to process\n");
        ret = -1;
    }
    pthread_mutex_unlock(&mutex);

    return ret;
}

int dev_size(inm_dev_info_t *dev_infop)
{
    int ret = 0;
    unsigned long long size = 0;
    unsigned long nrsectors;
    int fd;

    fd  = open(&dev_infop->d_guid[0], O_RDONLY);
    if(-1 == fd){
        err("Unable to open %s", &dev_infop->d_guid[0]);
        return -1;
    }

    if (ioctl(fd, BLKGETSIZE, &nrsectors))
        nrsectors = 0;

    if (ioctl(fd, BLKGETSIZE64, &size))
        size = 0;

    if(size == 0 || size == nrsectors)
        size = ((unsigned long long) nrsectors) * 512;

    close(fd);

    dev_infop->d_nblks = size >> 9;
    dev_infop->d_bsize = 512;

    dbg("size = %llu", size);

    return ret;
}

void
print_buf(char *bufp , int len)
{
    int i=0;

    dbg("\n****TAG START***************\n");
    for(i=0; i<len; i++) {
        dbg("%c", bufp[i]);
    }
    dbg("\n****TAG END****************\n");
}

int
process_tags_v2(UDIRTY_BLOCK_V2 *udbp, char *tag_name)
{
	PSTREAM_REC_HDR_4B tgp = NULL;
	char tag_guid[256] = { 0 };
	int tg_len;
	int ret = 0;


	tgp = &udbp->uTagList.TagList.TagEndOfList;
	while (tgp->usStreamRecType == STREAM_REC_TYPE_USER_DEFINED_TAG) {

		unsigned int tg_hdr_len = 0;
		if (tgp->ucFlags & STREAM_REC_FLAGS_LENGTH_BIT == STREAM_REC_FLAGS_LENGTH_BIT)
			tg_hdr_len = sizeof(STREAM_REC_HDR_8B);
		else
			tg_hdr_len = sizeof(STREAM_REC_HDR_4B);

		tg_len = *(unsigned short *)((char*)tgp + tg_hdr_len);

		char *tg_ptr = (char *)((char *)tgp + tg_hdr_len + sizeof(unsigned short));

		PSTREAM_REC_HDR_4B tag_guid_hdr_p = (PSTREAM_REC_HDR_4B)tg_ptr;
		unsigned int tg_guid_hdr_len = 0;
		if ((tag_guid_hdr_p->ucFlags & STREAM_REC_FLAGS_LENGTH_BIT) == STREAM_REC_FLAGS_LENGTH_BIT)
		{
			tg_guid_hdr_len = sizeof(STREAM_REC_HDR_8B);
			PSTREAM_REC_HDR_8B tag_guid_hdr_p = (PSTREAM_REC_HDR_8B)tg_ptr;
			memcpy(tag_guid,  (char*)tag_guid_hdr_p + tg_guid_hdr_len, tag_guid_hdr_p->ulLength - tg_guid_hdr_len);
			tg_ptr = (char *)((char *)tg_ptr + tag_guid_hdr_p->ulLength);
			dbg("tag guid: len %d - %s\n", 
                tag_guid_hdr_p->ulLength - tg_guid_hdr_len, tag_guid);
		}
		else
		{
			tg_guid_hdr_len = sizeof(STREAM_REC_HDR_4B);
			PSTREAM_REC_HDR_4B tag_guid_hdr_p = (PSTREAM_REC_HDR_4B)tg_ptr;
			memcpy(tag_guid,  (char*)tag_guid_hdr_p + tg_guid_hdr_len, tag_guid_hdr_p->ucLength - tg_guid_hdr_len);
			tg_ptr = (char *)((char *)tg_ptr + tag_guid_hdr_p->ucLength);
			dbg("tag guid: len %d - %s\n", tag_guid_hdr_p->ucLength - tg_guid_hdr_len, tag_guid);
		}

		

		PSTREAM_REC_HDR_4B tag_name_hdr_p = (PSTREAM_REC_HDR_4B)tg_ptr;
		unsigned int tg_name_hdr_len = 0;
		if ((tag_name_hdr_p->ucFlags & STREAM_REC_FLAGS_LENGTH_BIT) == STREAM_REC_FLAGS_LENGTH_BIT)
		{
			tg_name_hdr_len = sizeof(STREAM_REC_HDR_8B);
			PSTREAM_REC_HDR_8B tag_name_hdr_p = (PSTREAM_REC_HDR_8B)tg_ptr;
			memcpy(tag_name, (char*)tag_name_hdr_p + tg_name_hdr_len, tag_name_hdr_p->ulLength - tg_name_hdr_len);
			tg_ptr = (char *)((char *)tg_ptr + tag_name_hdr_p->ulLength);
			dbg("tag name: len %d - %s\n", tag_name_hdr_p->ulLength - tg_name_hdr_len, tag_name);
					}
		else
		{
			tg_name_hdr_len = sizeof(STREAM_REC_HDR_4B);
			PSTREAM_REC_HDR_4B tag_name_hdr_p = (PSTREAM_REC_HDR_4B)tg_ptr;
			memcpy(tag_name, (char*)tag_name_hdr_p + tg_name_hdr_len, tag_name_hdr_p->ucLength - tg_name_hdr_len);
			tg_ptr = (char *)((char *)tg_ptr + tag_name_hdr_p->ucLength);
			dbg("tag name: len %d - %s\n", tag_name_hdr_p->ucLength - tg_name_hdr_len, tag_name);
		}

        ret = 1;	
        break;

		tgp = (PSTREAM_REC_HDR_4B)((char *)tgp +
			GET_STREAM_LENGTH(tgp));
		
	}
	return ret;
}


int
open_ctrl_dev(void)
{
    int ret = 0;
    int fd = open(INM_CTRL_DEV, O_RDWR);

    if (fd > 0)
        ret=fd;

    return fd;
} 


int
inm_validate_prefix(inm_addr_t addr, int tag)
{
    SVD_PREFIX *svd_prefixp = (SVD_PREFIX *) addr;
    int ret = -1;

    if (svd_prefixp->tag != tag) {
        err("Expected tag = %d\n", tag);
        err("tag %d\n", svd_prefixp->tag);
        err("flags %d\n", svd_prefixp->Flags);
        err("count %d\n", svd_prefixp->count);
    } else {
        ret = 0;
    }

    return ret;
} /* end of inm_validate_prefix() */

/*
 *  * inm_read_data : reads the data from the descriptor
 *   */
int
inm_read_data(int fd, int len, void *datap)
{
    int nr_bytes = 0;
    int ret = -1;

    nr_bytes = read(fd, (char *)datap, len);
    if (nr_bytes == len)
        ret = 0;
    else
        ret = -1;

    return ret;
} /* end of inm_read_data() */

/* applying changes on target device */

int
inm_apply_dblk(int tgt_fd, SVD_DIRTY_BLOCK_V2 *dblkp, void *datap)
{
    int nr_bytes = 0, ret = -1;

    /*
    if (lseek(tgt_fd, dblkp->ByteOffset, SEEK_SET) == -1) {
        printf("err : failed in lseek \n");
    }
    */

    if (dblkp->Length == write(tgt_fd, datap, dblkp->Length))
        ret = 0;
    else
        err("write failed \n");

    return ret;
}

/*
 *  * inm_read_prefix fn
 *   * @ tag : expected tag value
 *    **/
int
inm_read_prefix(int fd, int tag)
{
    SVD_PREFIX prefix;
    int nr_bytes = 0;
    int ret = -1;

    nr_bytes = read(fd, &prefix, sizeof(SVD_PREFIX));
    if (nr_bytes == sizeof(SVD_PREFIX)) {
        if (prefix.tag != tag) {
            err("expectd tag %d, tag recvd %d\n", tag, prefix.tag);
            ret = -1;
        } else {
            ret = 0;
        }
    } else {
        ret = -1;
        err("failed to read prefix\n");
    }

    return ret;
} /* end of inm_read_prefix() */

/* processing data file */
int
process_dfm(char *src_dir_name, UDIRTY_BLOCK_V2 *udbp, int disk_index)
{
    int ret = -1, df_fd;
    SVD_HEADER1 svd_hdr;
    SVD_TIME_STAMP_V2 svd_tofc, svd_tolc;
    void *datap = NULL;
    long long nr_drtd_chgs = 0;
    char *dfnamep = NULL;
    SVD_DIRTY_BLOCK_V2 svd_dblk;
    char *buf;
    char log_file_name[MAX_PATH_LEN];
    int tgt_fd;
    uint64_t logfile_seqno = 0;
    uint64_t s_time_in_nsec = 0;
    uint64_t e_time_in_nsec = 0;

    pthread_mutex_lock(&mutex);
    logfile_seqno = g_disk_listp[disk_index].log_file_seqno;
    if (!logfile_seqno) {
        logfile_seqno = g_disk_listp[disk_index].log_file_seqno = 1;
    } else {
        ++logfile_seqno;
        g_disk_listp[disk_index].log_file_seqno = logfile_seqno;
    }
    pthread_mutex_unlock(&mutex);
    
    /*create log file*/
    s_time_in_nsec = udbp->uTagList.TagList.TagTimeStampOfFirstChange. \
                     TimeInHundNanoSecondsFromJan1601;

    e_time_in_nsec = udbp->uTagList.TagList.TagTimeStampOfLastChange. \
                     TimeInHundNanoSecondsFromJan1601;

    log_file_name[0] = '\0';
    snprintf(log_file_name, MAX_PATH_LEN, "%s%s%lu%s%lu%s%lu", 
             src_dir_name, "/log_file_", s_time_in_nsec, "_", 
             e_time_in_nsec, "_", logfile_seqno);
    
    dbg("Creating %s", log_file_name);
    tgt_fd = open(log_file_name, O_WRONLY|O_CREAT, 0640);
    if (tgt_fd <0) {
        err("failed to open %s", log_file_name);
        return -1;
    }

    dfnamep = (char *)udbp->uTagList.DataFile.FileName;
    df_fd = open(dfnamep, O_RDONLY);
    if (!df_fd) {
        err("failed to open data file %s", dfnamep);
        goto exit;
    }

    /* skip first 4 bytes (endian) */
    if (lseek(df_fd, 4, SEEK_SET) < 0) {
        err("lseek failed on data file");
        goto exit;
    }

    /* svd header */
    if (inm_read_prefix(df_fd, SVD_TAG_HEADER1) < 0) {
        err("failed for SVD_TAG_HEADER1");
        goto exit;
    }

    if (inm_read_data(df_fd, sizeof(SVD_HEADER1), &svd_hdr) < 0) {
        err("failed to read SVD_HEADER");
        goto exit;
    }

    /* tofc */
    if (inm_read_prefix(df_fd, 
                        SVD_TAG_TIME_STAMP_OF_FIRST_CHANGE_V2) < 0) {
        err("failed for SVD_TAG_TIME_STAMP_OF_FIRST_CHANGE_V2");
        goto exit;
    }

    if (inm_read_data(df_fd, sizeof(SVD_TIME_STAMP_V2), &svd_tofc)<0) {
        err("failed to read SVD_TIME_STAMP_V2");
        goto exit;
    }

    /* lodc */
    if (inm_read_prefix(df_fd, SVD_TAG_LENGTH_OF_DRTD_CHANGES) < 0) {
        err("failed for SVD_LENGTH_OF_DRTD_CHANGES");
        goto exit;
    }

    if (inm_read_data(df_fd, sizeof(long long), &nr_drtd_chgs) < 0) {
        err("failed to read # of drtd chgs");
        goto exit;
    }

    /* read dirty changes */
    while (nr_drtd_chgs > 0) {
        if (inm_read_prefix(df_fd, SVD_TAG_DIRTY_BLOCK_DATA_V2) < 0) {
            err("failed for SVD_TAG_DIRTY_BLOCK_DATA_V2");
            break;
        }

        if (inm_read_data(df_fd, sizeof(SVD_DIRTY_BLOCK_V2), &svd_dblk)<0){
            err("failed to read SVD_DIRTY_BLOCK");
            break;
        }

        datap = valloc(svd_dblk.Length);
        if (!datap) {
            err("malloc failed");
            break;
        }

#ifdef DEBUG_LOG
        printf("SVD DB (F)--@ off = %15lld, sz = %5u, td = %u, sd = %u\n", 
               svd_dblk.ByteOffset, svd_dblk.Length, svd_dblk.uiTimeDelta, 
               svd_dblk.uiSequenceNumberDelta);
#endif

        if (inm_read_data(df_fd, svd_dblk.Length, datap) < 0) {
            err("read data failed\n");
            free(datap);
            break;
        }

        /*
         * TODO: This is incorrect. inm_apply_blk will seek
         * to offset in log file and then apply
         */
        if (inm_apply_dblk(tgt_fd, &svd_dblk, datap) < 0) {
            err("failed to apply svd_dblk\n");
            free(datap);
            break;
        }

        nr_drtd_chgs -= (sizeof(SVD_PREFIX) + sizeof(SVD_DIRTY_BLOCK_V2) +
                        svd_dblk.Length);
        free(datap);
    }

    if (inm_read_prefix(df_fd, SVD_TAG_TIME_STAMP_OF_LAST_CHANGE_V2) < 0) {
        err("failed for SVD_TAG_TIME_STAMP_OF_LAST_CHANGE_V2");
        goto exit;
    }

    if (inm_read_data(df_fd, sizeof(SVD_TIME_STAMP_V2), &svd_tolc)<0) {
        err("failed to read SVD_TIME_STAMP_V2");
        goto exit;
    }

    ret = 0;

exit:
    if (df_fd != -1) {
        close(df_fd);
    }
    if (tgt_fd != -1) {
        close(tgt_fd);
    }

    return ret;
}

/* processing data mode stream */

int
process_dm_stream(char *src_dir_name, UDIRTY_BLOCK_V2 *udbp, int disk_index)
{
    uint64_t *len_chgsp, nr_chgs_in_strm = 0;
    int ret = -1, chg_idx = 0;
    SVD_PREFIX *svd_prefixp = NULL;
    SVD_TIME_STAMP_V2 *svd_tsp = NULL;
    uint64_t strm_len = 0;
    inm_addr_t addrp, start_addrp;
    char *buf;
    char log_file_name[MAX_PATH_LEN];
    int tgt_fd;
    uint64_t logfile_seqno = 0;
    uint64_t s_time_in_nsec = 0;
    uint64_t e_time_in_nsec = 0;

    /*
     * TODO: Is this lock required ... 
     * we are single threaded per dev
     */
    pthread_mutex_lock(&mutex);
    logfile_seqno = g_disk_listp[disk_index].log_file_seqno;
    if (!logfile_seqno) {
        logfile_seqno = g_disk_listp[disk_index].log_file_seqno = 1;
    } else {
        ++logfile_seqno;
        g_disk_listp[disk_index].log_file_seqno = logfile_seqno;
    }
    pthread_mutex_unlock(&mutex);

    /*create log file*/
    s_time_in_nsec = udbp->uTagList.TagList.TagTimeStampOfFirstChange. \
                     TimeInHundNanoSecondsFromJan1601;

    e_time_in_nsec = udbp->uTagList.TagList.TagTimeStampOfLastChange. \
                     TimeInHundNanoSecondsFromJan1601;

    log_file_name[0] = '\0';
    snprintf(log_file_name, MAX_PATH_LEN, "%s%s%lu%s%lu%s%lu", 
             src_dir_name, "/log_file_", s_time_in_nsec, "_", 
             e_time_in_nsec, "_", logfile_seqno);

    dbg("Creating %s", log_file_name);
    tgt_fd = open(log_file_name, O_WRONLY|O_CREAT, 0640);
    if (tgt_fd < 0) {
        err("Failed to create %s", log_file_name);
        return -1;
    }

    addrp = (inm_addr_t) udbp->uHdr.Hdr.ppBufferArray;
    start_addrp = addrp;
    
    /* skip first 4 bytes (endian) */
    addrp = (inm_addr_t)(((char *) start_addrp + sizeof(uint32_t)));

    if (inm_validate_prefix(addrp, (int)SVD_TAG_HEADER1)) {
        err("Failed to validate SVD_TAG_HEADER1");
        goto exit;
    }
    addrp += (sizeof(SVD_PREFIX) + sizeof(SVD_HEADER1));

    if (inm_validate_prefix(addrp, SVD_TAG_TIME_STAMP_OF_FIRST_CHANGE_V2)) {
        err("Failed to validate SVD_TAG_TIME_STAMP_OF_FIRST_CHANGE_V2");
        goto exit;
    }
    addrp += sizeof(SVD_PREFIX);


    svd_tsp = (SVD_TIME_STAMP_V2 *) addrp;
    addrp += sizeof(SVD_TIME_STAMP_V2);
    svd_prefixp = (SVD_PREFIX *) addrp;

    if (inm_validate_prefix(addrp, SVD_TAG_LENGTH_OF_DRTD_CHANGES)) {
        err("failed to validate SVD_TAG_LENGTH_OF_DRTD_CHANGES");
        goto exit;
    }

    addrp += sizeof(SVD_PREFIX);
    len_chgsp = (uint64_t *)addrp;
    addrp += sizeof(uint64_t);

    while (chg_idx < udbp->uHdr.Hdr.cChanges) {
        SVD_DIRTY_BLOCK_V2 *svd_dblkp = NULL;

        if (inm_validate_prefix(addrp, SVD_TAG_DIRTY_BLOCK_DATA_V2) < 0) {
                err("validation failed for SVD_TAG_DIRTY_BLOCK_DATA_V2");
                goto exit;
        }

        addrp += sizeof(SVD_PREFIX);
        svd_dblkp = (SVD_DIRTY_BLOCK_V2 *) addrp;

        if (0) {
            dbg("SVD DB (S)-->");
            dbg("off = %15lu, sz = %5u, td = %u, sd = %u",
                svd_dblkp->ByteOffset,
                svd_dblkp->Length,
                svd_dblkp->uiTimeDelta,
                svd_dblkp->uiSequenceNumberDelta);
        }

        addrp += sizeof(SVD_DIRTY_BLOCK_V2);

        buf = valloc((size_t)svd_dblkp->Length);
        if(!buf){
            err("Failed to allocate the aligned buffer\n");
            goto exit;
        }

        memcpy(buf, (void *)addrp, svd_dblkp->Length);

        //now write this change to a new log file
        if (write(tgt_fd, buf, (size_t)svd_dblkp->Length) < 0) {
            err("Write failed for off = %lu\n", svd_dblkp->ByteOffset);
            free(buf);
            goto exit;
        }

        free(buf);

        addrp += svd_dblkp->Length;
        nr_chgs_in_strm += svd_dblkp->Length;
        chg_idx++;
    }

    dbg("Num chgs in cur strm = %12lu\n", nr_chgs_in_strm);

    svd_prefixp = (SVD_PREFIX *) addrp;
    if (inm_validate_prefix(addrp, SVD_TAG_TIME_STAMP_OF_LAST_CHANGE_V2)) {
        err("faild to validate tolc\n");
        goto exit;
    }

    addrp += sizeof(SVD_PREFIX);
    svd_tsp = (SVD_TIME_STAMP_V2 *) addrp;

    if (0) {
        dbg("TOLC ");
        dbg("t = %10llu ", (unsigned long long)
            svd_tsp->TimeInHundNanoSecondsFromJan1601);
        dbg("seqno = %10llu \n", (unsigned long long)
            svd_tsp->ullSequenceNumber);
    }
    
    addrp += sizeof(SVD_TIME_STAMP_V2);
    
    strm_len = (addrp - start_addrp);
    if (strm_len != udbp->uHdr.Hdr.ulcbChangesInStream) {
        dbg("Strm len doesn't match with parse len %d, %ld", 
            udbp->uHdr.Hdr.ulcbChangesInStream, strm_len);
        goto exit;
    }

    ret = 0;

exit:
    if (tgt_fd != -1) {
        close(tgt_fd);
    }

    return ret;
}

/* validates the time stamp in udirty block */
int
inm_validate_ts(UDIRTY_BLOCK_V2 *udbp, int disk_index)
{
    uint64_t last_ts = 0;
    uint64_t last_seqno = 0;
    uint64_t s_time_in_nsec = 0, s_seqno = 0;
    uint64_t e_time_in_nsec = 0, e_seqno = 0;
    int ret = -1;

    //get last time stamp and seq. no. of this disk
    pthread_mutex_lock(&mutex);
    last_ts = g_disk_listp[disk_index].last_ts;
    last_seqno = g_disk_listp[disk_index].last_seqno;
    pthread_mutex_unlock(&mutex);

    s_time_in_nsec = udbp->uTagList.TagList.TagTimeStampOfFirstChange. \
                     TimeInHundNanoSecondsFromJan1601;

    e_time_in_nsec = udbp->uTagList.TagList.TagTimeStampOfLastChange. \
                     TimeInHundNanoSecondsFromJan1601;

    s_seqno = udbp->uTagList.TagList.TagTimeStampOfFirstChange. \
              ullSequenceNumber;

    e_seqno = udbp->uTagList.TagList.TagTimeStampOfLastChange. \
              ullSequenceNumber;

    if (last_ts) {
        if (last_seqno > s_seqno || s_seqno > e_seqno) {
            err("Seq # mismatch : \n"
                "last_seqno = %llu\n"
                "s_seqno = %llu\n"
                "e_seqno = %llu \n",
                (unsigned long long) last_seqno,
                (unsigned long long) s_seqno,
                (unsigned long long) e_seqno);

            ret = -1;
            assert(0);
            goto exit;
        }

        if (s_time_in_nsec < last_ts) {
            err("Time mismatch : tofc (%llu) < \
                last_ts (%llu)\n",
                (unsigned long long) s_time_in_nsec,
                (unsigned long long) last_ts);

            ret = -1;
            assert(0);
            goto exit;
        }

        if (e_time_in_nsec < s_time_in_nsec) {
            err("tofc (%llu) > tolc(%llu)\n",
                (unsigned long long) s_time_in_nsec,
                (unsigned long long) e_time_in_nsec);

            ret = -1;
            assert(0);
            goto exit;
        }
    }

    pthread_mutex_lock(&mutex);
    g_disk_listp[disk_index].last_ts = e_time_in_nsec;
    g_disk_listp[disk_index].last_seqno = e_seqno;
    pthread_mutex_unlock(&mutex);
    ret = 0;

exit:
        return ret;
}



int start_flt_replicate(char *src_disk, char *pname, int disk_index) {

    UDIRTY_BLOCK_V2 *udbp = NULL;
    COMMIT_TRANSACTION commit;
    int ret = -1;
    WAIT_FOR_DB_NOTIFY wait_db;
    inm_dev_info_t dev_info;
    int dev_fd;
    char src_dir_name[MAX_PATH_LEN];
    char src_dir_cmd[MAX_PATH_LEN];
    char log_file_name[MAX_PATH_LEN];//to create dummy file with name tag
    int log_fd;
    char inm_start_flt_disk_cmd[MAX_PATH_LEN];
    uint64_t dir_seq_no = 0;
    int first_iteration = 0;
    char tag_name[256] = {0};
 
    /*
     * first start filtering for the device so that bitmap 
     * file can created by driver for each source device
     */
    dbg("Stacking Disk");
    snprintf(inm_start_flt_disk_cmd, MAX_PATH_LEN, 
            "%s --op=start_flt --src_vol=%s",DEFAULT_INM_PATH,src_disk);
    dbg("CMD: %s", inm_start_flt_disk_cmd);

    pthread_mutex_lock(&mutex);
    system(inm_start_flt_disk_cmd);
    pthread_mutex_unlock(&mutex);

    /*
     * now open driver for each source disk context, because the same handle
     * will be use by start filtering and get db IOCTL call. start filtering
     * ioctl will save the target context inside struct file private_data
     * of this thread and get db will get context from the same private_data.
     */
    dbg("opening driver to replicate disk [%s]\n",src_disk);
    dev_fd = open(INM_CTRL_DEV, O_RDWR);
    if (dev_fd < 0) {
        err("Failed to open ctrl dev");
        return -1;
    }

    /*
     * you have already done with start_flt but still needed to do one more time, because
     * get_db requires private_data field of struct file structure which we have registered
     * with the driver for this thread in above open call but this private_data
     * field will get populated by driver only during start filtering ioctl call. note
     * start filtring  will polulate the private_data of struct file for this thread
     * with this source volume context and when get_db will get called with this
     * struct file driver will get the context of this source device from private_data
     * field of this thread struct file.
     */

    dev_info.d_mnt_pt[0] = '\0';
    dev_info.d_type = FILTER_DEV_HOST_VOLUME;
    strcpy(&dev_info.d_guid[0], src_disk);
    strcpy(&dev_info.d_pname[0], pname);
    if(dev_size(&dev_info)){
        goto exit;
    }

    dbg("Stacking disk %s, %s", src_disk, pname);
    dev_info.d_flags = 0;
    if (ioctl(dev_fd, IOCTL_INMAGE_START_FILTERING_DEVICE_V2,
              &dev_info) == -1) {
        dbg("IOCTL_INMAGE_START_FILTERING_DEVICE_V2 ioctl failed\n");
        ret = -1;
        goto exit;
    }

    udbp = (UDIRTY_BLOCK_V2 *)malloc(sizeof(*udbp));
    if (!udbp) {
        err("malloc failed for udbp\n");
        ret = -1;
        goto exit;
    }
    memcpy(wait_db.VolumeGUID, src_disk, GUID_SIZE_IN_CHARS);
    wait_db.Seconds = 65;

start_store_logfile_new_dir:
    //each thread will have their own dir_seq_no starting from 1
    ++dir_seq_no;

    src_dir_name[0] = '\0';
    src_dir_cmd[0] = '\0';
    
    snprintf(src_dir_name, MAX_PATH_LEN, "%s%s%s%lu", 
             base_dir, src_disk, DIR_LOG, dir_seq_no);
    dbg("log directory name is %s\n", src_dir_name);
    
    //create directory to store log files
    dbg("Create log directory"); 
    snprintf(src_dir_cmd, MAX_PATH_LEN, "mkdir -p %s", src_dir_name);
    dbg("CMD: %s",src_dir_cmd);

    //system command is not thread safe, so take lock
    pthread_mutex_lock(&mutex);
    system(src_dir_cmd);
    pthread_mutex_unlock(&mutex);
    if (!first_iteration) {
        first_iteration = 1;
        goto get_db;
    }

wait_on:
    //call waitdb IOCTL
    dbg("Waiting on %s", src_disk);
    ioctl(dev_fd, IOCTL_INMAGE_WAIT_FOR_DB, &wait_db);

get_db:
    memset(udbp, 0, sizeof(*udbp));

    //call getdb IOCTL
    if (ioctl(dev_fd, IOCTL_INMAGE_GET_DIRTY_BLOCKS_TRANS, udbp) < 0) {
        dbg("Stream_len = %u, errno = %d\n", 
            udbp->uHdr.Hdr.ulcbChangesInStream, errno);
        goto wait_on;
    }

    if (!(udbp->uHdr.Hdr.ulFlags & UDIRTY_BLOCK_FLAG_DATA_FILE) &&
        (inm_validate_ts(udbp, disk_index) < 0)) {
        err("TS validation has failed\n");
        ret = -1;
        goto exit;
    }

    if (udbp->uHdr.Hdr.ulFlags & UDIRTY_BLOCK_FLAG_DATA_FILE) {
        dbg("Data File name = %s\n", udbp->uTagList.DataFile.FileName);
        if (process_dfm(src_dir_name, udbp, disk_index) < 0) {
            ret = -1;
            goto exit;
        }
    } else if (udbp->uHdr.Hdr.ulFlags & UDIRTY_BLOCK_FLAG_SVD_STREAM) {
        if (process_dm_stream(src_dir_name, udbp, disk_index) < 0) {
            ret = -1;
            goto exit;
        }
    } else {
        dbg("Data Source = %s mode \n",
            (udbp->uTagList.TagList.TagDataSource.ulDataSource ==
            INVOLFLT_DATA_SOURCE_META_DATA) ? "METADATA" :
            "BITMAP");
        ret = process_tags_v2(udbp, tag_name);
        dbg("The value return for tag is ret=[%d]\n",ret);
    }

commit:
    memset(&commit, 0, sizeof(commit));
    
    if (udbp->uHdr.Hdr.ulFlags | UDIRTY_BLOCK_FLAG_VOLUME_RESYNC_REQUIRED)
        commit.ulFlags |= COMMIT_TRANSACTION_FLAG_RESET_RESYNC_REQUIRED_FLAG;
    
    commit.ulTransactionID = udbp->uHdr.Hdr.uliTransactionID;
    
    if (ioctl(dev_fd, IOCTL_INMAGE_COMMIT_DIRTY_BLOCKS_TRANS, &commit) == -1) {
        err("Commit Fail\n");
        goto exit;
    }

    //rerun once you get the tag
    if (ret == 1) {
        dbg("Tag issued successfully!\n");
        ret = 0;
        sleep(1);

        //create a file with name same as tag name received in tag buffer.
        //and this file act as marker.
        dbg("Received tag file name %s", tag_name);
        
        log_file_name[0] = '\0';
        snprintf(log_file_name, MAX_PATH_LEN, "%s%s%s", 
                 src_dir_name,"/", tag_name);
        
        log_fd = open(log_file_name, O_WRONLY|O_CREAT, 0640);
        if (log_fd <0) {
                err("Failed to create log file %s", log_file_name);
                return -1;
        }
        close(log_fd);

        snprintf(log_file_name, MAX_PATH_LEN, "%s%s/%s", 
                 base_dir, src_disk, tag_name);

        /* Rename to tag name for verification */
        dbg("Renaming %s -> %s", src_dir_name, log_file_name);
        rename(src_dir_name, log_file_name);

        dbg("New Dir");
        goto start_store_logfile_new_dir;
    }

    if (g_done_with_replication_thread) {
        ret = 0;
        goto exit;
    }

    ret = 0;
    if (udbp->uHdr.Hdr.ulTotalChangesPending > DEFAULT_DB_NOTIFY_THRESHOLD)
        goto get_db;
    else
        goto wait_on;

exit:
    close(dev_fd);
    
    if (udbp) {
        free(udbp);
    }

    return ret;
}

void *repl_op(void *threadid) 
{
    int disk_index;
    int ret = 0;

    disk_index = get_disk_list_index();
    dbg("Processing disk %s %s\n", g_disk_listp[disk_index].disk_name, g_disk_listp[disk_index].disk_pname);
    
    sleep(1);
    
    //now each thread will process one disk which it owns
    ret = start_flt_replicate(g_disk_listp[disk_index].disk_name, g_disk_listp[disk_index].disk_pname, disk_index);
    if (ret) {
        err("Replication error");
        exit(-1);
    }
}

int launch_replication_thread()
{
    int ret, t;
    pthread_t *threads = NULL;
    pthread_attr_t attr;

    threads = (pthread_t*) calloc((g_nr_disk), sizeof(pthread_t));
    if (!threads) {
        err("failed to allocate memory\n");
        exit(-1);
    }

    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

    pthread_mutex_init(&mutex, NULL);

    for (t = 0; t < g_nr_disk; t++) {
        dbg("Creating replication thread %d", t+1);
        ret = pthread_create(threads + t, &attr, repl_op, (void*)&t);
        if (ret) {
            err("Failed to create replication thread");
            exit(-1);
        }
    }

    for (t = 0; t < g_nr_disk; t++) {
        ret = pthread_join(threads[t], NULL);
        dbg("Replication thread number %d is dead", t);
    }

    sleep(5);
    
    pthread_attr_destroy(&attr);
    pthread_mutex_destroy(&mutex);
    if (threads) {
        free(threads);
    }

    return ret;
}

void *
start_service(void *thread)
{
    int ret;
    int dev_fd;
    SHUTDOWN_NOTIFY_INPUT stop_notify;
    unsigned long div = 0, rem = 0;

    dev_fd = open(INM_CTRL_DEV, O_RDWR);
    if (dev_fd < 0) {
        err("Failed to open ctrl dev");
        ret = -1;
        return &FAILED;
    }

    stop_notify.ulFlags = 0;
    dbg("Issuing service shutdown notifier ioctl");
    if (ioctl(dev_fd, IOCTL_INMAGE_SERVICE_SHUTDOWN_NOTIFY,
              &stop_notify) < 0) {
        err("IOCTL_INMAGE_SERVICE_SHUTDOWN_NOTIFY ioctl failed.\n");
        ret = -1;
        goto exit;
    }

    while (1) {
        sleep(60);
        if (g_done_with_replication_thread)
            break;
    }
   
exit: 
    close(dev_fd);

out:
    return &SUCCESS;
}

void launch_service_start_thread()
{
    int ret;
    int t = 0;

    if (signal(SIGINT, kill_threads) == SIG_ERR)
        exit(-1);

    pthread_t service_thread_id;
    pthread_attr_t attr;

    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    ret = pthread_create(&service_thread_id, &attr, start_service, (void*)&t);
    if (ret) {
        err("Failed to create service start thread");
        exit(-1);
    }
}

void stop_filtering_list_of_devices(void)
{
    int numdisk;
    char inm_stop_flt_disk_cmd[MAX_PATH_LEN];

    for (numdisk = 0; numdisk < g_nr_disk; numdisk++) {
        dbg("Stop filtering %s", g_disk_listp[numdisk].disk_name);
        memset(inm_stop_flt_disk_cmd, '\0', MAX_PATH_LEN);
        snprintf(inm_stop_flt_disk_cmd, MAX_PATH_LEN, 
                 "%s --op=stop_flt --src_vol=%s", DEFAULT_INM_PATH,
                 g_disk_listp[numdisk].disk_name);
        dbg("CMD: %s", inm_stop_flt_disk_cmd);
        system(inm_stop_flt_disk_cmd);
    }
}

void
usage(int exitval)
{
    log_err_exit("Usage:"
                 "\n\t -b <dir> - base dir for test run"
                 "\n\t -d <disk> - disk path"
                 "\n\t -h - Help"
                 "\n\t -l - Enable debug mode"
                 "\n\t ex: cc_drain -b /var/log/wal/test_crash_tag/ -d /dev/sdc -d /dev/sdd -d /dev/sdf"
                 "\n");
}

void
get_pname(char *disk_name, char *disk_pname)
{
    int i = 0;
    while(disk_name[i]!='\0')
    {
        if(disk_name[i]=='/')
        {
            disk_pname[i]='_';
        }
        else
        {
            disk_pname[i]=disk_name[i];
        }
        i++;
    }
    disk_pname[i]='\0';
}

int
main(int argc, char *argv[])
{
    int ret = -1;
    int optindx;
    char *disk_name;
    char cmd_buf[MAX_PATH_LEN];
    char disk_pname[MAX_DISK][MAX_PATH_LEN];

    while((optindx = getopt(argc, argv, "b:d:hl")) != -1) {

        switch (optindx) {
            case 'b':
                base_dir = optarg;
                break;

            case 'd':
                if (g_nr_disk < MAX_DISK) {
                    g_disk_listp[g_nr_disk].disk_name = optarg;
                    g_disk_listp[g_nr_disk].disk_pname = disk_pname[g_nr_disk];
                    get_pname(g_disk_listp[g_nr_disk].disk_name, g_disk_listp[g_nr_disk].disk_pname);
                    g_nr_disk++;
                }
                else
                    log_err_usage("Max %d disks supported", MAX_DISK);
                break;

            case 'h':
                usage(0);
                break;

            case 'l':
                debug = 1;
                break;

            default:
                log_err_usage("Invalid Option");
                goto exit;
        }
    }

    if (!g_nr_disk) {
        log_err_usage("No disks provided");
        goto exit;
    }

    cmd_buf[0] = '\0';
    snprintf(cmd_buf, MAX_PATH_LEN, "mkdir -p %s", base_dir);
    system(cmd_buf);

    launch_service_start_thread();
    
    ret = launch_replication_thread();
    if (ret) {
        err("Failed to launch replication threads\n");
        ret = -1;
    }

    stop_filtering_list_of_devices();

exit:
    return ret;
}
