#include "cc.h"
#include "libinclude.h"
#include "liblogger.h"

unsigned long g_nr_disk = 0;
disk_list g_disk_listp[MAX_DISK];
unsigned long g_sec_sz_inbyte = 0;
char *tag_name = NULL;

char *base_dir = BASE_DIR;

int cleanup = 0;

int veirfy_tag_accross_disks(void)
{
    int error = 0;
    char src_dir_name[MAX_PATH_LEN];
    char path[MAX_PATH_LEN];
    int no_of_tag = 0;
    FILE *fp;
    int disk_index;
    char ls_cmd[MAX_PATH_LEN];
    int retry = 5;

    dbg("Check for tag - %s", tag_name);

    while (retry) {
        for ( disk_index = 0; disk_index < g_nr_disk; disk_index++) {
    
            src_dir_name[0] = '\0';
            snprintf(src_dir_name, MAX_PATH_LEN, "%s%s/%s", base_dir,
                     g_disk_listp[disk_index].disk_name, tag_name);

            dbg("source directory name is %s\n", src_dir_name);

            ls_cmd[0] = '\0';
            snprintf(ls_cmd, MAX_PATH_LEN, "%s%s", "/bin/ls -t ", 
                     src_dir_name);
            dbg("CMD: [%s]\nOutput:", ls_cmd);

            fp = popen(ls_cmd, "r");
            if (fp == NULL) {
                err("Pipe Open Failed");
                error = -1;
                break;
            }

            path[0] = '\0';
            while (fgets(path, sizeof(path), fp) != NULL) {
                strtok(path, "\n");
                if(!strncmp(path, tag_name, strlen(tag_name))) {
                    no_of_tag++;
                    dbg("found tag");
                }
                dbg(" [%s] ", path);
            }
            pclose(fp);
        }

        if (no_of_tag == g_nr_disk)
            break;
        else {
            sleep(65);
            retry--;
        }
    }

out:
    return error;
}

int verify_write_order_accross_disks(void)
{
    int error = -1;
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
    int pattern_n_n_n = 0;
    int pattern_n_n_n_1 = 0;
    int pattern_n_n_1_n_1 = 0;
    int partial = 0;
    int copy_pattern = 0;

    dbg("Verifying write order for %s", tag_name);

    rbuf = (unsigned char*) malloc(sizeof(unsigned char) * g_sec_sz_inbyte);
    if (!rbuf)
    {
        err("failed to allocate memory for read buffern");
        goto exit;
    }
    //allocate pattern array, equal to number of disks used for WAL test
    pattern_array = (unsigned long *) malloc(g_nr_disk * (sizeof(unsigned long)));
    if (!pattern_array) {
        err("failed to allocate memory for pattern array");
        goto exit;
    }

    for ( disk_index = 0; disk_index < g_nr_disk; disk_index++) {

        src_dir_name[0] = '\0';
        snprintf(src_dir_name, MAX_PATH_LEN, "%s%s/%s", base_dir,
                 g_disk_listp[disk_index].disk_name, tag_name);

        dbg("source directory name is %s", src_dir_name);

        if (access(src_dir_name, F_OK)) {
            err("Partial Tag: %s", tag_name);
            /* 
             * populate pattern array with previous entry so that verification
             * does not fail
             */
            if (pattern_array[pattern_index] > 0 )
                pattern_array[pattern_index] = pattern_array[pattern_index - 1];
            else /* if failed for first disk, copy in next iteration */
                copy_pattern = 1;
            pattern_index++;
            partial++;
            continue;
        }

        ls_cmd[0] = '\0';
        snprintf(ls_cmd, MAX_PATH_LEN, "%s%s%s", "/bin/ls ", src_dir_name," | sort");
        dbg("CMD: [%s]", ls_cmd);
        fp = popen(ls_cmd, "r");
        if (fp == NULL) {
            err("process open failed for command [%s]\n", ls_cmd);
            goto exit;
        } 

        path[0] = '\0';
        while(fgets(path, sizeof(path), fp) != NULL) {
                strtok(path, "\n");
                if(!strncmp(path, tag_name, strlen(tag_name))) {
                    no_of_tag++;
                    dbg("found tag!");
                    found_tag = 1;
                    continue;
                }
        }

        if (found_tag) {
            dbg("last log file before tag is: [%s]\n", path);
        }

read_pattern:
        pclose(fp);

        /*
         * now path has the last changed file for this disk, 
         * now read the last 8 byte integer and store it inside the array.
         */
        last_log_file_name[0] = '\0';
        snprintf(last_log_file_name, MAX_PATH_LEN, "%s%s%s", 
                 src_dir_name,"/", path);
        dbg("complete path of last log file: [%s]", last_log_file_name);

        log_fd = open(last_log_file_name, O_RDONLY);
        if (log_fd < 0) {
            err("failed to open file [%s]", last_log_file_name);
            goto exit;
        }

        if (lseek(log_fd, -g_sec_sz_inbyte, SEEK_END) < 0) {
            err("error in seek file [%s]", last_log_file_name);
            goto exit;
         }

        memset(rbuf, 0, sizeof(*rbuf));
        if(read(log_fd, rbuf, g_sec_sz_inbyte) < 0) {
            err("error in reading file [%s]", last_log_file_name);
            goto exit;
        }     

        rp = NULL;
        rp = (rbuf + (g_sec_sz_inbyte - DEF_SIZE_OF_PATTERN_BYTE));
        rcount = (unsigned long *)&rp[0];
        dbg("the pattern read from file is %lu", *rcount);
        pattern_array[pattern_index] = *rcount;

        if (copy_pattern) {
            pattern_array[pattern_index - 1] = pattern_array[pattern_index];
            copy_pattern = 0;
        }
        pattern_index++;

        if (log_fd != -1) {
            close(log_fd);
        }
    }

    base_pattern = pattern_array[0];
    found_lower_val = 0;
    for (i = 0;  i < pattern_index; i++) {

         dbg("pattern array is [%lu]",pattern_array[i]);

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
             dbg("WAL test failed");
             goto exit;
         }
    }

    if (!partial)
        error = 0;
    else if (partial) /* if we did not drain any tags make it partial */
        error = 1;
    else
        error = -1;

    printf("%lu %lu\n", pattern_array[0], pattern_array[pattern_index - 1]);
 
exit:
    if (pattern_array) {
        free(pattern_array);
        pattern_array = NULL;
    }
    if (rbuf) {
        free(rbuf);
        rbuf = NULL;
    }
    
    return error;
}

int
start_verifier()
{
    int error;

    error = veirfy_tag_accross_disks();
    if (!error) {
        dbg("received tag on all disks");

        error = verify_write_order_accross_disks();
        if (!error) 
            info("Write order for %s ....[PASSED]", tag_name);
        else if (error = 1) 
            info("Partial Write order for %s ....[PASSED]", tag_name);
        else
            dbg("Write order for %s ....[FAILED]", tag_name);
    }

    return error;
}

void do_cleanup()
{
    int disk_index = 0;
    char rm_cmd_buf[MAX_PATH_LEN];
    char src_dir_name[MAX_PATH_LEN];
    for ( disk_index = 0; disk_index < g_nr_disk; disk_index++) {

        src_dir_name[0] = '\0';
        snprintf(src_dir_name, MAX_PATH_LEN, "%s%s/%s", base_dir,
                 g_disk_listp[disk_index].disk_name, tag_name);

        //done with this log directory for this iteration, now delete it
        dbg("done with verification with %s, now deleting it!\n", 
            src_dir_name);
        rm_cmd_buf[0] = '\0';
        snprintf(rm_cmd_buf, MAX_PATH_LEN, "rm -rf %s", src_dir_name);
        system(rm_cmd_buf);
    }

}


void
usage(int retval)
{
    printf("Usage:"
           "\n\t -b <dir> - directory path for test run "
           "\n\t -c - cleanup the dir for a given tag name"
           "\n\t -d <disk> - disk path"
           "\n\t -h - Print usage help"
           "\n\t -l - Enable debug logging"
           "\t\n -r <blocksize> - IO block size (Default 512)"
           "\n\t -t <tag> - tag name to verify"
           "\n\t ex: cc_verify -b /var/log/wal/test_crash_tag/ -d /dev/sdc -d /dev/sdd -d /dev/sdf -t DistributedTag1"
           "\n\t ex: cc_verify -b /var/log/wal/test_crash_tag/ -c -d /dev/sdc -d /dev/sdd -d /dev/sdf -t DistributedTag1"
           "\n");

    exit(retval);
}

int
main(int argc, char *argv[])
{
    int ret = -1;
    int optindx;
    char *disk_name;

    while((optindx = getopt(argc, argv, "b:cd:hlt:r:")) != -1) {

        switch (optindx) {
            case 'b':
                base_dir = optarg;
                break;

            case 'c':
                cleanup = 1;
                break;

            case 'd':
                if (g_nr_disk< MAX_DISK)
                    g_disk_listp[g_nr_disk++].disk_name = optarg;
                else
                    log_err_usage("Max %d disks supported", MAX_DISK);
                break;

            case 'h':
                usage(0);
                break;

            case 'l':
                debug = 1;
                break;

            case 't':
                tag_name = optarg;
                break;

            case 'r':
                g_sec_sz_inbyte = strtoul(optarg, NULL, 10);
                break;

            default:
                log_err_usage("Invalid Option");
                goto exit;
        }
    }

    if (!tag_name)
        log_err_usage("No tag name provided");

    if (!g_nr_disk) {
        log_err_usage("No disks provided");
        goto exit;
    }

    if (!g_sec_sz_inbyte)
        g_sec_sz_inbyte = DEF_SIZE_OF_SECTOR_INBYTE;

    if (cleanup)
        do_cleanup();
    else
        ret = start_verifier();

exit:
    return ret;
}
