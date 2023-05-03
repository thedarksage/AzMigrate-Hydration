

/* inm_validate_ts : validates time stamp in udb */
int
inm_validate_ts(void *argp)
{
	int ret = -1;

	INM_TRACE_START();

	if (app_confp->ver == VERSION_V1) {
		ret = validate_ts_v1((UDIRTY_BLOCK *)argp);
	} else if (app_confp->ver == VERSION_V2) {
		ret = validate_ts_v2((UDIRTY_BLOCK_V2 *) argp);
	} else if (app_confp->ver == VERSION_V3) {
		ret = validate_ts_v3((UDIRTY_BLOCK_V3 *) argp);
	}

	INM_TRACE_END("%d", ret);
	return ret;
}

int
inm_replicate(char *src_volp, char *tgt_volp)
{
	int ret = -1;

	INM_TRACE_START();

	if (app_confp->ver == VERSION_V1) {
		ret = replicate_v1(char *src_volp, char *tgt_volp);
	} else if (app_confp->ver == VERSION_V2) {
		ret = replicate_v2(char *src_volp, char *tgt_volp);
	} else if (app_confp->ver == VERSION_V3) {
		ret = replicate_v3(char *src_volp, char *tgt_volp);
	}

	INM_TRACE_END("%d", ret);
	return ret;
}
	

int
inm_process_tags(void *argp, char **tag, int *tag_len)
{
	int ret = -1;

	INM_TRACE_START();

	if (app_confp->ver == VERSION_V1) {
		ret = process_tags_v1((UDIRTY_BLOCK *)argp);
	} else if (app_confp->ver == VERSION_V2) {
		ret = process_tags_v2((UDIRTY_BLOCK_V2 *) argp, tag, tag_len);
	} else if (app_confp->ver == VERSION_V3) {
		ret = process_tags_v3((UDIRTY_BLOCK_V3 *) argp);
	}

	INM_TRACE_END("%d", ret);
	return ret;
}
/*
int
inm_apply_dblk(int tgt_fd, void *argp, void *datap)
{
	int ret = -1;

	INM_TRACE_START();

	if (app_confp->ver == VERSION_V1) {
		ret = apply_dblk_v1(tgt_fd, (SVD_DIRTY_BLOCK *)argp,
					datap);
	} else if (app_confp->ver == VERSION_V2) {
		ret = apply_dblk_v2(tgt_fd, (SVD_DIRTY_BLOCK_V2 *)argp,
					datap);
	} else if (app_confp->ver == VERSION_V3) {
		ret = apply_dblk_v3(tgt_fd, (SVD_DIRTY_BLOCK_V3 *) argp,
					 datap);
	}

	INM_TRACE_END("%d", ret);
	return ret;
}
*/
int
inm_process_dm_stream(int tgt_fd, void *argp)
{
        int ret = -1;

        INM_TRACE_START();

        if (app_confp->ver == VERSION_V1) {
                ret = process_dm_stream_v1(tgt_fd, (UDIRTY_BLOCK *) argp);
        } else if (app_confp->ver == VERSION_V2) {
                ret = process_dm_stream_v2(tgt_fd,
						 (UDIRTY_BLOCK_V2 *) argp);
        } else if (app_confp->ver == VERSION_V3) {
                ret = process_dm_streamv3(tgt_fd,
						 (UDIRTY_BLOCK_V3 *) argp);
        }

        INM_TRACE_END("%d", ret);
        return ret;
}

int
inm_process_dfm(int tgt_fd, void *argp)
{
        int ret = -1;

        INM_TRACE_START();

        if (app_confp->ver == VERSION_V1) {
                ret = process_dfm_v1(tgt_fd, (UDIRTY_BLOCK *) argp,
					datap);
        } else if (app_confp->ver == VERSION_V2) {
                ret = process_dfm_v2(tgt_fd, (UDIRTY_BLOCK_V2 *) argp,
					datap);
        } else if (app_confp->ver == VERSION_V3) {
                ret = process_dfm_v3(tgt_fd, (UDIRTY_BLOCK_V3 *) argp,
					datap);
        }

        INM_TRACE_END("%d", ret);
        return ret;
}

int
inm_process_md(int src_fd, int tgt_fd, void *argp)
{
        int ret = -1;

        INM_TRACE_START();

        if (app_confp->ver == VERSION_V1) {
                ret = process_md_v1(src_fd, tgt_fd, (UDIRTY_BLOCK *) argp);
        } else if (app_confp->ver == VERSION_V2) {
                ret = process_md_v2(src_fd, tgt_fd,
					 (UDIRTY_BLOCK_V2 *) argp);
        } else if (app_confp->ver == VERSION_V3) {
                ret = process_md_v3(src_fd, tgt_fd,
					 (UDIRTY_BLOCK_V3 *) argp);
        }

        INM_TRACE_END("%d", ret);
        return ret;
}


