#include "libinclude.h"

int
open_control_dev()
{
    return open(FLTCTRL, O_RDONLY);
}

int
control_ioctl(int cmd, void *argv)
{
    int error = 0;
    int fd = -1;

    fd = open_control_dev();
    if (fd < 0)
        return -1;

    error = ioctl(fd, cmd, argv);

    close(fd);

    return error;
}
