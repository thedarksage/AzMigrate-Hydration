#ifndef _INM_FILTER_H
#define _INM_FILTER_H

#ident "@(#) $Source $Revision: 1.1.2.1 $ $Name: INT_BRANCH $ $Revision: 1.1.2.1 $"

#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include "inmtypes.h"
#include "involflt.h"
#include "osdep.h"
#include "safecapismajor.h"

/* global macros */
#define INM_CTRL_DEV 	"/dev/involflt"

struct str_idx {
        const char *key_name;
        const int code;
};

#ifdef INM_LINUX
#define INM_DIRECTIO      (O_DIRECT)
#endif

#ifdef INM_SOLARIS
#define INM_DIRECTIO      (O_SYNC|O_RSYNC)
#endif

#ifdef INM_AIX
#define INM_DIRECTIO      (O_DIRECT)
#endif

enum opcodes {
	STACK_OP 		= 1,
	START_FLT_OP		= 2,
	UNSTACK_OP		= 3,
	STOP_FLT_OP		= 4,
	GET_TIME_OP		= 5,
	REPLICATE_OP		= 6,
	GET_DB_OP		= 7,
	GET_DB_COMMIT_OP	= 8,
	CLEAR_DIFFS_OP		= 9,
	GET_DB_THRES_OP		= 10,
	PRINT_IOCTL_CODES_OP	= 11,
	PRINT_TAG_CODES_OP	= 12,
	GET_DRIVER_VER_OP	= 13,
	RESYNC_START_OP		= 14,
	RESYNC_END_OP		= 15,
	SYS_SHUTDOWN_OP		= 16,
	ISSUE_TAG_OP		= 17,
	SERVICE_START_OP	= 18,
	START_NOTIFY_OP		= 19,
	LUN_CREATE_OP		= 20,
	LUN_DELETE_OP		= 21,	
	UNSTACK_ALL_OP		= 22,
	BOOTTIME_STACKING_OP    = 23,
	START_MIRROR_OP		= 24,
	STOP_MIRROR_OP		= 25,
	STACK_MIRROR_OP		= 26,
	DMESG_OP		= 27,
	SET_VERBOSITY_OP	= 28,
	TEST_HEARTBEAT_OP       = 29,
	START_LOCAL_MIRROR_OP	= 30,
	STOP_LOCAL_MIRROR_OP	= 31,
	ADDITIONAL_STATS_OP     = 32,
	LATENCY_STATS_OP        = 33,
	BMAP_STATS_OP           = 34,
	BLOCK_AT_LUN_OP		= 35,
	UNBLOCK_AT_LUN_OP	= 36,
	INIT_DRIVER_FULLY_OP = 37,
	FREEZE_VOL_OP           = 38,
	THAW_VOL_OP             = 39,
	MONITORING_STATS_OP     = 40,
        GET_BLK_MQ_STATUS_OP    = 41,
        GET_VOLUME_STATS_V2_OP  = 42,
    GET_NAME_MAPPING_OP     = 43,
    LCW_OP                  = 44,
    GET_CX_STATUS_OP        = 45,
    WAKEUP_MONITOR_THREAD   = 46,
    TAG_DRAIN_NOTIFY_OP     = 47,
    WAKEUP_TAG_DRAIN_NOTIFY = 48,
    MODIFY_PERSISTENT_DEVICE_NAME_OP = 49,
    BLOCK_DRAIN_OP          = 50,
    UNBLOCK_DRAIN_OP        = 51,
    GET_DRAIN_STATE_OP      = 52,
    CREATE_BARRIER_OP       = 53,
    REMOVE_BARRIER_OP       = 54,
    STOP_FLT_ALL_OP         = 55,
    MAX_NR_OPS              = 56
};

/* structure for configuration*/
struct config {
	/* debug params */
	int 		verbose;
	int 		trace;
	int 		dbg_lvl;

	/* global variables */
	int 		ctrl_dev_fd;

	/* application params*/
	int 		ver;
	enum opcodes 	op_code;
	int 		resync;
	int 		prof;
	char *		src_volp;
	char *		tgt_volp;
	char *          mnt_pt;
	char *          dev_type;
	char *          tag_name;
	char *          fsync_file_name;
	int		stat;
	int		attribute;
	char *		lun_name;	
	long long 	lun_size;
	long long 	lun_startoff;
	long long	flags;
	int		vendor;
	mirror_conf_info_t mconf;
	long long	delay;
	int		drv_verbose;
	int     timeout;
	int     nr_vols;
    char    *tag_notify;
    char    *tag_pending_notify;
    char    *pname;
    char    *fname;
    int     sub_opcode;
    char    *pnames;
    int     nr_pnames;
    int     fail_commit_db;
};

int init_config(void);
int open_ctrl_dev(void);




/* trace api */
#define INM_TRACE_START()	\
	if (app_confp->trace) {	\
		printf("\n%s : %d --> entered\n", __FUNCTION__, __LINE__); \
	}

#define INM_TRACE_END(FMT_STR, VALUE)		\
	if (app_confp->trace) {			\
		printf("\n%s : %d <-- end", __FUNCTION__, __LINE__); 	\
		printf("[ ret = " FMT_STR, VALUE);		 \
		printf("] \n");					\
	}

#define min(a, b)	(((a) < (b))?(a):(b))

void print_buf(char *bufp , int len);
int inm_start_filtering(char **argv);
int inm_process_cmd(int argc, char **argv, int start_idx);
char *convert_path_to_str(char *path); 
int inm_read_prefix(int fd, int tag);
int inm_read_data(int fd, int len, void *datap);
int inm_validate_ts(void *argp);
int inm_replicate(char *src_volp, char *tgt_volp, char *lun_name);
int inm_process_tags(void *argp, char **tag, int *tag_len);
int inm_apply_dblk(int tgt_fd, void *argp, void *datap);
int inm_process_dm_stream(int tgt_fd, void *argp);
int inm_process_dfm(int tgt_fd, void *argp);
int inm_process_md(int src_fd, int tgt_fd, void *argp);
int inm_validate_prefix(inm_addr_t addr, int tag);
int inm_process_tags(void *argp, char **tag, int *tag_len);
int inm_ioctl(int fd, int ioctl_code, void *argp);


/* macros for version info */
enum ver_codes {
	VERSION_V1 = 1,
	VERSION_V2 = 2,
	VERSION_V3 = 3,
	MAX_NR_VERS= 3,
};

#define DRIVER_STAT	1
#define TARGET_STAT	2
#define PROT_VOL_LIST	3

#define GET_ATTR	1
#define SET_ATTR	2

#define INM_MIN(a,b)	(((a) < (b)) ? (a) : (b))

#endif
