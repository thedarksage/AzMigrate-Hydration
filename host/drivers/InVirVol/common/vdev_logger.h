#ifndef _INMAGE_LOGGER_H
#define _INMAGE_LOGGER_H

#ifdef _INM_LOGGER_

#include "common.h"
#include "vsnap_kernel_helpers.h"

/* This logger is not intended to be used in interrupt env */

#define INM_VDEV_LOGFILE "/var/log/linvsnap.log"

#define INM_LOG_SIZE		(64*1024)
#define INM_LOG_PAGE_SIZE	INM_PAGE_SIZE
#define INM_LOG_NUM_PAGES	( INM_LOG_SIZE / INM_LOG_PAGE_SIZE )

/* Log priorities */
/* Errors are synchronously logged. For other messages which need to be sync.
 * written, put it at par with INM_LOG_ERROR
 */
#define INM_LOG_ERR	0
#define INM_LOG_INFO	0
#define INM_LOG_DBG	0
#define INM_LOG_DEV_DBG	1

/* Following macros help in using multiple pages as single contiguos array */
/* Gives the index in page array for a given byte */
#define INM_LOG_PAGE_IDX(byte)	( byte >> 12 )
/* Gives index within the page for a given byte */
#define INM_LOG_BYTE_IDX(byte)	( byte & 0xfff )
/* Gives pointer to address which corresponds to byte in contiguous buffer */
#define INM_LOG_PTR(log_t, byte) \
	(( log_t->log_buf[INM_LOG_PAGE_IDX(byte)] ) + INM_LOG_BYTE_IDX(byte))

extern char __logstr__[1024];
extern inm_mutex_t logstr_mutex;

typedef struct inm_log {
	char		*log_last_page;		     /* Last & latest page */
	char		*log_sec_last_page;	     /* Second last page */
	char		*log_last_err_page;
	inm_u32_t	log_num_bytes;		     /* num bytes in use */
	char 		*log_buf[INM_LOG_NUM_PAGES]; /* memory buffer pages */
	void		*log_filp;		     /* log file pointer */
	inm_u32_t	log_sync_start_byte;	     /* sync data from */
	inm_spinlock_t	log_spinlock; 		     /* for log counters */
} inm_log_t;

/* prototypes */
void inm_sync_log(void);
void inm_log(inm_32_t , char *);
inm_32_t inm_init_vdev_logger(void);
void inm_fini_vdev_logger(void);

#endif

#endif

