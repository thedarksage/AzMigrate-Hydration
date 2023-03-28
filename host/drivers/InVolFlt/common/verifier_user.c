#include "verifier.h"

void
verify_file(char *fname)
{
    int fd = -1;
    unsigned long long size = 0;
    char *buf = NULL;

    if (!fname)
        exit(-1);

    fd = open(fname, O_RDONLY);
    if (fd < 0) {
        perror("Open");
        exit(-1);
    }

    size = lseek(fd, 0, SEEK_END);
    printf("File Size: %llu\n", size);

    buf = malloc(size);
    if (!buf) {
        perror("Malloc");
        exit(-1);
    }

    if (lseek(fd, 0, SEEK_SET) != 0) {
        perror("Seek SET");
        exit(-1);
    }

    if (size != read(fd, buf, size)) {
        perror("Read");
        exit(-1);
    }

    if (inm_verify_change_node_data(buf, size, 1)) {
        printf("Verification failed");
        exit(-1);
    }
}

int
main(int argc, char *argv[])
{
    verify_file(argv[1]);
}
