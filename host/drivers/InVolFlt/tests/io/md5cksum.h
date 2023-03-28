#ifndef __THREADS_MD5__
#define __THREADS_MD5__

#include <pthread.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include "mddriver.h"

#define DEFAULT_SOURCE_BLOCKDEVICE "/dev/urandom"
#define DEFAULT_NUM_THREADS 1
#define MAX_NUM_THREADS 20
#define NUM_MEMALLOC_RETRY 1000
#define DEFAULT_PROGRESS_OP 16384
#define DEFAULT_TIMELOG_OPERATIONS	128
#define MAX_SLEEP_SECS	60

#define DIGEST_BYTES 		16
#define BLOCKSIZE_BYTES		4096
#define	DEFAULT_DATA_BYTES 	((BLOCKSIZE_BYTES*3)-16)

enum operations {
	WRITECHECKSUM_OP=0x1,
	SEQUENTIAL_OP=0x2,
	RANDOM_OP=0x4,
	VERIFY_OP=0x8,
	PROGRESS_OP=0x10,
	TIME_IN_MSEC_OP=0x20
};

#define NUM_MEMALLOC_RETRY 1000

/* Linkedlist to maintain the jobs */
typedef struct job {
 struct job *next;
 unsigned int flags;
} jobNode;

int inserJob(int flags);
jobNode* consumeJob();
int launchThreads(int nThreads);
void *execOperation(void *threadid);
void printtime(int tid, long long nOp, int wantCurrentTime);

#endif /* __THREADS_MD5__ */
