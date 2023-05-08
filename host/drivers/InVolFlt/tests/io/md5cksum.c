#define _GNU_SOURCE
#define _FILE_OFFSET_BITS	64   // enable 64 bit file access
#include "libinclude.h"
#include "md5cksum.h"
#define SECTOR_SIZE			512
#define CRASH_RECOVERY_ERROR 1
//#define DEBUG_LOG

jobNode *jobHead = NULL;
int initSync = 0;
long long int progressOp = DEFAULT_PROGRESS_OP;

/* Mutex for syncronization */
pthread_mutex_t mutex;

/* Condition variable for sleep/wakeup */
pthread_cond_t condition;

/* job count */
unsigned int jobCount = 0;

long int dataBytes=DEFAULT_DATA_BYTES;
long long int maxcount = 0;
long long int offset = 0;
long long int nOperations = 0;
long long int firstErrorOffset = -1;
int killAll = 0;
long int timeLogOperations = DEFAULT_TIMELOG_OPERATIONS;
struct timeval start,end;

char *srcVol = DEFAULT_SOURCE_BLOCKDEVICE;
char *dstVol = NULL;

int SUCCESS = 0;
int FAILED = -1;

int insertJob(int flags)
{
	jobNode *newjob = NULL;
	int tOp = (int)flags;

	/* Initialize synchronization- mutex & condition variable */
	if (initSync == 0 )
	{
		pthread_mutex_init(&mutex,NULL);
		pthread_cond_init(&condition,NULL);
		initSync=1;
	}

	newjob = (jobNode*)malloc(sizeof(jobNode*));
	if ( newjob == NULL ){
		printf("insertJob: Memory allocation failure \n");
		return -1;
	}
	newjob->next = NULL;
	newjob->flags = tOp;

	pthread_mutex_lock(&mutex);
	if (jobHead == NULL) {
		jobHead = newjob; 
		/* Wake up consumer */
		pthread_cond_signal(&condition);
	}    
   else {
		jobNode *tmp=jobHead;
		while(tmp->next) tmp=tmp->next;
		tmp->next = newjob;
	}
	assert(jobCount >= 0);
	jobCount++;
	pthread_mutex_unlock(&mutex);
	
	return 0;
}

jobNode *consumeJob()
{
	jobNode *job = NULL;

	assert (initSync != 0 );

	pthread_mutex_lock(&mutex);
	if (jobHead != NULL)
	{
		job=jobHead;
		if (jobHead->next != NULL)
			jobHead = jobHead->next;
		else
			jobHead = NULL;
	}
	pthread_mutex_unlock(&mutex);
	return job;
}

int launchThreads(int nThreads)
{
	int returnCode = -1, t;
	pthread_t *threads = NULL;
	pthread_attr_t attr;
#ifdef DEBUG_LOG
	printf("In launchThreads: Create thread:%d\n", nThreads );
#endif
	/* Initialize synchronization- mutex & condition variable */
	if (initSync == 0)
	{
		pthread_mutex_init(&mutex,NULL);
		pthread_cond_init(&condition,NULL);
		initSync=1;
    }
	threads = (pthread_t*)malloc(sizeof(*threads)*nThreads);

	if (threads == NULL)
	{
			printf("ERROR in launchThreads - memory allocation failured\n");
			exit(-1);
    }
	/* Create threads in joinable state */
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

	/* Create threads */
	for(t=0; t<nThreads; t++) {
		returnCode = pthread_create(threads+t, NULL, execOperation, (void*)t);
		if (returnCode) {
			printf("ERROR - return code from pthread_create - %d\n", returnCode);
			exit(-1);
		}
 	}

	/* join threds */
	for(t=0; t<nThreads; t++) {
#ifdef DEBUG_LOG
		printf("In launchThreads: Join thread:%d\n", t);
#endif
		returnCode = pthread_join(threads[t], NULL);
	}
	initSync = 0;
	 /* Clean up actions */
	pthread_attr_destroy(&attr);
	pthread_mutex_destroy(&mutex);
	pthread_cond_destroy(&condition);
//	pthread_exit(NULL);
	if (threads)
		free(threads);
    return returnCode;
}

void printtime(int tid, long long nOp, int wantCurrentTime)
{
	long hours, mins;
	long long secs, msecs;
	if (wantCurrentTime)
   {
		struct timeval current;
		gettimeofday(&current, NULL);
		msecs=((long long)current.tv_sec*1000)+((long long)current.tv_usec/1000);
//		hours = msecs/ (1000*3600);
//		mins = msecs/ (1000*60) - hours*60;
//		secs = msecs/ (1000) - hours*60- mins*60;
//		msecs = msecs/ (1000) - hours*60- mins*60- secs*1000;
		printf("Thread:%d current time in milliseconds %lld current operations:%lld\n",
				tid, msecs, nOp);
		system("date -u \"+%Y/%m/%d %T\"");
   }
   else 
   {
    	gettimeofday(&end, NULL);	
		long long stime=start.tv_sec*1000+start.tv_usec/1000;
		long long etime=end.tv_sec*1000+end.tv_usec/1000;
		hours=(etime-stime)/(1000*3600);
		mins=(etime-stime)/(1000*60)-hours*60;
		secs=(etime-stime)/1000-hours*3600-mins*60;
		msecs=(etime-stime)-hours*3600*1000-mins*60*1000-secs*1000;
		printf("\n\nTotal time for accomplishing the task: %ld hours %ld mins %lld secs %lld msecs  Total operations:%lld\n\n",
				hours, mins, secs, msecs, nOp);
   }
	
}

void usage()
{
		printf("Usage:\n");
		printf("md5 -d blockdevice [ -s blockdevice] [ -w ] [ -o offset ] [ -x nOperations ] [ -v ] [ -i iosize ] [ -r ] [ -t threads ] [ -l loperations ]\n");
		printf("-d  Destination block device for checksum verification\n");
		printf("-s  Source block device for generating checksum on target\n");
		printf("-o  Start byte offset on destination device for generating checksum\n");
		printf("-x  Number of iosize operations on destination\n");
		printf("-w  Write checksum operation\n");
		printf("-v  Verify checksum operation\n");
		printf("-i  IO size for checksum verification in bytes aligned to sector size\n");
		printf("-r  Random IO of iosize size\n");
		printf("-t  Multiple threads to execute same operation in parallel\n");
		printf("-p  Print/Display progress of the tool after every nOperations\n\n");
		printf("-l  Log current time in milliseconds after every loperations\n\n");
		printf("Default: Sequential verify operation, source block device = %s\n", DEFAULT_SOURCE_BLOCKDEVICE);
		printf("iosize =  %d Bytes (12 K) -- aligned to sector size (%d byte) boundary\n",
				DEFAULT_DATA_BYTES+DIGEST_BYTES, SECTOR_SIZE);
		printf("random IO is off by default\n");
		printf("threads = 1 , Log progress after %lld IOs of iosize\n", progressOp);
}
int main (int argc, char *argv[])
{
	int fdSrc=-1, fdDst=-1,status=-1, z, threads=1, writeVerify= 0;
	unsigned int flags=0;
	flags = SEQUENTIAL_OP;

#ifdef DEBUG_LOG
	printf("\nExecuting checksum tool- Destination device:%s Source device:%s Flags:%d iosize:%lu threads:%d write:%d verify:%d random:%d sequential:%d progressLog:%lld\n",
	        dstVol, srcVol, flags, dataBytes+DIGEST_BYTES, threads,
		(flags&WRITECHECKSUM_OP)==WRITECHECKSUM_OP, (flags&VERIFY_OP)==VERIFY_OP,
		(flags&RANDOM_OP)==RANDOM_OP, (flags&SEQUENTIAL_OP)==SEQUENTIAL_OP, progressOp);
#endif
	while ((z=getopt(argc, argv, "d:s:o:x:wvi:rt:p:l:"))!= -1)
	{
		switch(z)
		{
			case 'd':
					dstVol = (unsigned char*)optarg;	
				break;
			case 's':
					srcVol = (unsigned char*)optarg;	
				break;
			case 'o':
					offset = strtoll(optarg,NULL,10);
					if (offset<0)
					{
						printf("Offset:%lld error - invalid offset value!\n", offset);
						return(status);
					}
					if (offset%SECTOR_SIZE != 0)
					{
						printf("Offset:%lld error - value is not multiple of sector size:%d!\n", offset, SECTOR_SIZE);
						return(status);
					}
				break;
			case 'x':
					nOperations = strtoll(optarg,NULL,10);
					if (nOperations <= 0)
					{
						printf("Invalid value of total  operations:%lld of size iosize!\n", nOperations);
						return(status);
					}
				break;
			case 'w':
					if (flags & VERIFY_OP)
					{
						writeVerify = 1;
						flags &= ~(unsigned int)VERIFY_OP;
					}
					flags |= WRITECHECKSUM_OP;
				break;
			case 'v':
					if (flags & WRITECHECKSUM_OP)
					{
						writeVerify = 1;
					}
					else
						flags |= VERIFY_OP;
				break;
			case 'i':
					dataBytes = strtoul(optarg,NULL,10);
					if ((dataBytes % 512) != 0)
					{
						printf("iosize:%ld error - iosize bytes is not sector aligned!\n", dataBytes);
						return(status);
					}
					dataBytes-=16;
				break;
			case 'r':
					flags |= RANDOM_OP;
					flags &= ~SEQUENTIAL_OP;
				break;
			case 't':
					threads = strtoul(optarg,NULL,10);
					if (!(threads >= 0 && threads < MAX_NUM_THREADS))
					{
						printf("Invalid threads:%d value\n\n", threads);
						usage();
						return(status);
					}
				break;
			case 'p':
					progressOp = strtoll(optarg,NULL,10);
					flags |= PROGRESS_OP;
				break;
			case 'l':
					flags |= TIME_IN_MSEC_OP;
					timeLogOperations = strtol(optarg,NULL,10);
					if (!(timeLogOperations >= 0))
					{
						printf("Invalid threads:%ld value\n\n", timeLogOperations);
						usage();
						return(status);
					}
				break;
			case '?':
					usage();
					return(status);
				break;
			default: printf("\n Invalid input. Check usage for more details\n"); return(status);
		}
	}

	if (!((flags & WRITECHECKSUM_OP) || (flags & VERIFY_OP)))
	{
		flags |= VERIFY_OP;
	}

	if (!srcVol || !dstVol)
	{
		usage();
		return(status);
	}

	if ( (flags & RANDOM_OP) || (flags & VERIFY_OP) != VERIFY_OP) 
	{
		fdSrc = open(srcVol, O_RDONLY);
		if (fdSrc == -1 )
		{
			printf(" Error occurred while trying to open source blockdevice:%s\n", srcVol);
			return(status);
		}
		close(fdSrc);
	}

	fdDst = open(dstVol,O_RDWR|O_DIRECT);
	if (fdDst == -1)
	{
		printf("Error occurred while trying to open destination  device:%s\n", dstVol);
		return(status);
	}
	maxcount = lseek(fdDst, 0, SEEK_END);
	maxcount = maxcount/(dataBytes+DIGEST_BYTES);
	close(fdDst);

	if ((offset%(dataBytes+DIGEST_BYTES))!=0)
	{
		printf("Invalid offset value:%lld on device:%s of size:%lld! offset is not aligned to iosize:%ld\n",
				offset, dstVol, maxcount*(dataBytes+DIGEST_BYTES), dataBytes+DIGEST_BYTES);
		return(status);
	}

	if (!((offset/(dataBytes+DIGEST_BYTES) >= 0) && (offset/(dataBytes+DIGEST_BYTES) < maxcount)))
	{
		printf("Invalid offset value:%lld on device:%s of size:%lld. Value is out of bound.\n",
				offset, dstVol, maxcount*(dataBytes+DIGEST_BYTES));
		return(status);
	}
	if (nOperations == 0)
		nOperations = maxcount;

#ifdef DEBUG_LOG
	printf(" opened blockdevice:%s\n", dstVol);
#endif
    if (progressOp > maxcount) progressOp = DEFAULT_PROGRESS_OP;
	printf("\nExecuting checksum tool- Destination device:%s Source device:%s Flags:%d iosize:%lu threads:%d write:%d verify:%d random:%d sequential:%d maxcount:%lld progressLog:%lld writeVerify:%d timeLogOperations:%ld\n",
	        dstVol, srcVol, flags, dataBytes+DIGEST_BYTES, threads,
		(flags&WRITECHECKSUM_OP)==WRITECHECKSUM_OP, (flags&VERIFY_OP)==VERIFY_OP,
		(flags&RANDOM_OP)==RANDOM_OP, (flags&SEQUENTIAL_OP)==SEQUENTIAL_OP,
		maxcount*(dataBytes+DIGEST_BYTES), progressOp, writeVerify, timeLogOperations);

    gettimeofday(&start, NULL);	

	/* Insert jobs into a linekd list */
	for (z=0; z<threads; z++)
	{	
		insertJob(flags);
	}

	/* Launch consumer threads to drain operations from the linked list */
	status = launchThreads(threads);
    if (writeVerify)
    {
#ifdef DEBUG_LOG
		printf("\n\n Starting verify operation now:%s\n", dstVol);
#endif
		flags &= ~WRITECHECKSUM_OP;
		flags |= VERIFY_OP;
#ifdef DEBUG_LOG
	printf("\nExecuting checksum tool- Destination device:%s Source device:%s Flags:%d iosize:%lu threads:%d write:%d verify:%d random:%d sequential:%d maxcount:%lld progressLog:%lld writeVerify:%d\n\n\n",
	        dstVol, srcVol, flags, dataBytes+DIGEST_BYTES, threads,
		(flags&WRITECHECKSUM_OP)==WRITECHECKSUM_OP, (flags&VERIFY_OP)==VERIFY_OP,
		(flags&RANDOM_OP)==RANDOM_OP, (flags&SEQUENTIAL_OP)==SEQUENTIAL_OP,
		maxcount*(dataBytes+DIGEST_BYTES), progressOp, writeVerify);
#endif

		/* Insert jobs into a linekd list */
		for (z=0; z<threads; z++)
		{	
			insertJob(flags);
		}
#ifdef DEBUG_LOG
		printf("\n\n Starting verify operation now:%s\n", dstVol);
#endif
		/* Launch consumer threads to drain operations from the linked list */
		status = launchThreads(threads);
    }
	printtime(0,nOperations,0);
	if (killAll) {
		 status = -1;
	}
	else {
		if (flags & VERIFY_OP)
			printf("\nDATA CONSISTENCY VERIFICATION TEST IS SUCCESSFUL.\n");
	}

	return (status);
}

int doIO(int fd, const char *buf, size_t size, int readOperation)
{
	int nio;
	if (readOperation)
		nio = read(fd, (void *)buf, size);
	else
		nio = write(fd, (void *)buf, size);
	return nio;
}

void *execOperation(void *threadid)
{
	int fdDst = -1, fdSrc = -1;
    int *status = &FAILED;
	long long int count=0, byteCount = 0;
	unsigned char *buffer = NULL, *orgDigest = NULL;
	int flags= 0, tid = (int)threadid;	
	long rseed = 512;
	long long pos=0;
	jobNode *node = NULL;	
	long long int currentOffset = -1;
#ifdef DEBUG_LOG
	printf("In execOperation: executing thread:%d\n", tid );
#endif
	node = consumeJob();
	if (node == NULL)
	{
		printf("Thread:%d - No jobs to execute! \n", tid);
		return(status);
	}
	flags = node->flags;
	free(node);

	buffer = (unsigned char*)valloc(dataBytes+DIGEST_BYTES);
	if (buffer == NULL)
	{
		printf(" Couldn't satisfy memory alloc request\n");
		return(status);
	}

	if ((flags & VERIFY_OP) != VERIFY_OP) 
	{
		fdSrc = open(srcVol, O_RDONLY);
		if (fdSrc == -1)
		{
			printf(" Error occurred while opening blockdevice:%s\n", srcVol);
			return(status);
		}
#ifdef DEBUG_LOG
		printf(" opened blockdevice:%s fdSrc:%d\n", srcVol, fdSrc);
#endif
	}
	else
	{
		orgDigest = (unsigned char*)malloc(DIGEST_BYTES);
		if (orgDigest == NULL)
		{
			printf(" Couldn't satisfy memory alloc request\n");
			goto Cleanup;
		}
	}
	if ((fdDst = open(dstVol, O_RDWR|O_DIRECT))<0)
	{
		printf("Error - Cannot open device:%s\n", dstVol);
		goto Cleanup;
	}
	
#ifdef DEBUG_LOG
	printf(" opened blockdevice:%s fdDst:%d\n", dstVol,fdDst);
#endif

	assert(fdDst!=-1);

	if(rseed<-2 || rseed>32767 )
	{
		printf("Seed out of range.\n");
		exit(1);
	}
	if(rseed>=0)
		srand(rseed);
	
	currentOffset = offset;
	byteCount = lseek(fdDst, offset ,SEEK_SET); 
	assert(byteCount == offset);
	while(count<maxcount && count<nOperations && !killAll)
	{
		if (flags & VERIFY_OP)
		{
			byteCount = doIO(fdDst, buffer, dataBytes+DIGEST_BYTES, 1);
//			byteCount = read(fdDst, buffer, dataBytes+DIGEST_BYTES);
			if (byteCount!=dataBytes+DIGEST_BYTES)
			{
				printf(" Verify flags - Couldn't satisfy read request:%s byteCount:%lld data:%lu\n",
						 dstVol, byteCount, dataBytes+DIGEST_BYTES);
				goto Cleanup;
			}
			memcpy(orgDigest, buffer+dataBytes, DIGEST_BYTES);
			memset(buffer+dataBytes, 0, DIGEST_BYTES);
  			MDString (buffer, dataBytes);
			if (memcmp((void*)(buffer+dataBytes), (void*)orgDigest, DIGEST_BYTES)!=0)
			{
				printf("Verify flags - count:%lld offset:%lld Checksum %s :",
						count, currentOffset,"not matched");
				MDPrint(orgDigest);
				printf("<-->");
				MDPrint(buffer+dataBytes);
				printf("\n");
				if (firstErrorOffset == -1)
				{
					firstErrorOffset = currentOffset;
					printf("Verify flags - first failure count:%lld offset:%lld\n",
						count, firstErrorOffset);
				}
				else
				if (firstErrorOffset != currentOffset)
				{
					printf("\nDATA INCONSISTENCY DETECTED offset:%llu bytes dataBytes:%ld\n\n",
						currentOffset, dataBytes + DIGEST_BYTES);
					killAll = 1;
					goto Cleanup;
				}
			}
#ifdef DEBUG_LOG
			printf("count:%lld offset:%lld data:%d digest:%d Checksum\n",
					count, count*(dataBytes+DIGEST_BYTES), dataBytes,DIGEST_BYTES);
#endif
		}
		else if (flags & WRITECHECKSUM_OP)
		{
			byteCount = doIO(fdSrc, buffer, dataBytes, 1);
//			byteCount = read(fdSrc, buffer, dataBytes);
			if (byteCount != dataBytes)
			{
				printf("Couldn't satisfy read request:%s byteCount:%lld dataBytes:%ld\n",
						dstVol, byteCount, dataBytes);
				goto Cleanup;
			}
  			MDString (buffer, dataBytes);
			byteCount = doIO(fdDst, buffer, dataBytes+DIGEST_BYTES, 0);
//			byteCount = write(fdDst, buffer, dataBytes+DIGEST_BYTES);
			if (byteCount!=(dataBytes+DIGEST_BYTES))
			{
				printf("Couldn't satisfy read request:%s byteCount:%lld data:%ld\n",
						dstVol, byteCount, dataBytes+DIGEST_BYTES);
				goto Cleanup;
			}
#ifdef DEBUG_LOG
			printf("count:%lld offset:%lld data:%d digest:%d Checksum\n",
					count, count*(dataBytes+DIGEST_BYTES), dataBytes,DIGEST_BYTES);
			MDPrint(buffer+DIGEST_BYTES);
			printf("\n");
#endif
		}

		if (flags & SEQUENTIAL_OP)
		{
			count++;
			currentOffset += (dataBytes+DIGEST_BYTES);
			assert(currentOffset <= (maxcount*(dataBytes+DIGEST_BYTES))); 
#ifdef DEBUG_LOG
			printf("Sequential IO count:%lld offset:%lld \n",
					count, count*(dataBytes+DIGEST_BYTES));
#endif
		}
		else
		if (flags & RANDOM_OP)
		{
			float tmp = maxcount;
			// j = 1 + (int) (10.0 * (rand() / (RAND_MAX + 1.0)));
			pos = 1 + (int) (tmp * (rand() / (RAND_MAX + 1.0)));
			pos = pos%(maxcount);
			count++;
			byteCount = lseek(fdDst,pos*(dataBytes+DIGEST_BYTES),SEEK_SET); 
			currentOffset = pos*(dataBytes+DIGEST_BYTES);
			assert(currentOffset <= (maxcount*(dataBytes+DIGEST_BYTES))); 
#ifdef DEBUG_LOG
			printf("Random IO count:%lld maxcount:%lld pos:%lld byteCount:%lld\n",
					count, maxcount, pos*(dataBytes+DIGEST_BYTES), byteCount);
#endif
		}
		if ((flags & PROGRESS_OP) && ((count%progressOp)==0))
			printf("Thread:%d Operation:%s %s count:%lld offset:%lld data:%ld checksum:%d\n",
					tid,(flags & WRITECHECKSUM_OP)?"WRITE":"VERIFY",
					(flags & SEQUENTIAL_OP)?"SEQUENTIAL":"RANDOM",
					count, count*(dataBytes+DIGEST_BYTES), dataBytes, DIGEST_BYTES);
		if ((flags & TIME_IN_MSEC_OP) && ((count%timeLogOperations)==0))
		{
			sleep(1);
			printtime(tid, count, 1);
		}
	}
	status = &SUCCESS;

Cleanup:	
	if (fdDst != -1)
		close(fdDst);
	if (fdSrc != -1) 
		close(fdSrc);
#ifdef DEBUG_LOG
	printf("Closed devices\n");
#endif
	if (buffer)
		free(buffer);
	if (orgDigest)
		free(orgDigest);

	return(status);
}
