
#ident "@(#) $Source $Revision: 1.1.2.1 $ $Name: INT_BRANCH $ $Revision: 1.1.2.1 $"

#include "inm_filter.h"
#include "involflt.h"
#include "svdparse.h"
#include "inm_subr_v1.h"
#include "inm_subr_v3.h"
#include "inm_subr_v3.h"

extern struct config *app_confp;
extern unsigned int db_notify_threshold;
/* validates the time stamp in udirty block */
int
validate_ts_v3(UDIRTY_BLOCK_V3 *udbp)
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
			ullSequenceNumber;

	e_time_in_nsec = udbp->uTagList.TagList.TagTimeStampOfLastChange. \
			TimeInHundNanoSecondsFromJan1601;

	e_seqno = udbp->uTagList.TagList.TagTimeStampOfLastChange. \
			ullSequenceNumber;

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
apply_dblk_v3(int tgt_fd, SVD_DIRTY_BLOCK_V2 *dblkp, void *datap)
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
process_dfm_v3(int tgt_fd, UDIRTY_BLOCK_V3 *udbp)
{
	int ret = -1, df_fd;
	SVD_HEADER1 svd_hdr;
	SVD_TIME_STAMP_V2 svd_tofc, svd_tolc;
	void *datap = NULL;
	long long nr_drtd_chgs = 0;
	char *dfnamep = NULL;
	SVD_DIRTY_BLOCK_V2 svd_dblk;

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
		
		datap = malloc(svd_dblk.Length);
		if (!datap) {
			printf("err : malloc failed\n");
			break;
		}
		printf("off = %15lld, size = %5u\n", svd_dblk.ByteOffset,
			svd_dblk.Length);

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
	if (df_fd > 0) {
		close(df_fd);
	}

	INM_TRACE_END("%d", ret);

	return ret;
}

/* processing meta data changes */
int
process_md_v3(int src_fd, int tgt_fd, UDIRTY_BLOCK_V3 *udbp)
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
				ullSequenceNumber);

	printf("tolc = %llu, e_seqno = %llu\n", 
		(unsigned long long)				\
		udbp->uTagList.TagList.TagTimeStampOfLastChange. \
				TimeInHundNanoSecondsFromJan1601,
		(unsigned long long)				\
		udbp->uTagList.TagList.TagTimeStampOfLastChange. \
				ullSequenceNumber);

        if(udbp->uHdr.Hdr.ulFlags & UDIRTY_BLOCK_FLAG_TSO_FILE)
                printf("TSO file\n");

	if (!udbp->uHdr.Hdr.cChanges) {
		printf("looking like TSO file\n");
        }
		
	while(cur_chg < udbp->uHdr.Hdr.cChanges) {
		off = udbp->ChangeOffsetArray[cur_chg];
		sz = udbp->ChangeLengthArray[cur_chg];
		
		printf("off = %llu, size = %llu\n", (unsigned long long) off,
						(unsigned long long) sz);

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
process_dm_stream_v3(int tgt_fd, UDIRTY_BLOCK_V3 *udbp)
{
	uint64_t *len_chgsp, nr_chgs_in_strm = 0;
	int ret = -1, chg_idx = 0;
	SVD_PREFIX *svd_prefixp = NULL;
	SVD_TIME_STAMP_V2 *svd_tsp = NULL;
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

	if (inm_validate_prefix(addrp, SVD_TAG_TIME_STAMP_OF_FIRST_CHANGE_V2)
		 < 0) {
		printf("err : failed to validate \
			SVD_TAG_TIME_STAMP_OF_FIRST_CHANGE_V2\n");
		goto exit;
	}
	addrp += sizeof(SVD_PREFIX);

	svd_tsp = (SVD_TIME_STAMP_V2 *) addrp;

	if (app_confp->verbose) {
		printf("udb -> first ts : t = %llu, seqno = %llu\n",
			(unsigned long long)				\
			udbp->uTagList.TagList.TagTimeStampOfFirstChange. \
				TimeInHundNanoSecondsFromJan1601,
			(unsigned long long)				\
			udbp->uTagList.TagList.TagTimeStampOfFirstChange. \
				ullSequenceNumber);
	}
	
	addrp += sizeof(SVD_TIME_STAMP_V2);
	
	svd_prefixp = (SVD_PREFIX *) addrp;
	if (svd_prefixp->tag == SVD_TAG_USER) {
		process_stream_tag_v3(&addrp);
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
			SVD_DIRTY_BLOCK_V2 *svd_dblkp = NULL;

			if (inm_validate_prefix(addrp,
					SVD_TAG_DIRTY_BLOCK_DATA_V2) < 0) {
				printf("err: validation failed for \
					SVD_TAG_DIRTY_BLOCK_DATA_V2\n");
				goto exit;
			}
			addrp += sizeof(SVD_PREFIX);

			svd_dblkp = (SVD_DIRTY_BLOCK_V2 *) addrp;

			if (app_confp->verbose) {
				printf("SVD DB ->");
				printf("off = %15lld, sz = %5u, td = %u, sd = %u",
					svd_dblkp->ByteOffset,
					svd_dblkp->Length,
					svd_dblkp->uiTimeDelta,
					svd_dblkp->uiSequenceNumberDelta);
				printf("\n");
			}
			addrp += sizeof(SVD_DIRTY_BLOCK_V2);
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
	if (inm_validate_prefix(addrp, SVD_TAG_TIME_STAMP_OF_LAST_CHANGE_V2)
		 < 0) {
		printf("err : faild to validate tolc\n");
		goto exit;
	}

	addrp += sizeof(SVD_PREFIX);

	svd_tsp = (SVD_TIME_STAMP_V2 *) addrp;

	if (app_confp->verbose) {
		printf("TOLC ");
		printf (" t = %10llu ",
			(unsigned long long)
			svd_tsp->TimeInHundNanoSecondsFromJan1601);
		printf("seqno = %10llu \n", (unsigned long long) 
					svd_tsp->ullSequenceNumber);
	}
	addrp += sizeof(SVD_TIME_STAMP_V2);

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
process_tags_v3(UDIRTY_BLOCK_V3 *udbp)
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
			return ret;

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
 
int replicate_v3(char *src_volp, char *tgt_volp, char *lun_name)
{
	UDIRTY_BLOCK_V3 *udbp = NULL;
	COMMIT_TRANSACTION commit;
	int src_fd = -1, tgt_fd = -1, ret = -1;
	uint64_t nr_chgs_drained = 0;
	WAIT_FOR_DB_NOTIFY wait_db;


	INM_TRACE_START();

	src_fd = open(src_volp, O_RDONLY);
	if (app_confp->prof) {
		tgt_fd = 0;
	} else {
		tgt_fd = open(tgt_volp, O_WRONLY);
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
	
	udbp = (UDIRTY_BLOCK_V3 *) malloc(sizeof(*udbp));
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
		if (process_dfm_v3(tgt_fd, udbp) < 0) {
			goto exit;
		}
	} else if (udbp->uHdr.Hdr.ulFlags & UDIRTY_BLOCK_FLAG_SVD_STREAM) {
		if (process_dm_stream_v3(tgt_fd, udbp) < 0) {
			goto exit;
		}
		nr_chgs_drained += udbp->uHdr.Hdr.cChanges;
	} else {
		printf("data source = %s mode \n",
			(udbp->uTagList.TagList.TagDataSource.ulDataSource ==
			 INVOLFLT_DATA_SOURCE_META_DATA) ? "METADATA" :
							"BITMAP");

		if (process_md_v3(src_fd, tgt_fd, udbp) < 0) {
			goto exit;
		}

		inm_process_tags(udbp, NULL, NULL);
		nr_chgs_drained += udbp->uHdr.Hdr.cChanges;
	}
	
	memset(&commit, 0, sizeof(commit));
        commit.ulTransactionID = udbp->uHdr.Hdr.uliTransactionID;
        if(inm_ioctl(app_confp->ctrl_dev_fd, IOCTL_INMAGE_COMMIT_DIRTY_BLOCKS_TRANS,
                        &commit) == -1) {
                printf("Commit Fail\n");
                goto exit;
        }


#ifdef DB_NOTIFY
	if (udbp->uHdr.Hdr.ulTotalChangesPending >= db_notify_threshold) {
		goto get_db;
	}
#else
	if (udbp->uHdr.Hdr.ulTotalChangesPending > 0) {
		goto get_db;
	} else {
		goto wait_on;
	}
#endif
	ret = 0;

exit:

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
process_stream_tag_v3(inm_addr_t *addr)
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
get_db_trans_v3(char *src_volp, char *tgt_volp, int commit_required)
{
        printf("Yet to implement this functionality\n");
        return 0;
}

