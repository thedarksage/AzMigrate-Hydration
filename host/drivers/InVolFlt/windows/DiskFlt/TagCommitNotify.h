#pragma once

#include "Common.h"
#include "global.h"
#include "FltContext.h"
#include "DevContext.h"
#include "CxSession.h"

#define     POOL_TAG_COMMIT_NOTIFY       'YNDT'

struct _DEV_CONTEXT;
struct _TAG_INFO;

// This state is needed for follows
// If user mode input contains a device entry twice
// driver needs to validate it.
typedef enum {
    DEVICE_TAG_NOTIFY_STATE_NO_REQUEST = 0,
    DEVICE_TAG_NOTIFY_STATE_WAITING_FOR_TAG,
    DEVICE_TAG_NOTIFY_STATE_TAG_INSERTED,
    DEVICE_TAG_NOTIFY_STATE_TAG_NOT_INSERTED,
    DEVICE_TAG_NOTIFY_STATE_TAG_COMMITTED,
    DEVICE_TAG_NOTIFY_STATE_TAG_DROPPED
}DEVICE_TAG_NOTIFY_STATE;

typedef enum {
    DEVICE_TAG_STATE_UNKNOWN = 0,
    DEVICE_TAG_DROPPED_REASON_NONWRITE_ORDER,
    DEVICE_TAG_DROPPED_REASON_FILTERING_STOPPED,
    DEVICE_TAG_DROPPED_REASON_DEVICE_NOT_FOUND,
    DEVICE_TAG_DROPPED_REASON_DEVICE_REMOVED
}DEVICE_TAG_COMMIT_REASON_CODE;

typedef struct _TAG_COMMIT_NOTIFY_CONTEXT {
    UCHAR                   TagGUID[GUID_SIZE_IN_CHARS + 1];
    NTSTATUS                TagCommitStatus;
    ULONG                   ulFlags;
    bool                    isOwned;
    bool                    isCommitNotifyTagInsertInProcess;
    ULONG                   ulNumDevices;
    ULONG                   ulNumDevicesTagCommitted;
    VM_CXFAILURE_STATS      vmCxStatus;
    BOOLEAN                 bTimerSet;
    PIRP                    pTagCommitNotifyIrp;
}TAG_COMMIT_NOTIFY_CONTEXT, *PTAG_COMMIT_NOTIFY_CONTEXT;

// In this structure CX Session looks overdo as CxSession data is already in Device context
// But the problem is to handle synchronization, it is required.
// Take following case
//  During tag insert device was in non-WO state. It can have some Cx session data
//  Now as not all disks are tagged, commit notify irp will be completed
// But during complete device may have come back to WO state
// In this case CX data will be lost completely.
// To handle this condition, CX session need to be inside CX Notify context
typedef struct __TAG_COMMIT_NOTIFY_DEV_CONTEXT {
    UCHAR                           TagGUID[GUID_SIZE_IN_CHARS + 1];
    DEVICE_TAG_NOTIFY_STATE         TagState;
    bool                            isCommitNotifyTagPending;
    ULONGLONG                       ullCommitTimeStamp;
    ULONGLONG                       ullInsertTimeStamp;
    ULONGLONG                       ullSequenceNumber;
    DEVICE_CX_SESSION               CxSession;
}TAG_COMMIT_NOTIFY_DEV_CONTEXT, *PTAG_COMMIT_NOTIFY_DEV_CONTEXT;

NTSTATUS
DeviceIoControlTagCommitNotify(
    IN      PDEVICE_OBJECT  DeviceObject,
    _Inout_ PIRP            Irp
);

NTSTATUS
DeviceIoControlSetDrainState(
    IN      PDEVICE_OBJECT  DeviceObject,
    _Inout_ PIRP            Irp
);

NTSTATUS
DeviceIoControlGetDiskState(
    IN      PDEVICE_OBJECT  DeviceObject,
    _Inout_ PIRP            Irp
);

VOID
IndskFltCompleteTagNotifyIfDone(
    struct _DEV_CONTEXT *DevContext);

NTSTATUS
IndskFltValidateTagInsertion();

NTSTATUS
IndskFltValidateTagInsertion(_TAG_INFO*  pTagInfo);

VOID
IndskFltResetDeviceTagNotifyState(bool bReleaseDrainBarrier);

DRIVER_CANCEL InDskFltCancelTagCommitNotifyIrp;

NTSTATUS
SetDrainStateForAllDevices(bool set, PSET_DRAIN_STATE_OUTPUT    pDrainStateOutput, ULONG ulSize);

NTSTATUS
SetDeviceState(Registry* pDevParam, PWCHAR wDeviceId, PWCHAR StateName, ULONG ulValue);

VOID
ResetDrainState(PTAG_COMMIT_STATUS pCommitStatus, ULONG    ulNumDevices, Registry* pDevParam);

NTSTATUS
ResetDrainState(_DEV_CONTEXT    *pDevContext);

NTSTATUS
PersistDrainStatus(PTAG_COMMIT_STATUS   pCommitStatus, ULONG    ulNumDevices);