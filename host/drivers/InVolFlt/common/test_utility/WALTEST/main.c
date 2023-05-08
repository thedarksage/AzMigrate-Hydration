#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "WalTest.h"
#include "verifier.h"
#include "safecapismajor.h"

unsigned long g_nr_disk = 0;
unsigned long g_tag_timeout = 0;
unsigned long g_service_timeout = 0;
unsigned long long g_com_min_disk_offset_all_disk = 0;
unsigned long g_pattern_n_n_n;
unsigned long g_pattern_n_n_n_1;
unsigned long g_pattern_n_n_1_n_1;
unsigned long g_sec_sz_inbyte; 
disk_list *g_disk_listp = NULL;

unsigned long g_start_verifier_thread = 0;

FILE *dbgfd;
FILE *wfd;
extern void stop_filtering_list_of_devices();

int main(int argc, char *argv[]) {
    int ret = 0;
    int numdisk;
    int optindx;
    char *src_diskp;
    char *disk_name;
    char *disk_pname;
    char cmd_buf[MAX_PATH_LEN];

    while((optindx = getopt(argc, argv, "n:w:s:p:t:r:")) != -1) {

        switch (optindx) {

            case 'n':
                g_nr_disk = strtoul(optarg, NULL, 10);
                if ( g_nr_disk < 1 || g_nr_disk > 256) {
                    printf("number fo disks in range of 1 to 256 is supported.\n");
                    usage();
                    ret = -1;
                    goto exit;
                }
                break;

            case 'w':
                g_service_timeout = strtoul(optarg, NULL, 10);
                break;

            case 's':
                src_diskp = (char*) optarg;
                g_disk_listp = (disk_list*) calloc(g_nr_disk, sizeof(disk_list));
                if (!g_disk_listp) {
                    printf("error: failed to allocate memory\n");
                    ret = -1;
                    goto exit;
                }
                numdisk = 0;
                disk_name = strtok(src_diskp, ", ");
                do {
                    strcpy_s(g_disk_listp[numdisk].disk_name, sizeof(g_disk_listp[numdisk].disk_name), disk_name);
                    numdisk++;
                }while((disk_name = strtok(NULL,", ")));
                if (numdisk == g_nr_disk ) {
                    printf("List of disks participating for WAL Test:\n");
                    for (numdisk = 0; numdisk < g_nr_disk; numdisk++) {
                        printf("%s\n",g_disk_listp[numdisk].disk_name);
                    }
                } else {
                    printf("Provided disk list is not equal to provided number of disks\n");
                    usage();
                    goto exit;
                }
                break;
            case 'p':
                src_diskp = (char*) optarg;
                numdisk = 0;
                disk_pname = strtok(src_diskp, ", ");
                do {
                    strcpy_s(g_disk_listp[numdisk].disk_pname, sizeof(g_disk_listp[numdisk].disk_pname), disk_pname);
                    numdisk++;
                }while((disk_pname = strtok(NULL,", ")));
                if (numdisk == g_nr_disk ) {
                    printf("List of disks participating for WAL Test:\n");
                    for (numdisk = 0; numdisk < g_nr_disk; numdisk++) {
                        printf("%s\n",g_disk_listp[numdisk].disk_pname);
                    }
                } else {
                    printf("Provided disk pname list is not equal to provided number of disks\n");
                    usage();
                    goto exit;
                }
                break;

            case 't':
                g_tag_timeout = strtoul(optarg, NULL, 10);
                break;

            case 'r':
                g_sec_sz_inbyte = strtoul(optarg, NULL, 10);
                break;

            default:
                usage();
                ret = -1;
                goto exit;
        }
    }

    if ( g_service_timeout < g_tag_timeout ) {
        printf("Timeout to complete WAL test is less than the timeout of issue tag!\n");
        usage();
        goto exit;
    }

    if (!g_disk_listp || !g_nr_disk || !g_tag_timeout || !g_service_timeout) {
        usage();
        goto exit;
    }

    if (!g_sec_sz_inbyte) {
        g_sec_sz_inbyte = DEF_SIZE_OF_SECTOR_INBYTE;
    }

    cmd_buf[0] = '\0';
    snprintf(cmd_buf, MAX_PATH_LEN, "rm -rf %s", BASE_DIR);
    system(cmd_buf);
    cmd_buf[0] = '\0';
    snprintf(cmd_buf, MAX_PATH_LEN, "mkdir -p %s", BASE_DIR);
    system(cmd_buf);

    dbgfd = fopen(DEBUG_LOG_FILE, "w+");
    if (dbgfd == NULL) {
         printf("error: failed to open file [%s]\n", DEBUG_LOG_FILE);
         ret = -1;
         goto exit;
    }

    wfd = fopen(WAL_TEST_RESULT_FILE, "w+");
    if (wfd == NULL) {
        printf("error: failed to open file [%s]\n", WAL_TEST_RESULT_FILE);
        ret = -1;
        goto exit;
    }

    printf("Start running WAL test .....\n");
    fprintf(dbgfd, "Start running WAL test using sctor size %lu bytes.....\n", g_sec_sz_inbyte);
    fflush(dbgfd);
    fprintf(wfd, "Start running WAL test using sector size %lu bytes.....\n", g_sec_sz_inbyte);
    fflush(wfd);

    launch_service_start_thread();

    issue_process_start_notify();

    launch_verifier_thread();

    ret = launch_replication_thread();
    if (ret) {
        fprintf(dbgfd,"error: failed to launch replication threads\n");
        ret = -1;
    }

    stop_filtering_list_of_devices();

    stop_process_start_notify();

    printf("WAL test completed! please check log file [%s] for the result\n", WAL_TEST_RESULT_FILE);
    printf("please check log file [%s] for detailed log information\n", DEBUG_LOG_FILE);
    fprintf(dbgfd, "WAL test completed!\n");
    fflush(dbgfd);
    fprintf(wfd, "\nstat of bucket count for each pattern:\n");
    fprintf(wfd, "total number of times for occurrence of pattern n_n_n : %lu\n", g_pattern_n_n_n);
    fprintf(wfd, "total number of times for occurrence of  pattern n_n_n_1: %lu\n", g_pattern_n_n_n_1);
    fprintf(wfd, "total number of times for occurrence of pattern n_n_1_n_1: %lu\n", g_pattern_n_n_1_n_1);
    fprintf(wfd, "\nWAL test completed!\n");
    fflush(wfd);

exit:
    if (g_disk_listp) {
        free(g_disk_listp);
        g_disk_listp = NULL;
    }
    if (dbgfd) {
        fclose(dbgfd);
    }
    if (wfd) {
        fclose(wfd);
    }

    return ret;
}

void usage(void) {
    printf("Usage:\n");
    printf("WalTest -n <number of disks> -t <timeout to issue tag in seconds> -w <timeout to end WAL test in seconds> -s <list of disks> [-r sector size]\n");
    printf("\n\nOptions:\n");
    printf("  -r=<bytes>   sector size in bytes [default: 512]\n");
}

