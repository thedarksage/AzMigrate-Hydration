#pragma once
#include "BitmapAPI.h"

#define DEV_BITMAP_FLAGS_WAITING_FOR_SETBITS_WORKITEM_LIST_EMPTY_NOTIFICATION    0x0001
#define VOLUME_BITMAP_FLAGS_HAS_VOLUME_LETTER                                       0x0002

struct _DEV_CONTEXT;

typedef struct _DEV_BITMAP
{
    LIST_ENTRY      ListEntry;  // This is used to link all bitmaps to DriverContext
    LONG            lRefCount;
    ULONG           ulFlags;
    ULONG           ulReserved;
    etVBitmapState  eVBitmapState;
    // This is added and referenced with bitmap is opened.
    // This is removed and dereference with bitmap device is closed.
    _DEV_CONTEXT    *pDevContext;// It can be volume/disk
    FAST_MUTEX      Mutex;
    LIST_ENTRY      WorkItemList;
    KSPIN_LOCK      Lock;   // Spinlock is used to protect SetBitsWorkItemList.
                            // Entires into this list are inserted at dispatch level.
    LIST_ENTRY      SetBitsWorkItemList;
    KEVENT          SetBitsWorkItemListEmtpyNotification;
    WCHAR           DevID[DEVID_LENGTH]; //It is MBR/GUID for disk. GUID for Volume
    WCHAR           wDevNum[DEVICE_NUM_LENGTH];
    ULONG           SegmentCacheLimit;
    class BitmapAPI *pBitmapAPI;
} DEV_BITMAP, *PDEV_BITMAP;

typedef enum _etBitmapWorkItem
{
    ecBitmapWorkItemNotInitialized = 0,
    ecBitmapWorkItemStartRead = 1,
    ecBitmapWorkItemContinueRead = 2,
    ecBitmapWorkItemClearBits = 3,
    ecBitmapWorkItemSetBits = 4
} etBitmapWorkItem, *petBitmapWorkItem;

typedef struct _BITMAP_WORK_ITEM
{
    LIST_ENTRY              ListEntry;
    LONG                    lRefCount;
    ULONG                   ulChanges;
    ULONGLONG               ullCbChangeData;
    etBitmapWorkItem        eBitmapWorkItem;
    PDEV_BITMAP             pDevBitmap;
    BitRuns                 BitRuns;
    // PreAllocation of WorkQueueEntry
    PWORK_QUEUE_ENTRY       pWorkQueueEntryPreAlloc;
    // Extra Flag for status of WorkQueueEntry
    bool bBitMapWorkQueueRef;
} BITMAP_WORK_ITEM, *PBITMAP_WORK_ITEM;


BOOLEAN
QueueWorkerRoutineForBitmapWrite(
    _DEV_CONTEXT        *pDevContext,
    ULONG               ulNumDirtyChanges
    );

VOID
QueueWorkerRoutineForStartBitmapRead(PDEV_BITMAP  pDevBitmap);

VOID
QueueWorkerRoutineForContinueBitmapRead(PDEV_BITMAP  pDevBitmap);

void
RequestServiceThreadToOpenBitmap(_DEV_CONTEXT  *pDevContext);

_IRQL_requires_max_(PASSIVE_LEVEL)
_Ret_maybenull_
_Must_inspect_result_
PDEV_BITMAP
OpenBitmapFile(
    _In_ _Notnull_  PDEV_CONTEXT    pDevContext,
    _Outref_        NTSTATUS        &Status);

VOID
CloseBitmapFile(PDEV_BITMAP pDevBitmap, bool bClearBitmap, PDEV_CONTEXT pDevContext, tagLogRecoveryState recoveryState, bool bDeleteBitmap = false, ULONG ResyncErrorCode = 0, LONGLONG ResyncTimeStamp = 0);

VOID
ClearBitmapFile(PDEV_BITMAP pDevBitmap);

NTSTATUS
WaitForAllWritesToComplete(PDEV_BITMAP pDevBitmap, PLARGE_INTEGER pliTimeOut);


PDEV_BITMAP
AllocateDevBitmap();

PDEV_BITMAP
ReferenceDevBitmap(PDEV_BITMAP    pDevBitmap);

VOID
DereferenceDevBitmap(PDEV_BITMAP  pDevBitmap);


/*-----------------------------------------------------------------------------
 *  Functions related to Allocate, Reference and Dereference Bitmap Work Items
 *-----------------------------------------------------------------------------
 */
PBITMAP_WORK_ITEM
AllocateBitmapWorkItem(etBitmapWorkItem wItemType);
/*
VOID
ReferenceBitmapWorkItem(PBITMAP_WORK_ITEM pBitmapWorkItem);
*/

VOID
DereferenceBitmapWorkItem(PBITMAP_WORK_ITEM pBitmapWorkItem);

NTSTATUS
GetDevBitmapGranularity(PDEV_CONTEXT pDevContext, PULONG pulBitmapGranularity);


