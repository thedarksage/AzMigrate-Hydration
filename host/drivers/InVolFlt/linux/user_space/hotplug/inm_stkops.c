#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include "involflt.h"
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include "safecapismajor.h"

#define CONTROL_DEVICE "/dev/involflt"


int dev_stacking(char *devpath);


inm_stkops(int argc, char **argv) {
	char *devpath = NULL;
	struct stat dev_stat = {0};
	int err = -1;

	devpath=argv[2];
	
	
        if (stat(devpath, &dev_stat) < 0) {
		perror(devpath);
		return -1;
	} else {
		if (!(dev_stat.st_mode & S_IFBLK)) {
			fprintf(stderr, "%s : is not a block device\n", devpath);
			return -1;
		}
	}	
		
	switch(argv[1][0]) {
	case 'a':
		system("date >> /.inm_hotplug.log");
		err=dev_stacking(argv[2]);
		if (err) {
			fprintf(stdout, "stacking %s failed\n", devpath);
		} else {
			fprintf(stdout, "%s stacked successfully\n", devpath);
		}
		break;
        case 'r':
		err = dev_stop_filtering(devpath);
		if (err) {
			fprintf(stdout, "stop filtering for %s failed\n",
					devpath);
		} else {
			fprintf(stdout, "stop filtering for %s succeeded\n",
					devpath);
		}
		break;

	default:
		fprintf(stderr, "unknown option");
		break;
	}
	return err;
}

/* issues start filtering ioctl */

int
dev_stacking(char *devpath)
{
	int fd = -1; /* for control device */
	VOLUME_GUID guid;
	int err = -1;
	
	fd = open(CONTROL_DEVICE, O_RDWR);
	if (fd == -1) {
		perror(CONTROL_DEVICE);
		exit(2);
	}
	memset(&guid, 0, sizeof(guid));

	if (strcpy_s(&guid.volume_guid[0], GUID_SIZE_IN_CHARS, devpath)) {
		close(fd);
		return -EFAULT;
	}

	err = ioctl(fd, IOCTL_INMAGE_VOLUME_STACKING, &guid);
	if (err) {
		printf("IOCTL_INMAGE_START_FILTERING_DEVICE ioctl failed\n");
		close(fd);
		return -1;
	}
	close(fd);
	return 0;
}
/* issues stop filtering ioctl */

int
dev_stop_filtering(char *devpath)
{
	int fd = -1; /* for control device */
	VOLUME_GUID guid;
	int err = -1;
	
	fd = open(CONTROL_DEVICE, O_RDWR);
	if (fd == -1) {
		perror(CONTROL_DEVICE);
		exit(2);
	}
	memset(&guid, 0, sizeof(guid));

	if (strcpy_s(&guid.volume_guid[0], GUID_SIZE_IN_CHARS, devpath)) {
		close(fd);
		return -EFAULT;
	}

	err = ioctl(fd, IOCTL_INMAGE_STOP_FILTERING_DEVICE, &guid);
	if (err) {
		printf("IOCTL_INMAGE_STOP_FILTERING_DEVICE ioctl failed\n");
		close(fd);
		return -1;
	}
	close(fd);
	return 0;
}

int
inm_logger(int argc, char **argv)
{
	int fd = -1; /* for control device */
	int err = -1, buf_len = 0, i = 0;
	char *buf = NULL;

	fd = open(CONTROL_DEVICE, O_RDWR);
	if (fd == -1) {
		perror(CONTROL_DEVICE);
		exit(2);
	}
	
	for(i=1; i<argc; i++)
		 buf_len+=strlen(argv[i])+1;
	buf_len++; /* for null char */

	buf = (char *) malloc(buf_len * sizeof(char));
	if (!buf) {
		perror("malloc failed");
		close(fd);
		return -1;
	}

	for(i=1; i<argc; i++) {
		if (strcat_s(buf, buf_len, " ")) {
			close(fd);
			return -EFAULT;
		}

		if (strcat_s(buf, buf_len, argv[i]), buf) {
			close(fd);
			return -EFAULT;
		}
	}

	err = ioctl(fd, IOCTL_INMAGE_SHELL_LOG, buf);
	if (err) {
		printf("IOCTL_INMAGE_SHELL_LOG ioctl failed\n");
		close(fd);
		return -1;
	}
	close(fd);
	return 0;
}

int
main(int argc, char *argv[])
{

	int ret = -1;
	char *cbuf = NULL;

	if (argc < 3) {
		fprintf(stderr, "insuccient # of args\n");
		fprintf(stderr, "usage %s {a|r} {device name}\n");
		fprintf(stderr, " a - stacking\n");
		fprintf(stderr, " r - stop filtering\n");
		exit(2);
	}
	
	cbuf = strrchr(argv[0], '/');
	cbuf++;
	if (strcmp(cbuf, "inm_logger") == 0) {
		ret = inm_logger(argc, argv);
	} else {
		ret = inm_stkops(argc, argv);
	}

	return ret;
}	
