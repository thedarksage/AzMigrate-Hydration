#ifndef _INMAGE_VOLUME_BITMAP_H__
#define _INMAGE_VOLUME_BITMAP_H__
#include "BitmapAPI.h"

#define VOLUME_BITMAP_FLAGS_WAITING_FOR_SETBITS_WORKITEM_LIST_EMPTY_NOTIFICATION    0x0001
#define VOLUME_BITMAP_FLAGS_HAS_VOLUME_LETTER                                       0x0002

struct _VOLUME_CONTEXT;

typedef struct _VOLUME_BITMAP
{
    LIST_ENTRY      ListEntry;  // This is used to link all bitmaps to DriverContext
    LONG            lRefCount;
    ULONG           ulFlags;
    ULONG           ulReserved;
    etVBitmapState  eVBitmapState;
    // This is added and referenced with bitmap is opened.
    // This is removed and dereference with bitmap volume is closed.
    _VOLUME_CONTEXT *pVolumeContext;
    FAST_MUTEX      Mutex;
    LIST_ENTRY      WorkItemList;
    KSPIN_LOCK      Lock;   // Spinlock is used to protect SetBitsWorkItemList.
                            // Entires into this list are inserted at dispatch level.
    LIST_ENTRY      SetBitsWorkItemList;
    KEVENT          SetBitsWorkItemListEmtpyNotification;
    WCHAR           VolumeGUID[GUID_SIZE_IN_CHARS + 1];
    WCHAR           VolumeLetter[VOLUME_LETTER_IN_CHARS + 1];
    ULONG           SegmentCacheLimit;
    class BitmapAPI *pBitmapAPI;
} VOLUME_BITMAP, *PVOLUME_BITMAP;

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
    PVOLUME_BITMAP          pVolumeBitmap;
    BitRuns                 BitRuns;
    // PreAllocation of WorkQueueEntry
    PWORK_QUEUE_ENTRY       pWorkQueueEntryPreAlloc;
    // Extra Flag for status of WorkQueueEntry
    bool bBitMapWorkQueueRef;
} BITMAP_WORK_ITEM, *PBITMAP_WORK_ITEM;


BOOLEAN
QueueWorkerRoutineForBitmapWrite(
    _VOLUME_CONTEXT     *pVolumeContext,
    ULONG               ulNumDirtyChanges
    );

VOID
QueueWorkerRoutineForStartBitmapRead(PVOLUME_BITMAP  pVolumeBitmap);

VOID
QueueWorkerRoutineForContinueBitmapRead(PVOLUME_BITMAP  pVolumeBitmap);

void
RequestServiceThreadToOpenBitmap(_VOLUME_CONTEXT  *pVolumeContext);

PVOLUME_BITMAP
OpenBitmapFile(struct _VOLUME_CONTEXT   *pVolumeContext, NTSTATUS &Status);

VOID
CloseBitmapFile(PVOLUME_BITMAP pVolumeBitmap, bool bClearBitmap, BitmapPersistentValues &BitmapPersistentValue, bool bDeleteBitmap = false);

VOID
ClearBitmapFile(PVOLUME_BITMAP pVolumeBitmap);

NTSTATUS
WaitForAllWritesToComplete(PVOLUME_BITMAP pVolumeBitmap, PLARGE_INTEGER pliTimeOut);


PVOLUME_BITMAP
AllocateVolumeBitmap();

PVOLUME_BITMAP
ReferenceVolumeBitmap(PVOLUME_BITMAP    pVolumeBitmap);

VOID
DereferenceVolumeBitmap(PVOLUME_BITMAP  pVolumeBitmap);


/*-----------------------------------------------------------------------------
 *  Functions related to Allocate, Reference and Dereference Bitmap Work Items
 *-----------------------------------------------------------------------------
 */
PBITMAP_WORK_ITEM
AllocateBitmapWorkItem(etBitmapWorkItem wItemType);

VOID
ReferenceBitmapWorkItem(PBITMAP_WORK_ITEM pBitmapWorkItem);

VOID
DereferenceBitmapWorkItem(PBITMAP_WORK_ITEM pBitmapWorkItem);

NTSTATUS
UpdateGlobalWithPersistentValuesReadFromBitmap(BOOLEAN bVolumeInSync, BitmapPersistentValues &BitmapPersistentValue) ;


#endif // _INMAGE_BITMAP_H__