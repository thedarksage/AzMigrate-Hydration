#pragma once

#include "Common.h"
#include "global.h"
#include "FltContext.h"
#include "DevContext.h"

struct _DEV_CONTEXT;

#define DISK_CXSESSION_DIFFTHROUGHPUT_THRESOLD              (8*1024*1024)

#define DEFAULT_CXSESSION_START_ID                          10000
#define DEFAULT_CXSESSION_USER_TRANSID                      100

#define DEFAULT_PREMISSIBLE_TIMEJMP_FWD_MS                  (180  * 1000)      // 3 mins fwd jump 
#define DEFAULT_PREMISSIBLE_TIMEJMP_BWD_MS                  (3600 * 1000)       // 1 hour bck jump

typedef enum {
    ProductIssueS2Quit = 0,
    ProductIssueSvAgentsQuit
}ProductIssue;

// CX Session state
// It has 4 states
//     |                  |                   |                      |
//  CxSession          CxSession            CxSession            CxSession
//   NotStarted          InProgress    ->  ->Over   -> ->  ->      Closes
//      |                  .                                        |
//      |<      <    <     .<------<---<-------<-------<------<---< |
typedef enum _CxSessionState{
    CxSessionNotStarted,
    CxSessionInProgress,
    CxSessionOver,
    CxSessionInvalid
}CxSessionState;

// CX session for a given device
typedef struct _DEVICE_CX_SESSION {
    BOOLEAN         hasCxSession;
    ULONGLONG       ullSessionId;
    ULONGLONG       ullFlags;                           // Flags
    ULONGLONG       ChurnBucketsMBps[DEFAULT_NR_CHURN_BUCKETS]; // Peak churn count in each buckets
    ULONGLONG       ExcessChurnBucketsMBps[DEFAULT_NR_CHURN_BUCKETS]; // Excess Churn buckets

    ULONGLONG       FirstChurnTSBuckets[DEFAULT_NR_CHURN_BUCKETS];
    ULONGLONG       LastChurnTSBuckets[DEFAULT_NR_CHURN_BUCKETS];

    ULONGLONG       ullChurnInBytesPerSecond;
    ULONGLONG       ullCxStartTS; // Cx Start Time
    ULONGLONG       ullMaxChurnThroughputTS; // Time when maximum churn throughput was pending
    LARGE_INTEGER   liFirstNwFailureTS; // First NW failure timestamp
    LARGE_INTEGER   liLastNwFailureTS; // Last NW failure timestamp
    LARGE_INTEGER   liFirstPeakChurnTS; // First Peak Churn Timestamp
    LARGE_INTEGER   liLastPeakChurnTS; // Last Peak churn Timestamp
    LARGE_INTEGER   liCxEndTS; // Cx session end timestamp.

    ULONGLONG       ullLastNWErrorCode; // Last NW error code seen
    ULONGLONG       ullMaximumPeakChurnInBytes; // Maximum Peak churn seen in current session
    ULONGLONG       ullDiffChurnThroughputInBytes; // Last diff churn pending when cx session ended.
    ULONGLONG       ullMaxDiffChurnThroughputInBytes; // Maximum diff churn seen in current session
    ULONGLONG       ullTotalNWErrors; // Total NW errors seen in current session
    ULONGLONG       ullTotalExcessChurnInBytes; // Total Excess churn seen in current session
    ULARGE_INTEGER  uliIOTimeinSec;
    ULONGLONG       ullMaxS2LatencyInMS;
}DEVICE_CX_SESSION, *PDEVICE_CX_SESSION;

// CX session for a given device
typedef struct _VM_CX_SESSION_DATA{
    CxSessionState  cxState;
    ULONGLONG       ullFlags;
    ULARGE_INTEGER  ulTransactionID;
    ULONGLONG       ChurnBucketsMBps[DEFAULT_NR_CHURN_BUCKETS];
    ULONGLONG       ExcessChurnBucketsMBps[DEFAULT_NR_CHURN_BUCKETS];

    ULONGLONG       FirstChurnTSBuckets[DEFAULT_NR_CHURN_BUCKETS];
    ULONGLONG       LastChurnTSBuckets[DEFAULT_NR_CHURN_BUCKETS];

    ULONGLONG       ullCxStartTS;
    ULONGLONG       firstPeakChurnTS;
    ULONGLONG       lastPeakChurnTS;
    ULONGLONG       ullMaxChurnThroughputTS;
    ULONGLONG       ullCxEndTS;

    ULONGLONG       ullMaximumPeakChurnInBytes;
    ULONGLONG       ullDiffChurnThroughputInBytes;
    ULONGLONG       ullMaxDiffChurnThroughputInBytes;
    ULONGLONG       ullTotalExcessChurnInBytes;

    ULONGLONG       ullConsTagFail;
    ULONGLONG       TimeJumpTS;
    LONGLONG        ullTimeJumpInMS;

    ULONG           ulNumDiskCxSessionInProgress;
    ULONGLONG       ullChurnInBytesPerSecond;
    ULONGLONG       ullPendingTransId;
    ULONGLONG       ullNumPendTagFail;
    ULONGLONG       ullNumFailedTagsReported;
    ULONGLONG       ullPendingTimeJumpTS;

    ULONGLONG       ullNumS2Quits;
    ULONGLONG       ullNumSvagentsQuit;
    ULARGE_INTEGER  uliIOTimeinSec;
    ULONGLONG       ullMaxS2LatencyInMS;
}VM_CX_SESSION_DATA, *PVM_CX_SESSION_DATA;

typedef struct _VM_CX_SESSION_CONTEXT{
    KSPIN_LOCK                  CxSessionLock;
    BOOLEAN                     isCxIrpInProgress;
    ULONGLONG                   ullVmPeakChurnThresoldInBytes;
    ULONGLONG                   ullDiskPeakChurnThresoldInBytes;
    ULONGLONG                   ullDiskDiffThroughputThresoldInBytes;
    VM_CX_SESSION_DATA          vmCxSession;
    ULONGLONG                   ullSessionId;
    PIRP                        cxNotifyIrp;
    ULONG                       ulNumFilteredDisks;
    ULONG                       ulDisksInDataMode;
    ULONGLONG                   ullMinConsTagFail;
    ULONGLONG                   ullLastUserTransactionId;
    ULONGLONG                   ullMaxAccpetableTimeJumpFWDInMS;
    ULONGLONG                   ullMaxAccpetableTimeJumpBKWDInMS;
}VM_CX_SESSION_CONTEXT, *PVM_CX_SESSION_CONTEXT;


NTSTATUS
DeviceIoControlCommitDBFailTrans(
    IN      PDEVICE_OBJECT  DeviceObject,
    _Inout_ PIRP            Irp
);

NTSTATUS
DeviceIoControlGetCXStatsNotify(
    IN      PDEVICE_OBJECT  DeviceObject,
    _Inout_ PIRP            Irp
);

DRIVER_CANCEL InDskFltCancelCxNotifyIrp;

NTSTATUS
ValidateGetCxFailureNotifyInput(
    IN  PGET_CXFAILURE_NOTIFY   pGetCxFailureNotify,
    IN  ULONG                   ulInputLength,
    IN  ULONG                   ulOutputLength
);

BOOLEAN
CopyVMCxContextDataIfCxConditionsAreSet(
    _Inout_     PVM_CXFAILURE_STATS pVmCxFailureStats,
    IN          BOOLEAN             bAcquireLock
);

// This function should be called inside device spin lock
VOID
UpdateCXCountersAfterIo(IN  struct _DEV_CONTEXT* DevContext, ULONG ulIoSize);

VOID
UpdateCXCountersAfterCommit(IN  struct _DEV_CONTEXT*    DevContext);

// This function should be called inside device spin lock
VOID
EndOrCloseCxSessionIfNeeded(struct _DEV_CONTEXT *pDevContext);

NTSTATUS
CopyDiskCxSessionData(
_Inout_ PVM_CXFAILURE_STATS     pVmCxFailureStats,
IN      ULONG                   ulOutputLength
);

VOID
CopyDeviceCxStatus(
IN          struct _DEV_CONTEXT *       pDevContext,
_Inout_     PDEVICE_CXFAILURE_STATS     pDeviceCxFailureStats
);

VOID
CxSessionStopFilteringOrRemoveDevice(struct _DEV_CONTEXT *pDevContext);

VOID CXSessionTagFailureNotify();

VOID CxNotifyProductIssue(IN ProductIssue issue);

VOID
CalculateExcessChurn(
    PULONGLONG  a_MaxChurnBuckets,
    PULONGLONG  a_MaxChurnCountPerBucket,
    ULONG       ulNumBuckets,
    ULONGLONG   ullDiskPeakChurnThresoldInBytes,
    ULONGLONG   *pullTotalExcessChurnInBytes
);

VOID
CxNotifyTimeJump(
    IN  ULONGLONG   ullJumpDetectedAtTS,
    IN  ULONGLONG   ullTotalTimeJumpDetectedInMs,
    IN  BOOLEAN     bTimeJumpForward
);

