#define _GNU_SOURCE
#define _XOPEN_SOURCE         600// for posix_memalign()

#include <string.h>
#include <stdlib.h>
#include "WalTest.h"
#include "involflt.h"
#include "safecapismajor.h"

extern unsigned long g_nr_disk;
extern unsigned long g_tag_timeout;
extern unsigned long g_service_timeout;
extern unsigned long long g_com_min_disk_offset_all_disk;
extern disk_list *g_disk_listp;

extern pthread_mutex_t verifier_mutex;
extern pthread_cond_t  verifier_cond_wait;
extern unsigned long g_start_verifier_thread;
extern unsigned long g_sec_sz_inbyte;

extern FILE *dbgfd;
extern FILE *wfd;

pthread_mutex_t mutex;
int g_disk_list_index = 0;
unsigned long *g_io_seq_no;
int g_done_with_replication_thread = 0;
int g_tag_ioctl_failed_for_itr = 0;

int get_disk_list_index(void) {

    int ret = 0;

    pthread_mutex_lock(&mutex);
    if (g_disk_list_index < g_nr_disk) {
        ret = g_disk_list_index;
        ++g_disk_list_index;
    } else {
        fprintf(dbgfd,"no disk to process\n");
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

    WAL_TRACE_START();

    fd  = open(&dev_infop->d_guid[0], O_RDONLY);
    if(-1 == fd){
        fprintf(dbgfd,"Unable to open the file %s\n", &dev_infop->d_guid[0]);
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

    fprintf(dbgfd,"size = %llu\n", size);

    WAL_TRACE_END();

    return ret;
}

void
print_buf(char *bufp , int len)
{
    int i=0;

    WAL_TRACE_START();

    fprintf(dbgfd,"\n****TAG START***************\n");
    for(i=0; i<len; i++) {
        fprintf(dbgfd,"%c", bufp[i]);
    }
    fprintf(dbgfd,"\n****TAG END****************\n");

    WAL_TRACE_END();
}

int
process_tags_v2(UDIRTY_BLOCK_V2 *udbp, char *tag_name)
{
    PSTREAM_REC_HDR_4B tgp = NULL;
    //char tg[256] = {0};
    int tg_len;
    int ret = 0;

    WAL_TRACE_START();

    tgp = &udbp->uTagList.TagList.TagEndOfList;
    while (tgp->usStreamRecType == STREAM_REC_TYPE_USER_DEFINED_TAG) {
        tg_len = *(unsigned short *)((char*)tgp +
                sizeof(STREAM_REC_HDR_4B));

        memcpy(tag_name, ((char*)tgp + sizeof(STREAM_REC_HDR_4B) +
                    sizeof(unsigned short)), tg_len);

        //tgp[tg_len] = '\0';
        tgp = (PSTREAM_REC_HDR_4B) ((char *)tgp +
                GET_STREAM_LENGTH(tgp));
        fprintf(dbgfd,"tag received: %s\n", tag_name);
        print_buf(tag_name, tg_len);
        ret = 1;
    }

    WAL_TRACE_END();
    return ret;
}

int
open_ctrl_dev(void)
{
    int ret = 0;
    int fd = open(INM_CTRL_DEV, O_RDWR);

    WAL_TRACE_START();

    if (fd > 0) {
        ret=fd;
    }

    WAL_TRACE_END();

    return fd;
} 


int
inm_validate_prefix(inm_addr_t addr, int tag)
{
    SVD_PREFIX *svd_prefixp = (SVD_PREFIX *) addr;
    int ret = -1;

    //WAL_TRACE_START();

    if (svd_prefixp->tag != tag) {
        fprintf(dbgfd,"Expected tag = %d\n", tag);
        fprintf(dbgfd,"tag %d\n", svd_prefixp->tag);
        fprintf(dbgfd,"flags %d\n", svd_prefixp->Flags);
        fprintf(dbgfd,"count %d\n", svd_prefixp->count);
    } else {
        ret = 0;
    }

    //WAL_TRACE_END();

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

    WAL_TRACE_START();

    nr_bytes = read(fd, (char *)datap, len);
    if (nr_bytes == len) {
        ret = 0;
    } else {
        ret = -1;
    }

    WAL_TRACE_END();

    return ret;
} /* end of inm_read_data() */

/* applying changes on target device */

int
inm_apply_dblk(int tgt_fd, SVD_DIRTY_BLOCK_V2 *dblkp, void *datap)
{
    int nr_bytes = 0, ret = -1;

    WAL_TRACE_START();

    if (lseek(tgt_fd, dblkp->ByteOffset, SEEK_SET) == -1) {
        printf("err : failed in lseek \n");
    }

    if ((nr_bytes = write(tgt_fd, datap, dblkp->Length)) != -1 &&
            (nr_bytes == dblkp->Length)) {
        ret = 0;
    } else {
        printf("err : write failed \n");
    }

    WAL_TRACE_END();

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

    WAL_TRACE_START();;

    nr_bytes = read(fd, &prefix, sizeof(SVD_PREFIX));
    if (nr_bytes == sizeof(SVD_PREFIX)) {
        if (prefix.tag != tag) {
            printf("err : expectd tag %d, tag recvd %d\n",
                    tag, prefix.tag);
            ret = -1;
        } else {
            ret = 0;
        }
    } else {
        ret = -1;
        perror("err : failed to read prefix\n");
    }

    WAL_TRACE_END(); 

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

    WAL_TRACE_START();

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
    snprintf(log_file_name, MAX_PATH_LEN, "%s%s%llu%s%llu%s%lu", src_dir_name,"/log_file_",s_time_in_nsec,"_", e_time_in_nsec,"_",logfile_seqno);
    fprintf(dbgfd,"log file name is [%s]\n", log_file_name);
    /*now open target log file to write this data stream*/
    tgt_fd = open(log_file_name, O_WRONLY|O_CREAT, 0640);
    if (tgt_fd <0) {
        fprintf(dbgfd,"error failed to open target log file [%s]\n",log_file_name);
        return -1;
    }


    dfnamep = (char *)udbp->uTagList.DataFile.FileName;
    df_fd = open(dfnamep, O_RDONLY);
    if (!df_fd) {
        printf("err: failed to open data file %s\n", dfnamep);
        goto exit;
    }

    /* skip first 4 bytes (endian) */
    if (lseek(df_fd, 4, SEEK_SET) < 0) {
        printf("err : lseek failed on data file\n");
        goto exit;
    }
    /* svd header */
    if (inm_read_prefix(df_fd, SVD_TAG_HEADER1) < 0) {
        printf("err : failed for SVD_TAG_HEADER1\n");
        goto exit;
    }

    if (inm_read_data(df_fd, sizeof(SVD_HEADER1), &svd_hdr)<0) {
        printf("err : failed to read SVD_HEADER");
        goto exit;
    }

    /* tofc */
    if (inm_read_prefix(df_fd,
                SVD_TAG_TIME_STAMP_OF_FIRST_CHANGE_V2) < 0) {
        printf("err : failed for \
                SVD_TAG_TIME_STAMP_OF_FIRST_CHANGE_V2\n");
        goto exit;
    }

    if (inm_read_data(df_fd, sizeof(SVD_TIME_STAMP_V2),
                &svd_tofc)<0) {
        printf("err : failed to read SVD_TIME_STAMP_V2");
        goto exit;
    }

    /* lodc */
    if (inm_read_prefix(df_fd, SVD_TAG_LENGTH_OF_DRTD_CHANGES) < 0) {
        printf("err : failed for SVD_LENGTH_OF_DRTD_CHANGES\n");
        goto exit;
    }

    if (inm_read_data(df_fd, sizeof(long long), &nr_drtd_chgs)<0) {
        printf("err : failed to read # of drtd chgs\n");
        goto exit;
    }

    /* read dirty changes */
    while (nr_drtd_chgs > 0) {
        if (inm_read_prefix(df_fd, SVD_TAG_DIRTY_BLOCK_DATA_V2) < 0) {
            printf("err : failed for SVD_TAG_DIRTY_BLOCK_DATA_V2\n");
            break;
        }

        if (inm_read_data(df_fd, sizeof(SVD_DIRTY_BLOCK_V2), &svd_dblk)<0){
            printf("err : failed to read SVD_DIRTY_BLOCK\n");
            break;
        }

        datap = valloc(svd_dblk.Length);
        if (!datap) {
            printf("err : malloc failed\n");
            break;
        }

#ifdef DEBUG_LOG
        printf("SVD DB (F)--@ off = %15lld, sz = %5u, td = %u, sd = %u\n", svd_dblk.ByteOffset,
                svd_dblk.Length, svd_dblk.uiTimeDelta, svd_dblk.uiSequenceNumberDelta);
#endif

        if (inm_read_data(df_fd, svd_dblk.Length, datap) < 0) {
            printf("err : read data failed\n");
            free(datap);
            break;
        }

        if (inm_apply_dblk(tgt_fd, &svd_dblk, datap) < 0) {
            printf("err : failed to apply svd_dblk\n");
            free(datap);
            break;
        }

        nr_drtd_chgs -= ( sizeof(SVD_PREFIX) +
                sizeof(SVD_DIRTY_BLOCK_V2) +
                svd_dblk.Length);
        free(datap);
    }

    if (inm_read_prefix(df_fd,
                SVD_TAG_TIME_STAMP_OF_LAST_CHANGE_V2) < 0) {
        printf("err : failed for \
                SVD_TAG_TIME_STAMP_OF_LAST_CHANGE_V2\n");
        goto exit;
    }

    if (inm_read_data(df_fd, sizeof(SVD_TIME_STAMP_V2),
                &svd_tolc)<0) {
        printf("err : failed to read SVD_TIME_STAMP_V2");
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

    WAL_TRACE_END();

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


    WAL_TRACE_START();

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
    snprintf(log_file_name, MAX_PATH_LEN, "%s%s%llu%s%llu%s%lu", src_dir_name,"/log_file_",s_time_in_nsec,"_", e_time_in_nsec,"_",logfile_seqno);
    fprintf(dbgfd,"log file name is [%s]\n", log_file_name);
    /*now open target log file to write this data stream*/
    tgt_fd = open(log_file_name, O_WRONLY|O_CREAT, 0640);
    if (tgt_fd < 0) {
        fprintf(dbgfd,"error failed to open target log file [%s]\n",log_file_name);
        return -1;
    }

    /*now process the data stream*/
    addrp = (inm_addr_t) udbp->uHdr.Hdr.ppBufferArray;

#if DEBUG_MMAPPED_DATA
    ret = -1;
    if (1) {
        unsigned char buf[0x1000] = {0};
        uint64_t dup_addrp=addrp;
        uint32_t rem = 0, len = 0;

        rem = udbp->uHdr.Hdr.ulcbChangesInStream;
        while (rem > 0) {
            len = min(rem,4096);
            memset(buf, 0, sizeof(buf));
            strncpy_s(buf, 0x1000, dup_addrp, 0x1000);
            dup_addrp += 0x1000;
            rem -= len;
            //ADDING BREAK TO READ ONLY FIRST PAGE

            break;
        }
        fprintf(dbgfd,"reading mmaped area %s\n", buf);
        fprintf(dbgfd,"this can be ignored by recompiling the binary"
                "by skipping compilation macro\n");

    }
#endif

    start_addrp = addrp;
    /* skip first 4 bytes (endian) */
    addrp = (inm_addr_t)(((char *) start_addrp + sizeof(uint32_t)));

    if (inm_validate_prefix(addrp, (int)SVD_TAG_HEADER1) < 0) {
        fprintf(dbgfd,"err : failed to validate SVD_TAG_HEADER1\n");
        goto exit;
    }
    addrp += (sizeof(SVD_PREFIX) + sizeof(SVD_HEADER1));

    if (inm_validate_prefix(addrp, SVD_TAG_TIME_STAMP_OF_FIRST_CHANGE_V2)
            < 0) {
        fprintf(dbgfd,"err : failed to validate \
                SVD_TAG_TIME_STAMP_OF_FIRST_CHANGE_V2\n");
        goto exit;
    }
    addrp += sizeof(SVD_PREFIX);


    svd_tsp = (SVD_TIME_STAMP_V2 *) addrp;

#ifdef DEBUG_LOG       
    fprintf(dbgfd,"udb -> first ts : t = %llu, seqno = %llu\n",
            (unsigned long long)                            \
            udbp->uTagList.TagList.TagTimeStampOfFirstChange. \
            TimeInHundNanoSecondsFromJan1601,
            (unsigned long long)                            \
            udbp->uTagList.TagList.TagTimeStampOfFirstChange. \
            ullSequenceNumber);
#endif

    addrp += sizeof(SVD_TIME_STAMP_V2);

    svd_prefixp = (SVD_PREFIX *) addrp;

    if (1) {
        if (inm_validate_prefix(addrp,
                    SVD_TAG_LENGTH_OF_DRTD_CHANGES) < 0) {
            fprintf(dbgfd,"err : failed to validate \
                    SVD_TAG_LENGTH_OF_DRTD_CHANGES\n");
            goto exit;
        }

        addrp += sizeof(SVD_PREFIX);
        len_chgsp = (uint64_t *)addrp;
        addrp += sizeof(uint64_t);

#ifdef DEBUG_LOG                 
        fprintf(dbgfd,"LODC --> %llu \n",
                (unsigned long long) *len_chgsp);

        fprintf(dbgfd,"udb : # of chgs in cur strm = %u \n",
                udbp->uHdr.Hdr.cChanges);
#endif


        while (chg_idx < udbp->uHdr.Hdr.cChanges) {
            SVD_DIRTY_BLOCK_V2 *svd_dblkp = NULL;

            if (inm_validate_prefix(addrp,
                        SVD_TAG_DIRTY_BLOCK_DATA_V2) < 0) {
                fprintf(dbgfd,"err: validation failed for \
                        SVD_TAG_DIRTY_BLOCK_DATA_V2\n");
                goto exit;
            }
            addrp += sizeof(SVD_PREFIX);

            svd_dblkp = (SVD_DIRTY_BLOCK_V2 *) addrp;

            if (0) {
                fprintf(dbgfd,"SVD DB (S)-->");
                fprintf(dbgfd,"off = %15lld, sz = %5u, td = %u, sd = %u",
                        svd_dblkp->ByteOffset,
                        svd_dblkp->Length,
                        svd_dblkp->uiTimeDelta,
                        svd_dblkp->uiSequenceNumberDelta);
                fprintf(dbgfd,"\n");
            }
            addrp += sizeof(SVD_DIRTY_BLOCK_V2);

            buf = valloc((size_t)svd_dblkp->Length);
            if(!buf){
                fprintf(dbgfd,"Failed to allocate the aligned buffer\n");
                goto exit;
            }

            memcpy(buf, addrp, svd_dblkp->Length);

            //now write this change to a new log file
            if (write(tgt_fd, buf, (size_t)svd_dblkp->Length)
                    < 0) {
                fprintf(dbgfd,"write failed for off = %llu\n",
                        svd_dblkp->ByteOffset);
                free(buf);
                goto exit;
            }
            free(buf);

            addrp += svd_dblkp->Length;

            nr_chgs_in_strm += svd_dblkp->Length;
            chg_idx++;
        }
    }
#ifdef DEBUG_LOG
    fprintf(dbgfd,"amt of chgs in cur strm = %12llu\n",
            (unsigned long long)nr_chgs_in_strm);
#endif

    svd_prefixp = (SVD_PREFIX *) addrp;
    if (inm_validate_prefix(addrp, SVD_TAG_TIME_STAMP_OF_LAST_CHANGE_V2)
            < 0) {
        fprintf(dbgfd,"err : faild to validate tolc\n");
        goto exit;
    }

    addrp += sizeof(SVD_PREFIX);

    svd_tsp = (SVD_TIME_STAMP_V2 *) addrp;

    if (0) {
        fprintf(dbgfd,"TOLC ");
        fprintf (dbgfd, " t = %10llu ",
                (unsigned long long)
                svd_tsp->TimeInHundNanoSecondsFromJan1601);
        fprintf(dbgfd,"seqno = %10llu \n", (unsigned long long)
                svd_tsp->ullSequenceNumber);
    }
    addrp += sizeof(SVD_TIME_STAMP_V2);



    strm_len = (addrp - start_addrp);
    if (strm_len != udbp->uHdr.Hdr.ulcbChangesInStream) {
        fprintf(dbgfd,"strm len doesn't match with parse len \
                %d, %ld\n", udbp->uHdr.Hdr.ulcbChangesInStream,
                strm_len);
        goto exit;
    }

    ret = 0;

exit:
    if (tgt_fd != -1) {
        close(tgt_fd);
    }
    WAL_TRACE_END();

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

#ifdef DEBUG_LOG
        WAL_TRACE_START();
#endif
        //get last time stamp and seq. no. of this disk
        pthread_mutex_lock(&mutex);
        last_ts = g_disk_listp[disk_index].last_ts;
        last_seqno = g_disk_listp[disk_index].last_seqno;
        pthread_mutex_unlock(&mutex);

        s_time_in_nsec = udbp->uTagList.TagList.TagTimeStampOfFirstChange. \
                        TimeInHundNanoSecondsFromJan1601;

        s_seqno = udbp->uTagList.TagList.TagTimeStampOfFirstChange. \
                        ullSequenceNumber;

        e_time_in_nsec = udbp->uTagList.TagList.TagTimeStampOfLastChange. \
                        TimeInHundNanoSecondsFromJan1601;

        e_seqno = udbp->uTagList.TagList.TagTimeStampOfLastChange. \
                        ullSequenceNumber;

        if (last_ts) {
                if (last_seqno > s_seqno || s_seqno > e_seqno) {
                        fprintf(dbgfd,"err : seq # mismatch : \n"
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
                        fprintf(dbgfd,"err : time mismatch : tofc (%llu) < \
                                last_ts (%llu)\n",
                                (unsigned long long) s_time_in_nsec,
                                (unsigned long long) last_ts);
                        ret = -1;
                        assert(0);
                        goto exit;
                }

                if (e_time_in_nsec < s_time_in_nsec) {
                        fprintf(dbgfd,"err : tofc (%llu) > tolc(%llu)\n",
                                (unsigned long long) s_time_in_nsec,
                                (unsigned long long) e_time_in_nsec);
                        ret = -1;
                        assert(0);
                        goto exit;
                }
        //      case : equal
        //      (s_time_in_nsec == e_time_in_nsec &&
        //      s_seqno == e_seqno){
        }
        pthread_mutex_lock(&mutex);
        g_disk_listp[disk_index].last_ts = e_time_in_nsec;
        g_disk_listp[disk_index].last_seqno = e_seqno;
        pthread_mutex_unlock(&mutex);
        ret = 0;
exit:
#ifdef DEBUG_LOG
        WAL_TRACE_END();
#endif
        return ret;
}



int start_flt_replicate(char *src_disk, char *pname, int disk_index) {

    UDIRTY_BLOCK_V2 *udbp = NULL;
    COMMIT_TRANSACTION commit;
    int src_fd = -1, tgt_fd = -1, ret = -1;
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
 
    WAL_TRACE_START();

    /*
     * first start filtering for the device, this needed because the
     * use of filp_open to open bitmap file by the driver and it requires process context.
     * require to do it so that bitmap file can created by driver for each source device
     * in process context, so using inm_dmit to use filp_open in process context.
     */

    snprintf(inm_start_flt_disk_cmd, MAX_PATH_LEN, "%s --op=start_flt --src_vol=%s",DEFAULT_INM_PATH,src_disk);
    fprintf(dbgfd,"stack disk command is [%s]\n",inm_start_flt_disk_cmd);
    pthread_mutex_lock(&mutex);
    system(inm_start_flt_disk_cmd);
    pthread_mutex_unlock(&mutex);

    /*
     * now open driver for each source disk context, because the same handle
     * will be use by start filtering and get db IOCTL call. start filtering
     * ioctl will save the target context inside struct file private_data
     * of this thread and get db will get context from the same private_data.
     */

    fprintf(dbgfd,"opening driver to replicate disk [%s]\n",src_disk);
    dev_fd = open(INM_CTRL_DEV, O_RDWR);
    if (dev_fd < 0) {
        fprintf(dbgfd,"failed to open ctrl dev /dev/involflt.\n");
        return -1;
    }
    fprintf(dbgfd,"done opening driver to replicate disk [%s]\n",src_disk);

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
    strcpy_s(&dev_info.d_guid[0], sizeof(dev_info.d_guid), src_disk);
    if(dev_size(&dev_info)){
        goto exit;
    }

    dev_info.d_flags = 0;
    fprintf(dbgfd,"stacking disk [%s], pname [%s]\n",src_disk, pname);
    strcpy_s(&dev_info.d_pname[0], sizeof(dev_info.d_pname), pname);
    if (ioctl(dev_fd,
                IOCTL_INMAGE_START_FILTERING_DEVICE_V2,
                &dev_info) == -1) {
        fprintf(dbgfd,"IOCTL_INMAGE_START_FILTERING_DEVICE_V2 ioctl failed\n");
        ret = -1;
        goto exit;
    }
    fprintf(dbgfd,"done stacking disk [%s], pname [%s]\n",src_disk, pname);

    //allocate udirty block
    udbp = (UDIRTY_BLOCK_V2 *) malloc(sizeof(*udbp));
    if (!udbp) {
        fprintf(dbgfd,"malloc failed for udbp\n");
        ret = -1;
        goto exit;
    }
    memcpy(wait_db.VolumeGUID, src_disk, GUID_SIZE_IN_CHARS);
    wait_db.Seconds = 65;

start_store_logfile_new_dir:
    //each thread will have their own dir_seq_no starting from 1
    ++dir_seq_no;

    //create directory to store log files
    //after each tag you need new set of dir to store the log files
    src_dir_name[0] = '\0';
    src_dir_cmd[0] = '\0';
    snprintf(src_dir_name, MAX_PATH_LEN, "%s%s%s%lu", BASE_DIR, src_disk, DIR_LOG, dir_seq_no);
    fprintf(dbgfd,"log directory name is %s\n", src_dir_name);
    snprintf(src_dir_cmd, MAX_PATH_LEN, "mkdir -p %s", src_dir_name);
    fprintf(dbgfd,"log directory create commad is [%s]\n",src_dir_cmd);
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
    fprintf(dbgfd,"Waiting on ...\n");
    if( ioctl(dev_fd,
                IOCTL_INMAGE_WAIT_FOR_DB, &wait_db) < 0) {
        fprintf(dbgfd,"Timedout in wait for db ioctl for vol = %s, errno %d\n",
                wait_db.VolumeGUID, errno);
    }

get_db:
    memset(udbp, 0, sizeof(*udbp));

    //call getdb IOCTL
    if ( ioctl(dev_fd,
                IOCTL_INMAGE_GET_DIRTY_BLOCKS_TRANS,
                udbp) < 0) {
        fprintf(dbgfd,"stream_len = %u, errno = %d\n", udbp->uHdr.Hdr.ulcbChangesInStream, errno);
        goto wait_on;
    }

    /*
     * parse UDB structure
     * now you needed to serialize last sequence number for each device
     * and to do so you needed static variable to save last sequence number
     * but static variable is not thread safe, so you will be needed
     * a thread safe mechanism and to do so you will require to maintain
     * a varibale for each disk inside global disk list structure,
     * and at the time of verification you will only require to
     * pass the index of disk present inside disk list so that validate will
     * check and modify only variable of that disk for which index have been send
     * and not for others. Life of these variables will be iequal to program's life
     * and will remain alive until you don't free it or programe terminates.
     */
 
    if (!(udbp->uHdr.Hdr.ulFlags & UDIRTY_BLOCK_FLAG_DATA_FILE) &&
            (inm_validate_ts(udbp, disk_index) < 0)) {
        fprintf(dbgfd,"err : ts validation has failed ... exiting\n");
        ret = -1;
        goto exit;
    }

    if (udbp->uHdr.Hdr.ulFlags & UDIRTY_BLOCK_FLAG_DATA_FILE) {
        fprintf(dbgfd,"Data File name = %s\n", udbp->uTagList.DataFile.FileName);
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
        fprintf(dbgfd,"data source = %s mode \n",
                (udbp->uTagList.TagList.TagDataSource.ulDataSource ==
                 INVOLFLT_DATA_SOURCE_META_DATA) ? "METADATA" :
                "BITMAP");
        ret = process_tags_v2(udbp, tag_name);
        fprintf(dbgfd,"the value return for tag is ret=[%d]\n",ret);

    }

#ifdef DEBUG_LOG
    fprintf(dbgfd,"split seq id = %d\n", udbp->uHdr.Hdr.ulSequenceIDforSplitIO);
    fprintf(dbgfd,"size of the last udb = %.4f MB\n", (float)udbp->uHdr.Hdr.ulicbChanges/(1024*1024));
    fprintf(dbgfd,"Total changes pending the driver = %llu\n", (unsigned long long)udbp->uHdr.Hdr.ulTotalChangesPending);
    fprintf(dbgfd,"Current Timestamps = %llu, Sequence Number = %llu, Continuation id = %u\n",
            udbp->uTagList.TagList.TagTimeStampOfLastChange.TimeInHundNanoSecondsFromJan1601,
            udbp->uTagList.TagList.TagTimeStampOfLastChange.ullSequenceNumber,
            udbp->uHdr.Hdr.ulSequenceIDforSplitIO);
    fprintf(dbgfd,"Previous Timestamps = %llu, Sequence Number = %llu, Continuation id = %u\n",
            (unsigned long long)udbp->uHdr.Hdr.ullPrevEndTimeStamp,
            (unsigned long long)udbp->uHdr.Hdr.ullPrevEndSequenceNumber,
            udbp->uHdr.Hdr.ulPrevSequenceIDforSplitIO);
    fprintf(dbgfd,"Write Order State = %d\n", udbp->uHdr.Hdr.eWOState);
#endif

commit:
    memset(&commit, 0, sizeof(commit));
    if(udbp->uHdr.Hdr.ulFlags | UDIRTY_BLOCK_FLAG_VOLUME_RESYNC_REQUIRED)
        commit.ulFlags |= COMMIT_TRANSACTION_FLAG_RESET_RESYNC_REQUIRED_FLAG;
    commit.ulTransactionID = udbp->uHdr.Hdr.uliTransactionID;
    if( ioctl(dev_fd,
                IOCTL_INMAGE_COMMIT_DIRTY_BLOCKS_TRANS,
                &commit) == -1) {
        fprintf(dbgfd,"Commit Fail\n");
        goto exit;
    }

    //rerun once you get the tag
    if (ret == 1) {
        fprintf(dbgfd,"Tag issued successfully!\n");
        ret = 0;
        sleep(1);

        if (g_tag_ioctl_failed_for_itr) {
            fprintf(dbgfd,"Skipping this tag because tag ioctl failed for this iteration.\n");
            goto wait_on;
        }
        //create a file with name same as tag name received in tag buffer.
        //we are keep issuing tag on timeout value, and this file act as marker.
        fprintf(dbgfd, "received tag file name from tag is %s\n", tag_name);
        log_file_name[0] = '\0';
        snprintf(log_file_name, MAX_PATH_LEN, "%s%s%s", src_dir_name,"/", tag_name);
        fprintf(dbgfd,"creating tag file with name [%s]\n", log_file_name);
        log_fd = open(log_file_name, O_WRONLY|O_CREAT, 0640);
        if (log_fd <0) {
                fprintf(dbgfd,"error failed to open target lof file [%s]\n",log_file_name);
                return -1;
        }
        close(log_fd);

        if (!g_start_verifier_thread) {

            g_start_verifier_thread = 1;

	    //now send signal to verifier thread to start verifier
            pthread_mutex_lock(&verifier_mutex);
            pthread_cond_signal(&verifier_cond_wait);
            pthread_mutex_unlock(&verifier_mutex);

        }

        //now go for next iteration of tag, don't exit till timeout of process happens
        fprintf(dbgfd,"now going for next iteration to receive tag!\n");
        goto start_store_logfile_new_dir;
    }

    if (g_done_with_replication_thread) {
        //wal test completion timeout
        ret = 0;
        goto exit;
    }

    ret = 0;
    //keep looping
    if (udbp->uHdr.Hdr.ulTotalChangesPending > DEFAULT_DB_NOTIFY_THRESHOLD) {
            goto get_db;
    } else {
            goto wait_on;
    }

exit:
    close(dev_fd);
    if (src_fd > 0) {
        close(src_fd);
    }
    if (tgt_fd > 0) {
        close(tgt_fd);
    }
    if (udbp) {
        free(udbp);
    }

    WAL_TRACE_END();
    return ret;
}

void *repl_op(void *threadid) {

    int disk_index;
    int ret;

    WAL_TRACE_START();
    disk_index = get_disk_list_index();
    fprintf(dbgfd,"Processing disk %s\n", g_disk_listp[disk_index].disk_name);
    sleep(1);
    //now each thread will process one disk which it owns
    ret = start_flt_replicate(g_disk_listp[disk_index].disk_name, g_disk_listp[disk_index].disk_pname, disk_index);
    if (ret) {
        ret = -1;
        goto exit;
    }

exit:
    WAL_TRACE_END();
    if (ret == -1) { 
        return ret;
    }

    return (NULL);
}


int apply_data_on_disk(char *src_disk, int disk_index, unsigned char *buf)
{
    int ret = -1;
    int fdSrc = -1;
    unsigned long long max_disk_offset_count = 0;
    unsigned long long last_disk_offset = 0;
    unsigned long long bytecount;
    unsigned long long no_of_io;
    int i = 0;

#ifdef DEBUG_LOG
    WAL_TRACE_START();
#endif

    //now write whole string of pattern to disk
#ifdef DEBUG_LOG
    fprintf(dbgfd,"writting pattern [%lu] --> to disk [%s]\n", *g_io_seq_no, src_disk);
#endif
    //now open device to write pattern
    if ( (fdSrc = open(src_disk, O_RDWR|O_DIRECT)) < 0 )
    {
        fprintf(dbgfd,"error- failed to open source disk [%s]\n", src_disk);
        ret = -1;
        goto exit;
    }

    //each disk can have different size but will rewrite after common
    //minimum size accross all the disks strting from offset zero. 
    //each disk will have thier own last written offset number.
    last_disk_offset = g_disk_listp[disk_index].last_disk_offset;

    if (last_disk_offset >= g_com_min_disk_offset_all_disk) {
#ifdef DEBUG_LOG
        fprintf(dbgfd,"reached end of disk, last_disk_offset [%lu], g_com_min_disk_offset_all_disk [%lu], restarting write from offset zero for disk [%s].\n",
                last_disk_offset, g_com_min_disk_offset_all_disk, src_disk);
#endif
        last_disk_offset = 0;
    }

#ifdef DEBUG_LOG
    fprintf(dbgfd,"writing at offset [%llu]\n", last_disk_offset);
#endif

    //lseek to last written offset and start writing from next first byte
    if( lseek(fdSrc, last_disk_offset, SEEK_SET) < 0) {
        fprintf(dbgfd,"failed to lseek for device [%s] at offset [%llu]\n", src_disk, last_disk_offset);
        ret = -1;
        goto exit;
    }
    //now write pattern
    if( write(fdSrc, buf, g_sec_sz_inbyte) < 0) {
        fprintf(dbgfd,"failed to write for device [%s] at offset [%llu]\n", src_disk, last_disk_offset);
        ret = -1;
        goto exit;
    }
    last_disk_offset += g_sec_sz_inbyte;
    g_disk_listp[disk_index].last_disk_offset = last_disk_offset;
   
    ret = 0;

exit: 
    if (fdSrc != -1) {
        close(fdSrc);
    }

#ifdef DEBUG_LOG
    WAL_TRACE_END();
#endif
    return ret;
}

void *apply_wo_data(void *threadid) {

    int ret;
    int disk_index;
    int fdSrc = -1;
    int flag = 0;
    unsigned char *buf, *p, *src_disk;
    unsigned long long max_disk_offset_count = 0;
   
    WAL_TRACE_START();

    //first find common minimum size accross all the disks
    for ( disk_index = 0; disk_index < g_nr_disk; disk_index++) {

        src_disk = g_disk_listp[disk_index].disk_name;
        if ( (fdSrc = open(src_disk, O_RDONLY|O_DIRECT)) < 0 )
        {
            fprintf(dbgfd,"error: failed to open source disk [%s]\n", src_disk);
            ret = -1;
            goto exit;
        }
        max_disk_offset_count = lseek(fdSrc, 0, SEEK_END);
        fprintf(dbgfd,"maximum disk offset count [%llu] for disk [%s]\n",
                max_disk_offset_count, g_disk_listp[disk_index].disk_name);
        max_disk_offset_count = max_disk_offset_count/g_sec_sz_inbyte;
        fprintf(dbgfd,"maximum disk offset count in chunks of 512 byte [%llu] for disk [%s]\n",
                max_disk_offset_count, g_disk_listp[disk_index].disk_name);
        if (!flag) {
            g_com_min_disk_offset_all_disk = max_disk_offset_count;
            flag = 1;
        } else if ( max_disk_offset_count < g_com_min_disk_offset_all_disk ) {
            g_com_min_disk_offset_all_disk = max_disk_offset_count;
        }
        if (fdSrc != -1) {
            close(fdSrc);
        }
    }
    fprintf(dbgfd,"com_min_disk_offset_all_disk is [%lu]\n", g_com_min_disk_offset_all_disk);

    ret = posix_memalign(&buf, g_sec_sz_inbyte, g_sec_sz_inbyte);
    if (ret) {
         fprintf(dbgfd,"error: failed to allocate memory\n");
         ret = -1;
         goto exit;
    }
    //now write the pattern of sequence from 1...n
    memset(buf, 0, g_sec_sz_inbyte);
    p = (buf + (g_sec_sz_inbyte - DEF_SIZE_OF_PATTERN_BYTE));
    g_io_seq_no = (unsigned long *)&p[0];

    while ( 1 ) {

        (*g_io_seq_no)++;

        for ( disk_index = 0; disk_index < g_nr_disk; disk_index++) {
#ifdef DEBUG_LOG
            fprintf(dbgfd,"Applying data on disk [%s]\n",g_disk_listp[disk_index].disk_name);
#endif
            src_disk = g_disk_listp[disk_index].disk_name;
            ret = apply_data_on_disk(src_disk, disk_index, buf);
            if (ret) {
                ret = -1;
                goto exit;
            }
        }
        //if you want to debug with one pattern on all disks
        //return (NULL);
        if (g_done_with_replication_thread) {
            ret = 0;
            goto exit;
        }
    }

exit:
    if (buf) {
        free(buf);
    }

    WAL_TRACE_END();
    if (ret == -1) {
        return ret;
    }
    return (NULL);
}

void *issue_tag(void *thread)
{
    int ret = 0;
    int dev_fd;
    tag_info_t_v2 *tagp = NULL;
    unsigned int rand_num;
    char rand_tag_name[MAX_PATH_LEN];
    char *tag_name_prefix = "tag";

    WAL_TRACE_START();

    tagp = (tag_info_t_v2 *) calloc(1, sizeof(tag_info_t_v2));
    if(!tagp) {
        fprintf(dbgfd,"error: failed to allocate memory to store tag info\n");
        ret = -1;
        goto exit;
    }
    tagp->flags |= TAG_ALL_PROTECTED_VOLUME_IOBARRIER;
    tagp->nr_tags = 1;
    tagp->tag_names = NULL;
   
    tagp->tag_names = (tag_names_t *)calloc(1, sizeof(tag_names_t));
    if (!tagp->tag_names) {
        fprintf(dbgfd,"error: failed to allocate memory to store tag name\n");
        ret = -1;
        goto exit;
    }

    dev_fd = open(INM_CTRL_DEV, O_RDWR);
    if (dev_fd < 0) {
        fprintf(dbgfd,"failed to open ctrl dev /dev/involflt.\n");
        ret = -1;
        goto exit;
    }

    //keep issuing tag on userdefine time out value till process not happens.
    while (1) {

        //sleep for tag timeout
        fprintf(dbgfd,"waiting on timeout [%lu] value to issue tag...\n", g_tag_timeout);
        sleep(g_tag_timeout);

        if (g_done_with_replication_thread) {
            goto exit;
        }

        tagp->tag_names->tag_len = 0;
        tagp->tag_names->tag_name[0] = '\0';
        srand((unsigned int) time(NULL));
        rand_num = rand();
        rand_tag_name[0] = '\0';
        snprintf(rand_tag_name, MAX_PATH_LEN, "%s%s%u", tag_name_prefix,"_", rand_num);
        fprintf(dbgfd,"generated tag file name [%s]\n", rand_tag_name);
        tagp->tag_names->tag_len = strlen(rand_tag_name);
        strcpy_s(tagp->tag_names->tag_name, sizeof(tagp->tag_names->tag_name), rand_tag_name);

        fprintf(dbgfd,"issuing tag with tag name [%s]\n",tagp->tag_names->tag_name);
        if (ioctl(dev_fd,
                    IOCTL_INMAGE_IOBARRIER_TAG_VOLUME,
                    tagp) < 0) {
            fprintf(dbgfd,"IOCTL_INMAGE_IOBARRIER_TAG_VOLUME ioctl failed, going for next iteration to issue tag\n");
            g_tag_ioctl_failed_for_itr = 1;
            ret = -1;
            continue;
        }
        fprintf(dbgfd,"system level tag with name [%s] successfully issued, going for next iteration to issue tag\n", rand_tag_name);
        g_tag_ioctl_failed_for_itr = 0;
        ret = 0;
    }

exit:
    if (dev_fd != -1) {
        close(dev_fd);
    }
    if (tagp) {
        if (tagp->tag_names) {
            free(tagp->tag_names);
            tagp->tag_names = NULL;
        }
        free(tagp);
        tagp = NULL;
    } 

    WAL_TRACE_END();

    if (ret == -1) {
        return ret;
    }
    return (NULL);
}

int launch_replication_thread(void)
{
    int ret, t;
    pthread_t *threads = NULL;
    pthread_attr_t attr;

    WAL_TRACE_START();
    //alloc memory for thread objects
    //number of source disk + apply io on disk thread + issue tag thread
    threads = (pthread_t*) calloc((g_nr_disk+1+1), sizeof(pthread_t));
    if (!threads) {
        fprintf(dbgfd,"error: failed to allocate memory\n");
        exit(-1);
    }

    //make thread joinable
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

    //init mutex
    pthread_mutex_init(&mutex, NULL);

    //create replication threads
    for (t = 0; t < g_nr_disk; t++) {
        fprintf(dbgfd,"Creating replication thread number %d\n", t+1);
        //first argument of create should be a pointer
        //it only starts execution of repl_op, it doen't wait for its completion
        ret = pthread_create(threads + t, &attr, repl_op, (void*)t);
        if (ret) {
            fprintf(dbgfd,"error: failed to create replication thread\n");
            exit(-1);
        }
    }

    //create disk io thread, don't need to increament t it is already
    //been incremented in last loop.
    //sleep for some time so that replication get started first for
    //all the source devices.
    sleep(60);
    ret = pthread_create(threads + t, &attr, apply_wo_data, (void*)t);
    if (ret) {
        fprintf(dbgfd,"error: failed to create apply io on disk thread\n");
        exit(-1);
    }

    //create issue tag thread, need to increament t
    ++t;
    ret = pthread_create(threads + t, &attr, issue_tag, (void*)t);
    if (ret) {
        fprintf(dbgfd,"error: failed to create issue_tag thread\n");
        exit(-1);
    }

    //join replication threads
    for (t = 0; t < g_nr_disk; t++) {
        //first argument of join should be a variable
        ret = pthread_join(threads[t], NULL);
        fprintf(dbgfd,"Exited replication thread number %d\n", t);
    }

    //all replication thread are closed now also close the apply
    //data thread
    pthread_mutex_lock(&mutex);
    g_done_with_replication_thread = 1;
    pthread_mutex_unlock(&mutex);
 
    //join disk io thread, don't need to increment t it is
    //already been incremented in last loop.
    ret = pthread_join(threads[t], NULL);
    fprintf(dbgfd,"Exited apply io on disk thread number %d\n", t);

    //join issue tag thread, need to increment t
    ++t;
    ret = pthread_join(threads[t], NULL);
    fprintf(dbgfd,"Exited issue tag thread number %d\n", t);

    sleep(5);
    //free threads
    pthread_attr_destroy(&attr);
    pthread_mutex_destroy(&mutex);
    if (threads) {
        free(threads);
    }

    WAL_TRACE_END();
    return ret;
}

void *start_service(void *thread)
{
    int ret;
    int dev_fd;
    SHUTDOWN_NOTIFY_INPUT stop_notify;
    unsigned long div = 0, rem = 0;

    dev_fd = open(INM_CTRL_DEV, O_RDWR);
    if (dev_fd < 0) {
        fprintf(dbgfd,"failed to open ctrl dev /dev/involflt.\n");
        ret = -1;
        goto exit;
    }

    stop_notify.ulFlags = 0;
    fprintf(dbgfd,"issuing service shutdown notifier ioctl.\n");
    if (ioctl(dev_fd,
                IOCTL_INMAGE_SERVICE_SHUTDOWN_NOTIFY,
                &stop_notify) < 0) {
        fprintf(dbgfd,"IOCTL_INMAGE_SERVICE_SHUTDOWN_NOTIFY ioctl failed.\n");
        ret = -1;
        goto exit;
    }
    fprintf(dbgfd,"service shutdown notifier ioctl issued successfully.\n");

    div = g_service_timeout / SERVICE_SLEEP_INTERVAL;
    rem = g_service_timeout % SERVICE_SLEEP_INTERVAL;

    //sleep contineously in the back ground
    while (1) {

        if ( g_service_timeout < SERVICE_SLEEP_INTERVAL ) {
            sleep(g_service_timeout);
        } else {
           while (div) {
               sleep(SERVICE_SLEEP_INTERVAL);
               --div;
           } 
           if (rem) {
               sleep(rem);
           }
       }
        //now set flag here, time to stop all the threads and exit from this process
        g_done_with_replication_thread = 1;
        fprintf(dbgfd,"Received WAL test timeout value! stoping WAL test now.\n");
        sleep(SERVICE_SLEEP_INTERVAL);
    }

out:
    ret = 0;

exit:
    if (dev_fd != -1) {
        close(dev_fd);
    }

    WAL_TRACE_END();

    if (ret == -1) {
        return ret;
    }
    return (NULL);
}

void launch_service_start_thread(void)
{
    int ret;
    int t = 0;
    pthread_t service_thread_id;
    pthread_attr_t attr;

    WAL_TRACE_START();

    //set state to detached, don't need to wait for it inside main thread
    //the thread creation will only call start_service function and comes out
    //doesn't wait for completion of start_service() function, so the control
    //immediatley goes to main thread and main thread keep doing its work while
    //this thread will keep running in a while loop.
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    //create thread
    ret = pthread_create(&service_thread_id, &attr, start_service, (void*)t);
    if (ret) {
        fprintf(dbgfd,"error: failed to create service start thread.\n");
        exit(-1);
    }

    WAL_TRACE_END(); 
}

void stop_filtering_list_of_devices(void)
{
    int numdisk;
    char inm_stop_flt_disk_cmd[MAX_PATH_LEN];

    WAL_TRACE_START();

    for (numdisk = 0; numdisk < g_nr_disk; numdisk++) {
        fprintf(dbgfd,"stop filtering for device [%s]\n", g_disk_listp[numdisk].disk_name);
        memset(inm_stop_flt_disk_cmd, '\0', MAX_PATH_LEN);
        snprintf(inm_stop_flt_disk_cmd, MAX_PATH_LEN, "%s --op=stop_flt --src_vol=%s",
                                        DEFAULT_INM_PATH,
                                        g_disk_listp[numdisk].disk_name);
        fprintf(dbgfd,"stop filtering disk command is [%s]\n", inm_stop_flt_disk_cmd);
        system(inm_stop_flt_disk_cmd);
    }

   WAL_TRACE_END();
}

void issue_process_start_notify(void)
{
    char inm_process_start_notify[MAX_PATH_LEN];

    WAL_TRACE_START();

    fprintf(dbgfd,"issuing process start notify\n");
    memset(inm_process_start_notify, '\0', MAX_PATH_LEN);
    snprintf(inm_process_start_notify, MAX_PATH_LEN, "%s --op=start_notify &",
                                    DEFAULT_INM_PATH);
    fprintf(dbgfd,"process start notify command is [%s]\n", inm_process_start_notify);
    system(inm_process_start_notify);

    WAL_TRACE_END();
}

void stop_process_start_notify(void)
{
    char inm_process_start_notify[MAX_PATH_LEN];

    WAL_TRACE_START();

    fprintf(dbgfd,"stopping process start notify\n");
    memset(inm_process_start_notify, '\0', MAX_PATH_LEN);
    snprintf(inm_process_start_notify, MAX_PATH_LEN, "%s %s", PKILL_CMD, INM_DMIT);
    fprintf(dbgfd,"stop process start notify command is [%s]\n", inm_process_start_notify);
    system(inm_process_start_notify);

    fprintf(dbgfd,"stopping process start notify\n");
    memset(inm_process_start_notify, '\0', MAX_PATH_LEN);
    snprintf(inm_process_start_notify, MAX_PATH_LEN, "%s %s", KILLALL_CMD, INM_DMIT);
    fprintf(dbgfd,"stop process start notify command is [%s]\n", inm_process_start_notify);
    system(inm_process_start_notify);

    WAL_TRACE_END();
}
