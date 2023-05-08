#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>

#define PSIZE 4096
#define BSIZE 1048576UL

int 
main(int argc, char *argv[])
{
    int fd1 = -1;
    int fd2 = -1;
    char *buf1 = NULL;
    char *buf2 = NULL;
    int nbytes = 0;
    int slab = 0;
    int i = 0;
    int error = 0;
    off_t start = 0;
    int mismatch = 0;
    unsigned long long mcount = 0;
    off_t fsize = 0;

    if (argc != 3) {
        fprintf(stderr, "filecmp <file 1> <file 2>");
        exit(-1);
    }

    if (posix_memalign((void **)&buf1, PSIZE, BSIZE)) {
        perror("buf1");
        exit(-1);
    }

    if (posix_memalign((void **)&buf2, PSIZE, BSIZE)) {
        perror("buf2");
        exit(-1);
    }

    fd1 = open(argv[1], O_RDONLY | O_DIRECT);
    if (fd1 < 0 ) {
        perror(argv[1]);
        exit(-1);
    }
        
    fd2 = open(argv[2], O_RDONLY | O_DIRECT);
    if (fd1 < 0 ) {
        perror(argv[2]);
        exit(-1);
    }

    fsize = lseek(fd1, 0, SEEK_END);
    if (fsize != lseek(fd2, 0, SEEK_END)) {
        printf("File size mismatch\n");
        exit(-1);
    }

    lseek(fd1, 0, SEEK_SET);
    lseek(fd2, 0, SEEK_SET);

    while (1) {
        if ((nbytes = read(fd1, buf1, BSIZE)) == -1) {
            perror("Read1");
            exit(-1);
        }

        if (nbytes != read(fd2, buf2, BSIZE)) {
            perror("Read2");
            exit(-1);
        }

        for (i = 0; i < nbytes; i++) {
            if (buf1[i] != buf2[i]) {
                if (!mismatch) {
                    if (!error)
                        printf("Files dont match\n");

                    start = ((BSIZE * slab) + i);
                    mismatch = 1;
                    error = -1;
                }
                mcount++;
            } else {
                if (mismatch) {
                    printf("%lu - %lu bytes mismatch\n", start,
                            ((BSIZE * slab) + (i - 1)));

                    start = 0;
                    mismatch = 0;
                }
            }
        }

        if (nbytes != BSIZE)
            break;

        slab++;
    }

    if (!error) {
        printf("%lu bytes match\n", lseek(fd1, 0, SEEK_CUR));
    } else {
        if (mismatch)
            printf("%lu - %lu bytes mismatch\n", start, 
                    lseek(fd1, 0, SEEK_CUR) - 1);

        printf("TOTAL: %llu bytes mismatch\n", mcount);
    }

    exit(error);
}
        
