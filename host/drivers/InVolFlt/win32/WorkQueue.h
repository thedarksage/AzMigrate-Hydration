#ifndef _INMAGE_WORK_QUEUE_H_
#define _INMAGE_WORK_QUEUE_H_


struct _WORK_QUEUE_ENTRY;
struct _WORK_QUEUE;

#define WQ_FLAGS_THREAD_SHUTDOWN        0x00000001

typedef struct _WORK_QUEUE
{
    LIST_ENTRY  WorkerQueueHead;
    KSPIN_LOCK  Lock;
    KEVENT      WakeEvent;
    KEVENT      ShutdownEvent;
    PKSTART_ROUTINE WorkerThreadRoutine;
    PKTHREAD    ThreadObject;
    ULONG       ulFlags;
    ULONG       ulThreadPriority;
} WORK_QUEUE, *PWORK_QUEUE;

typedef VOID (*WORKER_ROUTINE_TYPE)(struct _WORK_QUEUE_ENTRY *pWorkItem);

#define WQE_FLAGS_THREAD_SHUTDOWN       0x00000001

typedef enum _etWorkItem
{
    ecWorkItemUnInitialized = 0,
    ecWorkItemOpenBitmap = 1,
    ecWorkItemBitmapWrite = 2,
    ecWorkItemStartBitmapRead = 3,
    ecWorkItemContinueBitmapRead = 4,
    ecWorkItemForVolumeUnload = 5,
} etWorkItem, *petWorkItem;

typedef struct _WORK_QUEUE_ENTRY
{
    LIST_ENTRY  ListEntry;
    LONG        lRefCount;
    ULONG       ulFlags;
    PVOID       Context;
    PKEVENT     pNotificationEvent;
    etWorkItem  eWorkItem;
    ULONG       ulInfo1;
    ULONG       ulInfo2;
    WORKER_ROUTINE_TYPE WorkerRoutine;
} WORK_QUEUE_ENTRY, *PWORK_QUEUE_ENTRY;

NTSTATUS
InitializeWorkQueue(PWORK_QUEUE pWorkQueue, PKSTART_ROUTINE WorkerThreadRoutine, ULONG ulThreadPriority);

NTSTATUS
SetWorkQueueThreadPriority(PWORK_QUEUE pWorkQueue, ULONG ulPriority);

NTSTATUS
CleanWorkQueue(PWORK_QUEUE pWorkQueue);

NTSTATUS
AddItemToWorkQueue(PWORK_QUEUE pWorkQueue, PWORK_QUEUE_ENTRY pEntry);

PWORK_QUEUE_ENTRY
AllocateWorkQueueEntry();

VOID
InitializeWorkQueueEntry(PWORK_QUEUE_ENTRY pWorkQueueEntry);

VOID
ReferenceWorkQueueEntry(PWORK_QUEUE_ENTRY   pWorkQueueEntry);

VOID
DereferenceWorkQueueEntry(PWORK_QUEUE_ENTRY pWorkQueueEntry);

#endif //_INMAGE_WORK_QUEUE_H_