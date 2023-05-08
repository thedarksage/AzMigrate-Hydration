#ifndef _VV_THREAD_POOL_H_
#define _VV_THREAD_POOL_H_

#include "WinVsnapKernelHelpers.h"
#include "VVCommon.h"
//
#define FLAG_READ_INTO_TEMPORARY_BUFFER 0x10000000

#define TASK_BTREE_LOOKUP  		1
#define TASK_READ_RETENTION  	2
#define TASK_END_IO   		    3
#define TASK_WRITE   		    4
#define TASK_RETENTION_IO  		5
#define TASK_START_QUEUED_IO 	6
#define TASK_DELETE_VSNAP  		7
#define TASK_UPDATE_VSNAP  		8
#define TASK_READ_VVOLUME       9
#define TASK_WRITE_VVOLUME       10
#define TASK_KILL_THREAD		255


typedef struct _THREAD_POOL {
	LIST_ENTRY          GlobalListHead;	// list of tasks - maintain a queue 
    FAST_MUTEX          hMutex; 		// for task list operations 
	KEVENT              TaskNotifyEvent;		// for threads to wait on 
	ULONGLONG           ullTaskCount;		// num of tasks		
} THREAD_POOL,* PTHREAD_POOL ;

typedef struct _THREAD_POOL_INFO {
	VsnapRWLock          *rwLock;
	LONG                  ulCountThreads;
    PTHREAD_POOL	      pThreadPool;
} THREAD_POOL_INFO, * PTHREAD_POOL_INFO;	
	

typedef struct _VSNAP_TASK {
	LIST_ENTRY	            TaskListEntry;	// list of tasks 
    USHORT                  usTaskId;
	void 			        *TaskData;	// data required for the task 
    struct VsnapInfoTag     *Vsnap;
} VSNAP_TASK, *PVSNAP_TASK;

typedef struct _VSNAP_READ_RETENTION {
    VsnapKeyData            VsnapRetdata;
    PVOID                   pBuffer;
}VSNAP_READ_RETENTION, *PVSNAP_READ_RETENTION;



#define ADD_TASK_TO_POOL(POOL, TASK) \
        {\
		ExAcquireFastMutex(&(POOL)->hMutex); \
		InsertTailList( &(POOL)->GlobalListHead, &TASK->TaskListEntry); \
		(POOL)->ullTaskCount++; \
		ExReleaseFastMutex(&(POOL)->hMutex);\
		KeSetEvent(&(POOL)->TaskNotifyEvent, EVENT_INCREMENT, FALSE);\
        }

#define ADD_TASK_TO_POOL_HEAD(POOL, TASK)\
        {\
		ExAcquireFastMutex(&(POOL)->hMutex); \
		InsertHeadList( &(POOL)->GlobalListHead, &TASK->TaskListEntry); \
		(POOL)->ullTaskCount++; \
		ExReleaseFastMutex(&(POOL)->hMutex);\
		KeSetEvent(&(POOL)->TaskNotifyEvent, EVENT_INCREMENT, FALSE);\
        }


#define GET_TASK_FROM_POOL(POOL, TASK) \
        {\
		ExAcquireFastMutex(&(POOL)->hMutex); \
		if ( !(POOL)->ullTaskCount ) {\
			TASK = NULL; \
		} else {\
			TASK = (VSNAP_TASK *)RemoveHeadList(&(POOL)->GlobalListHead); \
			(POOL)->ullTaskCount--; \
		} \
		ExReleaseFastMutex(&(POOL)->hMutex);\
        }

#define	WAIT_FOR_NEXT_TASK(POOL)	\
	KeWaitForSingleObject(&(POOL)->TaskNotifyEvent, Executive, KernelMode, FALSE, NULL);


int InitThreadPool(void);
bool UnInitThreadPool(void);
void KillThread(THREAD_POOL_INFO *);
void DestroyThread(void);
void VsnapIOWorkerThread(PVOID Buffer) ;
void VSnapVolumeWriteData(VSNAP_TASK  *task);
void VsnapStartQueuedIo(VSNAP_TASK  *task);
void VsnapVolumeBtreeLookup(VSNAP_TASK  *task);
void VsnapVolumeEndIO(VSNAP_TASK  *task);
void VsnapVolumeRetentionFile(VSNAP_TASK  *task);
int  VsnapVolumeCreateRetentionTask(VSNAP_TASK  *task);
void VsnapVolumeReadRetentionFile(VSNAP_TASK *task);

void
VsnapUpdateWorkerThread(PVOID Buffer) ;
bool VsnapQueueUpdateInQueue(struct _UPDATE_VSNAP_VOLUME_INPUT *UpdateInfo, PIRP   Irp, bool *IsIOComplete);
void VsnapUpdateMaps(VSNAP_TASK *task);
extern bool IsSingleListEmpty(PSINGLE_LIST_ENTRY);
extern PSINGLE_LIST_ENTRY InPopEntryList(PSINGLE_LIST_ENTRY);
extern void InPushEntryList(PSINGLE_LIST_ENTRY, PSINGLE_LIST_ENTRY);

void
VVolumeIOWorkerThread(PVOID Buffer) ;
void VVolumeWriteData(VSNAP_TASK  *task);
void VVolumeReadData(VSNAP_TASK  *task);
void VVolumeEndIO(VSNAP_TASK  *task);
void VVolumeStartQueuedIo(VSNAP_TASK  *task);
bool ReadFromMultiPartSparseFile(void **Handle, void *Buffer, ULONGLONG Offset, ULONG Length, int *BytesRead, ULONGLONG ChunkSize, NTSTATUS *ErrStatus);

//#include "VVCmdQueue.h"
#endif