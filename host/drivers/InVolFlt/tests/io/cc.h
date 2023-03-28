#ifndef __WalTest_h_
#define __WalTest_h

/*
#include <dirent.h>
#include <sys/mount.h>
#include <linux/fs.h>
#include <signal.h>
*/

#include "libinclude.h"

#define INM_CTRL_DEV                    "/dev/involflt"
#define MAX_DISK_NAME_LENGTH            256
#define MAX_PATH_LEN                    2048
#define GUID_SIZE_IN_CHARS              128
#define DEF_SIZE_OF_SECTOR_INBYTE       512
#define DEF_SIZE_OF_PATTERN_BYTE        8 
#define BASE_DIR                        "/var/log/wal"
#define DIR_LOG                         "/dir_log_"
#define SERVICE_SLEEP_INTERVAL          100
#define DEFAULT_DB_NOTIFY_THRESHOLD     (16*1024*1024)  //16M
#define DEFAULT_INM_PATH                "/usr/local/ASR/Vx/bin/inm_dmit"
#define MAX_DISK                        8

typedef unsigned long    inm_addr_t;

typedef struct disk_list {
    uint64_t last_ts;
    uint64_t last_seqno;
    uint64_t log_file_seqno;
    unsigned long long last_disk_offset;
    char *disk_name;
    char *disk_pname;

}disk_list;

int debug=0;
#define dbg(format, arg...)                             \
    do {                                                \
        if (debug) {                                    \
            fprintf(stderr, "DBG:" format "\n", ##arg); \
        }                                               \
    } while(0)

#define err(format, arg...)                             \
    fprintf(stderr, "ERR:" format "\n", ##arg) 

#define info(format, arg...)                             \
    fprintf(stderr, "INFO:" format "\n", ##arg) 

#endif
