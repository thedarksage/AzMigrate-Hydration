#include <stdio.h>
#include <pthread.h>
#include <errno.h>
#include <stdlib.h>
#include <fcntl.h>

#define BUFSZ 1048576
#define SEGSZ 67108864


/* Sync */
pthread_mutex_t mt_sync;
pthread_cond_t c_sync = PTHREAD_COND_INITIALIZER;
int numt = 0;
int pending = 0;

/* Heartbeat */
pthread_mutex_t mt_hbt;
pthread_cond_t c_hbt = PTHREAD_COND_INITIALIZER;

int gargc = 0;
char **gargv = NULL;

void usage()
{
    printf("Usage\n");
}

void *
doio(void *arg)
{
    int tid = *(int *)arg;
    int idx = 0;
    char *dev = NULL;
    int fd;
    char buf[1048576];
    int i = 0;

    idx = tid % ( gargc - 2 ) + 2;
    dev = gargv[idx];

    printf("%d: %s\n", tid, dev);

    lseek(fd, SEGSZ * tid, SEEK_SET);

    fd = open(dev, O_RDWR);
    if( fd < 0 ) {
        printf("%s\n", dev);
        perror("Open");
        exit(-1);
    }

    while(1) {

        /* Skip 1M to confuse page cache */   
        lseek(fd, sizeof(buf), SEEK_CUR);
 
        if( sizeof(buf) != write(fd, buf, sizeof(buf)) ) {
            printf("%s\n", dev);
            perror("write");
        }
    
        pthread_mutex_lock(&mt_sync);
        pending--;
        if( !pending ) {
            pending = numt;
            /* all threads have finished write(),
             * wake everyone to fsync()
             */
            pthread_cond_broadcast(&c_sync);
            pthread_mutex_unlock(&mt_sync);
            /* Since we are the last thread
             * we send the heartbeat
             */
            pthread_mutex_lock(&mt_hbt);
            pthread_cond_signal(&c_hbt);
            pthread_mutex_unlock(&mt_hbt);
        } else {
            pthread_cond_wait(&c_sync, &mt_sync);
            pthread_mutex_unlock(&mt_sync);
        }
   
        if( ++i == 5 ) {
            fsync(fd);
            i=0;
            lseek(fd, SEGSZ * tid, SEEK_SET);
        } 
    }
}

int 
main(int argc, char *argv[])
{
    pthread_t *th = NULL;
    int error = 0;
    struct timespec   ts;
    struct timeval    tp;
    int th_arg[64]; 
    int i = 0;
    int ioerror = 0;
    unsigned long long start = 0, diff = 0;

    if( argc < 3 ) {
        usage();
        exit(-1);
    }

    pending = numt = atoi(argv[1]);
    if( numt <= 0 ) {
        usage();
        exit(-1);
    }

    gargc = argc;
    gargv = argv;

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
        error = pthread_create(&th[i], NULL, doio, &th_arg[i]);
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
        } else if( error ) {
            perror("HBT pthread_cond_timedwait");
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
        }

    }

    exit(0);

}


