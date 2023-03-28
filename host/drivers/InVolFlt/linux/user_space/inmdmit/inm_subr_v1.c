
#ident "@(#) $Source $Revision: 1.1.2.1 $ $Name: INT_BRANCH $ $Revision: 1.1.2.1 $"

#include "inm_filter.h"
#include "involflt.h"
#include "svdparse.h"
#include "inm_subr_v1.h"
#include "inm_subr_v2.h"
#include "inm_subr_v3.h"

extern char     *getfullrawname(char *);
extern struct config *app_confp;
unsigned int db_notify_threshold = (1024 * 1024);
/* validates the time stamp in udirty block */
int
validate_ts_v1(UDIRTY_BLOCK *udbp)
{
	static uint64_t last_ts = 0;
	static uint64_t last_seqno = 0;
	uint64_t s_time_in_nsec = 0, s_seqno = 0;
	uint64_t e_time_in_nsec = 0, e_seqno = 0;
	int ret = -1;

	INM_TRACE_START();

	s_time_in_nsec = udbp->uTagList.TagList.TagTimeStampOfFirstChange. \
			TimeInHundNanoSecondsFromJan1601;

	s_seqno = udbp->uTagList.TagList.TagTimeStampOfFirstChange. \
			ulSequenceNumber;

	e_time_in_nsec = udbp->uTagList.TagList.TagTimeStampOfLastChange. \
			TimeInHundNanoSecondsFromJan1601;

	e_seqno = udbp->uTagList.TagList.TagTimeStampOfLastChange. \
			ulSequenceNumber;

	if (!last_ts) {
		last_ts = s_time_in_nsec;
		last_seqno = s_seqno;
	} else {
		if (s_time_in_nsec < last_ts) {
			printf("err : time mismatch : tofc (%llu) < \
				last_ts (%llu)\n", 
				(unsigned long long) s_time_in_nsec,
				(unsigned long long) last_ts);
		}

		if (e_time_in_nsec < s_time_in_nsec) {
			printf("err : tofc (%llu) > tolc(%llu)\n",
				(unsigned long long) s_time_in_nsec,
				(unsigned long long) e_time_in_nsec);
			return -1;
		}

	//	case : equal
	//	(s_time_in_nsec == e_time_in_nsec &&
	//		 s_seqno == e_seqno){
		
	}

	last_ts = e_time_in_nsec;
	last_seqno = e_seqno;
	ret = 0;

	INM_TRACE_END("%d", ret);
	return ret;
}

/* applying changes on target device */

int
apply_dblk_v1(int tgt_fd, SVD_DIRTY_BLOCK *dblkp, void *datap)
{
	int nr_bytes = 0, ret = -1;

	INM_TRACE_START();

	if (lseek(tgt_fd, dblkp->ByteOffset, SEEK_SET) == -1) {
		printf("err : failed in lseek \n");
	}

	if ((nr_bytes = write(tgt_fd, datap, dblkp->Length) != -1) &&
		(nr_bytes == dblkp->Length)) {
		ret = 0;
	} else {
		printf("err : write failed \n");
	}

	INM_TRACE_END("%d", ret);

	return ret;
}

/* processing data file */
int
process_dfm_v1(int tgt_fd, UDIRTY_BLOCK *udbp)
{
	int ret = -1, df_fd;
	SVD_HEADER1 svd_hdr;
	SVD_TIME_STAMP svd_tofc, svd_tolc;
	void *datap = NULL;
	long long nr_drtd_chgs = 0;
	char *dfnamep = NULL;
	SVD_DIRTY_BLOCK svd_dblk;

	INM_TRACE_START();
		
	dfnamep = (char *)udbp->uTagList.DataFile.FileName;
	df_fd = open(dfnamep, O_RDONLY);
	if (!df_fd) {
		printf("err: failed to open data file %s\n", dfnamep);
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
			SVD_TAG_TIME_STAMP_OF_FIRST_CHANGE) < 0) {
		printf("err : failed for \
			SVD_TAG_TIME_STAMP_OF_FIRST_CHANGE\n");
		goto exit;
	}

	if (inm_read_data(df_fd, sizeof(SVD_TIME_STAMP),
				 &svd_tofc)<0) {
		printf("err : failed to read SVD_TIME_STAMP");
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
		if (inm_read_prefix(df_fd, SVD_TAG_DIRTY_BLOCK_DATA) < 0) {
			printf("err : failed for SVD_TAG_DIRTY_BLOCK_DATA\n");
			break;
		}

		if (inm_read_data(df_fd, sizeof(SVD_DIRTY_BLOCK), &svd_dblk)<0){
			printf("err : failed to read SVD_DIRTY_BLOCK\n");
			break;
		}
		
		datap = malloc(svd_dblk.Length);
		if (!datap) {
			printf("err : malloc failed\n");
			break;
		}
		if (app_confp->verbose) {
			printf("SVD DB (F)--@ off = %15lld, size = %5lld\n", svd_dblk.ByteOffset,
				svd_dblk.Length);
		}

		if (inm_read_data(df_fd, svd_dblk.Length, datap) < 0) {
			printf("err : read data failed\n");
			free(datap);
			break;
		}
		
		if (app_confp->prof) {
			goto jump_profiler;
		}

		if (inm_apply_dblk(tgt_fd, &svd_dblk, datap) < 0) {
			printf("err : failed to apply svd_dblk\n");
			free(datap);
			break;
		}

jump_profiler:
		nr_drtd_chgs -= ( sizeof(SVD_PREFIX) +
					sizeof(SVD_DIRTY_BLOCK) +
					svd_dblk.Length);
		free(datap);
	}

	if (inm_read_prefix(df_fd,
			SVD_TAG_TIME_STAMP_OF_LAST_CHANGE) < 0) {
		printf("err : failed for \
			SVD_TAG_TIME_STAMP_OF_LAST_CHANGE\n");
		goto exit;
	}

	if (inm_read_data(df_fd, sizeof(SVD_TIME_STAMP),
				 &svd_tolc)<0) {
		printf("err : failed to read SVD_TIME_STAMP");
		goto exit;
	}

	ret = 0;

exit:
	if (df_fd > 0) {
		close(df_fd);
	}

	INM_TRACE_END("%d", ret);

	return ret;
}

/* processing meta data changes */
int
process_md_v1(int src_fd, int tgt_fd, UDIRTY_BLOCK *udbp)
{
	uint64_t off = 0;
	uint32_t sz = 0;
	void *bufp = NULL;
	int ret = -1, cur_chg = 0, nr_bytes = 0;
	
	INM_TRACE_START();
	
	printf("tofc = %llu , s_seqno = %llu\n",
		(unsigned long long)				\
		udbp->uTagList.TagList.TagTimeStampOfFirstChange. \
				TimeInHundNanoSecondsFromJan1601,
		(unsigned long long)				\
		udbp->uTagList.TagList.TagTimeStampOfFirstChange. \
				ulSequenceNumber);

	printf("tolc = %llu, e_seqno = %llu\n", 
		(unsigned long long)				\
		udbp->uTagList.TagList.TagTimeStampOfLastChange. \
				TimeInHundNanoSecondsFromJan1601,
		(unsigned long long)				\
		udbp->uTagList.TagList.TagTimeStampOfLastChange. \
				ulSequenceNumber);

	if(udbp->uHdr.Hdr.ulFlags & UDIRTY_BLOCK_FLAG_TSO_FILE)
		printf("TSO file\n");

	if (!udbp->uHdr.Hdr.cChanges) {
		printf("looking like TSO file\n");
        }
		
	while(cur_chg < udbp->uHdr.Hdr.cChanges) {
		off = udbp->ChangeOffsetArray[cur_chg];
		sz = udbp->ChangeLengthArray[cur_chg];
		
		if (app_confp->verbose) {
			printf("DB (M)<-- off = %llu, size = %llu\n", (unsigned long long) off,
						(unsigned long long) sz);
		}

		if (app_confp->prof) {
			goto jump_profiler;
		}
		if (INM_MEM_ALIGN(bufp, sz+1)) {
			printf("err : posix mem align failed\n");
			goto exit;
		}
			
#if 0
		if (posix_memalign(&bufp, sysconf(_SC_PAGE_SIZE), sz+1)) {
			printf("err : posix mem align failed\n");
			goto exit;
		}
#endif

		if (lseek(src_fd, off, SEEK_SET) < 0) {
			printf("err : lseek failed on src vol\n");
			goto exit;
		}
		
		nr_bytes = 0;
		if ((nr_bytes = read(src_fd, bufp, sz)) < 0 ||
			(nr_bytes != sz)) {
			printf("read failed\n");
			goto exit;
		}

		if (lseek(tgt_fd, off, SEEK_SET) < 0) {
			printf("err : lseek failed on tgt vol\n");
			goto exit;
		}
		if ((nr_bytes = write(tgt_fd, bufp, sz)) &&
			(nr_bytes != sz)) {
			printf("err: write failed for tgt vol \n");
			goto exit;
		}

		free(bufp);
		bufp = NULL;
jump_profiler:
		cur_chg++;
	}
	ret = 0;
exit:
	if (bufp) {
		free(bufp);
	}
	
	INM_TRACE_END("%d", ret);

	return ret;
}

/* processing data mode stream */

int
process_dm_stream_v1(int tgt_fd, UDIRTY_BLOCK *udbp)
{
	uint64_t *len_chgsp, nr_chgs_in_strm = 0;
	int ret = -1, chg_idx = 0;
	SVD_PREFIX *svd_prefixp = NULL;
	SVD_TIME_STAMP *svd_tsp = NULL;
	uint64_t strm_len = 0;
	inm_addr_t addrp, start_addrp;

	INM_TRACE_START();

	addrp = (inm_addr_t) udbp->uHdr.Hdr.ppBufferArray;
        start_addrp = addrp;
	printf("======================================================\n");
	
	if (inm_validate_prefix(addrp, SVD_TAG_HEADER1) < 0) {
		printf("err : failed to validate SVD_TAG_HEADER1\n");
		goto exit;
	}
	addrp += (sizeof(SVD_PREFIX) + sizeof(SVD_HEADER1));

	if (inm_validate_prefix(addrp, SVD_TAG_TIME_STAMP_OF_FIRST_CHANGE)
		 < 0) {
		printf("err : failed to validate \
			SVD_TAG_TIME_STAMP_OF_FIRST_CHANGE\n");
		goto exit;
	}
	addrp += sizeof(SVD_PREFIX);

	svd_tsp = (SVD_TIME_STAMP *) addrp;

	if (app_confp->verbose) {
		printf("udb -> first ts : t = %llu, seqno = %llu\n",
			(unsigned long long)				\
			udbp->uTagList.TagList.TagTimeStampOfFirstChange. \
				TimeInHundNanoSecondsFromJan1601,
			(unsigned long long)				\
			udbp->uTagList.TagList.TagTimeStampOfFirstChange. \
				ulSequenceNumber);
	}
	
	addrp += sizeof(SVD_TIME_STAMP);
	
	svd_prefixp = (SVD_PREFIX *) addrp;
	if (svd_prefixp->tag == SVD_TAG_USER) {
		process_stream_tag_v1(&addrp);
	} else {
		if (inm_validate_prefix(addrp, 
				SVD_TAG_LENGTH_OF_DRTD_CHANGES) < 0) {
			printf("err : failed to validate \
				SVD_TAG_LENGTH_OF_DRTD_CHANGES\n");
			goto exit;
		}

		addrp += sizeof(SVD_PREFIX);
		len_chgsp = (uint64_t *)addrp;
		addrp += sizeof(uint64_t);

		if (app_confp->verbose) {
			printf("LODC --> %llu \n", 
				(unsigned long long) *len_chgsp);
		}

		printf("udb : # of chgs in cur strm = %u \n",
			udbp->uHdr.Hdr.cChanges);
		
		while (chg_idx < udbp->uHdr.Hdr.cChanges) {
			SVD_DIRTY_BLOCK *svd_dblkp = NULL;

			if (inm_validate_prefix(addrp,
					SVD_TAG_DIRTY_BLOCK_DATA) < 0) {
				printf("err: validation failed for \
					SVD_TAG_DIRTY_BLOCK_DATA\n");
				goto exit;
			}
			addrp += sizeof(SVD_PREFIX);

			svd_dblkp = (SVD_DIRTY_BLOCK *) addrp;

			if (app_confp->verbose) {
				printf("SVD DB (S)-->");
				printf("off = %15lld, sz = %5lld",
					svd_dblkp->ByteOffset,
					svd_dblkp->Length);
				printf("\n");
			}
			addrp += sizeof(SVD_DIRTY_BLOCK);
			if (app_confp->prof) {
				goto jump_profiler;
			}

			if (lseek(tgt_fd, svd_dblkp->ByteOffset, SEEK_SET)
				< 0) {
				printf("lseek failed on tgt vol\n");
				goto exit;
			}

			if (write(tgt_fd, (void *)addrp, svd_dblkp->Length)
				< 0) {
				printf("write failed for off = %llu\n",
					svd_dblkp->ByteOffset);
				goto exit;
			}
jump_profiler:
			addrp += svd_dblkp->Length;

			nr_chgs_in_strm += svd_dblkp->Length;
			chg_idx++;
		}
	}
	printf("amt of chgs in cur strm = %12llu\n", 
			(unsigned long long)nr_chgs_in_strm);
	
	svd_prefixp = (SVD_PREFIX *) addrp;
	if (inm_validate_prefix(addrp, SVD_TAG_TIME_STAMP_OF_LAST_CHANGE)
		 < 0) {
		printf("err : faild to validate tolc\n");
		goto exit;
	}

	addrp += sizeof(SVD_PREFIX);

	svd_tsp = (SVD_TIME_STAMP *) addrp;

	if (app_confp->verbose) {
		printf("TOLC ");
		printf (" t = %10llu ",
			(unsigned long long)
			svd_tsp->TimeInHundNanoSecondsFromJan1601);
		printf("seqno = %10llu \n", (unsigned long long) 
					svd_tsp->ulSequenceNumber);
	}
	addrp += sizeof(SVD_TIME_STAMP);

	strm_len = (addrp - start_addrp);
	if (strm_len != udbp->uHdr.Hdr.ulcbChangesInStream) {
		printf("strm len doesn't match with parse len \
			%d, %ld\n", udbp->uHdr.Hdr.ulcbChangesInStream,
				strm_len);
		goto exit;
	}

	ret = 0;

exit:
	INM_TRACE_END("%d", ret);

	return ret;
}

int
process_tags_v1(UDIRTY_BLOCK *udbp)
{
	PSTREAM_REC_HDR_4B tgp = NULL;
	char tg[256] = {0};
	int tg_len;
	int ret = -1;

	INM_TRACE_START();

	tgp = &udbp->uTagList.TagList.TagEndOfList;
	while (tgp->usStreamRecType == STREAM_REC_TYPE_USER_DEFINED_TAG) {
		tg_len = *(unsigned short *)((char*)tgp +
				sizeof(STREAM_REC_HDR_4B));

		if (memcpy_s(tg, tg_len, ((char*)tgp + sizeof(STREAM_REC_HDR_4B) +
				sizeof(unsigned short)), tg_len))
			return -1;

		//tgp[tg_len] = '\0';
		tgp = (PSTREAM_REC_HDR_4B) ((char *)tgp +
				GET_STREAM_LENGTH(tgp));
		printf("tag received: %s\n", tg);
		print_buf(tg, tg_len);
	}
	ret = 0;

	INM_TRACE_END("%d", ret);
	return 0;
}

/* replicate data */
 
int replicate_v1(char *src_volp, char *tgt_volp, char *lun_name)
{
	UDIRTY_BLOCK *udbp = NULL;
	COMMIT_TRANSACTION commit;
	int src_fd = -1, tgt_fd = -1, ret = -1;
	uint64_t nr_chgs_drained = 0;
	WAIT_FOR_DB_NOTIFY wait_db;

#ifdef INM_SOLARIS
    char *src_volp_raw = NULL;
    char *tgt_volp_raw = NULL;

    src_volp_raw = getfullrawname(src_volp);
    if (src_volp_raw == 0) {
        printf("err 1 : failed to get the src raw device(s)");
        return ret;
    }
    if (*src_volp_raw == '\0') {
        printf("err 2 : failed to get the src raw device(s)");
        free(src_volp_raw);
        return ret;
    }
    tgt_volp_raw = getfullrawname(tgt_volp);
    if (tgt_volp_raw == 0) {
        printf("err 1 : failed to get the tgt raw device(s)");
        free(src_volp_raw);
        return ret;
    }
    if (*tgt_volp_raw == '\0') {
        printf("err 2 : failed to get the tgt raw device(s)");
        free(src_volp_raw);
        free(tgt_volp_raw);
        return ret;
    }
#endif

	INM_TRACE_START();

	if (app_confp->prof) {
		tgt_fd = 0;
                src_fd = 0;
	} else {
#ifdef INM_LINUX
        tgt_fd = open(tgt_volp, O_WRONLY | INM_DIRECTIO);
        src_fd = open(src_volp, O_RDONLY | INM_DIRECTIO);

#endif
#ifdef INM_SOLARIS
        tgt_fd = open(src_volp_raw, O_WRONLY | INM_DIRECTIO);
        src_fd = open(tgt_volp_raw, O_RDONLY | INM_DIRECTIO);
#endif

    }
	if (src_fd < 0 || tgt_fd < 0) {
		printf("err : failed to open device(s)");
		if (src_fd < 0) {
			printf("source = %s\n", src_volp);
		}
		if (tgt_fd < 0) {
			printf("target = %s\n", tgt_volp);
		}
		goto exit;
	}
	
	udbp = (UDIRTY_BLOCK *) malloc(sizeof(*udbp));
	if (!udbp) {
		printf("malloc failed for udbp\n");
		goto exit;
	}
	if (memcpy_s(wait_db.VolumeGUID, GUID_SIZE_IN_CHARS, lun_name, GUID_SIZE_IN_CHARS))
		goto exit;

        wait_db.Seconds = 65;
	goto get_db;

wait_on:
	printf("Waiting on ...\n");
	if(inm_ioctl(app_confp->ctrl_dev_fd, IOCTL_INMAGE_WAIT_FOR_DB, &wait_db) < 0) {
                        printf("Timedout in wait for db ioctl, errno %d\n", errno);
	}
get_db:
	memset(udbp, 0, sizeof(*udbp));

	if ( inm_ioctl(app_confp->ctrl_dev_fd, IOCTL_INMAGE_GET_DIRTY_BLOCKS_TRANS,
			udbp) < 0) {
#if 0
		free(udbp);
		goto exit;
#endif
		goto wait_on;
	}

	/* for data file , the driver will nt give correct ts */
	if (!(udbp->uHdr.Hdr.ulFlags & UDIRTY_BLOCK_FLAG_DATA_FILE) &&
		(inm_validate_ts(udbp) < 0)) {
		printf("err : ts validation has failed ... exiting\n");
		goto exit;
	}

	printf("split seq id = %d\n", udbp->uHdr.Hdr.ulSequenceIDforSplitIO);
	if (udbp->uHdr.Hdr.ulFlags & UDIRTY_BLOCK_FLAG_DATA_FILE) {
		if (process_dfm_v1(tgt_fd, udbp) < 0) {
			goto exit;
		}
	} else if (udbp->uHdr.Hdr.ulFlags & UDIRTY_BLOCK_FLAG_SVD_STREAM) {
		if (process_dm_stream_v1(tgt_fd, udbp) < 0) {
			goto exit;
		}
		nr_chgs_drained += udbp->uHdr.Hdr.cChanges;
	} else {
		printf("data source = %s mode \n",
			(udbp->uTagList.TagList.TagDataSource.ulDataSource ==
			 INVOLFLT_DATA_SOURCE_META_DATA) ? "METADATA" :
							"BITMAP");

		if (process_md_v1(src_fd, tgt_fd, udbp) < 0) {
			goto exit;
		}

		inm_process_tags(udbp, NULL, NULL);
		nr_chgs_drained += udbp->uHdr.Hdr.cChanges;
	}
	
	printf("size of the last udb = %.4f MB\n", (float)udbp->uHdr.Hdr.ulicbChanges/(1024*1024));
	printf("Total changes pending the driver = %llu\n", (unsigned long long)udbp->uHdr.Hdr.ulTotalChangesPending);
	memset(&commit, 0, sizeof(commit));
        commit.ulTransactionID = udbp->uHdr.Hdr.uliTransactionID;
        if(inm_ioctl(app_confp->ctrl_dev_fd, IOCTL_INMAGE_COMMIT_DIRTY_BLOCKS_TRANS,
                        &commit) == -1) {
                printf("Commit Fail\n");
                goto exit;
        }

	ret = 0;

#ifdef DB_NOTIFY
	if (udbp->uHdr.Hdr.ulTotalChangesPending >= db_notify_threshold) {
		goto get_db;
	} else {
		goto wait_on;
	}
#else
	if (udbp->uHdr.Hdr.ulTotalChangesPending > 0) {
		goto get_db;
	} else {
		goto wait_on;
	}
#endif

exit:
#ifdef INM_SOLARIS
    if (src_volp_raw)
        free(src_volp_raw);
    if (tgt_volp_raw)
        free(tgt_volp_raw);
#endif

	if (src_fd > 0) {
		close(src_fd);
	}
	if (tgt_fd > 0) {
		close(tgt_fd);
	}

	if (udbp) {
		free(udbp);
	}

	INM_TRACE_END("%d", ret);

	return ret;
}



int
process_stream_tag_v1(inm_addr_t *addr)
{
        inm_addr_t addr1 = *addr;
        SVD_PREFIX *prefix = (SVD_PREFIX *)addr1;
        int len;
        char *buf;
	int ret = -1;

	INM_TRACE_START();

        while(prefix->tag == SVD_TAG_USER) {
                len = prefix->Flags;

                addr1 += sizeof(SVD_PREFIX);

                buf = malloc(len + 1);
                if (memcpy_s((void *)buf, len, (void *)addr1, len))
			return ret;

                buf[len] = '\0';
                printf("Recevied Tag in stream mode \n");
                print_buf(buf, len);
                free(buf);

                addr1 += len;
                prefix = (SVD_PREFIX *)addr1;
        }

        *addr = addr1;
	ret = 0;
	INM_TRACE_END("%d", ret);

        return 0;
}

int
get_db_trans_v1(char *src_volp, char *tgt_volp, int commit_required)
{
	printf("Yet to implement this functionality\n");
	return 0;
}
