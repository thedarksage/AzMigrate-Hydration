#include "WalTest.h"
#include "verifier.h"

extern unsigned long g_nr_disk;
extern disk_list *g_disk_listp;

extern unsigned long g_start_verifier_thread;
extern unsigned long g_pattern_n_n_n;
extern unsigned long g_pattern_n_n_n_1;
extern unsigned long g_pattern_n_n_1_n_1;
extern unsigned long g_sec_sz_inbyte;

extern FILE *dbgfd;
extern FILE *wfd;

unsigned long g_dir_seq_no = 1;

pthread_mutex_t verifier_mutex;
pthread_cond_t  verifier_cond_wait;

int veirfy_tag_accross_disks(void)
{
    int ret = 0;
    char src_dir_name[MAX_PATH_LEN];
    char path[MAX_PATH_LEN];
    int no_of_tag = 0;
    FILE *fp;
    int disk_index;
    char ls_cmd[MAX_PATH_LEN];

    WAL_TRACE_START();

    fprintf(dbgfd,"\nfirst check if for iteration log_[%lu] last file of each device has a tag\n", g_dir_seq_no);

    for ( disk_index = 0; disk_index < g_nr_disk; disk_index++) {

        src_dir_name[0] = '\0';
        snprintf(src_dir_name, MAX_PATH_LEN, "%s%s%s%lu", BASE_DIR,
                 g_disk_listp[disk_index].disk_name, DIR_LOG, g_dir_seq_no);

        fprintf(dbgfd,"source directory name is %s\n", src_dir_name);

        ls_cmd[0] = '\0';
        snprintf(ls_cmd, MAX_PATH_LEN, "%s%s", "/bin/ls -t ", src_dir_name);
        fprintf(dbgfd,"\nthe command is: [%s] and output of command is:\n", ls_cmd);

        fp = popen(ls_cmd,"r");
        if (fp == NULL)
                fprintf(dbgfd,"error: process open failed for command [%s]\n", ls_cmd);

        path[0] = '\0';
        while(fgets(path, sizeof(path), fp) != NULL) {
                strtok(path, "\n");
                if(!strncmp(path, TAG_FILE_NAME_PREFIX, PREFIX_LEN)) {
                    no_of_tag++;
                    fprintf(dbgfd,"found tag!\n");
                }
                fprintf(dbgfd," [%s] ", path);
        }
        pclose(fp);
    }

    if (no_of_tag == g_nr_disk) {
        ret = 1;
    } else {
        ret = 0;
    }

    WAL_TRACE_END();
    return ret;
}

int verify_write_order_accross_disks(void)
{
    int ret = 0;
    char src_dir_name[MAX_PATH_LEN];
    char path[MAX_PATH_LEN];
    char ls_cmd[MAX_PATH_LEN];
    char last_log_file_name[MAX_PATH_LEN];
    int no_of_tag = 0;
    FILE *fp;
    unsigned char *rbuf = NULL;
    unsigned long *rcount;
    unsigned char *rp;
    unsigned long *pattern_array;
    unsigned long base_pattern;
    int found_lower_val;
    int pattern_index = 0;
    int i;
    int found_tag = 0;
    int disk_index;
    int log_fd;
    char rm_cmd_buf[MAX_PATH_LEN];
    int pattern_n_n_n = 0;
    int pattern_n_n_n_1 = 0;
    int pattern_n_n_1_n_1 = 0;

    WAL_TRACE_START();

    fprintf(dbgfd,"\nnow verifying write order across disks for iteration log_[%lu]\n", g_dir_seq_no);

    rbuf = (unsigned char*) malloc(sizeof(unsigned char) * g_sec_sz_inbyte);
    if (!rbuf)
    {
        fprintf(dbgfd, "error: failed to allocate memory for read buffer in verifier\n");
        ret = 0;
        goto exit;
    }
    //allocate pattern array, equal to number of disks used for WAL test
    pattern_array = (unsigned long *) malloc(g_nr_disk * (sizeof(unsigned long)));
    if (!pattern_array) {
        printf ("error: failed to allocate memory for pattern array\n");
        fprintf(dbgfd,"error: failed to allocate memory for pattern array\n");
        ret = 0;
        goto exit;
    }

    for ( disk_index = 0; disk_index < g_nr_disk; disk_index++) {

        src_dir_name[0] = '\0';
        snprintf(src_dir_name, MAX_PATH_LEN, "%s%s%s%lu", BASE_DIR,
                 g_disk_listp[disk_index].disk_name, DIR_LOG, g_dir_seq_no);

        fprintf(dbgfd,"\nsource directory name is %s\n", src_dir_name);

        ls_cmd[0] = '\0';
        snprintf(ls_cmd, MAX_PATH_LEN, "%s%s", "/bin/ls -t ", src_dir_name);
        fprintf(dbgfd,"the command is [%s]\n", ls_cmd);
        fp = popen(ls_cmd, "r");
        if (fp == NULL) {
            fprintf(dbgfd,"error: process open failed for command [%s]\n", ls_cmd);
            ret = 0;
            goto exit;
        } 

        path[0] = '\0';
        while(fgets(path, sizeof(path), fp) != NULL) {
                strtok(path, "\n");
                if(!strncmp(path, TAG_FILE_NAME_PREFIX, PREFIX_LEN)) {
                    no_of_tag++;
                    fprintf(dbgfd,"found tag!\n");
                    found_tag = 1;
                    continue;
                }
                if (found_tag) {
                    fprintf(dbgfd,"\ncommand is: [%s] and last log file before tag is: [%s]\n", ls_cmd, path);
                    goto read_pattern;
                }
        }

read_pattern:
        pclose(fp);

        //now path has the last changed file for this disk, now read the last 8 byte integer and store it inside the array.
        last_log_file_name[0] = '\0';
        snprintf(last_log_file_name, MAX_PATH_LEN, "%s%s%s", src_dir_name,"/", path);
        fprintf(dbgfd,"complete path of last log file name before tag is: [%s]\n", last_log_file_name);

        log_fd = open(last_log_file_name, O_RDONLY);
        if (log_fd < 0) {
            fprintf(dbgfd,"error: failed to open file [%s]\n", last_log_file_name);
            ret = 0;
            goto exit;
        }

        if (lseek(log_fd, -g_sec_sz_inbyte, SEEK_END) < 0) {
            fprintf(dbgfd,"error in seek file [%s]\n", last_log_file_name);
            ret = 0;
            goto exit;
         }

        memset(rbuf, 0, sizeof(*rbuf));
        if(read(log_fd, rbuf, g_sec_sz_inbyte) < 0) {
            fprintf(dbgfd,"error in reading file [%s]\n", last_log_file_name);
            ret = 0;
            goto exit;
        }     

        rp = NULL;
        rp = (rbuf + (g_sec_sz_inbyte - DEF_SIZE_OF_PATTERN_BYTE));
        rcount = (unsigned long *)&rp[0];
        fprintf(dbgfd,"the pattern read from file is %lu\n", *rcount);
        pattern_array[pattern_index++] = *rcount;

        if (log_fd != -1) {
            close(log_fd);
        }
    }

    base_pattern = pattern_array[0];
    found_lower_val = 0;
    for (i = 0;  i < pattern_index; i++) {

         fprintf(dbgfd,"pattern array is [%lu]",pattern_array[i]);

         if (base_pattern == pattern_array[i]) {
             pattern_n_n_n = 1;
         } else if (!found_lower_val && ((base_pattern - pattern_array[i]) == 1)) {
             base_pattern = pattern_array[i];
             found_lower_val = 1;
             //(n-1) is the only one pattern at last
             if ( i  == pattern_index - 1) {
                 pattern_n_n_n_1 = 1;
                 pattern_n_n_n = 0;
             } else {
                 pattern_n_n_1_n_1 = 1;
                 pattern_n_n_n = 0;
             }
         } else {
             fprintf(dbgfd,"WAL test failed\n");
             ret = 0;
             goto exit;
         }
    }
    ret = 1;
 
    //stat of bucket count for each pattern (n,n,n),(n,n,n-1) and (n,n-1,n-1) 
    if (pattern_n_n_n) ++g_pattern_n_n_n;
    else if (pattern_n_n_n_1) ++g_pattern_n_n_n_1;
    else if (pattern_n_n_1_n_1) ++g_pattern_n_n_1_n_1;
 
exit:
    if (pattern_array) {
        free(pattern_array);
        pattern_array = NULL;
    }
    if (rbuf) {
        free(rbuf);
        rbuf = NULL;
    }
    if (ret) {
        for ( disk_index = 0; disk_index < g_nr_disk; disk_index++) {

            src_dir_name[0] = '\0';
            snprintf(src_dir_name, MAX_PATH_LEN, "%s%s%s%lu", BASE_DIR,
                    g_disk_listp[disk_index].disk_name, DIR_LOG, g_dir_seq_no);
            //done with this log directory for this iteration, now delete it
            fprintf(dbgfd,"done with verification of log diretory name %s, now deleting it!\n", src_dir_name);
            rm_cmd_buf[0] = '\0';
            snprintf(rm_cmd_buf, MAX_PATH_LEN, "rm -rf %s", src_dir_name);
            system(rm_cmd_buf);
        }
    }

    WAL_TRACE_END();
    return ret;
}

void *start_verifier(void *thread)
{
    int ret;
    int disk_index;

    WAL_TRACE_START();

    pthread_mutex_lock(&verifier_mutex);

    while (!g_start_verifier_thread) {
        pthread_cond_wait(&verifier_cond_wait, &verifier_mutex);
        fprintf(dbgfd,"Received verifier condition signal! starting verifier.\n");

check_tag_on_all_disks:
        if (veirfy_tag_accross_disks()) {
            fprintf(dbgfd,"\nreceived tag on all disks for this iteration. now verify write order\n");
        } else {
             fprintf(dbgfd,"\ncouldn't received tag on all disks for this iteration till now. Retrying ...\n");
             goto check_tag_on_all_disks;
        }

        if (verify_write_order_accross_disks()) {
            fprintf(dbgfd,"\nWrite order for iteration  log_[%lu]....[PASSED]\n", g_dir_seq_no);
            fflush(dbgfd);
            fprintf(wfd,"Write order for iteration  log_[%lu]....[PASSED]\n", g_dir_seq_no);
            fflush(wfd);
        } else {
            fprintf(dbgfd,"\nWrite order for iteration  log_[%lu]....[FAILED]\n", g_dir_seq_no);
            fflush(dbgfd);
            fprintf(wfd,"Write order for iteration  log_[%lu]....[FAILED]\n", g_dir_seq_no);
            fflush(wfd);
        }
        ++g_dir_seq_no;
        g_start_verifier_thread = 0;
    }

    pthread_mutex_unlock(&verifier_mutex);

out:
    ret = 0;

exit:

    WAL_TRACE_END();

    if (ret == -1) {
        return ret;
    }
    return (NULL);
}

void launch_verifier_thread(void)
{
    int ret;
    int t = 0;
    pthread_t verifier_thread_id;
    pthread_attr_t attr;

    WAL_TRACE_START();

    //set state to detached, don't need to wait for it inside main thread
    //the thread creation will only call start_service function and comes out
    //doesn't wait for completion of start_verifier() function, so the control
    //immediatley goes to main thread and main thread keep doing its work while
    //this thread will keep running in a while loop and when main thread terminates
    //at that time its resource will get clean automatically by POSIX system. 
    pthread_attr_init(&attr);
    pthread_mutex_init(&verifier_mutex, NULL);
    pthread_cond_init(&verifier_cond_wait, NULL);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    //create thread
    ret = pthread_create(&verifier_thread_id, &attr, start_verifier, (void*)t);
    if (ret) {
        fprintf(dbgfd,"error: failed to create start verifier thread.\n");
        exit(-1);
    }

    WAL_TRACE_END(); 
}
