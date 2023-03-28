#include "vdev_logger.h"

static inm_log_t vdev_log_t;
inm_log_t *vdev_log = &vdev_log_t;

char __logstr__[1024];
inm_mutex_t logstr_mutex;
void *vdev_log_filp;

inm_u32_t logwrap = 0;
inm_u32_t loginit = 0;

/* Syncs the data to logfile */
static void
inm_sync_to_logfile(inm_u32_t start, inm_u32_t end)
{
	char *buf;
	inm_u32_t len = (inm_u32_t)(end - start);
	inm_u32_t sync_len;
	inm_u32_t written;
	inm_u32_t offset;
	inm_u32_t tlogwrap = logwrap;

	/* We may have to sync multiple pages */
	/* Whats the max we can sync in this page */
	sync_len = ( ( INM_LOG_PAGE_IDX(start) + 1 ) * INM_LOG_PAGE_SIZE )
		   - start;

	/* if the str to copy is =< max, copy whole str */
	if( sync_len >= len )
		sync_len = len;

	/* If log wrapped, logwrap has been incremented */
	if( end > INM_LOG_SIZE )
		tlogwrap--;

	buf = INM_LOG_PTR(vdev_log, start);
	INM_WRITE_FILE(vdev_log->log_filp, buf,
		       (inm_u64_t)((tlogwrap * INM_LOG_SIZE) + start), sync_len,
		       &written);

	/* If split across pages, copy the remaining str */
	if( sync_len != len ) {
		start += sync_len;
		/*
		 * The log may need to be rotated if the first half was
		 * written in the last page
		 */
		start %= INM_LOG_SIZE;
		len -= sync_len;

		buf = INM_LOG_PTR(vdev_log, start);

		INM_WRITE_FILE(vdev_log->log_filp, buf,
			       (inm_u64_t)((logwrap * INM_LOG_SIZE) + start),
			       len, &written);
	}
}

void
inm_sync_log()
{
	inm_u32_t start;
	inm_u32_t num_bytes;

	INM_ACQ_MUTEX(&logstr_mutex);

	INM_ACQ_SPINLOCK(&vdev_log->log_spinlock);
	start = vdev_log->log_sync_start_byte;
	num_bytes = vdev_log->log_num_bytes;

	/* If log wrapped, start > num_bytes */
	if( start > num_bytes )
		num_bytes += INM_LOG_SIZE;

	/* move the sync start byte to new offset */
	vdev_log->log_sync_start_byte = vdev_log->log_num_bytes;
	INM_REL_SPINLOCK(&vdev_log->log_spinlock);

	/* sync only if required */
	if( num_bytes != start )
		inm_sync_to_logfile(start, num_bytes);

	INM_REL_MUTEX(&logstr_mutex);
}

/*
 * inm_log: 	Copies the log string from string passed in buffer(__logstr) to
 * 		actual log buffer. If an error/alert/emergency messages
 * 		are to be logged, it will sync the log file before returning.
 * 		logstr_mutex should already be held before entering.
 */
void
inm_log(inm_32_t type, char *str)
{
	char *buf;
	inm_u32_t num_bytes;
	inm_u32_t start_byte;
	inm_u32_t len = strlen(str);
	inm_u32_t copy_len;
	inm_32_t sync = 0;

	/* If log is not initialised/driver exiting, print it to console */
	if( !loginit ) {
		UPRINT("%s", str);
		INM_REL_MUTEX(&logstr_mutex);
		return;
	}

	/* Get current counters into locals and update with new values */
	INM_ACQ_SPINLOCK(&vdev_log->log_spinlock);

	num_bytes = vdev_log->log_num_bytes;

	/* The log may be rotated */
	vdev_log->log_num_bytes += len;
	vdev_log->log_num_bytes %= INM_LOG_SIZE;

	/* If we cross page boundary or if errors/alerts, sync logfile */
	if( INM_LOG_PAGE_IDX(len + num_bytes) > INM_LOG_PAGE_IDX(num_bytes) ||
	    type == INM_LOG_ERR ) {

		/* If we crossed into next page, update the last pages ptr */
		if( INM_LOG_PAGE_IDX(len + num_bytes) >
		    INM_LOG_PAGE_IDX(num_bytes) ) {
			vdev_log->log_sec_last_page = vdev_log->log_last_page;
			vdev_log->log_last_page =
			   vdev_log->log_buf[INM_LOG_PAGE_IDX(len + num_bytes)];
		}

		if( type == INM_LOG_ERR ) {
			vdev_log->log_last_err_page =
			   vdev_log->log_buf[INM_LOG_PAGE_IDX(len + num_bytes)];
		}

		start_byte = vdev_log->log_sync_start_byte;
		/* Move the sync start byte to the end of buffer */
		vdev_log->log_sync_start_byte = num_bytes + len;
		vdev_log->log_sync_start_byte %= INM_LOG_SIZE;
		sync = 1;

	}

	INM_REL_SPINLOCK(&vdev_log->log_spinlock);

	/* The str may be split across multiple buffers */
	/* Whats the max we can accomodate in this page */
	copy_len = ( ( INM_LOG_PAGE_IDX(num_bytes) + 1 ) * INM_LOG_PAGE_SIZE )
		   - num_bytes;

	/* if the str to copy is =< max, copy whole str */
	if( copy_len >= len )
		copy_len = len;

	buf = INM_LOG_PTR(vdev_log, num_bytes);
	memcpy_s(buf, copy_len, str, copy_len);

	/* If split across pages, copy the remaining str
	 *
	 * The log may need to be rotated if the first half was
	 * written in the last page
	 */

	if( copy_len != len ) {
#ifndef _INM_LOG_WRAP_
	if( (num_bytes + copy_len) >= INM_LOG_SIZE )
		logwrap++;
#endif
		buf = INM_LOG_PTR(vdev_log,
				  (num_bytes + copy_len) % INM_LOG_SIZE );
		memcpy_s(buf, len - copy_len, str + copy_len, len - copy_len);
	}

	/* If we need to sync, first acq sync mutex */
	if( sync )
		inm_sync_to_logfile(start_byte, num_bytes + len);

	INM_REL_MUTEX(&logstr_mutex);
}

inm_32_t
inm_vdev_init_logger()
{
	inm_32_t retval = 0;
	inm_32_t i = 0;

	DBG("Initialising logger at %p", vdev_log);

	for( i = 0; i < INM_LOG_NUM_PAGES; i++ ) {
		vdev_log->log_buf[i] = INM_ALLOC(INM_LOG_PAGE_SIZE, GFP_KERNEL);
		if( !vdev_log->log_buf[i] ) {
			ERR("Cannot alocate memory for logger");
			retval = -ENOMEM;
			goto buf_alloc_failed;
		}
		DBG("Page %d = %p", vdev_log->log_buf[i]);
	}

	vdev_log->log_last_page = vdev_log->log_buf[0];
	vdev_log->log_sec_last_page = NULL;
	vdev_log->log_last_err_page = NULL;

	retval = INM_OPEN_FILE(INM_VDEV_LOGFILE, INM_RDWR | INM_CREAT,
				      &vdev_log->log_filp);
	if( !vdev_log->log_filp ) {
		ERR("Cannot create/open log file %s", INM_VDEV_LOGFILE);
		goto open_file_failed;
	}

	vdev_log_filp = vdev_log->log_filp;

	INM_INIT_SPINLOCK(&vdev_log->log_spinlock);
	INM_INIT_MUTEX(&logstr_mutex);

	vdev_log->log_num_bytes = 0;
	vdev_log->log_sync_start_byte = 0;

	loginit = 1;

	INFO("Initialised logger at %p", vdev_log);

out:
	return retval;

open_file_failed:
buf_alloc_failed:
	for( i = 0; i < INM_LOG_NUM_PAGES; i++ ) {
		/* On encountering first NULL, get out as thats where we
		 * failed, so next values are invalid
		 */
		if( !vdev_log->log_buf[i] )
			break;

		INM_FREE(&(vdev_log->log_buf[i]), INM_LOG_PAGE_SIZE);
	}
	goto out;
}

void
inm_vdev_fini_logger()
{
	inm_32_t i = 0;

	loginit = 0;

	DBG("Switching off logger");
	inm_sync_log();

	INM_CLOSE_FILE(vdev_log->log_filp, INM_DIRECT | INM_RDWR | INM_CREAT);

	for( i = 0; i < INM_LOG_NUM_PAGES; i++ )
		INM_FREE(&(vdev_log->log_buf[i]), INM_LOG_PAGE_SIZE);

	INM_DESTROY_SPINLOCK(&vdev_log->log_spinlock);
}

