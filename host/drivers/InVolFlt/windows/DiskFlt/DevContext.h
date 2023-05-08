/*++

Copyright (c) Microsoft Corporation

Module Name:

    DevContext.h

Abstract:

    This module contains the type and function definitions specifically for disk filter in Device Context

Environment:

    Kernel mode

Revision History:

--*/

#pragma once

#include "global.h"
#include "FltContext.h"
#include "NoReboot.h"


// Following flags are overlapped with Volflt
#define DCF_BITMAP_DEV                      0x00000002
#define DCF_PHY_DEVICE_INRUSH               0x00100000
#define DCF_PROCESSED_POWER                 0x00200000




#define FILTER_DEVICE_PROPOGATE_CHARACTERISTICS (FILE_REMOVABLE_MEDIA |  \
                                                 FILE_PORTABLE_DEVICE    \
                                                 )

#define LOG_FILE_NAME_SUFFIX LOG_FILE_NAME_SUFFIX_DISK

//#define InDiskDbgPrint(Level, Mod, x) DbgPrint x;

//Base structure FLT_CONTEXT is inherited
typedef struct _DEV_CONTEXT:FLT_CONTEXT
{
    ULONG                    ulDevNum; //Store disk number as information.
    ULONG                    ulFlags2;
    ULONG                    ulClusterFlags;
    PARTITION_STYLE          DiskPartitionFormat; //Initialized to RAW style.
    BOOLEAN                  IsDeviceEnumerated;      //Set for Enumerated Device
    BOOLEAN                  IsPageFileDevicePossible;//Set for Suspected Page File Devices
    ULONGLONG                IoCounterWithPagingIrpSet;
    ULONGLONG                IoCounterWithSyncPagingIrpSet;
    ULONGLONG                IoCounterWithNullFileObject;
    ULONGLONG                ullOutOfSyncSequnceNumber;
    ULONG                    ulOutOfSyncTotalCount;
    ULONG                    ulLastOutOfSyncErrorCode;
    ULONGLONG                ullLastOutOfSyncSeqNumber;
    volatile LONGLONG        llInCorrectCompletionRoutineCount;
    volatile LONGLONG        llRedirectIrpCount;
    volatile LONGLONG        llIoCounterNonPassiveLevel;
    volatile LONGLONG        llIoCounterWithNULLMdl;
    volatile LONGLONG        llIoCounterWithInValidMDLFlags;
    volatile LONGLONG        llIoCounterMDLSystemVAMapFailCount;
    LARGE_INTEGER            licbReturnedToServiceInNonDataMode;
    LARGE_INTEGER            liChangesReturnedToServiceInNonDataMode;
    ULONGLONG                ullCounterTSODrained;
    ULONGLONG                ullLastCommittedTagTimeStamp;
    ULONGLONG                ullLastCommittedTagSequeneNumber;
    UCHAR                    ucShutdownMarker;
    UCHAR                    ucReserved[7];
    ULONG                    BitmapRecoveryState;
    ULONGLONG                ullGroupingID;
    DISK_TELEMETRY           DiskTelemetryInfo;
    REPLICATION_STATS        replicationStats;
    HANDLE                   DeviceHandle;
    KEVENT                   ClusterNotificationEvent;
    PSTORAGE_DEVICE_DESCRIPTOR      pDeviceDescriptor;
    ULONG                    ulPrevOutOfSyncErrorCodeSet;
    LARGE_INTEGER            liLogLastOutOfSyncTimeStamp;
    ULONG                    ulLogLastOutOfSyncErrorCode;
    LARGE_INTEGER            liLastCommittedOutOfSyncTimeStamp;  // This can be zero if the resync is not initiated by Source
    ULONG                    ulLastCommittedtOutOfSyncErrorCode; // This can be zero if the resync is not initiated by Source
    DEVICE_CX_SESSION        CxSession;
    BOOLEAN                  isInVmCxSessionFilteredList;
    TAG_COMMIT_NOTIFY_DEV_CONTEXT        TagCommitNotifyContext;
    volatile BOOLEAN         isBootDisk;
    volatile BOOLEAN         isResourceDisk;
    PIO_WORKITEM             ResourceDiskWorkItem;	
} DEV_CONTEXT, *PDEV_CONTEXT;

#define DEVICE_FLAGS_DONT_DRAIN                 0x1

#define DCF_CLUSTER_FLAGS_DISK_CLUSTERED                    0x1
#define DCF_CLUSTER_FLAGS_DISK_ONLINE                       0x2
#define DCF_CLUSTER_FLAGS_DISK_PROTECTED                    0x4


#define IS_DISK_CLUSTERED(pDevContext)  \
    TEST_FLAG((pDevContext)->ulClusterFlags, DCF_CLUSTER_FLAGS_DISK_CLUSTERED)

#define IS_DISK_ONLINE(pDevContext)  \
    TEST_FLAG((pDevContext)->ulClusterFlags, DCF_CLUSTER_FLAGS_DISK_ONLINE)


typedef enum _VDS_SAN_POLICY {
    VDS_SP_UNKNOWN,
    VDS_SP_ONLINE,
    VDS_SP_OFFLINE_SHARED,
    VDS_SP_OFFLINE,
    VDS_SP_OFFLINE_INTERNAL,
    VDS_SP_MAX
} VDS_SAN_POLICY;

NTSTATUS
InMageFltSaveAllChanges(PDEV_CONTEXT   pDevContext, BOOLEAN bWaitRequied, PLARGE_INTEGER pliTimeOut, bool bOpenBitmap = false);


NTSTATUS
InDskFltCreateClusterNotificationThread(
    _In_    PDEV_CONTEXT    pDevContext
);

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
FillBitmapFilenameInDevContext(_In_ _Notnull_ PDEV_CONTEXT pDevContext);

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
GetBitmapFileName(
    _In_ _Notnull_ PDEV_CONTEXT pDevContext,
    _In_ UNICODE_STRING &bitmapFileName,
    _In_ bool writeFileNameToRegistry
    );

VOID
CheckAndSetDiskIdConflict(
    IN  PDEV_CONTEXT    pDevContext
);

_DEV_CONTEXT *
GetDevContextWithGUID(PWCHAR pInputBuf, ULONG length);

PDEV_CONTEXT
GetDevContextForThisIOCTL(
    PDEVICE_OBJECT  DeviceObject, 
    PIRP            Irp
    );

void PostRemoveDevice(
    __in    PDEV_CONTEXT    pDevContext
    );

NTSTATUS
DeviceIoControlAppTagDisk(
    __in PDEVICE_OBJECT  DeviceObject,
    __in PIRP            Irp
  );

NTSTATUS
DeviceIoControlCommitAppTagDisk(
    __in PDEVICE_OBJECT  DeviceObject,
    __in PIRP            Irp

);

NTSTATUS
InMageFltDispatchWrite(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN BOOLEAN bNoRebootLayer = FALSE,
    IN PDRIVER_DISPATCH pDispatchEntryFunction = NULL
    );


NTSTATUS
InMageFltDispatchPnp(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN BOOLEAN bNoRebootLayer = FALSE,
    IN PDRIVER_DISPATCH pDispatchEntryFunction = NULL
    );

NTSTATUS
InMageFltDispatchPower(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN BOOLEAN bNoRebootLayer = FALSE,
    IN PDRIVER_DISPATCH pDispatchEntryFunction = NULL
    );

NTSTATUS
InMageFltDeviceControl(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN BOOLEAN bNoRebootLayer = FALSE,
    IN PDRIVER_DISPATCH pDispatchEntryFunction = NULL
    );


NTSTATUS
InMageFltAddDevice(
    IN PDRIVER_OBJECT DriverObject,
    IN PDEVICE_OBJECT LowerDeviceObject,
    IN BOOLEAN        bNoRebootLayer = FALSE,
    IN BOOLEAN        bIsPageFileDevicePossible = FALSE
    );

