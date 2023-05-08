#pragma once

#include "Global.h"
#include "DevContext.h"
#include "TagNode.h"

#define SYSTEM_ROOT             L"\\SystemRoot"
#define BOOT_DEVICE             L"\\Device\\BootDevice"
#define SAN_ATTRIB_NAME         L"Attributes"

#define ARCNAME_SLASH_COUNT 3

#define IS_APP_TAG(flags) \
    (TEST_FLAG(flags, TAG_INFO_FLAGS_LOCAL_APP) || \
    TEST_FLAG(flags, TAG_INFO_FLAGS_DISTRIBUTED_APP))

#define INDSKFLT_UPDATE_LAST_TAGREQUEST_TIME(pTagRequestTime) {  \
    KeAcquireSpinLock(&DriverContext.Lock, &OldIrql); \
    DriverContext.globalTelemetryInfo.liLastTagRequestTime = *pTagRequestTime; \
    KeReleaseSpinLock(&DriverContext.Lock, OldIrql); \
    }

#define INDSKFLT_SET_TAGREQUEST_TIME(pTagRequestTime) {  \
    KIRQL OldIrql; \
    KeAcquireSpinLock(&DriverContext.Lock, &OldIrql); \
    *pTagRequestTime = DriverContext.globalTelemetryInfo.liTagRequestTime; \
    KeReleaseSpinLock(&DriverContext.Lock, OldIrql); \
}

#define IsResourceDiskSuspected(pDevContext)     \
    TEST_FLAG((pDevContext)->ulFlags, DCF_FILTERING_STOPPED)    \
            &&  \
    ((pDevContext)->pDeviceDescriptor && (pDevContext)->pDeviceDescriptor->BusType == BusTypeAta) 

#define AcquireLockIf(c, l, irql) \
    if ((c)) KeAcquireSpinLock((l), (irql));

#define ReleaseLockIf(c, l, irql) \
    if ((c)) KeReleaseSpinLock((l), (irql));



NTSTATUS
GetDeviceNumber(
__in   PDEVICE_OBJECT pDevObject,
__inout PSTORAGE_DEVICE_NUMBER pStorageDevNum,
__in   USHORT usBufferSize
);

NTSTATUS
InMageFltForwardIrpSynchronous(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
InMageSendDeviceControl(
    __in      PDEVICE_OBJECT DeviceObject,
    __in      ULONG IoControlCode,
    __in_opt  PVOID InputBuffer,
    __in      ULONG InputBufferLength,
    __out_opt PVOID OutputBuffer,
    __in      ULONG OutputBufferLength,
    __inout   PIO_STATUS_BLOCK pIoStatus,
    __in_opt  BOOLEAN InternalDeviceIoControl = FALSE
    );

NTSTATUS
ValidateRequestForDeviceAndHandle (
    __in    PDEVICE_OBJECT    DeviceObject,
    __in    PIRP              Irp
    );

NTSTATUS
InMageFltSendToNextDriver(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
InMageFltClose(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
InDskFltCleanup(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
InMageFltCleanup(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN BOOLEAN bNoRebootLayer = FALSE,
    IN PDRIVER_DISPATCH pDispatchEntryFunction = NULL
    );


NTSTATUS
InMageFltCreate(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );
#if DBG
NTSTATUS
InMageFltRead(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );
#endif
VOID
InMageFltUnload(
    IN PDRIVER_OBJECT DriverObject
    );

NTSTATUS
InMageFltIrpCompletion(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context
    );

VOID
InMageFltSyncFilterWithTarget(
    IN PDEVICE_OBJECT FilterDevice,
    IN PDEVICE_OBJECT TargetDevice
    );

VOID
ServiceStateChangeFunction(
    IN PVOID Context
    );

//Added for Persistent Time Stamp
VOID
PersistentValuesFlush(
    IN PVOID Context
    );

VOID
ClearDevStats(PDEV_CONTEXT   pDevContext);

NTSTATUS
ValidateParameters(SET_MIN_FREE_DATA_TO_DISK_SIZE_LIMIT_INPUT Input,PSET_MIN_FREE_DATA_TO_DISK_SIZE_LIMIT_OUTPUT pOutputBuffer);

bool
IsValidIoctlTagDeviceInputBuffer(
    PUCHAR pBuffer,
    ULONG  ulBufferLength
    );

VOID
InMageSetBootVolumeChangeNotificationFlag(
	PVOID pContext
	);

VOID InMageFltOnReinitialize(IN PDRIVER_OBJECT pDriverObj,
							 IN PVOID          pContext,
							 IN ULONG          ulCount
							 );
BOOLEAN
ProcessDevStateChange(
    PDEV_CONTEXT     pDevContext,
    PLIST_ENTRY         ListHead,
    BOOLEAN             bServiceStateChanged
    );

NTSTATUS
DeviceIoControlSetDirtyBlockNotifyEvent(
    PDEVICE_OBJECT  DeviceObject, 
    PIRP            Irp
    );

#ifdef _CUSTOM_COMMAND_CODE_
NTSTATUS
DeviceIoControlCustomCommand(
    PDEVICE_OBJECT  DeviceObject,
    PIRP            Irp
    );

#endif // _CUSTOM_COMMAND_CODE_

NTSTATUS
InDskSendDeviceControl(
_In_ PDEVICE_OBJECT DeviceObject,
_In_ ULONG IoControlCode,
_In_ PVOID InputBuffer,
_In_ ULONG InputBufferLength,
_Out_ PVOID OutputBuffer,
_In_ ULONG OutputBufferLength,
_In_ BOOLEAN IsInternal,
_In_ BOOLEAN bNoReboot
);

NTSTATUS
InmageGetTopOfDeviceStack(
    _In_    PDEV_CONTEXT pDevContext,
    _Out_ PFILE_OBJECT* FileObject,
    _Out_ PDEVICE_OBJECT* DeviceObject
);

NTSTATUS
IndskfltGetGeometryEx(PDEV_CONTEXT            pDevContext, PDISK_GEOMETRY_EX* ppGeometryEx);

NTSTATUS
ExtractDevIDFromDiskSignature(
    __in    PDISK_SIGNATURE pDiskSignature,
    __out   PWSTR wBuffer,
    __in    USHORT usBufferLength
);

VOID
InDskFltHandleDiskResize(
_In_    PDEV_CONTEXT   pDevContext,
_In_    PGET_LENGTH_INFORMATION    pLengthInfo);

NTSTATUS
DeviceIoControlGetLengthInfo(
_In_    PDEVICE_OBJECT  DeviceObject,
_In_    PIRP            Irp,
_In_    BOOLEAN         bNoRebootLayer
);

NTSTATUS
DeviceIoControlSetDriverFlags(
    PDEVICE_OBJECT  DeviceObject, 
    PIRP            Irp
    );

NTSTATUS
DeviceIoControlSetDriverConfig(
    PDEVICE_OBJECT  DeviceObject, 
    PIRP            Irp
    );

NTSTATUS
DeviceIoControlSyncTagVolume(
    PDEVICE_OBJECT  DeviceObject, 
    PIRP            Irp
    );

NTSTATUS
DeviceIoControlGetTagVolumeStatus(
    PDEVICE_OBJECT DeviceObject, 
    PIRP Irp
    );

NTSTATUS
StopFiltering(PDEV_CONTEXT  pDevContext, bool bDeleteBitmapFile);

NTSTATUS
StopFiltering(PDEV_CONTEXT  pDevContext, bool bDeleteBitmapFile, bool bSetRegistry);

#if DBG
NTSTATUS
DeviceIoControlSetDebugInfo(
    PDEVICE_OBJECT  DeviceObject, 
    PIRP            Irp
    );
#endif // DBG

NTSTATUS
DeviceIoControlResyncStartNotification(
    PDEVICE_OBJECT  DeviceObject, 
    PIRP            Irp
    );

NTSTATUS
DeviceIoControlResyncEndNotification(
    PDEVICE_OBJECT  DeviceObject, 
    PIRP            Irp
    );

NTSTATUS
DeviceIoControlClearBitMap(
    PDEVICE_OBJECT  DeviceObject, 
    PIRP            Irp
    );

NTSTATUS
DeviceIoControlProcessStartNotify(
    PDEVICE_OBJECT  DeviceObject, 
    PIRP            Irp
    );

NTSTATUS
DeviceIoControlServiceShutdownNotify(
    PDEVICE_OBJECT  DeviceObject, 
    PIRP            Irp
    );

NTSTATUS
DeviceIoControlCommitDirtyBlocksTransaction(
    PDEVICE_OBJECT  DeviceObject, 
    PIRP            Irp
    );

NTSTATUS
DeviceIoControlSetResyncRequired(
    PDEVICE_OBJECT  DeviceObject, 
    PIRP            Irp
    );

NTSTATUS
DeviceIoControlGetDirtyBlocksTransaction(
    PDEVICE_OBJECT  DeviceObject, 
    PIRP            Irp
    );

NTSTATUS
DeviceIoControlClearDevStats(
    PDEVICE_OBJECT DeviceObject,
    PIRP Irp
    );

NTSTATUS
DeviceIoControlGetDMinfo(
    PDEVICE_OBJECT DeviceObject,
    PIRP Irp
    );

NTSTATUS
DeviceIoControlGetDeviceTrackingSize(
    PDEVICE_OBJECT DeviceObject,
    PIRP Irp
    );

NTSTATUS 
DeviceIoControlGetLcn (
    PDEVICE_OBJECT DeviceObject, 
    PIRP Irp);

NTSTATUS
DeviceIoControlGetVolumeWriteOrderState(
    PDEVICE_OBJECT DeviceObject,
    PIRP Irp
    );

NTSTATUS
DeviceIoControlGetVolumeStats(
    PDEVICE_OBJECT DeviceObject,
    PIRP Irp
    );

NTSTATUS
DeviceIoControlGetAdditionalDeviceStats(
    PDEVICE_OBJECT  DeviceObject,
    PIRP            Irp
    );
 
// This IOCTL gets ther Driver global Time Stamp and Sequence number
NTSTATUS
DeviceIoControlGetGlobalTSSN(
    PDEVICE_OBJECT  DeviceObject, 
    PIRP            Irp
    );

NTSTATUS
DeviceIoControlIoBarrierTagDisk(
	PDEVICE_OBJECT  DeviceObject,
	PIRP            Irp
);

VOID
InmageReleaseWrites(PTAG_INFO pTagInfo);

NTSTATUS
IsValidBarrierIoctlTagDiskInputBuffer(
	PUCHAR	pBuffer,
	ULONG	ulBufferLength,
    ULONG   ulNumberOfTags
);

NTSTATUS
DeviceIoControlSetIoSizeArray(
    PDEVICE_OBJECT  DeviceObject, 
    PIRP            Irp
    );

NTSTATUS
DeviceIoControlSetDataSizeToDiskLimit(
    PDEVICE_OBJECT  DeviceObject, 
    PIRP            Irp
    );

NTSTATUS
DeviceIoControlSetMinFreeDataSizeToDiskLimit(
    PDEVICE_OBJECT  DeviceObject, 
    PIRP            Irp
    );

NTSTATUS
DeviceIoControlSetDataNotifyLimit(
    PDEVICE_OBJECT  DeviceObject, 
    PIRP            Irp
    );

NTSTATUS
DeviceIoControlSetDataFileThreadPriority(
    PDEVICE_OBJECT  DeviceObject, 
    PIRP            Irp
    );

NTSTATUS
DeviceIoControlSetWorkerThreadPriority(
    PDEVICE_OBJECT  DeviceObject, 
    PIRP            Irp
    );

NTSTATUS
DeviceIoControlGetMonitoringStats(
    PDEVICE_OBJECT  DeviceObject,
    PIRP            Irp
    );

NTSTATUS
DeviceIoControlSetReplicationState(
    PDEVICE_OBJECT  DeviceObject,
    PIRP            Irp
);
NTSTATUS
DeviceIoControlGetReplicationStats(
    PDEVICE_OBJECT  DeviceObject,
    PIRP            Irp
);
static
NTSTATUS InMageExtractDriveString(
    IN OUT  PUNICODE_STRING Source
    );

NTSTATUS
TagVolumesAtomically(
    PTAG_INFO       pTagInfo,
    PUCHAR          pSequenceOfTags
    );

NTSTATUS
TagVolumesInSequence(
    PTAG_INFO       pTagInfo,
    PUCHAR          pSequenceOfTags);

NTSTATUS
TagVolumesInSequence(
    PTAG_INFO       pTagInfo,
    PUCHAR          pSequenceOfTags,
    bool            isCommitNotifyTagRequest
);

VOID
ServiceStateChangeFunction(
    IN PVOID Context
    );

//Added for Persistent Time Stamp
VOID
PersistentValuesFlush(
    IN PVOID Context
    );

VOID
ClearDevStats(PDEV_CONTEXT   pDevContext);

NTSTATUS
GetUninitializedDisks(
_Inout_    PLIST_ENTRY ListHead
);

NTSTATUS
InDskFltRescanDisk(
_In_    PDEV_CONTEXT pDevContext
);

BOOLEAN
InDskFltRescanUninitializedDisks();

NTSTATUS
ValidateParameters(SET_MIN_FREE_DATA_TO_DISK_SIZE_LIMIT_INPUT Input,PSET_MIN_FREE_DATA_TO_DISK_SIZE_LIMIT_OUTPUT pOutputBuffer);

NTSTATUS
InDskFltRegisterLowMemoryNotification(
    VOID
);

VOID
InDskFltDeregisterLowMemoryNotification(
    VOID
);

VOID
InDskFltLowMemoryThreadProc(
    _In_    PVOID Context
);


NTSTATUS
InDskFltCompareExchangeCrashContext(
_In_    PUCHAR   crashTagContext,
_In_    PUCHAR   oldContext,
_In_    BOOLEAN exchange
);

VOID
InDskFltResetCrashContext(
    VOID
);

LONG
InDskFltGetNumFilteredDevices(
    VOID
);

NTSTATUS
CreateCrashConsistentBarrier(
    VOID
);

VOID
InDskFltReleaseWrites(
    VOID
);

VOID
InDskFltCrashConsistentTagCleanup(
     _In_   etTagStateTriggerReason tagStateReason
);

VOID
InDskFltCancelQueuedWriteForFileObject(
    _In_    PDEVICE_EXTENSION pDeviceExtension,
    _In_    PFILE_OBJECT fileObject
);

NTSTATUS
ValidateAndCreateDeviceBarrier(
    _In_        PTAG_INFO *ppTagInfo,
    _In_        BOOLEAN bBarrierAlreadyCreated,
    _In_        ULONG ulFlags,
    _Inout_     PTAG_TELEMETRY_COMMON pTagCommonAttribs
);

VOID
InDskFltCrashConsistentTimerDpc(
    _In_        PKDPC   TimerDpc,
    _In_opt_    PVOID   Context,
    _In_opt_    PVOID   SystemArgument1,
    _In_opt_    PVOID   SystemArgument2
);


VOID
InDskFltCancelCrashConsistentTag(
    _Inout_ PDEVICE_OBJECT  DeviceObject,
    _Inout_ PIRP            Irp
);

NTSTATUS
DeviceIoControlIoBarrierTagDisk(
    _Inout_    PDEVICE_OBJECT  DeviceObject,
    _Inout_    PIRP            Irp
);

NTSTATUS
DeviceIoControlHoldWrites(
    _In_        PDEVICE_OBJECT  DeviceObject,
    _In_        PIRP            Irp
);

NTSTATUS
DeviceIoControlDistributedCrashTag(
    _In_    PDEVICE_OBJECT  DeviceObject,
    _In_    PIRP            Irp
);

NTSTATUS
DeviceIoControlReleaseWrites(
    _In_    PDEVICE_OBJECT  DeviceObject,
    _In_    PIRP            Irp
);

NTSTATUS
DeviceIoControlCommitDistributedTag(
    _In_    PDEVICE_OBJECT  DeviceObject,
    _In_    PIRP            Irp
);

NTSTATUS
DeviceIoControlGetNumInflightIos(
    _In_    PDEVICE_OBJECT  DeviceObject,
    _In_    PIRP            Irp
);

VOID InDskFltCsqAcquireLock(
    _In_        PIO_CSQ csq,
    _In_        PKIRQL irql
    );

VOID InDskFltCsqReleaseLock(
    _In_    PIO_CSQ csq,
    _In_    KIRQL irql
    );

VOID InDskFltCsqInsertIrp(
    _In_        PIO_CSQ csq,
    _In_        PIRP Irp
    );

VOID InDskFltCsqRemoveIrp(
    _In_    PIO_CSQ csq,
    _In_    PIRP Irp
    );

VOID InDskFltCompleteCancelledIrp(
    _In_    PIO_CSQ csq,
    _In_    PIRP Irp
    );

PIRP InDskFltCsqPeekNextIrp(
    _In_    PIO_CSQ csq,
    _In_    PIRP Irp,
    _In_    PVOID PeekContext
    );

NTSTATUS
InDskFltCreateReleaseWritesThread(
_In_    PDEV_CONTEXT    pDevContext
);

VOID
InDskFltReleaseDeviceWrites(
_In_        PDEV_CONTEXT pDevContext
);

VOID
InDskReleaseWritesThread(
_In_    PVOID Context
);


VOID
InDskFltIoBarrierCleanup(
_In_    PDEV_CONTEXT    pDevContext
);

ULONG
InmageGetNumInFlightIos(
    VOID
);

BOOLEAN
InDskFltCancelProtocolHoldIrp(
    _In_    BOOLEAN bResetCancelRoutine, 
    _In_    BOOLEAN bCancelTimer
);

NTSTATUS
IsValidBarrierIoctlTagDiskInputBuffer(
    _In_    PUCHAR	pBuffer,
    _In_    ULONG	ulBufferLength,
    _In_    ULONG	ulNumberOfTags
);

NTSTATUS
InDskFltCompareExchangeTagState(
_In_        PUCHAR              TagContext,
_In_        etTagProtocolState  oldState,
_In_        etTagProtocolState  newState,
_Inout_     PBOOLEAN            pIsValidContext
);

NTSTATUS
InDskFltCompareExchangeAppConsistentTagState(
_In_        PUCHAR              TagContext,
_In_        etTagProtocolState  oldState,
_In_        etTagProtocolState  newState,
_Out_       PBOOLEAN            pIsValidContext,
_In_        BOOLEAN             bLockAcquired = FALSE
);

#define GET_DEVICE_EXTENSION(csq) \
    CONTAINING_RECORD(csq, DEV_CONTEXT, CsqIrpQueue)

#define CAN_HANDLE_TAG_CLEANUP() \
    ((ecTagNoneInProgress != DriverContext.TagProtocolState) && \
     (ecTagInsertTagStarted != DriverContext.TagProtocolState) && \
     (ecTagCommitStarted != DriverContext.TagProtocolState)  && \
     (ecTagCleanup != DriverContext.TagProtocolState))

#define IsWindows2008()  (DriverContext.osMajorVersion == 6 && DriverContext.osMinorVersion == 0)
void
AppTagHoldIrpCancelRoutine(PDEVICE_OBJECT DeviceObject, PIRP Irp);

void
FinalizeDeviceTagStateAll(
    __in    PTAG_INFO pTagInfo,
    __in    bool      LockAcquired,
    __in    BOOLEAN   CommitAll,
    __in    etTagStateTriggerReason tagStateReason
);

void
UpdateNotifyAndDataRetrievalStats(
__in    PDEV_CONTEXT  pDeviceContext,
__in    PKDIRTY_BLOCK DirtyBlock
);

NTSTATUS
CleanupTagProtocol(BOOLEAN bCancelTimer, BOOLEAN bCancelIrp, etTagStateTriggerReason tagStateReason, BOOLEAN bCommitAll = FALSE, PTAG_INFO* ppTagInfo = NULL);

void
CloseLinkHandle(_In_ HANDLE *handle);

NTSTATUS
InDskFltSetSanPolicy(
    _Inout_ PDEVICE_EXTENSION DevExt
);

NTSTATUS
InDskFltUpdateHardwareId(
_In_    PDEVICE_EXTENSION  DeviceExtension,
_In_    PWCHAR             HardwareIdName
);

NTSTATUS
InDskFltQuerySanPolicy(
_In_    HANDLE              hParametersKey,
_In_    PWCHAR              ParameterName,
_Out_   PULONG              ParameterValue
);

NTSTATUS
InDskFltQueryPreviousSanPolicy(
_In_    PWCHAR              PrevHardwareIdName,
_In_    PWCHAR              ParameterName,
_Out_   PULONG              ParameterValue
);

NTSTATUS
InDskFltUpdateSanPolicy(
_In_    HANDLE              hParametersKey,
_In_    PWCHAR              ParameterName,
_In_    ULONG              ParameterValue
);

NTSTATUS
InDskFltUpdateSanPolicy(
_In_    PDEV_CONTEXT       pDevContext,
_In_    PWCHAR             ParameterName,
_In_    ULONG              ParameterValue
);

NTSTATUS
IndskfltQueryPreviousHardwareIdName(
_In_    PDEV_CONTEXT DevContext
);

NTSTATUS
InDskFltQueryNameForRegistryHandle(
_In_    HANDLE              hParametersKey,
_Out_   PWCHAR*             HardwareIdKeyName,
_Out_   PULONG              ulSize
);

void InDskFltTelemetryInsertTagFailure(
_Inout_   PDEV_CONTEXT           DevContext,
_In_      PTAG_TELEMETRY_COMMON  pTagCommonAttribs,
_In_      ULONG                  MessageType
);

void InDskFltGetTagMarkerGUID(
_In_      PUCHAR            pTagBuffer,
_Out_     PCHAR             pTagMarkerGUID,
_In_      ULONG             guidLength,
_In_      ULONG             tagInputBufferLength
);

void InDskFltTelemetryTagIOCTLFailure(
    _In_      PTAG_TELEMETRY_COMMON  pTagCommonAttribs,
    _In_      NTSTATUS               Status,
    _In_      ULONG                  MessageType
    );

VOID
CloseDeviceHandles();

NTSTATUS
OpenHandleToDevice(
_In_  PDEVICE_OBJECT DeviceObject,
_Out_ PHANDLE FileHandle
);

bool InDskFltAppGetTagMarkerGUID(
    _In_  PUCHAR pTagBuffer,
    _Out_ PCHAR  pTagMarkerGUID,
    _In_  ULONG  guidLength,
    _In_  ULONG  tagInputBufferLength
    );

NTSTATUS DeviceIoControlPreCheckTagDisk(
    _In_ PDEVICE_OBJECT DeviceObject,
    _In_ PIRP           Irp);

NTSTATUS
DeviceIoControlGetDiskIndexList(
    IN PDEVICE_OBJECT  DeviceObject,
    IN PIRP            Irp
);

VOID
InDskFltFlushFileBuffers();

VOID
CleanupFlushVolumeNodes();

BOOLEAN
InDskFltFlushVolumeBuffer(
    _In_    PWCHAR                  VolumeSymName,
    _In_    PULONG                  pProtectedDiskNumbers,
    _In_    ULONG                   ulNumProtectedDisks,
    _In_    PVOLUME_DISK_EXTENTS    pVolExtents,
    _In_    DWORD                   dwVolExtentsSize
);

NTSTATUS
InDskFltQueryStorageProperty(
    _In_    PDEVICE_OBJECT                  DeviceObject,
    _Inout_ PSTORAGE_DEVICE_DESCRIPTOR*     ppStorageDesc
);


NTSTATUS
InDskFltUpdateOSVersion(
    _In_ PDEV_CONTEXT DevContext
);

void SetMessageTypeFeature(
    _Inout_ ULONG *MessageType
    );

VOID
FillVolumeStatsStructure(
    _Inout_ PVOLUME_STATS     pVolumeStats,
    _In_    PDEV_CONTEXT      pDevContext
);

VOID
FillVolumeStatsStructure(
    _Inout_ PVOLUME_STATS     pVolumeStats,
    _In_    PDEV_CONTEXT      pDevContext,
    _In_    BOOLEAN         bReadBitmap
);

NTSTATUS
DeviceIoControlSetSanPolicy(
    _In_    PDEVICE_OBJECT  DeviceObject,
    _Inout_ PIRP            Irp
);

NTSTATUS
InDskFltDetectFailover(
    _In_    PDEV_CONTEXT    pDevContext
);

NTSTATUS
InDskFltDetectResourceDiskOnFOVM(
    _In_    PDEV_CONTEXT    pDevContext
);

VOID ResourceDiskDetectionWorkItem(
    _In_     PDEVICE_OBJECT DeviceObject,
    _In_opt_ PVOID          Context
    );

NTSTATUS
VerifyOSUpgrade(
    _In_        PDEV_CONTEXT    pDevContext
);

NTSTATUS
QueueResourceDiskDetectionWFIfRequired
(
    _In_ PDEV_CONTEXT DevContext,
    _In_ PIRP         Irp
);

NTSTATUS
QueryPreviousBootInformation(Registry ParametersKey);

NTSTATUS
QueryNoHydration(
    _In_ PDEV_CONTEXT   pDevContext,
    _In_ PIRP           Irp
);

BOOLEAN
IsDiskSeenFirstTime(PDEV_CONTEXT pDevContext);

NTSTATUS
DeviceIoControlGetDriverFlags(
_In_    PDEVICE_OBJECT  DeviceObject,
_Inout_ PIRP            Irp
);

#ifdef _WIN64
NTSTATUS
DeviceIoControlSCSIPassThrough(
    IN      PDEVICE_OBJECT  DeviceObject,
    _Inout_ PIRP            Irp
);

NTSTATUS
DeviceIoControlStoragePersistentReserveOut(
    IN      PDEVICE_OBJECT       DeviceObject,
    _Inout_ PIRP                 Irp,
    IN      BOOLEAN              bNoRebootLayer,
    IN      PDRIVER_DISPATCH     pDispatchEntryFunction
);
#endif

NTSTATUS
DeviceIoControlSetDiskClustered(
    IN      PDEVICE_OBJECT  DeviceObject,
    _Inout_ PIRP            Irp
);

NTSTATUS
UpdateBootInformation();

NTSTATUS
QueryBootDiskInformation();

VOID
IndskFltCompleteIrp(PIRP        Irp,
    NTSTATUS                    Status,
    ULONG                       ulOutputLength);

VOID
InDskFltTagNotifyTimerDpc(
    _In_        PKDPC   TimerDpc,
    _In_opt_    PVOID   Context,
    _In_opt_    PVOID   SystemArgument1,
    _In_opt_    PVOID   SystemArgument2
);


VOID
InDskFltDeleteClusterNotificationThread(
    _In_    PDEV_CONTEXT    pDevContext
);