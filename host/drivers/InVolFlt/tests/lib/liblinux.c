#include "libinclude.h"

int
exec_cmd(char *cmd, char *output, int output_size)
{
    FILE *file = NULL;

	file = popen(cmd, "r");
	if (!file)
		return -1;

	fgets(output, output_size, file);
	pclose(file);

    return 0;
}

unsigned long long
get_device_size(char *dev)
{
    int fd = -1;
    unsigned long long size = 0;

    fd = open(dev, O_RDONLY);
    if (fd < 0) 
        return -1;

    /*
    if (ioctl(fd, BLKGETSIZE, &nrsectors))
        nrsectors = 0;
    */

    if (ioctl(fd, BLKGETSIZE64, &size))
        return -1;

    /*
    if(size == 0 || size == nrsectors)
        size = ((unsigned long long) nrsectors) * 512;
    */

    return size;
}

unsigned int
get_major(char *dev)
{
	struct stat statbuf;
	int fd;

	if (stat(FLTCTRL, &statbuf)) 
		return -1;

	return major(statbuf.st_rdev);
}
