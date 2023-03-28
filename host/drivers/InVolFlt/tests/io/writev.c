#include "libinclude.h"

#define IO_SZ           4608
#define PAGE_SZ         4096
#define NR_IOVECS       8
#define NR_THREADS      8

#define BUF_SZ          ((IO_SZ + PAGE_SZ) * NR_IOVECS)

#define IORATE          1024 /* KBPS */
#define IORATE_THREAD   ((IORATE * 1024 )/ NR_THREADS)

struct ioargs {
    int idx;
    char *fname;
};
 
struct iovec iov[NR_IOVECS];
int thret = 0;

void
randomise_iovecs(struct iovec *src, struct iovec *dest, int n)
{
    int bitmap = 0;
    int si = 0;
    int di = 0;
    long int r = 0;
    
    while (di < n) {
        si = random() % n;

        if (!(bitmap & (1 << si))) {
            dest[di++] = src[si];
            bitmap |= (1 << si);
        }
    }
}

void *
do_io(void *data)
{
    int i = 0;
    int j = 0;
    int fd = -1;
    unsigned long long size = 0;
    unsigned long long start = 0;
    unsigned long long ostart = 0;
    unsigned long long end = 0;
    struct ioargs *args = (struct ioargs *)data;
    struct iovec wiov[NR_IOVECS];
    
    fd = open(args->fname, O_RDWR | O_DIRECT);
    if (fd < 0 ) {
        perror("Open");
        exit(-1);
    }

    size = lseek(fd, 0, SEEK_END);
    ostart = start = (size / NR_THREADS) * args->idx;
    end = (size / NR_THREADS) * (args->idx + 1);

    printf("%d: %llu to %llu of %llu\n", 
           args->idx, start, end, size);
    
    if (start != lseek(fd, start, SEEK_SET)) {
        perror("lseek start");
        exit(-1);
    }

    randomise_iovecs(iov, wiov, NR_IOVECS);

    j = 0;
    while (start < end) {
        i = writev(fd, wiov, NR_IOVECS);
        if (i != (NR_IOVECS * IO_SZ)) {
            perror("writev");
            printf("Only wrote %d bytes, offset = %llu\n", 
                   i, (unsigned long long)lseek(fd, 0, SEEK_CUR));
            break;
        }
    
        start += i;
        if ((start - ostart) >= 10485760) {
            printf("%d: Reached %llu\n", args->idx, start);
            ostart = start;
        }
        
        j++;
        if (j * (NR_IOVECS * IO_SZ) >= IORATE_THREAD) {
            sleep(1);
            j=0;
            randomise_iovecs(iov, wiov, NR_IOVECS);
        }
    }

    close(fd);
    printf("%d: Write Completed\n", args->idx);
    pthread_exit(&thret);
}
      
int
main(int argc, char *argv[])
{
    char *str = NULL;
    char *buf = NULL;
    char c = '0';
    pthread_t thr[NR_THREADS];
    struct ioargs args[NR_THREADS];
    int i = 0;
    int j = 0;
    unsigned int seed = 0;

    errno = posix_memalign((void **)&buf, PAGE_SZ, BUF_SZ);
    if (!buf) {
        perror("aligned_alloc");
        exit(-1);
    }

    memset((void *)buf, c, BUF_SZ);

    for (i = 0; i < NR_IOVECS; i++) {
        str = iov[i].iov_base = (buf + ((IO_SZ + PAGE_SZ) * i));
        iov[i].iov_len = IO_SZ;
        for (j = 0; j < IO_SZ; j++)
            str[j] = c + (char)i;
    }

    seed = (unsigned int)time(NULL);
    printf("Seed: %u\n", seed);
    srandom(seed);

    for (i = 0; i < NR_THREADS; i++) {

        args[i].idx = i;
        args[i].fname = argv[1];

        if (pthread_create(&thr[i], NULL, do_io, &args[i])) {
            perror("pthread_create");
            exit(-1);
        }
    }

    for (i = 0; i < NR_THREADS; i++)
        pthread_join(thr[i], NULL);

    exit(0);    
}

