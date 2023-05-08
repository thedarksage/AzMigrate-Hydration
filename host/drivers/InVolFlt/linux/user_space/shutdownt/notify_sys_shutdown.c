#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/kdev_t.h>
#include "involflt.h"
#include "inm_cmd_parser.h"

#define INVOLFLT_FILTER_TARGET_NAME "involflt"
#define MAX_TARGET_TYPE_LEN 256
#define MAX_TARGET_PARAMS_LEN 256
#define MAX_DM_NAME_LENGTH 256
#define CONTROL_DEVICE "/dev/involflt"

int notify_sys_pre_shutdown()
{
	int fd = 0;

	fd = open(CONTROL_DEVICE, O_RDWR);
	if(fd == -1) {
		printf("Failed to open control device\n");
		return -1;
	}

	if(ioctl(fd, IOCTL_INMAGE_SYS_PRE_SHUTDOWN, NULL) == -1) {
		printf("IOCTL_INMAGE_SYS_PRE_SHUTDOWN ioctl failed\n");
		close(fd);
		return -1;
	}
	close(fd);
	
	return 0;
}

int notify_sys_shutdown_routine()
{
	int fd = 0;
	SYS_SHUTDOWN_NOTIFY_INPUT sys_shutdown_notify;

	fd = open(CONTROL_DEVICE, O_RDWR);
	if(fd == -1) {
		printf("Failed to open control device\n");
		return 0;
	}

	if(ioctl(fd, IOCTL_INMAGE_SYS_SHUTDOWN, &sys_shutdown_notify) == -1) {
		printf("IOCTL_INMAGE_SYS_SHUTDOWN ioctl failed\n");
		close(fd);
		return 0;
	}
	close(fd);
	
	return 1;
}

int main(int argc, char *argv[])
{
    int shutdown = 0;
    int c = 0;
    int option_index = 0;
    int ret = 0;
	
    static struct inm_option long_options[] = {
        /* These options set a flag. */
        {"pre",  INM_NO_ARGUMENT, NULL, 1},
        {0, 0, 0, 0} };
    
    c = inm_getopt_long(argc, argv, ":c:", long_options, &option_index);

    switch (c) {
        case 1:     ret = notify_sys_pre_shutdown();
                    shutdown = 0;
                    break;

        default:    shutdown = 1;
                    break;
    }

    if (shutdown)
    	notify_sys_shutdown_routine();

	return ret;
}
