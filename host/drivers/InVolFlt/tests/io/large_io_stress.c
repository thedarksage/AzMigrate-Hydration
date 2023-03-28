#include "libinclude.h"

#define USECS_IN_A_SEC 1000000

#define BUFSZ (( 4 * 1048576 ))
#define NUMIO 8
/* Expect atleast 512MB disk */
#define DSIZE 536870912LL

/* Sync */
pthread_mutex_t mt_sync;
pthread_cond_t c_sync = PTHREAD_COND_INITIALIZER;
int numt = 1;
int pending = 0;
int rate_limit = 0;

/* Heartbeat */
pthread_mutex_t mt_hbt;
pthread_cond_t c_hbt = PTHREAD_COND_INITIALIZER;

int gargc = 0;
char *gargv[8];
int bufsz = BUFSZ;
int random_io = 0;
unsigned long long start_offset = 0;
unsigned long long max_offset = DSIZE;
int segsz = 0;
int nwrites = 0;
int totalio=0;

void usage()
{
    printf("Usage: < -b io size - default 4M > -d device name (upto 8 devices) < -t num threads > < -r Enable random IO > <-w write percentage>\n");
    exit(-1);
}

void *
doreads(void *arg)
{
    int tid = *(int *)arg;
    int idx = 0;
    char *dev = NULL;
    int fd;
    char *buf = NULL;
    int i = 0;
    unsigned long long offset = tid * segsz;

    buf = malloc(bufsz);
    if( !buf ) {
        perror("malloc");
        exit(-1);
    }

    idx = tid % gargc;
    dev = gargv[idx];

    printf("%d(r): %s\n", tid, dev);

    fd = open(dev, O_RDWR);
    if( fd < 0 ) {
        printf("%s\n", dev);
        perror("Open");
        exit(-1);
    }

    while(1) {
        lseek(fd, offset + start_offset, SEEK_SET);

        for(i=0; i < NUMIO; i++) {
            if( bufsz != read(fd, buf, bufsz) ) {
                printf("%s -> %lu\n", dev, lseek(fd, 0, SEEK_CUR));
                perror("read");
                exit(-1);
            }
        }
    
        if( random_io )
            offset = ( random() * segsz ) % max_offset;
        else {
            offset += ( numt * segsz);
            offset %= max_offset;
        }
    }
}

void *
dowrites(void *arg)
{
    int tid = *(int *)arg;
    int idx = 0;
    char *dev = NULL;
    int fd;
    char *buf = NULL;
    int i = 0;
    unsigned long long offset = tid * segsz;
    int iodone = 0;
    int iops = 0;

    if (rate_limit)
        iops = rate_limit/bufsz;

    buf = malloc(bufsz);
    if( !buf ) {
        perror("malloc");
        exit(-1);
    }

    idx = tid % gargc;
    dev = gargv[idx];

    printf("%d(w): %s\n", tid, dev);

    fd = open(dev, O_RDWR);
    if( fd < 0 ) {
        printf("%s\n", dev);
        perror("Open");
        exit(-1);
    }

    while(1) {
        lseek(fd, offset + start_offset, SEEK_SET);

        for(i=0; i < NUMIO; i++) {

            if( bufsz != read(fd, buf, bufsz) ) {
                printf("%s -> %lu\n", dev, lseek(fd, 0, SEEK_CUR));
                perror("write");
                exit(-1);
            }

            memset(buf, (int)*buf + 1, bufsz);
            lseek(fd, -1 * bufsz, SEEK_CUR);

            if( bufsz != write(fd, buf, bufsz) ) {
                printf("%s -> %lu\n", dev, lseek(fd, 0, SEEK_CUR));
                perror("write");
                exit(-1);
            }
        }

        /*
        pthread_mutex_lock(&mt_sync);
        pending--;
        if( !pending ) {
            pending = nwrites;
            
            /*
             * all threads have finished write(),
             * wake everyone to fsync()
             
            pthread_cond_broadcast(&c_sync);
            pthread_mutex_unlock(&mt_sync);
            /* Since we are the last thread
             * we send the heartbeat
             
            pthread_mutex_lock(&mt_hbt);
            pthread_cond_signal(&c_hbt);
            pthread_mutex_unlock(&mt_hbt);
        } else {
            pthread_cond_wait(&c_sync, &mt_sync);
            pthread_mutex_unlock(&mt_sync);
        }

        if( fsync(fd) ) {
            perror("fsync");
            break;
        }
        */

//        if( bufsz != write(fd, buf, bufsz) ) {
        
        if( random_io ) 
            offset = ( random() * segsz ) % max_offset;
        else {
            offset += ( numt * segsz);
            offset %= max_offset;
        }

        iodone += NUMIO;
        if (totalio && iodone >= totalio)
            break; 

        if (iops)
            usleep(USECS_IN_A_SEC/iops);

    }

    exit(0);
}

int 
main(int argc, char *argv[])
{
    pthread_t *th = NULL;
    int error = 0;
    struct timespec   ts;
    struct timeval    tp;
    int *th_arg; 
    int i = 0;
    int ioerror = 0;
    unsigned long long start = 0, diff = 0;
    int c;
    int writep = 0;

	while ((c = getopt(argc, argv, "b:d:l:n:rs:t:o:w:")) != -1 ) {
		switch(c) {
            case 'b':   bufsz = atoi(optarg);
                        break;

            case 'd':   gargv[gargc++] = optarg;
                        break;

            case 'l':   rate_limit = atoi(optarg);
                        break;

            case 'n':   totalio = atoi(optarg);
                        break;

            case 'o':   start_offset = strtoull(optarg, NULL, 0);
                        break;

            case 'r':   printf("Enabling random IO\n");
                        random_io = 1;
                        break;

            case 's':   max_offset = strtoull(optarg, NULL, 0);
                        break;

            case 't':   numt = atoi(optarg);
                        break;

            case 'w':   writep = atoi(optarg);
                        break;
        }
    }

    if( !gargc ) {
        printf("Need to sepcify devices\n");
        usage();
    }

    if (start_offset > max_offset) {
        fprintf(stderr, "Start offset cannot be greater than max offset");
        exit(-1);
    }

    nwrites = pending = ( numt * writep ) / 100;
    segsz = bufsz * NUMIO;

    th_arg = malloc(sizeof(int) * numt);
    if( !th_arg ) {
        perror("th_arg malloc");
        exit(-1);
    }

    printf("Buf Size = %d\n", bufsz);

    th = malloc(sizeof(pthread_t) * numt);
    if( !th ) {
        perror("Pthread malloc");
        exit(-1);
    }

    pthread_mutex_init(&mt_sync, NULL);
    pthread_mutex_init(&mt_hbt, NULL);

    /* Acquire heartbeat mutex to not miss any heartbeat */
    pthread_mutex_lock(&mt_hbt);

    for( i = 0; i < numt; i++ ) {
        th_arg[i] = i;

        if( i < nwrites )
            error = pthread_create(&th[i], NULL, dowrites, &th_arg[i]);
        else
            error = pthread_create(&th[i], NULL, doreads, &th_arg[i]);

        if( error ) {
            perror("Cannot start new thread");
            exit(-1);
        }
    }

    printf("Waiting ...\r");

    ioerror=1;
    while(1) {
        gettimeofday(&tp, NULL);

        /* Convert from timeval to timespec */
        ts.tv_sec  = tp.tv_sec;
        ts.tv_nsec = tp.tv_usec * 1000;
        ts.tv_sec += 60;

        error = pthread_cond_timedwait(&c_hbt, &mt_hbt, &ts);
        if( error == ETIMEDOUT ) {
            if( !ioerror ) {
                printf("\n");
                i = 0;
                ioerror = 1;
            }
            i++;
            printf("\rHung for %d minutes", i);
            fflush(stdout);
        } else if( error ) {
            perror("HBT pthread_cond_timedwait");
            fflush(stderr);
            exit(-1);
        } else {
            if( ioerror ) {
                start = tp.tv_sec;
                printf("\n");
                i=0;
                ioerror = 0;
            }
            i++;
            diff = tp.tv_sec - start;
            printf("\rPerforming IO for %llu minutes (%d)", diff/60, i);
            fflush(stdout);
        }

    }

    exit(0);

}


