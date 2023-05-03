#ident "@(#) $Source $Revision: 1.1.2.1 $ $Name: INT_BRANCH $ $Revision: 1.1.2.1 $"


#include "inm_filter.h"
#include "inm_cmd_parser.h"
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include<errno.h>
struct config *app_confp;

void
print_help(int argc)
{
    const char *man_pg = NULL;

    printf("Usage : inm_dmit [--version=[ver_options]]        \n\
                    --op={additional_stats|latency_stats|dmesg} --src_vol={src_device}\n\
                    --get_drv_stat                           \n\
                    --get_volume_stat target-name            \n\
                    --get_protected_volume_list              \n\
                    --get_attr [volume volume-name] attribute-name  \n\
                    --set_attr [volume volume-name] attribute-name attribute-value\n");

    if (argc < 2) return;
    printf("                    --resync=[options]           \n\
                    --profiler_mode=[on|off]                 \n\
                    --verbose=yes                            \n\
                    --trace=[on|off]                         \n");

    man_pg = "                                               \n\
ver_options: based on version number, it allocates and uses  \n\
        appropriate data structures.                         \n\
                                                             \n\
    v1 : base version.                                       \n\
    v2 : phase1 of per-io-time stamps.                       \n\
    v3 : phase2 of per-io-time stamps.                       \n\
                    \n\
op_options : operation type options                          \n\
    service_start                                            \n \
    stack name                                               \n\
        stacks the volume                                    \n\
        ex: inm_dmit --op=stack /dev/sdb                     \n\
            inm_dmit --op=stack /dev/mapper/testvg-vol1      \n\
                                                             \n\
        driver notes : Allocates the target context and marks the \n\
            filtering is disabled.                           \n\
    start name                                               \n\
        starts filtering on volume                           \n\
        ex: inm_dmit --op=start /dev/sdb                     \n\
            inm_dmit --op=start /dev/mapper/testvg-vol1      \n\
                                                             \n\
        driver notes : Allocates the target context and marks the \n\
            filtering is enabled.                            \n\
                                                             \n\
    unstack name                                             \n\
        unstacks the volume                                  \n\
        ex: inm_dmit --op=unstack /dev/sdb                   \n\
            inm_dmit --op=unstack /dev/mapper/testvg-vol1    \n\
                                                             \n\
        driver notes : Deallocates the target context.       \n\
    stop name                                                \n\
        stops filtering on volume name                       \n\
        ex: inm_dmit --op=stop /dev/sdb                      \n\
            inm_dmit --op=stop /dev/mapper/testvg-vol1       \n\
        driver notes : disables the filtering.               \n\
                                                             \n\
    freeze name                                              \n\
        freeze on volume name                                \n\
        ex: inm_dmit --op=freeze /dev/sda                    \n\
            inm_dmit --op=freeze /dev/mapper/testvg-vol1 1     \n\
        driver notes : freeze the volume, linux specific.    \n\
                                                             \n\
    thaw name                                                \n\
        thaw on volume name                                  \n\
        ex: inm_dmit --op=thaw /dev/sda                      \n\
            inm_dmit --op=thaw /dev/mapper/testvg-vol1  1    \n\
        driver notes : thaw the volume, linux specific.      \n\
                                                             \n\
    get_time                                                 \n\
        ex: inm_dmit --op=get_time                           \n\
        driver notes : returns the driver time               \n\
                                                             \n\
	for host protection following is replicate option    \n\
    replicate --src_vol=<src_volume --tgt_vol=tgt_vol --version=<version> [--resync=no]               \n\
	for prism protection following is replicate option    \n\
    replicate --src_vol=<resync_device> --tgt_vol=<tgt_device> --version=<version> --lun_name=<AT LUN name> --dev_type=fabric [--resync=no]               \n\
        by default the resync option for this is no          \n\
        the app drains the changes and applies to the target volume. \n\
                                                             \n\
    get_db src_vol                                           \n\
        gets single dirty block.                             \n\
        ex: inm_dmit --op=get_db /dev/sdb                    \n\
        driver notes : drains one dirty block                \n\
                                                             \n\
    clear_diffs src_vol                                      \n\
        ex: inm_dmit --op=clear_diffs /dev/sdb               \n\
                                                             \n\
    get_db_thres                                             \n\
        ex: inm_dmit --op=get_db_thres /dev/sdb              \n\
                                                             \n\
    print_ioctl_codes                                        \n\
        ex: inm_dmit --op=print_ioctl_codes                  \n\
                                                             \n\
    print_tag_codes                                          \n\
        ex: inm_dmit --op=print_tag_codes                    \n\
                                                             \n\
    get_driver_version                                       \n\
        ex: inm_dmit --op=get_driver_version                 \n\
                                                             \n\
    resync_start src_vol                                     \n\
        ex: inm_dmit --op=resync_start src_vol               \n\
                                                             \n\
    resync_end src_vol                                       \n\
        ex: inm_dmit --op=resync_end src_vol                 \n\
                                                             \n\
    system_shutdown_notify                                   \n\
        ex: inm_dmit --op=system_shutdown_notify             \n\
                                                             \n\
    tag tag_name <#of vols> <vol1>, <vol2>, <vol3>           \n\
                                                             \n\
    boottime_stacking                                        \n\
        ex: inm_dmit --op=boottime_stacking                  \n\
                                                             \n\
    do start mirroring					     \n\
       ex: inm_dmit --op=start_mirror --src_list=<list of device> --dst_list=<list of devices> \n\
    do start mirroring for local devices		     \n\
    To change filter driver verbosity at run time            \n\
        ex: inm_dmit --op=set_verbosity --verbosity=< value >\n\
                                                             \n\
--get_attr :       use this option to get attribute value. The attribute could be a \n\
        common or specific to a volume. If [volume volume-name] is not specified    \n\
        in the command then inmdmit consider it to be a common attribute.           \n\
                                                                                    \n\
--set_attr :       use this option to set attribute value. The attribute could be a \n\
        common or specific to a volume. If [volume volume-name] is not specified    \n\
        in the command then inmdmit consider it to be a common attribute.           \n\
                                                                                    \n\
        LIST OF THE COMMON ATTRIBUTES IS AS FOLLOWS:-        \n\
        DataPoolSize                                         \n\
        DefaultLogDirectory                                  \n\
        FreeThresholdForFileWrite                            \n\
        VolumeThresholdForFileWrite                          \n\
        DirtyBlockHighWaterMarkServiceNotStarte              \n\
        DirtyBlockLowWaterMarkServiceRunning                 \n\
        DirtyBlockHighWaterMarkServiceRunning                \n\
        DirtyBlockHighWaterMarkServiceShutdown               \n\
        DirtyBlocksToPurgeWhenHighWaterMarkIsReached         \n\
        MaximumBitmapBufferMemory                            \n\
        Bitmap512KGranularitySize                            \n\
        VolumeDataFiltering                                  \n\
        VolumeDataFilteringForNewVolumes                     \n\
        VolumeDataFiles                                      \n\
        VolumeDataFilesForNewVolumes                         \n\
        VolumeDataToDiskLimitInMB                            \n\
        VolumeDataNotifyLimit                                \n\
        SequenceNumber                                       \n\
        MaxDataSizeForDataModeDirtyBlock                     \n\
        VolumeResDataPoolSize                                \n\
        MaxDataPoolSize                                      \n\
        VacpIObarrierTimeout                                 \n\
        FsFreezeTimeout                                      \n\
        VacpAppTagCommitTimeout                              \n\
                                                             \n\
        LIST OF THE VOLUME ATTRIBUTES IS AS FOLLOWS:-        \n\
        VolumeFilteringDisabled                              \n\
        VolumeBitmapReadDisabled                             \n\
        VolumeBitmapWriteDisabled                            \n\
        VolumeDataFiltering                                  \n\
        VolumeDataFiles                                      \n\
        VolumeDataToDiskLimitInMB                            \n\
        VolumeDataNotifyLimitInKB                            \n\
        VolumeDataLogDirectory                               \n\
        VolumeBitmapGranularity                              \n\
        VolumeResyncRequired                                 \n\
        VolumeOutOfSyncErrorCode                             \n\
        VolumeOutOfSyncErrorStatus                           \n\
        VolumeOutOfSyncCount                                 \n\
        VolumeOutOfSyncTimestamp                             \n\
        VolumeOutOfSyncErrorDescription                      \n\
        VolumeFilterDevType                                  \n\
        VolumeNblks                                          \n\
        VolumeBsize                                          \n\
        VolumeResDataPoolSize                                \n\
        VolumeMirrorSourceList                               \n\
        VolumeMirrorDestinationList                          \n\
        VolumeMirrorDestinationScsiID                        \n\
	VolumePTpathList				     \n\
	VolumePerfOptimization				     \n\
                                                             \n\
NOTE:- Option --get_attr and --set_attr is Solaris specific, use sysfs interface \n\
    for linux.\n";

    printf(man_pg);
}

struct str_idx opi[] =
{{"", 0},
 {"stack", STACK_OP},
 {"start_flt", START_FLT_OP},
 {"unstack", UNSTACK_OP},
 {"stop_flt", STOP_FLT_OP},
 {"get_time", GET_TIME_OP},
 {"replicate", REPLICATE_OP},
 {"get_db", GET_DB_OP},
 {"get_db_commit", GET_DB_COMMIT_OP},
 {"clear_diffs", CLEAR_DIFFS_OP},
 {"get_db_thres", GET_DB_THRES_OP},
 {"print_ioctl_codes", PRINT_IOCTL_CODES_OP},
 {"print_tag_codes", PRINT_TAG_CODES_OP},
 {"get_driver_version", GET_DRIVER_VER_OP},
 {"resync_start", RESYNC_START_OP},
 {"resync_end", RESYNC_END_OP},
 {"system_shutdown_notify", SYS_SHUTDOWN_OP},
 {"tag", ISSUE_TAG_OP},
 {"service_start", SERVICE_START_OP},
 {"start_notify", START_NOTIFY_OP},
 {"add_lun", LUN_CREATE_OP},
 {"del_lun", LUN_DELETE_OP},
 {"unstack_all", UNSTACK_ALL_OP},
 {"boottime_stacking", BOOTTIME_STACKING_OP},
 {"start_mirror", START_MIRROR_OP},
 {"stop_mirror", STOP_MIRROR_OP},
 {"stack_mirror", STACK_MIRROR_OP},
 {"dmesg", DMESG_OP},
 {"set_verbosity", SET_VERBOSITY_OP},
 {"test_heartbeat", TEST_HEARTBEAT_OP},
 {"start_local_mirror", START_LOCAL_MIRROR_OP},
 {"stop_local_mirror", STOP_LOCAL_MIRROR_OP},
 {"additional_stats", ADDITIONAL_STATS_OP},
 {"latency_stats", LATENCY_STATS_OP},
 {"bmap_stats", BMAP_STATS_OP},
 {"block_at_lun", BLOCK_AT_LUN_OP},
 {"unblock_at_lun", UNBLOCK_AT_LUN_OP},
 {"init_driver_fully", INIT_DRIVER_FULLY_OP},
 {"freeze_vol", FREEZE_VOL_OP},
 {"thaw_vol", THAW_VOL_OP},
 {"monitoring_stats", MONITORING_STATS_OP},
 {"get_blk_mq_status", GET_BLK_MQ_STATUS_OP},
 {"get_vol_stats_v2", GET_VOLUME_STATS_V2_OP},
 {"map_name", GET_NAME_MAPPING_OP},
 {"lcw", LCW_OP},
 {"get_cx_status", GET_CX_STATUS_OP},
 {"wakeup_monitor_thread", WAKEUP_MONITOR_THREAD},
 {"tag_drain_notify", TAG_DRAIN_NOTIFY_OP},
 {"wakeup_tag_drain_notify_thread", WAKEUP_TAG_DRAIN_NOTIFY},
 {"modify_persistent_device_name", MODIFY_PERSISTENT_DEVICE_NAME_OP},
 {"block_drain", BLOCK_DRAIN_OP},
 {"unblock_drain", UNBLOCK_DRAIN_OP},
 {"get_drain_state", GET_DRAIN_STATE_OP},
 {"create_barrier", CREATE_BARRIER_OP},
 {"remove_barrier", REMOVE_BARRIER_OP},
 {"stop_flt_all", STOP_FLT_ALL_OP},
};

/* macros for version info */
#define VERSION_V1     1
#define VERSION_V2     2
#define VERSION_V3     3
#define MAX_NR_VERS     3
#define COMMAND_LENGTH  512

struct str_idx ver_idx[] =
{
 {NULL, 0},
 {"v1", VERSION_V1},
 {"v2", VERSION_V2},
 {"v3", VERSION_V3},
};


/* function : get_idx_from_str, returns the operation code
 @ op_namep : name of the operation
 @ idx_tabp : ptr to str_idx array
 @ max_nr : max # of entries in str_idx array.
 **/
int
get_idx_from_str(char *op_namep, struct str_idx *idx_tabp, int max_nr)
{
    int i = 0;
    int op_code = 0;

    for (i=1; i <= max_nr; i++) {
        if (strcmp(idx_tabp[i].key_name, op_namep) == 0) {
            break;
        }
    }
    if (i <= max_nr) {
        op_code = i;
    }

    return op_code;
}

extern int remove_persistent_store(void);

int
main(int argc, char *argv[])
{
    int op_code = 0;
    int nvol = 0;
    int fd = 0;
    char *cmd = NULL;
    char *device = NULL;
    FILE *input;
    char *scsi_id = NULL;
    int offset = 0;
    int fsync_fd = 0, ret = 0;

    static struct inm_option long_options[] = {
    /* these options set a flag */
    {"version", INM_REQUIRED_ARGUMENT, 0, 1},
    {"op", INM_REQUIRED_ARGUMENT, 0, 2},
    {"resync", INM_NO_ARGUMENT, 0, 3},
    {"profiler", INM_NO_ARGUMENT, 0, 4},
    {"trace", INM_NO_ARGUMENT, 0, 5},
    {"verbose", INM_NO_ARGUMENT, 0, 6},
    {"src_vol", INM_REQUIRED_ARGUMENT, 0, 7},
    {"tgt_vol", INM_REQUIRED_ARGUMENT, 0, 8},
    {"lun_name", INM_REQUIRED_ARGUMENT, 0, 9},
    {"lun_size", INM_REQUIRED_ARGUMENT, 0, 10},
    {"get_drv_stat", INM_NO_ARGUMENT, 0, 11},
    {"get_volume_stat", INM_REQUIRED_ARGUMENT, 0, 12},
    {"get_protected_volume_list", INM_NO_ARGUMENT, 0, 13},
    {"get_attr", INM_REQUIRED_ARGUMENT, 0, 14},
    {"set_attr", INM_REQUIRED_ARGUMENT, 0, 15},
    {"mnt_pt", INM_REQUIRED_ARGUMENT, 0, 16},
    {"dev_type", INM_REQUIRED_ARGUMENT, 0, 17},
    {"src_list", INM_REQUIRED_ARGUMENT, 0, 18},
    {"dst_list", INM_REQUIRED_ARGUMENT, 0, 19},
    {"lun_startoff", INM_REQUIRED_ARGUMENT, 0, 20},
    {"vendor", INM_REQUIRED_ARGUMENT, 0, 21},
    {"flags", INM_REQUIRED_ARGUMENT, 0, 22},
    {"tag_name", INM_REQUIRED_ARGUMENT, 0, 23},
    {"delay", INM_REQUIRED_ARGUMENT, 0, 24},
    {"verbosity", INM_REQUIRED_ARGUMENT, 0, 25},
    {"src_scsi_id", INM_REQUIRED_ARGUMENT, 0, 26},
    {"fsync_file", INM_REQUIRED_ARGUMENT, 0, 27},
    {"help", INM_NO_ARGUMENT, 0, 28},
    {"timeout", INM_REQUIRED_ARGUMENT, 0, 29},
    {"nr_vols", INM_REQUIRED_ARGUMENT, 0, 30},
    {"tag_notify", INM_REQUIRED_ARGUMENT, 0, 31},
    {"tag_pending_notify", INM_REQUIRED_ARGUMENT, 0, 32},
    {"pname", INM_REQUIRED_ARGUMENT, 0, 33},
    {"fname", INM_REQUIRED_ARGUMENT, 0, 34},
    {"subop", INM_REQUIRED_ARGUMENT, 0, 35},
    {"fail_commit_db", INM_NO_ARGUMENT, 0, 36},
    {"pnames", INM_REQUIRED_ARGUMENT, 0, 37},
    {"old_pname", INM_REQUIRED_ARGUMENT, 0, 38},
    {"new_pname", INM_REQUIRED_ARGUMENT, 0, 39},
    {0,0,0,0}};
    int option_index = 0;

    /* process command line args */
    if (argc < 2) {
        print_help(argc);
        exit(2);
    }

    /* initialize the config structure */
    if (init_config() <  0) {
        printf("err : initial configuration failed\n");
	if ((2 == inm_getopt_long(argc, argv, "", long_options, &option_index)) != -1) {
		enum opcodes op_code;

		op_code= get_idx_from_str(inm_optarg, opi, MAX_NR_OPS);
		if (op_code == STOP_FLT_ALL_OP) {
			printf("Stop filtering all is issued, here the driver is not loaded or /dev/involflt file is missing\n");
			if (remove_persistent_store()) {
				printf("Persistent store removal failed\n");
				return -1;
			}
		}
	}
        exit(2);
    }

    memset(&app_confp->mconf, 0, sizeof(mirror_conf_info_t));

    while((op_code = 
        inm_getopt_long(argc, argv, "", long_options, &option_index)) != -1) {
    /* debug code */
#if debug
    printf("\ndebug: op_code = %d\n", op_code);
    printf("debug: opt arg = %s\n", inm_optarg);
#endif

    switch (op_code) {
    case 1:
        op_code = get_idx_from_str(inm_optarg, ver_idx, MAX_NR_VERS);
#if debug
        printf("debug: version code = %d\n", op_code);
#endif
        app_confp->ver = op_code;
        break;

    case 2:
        app_confp->op_code= get_idx_from_str(inm_optarg, opi, MAX_NR_OPS);
#if debug
        printf("debug: op code = %d\n", app_confp->op_code);
#endif
        break;

    case 3:
        app_confp->resync = 1;
        break;

    case 4:
        app_confp->prof = 1;
        break;

    case 5:
        app_confp->trace = 1;
        break;

    case 6: 
        app_confp->verbose = 1;
        break;

    case 7:
        app_confp->src_volp = inm_optarg;
        break;

    case 8:
        app_confp->tgt_volp = inm_optarg;
        break;
    case 9:
        app_confp->lun_name = inm_optarg;
        break;
    case 10:
        app_confp->lun_size = atoll(inm_optarg);
        break;
    case 11:
        app_confp->stat = DRIVER_STAT;
        break;
    case 12:
        app_confp->stat = TARGET_STAT;
        app_confp->tgt_volp = inm_optarg;
        break;
    
    case 13:
        app_confp->stat = PROT_VOL_LIST;
        break;

    case 14:
        app_confp->attribute = GET_ATTR;
        break;

    case 15:
        app_confp->attribute = SET_ATTR;
        break;

       case 16:
                app_confp->mnt_pt = inm_optarg;
                break;

    case 17:
        app_confp->dev_type = inm_optarg;
        break;

    case 18:
        
        printf("\nsource list:%s:\n", inm_optarg);
        if (!inm_optarg) {
            printf("Error: source (PT Lun) list empty!");
            exit(2);
        }
        if (strlen(inm_optarg)) {
            char *str, *str1, *str2, *ptr;
            int len = 0;
            int nvols = 0;
            offset = 0;

            str = inm_optarg;
            str1=str2 = NULL;

            str1 = str;
            while ((str2=strchr(str1,','))) {
                str1=++str2;
                nvols++;
            }
            nvols++;
            printf("Total number of volumes:%d\n", nvols);
            str1=str2 = NULL;
            
            device = malloc(INM_GUID_LEN_MAX);
            memset(device, 0, INM_GUID_LEN_MAX);
            scsi_id = malloc(INM_MAX_SCSI_ID_SIZE);
            memset(scsi_id, 0, INM_MAX_SCSI_ID_SIZE);
            cmd = malloc(COMMAND_LENGTH);
            memset(cmd, 0, COMMAND_LENGTH);
            
            app_confp->mconf.src_guid_list =
            malloc(nvols*INM_GUID_LEN_MAX);
            memset(app_confp->mconf.src_guid_list, 0,
                nvols*INM_GUID_LEN_MAX);
            ptr = app_confp->mconf.src_guid_list; 
            while (nvols) {
                if ((str1 = strchr(str,'/'))) {
                   str2 = NULL;
                   if ((str2 = strchr(str,','))) {
                      *str2 = '\0';
                   }
                   len = strlen(str1);
                   nvol++;

                   if (strncpy_s(ptr, nvols*INM_GUID_LEN_MAX, str1, len+1)) {
			exit(3);
		   }

                   ptr += (INM_GUID_LEN_MAX);
                   fd = open(str1, O_RDONLY | INM_DIRECTIO);
                   if (fd < 0) {
                       printf("Error opening source volume %s\n",str1);
                   }
                   else {
                       close(fd);
                       if (device && !*device) {
                          if (strcpy_s(device, INM_GUID_LEN_MAX, str1)) {
				exit(4);
			  }
                       }
                   }

                   if (strcpy_s(app_confp->mconf.src_guid_list+offset, nvols*INM_GUID_LEN_MAX - offset, str1))
			exit(5);

                   //printf("Copied source volume:%s to src_guid_list\n",str1);
                   offset += INM_GUID_LEN_MAX;
                   if (!str2) {
                       //printf("breaking out :%s",str);
                       break;
                   }
                   else {
                       *str2 = ',';
                       str = str2 + 1;
                   }
                }
            }
            app_confp->mconf.nsources = nvols;
            if(app_confp->op_code == START_MIRROR_OP || app_confp->op_code == STACK_MIRROR_OP || app_confp->op_code == STOP_MIRROR_OP){
                snprintf(cmd, COMMAND_LENGTH, "/etc/vxagent/bin/inm_scsi_id %s",
                    device);
                printf("Executing command :%s:\n", cmd);
            
                input = popen(cmd, "r");
                if (!input) {
                    fprintf(stderr, "incorrect parameters to scsi_id.sh shell script\n");
                    return EXIT_FAILURE;
                }
                fscanf(input, "%s", scsi_id);
                if (pclose(input) != 0 ) {
                    fprintf(stderr, "Command:%s\n",cmd);
                    fprintf(stderr, "Could not run for some reason\n");
                    exit(2);
                }
           } else {
               if(app_confp->op_code != START_LOCAL_MIRROR_OP && app_confp->op_code != STOP_LOCAL_MIRROR_OP){
                   printf("invalid op code %d\n", app_confp->op_code);
                   exit (1);
               }
               sprintf(scsi_id, "%s_%s","scsi_id", convert_path_to_str(device));
           }

            if (strcpy_s(app_confp->mconf.src_scsi_id, INM_MAX_SCSI_ID_SIZE, scsi_id))
		exit(3);

            printf("Source volume list :%s,... nvol:%d scsi_id:%s:\n",
                app_confp->mconf.src_guid_list, nvol, app_confp->mconf.src_scsi_id);
        }
        else {
            printf("invalid source volume list\n");
            exit(2);
        }

        break;

    case 19:
        printf("\ndestination list:%s\n", inm_optarg);
        if (!inm_optarg) {
            printf("Error: destination (AT Lun) list empty!");
            exit(2);
        }
        nvol = 0;
        offset = 0;
        if (strlen(inm_optarg)) {
            char *str, *str1, *str2, *ptr;
            int len = 0;
            int nvols = 0;

            str = inm_optarg;
            str1 = str2 = NULL;

            str1 = str;
            while ((str2 = strchr(str1, ','))) {
                str1 = ++str2;
                nvols++;
                str2=NULL;
            }
            
            nvols++;
            str1 = str2 = NULL;
            //printf("Total number of volumes:%d\n", nvols);
            
            memset(device, 0, INM_GUID_LEN_MAX);
            memset(scsi_id, 0, INM_MAX_SCSI_ID_SIZE);
            memset(cmd, 0, COMMAND_LENGTH);

            app_confp->mconf.dst_guid_list = malloc(nvols*INM_GUID_LEN_MAX);
            memset(app_confp->mconf.dst_guid_list, 0 ,
                nvols*INM_GUID_LEN_MAX);
            ptr = app_confp->mconf.dst_guid_list; 
            while (nvols) {
                if ((str1 = strchr(str,'/'))) {
                    str2 = NULL;
                    if ((str2 = strchr(str1,','))) {
                       *str2 = '\0';
                    }
                    len = strlen(str1);
                    nvol++;

                    if (strncpy_s(ptr, nvols*INM_GUID_LEN_MAX, str1, len+1)) {
			 exit(3);
		    }

                    ptr += (INM_GUID_LEN_MAX);
                    fd = open(str1, O_RDONLY | INM_DIRECTIO);
                    if (fd < 0) {
                        printf("Error opening destination volume %s\n",str1);
                    }
                    else {
                        close(fd);
                        if (device && !*device) {
                            if (strcpy_s(device, INM_GUID_LEN_MAX, str1))
				exit(4);
                        }
                    }

                    if (strcpy_s(app_confp->mconf.dst_guid_list+offset, nvols*INM_GUID_LEN_MAX, str1))
			exit(5);

                    //printf("Copied destination volume:%s to dst_guid_list\n",str1);
                    offset += INM_GUID_LEN_MAX;
                    if (!str2) {
                        //printf("breaking out :%s\n",str);
                        break;
                    }
                    else {
                        *str2 = ',';
                        str = str2 + 1;
                    }
                 }
            }
            app_confp->mconf.ndestinations = nvol;
            if(app_confp->op_code == START_MIRROR_OP || app_confp->op_code == STACK_MIRROR_OP || app_confp->op_code == STOP_MIRROR_OP){
                snprintf(cmd, COMMAND_LENGTH, "/etc/vxagent/bin/inm_scsi_id %s",
                      device);
                printf("Executing command :%s:\n", cmd);
                input = popen(cmd, "r");
                if (!input) {
                    fprintf(stderr, "incorrect parameters to scsi_id.sh shell script\n");
                    return EXIT_FAILURE;
                }
                fscanf(input, "%s", scsi_id);
                if (pclose(input) != 0 ) {
                    fprintf(stderr, "Command:%s\n",cmd);
                    fprintf(stderr, "Could not run for some reason\n");
                    exit(2);
                }
           } else {
               if(app_confp->op_code != START_LOCAL_MIRROR_OP && app_confp->op_code != STOP_LOCAL_MIRROR_OP){
                   printf("invalid op code %d\n", app_confp->op_code);
                   exit (1);
               }
               sprintf(scsi_id, "%s_%s","scsi_id", convert_path_to_str(device));
           }
            if (strcpy_s(app_confp->mconf.dst_scsi_id, INM_MAX_SCSI_ID_SIZE, scsi_id))
		exit(3);

            printf("Destination volume list :%s,.. nvol:%d scsi_id:%s\n",
                app_confp->mconf.dst_guid_list,nvol,
                app_confp->mconf.dst_scsi_id);
        }
        else {
            printf("invalid destination volume list\n");
            exit(2);
        }

        break;

    case 20:
        app_confp->lun_startoff = atoll(inm_optarg);
        break;

    case 21:
        app_confp->vendor = atoi(inm_optarg);
        break;

    case 22:
	if(strstr(inm_optarg,"FULL_DISK_FLAG")){
		app_confp->flags |= FULL_DISK_FLAG;
	}
	if(strstr(inm_optarg,"FULL_DISK_PARTITION_FLAG")){
		app_confp->flags |= FULL_DISK_PARTITION_FLAG;
	}
	if(strstr(inm_optarg,"FULL_DISK_LABEL_VTOC")){
		app_confp->flags |= FULL_DISK_LABEL_VTOC;
	}
	if(strstr(inm_optarg,"FULL_DISK_LABEL_EFI")){
		app_confp->flags |= FULL_DISK_LABEL_EFI;
	}
        break;

    case 23:
	app_confp->tag_name = inm_optarg;
	break;

    case 24:
	{
		char *timstrp = (char *) malloc(strlen(inm_optarg) + 1);
		int len = strlen(inm_optarg);
		long long factor = 0;

		if (!timstrp) {
			perror("malloc error");
			return (-1);
		}

		if (strcpy_s(timstrp, strlen(inm_optarg) + 1, inm_optarg))
			return (-2);

		if (timstrp[len-1] == 'S' && timstrp[len-2] != 'S') {
			timstrp[len-1]='\0';
			len--;
		}
		if (timstrp[len-1] == 'u') {
			factor = 1;
			timstrp[len-1]='\0';
			len--;
		}
		
		if (timstrp[len-1] == 'm') {
			factor = 1000;
			timstrp[len-1]='\0';
			len--;
		}
		if (timstrp[len-1] == 'S') {
			factor = 1000000;
			timstrp[len-1]='\0';
			len--;
		}
		if (timstrp[len-1] == 'M') {
			factor = 60*1000000;
			timstrp[len-1]='\0';
			len--;
		}
		if (factor == 0) {
			//default consider it as milli sec
			factor = 1000;
		}
		app_confp->delay = factor * atoi(timstrp);
		if (app_confp->delay == 0) {
			printf("Invalid delay value \n");
		}
	}
	
	break; 
    case 25:
        app_confp->drv_verbose = atol(inm_optarg);
        break;

    case 26:
        printf("debug: src scsi id is %s", inm_optarg);

        if (strncpy_s(app_confp->mconf.src_scsi_id,  INM_MAX_SCSI_ID_SIZE, inm_optarg, INM_MAX_SCSI_ID_SIZE - 1)) 
		exit(2);

        app_confp->mconf.src_scsi_id[INM_MAX_SCSI_ID_SIZE - 1] = '\0';
        break;

    case 27:
        printf("fsync file name (%s)\n", inm_optarg);
        app_confp->fsync_file_name =  inm_optarg;
        fsync_fd = open(app_confp->fsync_file_name, O_RDWR);
        if (fsync_fd < 0) {
            printf("Failed to open file:%s Error:%s \n",app_confp->fsync_file_name,strerror(errno));
            return errno;
        } else {
            ret = fsync(fsync_fd);
            if (ret < 0) {
                printf("Error:%s %s\n",app_confp->fsync_file_name,strerror(errno));
                close(fsync_fd);
                return errno;
            } else {
                printf("Successfully fsync on file:%s\n",app_confp->fsync_file_name);
                close(fsync_fd);
		return 0;
            }
        }
        break;

    case 28:
        print_help(argc);
        break;

    case 29:
        app_confp->timeout = atoi(inm_optarg);
        break;

    case 30:
        app_confp->nr_vols = atoi(inm_optarg);
        break;

    case 31:
        app_confp->tag_notify = inm_optarg;
        break;

    case 32:
        app_confp->tag_pending_notify = inm_optarg;
        break;

    case 33:
        app_confp->pname = inm_optarg;
        break;

    case 34:
        app_confp->fname = inm_optarg;
        break;

    case 35:
        app_confp->sub_opcode = atoi(inm_optarg);
        break;

    case 36:
	app_confp->fail_commit_db = 1;
        break;

    case 37:
	        printf("\npnames:%s:\n", inm_optarg);
        if (!inm_optarg) {
            printf("Error: pnmaes list is empty!");
            exit(2);
        }
        if (strlen(inm_optarg)) {
            char *str, *str1, *str2, *ptr;
            int len = 0;
            int nvols = 0;
            offset = 0;

            str = inm_optarg;
            str1=str2 = NULL;

            str1 = str;
            while ((str2=strchr(str1,','))) {
                str1=++str2;
                nvols++;
            }
            nvols++;
            printf("Total number of volumes:%d\n", nvols);
            str1=str2 = NULL;

            app_confp->nr_pnames = nvols;
            app_confp->pnames = malloc(nvols*GUID_SIZE_IN_CHARS);
            memset(app_confp->pnames, 0, nvols*GUID_SIZE_IN_CHARS);
            ptr = app_confp->pnames;
            while (nvols) {
                if ((str1 = strchr(str,'_'))) {
                   str2 = NULL;
                   if ((str2 = strchr(str,','))) {
                      *str2 = '\0';
                   }
                   len = strlen(str1);
                   nvol++;

                   if (strncpy_s(ptr, GUID_SIZE_IN_CHARS, str1, len+1)) {
					exit(3);
				   }

                    if (str2) {
                        *str2 = ',';
                        str = str2 + 1;
				    }
					ptr += GUID_SIZE_IN_CHARS;
					nvols--;
				}
			}
		}
        break;

    case 38:
	    printf("\nold pname:%s:\n", inm_optarg);
        if (!inm_optarg) {
            printf("Error: old pname is empty!");
            exit(2);
        }
        if (strlen(inm_optarg)) {
            app_confp->pname = malloc(GUID_SIZE_IN_CHARS);
            memset(app_confp->pname, 0, GUID_SIZE_IN_CHARS);
            if (strncpy_s(app_confp->pname, GUID_SIZE_IN_CHARS, inm_optarg, GUID_SIZE_IN_CHARS - 1)) {
                exit(3);
            }
		}
        break;

    case 39:
	    printf("\nnew pname:%s:\n", inm_optarg);
        if (!inm_optarg) {
            printf("Error: old pname is empty!");
            exit(2);
        }
        if (strlen(inm_optarg)) {
            app_confp->pnames = malloc(GUID_SIZE_IN_CHARS);
            memset(app_confp->pnames, 0, GUID_SIZE_IN_CHARS);
            if (strncpy_s(app_confp->pnames, GUID_SIZE_IN_CHARS, inm_optarg, GUID_SIZE_IN_CHARS - 1)) {
                exit(3);
            }
		}
        break;

    default:
        printf("invalid option\n");
        return 1;
    }
    }
    if (app_confp->prof) {
        app_confp->resync = 0;
    }

//    printf("src-scsi_id:%s dst-scsi id:%s \n",
 //   	app_confp->mconf.src_scsi_id,
  //  	app_confp->mconf.dst_scsi_id);
     ret = inm_process_cmd(argc, argv, ++option_index);

    if (app_confp->mconf.src_guid_list) {
        free(app_confp->mconf.src_guid_list);
    }
    if (app_confp->mconf.dst_guid_list) {
        free(app_confp->mconf.dst_guid_list);
    }

    if (cmd) {
        free(cmd);
    }
    if (device) {
        free(device);
    }
    if (scsi_id) {
        free(scsi_id);
    }

    return ret;
}
