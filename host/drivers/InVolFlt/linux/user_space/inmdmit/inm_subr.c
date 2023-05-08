
#ident "@(#) $Source $Revision: 1.1.2.2 $ $Name: INT_BRANCH $ $Revision: 1.1.2.2 $"

#include "inm_filter.h"
#include "involflt.h"
#include "svdparse.h"
#include "inm_subr_v1.h"
#include "inm_subr_v2.h"
#include "inm_subr_v3.h"

#ifdef INM_LINUX
#include <sys/mount.h>
#include <linux/fs.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#endif

#ifdef INM_SOLARIS
#include <sys/dkio.h>
#include <sys/vtoc.h>
#include <sys/param.h>
#ifndef SOL_5.8
#include <sys/efi_partition.h>
#include <dlfcn.h>
#endif
#include <link.h>
#endif

#define isdigit(ch)     ((ch) >= '0' && (ch) <= '9')

#ifdef INM_AIX
#include <sys/stat.h>
#include <sys/vnode.h>
#include <sys/types.h>
#include <j2/j2_cntl.h>
#include <sys/vmount.h>
#include <sys/ioctl.h>
#include <sys/devinfo.h>
#endif

int inm_get_db_trans(char *, char *);
int inm_get_db_commit(char *, char *);
int get_idx_from_str(char *, struct str_idx *, int );

extern struct config *app_confp;
char cmd_line[1024];

enum common_attr_index {
CommonDataPoolSize = 1,
CommonDefaultLogDirectory,
CommonFreeThresholdForFileWrite,
CommonVolumeThresholdForFileWrite,
CommonDirtyBlockHighWaterMarkServiceNotStarted,
CommonDirtyBlockLowWaterMarkServiceRunning,
CommonDirtyBlockHighWaterMarkServiceRunning,
CommonDirtyBlockHighWaterMarkServiceShutdown,
CommonDirtyBlocksToPurgeWhenHighWaterMarkIsReached,
CommonMaximumBitmapBufferMemory,
CommonBitmap512KGranularitySize,
CommonVolumeDataFiltering,
CommonVolumeDataFilteringForNewVolumes,
CommonVolumeDataFiles,
CommonVolumeDataFilesForNewVolumes,
CommonVolumeDataToDiskLimitInMB,
CommonVolumeDataNotifyLimit,
CommonSequenceNumber,
CommonMaxDataSizeForDataModeDirtyBlock,
CommonVolumeResDataPoolSize,
CommonMaxDataPoolSize,
CommonCleanShutdown,
CommonMaxCoalescedMetaDataChangeSize,
CommonPercentChangeDataPoolSize,
CommonTimeReorgDataPoolSec,
CommonTimeReorgDataPoolFactor,
CommonVacpIObarrierTimeout,
CommonFsFreezeTimeout,
CommonVacpAppTagCommitTimeout,
CommonTrackRecursiveWrites,
CommonStablePages,
CommonVerifier,
CommonChainedIO,
CommonDIMIT_MAX_NR_ATTR,
};

struct str_idx common_attr[] =
{
{"", 0},
{"DataPoolSize", CommonDataPoolSize},
{"DefaultLogDirectory", CommonDefaultLogDirectory},
{"FreeThresholdForFileWrite", CommonFreeThresholdForFileWrite},
{"VolumeThresholdForFileWrite", CommonVolumeThresholdForFileWrite},
{"DirtyBlockHighWaterMarkServiceNotStarted", CommonDirtyBlockHighWaterMarkServiceNotStarted},
{"DirtyBlockLowWaterMarkServiceRunning", CommonDirtyBlockLowWaterMarkServiceRunning},
{"DirtyBlockHighWaterMarkServiceRunning", CommonDirtyBlockHighWaterMarkServiceRunning},
{"DirtyBlockHighWaterMarkServiceShutdown", CommonDirtyBlockHighWaterMarkServiceShutdown},
{"DirtyBlocksToPurgeWhenHighWaterMarkIsReached", CommonDirtyBlocksToPurgeWhenHighWaterMarkIsReached},
{"MaximumBitmapBufferMemory", CommonMaximumBitmapBufferMemory},
{"Bitmap512KGranularitySize", CommonBitmap512KGranularitySize},
{"VolumeDataFiltering", CommonVolumeDataFiltering},
{"VolumeDataFilteringForNewVolumes", CommonVolumeDataFilteringForNewVolumes},
{"VolumeDataFiles", CommonVolumeDataFiles},
{"VolumeDataFilesForNewVolumes", CommonVolumeDataFilesForNewVolumes},
{"VolumeDataToDiskLimitInMB", CommonVolumeDataToDiskLimitInMB},
{"VolumeDataNotifyLimit", CommonVolumeDataNotifyLimit},
{"SequenceNumber", CommonSequenceNumber},
{"MaxDataSizeForDataModeDirtyBlock", CommonMaxDataSizeForDataModeDirtyBlock},
{"VolumeResDataPoolSize", CommonVolumeResDataPoolSize},
{"MaxDataPoolSize", CommonMaxDataPoolSize},
{"CleanShutdown", CommonCleanShutdown},
{"MaxCoalescedMetaDataChangeSize", CommonMaxCoalescedMetaDataChangeSize},
{"PercentChangeDataPoolSize", CommonPercentChangeDataPoolSize},
{"TimeReorgDataPoolSec", CommonTimeReorgDataPoolSec},
{"TimeReorgDataPoolFactor", CommonTimeReorgDataPoolFactor},
{"VacpIObarrierTimeout", CommonVacpIObarrierTimeout},
{"FsFreezeTimeout", CommonFsFreezeTimeout},
{"VacpAppTagCommitTimeout", CommonVacpAppTagCommitTimeout},
{"TrackRecursiveWrites", CommonTrackRecursiveWrites},
{"StablePages", CommonStablePages},
{"Verifier", CommonVerifier},
{"ChainedIO", CommonChainedIO},
};

enum volume_attr_index {
    VolumeFilteringDisabled = 1,
    VolumeBitmapReadDisabled,
    VolumeBitmapWriteDisabled,
    VolumeDataFiltering,
    VolumeDataFiles,
    VolumeDataToDiskLimitInMB,
    VolumeDataNotifyLimitInKB,
    VolumeDataLogDirectory,
    VolumeBitmapGranularity,
    VolumeResyncRequired,
    VolumeOutOfSyncErrorCode,
    VolumeOutOfSyncErrorStatus,
    VolumeOutOfSyncCount,
    VolumeOutOfSyncTimestamp,
    VolumeOutOfSyncErrorDescription,
    VolumeFilterDevType,
    VolumeNblks,
    VolumeBsize,
    VolumeResDataPoolSize,
    VolumeMountPoint,
    VolumePrevEndTimeStamp,
    VolumePrevEndSequenceNumber,
    VolumePrevSequenceIDforSplitIO,
    VolumePTPath,
    VolumeATDirectRead,
    VolumeMirrorSourceList,
    VolumeMirrorDestinationList,
    VolumeMirrorDestinationScsiID,
    VolumeDiskFlags,
    VolumeIsDeviceMultipath,
    VolumeDeviceVendor,
    VolumeDevStartOff,
    VolumePTpathList,
    VolumePerfOptimization,
    VolumeMaxXferSz,
    VolumeRpoTimeStamp,
    VolumeDrainBlocked,
    VolumeDIMIT_MAX_NR_ATTR	
};

struct str_idx volume_attr[] =
{
{"", 0},
{"VolumeFilteringDisabled", VolumeFilteringDisabled},
{"VolumeBitmapReadDisabled", VolumeBitmapReadDisabled},
{"VolumeBitmapWriteDisabled", VolumeBitmapWriteDisabled},
{"VolumeDataFiltering", VolumeDataFiltering},
{"VolumeDataFiles", VolumeDataFiles},
{"VolumeDataToDiskLimitInMB", VolumeDataToDiskLimitInMB},
{"VolumeDataNotifyLimitInKB", VolumeDataNotifyLimitInKB},
{"VolumeDataLogDirectory", VolumeDataLogDirectory},
{"VolumeBitmapGranularity", VolumeBitmapGranularity},
{"VolumeResyncRequired", VolumeResyncRequired},
{"VolumeOutOfSyncErrorCode", VolumeOutOfSyncErrorCode},
{"VolumeOutOfSyncErrorStatus", VolumeOutOfSyncErrorStatus},
{"VolumeOutOfSyncCount", VolumeOutOfSyncCount},
{"VolumeOutOfSyncTimestamp", VolumeOutOfSyncTimestamp},
{"VolumeOutOfSyncErrorDescription", VolumeOutOfSyncErrorDescription},
{"VolumeFilterDevType", VolumeFilterDevType},
{"VolumeNblks", VolumeNblks},
{"VolumeBsize", VolumeBsize},
{"VolumeResDataPoolSize", VolumeResDataPoolSize},
{"VolumeMountPoint", VolumeMountPoint},
{"VolumePrevEndTimeStamp", VolumePrevEndTimeStamp},
{"VolumePrevEndSequenceNumber", VolumePrevEndSequenceNumber},
{"VolumePrevSequenceIDforSplitIO", VolumePrevSequenceIDforSplitIO},
{"VolumePTPath", VolumePTPath},
{"VolumeATDirectRead", VolumeATDirectRead},
{"VolumeMirrorSourceList", VolumeMirrorSourceList},
{"VolumeMirrorDestinationList", VolumeMirrorDestinationList},
{"VolumeMirrorDestinationScsiID", VolumeMirrorDestinationScsiID},
{"VolumeDiskFlags", VolumeDiskFlags},
{"VolumeIsDeviceMultipath", VolumeIsDeviceMultipath},
{"VolumeDeviceVendor", VolumeDeviceVendor},
{"VolumeDevStartOff", VolumeDevStartOff},
{"VolumePTpathList", VolumePTpathList},
{"VolumePerfOptimization", VolumePerfOptimization},
{"VolumeMaxXferSz", VolumeMaxXferSz},
{"VolumeRpoTimeStamp", VolumePerfOptimization},
{"VolumeDrainBlocked", VolumeDrainBlocked}
};

char *convert_path_to_str(char *path) {
    char *s = path;

    while(*s) {
        if (*s == '/')
	    *s='_';
	s++;
    }
    return path;
}

/* initialize config structure */
int
init_config(void)
{
	int ret = -1;

	app_confp=(struct config *) malloc(sizeof(struct config));
	if (app_confp) {
		memset(app_confp, 0, sizeof(struct config));
		app_confp->ctrl_dev_fd = open_ctrl_dev();
		if (app_confp->ctrl_dev_fd < 0) {
			perror("failed to open ctrl dev /dev/involflt");
			free(app_confp);
		} else {
			ret = 0;
		}
	}

	return ret;
} /* end of init_config */

/* function defs for base version == v1 */

/* print the tag string
** @bufp : tag string
** @len : tag string len
**/
void
print_buf(char *bufp , int len)
{
	int i=0;

	INM_TRACE_START();

	printf("\n****TAG START***************\n");
	for(i=0; i<len; i++) {
		printf("%c", bufp[i]);
	}
	printf("\n****TAG END****************\n");

	
	INM_TRACE_END("%d", 0);
} /* end of print_buf */

/*
 * open_ctrl_dev : opens the control device "/dev/involflt"
 * on success returns the file descriptor
 */ 
int
open_ctrl_dev(void)
{
	int ret = 0;
	int fd = open(INM_CTRL_DEV, O_RDWR);
	
	INM_TRACE_START();

	if (fd > 0) {
		ret=fd;
	}

	INM_TRACE_END("%d" , ret);

	return fd;
} /* end of open_ctrl_dev */	

/*
 * inm_ioctl_str : returns the string value of ioctl code 
 * @ioctl_code : int value of the ioctl code
 */
const char *
inm_ioctl_str(int ioctl_code)
{
	switch (ioctl_code) {
        case IOCTL_INMAGE_VOLUME_STACKING:
                return "IOCTL_INMAGE_VOLUME_STACKING";
                break;
        case IOCTL_INMAGE_MIRROR_VOLUME_STACKING:
                return "IOCTL_INMAGE_MIRROR_VOLUME_STACKING";
                break;
        case IOCTL_INMAGE_VOLUME_UNSTACKING:
                return "IOCTL_INMAGE_VOLUME_UNSTACKING";
                break;
        case IOCTL_INMAGE_PROCESS_START_NOTIFY:
                return "IOCTL_INMAGE_PROCESS_START_NOTIFY";
                break;
        case IOCTL_INMAGE_SERVICE_SHUTDOWN_NOTIFY:
                return "IOCTL_INMAGE_SERVICE_SHUTDOWN_NOTIFY";
                break;
        case IOCTL_INMAGE_STOP_FILTERING_DEVICE:
                return "IOCTL_INMAGE_STOP_FILTERING_DEVICE";
                break;
        case IOCTL_INMAGE_START_FILTERING_DEVICE:
                return "IOCTL_INMAGE_START_FILTERING_DEVICE";
                break;
        case IOCTL_INMAGE_START_MIRRORING_DEVICE:
                return "IOCTL_INMAGE_START_MIRRORING_DEVICE";
                break;
        case IOCTL_INMAGE_STOP_MIRRORING_DEVICE:
                return "IOCTL_INMAGE_STOP_MIRRORING_DEVICE";
                break;
        case IOCTL_INMAGE_START_FILTERING_DEVICE_V2:
                return "IOCTL_INMAGE_START_FILTERING_DEVICE_V2";
                break;
        case IOCTL_INMAGE_GET_DIRTY_BLOCKS_TRANS:
                return "IOCTL_INMAGE_GET_DIRTY_BLOCKS_TRANS";
                break;
        case IOCTL_INMAGE_COMMIT_DIRTY_BLOCKS_TRANS:
                return "IOCTL_INMAGE_COMMIT_DIRTY_BLOCKS_TRANS";
                break;
        case IOCTL_INMAGE_SET_VOLUME_FLAGS:
                return "IOCTL_INMAGE_SET_VOLUME_FLAGS";
                break;
        case IOCTL_INMAGE_GET_VOLUME_FLAGS:
                return "IOCTL_INMAGE_GET_VOLUME_FLAGS";
                break;
        case IOCTL_INMAGE_WAIT_FOR_DB:
                return "IOCTL_INMAGE_WAIT_FOR_DB";
                break;
        case IOCTL_INMAGE_CLEAR_DIFFERENTIALS:
                return "IOCTL_INMAGE_CLEAR_DIFFERENTIALS";
                break;
        case IOCTL_INMAGE_GET_NANOSECOND_TIME:
                return "IOCTL_INMAGE_GET_NANOSECOND_TIME";
                break;
        case IOCTL_INMAGE_UNSTACK_ALL:
                return "IOCTL_INMAGE_UNSTACK_ALL";
                break;
        case IOCTL_INMAGE_SYS_SHUTDOWN:
                return "IOCTL_INMAGE_SYS_SHUTDOWN";
                break;
        case IOCTL_INMAGE_TAG_VOLUME:
                return "IOCTL_INMAGE_TAG_VOLUME";
                break;
        case IOCTL_INMAGE_WAKEUP_ALL_THREADS:
                return "IOCTL_INMAGE_WAKEUP_ALL_THREADS";
                break;
        case IOCTL_INMAGE_GET_DB_NOTIFY_THRESHOLD:
                return "IOCTL_INMAGE_GET_DB_NOTIFY_THRESHOLD";
                break;
        case IOCTL_INMAGE_RESYNC_START_NOTIFICATION:
                return "IOCTL_INMAGE_RESYNC_START_NOTIFICATION";
                break;
        case IOCTL_INMAGE_RESYNC_END_NOTIFICATION:
                return "IOCTL_INMAGE_RESYNC_END_NOTIFICATION";
                break;
        case IOCTL_INMAGE_GET_DRIVER_VERSION:
                return "IOCTL_INMAGE_GET_DRIVER_VERSION";
                break;
        case IOCTL_INMAGE_SHELL_LOG:
                return "IOCTL_INMAGE_SHELL_LOG";
                break;
        case IOCTL_INMAGE_AT_LUN_CREATE:
                return "IOCTL_INMAGE_AT_LUN_CREATE";
                break;
        case IOCTL_INMAGE_AT_LUN_DELETE:
                return "IOCTL_INMAGE_AT_LUN_DELETE";
                break;
        case IOCTL_INMAGE_AT_LUN_LAST_WRITE_VI:
                return "IOCTL_INMAGE_AT_LUN_LAST_WRITE_VI";
                break;
        case IOCTL_INMAGE_GET_MONITORING_STATS:
                return "IOCTL_INMAGE_GET_MONITORING_STATS";
                break;
        case IOCTL_INMAGE_GET_BLK_MQ_STATUS:
                return "IOCTL_INMAGE_GET_BLK_MQ_STATUS";
                break;
        case IOCTL_INMAGE_GET_VOLUME_STATS_V2:
                return "IOCTL_INMAGE_GET_VOLUME_STATS_V2";
                break;
	default:
		return "Unknown";
		break;
	}

}

/*
 * inm_ioctl : wrapper around ioctl call
 */
int
inm_ioctl(int fd, int ioctl_code, void *argp)
{
	int ret = -1;

	if (app_confp->verbose || app_confp->trace) {
		printf("ioctl %s is issued\n", inm_ioctl_str(ioctl_code));
	}
	ret = ioctl(fd, ioctl_code, argp);

	if (app_confp->verbose || app_confp->trace) {
		printf("ioctl status = %d\n", ret);
	}

	return ret;
} /* end of inm_ioctl() */

#ifdef INM_SOLARIS
int
cnvt_dsk_to_rdsk(char *blkname, char *rawname)
{
 
    int ret = 1;
    int len = 0;
    char *str1 = NULL;
    char *str2 = NULL;
 
    str1 = (char *)malloc(INM_PATH_MAX);
    if (!str1) {
        ret = 1;
        perror("cnvt_dsk_to_rdsk error ENOMEM");
        goto out;
    }
    if (blkname == NULL || rawname == NULL){
        printf("null pointer passed either for blkname or rawname");
        ret = 1;
        goto out;
    }
    if (strcpy_s(str1, INM_PATH_MAX, blkname)) {
	printf("strcpy_s failed to copy");
        ret = 1;
        goto out;
    }
    while (str2 = strchr(str1, '/')) {
       if ((strncmp(str2, "/dsk", 4) == 0) || (strncmp(str2, "/dmp", 4) == 0)){
           len += str2 - str1 + 1;
           if (strncpy_s(rawname, len + 1, blkname, len)) {
		printf("strncpy_s failed to copy");
		ret = 1;
		goto out;
	   }
           rawname[len] = 'r';
           if (strcpy_s((rawname + len +1), strlen((str2 + 1)) + 1, (str2 + 1))) {
		printf("strcpy_s failed to copy");
		ret = 1;
		goto out;
	   }
           ret = 0;
           break;
       }
       len += str2 - str1 + 1;
       if (strcpy_s(str1, INM_PATH_MAX, str2+1)) {
	   printf("strcpy_s failed to copy");
	   ret = 1;
	   goto out;
       }
   }
 
   if (ret){
       printf("not a block device %s\n",blkname);
   }
 
out:
    if (str1) free(str1);
    return ret;
}

int
is_efi_raw_disk(char *rdisk)
{
        int ret = 0;
        char *rdisk_prefix = "/dev/rdsk/";
 
        if(!rdisk)
                goto out;
 
        if (!strncmp(rdisk, rdisk_prefix, strlen(rdisk_prefix))){
                size_t len = strlen(rdisk);
                const char *p = rdisk + (len - 1);
 
                while (p >= rdisk){
                        if (isdigit(*p))
                                p--;
                        else{
                                ret = ('d' == *p)? 1 : 0;
                                break;
                        }
                }
        }
out:
        return ret;
}

int
validate_device_size(int fd, const char *devicename,
                        unsigned long long size, unsigned int blksize)
{
    int bretval = 0;
    off_t lseekrval = -1, savecurrpos, savebackpos, seekoff;
    char *buf = NULL;
 
    savecurrpos = lseek(fd, 0, SEEK_CUR);
    if ((off_t)-1 != savecurrpos) {
        seekoff = size - blksize;
        lseekrval = lseek(fd, seekoff, SEEK_SET);
 
        if (seekoff == lseekrval) {
           buf = (char *)malloc(blksize);
           if (buf) {
               ssize_t bytesread;
 
               bytesread = read(fd, buf, blksize);
               if ((-1 != bytesread) && (bytesread == blksize))
                   bretval = 1;
               else
                   printf("read on device %s from %lld failed with errno = %d\n", devicename, seekoff, errno);
 
               free(buf);
           } else
               printf("malloc failed with errno =%d\n", errno);
 
           savebackpos = lseek(fd, savecurrpos, SEEK_SET);
           if (savebackpos != savecurrpos) {
               bretval = 0;
               printf("lseek on device %s to save back original position failed with errno = %d\n", devicename, errno);
           }
        } else
            printf("lseek on device %s to %lld failed with errno = %d\n", devicename, seekoff, errno);
    } else
        printf("lseek on device %s to save existing position failed with errno = %d\n", devicename, errno);
 
    return bretval;
}
 
#define SIZE_LIMIT_TOP     0x100000000000LL
#define NUM_SECTORS_PER_TB    (1LL << 31)

unsigned long long
read_and_find_size(int fd, const char *dskdevicename)
{
    char            buf[DEV_BSIZE];
    diskaddr_t      read_failed_from_top = 0;
    diskaddr_t      read_succeeded_from_bottom = 0;
    unsigned long long sizetoreturn = 0;
    diskaddr_t      current_file_position;
    unsigned long long sizefromlseekend = 0;
    unsigned long long sizefromread = 0;
    off_t lseekrval = 0;
    int fd_dsk = -1;
 
    fd_dsk = open(dskdevicename, O_RDONLY | O_NDELAY);
    if (-1 != fd_dsk) {
        lseekrval = lseek(fd_dsk, 0, SEEK_END);
        if ((off_t)-1 != lseekrval)
            sizefromlseekend = lseekrval;
 
        close(fd_dsk);
    } else {
        printf("lseek failed for device %s with errno = %d\n", dskdevicename, errno);
        return 0;
    }
 
    if (((llseek(fd, (offset_t)0, SEEK_SET)) == -1) || ((read(fd, buf, DEV_BSIZE)) == -1)) {
        printf("lseek failed for device %s with errno = %d\n", dskdevicename, errno);
        return 0;
    }
 
    for (current_file_position = NUM_SECTORS_PER_TB * 4; read_failed_from_top == 0 && current_file_position < SIZE_LIMIT_TOP; current_file_position += 4 * NUM_SECTORS_PER_TB) {
        if (((llseek(fd, (offset_t)(current_file_position * DEV_BSIZE), SEEK_SET)) == -1) || ((read(fd, buf, DEV_BSIZE)) != DEV_BSIZE))
            read_failed_from_top = current_file_position;
        else
            read_succeeded_from_bottom = current_file_position;
    }
    if (read_failed_from_top == 0) {
        printf("read failed for device %s with errno = %d\n", dskdevicename, errno);
        return 0;
    }
 
    while (read_failed_from_top - read_succeeded_from_bottom > 1) {
        current_file_position = read_succeeded_from_bottom + (read_failed_from_top - read_succeeded_from_bottom)/2;
        if (((llseek(fd, (offset_t)(current_file_position * DEV_BSIZE), SEEK_SET)) == -1) || ((read(fd, buf, DEV_BSIZE)) != DEV_BSIZE))
            read_failed_from_top = current_file_position;
        else
            read_succeeded_from_bottom = current_file_position;
    }

    sizefromread = (((unsigned long long)(read_succeeded_from_bottom + 1)) * ((unsigned long long)DEV_BSIZE));
 
    if (sizefromlseekend != sizefromread)
        printf("sizefromread = %llu, sizefromlseekend = %llu which do not match for device %s\n", sizefromread, sizefromlseekend, dskdevicename);
 
    /* Assign currently the size read */
    sizetoreturn = sizefromread;
 
    return sizetoreturn;
}
#endif

#ifdef INM_AIX
void cnvt_dsk_to_rdsk(char *dsk, char *rdsk)
{
        int len = strlen(dsk), i;

        for(i = len - 1; dsk[i] != '/'; i--);

        i++;
        strncpy_s(rdsk, i + 1, dsk, i);
        rdsk[i] = 'r';

        for(; dsk[i] != '\0'; i++)
                rdsk[i + 1] = dsk[i];

        rdsk[i + 1] = '\0';
}
#endif

int dev_size(inm_dev_info_t *dev_infop)
{
        int ret = 0;
 
#ifdef INM_LINUX
	unsigned long long size = 0;
	unsigned long nrsectors;
	int fd;

	fd  = open(&dev_infop->d_guid[0], O_RDONLY);
	if(-1 == fd){
		printf("Unable to open the file %s\n", &dev_infop->d_guid[0]);
		ret = -1;
		goto out;
	}

	if (ioctl(fd, BLKGETSIZE, &nrsectors))
		nrsectors = 0;

	if (ioctl(fd, BLKGETSIZE64, &size))
		size = 0;

	if(size == 0 || size == nrsectors)
		size = ((unsigned long long) nrsectors) * 512;

	close(fd);

        dev_infop->d_nblks = size >> 9;
        dev_infop->d_bsize = 512;

	printf("size = %llu\n", size);
out:
#endif
 
#ifdef INM_SOLARIS
        char *rdevname = NULL;
        int fd = 0, read_vtoc_retval;
        struct vtoc vtoc;
        unsigned long long size = 0;
 
        rdevname = (char *)malloc(INM_PATH_MAX);
        if (!rdevname) {
                printf("cnvt_dsk_to_rdsk error: memory allocation failure\n");
                ret = -1;
                goto out;
        }
 
        if(cnvt_dsk_to_rdsk(&dev_infop->d_guid[0] , rdevname)) {
                printf("error in converting dsk to rdsk for :%s:\n",dev_infop->d_guid);
                perror("error in converting dsk to rdsk");
                ret = -1;
                goto out;
        }
 
        fd  = open(rdevname, O_RDONLY | O_NDELAY);
        if(-1 == fd){
                printf("Unable to open the file %s\n", rdevname);
                ret = -1;
                goto out;
        }
 
        read_vtoc_retval = read_vtoc(fd, &vtoc);
        if (read_vtoc_retval < 0){
#ifndef SOL_5.8
                if (VT_ENOTSUP == read_vtoc_retval){
                        dk_gpt_t *vtoc = NULL;
                        void *libefihandle = NULL;
                        int (*ptr_efi_alloc_and_read)(int fd, dk_gpt_t **vtoc) = NULL;
                        void (*ptr_efi_free)(dk_gpt_t *vtoc) = NULL;
                        void *pv_efi_alloc_and_read = NULL, *pv_efi_free = NULL;
                        int efi_alloc_and_read_rval;
 
#ifdef _LP64
                        char *libefiso = "/lib/64/libefi.so";
#else
                        char *libefiso = "/lib/libefi.so";
#endif
 
                        libefihandle = dlopen(libefiso, RTLD_LAZY);
                        if(!libefihandle){
                                printf("Opening of library file %s is failed\n", libefiso);
                                ret = -1;
                                goto out;
                        }
 
                        pv_efi_alloc_and_read = dlsym(libefihandle, "efi_alloc_and_read");
                        pv_efi_free = dlsym(libefihandle, "efi_free");
                        if (!pv_efi_alloc_and_read || !pv_efi_free){
                                dlclose(libefihandle);
                                printf("efi_free or efi_alloc_and_read are not present in libefi.so\n");
                                ret = -1;
                                goto out;
                        }
 
                        ptr_efi_alloc_and_read = (int (*)(int fd, dk_gpt_t **vtoc))pv_efi_alloc_and_read;
                        ptr_efi_free = (void (*)(dk_gpt_t *vtoc))pv_efi_free;
 
                        efi_alloc_and_read_rval = ptr_efi_alloc_and_read(fd, &vtoc);
                        if (efi_alloc_and_read_rval < 0 || !vtoc){
                                dlclose(libefihandle);
                                printf("Reading efi label failed\n");
                                ret = -1;
                                goto out;
                        }
 
                        if(is_efi_raw_disk(rdevname) || V_UNASSIGNED == vtoc->efi_parts[efi_alloc_and_read_rval].p_tag){
                                size = ((unsigned long long)(vtoc->efi_last_lba + 1)) * ((unsigned long long)(vtoc->efi_lbasize));
                                if(!validate_device_size(fd, &dev_infop->d_guid[0], size, vtoc->efi_lbasize))
                                        size = read_and_find_size(fd, &dev_infop->d_guid[0]);
                        }else{
                                size = ((unsigned long long)vtoc->efi_parts[efi_alloc_and_read_rval].p_size) *
                                        ((unsigned long long)vtoc->efi_lbasize);
                        }
 
                        ptr_efi_free(vtoc);
                        dlclose(libefihandle);
                }else{
#endif
                        printf("read_vtoc failed with error = %d\n", errno);
                        ret = -1;
                        goto out;
#ifndef SOL_5.8
                }
#endif
        }else{
                if (0 == vtoc.v_sectorsz)
                        size = ((unsigned long long)vtoc.v_part[read_vtoc_retval].p_size) * ((unsigned long long)DEV_BSIZE);
                else
                        size = ((unsigned long long)vtoc.v_part[read_vtoc_retval].p_size) * ((unsigned long long)vtoc.v_sectorsz);
        }
 
        dev_infop->d_nblks = size >> 9;
        dev_infop->d_bsize = 512;
 
out:
        if(fd)
                close(fd);
 
        if(rdevname)
                free(rdevname);
#endif

#ifdef INM_AIX
	int fd, read_len, size_in_mb;;
	unsigned long long disk_size;
	char file_size[PATH_MAX + 1];
	char cmd_buf[PATH_MAX + 100];
	char rdevname[PATH_MAX + 1];
	struct devinfo devinfo;

	cnvt_dsk_to_rdsk(&dev_infop->d_guid[0], rdevname);
	printf("dsk = %s, rdsk = %s\n", &dev_infop->d_guid[0], rdevname);

	fd = open(rdevname, O_NDELAY | O_RDONLY);
	if(fd < 0){
		printf("Error in opening the file %s\n", rdevname);
		ret = -1;
		goto out;
	}

	if(ioctl(fd, IOCINFO, &devinfo) == -1){
		printf("IOCINFO ioctl failed\n");
		close(fd);
		ret = -1;
		goto out;
	}

	close(fd);

	switch(devinfo.devtype){
		case DD_DISK:   /* disk */
			disk_size = (unsigned long long)devinfo.un.dk.segment_size * (unsigned long long)devinfo.un.dk.segment_count;
			break;

		case DD_SCDISK: /* scsi disk */
			disk_size = (unsigned long long)devinfo.un.scdk.blksize * (unsigned long long)devinfo.un.scdk.numblks;
			break;

		default:
			disk_size = 0;
			ret = -1;
	}

	printf("Size of the disk %s is %llu\n", rdevname, disk_size);
	dev_infop->d_nblks = disk_size >> 9;
	dev_infop->d_bsize = 512;

	if(disk_size)
		goto out;

	sprintf(file_size, "/tmp/mount_point%d", getpid());
        sprintf(cmd_buf, "getconf DISK_SIZE %s > %s", &dev_infop->d_guid[0], file_size);
        system(cmd_buf);

        fd = open(file_size, O_RDONLY, 0666);
        if(fd < 0){
                printf("Error in opening the file %s\n", file_size);
                unlink(file_size);
                ret = -1;
                goto out;
        }

        read_len = read(fd, cmd_buf, PATH_MAX);
        if(read_len < 0){
                printf("Error in reading the file %s\n", file_size);
                close(fd);
                unlink(file_size);
                ret = -1;
                goto out;
        }

        close(fd);
        unlink(file_size);

        if(cmd_buf[read_len - 1] == '\n')
                read_len--;

        cmd_buf[read_len] = '\0';
        size_in_mb = atoi(cmd_buf);
        printf("dev = %s, size = %s, %d in MB\n", &dev_infop->d_guid[0], cmd_buf, size_in_mb);
        dev_infop->d_nblks = (((unsigned long long)size_in_mb) * 1024 * 2);
        dev_infop->d_bsize = 512;
out:
#endif

        return ret;
}
void
print_log_values(inm_u64_t *log, inm_u32_t capacity, inm_u64_t min, inm_u64_t max)
{
	int i, count = 0;
	inm_u64_t tm, tm_usec, tm_msec, tm_sec, tm_min;

	
	printf("\n Log values : \n");
	printf("min val = ");

	tm = min;
	tm_usec = tm % 1000;
	tm /= 1000;
	tm_msec =  tm % 1000;
	tm /= 1000;
	tm_sec = tm % 60;
	tm /= 60;
	tm_min = tm;


	if (tm_min) {
		printf(" : %u M", (inm_u32_t)tm_min);
	}

	if (tm_sec) {
		printf(" : %u S", (inm_u32_t)tm_sec);
	} 

	if (tm_msec) {
		printf(" : %u mS", (inm_u32_t)tm_msec);
	}

	if (tm_usec) {
		printf(" : %u uS", (inm_u32_t)tm_usec);
	}
	printf("\tmax val = ");
	tm = max;
	tm_usec = tm % 1000;
	tm /= 1000;
	tm_msec =  tm % 1000;
	tm /= 1000;
	tm_sec = tm % 60;
	tm /= 60;
	tm_min = tm;

	if (tm_min) {
		printf(" : %u M", (inm_u32_t)tm_min);
	}

	if (tm_sec) {
		printf(" : %u S", (inm_u32_t)tm_sec);
	} 

	if (tm_msec) {
		printf(" : %u mS", (inm_u32_t)tm_msec);
	}

	if (tm_usec) {
		printf(" : %u uS\n",(inm_u32_t) tm_usec);
	}

	for(i=0; i < capacity; i++) {
		if (!log[i]) {
			continue;
		}

		count++;
		tm = log[i];
		tm_usec = tm % 1000;
		tm /= 1000;
		tm_msec =  tm % 1000;
		tm /= 1000;
		tm_sec = tm % 60;
		tm /= 60;
		tm_min = tm;

		printf("%2d ", count);
		if (tm_min) {
			printf(" : %u M", (inm_u32_t)tm_min);
		}

		if (tm_sec) {
			printf(" : %u S", (inm_u32_t)tm_sec);
		} 

		if (tm_msec) {
			printf(" : %u mS", (inm_u32_t)tm_msec);
		}

		if (tm_usec) {
			printf(" : %u uS", (inm_u32_t)tm_usec);
		}
		printf("\n");
	
	}
}

		
void
print_distr_time(inm_u64_t *bkts, inm_u32_t *freq, uint32_t nitems)
{
	int i;
	uint32_t tm = 0;

	printf("Distribution : \n");
	for(i=0; i < nitems; i++) {
		if (!freq[i]) {
			continue;
		}

		tm = bkts[i]; 
//		printf("%llu : %u\n", bkts[i], freq[i]); 
		if (tm <= 10) {//usec
			printf("<=10uS : %u \n", freq[i]);
		} else if (tm <= 100) {//100usec
			printf("<=100uS: %u \n", freq[i]);
		} else if (tm <= 1000) {//ms
			printf("<=1mS  : %u \n", freq[i]);
		} else if (tm <= 10000) {//10ms
			printf("<=10mS : %u \n", freq[i]);
		} else if (tm <= 100000) {//100ms
			printf("<=100mS: %u \n", freq[i]);
		} else if (tm <= 1000000) { //1sec
			printf("<=1S   : %u \n", freq[i]);
		} else if (tm <= 10000000) {//10sec
			printf("<=10S  : %u \n", freq[i]);
		} else if (tm <= 30000000) {//30sec
			printf("<=30S  : %u \n", freq[i]);
		} else if (tm <= 60000000) {//1min
			printf("<=1M   : %u \n", freq[i]);
		} else if (tm <= 600000000) { //10min
			printf("<=10M  : %u \n", freq[i]);
		}
	}
}

char *
resolve_driver_pname(char *pname)
{
    char *d_pname = NULL;
    struct stat st;
    int error = 1;
    char ppath[PATH_MAX];
    char rpath[PATH_MAX];

    d_pname = malloc(INM_GUID_LEN_MAX);
    if (!d_pname)
       goto out; 
  
    sprintf_s(ppath, sizeof(ppath), "%s/%s", PERSISTENT_DIR, pname);

    /* resolve symlinks */
    realpath(ppath, rpath);

    if (lstat(rpath, &st))
       goto out; 
    
    if (!(S_ISDIR(st.st_mode))) {
        errno = ENOTDIR;
        goto out;
    }

    sprintf_s(d_pname, INM_GUID_LEN_MAX, "%s", basename(rpath));
    error = 0;
    
out:
    if (error && d_pname) {
        free(d_pname);
        d_pname = NULL;
    }

    return d_pname;
}

int
get_pname_mapping(void)
{
    vol_name_map_t vnmap;
    char *d_pname = NULL;
    int error = 0;

    if (!(app_confp->src_volp) &&
        !(app_confp->pname)) {
        fprintf(stderr, "Usage: inm_dmit --op=map_name "
               "[ --src_vol=<disk_name> || --pname=<persistent_name>]\n");
        exit(-1);
    }

    if (app_confp->src_volp) {
        vnmap.vnm_flags = INM_VOL_NAME_MAP_GUID;
        strcpy_s(vnmap.vnm_request, sizeof(vnmap.vnm_request), 
                 app_confp->src_volp);
    } else {
        d_pname = resolve_driver_pname(app_confp->pname);
        if (!d_pname) {
            perror("pname resolution");
            exit(-1);
        }

        vnmap.vnm_flags = INM_VOL_NAME_MAP_PNAME;
        strcpy_s(vnmap.vnm_request, sizeof(vnmap.vnm_request), d_pname);

        free(d_pname);
    }

    error = inm_ioctl(app_confp->ctrl_dev_fd, IOCTL_INMAGE_NAME_MAPPING, &vnmap);
    if (error) {
        perror("ioctl");
        exit(-1);
    }

    printf("%s\n", vnmap.vnm_response);
    return 0;
}

int
perform_lcw_op()
{
    int error = EINVAL;
    char *name = NULL;
    enum LCW_OP op = app_confp->sub_opcode;
    lcw_op_t lcw;

    if (!app_confp->fname && !app_confp->src_volp)
        goto out;

    if (op <= LCW_OP_NONE || op >= LCW_OP_MAX)
        goto out;

    if (app_confp->fname) {
        if (op != LCW_OP_MAP_FILE) 
            goto out;

        error = access(app_confp->fname, R_OK | W_OK);
        if (error)
            goto out;

        name = app_confp->fname;
    } else {
        if (op == LCW_OP_MAP_FILE)
            goto out;

        name = app_confp->src_volp;
    }

    lcw.lo_op = op;
    snprintf(lcw.lo_name.volume_guid, sizeof(lcw.lo_name.volume_guid), 
        "%s", name);

    error = inm_ioctl(app_confp->ctrl_dev_fd, IOCTL_INMAGE_LCW, &lcw);

out:
    if (error)  {
        errno = error;
        perror("LCW");
    }

    return error;
}

TAG_COMMIT_NOTIFY_INPUT *
get_tag_commit_notify_input(char *src_guid_list, int num_disks)
{
    char *buf = NULL;
    int buf_sz, i;
    char TagGUID[GUID_LEN + 1] = "abcdefghijklmnopqrstuvwxyz0123456789";
    VOLUME_GUID *guid;
    TAG_COMMIT_NOTIFY_INPUT *tag_drain_notify_input = NULL;

    if (!src_guid_list) {
        printf("pname list is missing\n");
        return NULL;
    }

    buf_sz = sizeof(TAG_COMMIT_NOTIFY_INPUT) + ((num_disks - 1) * sizeof(VOLUME_GUID)) +
         sizeof(TAG_COMMIT_NOTIFY_OUTPUT) + ((num_disks - 1) * sizeof(TAG_COMMIT_STATUS));

    buf = malloc(buf_sz);
    if (!buf) {
        printf("Failed to allocate memory\n");
        return NULL;
    }
    memset(buf, 0, buf_sz);

    tag_drain_notify_input = (TAG_COMMIT_NOTIFY_INPUT *) buf;
    memcpy_s(tag_drain_notify_input->TagGUID, GUID_LEN, TagGUID, GUID_LEN);
    printf("Tag GUID = ");
    for (i = 0; i < GUID_LEN; i++)
        printf("%c", tag_drain_notify_input->TagGUID[i]);
    printf("\n");
    tag_drain_notify_input->ulNumDisks = num_disks;
    guid = buf + sizeof(TAG_COMMIT_NOTIFY_INPUT) - sizeof(VOLUME_GUID);
    for (i = 1; i <= num_disks; i++) {
        strcpy(guid->volume_guid, src_guid_list);
        printf("GUID - %d = %s\n", i, guid->volume_guid);
        guid++;
        src_guid_list += GUID_SIZE_IN_CHARS;
    }

    return buf;
}

int process_tag_commit_notify_output(TAG_COMMIT_NOTIFY_OUTPUT *tag_drain_notify_output, int num_out_disks)
{
    int i, idx;
    TAG_COMMIT_STATUS *tag_commit_status;
    VM_CXFAILURE_STATS *vm_cx_stats;

    printf("The number of output disks are %d\n", tag_drain_notify_output->ulNumDisks);
    if (tag_drain_notify_output->ulNumDisks != num_out_disks) {
        printf("The number of output disks are not matching, input = %d, output = %d\n",
                    num_out_disks, tag_drain_notify_output->ulNumDisks);
        return -1;
    }

    tag_commit_status = tag_drain_notify_output->TagStatus;
    for (idx = 0; idx < num_out_disks; idx++) {
        DEVICE_CXFAILURE_STATS *dev_cx_stats = &tag_commit_status[idx].DeviceCxStats;

        printf("device = %s, Dev status = %d, Tag status = %d, TagInsertionTime = %llu, ulSequenceNumber = %llu\n",
            tag_commit_status[idx].DeviceId.volume_guid, tag_commit_status[idx].Status, tag_commit_status[idx].TagStatus,
            tag_commit_status[idx].TagInsertionTime, tag_commit_status[idx].TagSequenceNumber);

        printf("\n\nDisk = %s\n", dev_cx_stats->DeviceId.volume_guid);
        if (dev_cx_stats->ullFlags & DISK_CXSTATUS_NWFAILURE_FLAG)
            printf("Network failures reported\n");
        if (dev_cx_stats->ullFlags & DISK_CXSTATUS_PEAKCHURN_FLAG)
            printf("Peak Churn is reported\n");
        if (dev_cx_stats->ullFlags & DISK_CXSTATUS_CHURNTHROUGHPUT_FLAG)
            printf("Churn Throughput reported\n");
        if (dev_cx_stats->ullFlags & DISK_CXSTATUS_DISK_NOT_FILTERED)
            printf("Disk is not a filtered one\n");
        if (dev_cx_stats->ullFlags & DISK_CXSTATUS_DISK_REMOVED)
            printf("Disk removal reported\n");

        printf("CX Start / End TS = %llu / %llu\n", dev_cx_stats->CxStartTS, dev_cx_stats->CxEndTS);
        printf("First / Last nw failures TS = %llu / %llu\n", dev_cx_stats->firstNwFailureTS, dev_cx_stats->lastNwFailureTS);
        printf("Total NW errors = %llu\n", dev_cx_stats->ullTotalNWErrors);
        printf("Churn buckets: ");
        for (i = 0; i < DEFAULT_NR_CHURN_BUCKETS; i++)
            printf("%llu ", dev_cx_stats->ChurnBucketsMBps[i]);
        printf("\nFirst / last Peak Churn TS = %llu / %llu\n", dev_cx_stats->firstPeakChurnTS, dev_cx_stats->lastPeakChurnTS);
        printf("Max Peak / Excess Churn = %llu/%llu\n", dev_cx_stats->ullMaximumPeakChurnInBytes, dev_cx_stats->ullTotalExcessChurnInBytes);
        printf("Churn Throughput = %llu\n", dev_cx_stats->ullDiffChurnThroughputInBytes);
        printf("S2 latency = %llu\n", dev_cx_stats->ullMaxS2LatencyInMS);
    }

    printf("\n\nVM level CX status is as follows:\n");
    vm_cx_stats = &tag_drain_notify_output->vmCxStatus;
    if (vm_cx_stats->ullFlags & VM_CXSTATUS_PEAKCHURN_FLAG)
        printf("Peak Churn is reported\n");
    if (vm_cx_stats->ullFlags & VM_CXSTATUS_CHURNTHROUGHPUT_FLAG)
        printf("Churn Throughput reported\n");
    if (vm_cx_stats->ullFlags & VM_CXSTATUS_TIMEJUMP_FWD_FLAG || vm_cx_stats->ullFlags & VM_CXSTATUS_TIMEJUMP_BCKWD_FLAG)
        printf("Time Jump reported\n");

    printf("Flags = %llu\n", vm_cx_stats->ullFlags);
    printf("CX Start / End TS = %llu / %llu\n", vm_cx_stats->CxStartTS, vm_cx_stats->CxEndTS);
    printf("Transaction ID = %llu", vm_cx_stats->ulTransactionID);
    printf("Churn buckets: ");
    for (idx = 0; idx < DEFAULT_NR_CHURN_BUCKETS; idx++)
        printf("%llu ", vm_cx_stats->ChurnBucketsMBps[idx]);
    printf("\nFirst / last Peak Churn TS = %llu / %llu", vm_cx_stats->firstPeakChurnTS, vm_cx_stats->lastPeakChurnTS);
    printf("Max Peak / Excess Churn = %llu/%llu\n", vm_cx_stats->ullMaximumPeakChurnInBytes, vm_cx_stats->ullTotalExcessChurnInBytes);
    printf("Churn Throughput = %llu\n", vm_cx_stats->ullDiffChurnThroughputInBytes);
    printf("Time jump TS / Actual Jump in MS = %llu / %llu\n", vm_cx_stats->TimeJumpTS, vm_cx_stats->ullTimeJumpInMS);
    printf("No. of Tag failures = %llu\n", vm_cx_stats->ullNumOfConsecutiveTagFailures);
    printf("S2 latency = %llu\n", vm_cx_stats->ullMaxS2LatencyInMS);
    printf("No. of output disks = %u", vm_cx_stats->ullNumDisks);

    return 0;
}

int get_protected_volume_list(GET_VOLUME_LIST *vol_list)
{
	int ret;

	vol_list->buf_len = INITIAL_BUFSZ_FOR_VOL_LIST;
	vol_list->bufp = NULL;
	do{
		vol_list->bufp = (void *)malloc(vol_list->buf_len);
		if(!vol_list->bufp){
			printf("can not allocate space, command failed");
			ret = ENOMEM;
			break;
		}
		if((ret = inm_ioctl(app_confp->ctrl_dev_fd,
		     IOCTL_INMAGE_GET_PROTECTED_VOLUME_LIST,
		     (void *)vol_list))) {
			if( errno == EAGAIN){
				free(vol_list->bufp);
				vol_list->bufp = NULL;
				continue;
			}
			printf("command failed with error %d, syslog might have some details.\n", errno);
			ret = -1;
			break;
		} else {
			ret = 0;
			break;
		}
	} while(ret);

	return ret;
}

int remove_persistent_store(void)
{
	DIR *d;
	struct dirent *dir;
	char pname[4096];

	printf("Started removing the persistent store\n");
	d = opendir(PERSISTENT_DIR);
	if (!d) {
		printf("Failed to open the persistent directory %s\n", PERSISTENT_DIR);
		return 1;
	}

	while ((dir = readdir(d)) != NULL) {
		DIR *dpname;
		struct dirent *pdir;
		char pentry[4096];

		if (!strcmp(dir->d_name, ".") || !strcmp(dir->d_name, "..") || !strcmp(dir->d_name, "common")) {
			printf("Ignoring the entry %s\n", dir->d_name);
			continue;
		}

		pname[0] = '\0';
		sprintf(pname, "%s/%s", PERSISTENT_DIR, dir->d_name);

		dpname = opendir(pname);
		if (!dpname) {
			printf("Failed to open the directory %s\n", pname);
			return 1;
		}

		while ((pdir = readdir(dpname)) != NULL) {
			if (!strcmp(pdir->d_name, ".") || !strcmp(pdir->d_name, ".."))
				continue;

			sprintf(pentry, "%s/%s", pname, pdir->d_name);
			if (remove(pentry)) {
				perror("Removing of file failed");
				printf("Removing of file %s failed\n", pentry);
				closedir(dpname);
				return 1;
			}
		}

		closedir(dpname);

		printf("Removing the entry %s\n", pname);
		if (remove(pname)) {
			perror("Removing of directory failed");
			closedir(d);
			return 1;
		}
	}

	printf("Done with removal of persistent store\n");
	closedir(d);
	return 0;
}

/*
 * process various types of cmds passed through cmd line
 */
int
inm_process_cmd(int argc, char **argv, int start_idx)
{
	char *dev_pathp = NULL;
	int ret = -1;
	VOLUME_GUID guid;
	long long time;
	SHUTDOWN_NOTIFY_INPUT stop_notify;
	LUN_CREATE_INPUT add_lun;
	LUN_DELETE_INPUT del_lun;
	void * get_stat = NULL;
	VOLUME_STATS	tg_stat;
	GET_VOLUME_LIST	vol_list;
	inm_attribute_t	*attr = NULL;
	int argv_index = 0;
        inm_dev_info_t dev_info;
        DRIVER_VERSION version;

	unsigned int total_buffer_size = 0;
	unsigned int flags = 0;
	unsigned short num_vols = 1;
	unsigned short num_tags = 1;
	char * buffer = NULL;
	unsigned int current_buf_size = 0;
	unsigned short len;
	inm_at_lun_reconfig_t at_lun_info;
        freeze_info_t freeze_info = {0};
        thaw_info_t   thaw_info = {0};
        char *vol_name = NULL;
        int  numvol = 0;

#ifdef INM_AIX
	char cmd_buf[PATH_MAX + 100];
	char mount_point[PATH_MAX + 1];
	int fd_mnt, read_len;
	struct stat mnt_stat;
#endif

	INM_TRACE_START();

        freeze_info.vol_info = NULL;
        thaw_info.vol_info = NULL;

	if (app_confp->attribute) {
		attr = (inm_attribute_t *) malloc (sizeof(inm_attribute_t));
		argv_index = argv_index + 1;
		if (strcmp(argv[++argv_index],"volume") == 0){
			if (strcpy_s(attr->guid.volume_guid, GUID_SIZE_IN_CHARS, argv[++argv_index])) {
				printf("strcpy_s failed");
				ret = -1;
				goto out;
			}
			if ((attr->type = get_idx_from_str(argv[++argv_index],
			   volume_attr, VolumeDIMIT_MAX_NR_ATTR ))){
				attr->type--;
			} else {
				printf("unrecongnized volume attribute\n");	
			}
		} else {
			if (strcpy_s(attr->guid.volume_guid, GUID_SIZE_IN_CHARS, "common")) {
				printf("strcpy_s failed");
				ret = -1;
				goto out;
			}
			if ((attr->type = get_idx_from_str(argv[argv_index],
			   common_attr, CommonDIMIT_MAX_NR_ATTR ))){
				attr->type--;
			} else {
				printf("unrecongnized common attribute\n");	
			}
		}
		attr->bufp = malloc(INM_PAGESZ);
		attr->buflen = INM_PAGESZ;
		switch (app_confp->attribute) {
		case GET_ATTR:
			attr->why = GET_ATTR;
			if(inm_ioctl(app_confp->ctrl_dev_fd,
			    IOCTL_INMAGE_GET_SET_ATTR,
			    (void *)attr)) {
				printf("IOCTL_INMAGE_GET_SET_ATTR ioctl failed\n");
				ret = -1;
			} else {
				printf("%s",(char *)attr->bufp);
				ret = 0;
			}
		break;    
		case SET_ATTR:
			attr->why = SET_ATTR;
			if (strcpy_s(attr->bufp, INM_PAGESZ, argv[++argv_index])) {
				printf("strcpy_s failed");
				ret = -1;
				goto out;
			}
			attr->buflen = INM_MIN (strlen(argv[argv_index]) , attr->buflen);
			if(inm_ioctl(app_confp->ctrl_dev_fd,
			    IOCTL_INMAGE_GET_SET_ATTR,
			    (void *)attr)) {
				printf("IOCTL_INMAGE_GET_SET_ATTR ioctl failed\n");
				ret = -1;
			} else {
				ret = 0;
			}

		}
		goto out;
	}

	if(app_confp->stat){
		if(app_confp->stat == DRIVER_STAT){
			get_stat = malloc(INM_PAGESZ);
			if(inm_ioctl(app_confp->ctrl_dev_fd,
			    IOCTL_INMAGE_GET_GLOBAL_STATS,
			    (void *)get_stat)) {
				printf("IOCTL_INMAGE_GET_GLOBAL_STATS ioctl failed\n");
				ret = -1;
			} else {
				printf("%s",(char *)get_stat);
				ret = 0;
			}
		}
		if(app_confp->stat == TARGET_STAT){
			tg_stat.bufp = (void *)malloc(INM_PAGESZ);
			app_confp->tgt_volp[GUID_SIZE_IN_CHARS-1] = '\0';
			if (strcpy_s(tg_stat.guid.volume_guid, GUID_SIZE_IN_CHARS, app_confp->tgt_volp)) {
				printf("strcpy_s failed");
				ret = -1;
				goto out;
			}
			tg_stat.buf_len = INM_PAGESZ;

			if(inm_ioctl(app_confp->ctrl_dev_fd,
			    IOCTL_INMAGE_GET_VOLUME_STATS,
			    (void *)&tg_stat)) {

				printf("IOCTL_INMAGE_GET_VOLUME_STATS ioctl failed\n");
				ret = -1;
			} else {
				printf("%s",(char *)tg_stat.bufp);
				ret = 0;
			}
		}
		if(app_confp->stat == PROT_VOL_LIST){
#ifdef INM_AIX
#ifndef __64BIT__
			vol_list.padding = 0;
#endif
#endif
			ret = get_protected_volume_list(&vol_list);
			if (!ret && vol_list.bufp)
			 	printf("%s",(char *)vol_list.bufp);

			if (vol_list.bufp)
				free(vol_list.bufp);
		}
		goto out;
	}

	switch (app_confp->op_code) {
        case WAKEUP_MONITOR_THREAD:
            ret = inm_ioctl(app_confp->ctrl_dev_fd, IOCTL_INMAGE_WAKEUP_GET_CXSTATS_NOTIFY_THREAD, NULL);
            if (ret < 0) {
                perror("IOCTL_INMAGE_WAKEUP_GET_CXSTATS_NOTIFY_THREAD ioctl failed\n");
            }
            break;

        case GET_CX_STATUS_OP:
	{
		int num_prot_disks;
        int num_out_disks;
		int idx, i;
        char *src_guid_list = app_confp->pnames;
        GET_CXFAILURE_NOTIFY *get_cx_notify;
		VOLUME_GUID          *guid;
		VM_CXFAILURE_STATS   *vm_cx_stats;
		DEVICE_CXFAILURE_STATS *dev_cx_stats;
		char *buf = NULL;
        int buf_sz;
        int ret;

        if (!src_guid_list) {
            printf("pname list is missing\n");
            ret = -1;
            break;
        }

        num_prot_disks = app_confp->nr_pnames;
        num_out_disks = num_prot_disks;

retry:
        printf("num_out_disks = %d, num_prot_disks = %d", num_out_disks, num_prot_disks);
		buf_sz = sizeof(GET_CXFAILURE_NOTIFY) + ((num_prot_disks - 1) * sizeof(VOLUME_GUID)) +
				sizeof(VM_CXFAILURE_STATS) + ((num_out_disks - 1) * sizeof(DEVICE_CXFAILURE_STATS));

		buf = malloc(buf_sz);
		if (!buf) {
			ret = 1;
			printf("Failed to allocate memory\n");
			break;
		}
		memset(buf, 0, buf_sz);

		get_cx_notify = (GET_CXFAILURE_NOTIFY *) buf;
		get_cx_notify->ullMinConsecutiveTagFailures = 3;
                get_cx_notify->ullMaxVMChurnSupportedMBps = 50;
		get_cx_notify->ullMaxDiskChurnSupportedMBps = 25;
		get_cx_notify->ullMaximumTimeJumpFwdAcceptableInMs = 3000;
		get_cx_notify->ullMaximumTimeJumpBwdAcceptableInMs = 60000;
		get_cx_notify->ulNumberOfOutputDisks = num_out_disks;
		get_cx_notify->ulNumberOfProtectedDisks = num_prot_disks;

		guid = buf + sizeof(GET_CXFAILURE_NOTIFY) - sizeof(VOLUME_GUID);
        printf("buf = %p, guid = %p\n", buf, guid);

		for (idx = 1; idx <= num_prot_disks; idx++) {
			strcpy(guid->volume_guid, src_guid_list);
            printf("GUID - %d = %s\n", idx, guid);
			guid++;
            src_guid_list += GUID_SIZE_IN_CHARS;
		}

		vm_cx_stats = buf + sizeof(GET_CXFAILURE_NOTIFY) + (num_prot_disks - 1) * sizeof(VOLUME_GUID);

        	while (1) {
			dev_cx_stats = buf + sizeof(GET_CXFAILURE_NOTIFY) + (num_prot_disks - 1) * sizeof(VOLUME_GUID) +
							sizeof(VM_CXFAILURE_STATS) - sizeof(DEVICE_CXFAILURE_STATS);
            printf("vm_cx_stats = %p, dev_cx_stats = %p\n", vm_cx_stats, dev_cx_stats);
			memset(dev_cx_stats, 0, sizeof(*dev_cx_stats));

			ret = inm_ioctl(app_confp->ctrl_dev_fd, IOCTL_INMAGE_GET_CXSTATS_NOTIFY, buf);
			if (ret < 0) {
				int err = errno;

				printf("err = %d\n", err);

                if (errno == EAGAIN) {
					free(buf);
					num_out_disks += 4;
					goto retry;
                }

				perror("IOCTL_INMAGE_GET_CXSTATS_NOTIFY ioctl failed\n");
				sleep(3);
				continue;
			}

			if (vm_cx_stats->ullFlags & VM_CXSTATUS_PEAKCHURN_FLAG)
				printf("Peak Churn is reported\n");
			if (vm_cx_stats->ullFlags & VM_CXSTATUS_CHURNTHROUGHPUT_FLAG)
				printf("Churn Throughput reported\n");
			if (vm_cx_stats->ullFlags & VM_CXSTATUS_TIMEJUMP_FWD_FLAG || vm_cx_stats->ullFlags & VM_CXSTATUS_TIMEJUMP_BCKWD_FLAG)
				printf("Time Jump reported\n");

			printf("Flags = %llu\n", vm_cx_stats->ullFlags);
                        printf("CX Start / End TS = %llu / %llu\n", vm_cx_stats->CxStartTS, vm_cx_stats->CxEndTS);
			printf("Transaction ID = %llu", vm_cx_stats->ulTransactionID);
			printf("Churn buckets: ");
			for (idx = 0; idx < DEFAULT_NR_CHURN_BUCKETS; idx++)
				printf("%llu ", vm_cx_stats->ChurnBucketsMBps[idx]);
			printf("\nFirst / last Peak Churn TS = %llu / %llu", vm_cx_stats->firstPeakChurnTS, vm_cx_stats->lastPeakChurnTS);
 			printf("Max Peak / Excess Churn = %llu/%llu\n", vm_cx_stats->ullMaximumPeakChurnInBytes, vm_cx_stats->ullTotalExcessChurnInBytes);
            printf("Churn Throughput = %llu\n", vm_cx_stats->ullDiffChurnThroughputInBytes);
			printf("Time jump TS / Actual Jump in MS = %llu / %llu\n", vm_cx_stats->TimeJumpTS, vm_cx_stats->ullTimeJumpInMS);
			printf("No. of Tag failures = %llu\n", vm_cx_stats->ullNumOfConsecutiveTagFailures);
			printf("S2 latency = %llu\n", vm_cx_stats->ullMaxS2LatencyInMS);
			printf("No. of output disks = %u", vm_cx_stats->ullNumDisks);

			for (i = 0; i < vm_cx_stats->ullNumDisks; i++) {
				printf("\n\nDisk = %s\n", dev_cx_stats->DeviceId.volume_guid);
				if (dev_cx_stats->ullFlags & DISK_CXSTATUS_NWFAILURE_FLAG)
					printf("Network failures reported\n");
				if (dev_cx_stats->ullFlags & DISK_CXSTATUS_PEAKCHURN_FLAG)
					printf("Peak Churn is reported\n");
				if (dev_cx_stats->ullFlags & DISK_CXSTATUS_CHURNTHROUGHPUT_FLAG)
					printf("Churn Throughput reported\n");
				if (dev_cx_stats->ullFlags & DISK_CXSTATUS_DISK_NOT_FILTERED)
					printf("Disk is not a filtered one\n");
				if (dev_cx_stats->ullFlags & DISK_CXSTATUS_DISK_REMOVED)
					printf("Disk removal reported\n");

				printf("CX Start / End TS = %llu / %llu\n", dev_cx_stats->CxStartTS, dev_cx_stats->CxEndTS);
				printf("First / Last nw failures TS = %llu / %llu\n", dev_cx_stats->firstNwFailureTS, dev_cx_stats->lastNwFailureTS);
				printf("Total NW errors = %llu\n", dev_cx_stats->ullTotalNWErrors);
				printf("Churn buckets: ");
				for (idx = 0; idx < DEFAULT_NR_CHURN_BUCKETS; idx++)
					printf("%llu ", dev_cx_stats->ChurnBucketsMBps[idx]);
				printf("\nFirst / last Peak Churn TS = %llu / %llu\n", dev_cx_stats->firstPeakChurnTS, dev_cx_stats->lastPeakChurnTS);
				printf("Max Peak / Excess Churn = %llu/%llu\n", dev_cx_stats->ullMaximumPeakChurnInBytes, dev_cx_stats->ullTotalExcessChurnInBytes);
            	printf("Churn Throughput = %llu\n", dev_cx_stats->ullDiffChurnThroughputInBytes);
				printf("S2 latency = %llu\n", dev_cx_stats->ullMaxS2LatencyInMS);

				dev_cx_stats++;
			}

			get_cx_notify->ulTransactionID = vm_cx_stats->ulTransactionID;
			get_cx_notify->ullFlags = CXSTATUS_COMMIT_PREV_SESSION;
		}
        }
        break;

        case WAKEUP_TAG_DRAIN_NOTIFY:
            ret = inm_ioctl(app_confp->ctrl_dev_fd, IOCTL_INMAGE_WAKEUP_TAG_DRAIN_NOTIFY_THREAD, NULL);
            if (ret < 0) {
                perror("IOCTL_INMAGE_WAKEUP_TAG_DRAIN_NOTIFY_THREAD ioctl failed\n");
            }
            break;

        case TAG_DRAIN_NOTIFY_OP:
        {
            TAG_COMMIT_NOTIFY_OUTPUT *tag_drain_notify_output;
            int num_prot_disks = app_confp->nr_pnames;
            int num_out_disks = num_prot_disks;
            char *src_guid_list = app_confp->pnames;
            char *buf;

            buf = get_tag_commit_notify_input(src_guid_list, num_prot_disks);
            if(!buf) {
                ret = -1;
                break;
            }

            ret = inm_ioctl(app_confp->ctrl_dev_fd, IOCTL_INMAGE_TAG_DRAIN_NOTIFY, buf);
            if (ret < 0) {
                int err = errno;
                printf("err = %d\n", err);
                perror("IOCTL_INMAGE_TAG_DRAIN_NOTIFY ioctl failed\n");
            }

            tag_drain_notify_output = buf + sizeof(TAG_COMMIT_NOTIFY_INPUT) + ((num_prot_disks - 1) * sizeof(VOLUME_GUID));
            if(process_tag_commit_notify_output(tag_drain_notify_output, num_out_disks)) {
                ret = -1;
                break;
            }
        }
        break;

        case MODIFY_PERSISTENT_DEVICE_NAME_OP:
        {

            if (!app_confp->src_volp) {
                printf("src_vol is missing\n");
                ret = -1;
                break;
            }
            if (!app_confp->pname) {
                printf("old_pname is missing\n");
                ret = -1;
                break;
            }
            if (!app_confp->pnames) {
                printf("new_pname is missing\n");
                ret = -1;
                break;
            }
            printf("source volume = %s, old pname = %s, new pname = %s\n",
                app_confp->src_volp, app_confp->pname, app_confp->pnames);

            char *buf = NULL;
            MODIFY_PERSISTENT_DEVICE_NAME_INPUT *modify_pname_input = NULL;

            buf = malloc(sizeof(MODIFY_PERSISTENT_DEVICE_NAME_INPUT));
            if (!buf) {
                ret = 1;
                printf("Failed to allocate memory\n");
                break;
            }
            memset(buf, 0, sizeof(MODIFY_PERSISTENT_DEVICE_NAME_INPUT));
            modify_pname_input = (MODIFY_PERSISTENT_DEVICE_NAME_INPUT *) buf;

		    memcpy_s(modify_pname_input->DevName.volume_guid, GUID_SIZE_IN_CHARS,
                        app_confp->src_volp, GUID_SIZE_IN_CHARS);
		    memcpy_s(modify_pname_input->OldPName.volume_guid, GUID_SIZE_IN_CHARS,
                        app_confp->pname, GUID_SIZE_IN_CHARS);
		    memcpy_s(modify_pname_input->NewPName.volume_guid, GUID_SIZE_IN_CHARS,
                        app_confp->pnames, GUID_SIZE_IN_CHARS);
            ret = inm_ioctl(app_confp->ctrl_dev_fd, IOCTL_INMAGE_MODIFY_PERSISTENT_DEVICE_NAME, buf);
            if (ret < 0) {
                perror("IOCTL_INMAGE_MODIFY_PERSISTENT_DEVICE_NAME ioctl failed\n");
            }
        }
        break;

        case BLOCK_DRAIN_OP:
        {
            TAG_COMMIT_NOTIFY_INPUT *tag_drain_notify_input;
            TAG_COMMIT_NOTIFY_OUTPUT *tag_drain_notify_output;
            int num_prot_disks = app_confp->nr_pnames;
            int num_out_disks = num_prot_disks;
            char *src_guid_list = app_confp->pnames;
            char *buf;

            buf = get_tag_commit_notify_input(src_guid_list, num_prot_disks);
            if(!buf) {
                ret = -1;
                break;
            }
            tag_drain_notify_input = (TAG_COMMIT_NOTIFY_INPUT *)buf;
            tag_drain_notify_input->ulFlags |= TAG_COMMIT_NOTIFY_BLOCK_DRAIN_FLAG;

            ret = inm_ioctl(app_confp->ctrl_dev_fd, IOCTL_INMAGE_TAG_DRAIN_NOTIFY, buf);
            if (ret < 0) {
                int err = errno;
                printf("err = %d\n", err);
                perror("IOCTL_INMAGE_TAG_DRAIN_NOTIFY ioctl failed\n");
            }

            tag_drain_notify_output = buf + sizeof(TAG_COMMIT_NOTIFY_INPUT) + ((num_prot_disks - 1) * sizeof(VOLUME_GUID));
            if(process_tag_commit_notify_output(tag_drain_notify_output, num_out_disks)) {
                ret = -1;
                break;
            }
        }
        break;

        case UNBLOCK_DRAIN_OP:
        {
            SET_DRAIN_STATE_INPUT *unblock_drain_input;
            SET_DRAIN_STATE_OUTPUT *unblock_drain_output;
            int num_disks = app_confp->nr_pnames;
            char *src_guid_list = app_confp->pnames;
            char *buf;
            VOLUME_GUID *guid;
            int buf_sz, i;

            if (!src_guid_list) {
                printf("pname list is missing\n");
                ret = -1;
                break;
            }

            buf_sz = sizeof(SET_DRAIN_STATE_INPUT) + ((num_disks - 1) * sizeof(VOLUME_GUID)) +
                sizeof(SET_DRAIN_STATE_OUTPUT) + ((num_disks - 1) * sizeof(SET_DRAIN_STATUS));

            buf = malloc(buf_sz);
            if (!buf) {
                printf("Failed to allocate memory\n");
                ret = -1;
                break;
            }
            memset(buf, 0, buf_sz);

            unblock_drain_input = (SET_DRAIN_STATE_INPUT *) buf;
            unblock_drain_input->ulNumDisks = num_disks;
            guid = buf + sizeof(SET_DRAIN_STATE_INPUT) - sizeof(VOLUME_GUID);
            for (i = 1; i <= num_disks; i++) {
                strcpy(guid->volume_guid, src_guid_list);
                printf("GUID - %d = %s\n", i, guid->volume_guid);
                guid++;
                src_guid_list += GUID_SIZE_IN_CHARS;
            }

            ret = inm_ioctl(app_confp->ctrl_dev_fd, IOCTL_INMAGE_SET_DRAIN_STATE, buf);
            if (ret < 0) {
                int err = errno;
                printf("err = %d\n", err);
                perror("IOCTL_INMAGE_SET_DRAIN_STATE ioctl failed\n");
            }

            unblock_drain_output = buf + sizeof(SET_DRAIN_STATE_INPUT) +
                ((num_disks - 1) * sizeof(VOLUME_GUID));

            printf("The number of output disks are %d\n", unblock_drain_output->ulNumDisks);
            if (unblock_drain_output->ulNumDisks != num_disks) {
                printf("The number of output disks are not matching, input = %d, output = %d\n",
                            num_disks, unblock_drain_output->ulNumDisks);
                ret = -1;
                break;
            }

            for (i = 0; i < num_disks; i++) {
                if(unblock_drain_output->diskStatus[i].Status == SET_DRAIN_STATUS_SUCCESS) {
                    printf("Unblock drain for disk : %s succeeded\n",
                        unblock_drain_output->diskStatus[i].DeviceId.volume_guid);
                }
                else if(unblock_drain_output->diskStatus[i].Status == 
                    SET_DRAIN_STATUS_DEVICE_NOT_FOUND) {
                    printf("Unblock drain for disk : %s failed as device was not found\n",
                        unblock_drain_output->diskStatus[i].DeviceId.volume_guid);
                }
                else if(unblock_drain_output->diskStatus[i].Status == 
                    SET_DRAIN_STATUS_PERSISTENCE_FAILED) {
                    printf("Unblock drain persistence failed for disk : %s\n",
                        unblock_drain_output->diskStatus[i].DeviceId.volume_guid);
                }
                else if (unblock_drain_output->diskStatus[i].Status == SET_DRAIN_STATUS_UNKNOWN) {
                    printf("Unblock drain for disk : %s failed with unknown error\n",
                        unblock_drain_output->diskStatus[i].DeviceId.volume_guid);
                }
            }
        }
        break;

        case GET_DRAIN_STATE_OP:
        {
            GET_DISK_STATE_INPUT *drain_state_input;
            GET_DISK_STATE_OUTPUT *drain_state_output;
            int num_disks = app_confp->nr_pnames;
            char *src_guid_list = app_confp->pnames;
            char *buf;
            VOLUME_GUID *guid;
            int buf_sz, i;

            if (!src_guid_list) {
                printf("pname list is missing\n");
                ret = -1;
                break;
            }

            buf_sz = sizeof(GET_DISK_STATE_INPUT) + ((num_disks - 1) * sizeof(VOLUME_GUID)) +
                sizeof(GET_DISK_STATE_OUTPUT) + ((num_disks - 1) * sizeof(DISK_STATE));

            buf = malloc(buf_sz);
            if (!buf) {
                printf("Failed to allocate memory\n");
                ret = -1;
                break;
            }
            memset(buf, 0, buf_sz);

            drain_state_input = (GET_DISK_STATE_INPUT *) buf;
            drain_state_input->ulNumDisks = num_disks;
            guid = buf + sizeof(GET_DISK_STATE_INPUT) - sizeof(VOLUME_GUID);
            for (i = 1; i <= num_disks; i++) {
                strcpy(guid->volume_guid, src_guid_list);
                printf("GUID - %d = %s\n", i, guid->volume_guid);
                guid++;
                src_guid_list += GUID_SIZE_IN_CHARS;
            }

            ret = inm_ioctl(app_confp->ctrl_dev_fd, IOCTL_INMAGE_GET_DRAIN_STATE, buf);
            if (ret < 0) {
                int err = errno;
                printf("err = %d\n", err);
                perror("IOCTL_INMAGE_GET_DRAIN_STATE ioctl failed\n");
            }

            drain_state_output = buf + sizeof(GET_DISK_STATE_INPUT) +
                ((num_disks - 1) * sizeof(VOLUME_GUID));

            printf("The number of output disks are %d\n", drain_state_output->ulNumDisks);
            if (drain_state_output->ulNumDisks != num_disks) {
                printf("The number of output disks are not matching, input = %d, output = %d\n",
                            num_disks, drain_state_output->ulNumDisks);
                ret = -1;
                break;
            }
            for(i = 0; i < num_disks; i++) {
                if (drain_state_output->diskState[i].ulFlags & DISK_STATE_FILTERED) {
                    printf("Draining is not blocked for %s\n",
                        drain_state_output->diskState[i].DeviceId.volume_guid);
                }
                else if (drain_state_output->diskState[i].ulFlags & DISK_STATE_DRAIN_BLOCKED) {
                    printf("Draining is blocked for %s\n",
                        drain_state_output->diskState[i].DeviceId.volume_guid);
                }
            }

        }
        break;

	case STACK_OP:
        memset(&dev_info, 0, sizeof(dev_info));
		if (inm_ioctl(app_confp->ctrl_dev_fd,
                        IOCTL_INMAGE_GET_DRIVER_VERSION,
                        &version) == -1) {
                        perror("IOCTL_INMAGE_GET_DRIVER_VERSION ioctl failed\n");
			ret = -1;
			break;
		}
		ret = 0;
               if(INM_DRIVER_VERSION(version.ulDrMajorVersion, version.ulDrMinorVersion,
                        version.ulDrMinorVersion2, version.ulDrMinorVersion3)
                        < INM_DRIVER_VERSION_LATEST) {
                       if (strcpy_s(&guid.volume_guid[0], GUID_SIZE_IN_CHARS, app_confp->src_volp)) {
				printf("strcpy_s failed");
				ret = -1;
				goto out;
		       }
                       if (inm_ioctl(app_confp->ctrl_dev_fd,
                               IOCTL_INMAGE_VOLUME_STACKING,
                               &guid) < 0) {
                               perror("IOCTL_INMAGE_VOLUME_STACKING ioctl failed\n");
                               ret = -1;
                       }
               }else{
                       if (strcpy_s(&dev_info.d_guid[0], INM_GUID_LEN_MAX, app_confp->src_volp)) {
				printf("strcpy_s failed");
				ret = -1;
				goto out;
		       }
                       if(app_confp->mnt_pt)
                               if (strcpy_s(&dev_info.d_mnt_pt[0], INM_PATH_MAX, app_confp->mnt_pt)){
					printf("strcpy_s failed");
					ret = -1;
					goto out;
			       }
                       else
                               dev_info.d_mnt_pt[0] = '\0';
                        dev_info.d_type = FILTER_DEV_HOST_VOLUME;
                        if(dev_size(&dev_info)){
                               ret = -1;
                               break;
                       }
                        dev_info.d_flags = 0;
       
                    if (strcpy_s(&dev_info.d_pname[0], GUID_SIZE_IN_CHARS, 
                        app_confp->pname ? 
                        app_confp->pname : app_confp->src_volp)) {
                        printf("strcpy_s failed");
                        ret = -1;
                        goto out;
                    }

                    if (!app_confp->pname)
                        convert_path_to_str(dev_info.d_pname);

                        if (inm_ioctl(app_confp->ctrl_dev_fd,
                                  IOCTL_INMAGE_VOLUME_STACKING,
                                  &dev_info) == -1) {
                                perror("IOCTL_INMAGE_VOLUME_STACKING ioctl failed\n");
                                ret = -1;
                        }
                }
		break;
	case START_FLT_OP:         
#if 0
		dev_pathp = argv[start_idx];
		if (strcpy_s(&guid.volume_guid[0], GUID_SIZE_IN_CHARS, dev_pathp)) {
			printf("strcpy_s failed");
			ret = -1;
			goto out;
		}
#endif
		
		if (!app_confp->src_volp) {
			printf("source volume is missing\n");
			ret = -1;
			break;
		}

		if (strcpy_s(&guid.volume_guid[0], GUID_SIZE_IN_CHARS, app_confp->src_volp)) {
			printf("strcpy_s failed");
			ret = -1;
			goto out;
		}

		if (inm_ioctl(app_confp->ctrl_dev_fd,
                        IOCTL_INMAGE_GET_DRIVER_VERSION,
                        &version) == -1) {
                        perror("IOCTL_INMAGE_GET_DRIVER_VERSION ioctl failed\n");
			ret = -1;
			break;
		}
		ret = 0;
               if(INM_DRIVER_VERSION(version.ulDrMajorVersion, version.ulDrMinorVersion,
                        version.ulDrMinorVersion2, version.ulDrMinorVersion3)
                        < INM_DRIVER_VERSION_LATEST) {

			if (strcpy_s(&guid.volume_guid[0], GUID_SIZE_IN_CHARS, app_confp->src_volp)) {
				printf("strcpy_s failed");
				ret = -1;
				goto out;
			}

                       if (inm_ioctl(app_confp->ctrl_dev_fd,
                               IOCTL_INMAGE_START_FILTERING_DEVICE,
                               &guid) < 0) {
                               perror("IOCTL_INMAGE_START_FILTERING_DEVICE ioctl failed\n");
                               ret = -1;
                       }
               }else{
			if (strcpy_s(&dev_info.d_guid[0], GUID_SIZE_IN_CHARS, app_confp->src_volp)) {
				printf("strcpy_s failed");
				ret = -1;
				goto out;
			}

            if (strcpy_s(&dev_info.d_pname[0], GUID_SIZE_IN_CHARS, 
                    app_confp->pname ? 
                    app_confp->pname : app_confp->src_volp)) {
                    printf("strcpy_s failed");
                    ret = -1;
                    goto out;
            }

            if (!app_confp->pname)
                convert_path_to_str(dev_info.d_pname);

                       if(app_confp->mnt_pt)
                               if (strcpy_s(&dev_info.d_mnt_pt[0], INM_PATH_MAX, app_confp->mnt_pt)) {
					printf("strcpy_s failed");
					ret = -1;
					goto out;
			       }
                       else
                               dev_info.d_mnt_pt[0] = '\0';
                        dev_info.d_type = FILTER_DEV_HOST_VOLUME;
                        if(dev_size(&dev_info)){
                               ret = -1;
                               break;
                       }
                        dev_info.d_flags = app_confp->flags;
 
                        if (inm_ioctl(app_confp->ctrl_dev_fd,
                                  IOCTL_INMAGE_START_FILTERING_DEVICE_V2,
                                  &dev_info) == -1) {
                                perror("IOCTL_INMAGE_START_FILTERING_DEVICE_V2 ioctl failed\n");
                                ret = -1;
                        }
                }

		break;

	case UNSTACK_OP:            
               if (!app_confp->src_volp) {
                       printf("source volume is missing\n");
                       ret = -1;
                       break;
               }

               if (inm_ioctl(app_confp->ctrl_dev_fd,
                       IOCTL_INMAGE_GET_DRIVER_VERSION,
                       &version) == -1) {
                       perror("IOCTL_INMAGE_GET_DRIVER_VERSION ioctl failed\n");
                       ret = -1;
                       break;
               }

               ret = 0;

               if(INM_DRIVER_VERSION(version.ulDrMajorVersion, version.ulDrMinorVersion,
                        version.ulDrMinorVersion2, version.ulDrMinorVersion3)
                        >= INM_DRIVER_VERSION_LATEST) {
			if (strcpy_s(&guid.volume_guid[0], GUID_SIZE_IN_CHARS, app_confp->src_volp)) {
				printf("strcpy_s failed");
				ret = -1;
				goto out;
			}
                       if (inm_ioctl(app_confp->ctrl_dev_fd,
                               IOCTL_INMAGE_VOLUME_UNSTACKING,
                               &guid) < 0) {
                               perror("IOCTL_INMAGE_VOLUME_UNSTACKING ioctl failed\n");
                               ret = -1;
                       }
               }else{
			if (strcpy_s(&guid.volume_guid[0], GUID_SIZE_IN_CHARS, app_confp->src_volp)) {
				printf("strcpy_s failed");
				ret = -1;
				goto out;
			}
                       if (inm_ioctl(app_confp->ctrl_dev_fd,
                                 IOCTL_INMAGE_STOP_FILTERING_DEVICE,
                                 &guid) == -1) {
                               perror("IOCTL_INMAGE_STOP_FILTERING_DEVICE ioctl failed\n");
                               return -1;
                       }
                       ret = -1;
               }
               break;

	case STOP_FLT_OP:           
#if 0
		dev_pathp = argv[start_idx];
		strcpy_s(&guid.volume_guid[0], dev_pathp);
		if (strcpy_s(&guid.volume_guid[0], GUID_SIZE_IN_CHARS, dev_pathp)) {
			printf("strcpy_s failed");
			ret = -1;
			goto out;
		}
#endif
		if (!app_confp->src_volp) {
			printf("source volume is missing\n");
			ret = -1;
			break;
		}
		if (strcpy_s(&guid.volume_guid[0], GUID_SIZE_IN_CHARS, app_confp->src_volp)) {
			printf("strcpy_s failed");
			ret = -1;
			goto out;
		}
		if (inm_ioctl(app_confp->ctrl_dev_fd,
			  IOCTL_INMAGE_STOP_FILTERING_DEVICE,
			  &guid) == -1) {
			perror("IOCTL_INMAGE_STOP_FILTERING_DEVICE ioctl failed\n");
			return -1;
		}
		
		ret = 0;
		break;

	case STOP_FLT_ALL_OP:
		printf("Stop filtering all operation started\n");
		ret = get_protected_volume_list(&vol_list);
		if (ret || !vol_list.bufp) {
			printf("Failed to get the protected list of disks\n");
			goto clean_persistent_store;
		}

		if (vol_list.bufp[0] == '\0')
			printf("No disk protected\n");
		else
			printf("Protected disks are as follows:\n%s",(char *)vol_list.bufp);

		do {
			char *device = (char *)vol_list.bufp;

			while (device) {
				char * nextDev = strchr(device, '\n');
				if (nextDev)
					*nextDev = '\0';

				if (device[0] == '\0' )
					goto next_device;

				printf("Issuing the stop filtering for %s\n", device);
				if (strcpy_s(&guid.volume_guid[0], GUID_SIZE_IN_CHARS, device)) {
					printf("strcpy_s failed\n");
					goto next_device;
				}

				if (inm_ioctl(app_confp->ctrl_dev_fd,
						IOCTL_INMAGE_STOP_FILTERING_DEVICE,
						&guid) == -1) {
					perror("IOCTL_INMAGE_STOP_FILTERING_DEVICE ioctl failed\n");
					return -1;
				}

next_device:
				if (nextDev)
					*nextDev = '\n';

				device = nextDev ? (nextDev + 1) : NULL;
			}
		} while(0);

		free(vol_list.bufp);
clean_persistent_store:
		if (remove_persistent_store()) {
			printf("Persistent store removal failed\n");
			return -1;
		}

		break;

	case GET_TIME_OP:          

		if (inm_ioctl(app_confp->ctrl_dev_fd,
			  IOCTL_INMAGE_GET_NANOSECOND_TIME,
			  &time) == -1) {
			perror("IOCTL_INMAGE_STOP_FILTERING_DEVICE ioctl failed\n");
			return -1;
		} else {
			ret = 0;
		}
		printf("time = %lld\n", time);
		break;

	case REPLICATE_OP:          
//		ret = inm_replicate();
	if (app_confp->resync) {
		snprintf(cmd_line, 1023, "dd if=%s of=%s", app_confp->src_volp, app_confp->tgt_volp);
		printf("syncing volumes src volume = %s, tgt volume = %s\n", app_confp->src_volp, app_confp->tgt_volp);
		system(cmd_line);
		printf("resync completed\n");
	
	}
		if (!app_confp->src_volp) {
			printf("source volume is missing\n");
			ret = -1;
			break;
		}

		if (inm_ioctl(app_confp->ctrl_dev_fd,
                        IOCTL_INMAGE_GET_DRIVER_VERSION,
                        &version) == -1) {
                        perror("IOCTL_INMAGE_GET_DRIVER_VERSION ioctl failed\n");
			ret = -1;
			break;
		}

               if(INM_DRIVER_VERSION(version.ulDrMajorVersion, version.ulDrMinorVersion,
                       version.ulDrMinorVersion2, version.ulDrMinorVersion3)
                       < INM_DRIVER_VERSION_LATEST) {
			if (strcpy_s(&guid.volume_guid[0], GUID_SIZE_IN_CHARS, app_confp->src_volp)) {
				printf("strcpy_s failed");
				ret = -1;
				goto out;
			}
               
                       if (inm_ioctl(app_confp->ctrl_dev_fd,
                                 IOCTL_INMAGE_START_FILTERING_DEVICE,
                                 &guid) == -1) {
                               perror("IOCTL_INMAGE_START_FILTERING_DEVICE ioctl failed\n");
                               ret = -1;
                               break;
                       }
               } else {
                        if(app_confp->mnt_pt) {
				if (strcpy_s(&dev_info.d_mnt_pt[0], INM_PATH_MAX, app_confp->mnt_pt)) {
					printf("strcpy_s failed");
					ret = -1;
					goto out;
				}
                        } else
                               dev_info.d_mnt_pt[0] = '\0';
			if(app_confp->dev_type && (!strcmp(app_confp->dev_type, "fabric") || !strcmp(app_confp->dev_type, "FABRIC"))){
				dev_info.d_type = FILTER_DEV_FABRIC_LUN;
				if (strcpy_s(&dev_info.d_guid[0], INM_GUID_LEN_MAX, app_confp->lun_name)) {
					printf("strcpy_s failed");
					ret = -1;
					goto out;
				}
			}
			else {
                        	dev_info.d_type = FILTER_DEV_HOST_VOLUME;
				if (strcpy_s(&dev_info.d_guid[0], GUID_SIZE_IN_CHARS, app_confp->src_volp)) {
					printf("strcpy_s failed");
					ret = -1;
					goto out;
				}
				if(dev_size(&dev_info)){
				       ret = -1;
				       break;
				}
			}

                        dev_info.d_flags = 0;
 
                if (strcpy_s(&dev_info.d_pname[0], GUID_SIZE_IN_CHARS,
                             app_confp->pname ?
                             app_confp->pname : app_confp->src_volp)) {
                    printf("strcpy_s failed");
                    ret = -1;
                    goto out;
                }

                if (!app_confp->pname)
                    convert_path_to_str(dev_info.d_pname);
 
                        if (inm_ioctl(app_confp->ctrl_dev_fd,
                                  IOCTL_INMAGE_START_FILTERING_DEVICE_V2,
                                  &dev_info) == -1) {
                                perror("IOCTL_INMAGE_START_FILTERING_DEVICE_V2 ioctl failed\n");
                                ret = -1;
                        }
               }
		if(dev_info.d_type == FILTER_DEV_HOST_VOLUME){
			app_confp->lun_name = app_confp->src_volp;
		}

		ret = inm_replicate(app_confp->src_volp, app_confp->tgt_volp, app_confp->lun_name);
		break;

	case GET_DB_OP:             
/*
		dev_pathp = argv[start_idx];
		if (strcpy_s(&guid.volume_guid[0], GUID_SIZE_IN_CHARS, dev_pathp)) {
				printf("strcpy_s failed");
				ret = -1;
				goto out;
		}
		if (inm_ioctl(app_confp->ctrl_dev_fd,
			  IOCTL_INMAGE_GET_DIRTY_BLOCKS_TRANS,
			  &guid) == -1) {
			perror("IOCTL_INMAGE_STOP_FILTERING_DEVICE ioctl failed\n");
			ret = -1;
		}
*/
		if (!app_confp->src_volp) {
			printf("source volume is missing\n");
			ret = -1;
			break;
		}

               if (inm_ioctl(app_confp->ctrl_dev_fd,
                        IOCTL_INMAGE_GET_DRIVER_VERSION,
                        &version) == -1) {
                        perror("IOCTL_INMAGE_GET_DRIVER_VERSION ioctl failed\n");
                        ret = -1;
                        break;
               }

               if(INM_DRIVER_VERSION(version.ulDrMajorVersion, version.ulDrMinorVersion,
                        version.ulDrMinorVersion2, version.ulDrMinorVersion3)
                        < INM_DRIVER_VERSION_LATEST) {
			if (strcpy_s(&guid.volume_guid[0], GUID_SIZE_IN_CHARS, app_confp->src_volp)) {
				printf("strcpy_s failed");
				ret = -1;
				goto out;
			}
               
                       if (inm_ioctl(app_confp->ctrl_dev_fd,
                                 IOCTL_INMAGE_START_FILTERING_DEVICE,
                                 &guid) == -1) {
                               perror("IOCTL_INMAGE_START_FILTERING_DEVICE ioctl failed\n");
                               ret = -1;
                               break;
                       }
               } else {
			if (strcpy_s(&dev_info.d_guid[0], GUID_SIZE_IN_CHARS, app_confp->src_volp)) {
				printf("strcpy_s failed");
				ret = -1;
				goto out;
			}
                       if(app_confp->mnt_pt)
                               if (strcpy_s(&dev_info.d_mnt_pt[0], INM_PATH_MAX, app_confp->mnt_pt)) {
					printf("strcpy_s failed");
					ret = -1;
					goto out;
			       }
                       else
                               dev_info.d_mnt_pt[0] = '\0';
                       dev_info.d_type = FILTER_DEV_HOST_VOLUME;
                       if(dev_size(&dev_info)){
                               ret = -1;
                               break;
                       }
                       dev_info.d_flags = 0;

                if (strcpy_s(&dev_info.d_pname[0], GUID_SIZE_IN_CHARS,
                             app_confp->pname ?
                             app_confp->pname : app_confp->src_volp)) {
                    printf("strcpy_s failed");
                    ret = -1;
                    goto out;
                }

                if (!app_confp->pname)
                    convert_path_to_str(dev_info.d_pname);

                       if (inm_ioctl(app_confp->ctrl_dev_fd,
                                 IOCTL_INMAGE_START_FILTERING_DEVICE_V2,
                                 &dev_info) == -1) {
                               perror("IOCTL_INMAGE_START_FILTERING_DEVICE_V2 ioctl failed\n");
                               ret = -1;
                               break;
                       }
	        }
		ret = inm_get_db_trans(app_confp->src_volp, app_confp->tgt_volp);
		break;

	case GET_DB_COMMIT_OP:             
		if (!app_confp->src_volp) {
			printf("source volume is missing\n");
			ret = -1;
			break;
		}

		if (inm_ioctl(app_confp->ctrl_dev_fd,
                        IOCTL_INMAGE_GET_DRIVER_VERSION,
                        &version) == -1) {
                        perror("IOCTL_INMAGE_GET_DRIVER_VERSION ioctl failed\n");
			ret = -1;
			break;
		}

               if(INM_DRIVER_VERSION(version.ulDrMajorVersion, version.ulDrMinorVersion,
                        version.ulDrMinorVersion2, version.ulDrMinorVersion3)
                        < INM_DRIVER_VERSION_LATEST) {
			if (strcpy_s(&guid.volume_guid[0], GUID_SIZE_IN_CHARS, app_confp->src_volp)) {
				printf("strcpy_s failed");
				ret = -1;
				goto out;
			}
               
                       if (inm_ioctl(app_confp->ctrl_dev_fd,
                                 IOCTL_INMAGE_START_FILTERING_DEVICE,
                                 &guid) == -1) {
                               perror("IOCTL_INMAGE_START_FILTERING_DEVICE ioctl failed\n");
                               ret = -1;
                               break;
                       }
               } else {
			if (strcpy_s(&dev_info.d_guid[0], GUID_SIZE_IN_CHARS, app_confp->src_volp)) {
				printf("strcpy_s failed");
				ret = -1;
				goto out;
			}
                       if(app_confp->mnt_pt) {
                               if (strcpy_s(&dev_info.d_mnt_pt[0], INM_PATH_MAX, app_confp->mnt_pt)) {
					printf("strcpy_s failed");
					ret = -1;
					goto out;
			       }
                       } else
                               dev_info.d_mnt_pt[0] = '\0';
                       dev_info.d_type = FILTER_DEV_HOST_VOLUME;
                       if(dev_size(&dev_info)){
                               ret = -1;
                               break;
                       }
                       dev_info.d_flags = 0;

                if (strcpy_s(&dev_info.d_pname[0], GUID_SIZE_IN_CHARS,
                             app_confp->pname ?
                             app_confp->pname : app_confp->src_volp)) {
                    printf("strcpy_s failed");
                    ret = -1;
                    goto out;
                }

                if (!app_confp->pname)
                    convert_path_to_str(dev_info.d_pname);

                       if (inm_ioctl(app_confp->ctrl_dev_fd,
                                 IOCTL_INMAGE_START_FILTERING_DEVICE_V2,
                                 &dev_info) == -1) {
                               perror("IOCTL_INMAGE_START_FILTERING_DEVICE_V2 ioctl failed\n");
                               ret = -1;
                               break;
                       }
                }

		ret = inm_get_db_commit(app_confp->src_volp, app_confp->tgt_volp);
		break;

	case CLEAR_DIFFS_OP:
		dev_pathp = argv[start_idx];
		if (strcpy_s(&guid.volume_guid[0], GUID_SIZE_IN_CHARS, dev_pathp)) {
			printf("strcpy_s failed");
			ret = -1;
			goto out;
		}
		if (inm_ioctl(app_confp->ctrl_dev_fd,
			  IOCTL_INMAGE_CLEAR_DIFFERENTIALS,
			  &guid) == -1) {
			perror("IOCTL_INMAGE_STOP_FILTERING_DEVICE ioctl failed\n");
			return -1;
		} else {
			ret = 0;
		}
		break;

	case GET_DB_THRES_OP:       
		dev_pathp = argv[start_idx];
		if (strcpy_s(&guid.volume_guid[0], GUID_SIZE_IN_CHARS, dev_pathp)) {
			printf("strcpy_s failed");
			ret = -1;
			goto out;
		}
		if (inm_ioctl(app_confp->ctrl_dev_fd,
			  IOCTL_INMAGE_STOP_FILTERING_DEVICE,
			  &guid) == -1) {
			perror("IOCTL_INMAGE_STOP_FILTERING_DEVICE ioctl failed\n");
			ret = -1;
		} else {
			ret = 0;
		}
		break;

	case PRINT_IOCTL_CODES_OP:
		break;

	case PRINT_TAG_CODES_OP:    
		printf("SVD_TAG_HEADER1                        = %d\n", SVD_TAG_HEADER1);
		printf("SVD_TAG_HEADER_V2                      = %d\n", SVD_TAG_HEADER_V2);
		printf("SVD_TAG_DIRTY_BLOCKS                   = %d\n", SVD_TAG_DIRTY_BLOCKS);
		printf("SVD_TAG_DIRTY_BLOCK_DATA               = %d\n", SVD_TAG_DIRTY_BLOCK_DATA);
		printf("SVD_TAG_DIRTY_BLOCK_DATA_V2            = %d\n", SVD_TAG_DIRTY_BLOCK_DATA_V2);
		printf("SVD_TAG_SENTINEL_HEADER                = %d\n", SVD_TAG_SENTINEL_HEADER);
		printf("SVD_TAG_SENTINEL_DIRT                  = %d\n", SVD_TAG_SENTINEL_DIRT);
		printf("SVD_TAG_SYNC_HASH_COMPARE_DATA         = %d\n", SVD_TAG_SYNC_HASH_COMPARE_DATA);
		printf("SVD_TAG_SYNC_DATA                      = %d\n", SVD_TAG_SYNC_DATA);
		printf("SVD_TAG_SYNC_DATA_NEEDED_INFO          = %d\n", SVD_TAG_SYNC_DATA_NEEDED_INFO);
		printf("SVD_TAG_TIME_STAMP_OF_FIRST_CHANGE     = %d\n", SVD_TAG_TIME_STAMP_OF_FIRST_CHANGE);
		printf("SVD_TAG_TIME_STAMP_OF_LAST_CHANGE      = %d\n", SVD_TAG_TIME_STAMP_OF_LAST_CHANGE);
		printf("SVD_TAG_TIME_STAMP_OF_FIRST_CHANGE_V2  = %d\n", SVD_TAG_TIME_STAMP_OF_FIRST_CHANGE_V2);
		printf("SVD_TAG_TIME_STAMP_OF_LAST_CHANGE_V2   = %d\n", SVD_TAG_TIME_STAMP_OF_LAST_CHANGE_V2);
		printf("SVD_TAG_LENGTH_OF_DRTD_CHANGES         = %d\n", SVD_TAG_LENGTH_OF_DRTD_CHANGES);
		printf("SVD_TAG_USER                           = %d\n", SVD_TAG_USER);
			ret = 0;
			break;

		case GET_DRIVER_VER_OP:     
			if (inm_ioctl(app_confp->ctrl_dev_fd,
                                IOCTL_INMAGE_GET_DRIVER_VERSION,
                                &version) == -1) {
                                perror("IOCTL_INMAGE_GET_DRIVER_VERSION ioctl failed\n");
				ret = -1;
				break;
			}
                        printf("Product Version = (%d, %d, %d, %d)\n", version.ulPrMajorVersion, version.ulPrMinorVersion,
                                                               version.ulPrMinorVersion2, version.ulPrBuildNumber);
                        printf("Driver Version = (%d, %d, %d, %d)\n", version.ulDrMajorVersion, version.ulDrMinorVersion,
                                                               version.ulDrMinorVersion2, version.ulDrMinorVersion3);
			break;

		case RESYNC_START_OP:       
			dev_pathp = argv[start_idx];
			if (strcpy_s(&guid.volume_guid[0], GUID_SIZE_IN_CHARS, dev_pathp)) {
				printf("strcpy_s failed");
				ret = -1;
				goto out;
			}
			if (inm_ioctl(app_confp->ctrl_dev_fd,
				  IOCTL_INMAGE_STOP_FILTERING_DEVICE,
				  &guid) == -1) {
				perror("IOCTL_INMAGE_STOP_FILTERING_DEVICE ioctl failed\n");
				ret = -1;
			}
			break;

		case RESYNC_END_OP:         
			dev_pathp = argv[start_idx];
			if (strcpy_s(&guid.volume_guid[0], GUID_SIZE_IN_CHARS, dev_pathp)) {
				printf("strcpy_s failed");
				ret = -1;
				goto out;
			}
			if (inm_ioctl(app_confp->ctrl_dev_fd,
				  IOCTL_INMAGE_STOP_FILTERING_DEVICE,
				  &guid) == -1) {
				perror("IOCTL_INMAGE_STOP_FILTERING_DEVICE ioctl failed\n");
				ret = -1;
			}
			break;

		case SYS_SHUTDOWN_OP:       
		printf("SYS_SHUTDOWN_OP ioctl executing now\n");
			if (inm_ioctl(app_confp->ctrl_dev_fd,
				  IOCTL_INMAGE_SYS_SHUTDOWN,
				  &guid) == -1) {
				perror("SYS_SHUTDOWN_OP ioctl failed\n");
				ret = -1;
			}
			break;

		case CREATE_BARRIER_OP:
		{
			flt_barrier_create_t bcreate;
			int i;
			if (app_confp->timeout <= 0) {
				app_confp->timeout = 300;
			}
			bcreate.fbc_timeout_ms = app_confp->timeout;
			memcpy_s(bcreate.fbc_guid, GUID_LEN, "abcdefghijklmnopqrstuvwxyz0123456789", 36);
			printf("Tag GUID = ");
			for (i = 0; i < GUID_LEN; i++)
				printf("%c", bcreate.fbc_guid[i]);
			printf("\n");

			if (inm_ioctl(app_confp->ctrl_dev_fd,
                                  IOCTL_INMAGE_CREATE_BARRIER_ALL,
                                  &bcreate) == -1) {
				perror("IOCTL_INMAGE_CREATE_BARRIER_ALL ioctl failed\n");
				ret = -1;
				break;
			}

			break;
		}

		case REMOVE_BARRIER_OP:
		{
			flt_barrier_remove_t bremove;
			int i;
			memcpy_s(bremove.fbr_guid, GUID_LEN, "abcdefghijklmnopqrstuvwxyz0123456789", 36);
			printf("Tag GUID = ");
			for (i = 0; i < GUID_LEN; i++)
				printf("%c", bremove.fbr_guid[i]);
			printf("\n");

			if (inm_ioctl(app_confp->ctrl_dev_fd,
                                  IOCTL_INMAGE_REMOVE_BARRIER_ALL,
                                  &bremove) == -1) {
				perror("IOCTL_INMAGE_REMOVE_BARRIER_ALL ioctl failed\n");
				ret = -1;
				break;
			}

			break;
		}

		case ISSUE_TAG_OP:
		{
			tag_info_t_v2 tag_vol;
			tag_names_t tag_names;
			int i;

#if 0
			if(!app_confp->src_volp || !app_confp->tag_name){
				printf("One of source volume or tag name is not provided\n");
				ret = -1;
				break;
			}
			printf("Src vol = %s, tag name = %s, mount point = %s\n", app_confp->src_volp, app_confp->tag_name, app_confp->mnt_pt);
#endif

			tag_vol.flags = TAG_ALL_PROTECTED_VOLUME_IOBARRIER;
			tag_vol.nr_vols = 0;
			tag_vol.nr_tags = 1;
			tag_vol.timeout = 60;
			memcpy_s(tag_vol.tag_guid, GUID_LEN, "abcdefghijklmnopqrstuvwxyz0123456789", 36);
                        printf("Tag GUID = ");
                	for (i = 0; i < GUID_LEN; i++)
                    	    printf("%c", tag_vol.tag_guid[i]);
                	printf("\n");
			tag_vol.vol_info = NULL;
			tag_names.tag_len = 100;
			strcpy(tag_names.tag_name, "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz0123456789");
			tag_vol.tag_names = &tag_names;

			if (inm_ioctl(app_confp->ctrl_dev_fd,
                                  IOCTL_INMAGE_IOBARRIER_TAG_VOLUME,
                                  &tag_vol) == -1) {
				perror("IOCTL_INMAGE_IOBARRIER_TAG_VOLUME ioctl failed\n");
				ret = -1;
				break;
			}

			break;
		}
#ifdef INM_AIX
			flags |= TAG_FS_FROZEN_IN_USERSPACE;
			sprintf(mount_point, "/tmp/mount_point%d", getpid());
			printf("The temp file for mount point is %s\n", mount_point);
			sprintf(cmd_buf, "mount | grep '%s' | grep -v grep | awk -F' ' '{print $2}' > %s", app_confp->src_volp, mount_point);
			system(cmd_buf);
			fd_mnt = open(mount_point, O_RDONLY, 0666);
			if(fd_mnt < 0){
				printf("Error in opening the file %s\n", mount_point);
				unlink(mount_point);
				break;
			}

			read_len = read(fd_mnt, cmd_buf, PATH_MAX);
			if(read_len < 0){
				printf("Error in reading the file %s\n", mount_point);
				close(fd_mnt);
				unlink(mount_point);
				break;
			}

			if(cmd_buf[read_len - 1] == '\n')
				read_len--;

			cmd_buf[read_len] = '\0';
			printf("The mount point is %s A, read_len = %d\n", cmd_buf, read_len);
			close(fd_mnt);
			unlink(mount_point);

			if(read_len){
				stat(cmd_buf, &mnt_stat);
				printf("mode = %x, st_type = %u, VDIR = %u, st_vfs = %u\n", mnt_stat.st_mode, mnt_stat.st_type, VDIR, mnt_stat.st_vfs);
				if(fscntl(mnt_stat.st_vfs, FSCNTL_FREEZE, 120, 0)){
					printf("failed to freeze the filesystem mounted at %s\n", cmd_buf);
					break;
				}

				printf("frozen successfully\n");
			}
#endif

			total_buffer_size = sizeof(flags) + sizeof(num_vols) +
					    (num_vols * sizeof(unsigned short)) +
					    sizeof(num_tags) +
					    (num_tags * sizeof(unsigned short)) +
					    strlen(app_confp->src_volp) +
					    strlen(app_confp->tag_name);

			buffer = (char *)malloc(total_buffer_size);
			if(!buffer){
				printf("Unable to allocate the buffer\n");
				ret = -1;
				goto unfreeze;
			}

			if (memcpy_s(buffer + current_buf_size, sizeof(flags), &flags, sizeof(flags))) {
				printf("memcpy_s failed to copy");
				ret = -1;
				goto unfreeze;
			}
			current_buf_size += sizeof(flags);

			if (memcpy_s(buffer + current_buf_size , sizeof(num_vols), &num_vols, sizeof(num_vols))) {
				printf("memcpy_s failed to copy");
				ret = -1;
				goto unfreeze;
			}
                        current_buf_size += sizeof(num_vols);

			len = strlen(app_confp->src_volp);
			if (memcpy_s(buffer + current_buf_size , sizeof(len), &len, sizeof(len))) {
				printf("memcpy_s failed to copy");
				ret = -1;
				goto unfreeze;
			}
			current_buf_size += sizeof(len);
			if (memcpy_s(buffer + current_buf_size, len, app_confp->src_volp, len)) {
				printf("memcpy_s failed to copy");
				ret = -1;
				goto unfreeze;
			}
                        current_buf_size += len;

			if (memcpy_s(buffer + current_buf_size, sizeof(unsigned short), &num_tags, sizeof(unsigned short))) {
				printf("memcpy_s failed to copy");
				ret = -1;
				goto unfreeze;
			}
                        current_buf_size += sizeof(num_tags);

			len = strlen(app_confp->tag_name);
			if (memcpy_s(buffer + current_buf_size , sizeof(len), &len, sizeof(len))) {
				printf("memcpy_s failed to copy");
				ret = -1;
				goto unfreeze;
			}
			current_buf_size += sizeof(len);
			if (memcpy_s(buffer + current_buf_size, len, app_confp->tag_name, len)) {
				printf("memcpy_s failed to copy");
				ret = -1;
				goto unfreeze;
			}
			current_buf_size += len;

			if (inm_ioctl(app_confp->ctrl_dev_fd,
				  IOCTL_INMAGE_TAG_VOLUME,
				  buffer) == -1) {
				perror("IOCTL_INMAGE_TAG_VOLUME ioctl failed\n");
				ret = -1;
			} else {
				printf("Tag issued successfully\n");
				ret = 0;
			}

			if(buffer)
				free(buffer);
unfreeze:
#ifdef INM_AIX
			if(read_len){
				if(fscntl(mnt_stat.st_vfs, FSCNTL_THAW, 0, 0)){
					printf("failed to thaw the filesystem mounted at %s\n", cmd_buf);
					ret = -1;
					break;
				}
			}
#endif
			break;

		case SERVICE_START_OP:
			stop_notify.ulFlags = 0;
			ret = 0;

			if(inm_ioctl(app_confp->ctrl_dev_fd,
				 IOCTL_INMAGE_SERVICE_SHUTDOWN_NOTIFY,
				 &stop_notify) < 0) {
				printf("IOCTL_INMAGE_SERVICE_SHUTDOWN_NOTIFY ioctl failed\n");
				ret = -1;
			}
			/* sleep continuously in the back ground */
			while (1) {
				sleep(100);
			}
			break;

		case START_NOTIFY_OP:
			{
			ret = 0;
			PROCESS_START_NOTIFY_INPUT start_notify;

			if(inm_ioctl(app_confp->ctrl_dev_fd,
				 IOCTL_INMAGE_PROCESS_START_NOTIFY,
				 &start_notify) < 0) {
				printf("IOCTL_INMAGE_PROCESS_START_NOTIFY ioctl \
					failed\n");
				ret = -1;
			}
			/* sleep continuously in the back ground */
			while (1) {
				sleep(100);
			}
			}
			break;

	case LUN_CREATE_OP:
		if(!app_confp->lun_name || !app_confp->lun_size) {
			printf("Failed: Lun Name and Lun Size input required\n");
			ret = -1;
		}

		if (strcpy_s(add_lun.uuid, GUID_SIZE_IN_CHARS+1, app_confp->lun_name)) {
			printf("strcpy_s failed");
			ret = -1;
			goto out;
		}
		add_lun.lunSize = app_confp->lun_size;
		add_lun.blockSize = 512;
		add_lun.lunStartOff = app_confp->lun_startoff;
		add_lun.lunType = FILTER_DEV_FABRIC_LUN;

		if(inm_ioctl(app_confp->ctrl_dev_fd,
				IOCTL_INMAGE_AT_LUN_CREATE,
				 &add_lun) < 0) {
			printf("IOCTL_INMAGE_AT_LUN_CREATE ioctl failed\n");
			ret = -1;
		} else {
			printf("Fabric lun %s created successfully\n", app_confp->lun_name);
			ret = 0;
		}
		break;

	case LUN_DELETE_OP:
		if(!app_confp->lun_name) {
			printf("Failed: Lun Name input required\n");
			ret = -1;
		}
		
		if (strcpy_s(del_lun.uuid, GUID_SIZE_IN_CHARS+1, app_confp->lun_name)) {
			printf("strcpy_s failed");
			ret = -1;
			goto out;
		}
		del_lun.lunType = FILTER_DEV_FABRIC_LUN;

                if(inm_ioctl(app_confp->ctrl_dev_fd,
                                IOCTL_INMAGE_AT_LUN_DELETE,
                                 &del_lun) < 0) {
                        printf("IOCTL_INMAGE_AT_LUN_DELETE ioctl failed\n");
                        ret = -1;
                } else {
                        printf("Fabric lun %s deleted successfully\n", app_confp->lun_name);
			ret = 0;
                }
	
		break;
	case UNSTACK_ALL_OP:
		if (inm_ioctl(app_confp->ctrl_dev_fd,
				IOCTL_INMAGE_UNSTACK_ALL,
				(void *)NULL) < 0) {
			printf("IOCTL_INMAGE_UNSTACK_ALL ioctl failed\n");
			ret = -1;
		} else {
			printf("Unstack all operation successfully completed\n");
			ret = 0;
		}
		break;
	case BOOTTIME_STACKING_OP:
		if (inm_ioctl(app_confp->ctrl_dev_fd,
				IOCTL_INMAGE_BOOTTIME_STACKING,
				(void *)NULL) < 0) {
			printf("IOCTL_INMAGE_BOOTIME_STACKING ioctl failed\n");
			ret = -1;
		} else {
			printf("Boottime stacking successfully completed\n");
			ret = 0;
		}
		break;
	case START_MIRROR_OP:         
	case START_LOCAL_MIRROR_OP:         
		if (!app_confp->mconf.src_guid_list) {
			printf("source volume list is missing\n");
			ret = -1;
			break;
		}
		if (!app_confp->mconf.dst_guid_list) {
			printf("Mirror volume list is missing\n");
			ret = -1;
			break;
		}
		if (inm_ioctl(app_confp->ctrl_dev_fd,
                        IOCTL_INMAGE_GET_DRIVER_VERSION,
                        &version) == -1) {
                        perror("IOCTL_INMAGE_GET_DRIVER_VERSION ioctl failed\n");
			ret = -1;
			break;
		}
        if (strncpy_s(&dev_info.d_guid[0], INM_GUID_LEN_MAX, app_confp->mconf.src_guid_list, INM_GUID_LEN_MAX - 1)) {
		printf("strncpy_s failed to copy");
		ret = 1;
		goto out;
	}
        dev_info.d_guid[INM_GUID_LEN_MAX-1]='\0';
        dev_info.d_type = FILTER_DEV_MIRROR_SETUP;
        if (dev_size(&dev_info)){
            ret = -1;
            break;
        }
        dev_info.d_flags = 0;
        app_confp->mconf.d_type = FILTER_DEV_MIRROR_SETUP;
        app_confp->mconf.d_nblks = dev_info.d_nblks;
        app_confp->mconf.d_bsize = dev_info.d_bsize;
        app_confp->mconf.d_flags = app_confp->flags;
        app_confp->mconf.d_vendor = app_confp->vendor;
        app_confp->mconf.startoff = app_confp->lun_startoff;

        if (inm_ioctl(app_confp->ctrl_dev_fd,
                      IOCTL_INMAGE_START_MIRRORING_DEVICE,
                      &app_confp->mconf) == -1) {
            perror("IOCTL_INMAGE_START_MIRRORING_DEVICE ioctl failed\n");
            ret = -1;
        }

		break;
	case STOP_MIRROR_OP:         
	case STOP_LOCAL_MIRROR_OP:         
		if (!app_confp->mconf.src_guid_list) {
			printf("source volume list is missing\n");
			ret = -1;
			break;
		}
		if (!app_confp->mconf.dst_guid_list) {
			printf("Mirror volume list is missing\n");
			ret = -1;
			break;
		}
		if (inm_ioctl(app_confp->ctrl_dev_fd,
                      IOCTL_INMAGE_GET_DRIVER_VERSION,
                      &version) == -1) {
            perror("IOCTL_INMAGE_GET_DRIVER_VERSION ioctl failed\n");
			ret = -1;
			break;
		}
        if (strncpy_s(&dev_info.d_guid[0], INM_GUID_LEN_MAX, app_confp->mconf.src_guid_list, INM_GUID_LEN_MAX - 1)) {
		printf("strncpy_s failed to copy");
		ret = 1;
		goto out;
	}
        dev_info.d_guid[INM_GUID_LEN_MAX-1]='\0';
        dev_info.d_type = FILTER_DEV_MIRROR_SETUP;
        if (dev_size(&dev_info)) {
            ret = -1;
            break;
        }
        dev_info.d_flags = 0;
        app_confp->mconf.d_type = FILTER_DEV_MIRROR_SETUP;
        app_confp->mconf.d_nblks = dev_info.d_nblks;
        app_confp->mconf.d_bsize = dev_info.d_bsize;
        app_confp->mconf.d_flags = dev_info.d_flags;

        if (inm_ioctl(app_confp->ctrl_dev_fd,
                      IOCTL_INMAGE_STOP_MIRRORING_DEVICE,
                      app_confp->mconf.src_scsi_id) == -1) {
		    perror("IOCTL_INMAGE_STOP_MIRRORING_DEVICE ioctl failed\n");
		    ret = -1;
		}

		break;

		case STACK_MIRROR_OP:
			if (!app_confp->mconf.src_guid_list) {
				printf("source volume list is missing\n");
				ret = -1;
				break;
			}
			if (!app_confp->mconf.dst_guid_list) {
				printf("Mirror volume list is missing\n");
				ret = -1;
				break;
			}
			if (inm_ioctl(app_confp->ctrl_dev_fd,
				IOCTL_INMAGE_GET_DRIVER_VERSION,
				&version) == -1) {
				perror("IOCTL_INMAGE_GET_DRIVER_VERSION ioctl failed\n");
				ret = -1;
				break;
			}
		if (strncpy_s(&dev_info.d_guid[0], INM_GUID_LEN_MAX, app_confp->mconf.src_guid_list, INM_GUID_LEN_MAX - 1)) {
			printf("strncpy_s failed to copy");
			ret = 1;
			goto out;
		}
		dev_info.d_guid[INM_GUID_LEN_MAX-1]='\0';
		dev_info.d_type = FILTER_DEV_MIRROR_SETUP;
		if (dev_size(&dev_info)){
		    ret = -1;
		    break;
		}
		dev_info.d_flags = 0;
		app_confp->mconf.d_type = FILTER_DEV_MIRROR_SETUP;
		app_confp->mconf.d_nblks = dev_info.d_nblks;
		app_confp->mconf.d_bsize = dev_info.d_bsize;
		app_confp->mconf.d_flags = 0;
#ifdef INM_AIX
#ifndef __64BIT__
		app_confp->mconf.padding = 0;
		app_confp->mconf.padding_2 = 0;
#endif
#endif
		if (inm_ioctl(app_confp->ctrl_dev_fd,
			      IOCTL_INMAGE_MIRROR_VOLUME_STACKING,
			      &app_confp->mconf) == -1) {
		    perror("IOCTL_INMAGE_MIRROR_VOLUME_STACKING ioctl failed\n");
		    ret = -1;
		}
		break;

	#ifdef INM_AIX
		case DMESG_OP:
			if (inm_ioctl(app_confp->ctrl_dev_fd, IOCTL_INMAGE_GET_DMESG, &ret)){
				perror("IOCTL_INMAGE_GET_DMESG ioctl failed\n");
				ret = -1;
			}
			break;
#endif

		case TEST_HEARTBEAT_OP:
			if (inm_ioctl(app_confp->ctrl_dev_fd,
				IOCTL_INMAGE_MIRROR_TEST_HEARTBEAT,
				&(app_confp->mconf.src_scsi_id)) == -1) {
			printf("IOCTL_INMAGE_MIRROR_TEST_HEARTBEAT ioctl failed to send test heartbeat\n");
			ret = -1;
			} else {
				printf("IOCTL_INMAGE_MIRROR_TEST_HEARTBEAT issued successfully\n");
				ret = 0;
			}
			break;


		case LATENCY_STATS_OP:
			if (strlen(app_confp->src_volp) > 0) {
				VOLUME_LATENCY_STATS vol_lat_stats;

				memset(&vol_lat_stats, 0, sizeof(vol_lat_stats));
				if (strncpy_s(vol_lat_stats.VolumeGuid.volume_guid, GUID_SIZE_IN_CHARS, app_confp->src_volp, GUID_SIZE_IN_CHARS - 1)) {
					printf("strncpy_s failed to copy");
					ret = 1;
					goto out;
				}
				if (inm_ioctl(app_confp->ctrl_dev_fd,
					IOCTL_INMAGE_GET_VOLUME_LATENCY_STATS,
					&vol_lat_stats) == -1) {
					perror("IOCTL_INMAGE_GET_VOLUME_LATENCY_STATS ioctl failed\n");
					ret = -1;
				} else {
					printf("IOCTL_INMAGE_GET_VOLUME_LATENCY_STATS issued successfully\n");
					printf("Volume Name : %s \n", vol_lat_stats.VolumeGuid.volume_guid);
					printf("S2 Data retrieval latency statistics \n\n");
					printf("Distribution : \n");
					print_distr_time(vol_lat_stats.s2dbret_bkts, vol_lat_stats.s2dbret_freq, vol_lat_stats.s2dbret_nr_avail_bkts);
					print_log_values(vol_lat_stats.s2dbret_log_buf, INM_LATENCY_LOG_CAPACITY,
						vol_lat_stats.s2dbret_log_min,
						vol_lat_stats.s2dbret_log_max);

					printf("S2 wait notify latency statistics \n\n");
					print_distr_time(vol_lat_stats.s2dbwait_notify_bkts,
						vol_lat_stats.s2dbwait_notify_freq,
						vol_lat_stats.s2dbwait_notify_nr_avail_bkts);
					
					print_log_values(vol_lat_stats.s2dbwait_notify_log_buf,
						INM_LATENCY_LOG_CAPACITY,
						vol_lat_stats.s2dbwait_notify_log_min,
						vol_lat_stats.s2dbwait_notify_log_max);

					printf("S2 commit latency statistics \n\n");
					print_distr_time(vol_lat_stats.s2dbcommit_bkts,
						vol_lat_stats.s2dbcommit_freq,
						vol_lat_stats.s2dbcommit_nr_avail_bkts);
					
					print_log_values(vol_lat_stats.s2dbcommit_log_buf,
						INM_LATENCY_LOG_CAPACITY,
						vol_lat_stats.s2dbcommit_log_min,
						vol_lat_stats.s2dbcommit_log_max);
					ret = 0;
				}
			}

			break;

		case ADDITIONAL_STATS_OP:
			if (strlen(app_confp->src_volp) > 0) {
				VOLUME_STATS_ADDITIONAL_INFO vol_add_stats;

				memset(&vol_add_stats, 0, sizeof(vol_add_stats));
				if (strncpy_s(vol_add_stats.VolumeGuid.volume_guid, GUID_SIZE_IN_CHARS, app_confp->src_volp, GUID_SIZE_IN_CHARS - 1)) {
					printf("strncpy_s failed to copy");
					ret = 1;
					goto out;
				}
				if (inm_ioctl(app_confp->ctrl_dev_fd,
					IOCTL_INMAGE_GET_ADDITIONAL_VOLUME_STATS,
					&vol_add_stats) == -1) {
					perror("IOCTL_INMAGE_GET_ADDITIONAL_VOLUME_STATS ioctl failed\n");
					ret = -1;
				} else {
					printf("IOCTL_INMAGE_GET_ADDITIONAL_VOLUME_STATS issued successfully\n");
					printf("Volume Name : %s \n", vol_add_stats.VolumeGuid.volume_guid);
					printf("Total changes  pending (including bitmap changes) = %llu\n",
						(unsigned long long)vol_add_stats.ullTotalChangesPending);
					printf("oldest time stamp = %llu \n", (unsigned long long)vol_add_stats.ullOldestChangeTimeStamp);
					printf("driver time stamp = %llu \n", (unsigned long long)vol_add_stats.ullDriverCurrentTimeStamp);
					ret = 0;
				}
			}
			break;

		case MONITORING_STATS_OP:
			if (strlen(app_confp->src_volp) > 0) {
				MONITORING_STATS monitoring_stats;

                                // Get tags stats
				memset(&monitoring_stats, 0, sizeof(monitoring_stats));
				if (strncpy_s(monitoring_stats.VolumeGuid.volume_guid, 
                                              GUID_SIZE_IN_CHARS, app_confp->src_volp,
                                              GUID_SIZE_IN_CHARS - 1)) {
					printf("strncpy_s failed to copy");
					ret = -1;
					goto out;
				}
                                monitoring_stats.ReqStat = GET_TAG_STATS;
				ret = inm_ioctl(app_confp->ctrl_dev_fd,
					IOCTL_INMAGE_GET_MONITORING_STATS,
					&monitoring_stats);
                                if (ret != 0) {
					perror("IOCTL_INMAGE_GET_MONITORING_STATS failed\n");
				} else {
					printf("IOCTL_INMAGE_GET_MONITORING_STATS issued "
                                               "successfully for TAG stats\n");
					printf("Volume Name : %s \n", monitoring_stats.VolumeGuid.volume_guid);
					printf("Tags dropped = %llu\n",
						(unsigned long long)monitoring_stats.TagStats.TagsDropped);
                                }

                                // Get churn stats
				memset(&monitoring_stats, 0, sizeof(monitoring_stats));
				if (strncpy_s(monitoring_stats.VolumeGuid.volume_guid, 
                                              GUID_SIZE_IN_CHARS, app_confp->src_volp,
                                              GUID_SIZE_IN_CHARS - 1)) {
					printf("strncpy_s failed to copy");
					ret = -1;
					goto out;
				}
                                monitoring_stats.ReqStat = GET_CHURN_STATS;
				ret = inm_ioctl(app_confp->ctrl_dev_fd,
					IOCTL_INMAGE_GET_MONITORING_STATS,
					&monitoring_stats);
                               if (ret != 0) {
					perror("IOCTL_INMAGE_GET_MONITORING_STATS failed\n");
				} else {
					printf("IOCTL_INMAGE_GET_MONITORING_STATS issued "
                                               "successfully for CHURN stats\n");
					printf("Volume Name : %s \n", monitoring_stats.VolumeGuid.volume_guid);
					printf("Tracked IO bytes = %lld \n", 
                                        (long long int)monitoring_stats.ChurnStats.NumCommitedChangesInBytes);
				}
			}
			break;

		case BMAP_STATS_OP:
			if (strlen(app_confp->src_volp) > 0) {
				VOLUME_BMAP_STATS vol_bmap_stats;

				memset(&vol_bmap_stats, 0, sizeof(vol_bmap_stats));
				if (strncpy_s(vol_bmap_stats.VolumeGuid.volume_guid, GUID_SIZE_IN_CHARS, app_confp->src_volp, GUID_SIZE_IN_CHARS - 1)) {
					printf("strncpy_s failed to copy");
					ret = 1;
					goto out;
				}
				if (inm_ioctl(app_confp->ctrl_dev_fd,
					IOCTL_INMAGE_GET_VOLUME_BMAP_STATS,
					&vol_bmap_stats) == -1) {
					perror("IOCTL_INMAGE_GET_VOLUME_BMAP_STATS ioctl failed\n");
					ret = -1;
				} else {
					printf("IOCTL_INMAGE_VOLUME_BMAP_STATS issued successfully\n");
					printf("Volume Name : %s \n", vol_bmap_stats.VolumeGuid.volume_guid);
					printf("Bitmap Granularity  = %llu\n",
						(unsigned long long)vol_bmap_stats.bmap_gran);
					printf("Bitmap Data Size = %llu \n", (unsigned long long)vol_bmap_stats.bmap_data_sz);
					printf("# of Bitmap Dirty Blks = %u\n", vol_bmap_stats.nr_dbs);
					ret = 0;
				}
			}
			break;
		case SET_VERBOSITY_OP:
			if (inm_ioctl(app_confp->ctrl_dev_fd,
				IOCTL_INMAGE_SET_INVOLFLT_VERBOSITY,
				&(app_confp->drv_verbose)) == -1) {
				printf("IOCTL_INMAGE_SET_INVOLFLT_VERBOSITY ioctl failed to set verbosity to %u\n",
					 app_confp->drv_verbose);
				ret = -1;
			} else {
				printf("IOCTL_INMAGE_SET_INVOLFLT_VERBOSITY issued successfully\n");
				printf("verbosity is set to = %u\n", app_confp->drv_verbose);
				ret = 0;
			}
			break;
	case BLOCK_AT_LUN_OP:
#ifndef INM_LINUX
		printf("Invalid ioctl, ioctl only valid for Linux platform\n");
		ret = EINVAL;
		break;
#endif
		if(strlen(app_confp->lun_name) <= 0){
			printf("provide the AT lun name, usage:\n");
			printf("inm_dmit --op=block_at_lun --lun_name=<lun name>\n");
			ret = EINVAL;
			break;
		}
		if (!strstr(app_confp->lun_name, "/dev/sd")) {
			printf("%s is invalid device name\n", app_confp->lun_name);
			ret = EINVAL;
			break;
		}
		if (strcpy_s(at_lun_info.atdev_name, INM_GUID_LEN_MAX, app_confp->lun_name)) {
			printf("strcpy_s failed");
			ret = -1;
			goto out;
		}
		at_lun_info.flag = ADD_AT_LUN_GLOBAL_LIST;
		if(inm_ioctl(app_confp->ctrl_dev_fd, IOCTL_INMAGE_BLOCK_AT_LUN,
		&at_lun_info) == -1) {
			printf("IOCTL_INMAGE_BLOCK_AT_LUN failed with %d\n",errno);
			ret = EINVAL;
		}
		break;
	case UNBLOCK_AT_LUN_OP:
#ifndef INM_LINUX
		printf("Invalid ioctl, ioctl only valid for Linux platform\n");
		ret = EINVAL;
		break;
#endif
		if(strlen(app_confp->lun_name) <= 0){
			printf("provide the AT lun name, usage:\n");
			printf("inm_dmit --op=unblock_at_lun --lun_name=<lun name>\n");
			ret = EINVAL;
			break;
		}
		if (!strstr(app_confp->lun_name, "/dev/sd")) {
			printf("%s is invalid device name\n", app_confp->lun_name);
			ret = EINVAL;
			break;
		}
		if (strcpy_s(at_lun_info.atdev_name, INM_GUID_LEN_MAX, app_confp->lun_name)) {
			printf("strcpy_s failed");
			ret = -1;
			goto out;
		}
		at_lun_info.flag = DEL_AT_LUN_GLOBAL_LIST;
		if(inm_ioctl(app_confp->ctrl_dev_fd, IOCTL_INMAGE_BLOCK_AT_LUN,
				&at_lun_info) == -1) {
			printf("For unblocking IOCTL_INMAGE_BLOCK_AT_LUN failed with %d\n",errno);
			ret = EINVAL;
		}
		break;


        case FREEZE_VOL_OP:
                if (!app_confp->src_volp) {
                        printf("source volume is missing\n");
                        ret = -1;
                        break;
                }
                printf("source volume is [%s]\n", app_confp->src_volp);
                if (!app_confp->timeout || app_confp->timeout < 0) {
                        printf("timout value is missing\n");
                        ret = -1;
                        break;
                }
                printf("timout value is [%d]\n", app_confp->timeout);
                if (!app_confp->nr_vols || app_confp->nr_vols < 0) {
                        printf("nr_vols value is missing\n");
                        ret = -1;
                        break;
                }
                printf("nr_vols value is [%d]\n", app_confp->nr_vols);
                freeze_info.vol_info = (volume_info_t *) calloc (app_confp->nr_vols, sizeof(volume_info_t));
                if(!freeze_info.vol_info) {
                      printf("Unable to allocate the buffer\n");
                      ret = -1;
                      goto out;
                }
                numvol = 0;
                vol_name = strtok (app_confp->src_volp, ", ");
                do {
                    strcpy_s(freeze_info.vol_info[numvol++].vol_name, INM_GUID_LEN_MAX, vol_name);
                } while ((vol_name = strtok (NULL, ", ")));
                freeze_info.timeout = app_confp->timeout;
                freeze_info.nr_vols = app_confp->nr_vols;
 
                if (inm_ioctl(app_confp->ctrl_dev_fd,
                             IOCTL_INMAGE_FREEZE_VOLUME,
                             &freeze_info) < 0) {
                            for(numvol = 0; numvol < freeze_info.nr_vols; numvol++) {
                                if(freeze_info.vol_info[numvol].status & STATUS_FREEZE_FAILED) {
                                    printf("The freeze got failed for vol name [%s]\n",
                                            freeze_info.vol_info[numvol].vol_name);
                                } else if(!freeze_info.vol_info[numvol].status) {
                                    printf("The freeze got succeeded for vol name [%s]\n",
                                            freeze_info.vol_info[numvol].vol_name);
                                }
                            }
                            perror("IOCTL_INMAGE_FREEZE_VOLUME ioctl failed\n");
                            ret = -1;
                } else {
                        for(numvol = 0; numvol < freeze_info.nr_vols; numvol++) {
                            if(!freeze_info.vol_info[numvol].status) {
                                printf("The freeze got succeeded for vol name [%s]\n",
                                        freeze_info.vol_info[numvol].vol_name);
                            }
                        }
                        printf("IOCTL_INMAGE_FREEZE_VOLUME issued successfully\n");
                        ret = 0;
                }
                break;

        case THAW_VOL_OP:
                if (!app_confp->src_volp) {
                        printf("source volume is missing\n");
                        ret = -1;
                        break;
                }
                printf("source volume is [%s]\n", app_confp->src_volp);
                if (!app_confp->nr_vols || app_confp->nr_vols < 0) {
                        printf("nr_vols value is missing\n");
                        ret = -1;
                        break;
                }
                printf("nr_vols value is [%d]\n", app_confp->nr_vols);
                thaw_info.vol_info = (volume_info_t *) calloc (app_confp->nr_vols, sizeof(volume_info_t));
                if(!thaw_info.vol_info) {
                      printf("Unable to allocate the buffer\n");
                      ret = -1;
                      goto out;
                }
                numvol = 0;
                vol_name = strtok (app_confp->src_volp, ", ");
                do {
                    strcpy_s(thaw_info.vol_info[numvol++].vol_name, INM_GUID_LEN_MAX, vol_name);
                } while ((vol_name = strtok (NULL, ", ")));
                thaw_info.nr_vols = app_confp->nr_vols;
                if (inm_ioctl(app_confp->ctrl_dev_fd,
                             IOCTL_INMAGE_THAW_VOLUME,
                             &thaw_info) < 0) {
                            for(numvol = 0; numvol < thaw_info.nr_vols; numvol++) {
                                if(thaw_info.vol_info[numvol].status & STATUS_THAW_FAILED) {
                                    printf("The thaw got failed for vol name [%s]\n",
                                            thaw_info.vol_info[numvol].vol_name);
                                } else if(!thaw_info.vol_info[numvol].status) {
                                    printf("The thaw got succeeded for vol name [%s]\n",
                                            thaw_info.vol_info[numvol].vol_name);
                                }
                            }
                            perror("IOCTL_INMAGE_THAW_VOLUME ioctl failed\n");
                            ret = -1;
                } else {
                        for(numvol = 0; numvol < thaw_info.nr_vols; numvol++) {
                            if(!thaw_info.vol_info[numvol].status) {
                                printf("The thaw got succeeded for vol name [%s]\n",
                                        thaw_info.vol_info[numvol].vol_name);
                            }
                        }
                        printf("IOCTL_INMAGE_THAW_VOLUME issued successfully\n");
                        ret = 0;
                }
                break;

#ifdef INM_LINUX
       	case INIT_DRIVER_FULLY_OP:
               if (inm_ioctl(app_confp->ctrl_dev_fd, IOCTL_INMAGE_INIT_DRIVER_FULLY, &ret)){
                       perror("IOCTL_INMAGE_INIT_DRIVER_FULLY ioctl failed\n");
                       ret = -1;
               }
               break;
#endif
       	case GET_BLK_MQ_STATUS_OP:
                if (!app_confp->src_volp) {
                        printf("Disk path is missing\n");
                        ret = -1;
                } else {
                    BLK_MQ_STATUS blk_mq_status;
                    memset(&blk_mq_status, 0, sizeof(blk_mq_status));
                    if (strncpy_s(blk_mq_status.VolumeGuid.volume_guid, 
                                  GUID_SIZE_IN_CHARS, app_confp->src_volp,
                                  GUID_SIZE_IN_CHARS - 1)) {
                        printf("strncpy_s failed to copy");
                        ret = -1;
                        goto out;
                    }

                    if (inm_ioctl(app_confp->ctrl_dev_fd,
                              IOCTL_INMAGE_GET_BLK_MQ_STATUS,
                              &blk_mq_status) < 0) {
                        perror("IOCTL_INMAGE_GET_BLK_MQ_STATUS ioctl failed\n");
                        ret = -1;
                    } else {
                        if (blk_mq_status.blk_mq_enabled)
                            printf("Enabled\n");
                        else
                            printf("Disabled\n");
                        ret = 0;
                    }
                }
                break;
        case GET_VOLUME_STATS_V2_OP:
                if (!app_confp->src_volp) {
                    printf("source volume is missing\n");
                    ret = -1;
                    break;
                }

                TELEMETRY_VOL_STATS telemetry_vol_stats;
                VOLUME_STATS_DATA *drv_statsp = NULL;
                VOLUME_STATS_V2 *vol_statsp = NULL;

                memset(&telemetry_vol_stats, 0, sizeof(telemetry_vol_stats));
                if (strncpy_s(telemetry_vol_stats.vol_stats.VolumeGUID, 
                                    GUID_SIZE_IN_CHARS, app_confp->src_volp,
                                    GUID_SIZE_IN_CHARS - 1)) {
                    printf("strncpy_s failed to copy");
                    ret = -1;
                    goto out;
                }
                ret = inm_ioctl(app_confp->ctrl_dev_fd,
                                    IOCTL_INMAGE_GET_VOLUME_STATS_V2,
                                    &telemetry_vol_stats);
                if (ret != 0) {
                    perror("IOCTL_INMAGE_GET_VOLUME_STATS_V2 failed\n");
                    ret = -1;
                    break;
                }

                printf("IOCTL_INMAGE_GET_VOLUME_STATS_V2 issued successfully\n");

                drv_statsp = &(telemetry_vol_stats.drv_stats);
                vol_statsp = &(telemetry_vol_stats.vol_stats);

                printf("Volume Name:                            %s\n", telemetry_vol_stats.vol_stats.VolumeGUID);
                printf("usMajorVersion:                         %u\n", drv_statsp->usMajorVersion);
                printf("usMinorVersion:                         %u\n", drv_statsp->usMinorVersion);
                printf("ulVolumesReturned:                      %lu\n", drv_statsp->ulVolumesReturned);
                printf("ulNonPagedMemoryLimitInMB:              %lu\n", drv_statsp->ulNonPagedMemoryLimitInMB);
                printf("LockedDataBlockCounter:                 %lu\n", drv_statsp->LockedDataBlockCounter);

                printf("ulTotalVolumes:                         %lu\n", drv_statsp->ulTotalVolumes);
                printf("ulNumProtectedDisk:                     %d\n", drv_statsp->ulNumProtectedDisk);
                printf("eServiceState:                          %u\n", drv_statsp->eServiceState);
                printf("eDiskFilterMode:                        %u\n", drv_statsp->eDiskFilterMode);
                printf("LastShutdownMarker:                     %u\n", drv_statsp->LastShutdownMarker);

                printf("PersistentRegistryCreated:              %d\n", drv_statsp->PersistentRegistryCreated);
                printf("ulDriverFlags:                          %lx\n", drv_statsp->ulDriverFlags);

                printf("ulCommonBootCounter:                    %ld\n", drv_statsp->ulCommonBootCounter);
                printf("ullDataPoolSizeAllocated:               %llu\n", drv_statsp->ullDataPoolSizeAllocated);
                printf("ullPersistedTimeStampAfterBoot:         %llu\n", drv_statsp->ullPersistedTimeStampAfterBoot);
                printf("ullPersistedSequenceNumberAfterBoot:    %llu\n", drv_statsp->ullPersistedSequenceNumberAfterBoot);

                printf("ullDataPoolSize:                        %llu\n", vol_statsp->ullDataPoolSize);
                printf("liDriverLoadTime:                       %lld\n", vol_statsp->liDriverLoadTime.QuadPart);
                printf("llTimeJumpDetectedTS:                   %lld\n", vol_statsp->llTimeJumpDetectedTS);
                printf("llTimeJumpedTS:                         %lld\n", vol_statsp->llTimeJumpedTS);
                printf("liLastS2StartTime:                      %lld\n", vol_statsp->liLastS2StartTime.QuadPart);
                printf("liLastS2StopTime:                       %lld\n", vol_statsp->liLastS2StopTime.QuadPart);
                printf("liLastAgentStartTime:                   %lld\n", vol_statsp->liLastAgentStartTime.QuadPart);
                printf("liLastAgentStopTime:                    %lld\n", vol_statsp->liLastAgentStopTime.QuadPart);
                printf("liLastTagReq:                           %lld\n", vol_statsp->liLastTagReq.QuadPart);
                printf("liStopFilteringAllTimeStamp:            %lld\n", vol_statsp->liStopFilteringAllTimeStamp.QuadPart);

                printf("ullTotalTrackedBytes:                   %llu\n", vol_statsp->ullTotalTrackedBytes);
                printf("ulVolumeFlags:                          %lx\n", vol_statsp->ulVolumeFlags);
                printf("ulVolumeSize:                           %llu\n", vol_statsp->ulVolumeSize.QuadPart);

                printf("liVolumeContextCreationTS:              %lld\n", vol_statsp->liVolumeContextCreationTS.QuadPart);

                printf("liStartFilteringTimeStamp:              %lld\n", vol_statsp->liStartFilteringTimeStamp.QuadPart);
                printf("liStartFilteringTimeStampByUser:        %lld\n", vol_statsp->liStartFilteringTimeStampByUser.QuadPart);

                printf("liStopFilteringTimeStamp:               %lld\n", vol_statsp->liStopFilteringTimeStamp.QuadPart);
                printf("liStopFilteringTimestampByUser:         %lld\n", vol_statsp->liStopFilteringTimestampByUser.QuadPart);
                printf("liClearDiffsTimeStamp:                  %lld\n", vol_statsp->liClearDiffsTimeStamp.QuadPart);
                printf("liCommitDBTimeStamp                     %lld\n", vol_statsp->liCommitDBTimeStamp.QuadPart);
                printf("liGetDBTimeStamp                        %lld\n", vol_statsp->liGetDBTimeStamp.QuadPart);

                break;
    case GET_NAME_MAPPING_OP:
                ret = get_pname_mapping();
                break;

    case LCW_OP:
                ret = perform_lcw_op();
                break;

	default:
		printf("debug: invalid op-code\n");
		ret = EINVAL;
		break;
	}

	INM_TRACE_END("%d", ret);

out:
	if (get_stat){
		free(get_stat);
	}
    	
	if (attr){
		if(attr->bufp)
			free(attr->bufp);
		free(attr);
	}
       
        if (freeze_info.vol_info) {
           free(freeze_info.vol_info);
           freeze_info.vol_info = NULL;
        }

        if (thaw_info.vol_info) {
            free(thaw_info.vol_info);
            thaw_info.vol_info = NULL;
        }

	return ret;
} /* end of inm_process_cmd() */


/*
 * inm_read_prefix fn 
 * @ tag : expected tag value
 **/
int
inm_read_prefix(int fd, int tag)
{
	SVD_PREFIX prefix;
	int nr_bytes = 0;
	int ret = -1;

	INM_TRACE_START();

	nr_bytes = read(fd, &prefix, sizeof(SVD_PREFIX));
	if (nr_bytes == sizeof(SVD_PREFIX)) {
		if (prefix.tag != tag) {
			printf("err : expectd tag %d, tag recvd %d\n", 
				tag, prefix.tag);
			ret = -1;
		} else {
			ret = 0;
		}
	} else {
		ret = -1;
		perror("err : failed to read prefix\n");
	}
	
	INM_TRACE_END("%d", ret);

	return ret;
} /* end of inm_read_prefix() */

/*
 * inm_read_data : reads the data from the descriptor
 */
int
inm_read_data(int fd, int len, void *datap)
{
	int nr_bytes = 0;
	int ret = -1;

	INM_TRACE_START();

	nr_bytes = read(fd, (char *)datap, len);
	if (nr_bytes == len) {
		ret = 0;
	} else {
		ret = -1;
	}

	INM_TRACE_END("%d", ret);

	return ret;
} /* end of inm_read_data() */

/* subr2 contents */


	


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
} /* end of inm_validate_ts() */

/*
 * inm_replicate : wrapper call around replicate fns
 * based on version passed from cmd line, it 
 * invokes appropriate call
 */
int
inm_replicate(char *src_volp, char *tgt_volp, char *lun_name)
{
	int ret = -1;

	INM_TRACE_START();

	if (app_confp->ver == VERSION_V1) {
		ret = replicate_v1(src_volp, tgt_volp, lun_name);
	} else if (app_confp->ver == VERSION_V2) {
		ret = replicate_v2(src_volp, tgt_volp, lun_name);
	} else if (app_confp->ver == VERSION_V3) {
		ret = replicate_v3(src_volp, tgt_volp, lun_name);
	}

	INM_TRACE_END("%d", ret);
	return ret;
} /* end of inm_replicate() */
	
/*
 * inm_process_tags : wrapper around version based fns
 */
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
} /* end of inm_process_tags() */

int
inm_apply_dblk(int tgt_fd, void *argp, void *datap)
{
	int ret = -1;

	INM_TRACE_START();

	if (app_confp->ver == VERSION_V1) {
		ret = apply_dblk_v1(tgt_fd, (SVD_DIRTY_BLOCK *) argp,
					datap);
	} else if (app_confp->ver == VERSION_V2) {
		ret = apply_dblk_v2(tgt_fd, (SVD_DIRTY_BLOCK_V2 *) argp,
					datap);
	} else if (app_confp->ver == VERSION_V3) {
#if 0
		ret = apply_dblk_v3(tgt_fd, (SVD_DIRTY_BLOCK_V3 *) argp,
					datap);
#endif
	}

	INM_TRACE_END("%d", ret);
	return ret;
} /* end of inm_apply_dblk() */

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
                ret = process_dm_stream_v3(tgt_fd,
						 (UDIRTY_BLOCK_V3 *) argp);
        }

        INM_TRACE_END("%d", ret);
        return ret;
} /* end of inm_process_dm_stream() */

int
inm_process_dfm(int tgt_fd, void *argp)
{
        int ret = -1;

        INM_TRACE_START();

        if (app_confp->ver == VERSION_V1) {
                ret = process_dfm_v1(tgt_fd, (UDIRTY_BLOCK *) argp);
        } else if (app_confp->ver == VERSION_V2) {
                ret = process_dfm_v2(tgt_fd, (UDIRTY_BLOCK_V2 *) argp);
        } else if (app_confp->ver == VERSION_V3) {
                ret = process_dfm_v3(tgt_fd, (UDIRTY_BLOCK_V3 *) argp);
        }

        INM_TRACE_END("%d", ret);
        return ret;
} /* end of inm_process_dfm() */

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
} /* end of inm_process_md() */

int
inm_validate_prefix(inm_addr_t addr, int tag)
{
	SVD_PREFIX *svd_prefixp = (SVD_PREFIX *) addr;
	int ret = -1;

	INM_TRACE_START();

	if (svd_prefixp->tag != tag) {
		printf("Expected tag = %d\n", tag);
		printf("tag %d\n", svd_prefixp->tag);
		printf("flags %d\n", svd_prefixp->Flags);
		printf("count %d\n", svd_prefixp->count);
	} else {
		ret = 0;
	}

	INM_TRACE_END("%d", ret);

	return ret;
} /* end of inm_validate_prefix() */

int
inm_get_db_trans(char *src_volp, char *tgt_volp)
{
	int ret = -1;

	INM_TRACE_START();

	if (app_confp->ver == VERSION_V1) {
		ret = get_db_trans_v1(src_volp, tgt_volp, 0);
	} else if (app_confp->ver == VERSION_V2) {
		ret = get_db_trans_v2(src_volp, tgt_volp, 0);
	} else if (app_confp->ver == VERSION_V3) {
		ret = get_db_trans_v3(src_volp, tgt_volp, 0);
	}

	INM_TRACE_END("%d", ret);
	return ret;
} /* end of inm_get_db_trans() */
	
int
inm_get_db_commit(char *src_volp, char *tgt_volp)
{
	int ret = -1;

	INM_TRACE_START();

	if (app_confp->ver == VERSION_V1) {
		ret = get_db_trans_v1(src_volp, tgt_volp, 1);
	} else if (app_confp->ver == VERSION_V2) {
		ret = get_db_trans_v2(src_volp, tgt_volp, 1);
	} else if (app_confp->ver == VERSION_V3) {
		ret = get_db_trans_v3(src_volp, tgt_volp, 1);
	}

	INM_TRACE_END("%d", ret);
	return ret;
} /* end of inm_get_db_commit() */
	
