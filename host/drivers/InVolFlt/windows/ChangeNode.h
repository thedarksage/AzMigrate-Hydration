#pragma once
/**
 * following type defines are used to define nodes in change list
 */

typedef enum _etChangeNode {
    ecChangeNodeUndefined = 0,
    ecChangeNodeDirtyBlock = 1,
    ecChangeNodeTags = 2,
    ecChangeNodeDataFile = 3,
} etChangeNode;

#define CHANGE_NODE_FLAGS_QUEUED_FOR_DATA_WRITE     0x0001
#define CHANGE_NODE_FLAGS_ERROR_IN_DATA_WRITE       0x0002

// TODO: If code at passive level and dispatch level have to be syncrhronized
// we will do the following. Currently we do not see any need for this.
// If node is to be owned exclusively, acquire the lock and check node is not acquired
// exclusively by any one else and set the exclusive flag. If it is already acquired
// set bSetEvent to true and release the lock, and wait on the NotificationEvent
// Thread that is releasing exclusive lock would set the NotificationEvent.
typedef struct _CHANGE_NODE {
    LIST_ENTRY      ListEntry;
    LIST_ENTRY      DataCaptureModeListEntry;
    LONG            lRefCount;
    LONG            ulFlags;
    etChangeNode    eChangeNode;
    KMUTEX          Mutex;
    ULONGLONG   NumChangeNodeInDataWOSAhead;
// Pointer to WorkQueueEntry for cleanup of ChangeNode
    PWORK_QUEUE_ENTRY pCleanupWorkQueueEntry;
// Extra Flag for status of WorkQueueEntry
    bool bCleanupWorkQueueRef;
//    KSPIN_LOCK      Lock;
//    KEVENT          NotificationEvent;
//    bool            bOwnedExclusively;
//    bool            bSetEvent;
    union {
        PVOID                   pContent;
        struct _KDIRTY_BLOCK   *DirtyBlock;
    };
} CHANGE_NODE, *PCHANGE_NODE;

typedef struct _CHANGE_LIST {
    LIST_ENTRY      Head;
    //This is the list used to store the dirty block when the bitmap is not 
    //opened and the globals are not updated
    LIST_ENTRY      TempQueueHead;
    LIST_ENTRY      DataCaptureModeHead;
    PCHANGE_NODE    CurrentNode;
    LONG            lDirtyBlocksInQueue;
    // ulTotalChangesPending includes changes that are pending commit.
    // This counter is decremented on commit of dirty block.
    ULONG           ulTotalChangesPending;
    ULONG           ulMetaDataChangesPending;
    ULONG           ulBitmapChangesPending;
    ULONG           ulDataChangesPending;
    ULONG           ulDataFilesPending;
    ULONGLONG       ullcbTotalChangesPending;
    ULONGLONG       ullcbMetaDataChangesPending;
    ULONGLONG       ullcbBitmapChangesPending;
    ULONGLONG       ullcbDataChangesPending;
} CHANGE_LIST, *PCHANGE_LIST;

typedef enum _etTagStateTriggerReason
{
    ecNotApplicable = 1,
    ecBitmapWrite,
    ecFilteringStopped,
    ecClearDiffs,
    ecNonPagedPoolLimitHitMDMode,
    ecChangesQueuedToTempQueue,
    ecRevokeTimeOut,
    ecRevokeCancelIrpRoutine,
    ecRevokeCommitIOCTL,
    ecRevokeLocalCrash,
    ecRevokeDistrubutedCrashCleanupTag,
    ecRevokeDistrubutedCrashInsertTag,
    ecRevokeDistrubutedCrashReleaseTag,
    ecRevokeAppTagInsertIOCTL,
    ecSplitIOFailed,                    // Linux: splitting large io failed
    ecOrphan                            // Linux: Orphan dropped on S2 exit
}etTagStateTriggerReason, *petTagStateTriggerReason;

void
InitializeChangeList(PCHANGE_LIST   pList);

void
DeductChangeCountersOnDataSource(PCHANGE_LIST pList, ULONG ulDataSource, ULONG ulChanges, ULONGLONG ullcbChanges);

void
AddChangeCountersOnDataSource(PCHANGE_LIST pList, ULONG ulDataSource, ULONG ulChanges, ULONGLONG ullcbChanges);

void
FreeChangeNodeList(PCHANGE_LIST pList, etTagStateTriggerReason tagStateReason);

PCHANGE_NODE
AllocateChangeNode();

PCHANGE_NODE
ReferenceChangeNode(PCHANGE_NODE Node);

void
DereferenceChangeNode(PCHANGE_NODE Node);

void
ChangeNodeCleanup(PCHANGE_NODE ChangeNode);

