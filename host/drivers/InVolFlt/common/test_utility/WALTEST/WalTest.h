#ifndef __WalTest_h_
#define __WalTest_h

#include <pthread.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <asm/fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <sys/mount.h>
#include <linux/fs.h>

#define MAX_DISK_NAME_LENGTH 256
#define MAX_PATH_LEN         2048
#define GUID_SIZE_IN_CHARS   128
#define DEF_SIZE_OF_SECTOR_INBYTE 512
#define DEF_SIZE_OF_PATTERN_BYTE 8 
#define BASE_DIR "/var/wallog"
#define DIR_LOG  "/dir_log_"
#define TAG_FILE_NAME_PREFIX "tag"
#define PREFIX_LEN            3
#define DEBUG_LOG_FILE "/var/wallog/waltest_dbg.log"
#define WAL_TEST_RESULT_FILE "/var/wallog/waltest_result.log"
#define SERVICE_SLEEP_INTERVAL 10
#define DEFAULT_DB_NOTIFY_THRESHOLD     (16*1024*1024)  //16M
#define DEFAULT_INM_PATH                "/usr/local/ASR/Vx/bin/inm_dmit"

#define PKILL_CMD                       "/usr/bin/pkill"
#define KILLALL_CMD                     "/usr/bin/killall"
#define INM_DMIT                        "inm_dmit"

typedef unsigned long    inm_addr_t;

typedef struct disk_list {
    uint64_t last_ts;
    uint64_t last_seqno;
    uint64_t log_file_seqno;
    unsigned long long last_disk_offset;
    char disk_name[MAX_DISK_NAME_LENGTH];
    char disk_pname[MAX_DISK_NAME_LENGTH];

}disk_list;

void usage (void);
int launch_replication_thread(void);
void *repl_op(void *threadid);
int launch_writeorder_thread(void);
void launch_service_start_thread(void);
void stop_process_start_notify(void);
void issue_process_start_notify(void);

#define WAL_TRACE_START()       \
                fprintf(dbgfd, "\n%s : %d --> entered\n", __FUNCTION__, __LINE__)

#define WAL_TRACE_END()       \
                fprintf(dbgfd, "\n%s : %d --> end\n", __FUNCTION__, __LINE__)


#endif
