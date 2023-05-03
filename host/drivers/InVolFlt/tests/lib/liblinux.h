#ifndef _LIBLINUX_H
#define _LIBLINUX_H

#define _GNU_SOURCE

#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <time.h>
#include <sys/time.h>
#include <linux/fs.h>
#include <getopt.h>
#include <assert.h>
#include <signal.h>
#include <pthread.h>
#include <sys/uio.h>
#include <sys/sysmacros.h>

#define FLTCTRL "/dev/involflt"
#define RUNDIR  "/var/run/flt_cvt"

int exec_cmd(char *cmd, char *output, int output_size);
unsigned long long get_device_size(char *dev);
unsigned int get_major(char *dev);

#endif
