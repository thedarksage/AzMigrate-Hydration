#include "libinclude.h"

#define SECTOR_SZ 512
#define MAX_FILES 8

void
usage(int exitval)
{
    fprintf(stderr, "Usage:"
                    "\n\t -d <diskname> - Disk name"
                    "\n\t -h - Help"
                    "\n\t -i - Direct IO"
                    "\n\t -l - Write to last byte"
                    "\n\t -o <offset> - Offset to write to"
                    "\n\t -p <pattern> - Pattern to writei"
                    "\n\t -s <size> - Size of buffer to write"
                    "\n\t ex: write_pattern -d /dev/sdc -d /dev/sdd -d /dev/sdf -s 512 -i -l -o 8 -p 10"
                    "\n");

    exit(exitval);
}

int
main(int argc, char *argv[])
{
    int i = 0;
    char *fname[MAX_FILES];
    int size = 0;
    unsigned long long pattern = 0;
    char *buf = NULL;
    int lastbyte = 0;
    int fd = -1;
    int oflags = O_CREAT | O_RDWR;
    unsigned long long offset;
    int ndisks = 0;

    while((i = getopt(argc, argv, "d:hilo:p:s:")) != -1) {
        switch (i) {
            case 'd':   if (ndisks < MAX_FILES) {
                            fname[ndisks++] = optarg;
                        } else {
                            fprintf(stderr, "ERROR: Upto MAX_FILES files");
                            exit(-1);
                        }
                        break;

            case 'h':   usage(0);
                        break;

            case 'i':   oflags |= O_DIRECT;
                        break;

            case 'l':   lastbyte = 1;
                        break;

            case 'o':   offset = strtoull(optarg, NULL, 0);
                        break;

            case 'p':   pattern = strtoull(optarg, NULL, 0);
                        break;

            case 's':   size = atoi(optarg);
                        break;

            default:    fprintf(stderr, "ERROR: Invalid option\n");
                        usage(-1);
                        break;
        }
    }

    if (!ndisks || !size) {
        fprintf(stderr, "ERROR: No file name or size provided\n");
        usage(-1);
    }

    if (posix_memalign((void **)&buf, SECTOR_SZ, size)) {
        perror("Bufer Alloc Failed");
        exit(-1);
    }

    if (lastbyte) {
        memset(buf, 0, size);
        *(unsigned long long *)(buf + size - sizeof(pattern)) = pattern;
    } else {
        memset(buf, (int)pattern, size);
    }

    for (i = 0; i < ndisks; i++) {
        fd = open(fname[i], oflags);
        if (fd < 0) {
            perror("Open File");
            exit(-1);
        }

        if (offset) {
            if (lseek(fd, offset, SEEK_SET) != offset) {
                perror("lseek");
                exit(-1);
            }
        }

        if (write(fd, buf, size) != size) {
            perror("write");
            exit(-1);
        }

        close(fd);
    }

    exit(0);
}
