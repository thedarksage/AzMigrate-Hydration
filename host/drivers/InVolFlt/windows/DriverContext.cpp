#include "Common.h"
#include "DriverContext.h"
#include "global.h"
#include "ListNode.h"
#include "VBitmap.h"
#include "misc.h"
#include "ChangeNode.h"
#include "DataFileWriterMgr.h"
#include "VContext.h"
#include "FltFunc.h"
#include <version.h>

DRIVER_CONTEXT  DriverContext;

NTSTATUS
AllocateGlobalPageFileNodes (
    );

void
FreeGlobalPageFileNodes (
    );

NTSTATUS
InitializeDriverContext(
    PDRIVER_OBJECT  DriverObject,
    PUNICODE_STRING RegistryPath
    )
/*++
Description  : This routine is called as part of DriverEntry(), creates the filter device name, initializes global parameters by reading from registry.
               If the registry key is not available, this would create a key and set the default value.               
               
Arguments    : DriverObject - Pointer to DriverObject
               RegistryPath - Pointer to Driver's Registry path
               
Return Value : STATUS_INSUFFICIENT_RESOURCES - on memory Allocation failure
               STATUS_SUCCES                 - on success
               or appropriate status returned from system calls
--*/

{
    NTSTATUS        Status = STATUS_SUCCESS;
    UNICODE_STRING  DeviceName, SymLinkName;
    USHORT          usLength;
    Registry        ParametersKey, KernelTagsKey;
    ULONG           ulDevDataModeCapture, ulDataModeCaptureForNewDevices, ulKernelTagsEnabled;
    ULONG           ulDataFiles = DEFAULT_DEVICE_DATA_FILES, ulDataFilesForNewDevs = DEFAULT_DATA_FILES_FOR_NEW_DEVICES, ulFiltDisabledForNewDevice;
    ULONG           ulFsFlushPreShutdown = DEFAULT_FS_FLUSH_PRE_SHUTDOWN;

#ifdef VOLUME_CLUSTER_SUPPORT
    ULONG           ulFiltDisabledForNewClusterDevice;
#endif

    ULONG           ulWorkerThreadPriority = 0;
    ULONG           ulLineNum = 0;
    ULONG           ulBootCounter = 0;
    BOOTDISK_INFORMATION    bootDiskInformation = { 0 };

    ULONG32         ul32Value;
    ULONG           ulResyncRequiredOnReboot = 0;

    RtlZeroMemory(&DriverContext, sizeof(DRIVER_CONTEXT));

#ifdef _CUSTOM_COMMAND_CODE_
    InitializeListHead(&DriverContext.MemoryAllocatedList);
#endif // _CUSTOM_COMMAND_CODE_
    DriverContext.DriverObject = DriverObject;
    // Store the Driver Registry Path and Parameters Path
    usLength = RegistryPath->Length;
    // Initialize data filtering values and data blocks
    KeInitializeSpinLock(&DriverContext.DataBlockCounterLock);
    KeInitializeSpinLock(&DriverContext.DataBlockListLock);
    InitializeListHead(&DriverContext.LockedDataBlockList);
    InitializeListHead(&DriverContext.FreeDataBlockList);
    InitializeListHead(&DriverContext.FreePageFileList);
    InitializeListHead(&DriverContext.OrphanedMappedDataBlockList);
    DriverContext.eServiceState = ecServiceNotStarted;
    KeInitializeSpinLock(&DriverContext.Lock);
    InitializeListHead(&DriverContext.HeadForDevContext);
    InitializeListHead(&DriverContext.HeadForBasicDisks);
    InitializeListHead(&DriverContext.HeadForTagNodes);
    KeInitializeSpinLock(&DriverContext.TagListLock);
    KeInitializeEvent(&DriverContext.ServiceThreadWakeEvent, NotificationEvent, FALSE);
    KeInitializeEvent(&DriverContext.ServiceThreadShutdownEvent, NotificationEvent, FALSE);

    KeInitializeTimer(&DriverContext.TagProtocolTimer);
    KeInitializeDpc(&DriverContext.TagProtocolTimerDpc, InDskFltCrashConsistentTimerDpc, &DriverContext);
    DriverContext.ulTagContextGUIDLength = ARRAYSIZE(DriverContext.TagContextGUID);

    KeInitializeTimer(&DriverContext.TagNotifyProtocolTimer);
    KeInitializeDpc(&DriverContext.TagNotifyProtocolTimerDpc, InDskFltTagNotifyTimerDpc, &DriverContext);

    //Added for persistent time stamp
    KeInitializeEvent(&DriverContext.PersistentThreadShutdownEvent, NotificationEvent, FALSE);
    InitializeListHead(&DriverContext.HeadForDevBitmaps);
    KeInitializeSpinLock(&DriverContext.PageFileListLock);
    DriverContext.ulNumDevBitmaps = 0;

    KeInitializeMutex(&DriverContext.DiskRescanMutex, 0);

    DriverContext.bootVolNotificationInfo.bootVolumeDetected = 0;
    DriverContext.bootVolNotificationInfo.pBootDevContext = NULL;
    DriverContext.bootVolNotificationInfo.failureCount = 0;
    DriverContext.bootVolNotificationInfo.reinitCount = 0;
    DriverContext.bootVolNotificationInfo.LockUnlockBootVolumeDisable = 0;
    //This is not-signalled or acquired or blocking state.
    KeInitializeEvent(&DriverContext.bootVolNotificationInfo.bootNotifyEvent, NotificationEvent, FALSE);
    KeInitializeEvent(&DriverContext.hFailoverDetectionOver,NotificationEvent, FALSE);

    // Timestamp initialization
    KeInitializeSpinLock(&DriverContext.TimeStampLock);
    DriverContext.ullLastTimeStamp = 0;
    DriverContext.ullLastPersistTimeStamp = 0;
    DriverContext.ullPersistedTimeStampAfterBoot = 0;
    DriverContext.PersistentRegistryCreatedFlag = FALSE;
    DriverContext.LastShutdownMarker = Uninitialized;
    DriverContext.CurrentShutdownMarker = DirtyShutdown;
    DriverContext.PersistantValueFlushedToRegistry = false;
    DriverContext.ullTimeStampSequenceNumber = 0;
    DriverContext.ullLastPersistSequenceNumber = 0;
    DriverContext.ullPersistedSequenceNumberAfterBoot= 0;
    KeInitializeSpinLock(&DriverContext.NoMemoryLogEventLock);
    DriverContext.liLastDBallocFailAtTickCount.QuadPart = 0;
    // This counter is to track paged pool memory allocation failures
    DriverContext.globalTelemetryInfo.ullMemAllocFailCount = 0;

    KeInitializeSpinLock(&DriverContext.vmCxSessionContext.CxSessionLock);
    // FS Flush notification
    DriverContext.ulNumFlushVolumeThreads = KeQueryActiveProcessorCount(NULL);
    DriverContext.liFsFlushTimeout.QuadPart = MIN_FS_FLUSH_TIMEOUT_IN_SEC;

    DriverContext.bTooManyLastChanceWritesHit = false;

    DriverContext.SystemStateFlags = 0;
    KeInitializeMutex(&DriverContext.SystemStateRegistryMutex, 0);

    InitializeListHead(&DriverContext.VolumeNameList);
    KeInitializeEvent(&DriverContext.StartFSFlushEvent, NotificationEvent, FALSE);
    KeInitializeEvent(&DriverContext.StopFSFlushEvent, NotificationEvent, FALSE);

    //Endianess Initialization
    DriverContext.bEndian = TestByteOrder();

    Status = IoGetBootDiskInformation(&bootDiskInformation, sizeof(bootDiskInformation));
    DriverContext.eDiskFilterMode = (STATUS_TOO_LATE == Status) ? NoRebootMode : RebootMode;

    Status = STATUS_SUCCESS;

    // Get OS version
    RTL_OSVERSIONINFOW osVersion;
    RtlZeroMemory(&osVersion, sizeof(RTL_OSVERSIONINFOW));

    osVersion.dwOSVersionInfoSize = sizeof(RTL_OSVERSIONINFOW);
    RtlGetVersion(&osVersion);

    DriverContext.osMajorVersion = osVersion.dwMajorVersion;
    DriverContext.osMinorVersion = osVersion.dwMinorVersion;

    // Windows 8/Windows 2012 has version 6.2
    DriverContext.IsWindows8AndLater = ((osVersion.dwMajorVersion > 6) || ((osVersion.dwMajorVersion == 6) && (osVersion.dwMinorVersion > 1)));

    //Ioctl spin lock initilization
    KeInitializeSpinLock(&DriverContext.MultiDeviceLock);
    DriverContext.DriverImage.MaximumLength = usLength + sizeof(WCHAR);
    DriverContext.DriverImage.Buffer = (PWCHAR)ExAllocatePoolWithTag(NonPagedPool,
                                            DriverContext.DriverImage.MaximumLength,
                                            TAG_GENERIC_NON_PAGED);
    if (NULL == DriverContext.DriverImage.Buffer)
    {
        ulLineNum = __LINE__;
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto InitializeDriverContextEnd;
    }
    RtlCopyUnicodeString(&DriverContext.DriverImage, RegistryPath);
    DriverContext.DriverImage.Buffer[DriverContext.DriverImage.Length/sizeof(WCHAR)] = L'\0';

    usLength = (USHORT)(RegistryPath->Length + (wcslen(PARAMETERS_KEY)*sizeof(WCHAR)));
    DriverContext.DriverParameters.MaximumLength = usLength + sizeof(WCHAR);
    DriverContext.DriverParameters.Buffer = (PWCHAR)ExAllocatePoolWithTag(NonPagedPool,
                                            DriverContext.DriverParameters.MaximumLength,
                                            TAG_GENERIC_NON_PAGED);
    if (NULL == DriverContext.DriverParameters.Buffer)
    {
        ulLineNum = __LINE__;
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto InitializeDriverContextEnd;
    }

    RtlCopyUnicodeString(&DriverContext.DriverParameters, RegistryPath);
    RtlAppendUnicodeToString(&DriverContext.DriverParameters, PARAMETERS_KEY);
    DriverContext.DriverImage.Buffer[DriverContext.DriverImage.Length/sizeof(WCHAR)] = L'\0';

    // Maximum Tag space in dirty block is UDIRTY_BLOCK_TAGS_SIZE - predefined tags.
    DriverContext.ulMaxTagBufferSize = UDIRTY_BLOCK_TAGS_SIZE - sizeof(STREAM_REC_HDR_4B) - 
                (FIELD_OFFSET(UDIRTY_BLOCK_V2, uTagList.TagList.TagEndOfList) - FIELD_OFFSET(UDIRTY_BLOCK_V2, uTagList.TagList.TagStartOfList));

// #define ALLOW_UNLOAD 1
#ifndef ALLOW_UNLOAD
#ifdef VOLUME_FLT
    RtlInitUnicodeString(&DeviceName, INMAGE_DEVICE_NAME);
    Status = IoCreateDevice(DriverObject, 
                            sizeof(DEVICE_EXTENSION), 
                            &DeviceName, 
                            FILE_DEVICE_UNKNOWN, 
                            FILE_DEVICE_SECURE_OPEN, 
                            FALSE, 
                            &DriverContext.ControlDevice);
    if (!NT_SUCCESS(Status))
    {
        ulLineNum = __LINE__;
        goto InitializeDriverContextEnd;
    }

    DriverContext.ulFlags |= DC_FLAGS_DEVICE_CREATED;

    IoRegisterShutdownNotification(DriverContext.ControlDevice);
    
    RtlZeroMemory(DriverContext.ControlDevice->DeviceExtension, sizeof(DEVICE_EXTENSION));

    RtlInitUnicodeString(&SymLinkName, INMAGE_SYMLINK_NAME);
    Status = IoCreateSymbolicLink(&SymLinkName, &DeviceName);
    if (!NT_SUCCESS(Status))
    {
        ulLineNum = __LINE__;
        goto InitializeDriverContextEnd;
    }

    DriverContext.ulFlags |= DC_FLAGS_SYMLINK_CREATED;
#else
    // TODO : DeviceType has to be set appropriately
    RtlInitUnicodeString(&DeviceName, INMAGE_DISK_FILTER_DEVICE_NAME);
    Status = IoCreateDevice(DriverObject, 
                            sizeof(DEVICE_EXTENSION), 
                            &DeviceName, 
                            FILE_DEVICE_UNKNOWN, 
                            FILE_DEVICE_SECURE_OPEN,
                            FALSE,
                            &DriverContext.ControlDevice);
    if (!NT_SUCCESS(Status))
    {
        ulLineNum = __LINE__;
        goto InitializeDriverContextEnd;
    }

    DriverContext.ulFlags |= DC_FLAGS_DEVICE_CREATED;

    Status = IoRegisterShutdownNotification(DriverContext.ControlDevice);
    if (!NT_SUCCESS(Status)) {
        InDskFltWriteEvent(INDSKFLT_ERROR_REGISTER_NOTIFICATION_FAILED, Status);
    }

    RtlZeroMemory(DriverContext.ControlDevice->DeviceExtension, sizeof(DEVICE_EXTENSION));

    RtlInitUnicodeString(&SymLinkName, INMAGE_DISK_FILTER_SYMLINK_NAME);
    Status = IoCreateSymbolicLink(&SymLinkName, &DeviceName);
    if (!NT_SUCCESS(Status))
    {
        ulLineNum = __LINE__;
        goto InitializeDriverContextEnd;
    }

    DriverContext.ulFlags |= DC_FLAGS_SYMLINK_CREATED;
    // Create device object to register for Post shutdown notification
    Status = IoCreateDevice(DriverObject, 
                            sizeof(DEVICE_EXTENSION),
                            NULL,
                            FILE_DEVICE_UNKNOWN,
                            FILE_DEVICE_SECURE_OPEN,
                            FALSE,
                            &DriverContext.PostShutdownDO);
    if (NT_SUCCESS(Status))
    {
        Status = IoRegisterLastChanceShutdownNotification(DriverContext.PostShutdownDO);
        if (!NT_SUCCESS(Status)) {
            InDskFltWriteEvent(INDSKFLT_ERROR_REGISTER_NOTIFICATION_FAILED, Status);
        }
        RtlZeroMemory(DriverContext.PostShutdownDO->DeviceExtension,
                      sizeof(DEVICE_EXTENSION));
        DriverContext.ulFlags |= DC_FLAGS_POST_SHUT_DEVICE_CREATED;
    } else { // Error case is not handled:- PostShutdown is for optimization
        InDskFltWriteEvent(INDSKFLT_ERROR_DEVICE_CREATION_FAILED, Status);
    }

#endif
#endif
    // initialize the driver global pool of dirty block structures
    ExInitializeNPagedLookasideList( &DriverContext.DirtyBlocksPool, NULL, NULL, 0, sizeof(KDIRTY_BLOCK), TAG_DIRTY_BLOCKS_POOL, 0 );
    DriverContext.ulFlags |= DC_FLAGS_DIRTYBLOCKS_POOL_INIT;
    ExInitializeNPagedLookasideList(&DriverContext.ChangeNodePool, NULL, NULL, 0, sizeof(CHANGE_NODE), TAG_CHANGE_NODE_POOL, 0);
    DriverContext.ulFlags |= DC_FLAGS_CHANGE_NODE_POOL_INIT;
    ExInitializeNPagedLookasideList(&DriverContext.ChangeBlock2KPool, NULL, NULL, 0, CB_SIZE_FOR_2K_CB, TAG_CHANGE_BLOCK_POOL, 0);
    DriverContext.ulFlags |= DC_FLAGS_CHANGE_BLOCK_2K_POOL_INIT;
    ExInitializeNPagedLookasideList( &DriverContext.BitmapWorkItemPool, NULL, NULL, 0, sizeof(BITMAP_WORK_ITEM), TAG_BITMAP_WORK_ITEM, 0);
    DriverContext.ulFlags |= DC_FLAGS_BITMAP_WORK_ITEM_POOL_INIT;
    ExInitializeNPagedLookasideList( &DriverContext.BitRunLengthOffsetPool, NULL, NULL, 0, sizeof(BitRunLengthOffsetArray), TAG_BITRUN_LENOFFSET_POOL, 0);
    DriverContext.ulFlags |= DC_FLAGS_BITRUN_LENOFFSET_ITEM_POOL_INIT;
    ExInitializeNPagedLookasideList( &DriverContext.WorkQueueEntriesPool, NULL, NULL, 0, sizeof(WORK_QUEUE_ENTRY), TAG_WORK_QUEUE_ENTRY, 0);
    DriverContext.ulFlags |= DC_FLAGS_WORKQUEUE_ENTRIES_POOL_INIT;
    ExInitializeNPagedLookasideList( &DriverContext.ListNodePool, NULL, NULL, 0, sizeof(LIST_NODE), TAG_NODE_LIST_POOL, 0);
    DriverContext.ulFlags |= DC_FLAGS_LIST_NODE_POOL_INIT;
    ExInitializeNPagedLookasideList(&DriverContext.CompletionContextPool, NULL, NULL, 0, sizeof(COMPLETION_CONTEXT), TAG_COMPLETION_CONTEXT_POOL, 0);
    DriverContext.ulFlags |= DC_FLAGS_COMPLETION_CONTEXT_POOL_INIT;
    DriverContext.ulNumberofAsyncTagIOCTLs = 0;
    Status = ParametersKey.OpenKeyReadWrite(&DriverContext.DriverParameters);
    if (!NT_SUCCESS(Status)) {
        // Failed to open the key, most probably there is no parameters key in registry.
        // Let me add the key.
        Status = ParametersKey.CreateKeyReadWrite(&DriverContext.DriverParameters);
    }

    if (NT_SUCCESS(Status)) {

        GetPersistentValuesFromRegistry(&ParametersKey);        

        ParametersKey.ReadULONG(DB_MAX_COALESCED_METADATA_CHANGE_SIZE, 
                                    (ULONG32 *) &DriverContext.MaxCoalescedMetaDataChangeSize,
                                    DEFAULT_MAX_COALESCED_METADATA_CHANGE_SIZE, TRUE);
        ParametersKey.ReadULONG(DB_VALIDATION_LEVEL, 
                                    (ULONG32 *) &DriverContext.ulValidationLevel,
                                    DEFAULT_VALIDATION_LEVEL, TRUE);
        ParametersKey.ReadULONG(DB_HIGH_WATERMARK_SERVICE_NOT_STARTED, 
                                    (ULONG32 *) &DriverContext.DBHighWaterMarks[ecServiceNotStarted],
                                    DEFAULT_DB_HIGH_WATERMARK_SERVICE_NOT_STARTED, TRUE );
        ParametersKey.ReadULONG(DB_LOW_WATERMARK_SERVICE_RUNNING,
                                    (ULONG32 *) &DriverContext.DBLowWaterMarkWhileServiceRunning,
                                    DEFAULT_DB_LOW_WATERMARK_SERVICE_RUNNING, TRUE);
        ParametersKey.ReadULONG(DB_HIGH_WATERMARK_SERVICE_RUNNING,
                                    (ULONG32 *) &DriverContext.DBHighWaterMarks[ecServiceRunning],
                                    DEFAULT_DB_HIGH_WATERMARK_SERVICE_RUNNING, TRUE);
        ParametersKey.ReadULONG(DB_HIGH_WATERMARK_SERVICE_SHUTDOWN,
                                    (ULONG32 *) &DriverContext.DBHighWaterMarks[ecServiceShutdown],
                                    DEFAULT_DB_HIGH_WATERMARK_SERVICE_SHUTDOWN, TRUE);
        ParametersKey.ReadULONG(DB_TO_PURGE_HIGH_WATERMARK_REACHED,
                                    (ULONG32 *) &DriverContext.DBToPurgeWhenHighWaterMarkIsReached,
                                    DEFAULT_DB_TO_PURGE_HIGH_WATERMARK_REACHED, TRUE);
        ParametersKey.ReadULONG(MAXIMUM_BITMAP_BUFFER_MEMORY,
                                    (ULONG32 *) &DriverContext.MaximumBitmapBufferMemory,
                                    DEFAULT_MAXIMUM_BITMAP_BUFFER_MEMORY, TRUE);
        ParametersKey.ReadULONG(BITMAP_512K_GRANULARITY_SIZE,
                                    (ULONG32 *) &DriverContext.Bitmap512KGranularitySize,
                                    DEFAULT_BITMAP_512K_GRANULARITY_SIZE, TRUE);

        ParametersKey.ReadULONG(NUM_FS_FLUSH_THREADS,
                                (ULONG32 *)&DriverContext.ulNumFlushVolumeThreads,
                                MIN_NUM_FS_FLUSH_THREADS, TRUE);

        ParametersKey.ReadULONG(RESDISK_DETECTION_TIME_MS,
                                (ULONG32 *)&DriverContext.ulMaxResDiskDetectionTimeInMS,
                                DEFAULT_RESDISK_DETECTION_TIME_IN_MS, TRUE);

        ParametersKey.ReadULONG(RESYNC_ON_REBOOT,
                               (ULONG32*)&ulResyncRequiredOnReboot);

        DriverContext.bResyncOnReboot = (0 != ulResyncRequiredOnReboot);

        if (MAX_NUM_FS_FLUSH_THREADS < DriverContext.ulNumFlushVolumeThreads) {
            DriverContext.ulNumFlushVolumeThreads = MAX_NUM_FS_FLUSH_THREADS;
        }

        // There should atleast one flush thread
        if (1 > DriverContext.ulNumFlushVolumeThreads) {
            DriverContext.ulNumFlushVolumeThreads = 1;
        }

        ParametersKey.ReadULONG(FS_FLUSH_TIMEOUT_IN_MS,
                                (ULONG32 *)&DriverContext.liFsFlushTimeout.QuadPart,
                                MIN_FS_FLUSH_TIMEOUT_IN_SEC, TRUE);

        if (MAX_FS_FLUSH_TIMEOUT_IN_SEC < DriverContext.liFsFlushTimeout.QuadPart) {
            DriverContext.liFsFlushTimeout.QuadPart = MAX_FS_FLUSH_TIMEOUT_IN_SEC;
        }

        if (MIN_FS_FLUSH_TIMEOUT_IN_SEC > DriverContext.liFsFlushTimeout.QuadPart) {
            DriverContext.liFsFlushTimeout.QuadPart = MIN_FS_FLUSH_TIMEOUT_IN_SEC;
        }
        // Registry keys related to data filtering
        ParametersKey.ReadULONG(DATA_BUFFER_SIZE,
                                    (ULONG32 *) &DriverContext.ulDataBufferSize,
                                    DEFAULT_DATA_BUFFER_SIZE,  TRUE);
        DriverContext.ulDataBufferSize *= KILOBYTES;
        ParametersKey.ReadULONG(DATA_BLOCK_SIZE,
                                    (ULONG32 *) &DriverContext.ulDataBlockSize,
                                    DEFAULT_DATA_BLOCK_SIZE, TRUE);
        DriverContext.ulDataBlockSize *= KILOBYTES;

        ParametersKey.ReadULONG(LOCKED_DATA_BLOCKS_FOR_USE_AT_DISPATCH_IRQL,
                                    (ULONG32 *) &DriverContext.ulMaxSetLockedDataBlocks,
                                    DEFAULT_MAX_LOCKED_DATA_BLOCKS, TRUE);
        ParametersKey.ReadULONG(MAX_DATA_SIZE_PER_DATA_MODE_DIRTY_BLOCK,
                                    (ULONG32 *)&DriverContext.ulMaxDataSizePerDataModeDirtyBlock,
                                    DEFAULT_MAX_DATA_SIZE_PER_DATA_MODE_DIRTY_BLOCK, TRUE);
        DriverContext.ulMaxDataSizePerDataModeDirtyBlock *= KILOBYTES;
        ParametersKey.ReadULONG(MAX_DATA_SIZE_PER_NON_DATA_MODE_DIRTY_BLOCK,
                                    (ULONG32 *)&DriverContext.ulMaxDataSizePerNonDataModeDirtyBlock,
                                    DEFAULT_MAX_DATA_SIZE_PER_NON_DATA_MODE_DIRTY_BLOCK, TRUE);
        DriverContext.ulMaxDataSizePerNonDataModeDirtyBlock *= KILOBYTES;
        ParametersKey.ReadULONG(MAX_WAIT_TIME_FOR_IRP_COMPLETION_IN_MINUTES,
                                    (ULONG32 *)&DriverContext.ulMaxWaitTimeForIrpCompletion,
                                    DEFAULT_MAX_WAIT_TIME_FOR_IRP_COMPLETION_IN_MINUTES, TRUE);  
        ParametersKey.ReadULONG(FREE_THRESHOLD_FOR_FILE_WRITE,
                                    (ULONG32 *)&DriverContext.ulDaBFreeThresholdForFileWrite,
                                    DEFAULT_FREE_THREASHOLD_FOR_FILE_WRITE, TRUE);

        ParametersKey.ReadULONG(DEVICE_DATAMODE_CAPTURE,
                                    (ULONG32 *) &ulDevDataModeCapture,
                                    DEFAULT_DEVICE_DATAMODE_CAPTURE, TRUE);
        // Following key is set globally
        ParametersKey.ReadULONG(DATAMODE_CAPTURE_FOR_NEW_DEVICES,
                                    (ULONG32 *)&ulDataModeCaptureForNewDevices,
                                    DEFAULT_DATAMODE_CAPTURE_FOR_NEW_DEVICES, TRUE);
        ParametersKey.ReadULONG(DEVICE_DATA_FILES,
                                    (ULONG32 *) &ulDataFiles,
                                    DEFAULT_DEVICE_DATA_FILES, TRUE);
        ParametersKey.ReadULONG(DATA_FILES_FOR_NEW_DEVICES,
                                    (ULONG32 *)&ulDataFilesForNewDevs,
                                    DEFAULT_DATA_FILES_FOR_NEW_DEVICES, TRUE);

        // This is used to set the limit on the disk size used to store data files for every device
        ParametersKey.ReadULONG(DEVICE_DATA_TO_DISK_LIMIT_IN_MB,
                                    (ULONG32 *)&DriverContext.ulDataToDiskLimitPerDevInMB,
                                    DEFAULT_DEVICE_DATA_TO_DISK_LIMIT_IN_MB, TRUE);
        ParametersKey.ReadULONG(DEVICE_MIN_FREE_DATA_TO_DISK_LIMIT_IN_MB,
                                    (ULONG32 *)&DriverContext.ulMinFreeDataToDiskLimitPerDevInMB,
                                    DEFAULT_DEVICE_MIN_FREE_DATA_TO_DISK_LIMIT_IN_MB, TRUE);
        ParametersKey.ReadULONG(DEVICE_DATA_NOTIFY_LIMIT_IN_KB,
                                    (ULONG32 *)&DriverContext.ulDataNotifyThresholdInKB,
            DEFAULT_DEVICE_DATA_NOTIFY_LIMIT_IN_KB, TRUE);
        ParametersKey.ReadULONG(DEVICE_FILE_WRITER_THREAD_PRIORITY,
                                   (ULONG32 *)&DriverContext.FileWriterThreadPriority,
                                   DEFAULT_DEVICE_FILE_WRITERS_THREAD_PRIORITY, TRUE);
        ParametersKey.ReadULONG(DEVICE_THRESHOLD_FOR_FILE_WRITE,
                                   (ULONG32 *)&DriverContext.ulDaBThresholdPerDevForFileWrite,
                                   DEFAULT_DEVICE_THRESHOLD_FOR_FILE_WRITE, TRUE);
        ParametersKey.ReadULONG(DISABLE_FILTERING_FOR_NEW_DEVICES,
                                   (ULONG32 *)&ulFiltDisabledForNewDevice,
                                   DEFAULT_DISABLE_FILTERING_FOR_NEW_DEVICES, TRUE);
#ifdef VOLUME_CLUSTER_SUPPORT
        ParametersKey.ReadULONG(DISABLE_FILTERING_FOR_NEW_CLUSTER_DEVICES,
            (ULONG32 *)&ulFiltDisabledForNewClusterDevice,
            DEFAULT_FILTERING_DISABLED_FOR_NEW_CLUSTER_DEVICES, TRUE);
#endif

        ParametersKey.ReadULONG(WORKER_THREAD_PRIORITY,
                                    (ULONG32 *)&ulWorkerThreadPriority,
                                    DEFAULT_WORKER_THREAD_PRIORITY, TRUE);
        ParametersKey.ReadULONG(MAX_NON_PAGED_POOL_IN_MB, 
                                    (ULONG32 *)&DriverContext.ulMaxNonPagedPool,
                                    DEFAULT_MAX_NON_PAGED_POOL_IN_MB, TRUE);
        ParametersKey.ReadULONG(DATA_ALLOCATION_LIMIT_IN_PERCENTAGE, 
                                    (ULONG32 *)&DriverContext.ulMaxDataAllocLimitInPercentage,
                                    DEFAULT_MAX_DATA_ALLOCATION_LIMIT, TRUE);
        ParametersKey.ReadULONG(FREE_AND_LOCKED_DATA_BLOCKS_COUNT_IN_PERCENTAGE, 
                                    (ULONG32 *)&DriverContext.ulAvailableDataBlockCountInPercentage,
                                    DEFAULT_FREE_AND_LOCKED_DATA_BLOCKS_COUNT, TRUE);

        ParametersKey.ReadUnicodeString(DEFAULT_LOG_DIRECTORY, STRING_LEN_NULL_TERMINATED, 
                                        &DriverContext.DefaultLogDirectory, DEFAULT_LOG_DIRECTORY_VALUE, true, PagedPool);

        ParametersKey.ReadUnicodeString(DATA_LOG_DIRECTORY, STRING_LEN_NULL_TERMINATED, 
                                        &DriverContext.DataLogDirectory, (PCWSTR)DriverContext.DefaultLogDirectory.Buffer, true, NonPagedPool);
        
        Status = ParametersKey.ReadULONG(NUMBER_OF_THREADS_PER_FILE_WRITER, (ULONG32 *)&DriverContext.ulNumberOfThreadsPerFileWriter, 
                                        DEFAULT_NUMBER_OF_THREADS_FOR_EACH_FILE_WRITER, TRUE);
        if (!NT_SUCCESS(Status)) {
            InVolDbgPrint(DL_WARNING, DM_REGISTRY, 
                          ("%s: ReadULONG for %S has failed\n", __FUNCTION__, NUMBER_OF_THREADS_PER_FILE_WRITER));
        }

        Status = ParametersKey.ReadULONG( NUMBER_OF_COMMON_FILE_WRITERS, (ULONG32 *)&DriverContext.ulNumberOfCommonFileWriters, 
                                        DEFAULT_NUMBER_OF_COMMON_FILE_WRITERS, TRUE);
        if (!NT_SUCCESS(Status)) {
            InVolDbgPrint(DL_WARNING, DM_REGISTRY, 
                          ("%s: ReadULONG for %S has failed\n", __FUNCTION__, NUMBER_OF_COMMON_FILE_WRITERS));
        }

        Status = ParametersKey.ReadULONG( TIME_IN_SECONDS_BETWEEN_NO_MEMORY_LOG_EVENTS, (ULONG32 *)&DriverContext.ulSecondsBetweenNoMemoryLogEvents, 
                                        DEFAULT_TIME_IN_SECONDS_BETWEEN_NO_MEMORY_LOG_EVENTS, TRUE);
        if (!NT_SUCCESS(Status)) {
            InVolDbgPrint(DL_WARNING, DM_REGISTRY, 
                          ("%s: ReadULONG for %S has failed\n", __FUNCTION__, TIME_IN_SECONDS_BETWEEN_NO_MEMORY_LOG_EVENTS));
        }

        Status = ParametersKey.ReadULONG( MAX_WAIT_TIME_IN_SECONDS_ON_FAIL_OVER, (ULONG32 *)&DriverContext.ulSecondsMaxWaitTimeOnFailOver, 
                                        DEFAULT_MAX_TIME_IN_SECONDS_ON_FAIL_OVER, TRUE);
        if (!NT_SUCCESS(Status)) {
            InVolDbgPrint(DL_WARNING, DM_REGISTRY, 
                          ("%s: ReadULONG for %S has failed\n", __FUNCTION__, MAX_WAIT_TIME_IN_SECONDS_ON_FAIL_OVER));
        }

        //These tags would help in debugging WO state transitions and generated by driver
        // These tags are by default disabled through registry and not maintaining for them years.
        // Kept them as it is to reuse later
        ParametersKey.ReadULONG(KERNEL_TAGS_ENABLED,
                                    (ULONG32 *)&ulKernelTagsEnabled,
                                    DEFAULT_KERNEL_TAGS_ENABLED, TRUE);
        if (ulKernelTagsEnabled) {
            DriverContext.bKernelTagsEnabled = TRUE;
        }

        Status = ParametersKey.OpenSubKey(KERNEL_TAGS_KEY, STRING_LEN_NULL_TERMINATED, KernelTagsKey, true);

        KernelTagsKey.ReadAString(MODE_CHANGE_TAG_DESC_FOR_BITMAP_TO_DATA, STRING_LEN_NULL_TERMINATED, &DriverContext.BitmapToDataModeTag, 
                                  MODE_CHANGE_TAG_DESC_FOR_BITMAP_TO_DATA_DEFAULT_VALUE, TRUE, NonPagedPool);
        DriverContext.usBitmapToDataModeTagLen = ReturnStringLenAInUSHORT(DriverContext.BitmapToDataModeTag);
        KernelTagsKey.ReadAString(MODE_CHANGE_TAG_DESC_FOR_BITMAP_TO_METADATA, STRING_LEN_NULL_TERMINATED, &DriverContext.BitmapToMetaDataModeTag, 
                                  MODE_CHANGE_TAG_DESC_FOR_BITMAP_TO_METADATA_DEFAULT_VALUE, TRUE, NonPagedPool);
        DriverContext.usBitmapToMetaDataModeTagLen = ReturnStringLenAInUSHORT(DriverContext.BitmapToMetaDataModeTag);
        KernelTagsKey.ReadAString(MODE_CHANGE_TAG_DESC_FOR_DATA_TO_METADATA, STRING_LEN_NULL_TERMINATED, &DriverContext.DataToMetaDataModeTag, 
                                  MODE_CHANGE_TAG_DESC_FOR_DATA_TO_METADATA_DEFAULT_VALUE, TRUE, NonPagedPool);
        DriverContext.usDataToMetaDataModeTagLen = ReturnStringLenAInUSHORT(DriverContext.DataToMetaDataModeTag);
        KernelTagsKey.ReadAString(MODE_CHANGE_TAG_DESC_FOR_METADATA_TO_DATA, STRING_LEN_NULL_TERMINATED, &DriverContext.MetaDataToDataModeTag, 
                                  MODE_CHANGE_TAG_DESC_FOR_METADATA_TO_DATA_DEFAULT_VALUE, TRUE, NonPagedPool);
        DriverContext.usMetaDataToDataModeTagLen = ReturnStringLenAInUSHORT(DriverContext.MetaDataToDataModeTag);

        Status = ParametersKey.ReadULONG(REBOOT_COUNTER, (ULONG32 *)&ulBootCounter, DEFAULT_BOOT_COUNTER, TRUE);
        if ((STATUS_INMAGE_OBJECT_CREATED == Status) && (DriverContext.eDiskFilterMode == RebootMode)) {
            ulBootCounter = 1;
        }

        Status = ParametersKey.ReadULONG(FS_FLUSH_PRE_SHUTDOWN,
                                         (ULONG32 *)&ulFsFlushPreShutdown,
                                         DEFAULT_FS_FLUSH_PRE_SHUTDOWN,
                                         TRUE);

        if (NT_SUCCESS(Status)) {
            DriverContext.bFlushFsPreShutdown = (0 != ulFsFlushPreShutdown);
        }

        ul32Value = 0;
        ParametersKey.ReadULONG(TOO_MANY_LAST_CHANCE_WRITES_HIT, &ul32Value, 0);
        DriverContext.bTooManyLastChanceWritesHit = (ul32Value != 0);

        ul32Value = 0;
        ParametersKey.ReadULONG(SYSTEM_STATE_FLAGS, &ul32Value, 0);
        DriverContext.SystemStateFlags = ul32Value;

        DriverContext.ulBootCounter = ulBootCounter;
        ulBootCounter++;
        ParametersKey.WriteULONG(REBOOT_COUNTER, ulBootCounter);
        InDskFltWriteEvent(INDSKFLT_INFO_REBOOT_COUNTER, DriverContext.ulBootCounter);

        ParametersKey.FlushKey();
        ParametersKey.CloseKey();
        KernelTagsKey.CloseKey();
    }
    else {
        InDskFltWriteEvent(INDSKFLT_ERROR_REGISTRY_OPERATION_FAILED, Status);
        DriverContext.DBHighWaterMarks[ecServiceNotStarted] = DEFAULT_DB_HIGH_WATERMARK_SERVICE_NOT_STARTED;
        DriverContext.DBLowWaterMarkWhileServiceRunning = DEFAULT_DB_LOW_WATERMARK_SERVICE_RUNNING;
        DriverContext.DBHighWaterMarks[ecServiceRunning] = DEFAULT_DB_HIGH_WATERMARK_SERVICE_RUNNING;
        DriverContext.DBHighWaterMarks[ecServiceShutdown] = DEFAULT_DB_HIGH_WATERMARK_SERVICE_SHUTDOWN;
        DriverContext.DBToPurgeWhenHighWaterMarkIsReached = DEFAULT_DB_TO_PURGE_HIGH_WATERMARK_REACHED;
        DriverContext.MaximumBitmapBufferMemory = DEFAULT_MAXIMUM_BITMAP_BUFFER_MEMORY;
        ulDevDataModeCapture = DEFAULT_DEVICE_DATAMODE_CAPTURE;
        ulDataModeCaptureForNewDevices = DEFAULT_DATAMODE_CAPTURE_FOR_NEW_DEVICES;
        DriverContext.FileWriterThreadPriority = DEFAULT_DEVICE_FILE_WRITERS_THREAD_PRIORITY;
        DriverContext.ulDataToDiskLimitPerDevInMB = DEFAULT_DEVICE_DATA_TO_DISK_LIMIT_IN_MB;
        DriverContext.ulMinFreeDataToDiskLimitPerDevInMB = DEFAULT_DEVICE_MIN_FREE_DATA_TO_DISK_LIMIT_IN_MB;
        DriverContext.ulDataNotifyThresholdInKB = DEFAULT_DEVICE_DATA_NOTIFY_LIMIT_IN_KB;
        DriverContext.ulNumberOfCommonFileWriters = DEFAULT_NUMBER_OF_COMMON_FILE_WRITERS;
        DriverContext.ulNumberOfThreadsPerFileWriter = DEFAULT_NUMBER_OF_THREADS_FOR_EACH_FILE_WRITER;
        DriverContext.ulSecondsBetweenNoMemoryLogEvents = DEFAULT_TIME_IN_SECONDS_BETWEEN_NO_MEMORY_LOG_EVENTS;
        DriverContext.ulDaBFreeThresholdForFileWrite = DEFAULT_FREE_THREASHOLD_FOR_FILE_WRITE;
        DriverContext.ulDaBThresholdPerDevForFileWrite = DEFAULT_DEVICE_THRESHOLD_FOR_FILE_WRITE;
        DriverContext.ulSecondsMaxWaitTimeOnFailOver = DEFAULT_MAX_TIME_IN_SECONDS_ON_FAIL_OVER;
        ulFiltDisabledForNewDevice = DEFAULT_DISABLE_FILTERING_FOR_NEW_DEVICES;
#ifdef VOLUME_CLUSTER_SUPPORT
        ulFiltDisabledForNewClusterDevice = DEFAULT_FILTERING_DISABLED_FOR_NEW_CLUSTER_DEVICES;
        if (ulFiltDisabledForNewClusterDevice) {
            DriverContext.bDisableVolumeFilteringForNewClusterVolumes = true;
        }
#endif
        ulWorkerThreadPriority = DEFAULT_WORKER_THREAD_PRIORITY;
        DriverContext.ulMaxNonPagedPool = DEFAULT_MAX_NON_PAGED_POOL_IN_MB;
        DriverContext.ulMaxWaitTimeForIrpCompletion = DEFAULT_MAX_WAIT_TIME_FOR_IRP_COMPLETION_IN_MINUTES;
        DriverContext.ulMaxDataAllocLimitInPercentage = DEFAULT_MAX_DATA_ALLOCATION_LIMIT;
        DriverContext.ulAvailableDataBlockCountInPercentage = DEFAULT_FREE_AND_LOCKED_DATA_BLOCKS_COUNT;
        DriverContext.MaxCoalescedMetaDataChangeSize = DEFAULT_MAX_COALESCED_METADATA_CHANGE_SIZE;
        DriverContext.ulValidationLevel = DEFAULT_VALIDATION_LEVEL;
        DriverContext.Bitmap512KGranularitySize = DEFAULT_BITMAP_512K_GRANULARITY_SIZE;
        DriverContext.bKernelTagsEnabled = DEFAULT_KERNEL_TAGS_ENABLED;
    }

    if(DriverContext.ulMinFreeDataToDiskLimitPerDevInMB > DriverContext.ulDataToDiskLimitPerDevInMB){
        DriverContext.ulMinFreeDataToDiskLimitPerDevInMB = DriverContext.ulDataToDiskLimitPerDevInMB;
    }

    if (ulFiltDisabledForNewDevice) {
        DriverContext.bDisableFilteringForNewDevice = true;
    }

    if (DriverContext.FileWriterThreadPriority < MINIMUM_DEVICE_FILE_WRITERS_THREAD_PRIORITY) {
        DriverContext.FileWriterThreadPriority = MINIMUM_DEVICE_FILE_WRITERS_THREAD_PRIORITY;
    }

    if (0 == DriverContext.DBToPurgeWhenHighWaterMarkIsReached) {
        DriverContext.DBToPurgeWhenHighWaterMarkIsReached = DEFAULT_DB_TO_PURGE_HIGH_WATERMARK_REACHED;
    }

    if (ulDevDataModeCapture) {
        DriverContext.bEnableDataCapture = true;
    } else {
        DriverContext.bEnableDataCapture = false;
    }

    if (ulDataModeCaptureForNewDevices) {
        DriverContext.bEnableDataModeCaptureForNewDevices = TRUE;
    }
    else {
        DriverContext.bEnableDataModeCaptureForNewDevices = FALSE;
    }

    if (ulDataFiles) {
        DriverContext.bEnableDataFiles = true;
    } else {
        DriverContext.bEnableDataFiles = false;
    }

    if (ulDataFilesForNewDevs) {
        DriverContext.bEnableDataFilesForNewDevs = true;
    } else {
        DriverContext.bEnableDataFilesForNewDevs = false;
    }

    DriverContext.bServiceSupportsDataCapture = false;
    DriverContext.bServiceSupportsDataFiles = false;
    DriverContext.bS2SupportsDataFiles = false;
    DriverContext.bS2is64BitProcess = false;
    DriverContext.ulMaxNonPagedPool *= MEGABYTES;
    DriverContext.ulMaxWaitTimeForIrpCompletion *= SECONDS_IN_MINUTE;   

#ifndef VOLUME_FLT
    Status = AllocateGlobalPageFileNodes();
    if (!NT_SUCCESS(Status)) {
        InDskFltWriteEventWithDevCtxt(INDSKFLT_ERROR_NO_MEMORY, (PDEV_CONTEXT)NULL, SourceLocationInitDrvContext);
        Status = STATUS_SUCCESS;
        // Error case not handled as driver can function in Metadata mode
    }
#endif

    Status = InitializeWorkQueue(&DriverContext.WorkItemQueue, NULL, ulWorkerThreadPriority);
    if (!NT_SUCCESS(Status))
    {
        ulLineNum = __LINE__;
        goto InitializeDriverContextEnd;

    }

    Status = AllocateAndInitializeDataPool();
    if (!NT_SUCCESS(Status)) {
        Status = STATUS_SUCCESS;
    }
    
    Status = SetSystemTimeChangeNotification();
    if (!NT_SUCCESS(Status)) {
        InDskFltWriteEvent(INDSKFLT_ERROR_REGISTER_NOTIFICATION_FAILED, Status);
        //Keep the functionality same
        Status = STATUS_SUCCESS;
    }

    Status = InDskFltCreateFlushVolumeCachesThread();
    if (!NT_SUCCESS(Status)) {
        InDskFltWriteEvent(INDSKFLT_ERROR_FLUSH_VOL_THREAD_CREATION_FAILED, Status);
        Status = STATUS_SUCCESS;
    }

    DriverContext.pTagInfo = NULL;

    // Initializations for App Tag Protocol
    KeInitializeTimer(&DriverContext.AppTagProtocolTimer);

    KDEFERRED_ROUTINE InDskFltAppTagProtocolDPC;
    KeInitializeDpc(&DriverContext.AppTagProtocolTimerDpc, InDskFltAppTagProtocolDPC, &DriverContext);

    DriverContext.TagProtocolState = ecTagNoneInProgress;

    DriverContext.ullDrvProdVer = (INMAGE_PRODUCT_VERSION_MAJOR * 1000000000000ULL) + (INMAGE_PRODUCT_VERSION_MINOR * 100000000ULL) + (INMAGE_PRODUCT_VERSION_BUILDNUM * 10000ULL) + INMAGE_PRODUCT_VERSION_PRIVATE;

    DriverContext.vmCxSessionContext.ullDiskDiffThroughputThresoldInBytes = DISK_CXSESSION_DIFFTHROUGHPUT_THRESOLD;
    DriverContext.vmCxSessionContext.ullSessionId = DEFAULT_CXSESSION_START_ID;
    DriverContext.vmCxSessionContext.ullLastUserTransactionId = DEFAULT_CXSESSION_USER_TRANSID;
    DriverContext.vmCxSessionContext.ullMaxAccpetableTimeJumpFWDInMS = DEFAULT_PREMISSIBLE_TIMEJMP_FWD_MS;
    DriverContext.vmCxSessionContext.ullMaxAccpetableTimeJumpBKWDInMS = DEFAULT_PREMISSIBLE_TIMEJMP_FWD_MS;

    InDskFltHandleSanAndNoHydWF();

    return Status;
InitializeDriverContextEnd:
    InDskFltWriteEvent(INDSKFLT_ERROR_DRIVER_CONTEXT_INIT_FAILED, Status);
    CleanDriverContext();
    return Status;
}

NTSTATUS
SetSystemTimeChangeNotification()
{
    PCALLBACK_OBJECT    SystemTimeCallbackObject;
    OBJECT_ATTRIBUTES TimeObjectAttributes;
    UNICODE_STRING TimerName;
    NTSTATUS       Status;

    RtlInitUnicodeString(&TimerName, L"\\Callback\\SetSystemTime");
    InitializeObjectAttributes(&TimeObjectAttributes, &TimerName, OBJ_KERNEL_HANDLE, NULL, NULL);

    Status = ExCreateCallback(&SystemTimeCallbackObject, &TimeObjectAttributes, FALSE, FALSE);
    if (NT_SUCCESS(Status)) {
        DriverContext.SystemTimeCallbackPointer = 
                      ExRegisterCallback(SystemTimeCallbackObject, 
                                         SystemTimeChangeNotification, 
                                         NULL);		
        ObDereferenceObject(SystemTimeCallbackObject);
    }
    return Status;
}

VOID
SystemTimeChangeNotification(
    IN PVOID CallbackContext,
    IN PVOID Argument1,
    IN PVOID Argument2
    )
{
    LARGE_INTEGER  CurrentTime;
    LONGLONG LastTime;
    UNREFERENCED_PARAMETER(CallbackContext);
    UNREFERENCED_PARAMETER(Argument1);
    UNREFERENCED_PARAMETER(Argument2);

    LastTime = DriverContext.ullLastTimeStamp;
    KeQuerySystemTime(&CurrentTime);

    if(LastTime > CurrentTime.QuadPart) {
        InVolDbgPrint(DL_WARNING, DM_ALL, ("SystemTimeChangeNotification: Clock is set back in time\n"));
    }
}

VOID
CleanDriverContext()
{
    if(DriverContext.SystemTimeCallbackPointer) {
        ExUnregisterCallback(DriverContext.SystemTimeCallbackPointer);
        DriverContext.SystemTimeCallbackPointer = NULL;
    }

    if (NULL != DriverContext.WorkItemQueue.ThreadObject) {
        CleanWorkQueue(&DriverContext.WorkItemQueue);
    }

    if (DriverContext.ulFlags & DC_FLAGS_LIST_NODE_POOL_INIT) {
        DriverContext.ulFlags &= ~DC_FLAGS_LIST_NODE_POOL_INIT;
        ExDeleteNPagedLookasideList( &DriverContext.ListNodePool );
    }

    if (DriverContext.ulFlags & DC_FLAGS_WORKQUEUE_ENTRIES_POOL_INIT) {
        DriverContext.ulFlags &= ~DC_FLAGS_WORKQUEUE_ENTRIES_POOL_INIT;
        ExDeleteNPagedLookasideList( &DriverContext.WorkQueueEntriesPool);
    }

    if (DriverContext.ulFlags & DC_FLAGS_BITMAP_WORK_ITEM_POOL_INIT) {
        DriverContext.ulFlags &= ~DC_FLAGS_BITMAP_WORK_ITEM_POOL_INIT;
        ExDeleteNPagedLookasideList( &DriverContext.BitmapWorkItemPool);
    }

    if (DriverContext.ulFlags & DC_FLAGS_BITRUN_LENOFFSET_ITEM_POOL_INIT) {
        DriverContext.ulFlags &= ~DC_FLAGS_BITRUN_LENOFFSET_ITEM_POOL_INIT;
        ExDeleteNPagedLookasideList( &DriverContext.BitRunLengthOffsetPool);
    }

    if (DriverContext.ulFlags & DC_FLAGS_CHANGE_NODE_POOL_INIT) {
        DriverContext.ulFlags &= ~DC_FLAGS_CHANGE_NODE_POOL_INIT;
        ExDeleteNPagedLookasideList(&DriverContext.ChangeNodePool);
    }

    if (DriverContext.ulFlags & DC_FLAGS_CHANGE_BLOCK_2K_POOL_INIT) {
        DriverContext.ulFlags &= ~DC_FLAGS_CHANGE_BLOCK_2K_POOL_INIT;
        ExDeleteNPagedLookasideList(&DriverContext.ChangeBlock2KPool);
    }

    if (DriverContext.ulFlags & DC_FLAGS_DIRTYBLOCKS_POOL_INIT) {
        DriverContext.ulFlags &= ~DC_FLAGS_DIRTYBLOCKS_POOL_INIT;
        ExDeleteNPagedLookasideList( &DriverContext.DirtyBlocksPool);
    }

    if (DriverContext.ulFlags & DC_FLAGS_COMPLETION_CONTEXT_POOL_INIT) {
        DriverContext.ulFlags &= ~DC_FLAGS_COMPLETION_CONTEXT_POOL_INIT;
        ExDeleteNPagedLookasideList(&DriverContext.CompletionContextPool);
    }

    FreeGlobalPageFileNodes();

    if (DriverContext.ulFlags & DC_FLAGS_SYMLINK_CREATED) {
        UNICODE_STRING          SymLinkName;

#ifdef VOLUME_FLT
        RtlInitUnicodeString(&SymLinkName, INMAGE_SYMLINK_NAME);
#else
        RtlInitUnicodeString(&SymLinkName, INMAGE_DISK_FILTER_SYMLINK_NAME);
#endif
        DriverContext.ulFlags &= ~DC_FLAGS_SYMLINK_CREATED;
        IoDeleteSymbolicLink(&SymLinkName);
    }

    if (DriverContext.ulFlags & DC_FLAGS_DEVICE_CREATED) {
        DriverContext.ulFlags &= ~DC_FLAGS_DEVICE_CREATED;
        IoUnregisterShutdownNotification(DriverContext.ControlDevice);
        IoDeleteDevice(DriverContext.ControlDevice);
    }

    if (DriverContext.ulFlags & DC_FLAGS_POST_SHUT_DEVICE_CREATED) {
        DriverContext.ulFlags &= ~DC_FLAGS_POST_SHUT_DEVICE_CREATED;
        IoUnregisterShutdownNotification(DriverContext.PostShutdownDO);
        IoDeleteDevice(DriverContext.PostShutdownDO);
    }

    if (NULL != DriverContext.DriverParameters.Buffer) {
        ExFreePoolWithTag(DriverContext.DriverParameters.Buffer, TAG_GENERIC_NON_PAGED);
        DriverContext.DriverParameters.Buffer = NULL;
    }
    DriverContext.DriverParameters.Length = DriverContext.DriverParameters.MaximumLength = 0;

    if (NULL != DriverContext.DriverImage.Buffer) {
        ExFreePoolWithTag(DriverContext.DriverImage.Buffer, TAG_GENERIC_NON_PAGED);
        DriverContext.DriverImage.Buffer = NULL;
    }
    DriverContext.DriverImage.Length = DriverContext.DriverImage.MaximumLength = 0;

    if (DriverContext.ulFlags & DC_FLAGS_DATA_POOL_VARIABLES_INTIALIZED) {
        CleanDataPool();
    }

    CleanupFlushVolumeNodes();

    SafeExFreePoolWithTag(DriverContext.pFlushVolumeCacheThreads, TAG_FS_FLUSH);
    SafeExFreePoolWithTag(DriverContext.hFlushThreadTerminated, TAG_FS_FLUSH);

    SafeExFreePoolWithTag(DriverContext.pProtectedDiskNumbers, TAG_FS_FLUSH);

    SafeFreeUnicodeString(DriverContext.bootDiskInfo.usVendorId);
    SafeFreeUnicodeString(DriverContext.prevBootDiskInfo.usVendorId);

    return;
}

NTSTATUS
OpenDriverParametersRegKey(Registry **ppDriverParam)
{
    Registry        *pDriverParam;
    NTSTATUS        Status;

    PAGED_CODE();

    pDriverParam = new Registry();
    if (NULL == pDriverParam) {
        InVolDbgPrint(DL_ERROR, DM_REGISTRY, 
            ("%s: Failed in creating registry object\n", __FUNCTION__));
        return STATUS_NO_MEMORY;
    }

    Status = pDriverParam->OpenKeyReadWrite(&DriverContext.DriverParameters);
    if (!NT_SUCCESS(Status)) {
        Status = pDriverParam->CreateKeyReadWrite(&DriverContext.DriverParameters);
        if (!NT_SUCCESS(Status)) {
            InVolDbgPrint(DL_ERROR, DM_REGISTRY, 
                ("%s: Failed to Creat reg key %wZ with Status %#x", __FUNCTION__,
                 &DriverContext.DriverParameters, Status));

            delete pDriverParam;
            pDriverParam = NULL;
        } else {
            InVolDbgPrint(DL_INFO, DM_REGISTRY, 
                ("%s: Created reg key %wZ", __FUNCTION__, &DriverContext.DriverParameters));
        }
    } else {
        InVolDbgPrint(DL_INFO, DM_DEV_CONTEXT, 
            ("%s: Opened reg key %wZ", __FUNCTION__, &DriverContext.DriverParameters));
    }

    *ppDriverParam = pDriverParam;
    return Status;
}

NTSTATUS
SetStrInParameters(PWCHAR pValueName, PUNICODE_STRING pValue)
{
    NTSTATUS    Status = STATUS_SUCCESS;

    Registry    *pDriverParam = NULL;

    ASSERT(NULL != pValueName);
    PAGED_CODE();

    if (NULL == pValueName) {
        InVolDbgPrint(DL_ERROR, DM_REGISTRY, ("%s: pValueName NULL\n", __FUNCTION__));
        return STATUS_INVALID_PARAMETER_1;
    }

    if ((NULL == pValue) || (NULL == pValue->Buffer) ) {
        InVolDbgPrint(DL_ERROR, DM_REGISTRY, ("%s: pValue or pValue->Buffer is NULL\n", __FUNCTION__));
        return STATUS_INVALID_PARAMETER_2;
    }    

    InVolDbgPrint(DL_FUNC_TRACE, DM_REGISTRY, 
        ("%s: Called - pValueName(%S) Value %wZ\n", __FUNCTION__, pValueName, pValue));

    // Let us open/create the registry key for this volume.
    Status = OpenDriverParametersRegKey(&pDriverParam);
    if (NULL == pDriverParam) {
        InVolDbgPrint(DL_ERROR, DM_REGISTRY, 
                ("%s: Failed to open driver parameters key", __FUNCTION__));
        return Status;
    }

    Status = pDriverParam->WriteUnicodeString(pValueName, pValue);
    if (NT_SUCCESS(Status)) {
        InVolDbgPrint(DL_INFO, DM_REGISTRY, 
            ("%s: Added %S with value %wZ\n", __FUNCTION__, pValueName, pValue));
    } else {
        InVolDbgPrint(DL_ERROR, DM_REGISTRY, 
            ("%s: Adding %S with value %wZ Failed with Status 0x%x\n", __FUNCTION__,
                                    pValueName, pValue, Status));
    }

    delete pDriverParam;
 
    return Status;
}

NTSTATUS
SetDWORDInParameters(PWCHAR pValueName, ULONG ulValue)
{
    NTSTATUS    Status = STATUS_SUCCESS;
    Registry    *pDriverParam = NULL;

    ASSERT(NULL != pValueName);
    PAGED_CODE();

    if (NULL == pValueName) {
        InVolDbgPrint(DL_ERROR, DM_REGISTRY, ("%s: pValueName NULL\n", __FUNCTION__));
        return STATUS_INVALID_PARAMETER_1;
    }

    InVolDbgPrint(DL_FUNC_TRACE, DM_REGISTRY, 
        ("%s: Called - pValueName(%S) ulValue(%#x)\n", __FUNCTION__, pValueName, ulValue));

    // Let us open/create the registry key for this volume.
    Status = OpenDriverParametersRegKey(&pDriverParam);
    if (NULL == pDriverParam) {
        InVolDbgPrint(DL_ERROR, DM_REGISTRY, 
                ("%s: Failed to open driver parameters key", __FUNCTION__));
        return Status;
    }
    
    Status = pDriverParam->WriteULONG(pValueName, ulValue);
    if (NT_SUCCESS(Status)) {
        InVolDbgPrint(DL_INFO, DM_REGISTRY, 
            ("%s: Added %S with value %#x\n", __FUNCTION__, pValueName, ulValue));
    } else {
        InVolDbgPrint(DL_ERROR, DM_REGISTRY, 
            ("%s: Adding %S with value %#x Failed with Status 0x%x\n", __FUNCTION__,
                                    pValueName, ulValue, Status));
    }

    delete pDriverParam;
    
    return Status;
}

#if DBG
const char *
GetServiceStateString()
{
    switch(DriverContext.eServiceState)
    {
        case ecServiceUnInitiliazed:
            return "ecServiceUnInitiliazed";
            break;
        case ecServiceNotStarted:
            return "ecServiceNotStarted";
            break;
        case ecServiceRunning:
            return "ecServiceRunning";
            break;
        case ecServiceShutdown:
            return "ecServiceShutdown";
            break;
        default:
            break;
    }

    return "ecServiceUnKnown";
}
#endif

VOID
WakeServiceThread(BOOLEAN bDriverContextLockAcquired)
{
    KIRQL   OldIrql = 0;

    if (FALSE == bDriverContextLockAcquired)
        KeAcquireSpinLock(&DriverContext.Lock, &OldIrql);
    
    InVolDbgPrint(DL_INFO, DM_DEVICE_STATE, 
        ("WakeServiceThread: Waking service thread, ServiceState = %s\n", GetServiceStateString() ));
    KeSetEvent(&DriverContext.ServiceThreadWakeEvent, IO_DISK_INCREMENT, FALSE);

    if (FALSE == bDriverContextLockAcquired)
        KeReleaseSpinLock(&DriverContext.Lock, OldIrql);
}


NTSTATUS
GetUninitializedDisks(
_Inout_   PLIST_ENTRY ListHead
)
/*++

Routine Description:

This routine scans all disks in the DriverContext List and tries to get all uninitialized disks.

Arguments:

ListHead    - List that is populated with all uninitialized device contexts.

Return Value:
    NTSTATUS
--*/
{
    KIRQL       OldIrql;
    NTSTATUS    Status = STATUS_SUCCESS;
    PLIST_NODE  pNode = NULL;
    PDEV_CONTEXT DevContext = NULL;
    PLIST_ENTRY     pDevExtList;
    PLIST_ENTRY     l;

    KeAcquireSpinLock(&DriverContext.Lock, &OldIrql);

    pDevExtList = DriverContext.HeadForDevContext.Flink;

    for (; pDevExtList != &DriverContext.HeadForDevContext; pDevExtList = pDevExtList->Flink)
    {
        DevContext = (PDEV_CONTEXT)pDevExtList;

        KeAcquireSpinLockAtDpcLevel(&DevContext->Lock);
        if (TEST_FLAG(DevContext->ulFlags, DCF_DEVID_OBTAINED))
        {
            KeReleaseSpinLockFromDpcLevel(&DevContext->Lock);
            continue;
        }
        KeReleaseSpinLockFromDpcLevel(&DevContext->Lock);

        pNode = AllocateListNode();
        if (NULL == pNode)
        {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            break;
        }

        pNode->pData = ReferenceDevContext(DevContext);
        InsertTailList(ListHead, &pNode->ListEntry);
    }

    KeReleaseSpinLock(&DriverContext.Lock, OldIrql);

    if (!NT_SUCCESS(Status))
    {
        // Free all nodes.
        for (l = ListHead->Flink; l != ListHead; l = l->Flink)
        {
            pNode = CONTAINING_RECORD(l, LIST_NODE, ListEntry);

            RemoveEntryList(l);
            l = l->Blink;
            DereferenceDevContext((PDEV_CONTEXT)pNode->pData);
            DereferenceListNode(pNode);
        }
    }

    return Status;
}

NTSTATUS
GetListOfDevs(
_In_    PLIST_ENTRY ListHead
)
{
    KIRQL       OldIrql;
    NTSTATUS    Status = STATUS_SUCCESS;
    PLIST_NODE  pNode = NULL;
    PDEV_CONTEXT DevContext = NULL;

    InitializeListHead(ListHead);

    KeAcquireSpinLock(&DriverContext.Lock, &OldIrql);
    DevContext = (PDEV_CONTEXT)DriverContext.HeadForDevContext.Flink;
    while((PLIST_ENTRY)DevContext != &DriverContext.HeadForDevContext) {
        pNode = AllocateListNode();
        if (NULL == pNode) {
            Status = STATUS_NO_MEMORY;
            break;
        }

        pNode->pData = ReferenceDevContext(DevContext);
        InsertTailList(ListHead, &pNode->ListEntry);
        DevContext = (PDEV_CONTEXT)DevContext->ListEntry.Flink;
    }
    KeReleaseSpinLock(&DriverContext.Lock, OldIrql);

    if (STATUS_SUCCESS != Status) {
        // Free all nodes.
        while (!IsListEmpty(ListHead)) {
            pNode = (PLIST_NODE)RemoveHeadList(ListHead);
            DereferenceDevContext((PDEV_CONTEXT)pNode->pData);
            DereferenceListNode(pNode);
        }
    }

    return Status;
}

bool
MaxNonPagedPoolLimitReached(bool bAllocating)
{
    ASSERT(DriverContext.lNonPagedMemoryAllocated >= 0);
    if ((0 == DriverContext.ulMaxNonPagedPool) || (DriverContext.lNonPagedMemoryAllocated < 0)) {
        return false;
    }


    if ((ULONG)DriverContext.lNonPagedMemoryAllocated > DriverContext.ulMaxNonPagedPool) {
        if (bAllocating) {
            if (0 == DriverContext.liNonPagedLimitReachedTSforFirstTime.QuadPart) {
                KeQuerySystemTime(&DriverContext.liNonPagedLimitReachedTSforFirstTime);
                DriverContext.liNonPagedLimitReachedTSforLastTime.QuadPart = DriverContext.liNonPagedLimitReachedTSforFirstTime.QuadPart;
            } else {
                KeQuerySystemTime(&DriverContext.liNonPagedLimitReachedTSforLastTime);
            }
        }
        return true;
    }

    return false;
}


/*
 * This Function read the global persistent Time stamp and the Sequence Number
 * from the Registry. If the registry is not present it creates the new one with 
 * as the default value given
*/
VOID
GetPersistentValuesFromRegistry(Registry *pParametersKey)
{
    NTSTATUS        Status = STATUS_SUCCESS;
    ULONG           ulcbRead = 0;
    CHAR            ShutdownMarker = uninitialized;

    ULONG           NumberOfInfo = 2;
    const unsigned long SizeOfBuffer = ( sizeof(PERSISTENT_INFO) + sizeof(PERSISTENT_VALUE) );
    CHAR            LocalBuffer[SizeOfBuffer];
    PPERSISTENT_INFO  pPersistentInfo, pDefaultInfo;

    pPersistentInfo = (PPERSISTENT_INFO)LocalBuffer;
    pDefaultInfo = (PPERSISTENT_INFO)LocalBuffer;

    //Filling in the default values
    pDefaultInfo->NumberOfInfo = NumberOfInfo;
    pDefaultInfo->ShutdownMarker = DirtyShutdown;
    RtlZeroMemory(pDefaultInfo->Reserved, sizeof(pDefaultInfo->Reserved));

    pDefaultInfo->PersistentValues[0].PersistentValueType = LAST_GLOBAL_TIME_STAMP;
    pDefaultInfo->PersistentValues[0].Reserved = 0;
    KeQuerySystemTime((PLARGE_INTEGER)&pDefaultInfo->PersistentValues[0].Data);

    pDefaultInfo->PersistentValues[1].PersistentValueType = LAST_GLOBAL_SEQUENCE_NUMBER;
    pDefaultInfo->PersistentValues[1].Reserved = 0;
    pDefaultInfo->PersistentValues[1].Data = 0;

    //Reading the registry
    Status = pParametersKey->ReadBinary(LAST_PERSISTENT_VALUES, 
                                        STRING_LEN_NULL_TERMINATED, 
                                        (PVOID *)&pPersistentInfo,
                                        SizeOfBuffer,
                                        &ulcbRead,
                                        (PVOID)pDefaultInfo,
                                        SizeOfBuffer,
                                        TRUE);//sending bCreateIfFail as TRUE so as to create the key if not found
    if (NT_SUCCESS(Status)) {
        if (STATUS_INMAGE_OBJECT_CREATED == Status) {//New Registry has been created
            DriverContext.PersistentRegistryCreatedFlag = TRUE;
            DriverContext.ullPersistedTimeStampAfterBoot = pPersistentInfo->PersistentValues[0].Data;
            InVolDbgPrint(DL_ERROR, DM_REGISTRY, 
                ("%s: Created %S with Default Time Stamp %#I64x ",__FUNCTION__,LAST_PERSISTENT_VALUES, pDefaultInfo->PersistentValues[0].Data));
            DriverContext.ullPersistedSequenceNumberAfterBoot = pPersistentInfo->PersistentValues[1].Data;
            InVolDbgPrint(DL_ERROR, DM_REGISTRY, 
                ("and Default Sequence Number %#x\n", 0));
        } else {//Registry found. Values has been read.
            DriverContext.LastShutdownMarker = pPersistentInfo->ShutdownMarker; 
            ShutdownMarker = pPersistentInfo->ShutdownMarker;
            InVolDbgPrint(DL_ERROR, DM_REGISTRY, 
                ("%s: Read Registry %S Shutdown Marker  = %d\n",__FUNCTION__,LAST_PERSISTENT_VALUES, pPersistentInfo->ShutdownMarker));
            if (2 == pPersistentInfo->NumberOfInfo)
                ASSERT(SizeOfBuffer == ulcbRead);

            for (ULONG i = 0; i < pPersistentInfo->NumberOfInfo; i++) {
                switch (pPersistentInfo->PersistentValues[i].PersistentValueType) {
                    case LAST_GLOBAL_TIME_STAMP:
                        DriverContext.ullPersistedTimeStampAfterBoot = pPersistentInfo->PersistentValues[i].Data;
                        if (ShutdownMarker == DirtyShutdown) {//means that the last shutdown was dirty. We will bump the time stamp.
                            DriverContext.ullLastTimeStamp = pPersistentInfo->PersistentValues[i].Data + TIME_STAMP_BUMP;
                            InDskFltWriteEvent(INDSKFLT_ERROR_UNCLEAN_GLOBAL_SHUTDOWN_MARKER);
                        } else { //Means that the last shutdown was clean. We can use the same time stamp.
                            DriverContext.ullLastTimeStamp = pPersistentInfo->PersistentValues[i].Data;
                        }
                            // Global sequence number for 32-bit systems supported now. Let's increment TimeStamp as 
                            // We may end up in same timestamp(when time goes back)and Global Sequence Number start as '0'
                            // This would lead to OOD issue
                            if (1 == pPersistentInfo->NumberOfInfo) {
                                ASSERT(sizeof(PERSISTENT_INFO) == ulcbRead);
                                DriverContext.ullLastTimeStamp++;
                            }
                        InVolDbgPrint(DL_ERROR, DM_REGISTRY, 
                            ("%s: Read Registry %S Time Stamp Value = %#I64x\n",__FUNCTION__,LAST_PERSISTENT_VALUES, pPersistentInfo->PersistentValues[i].Data));
                        break;
                    case LAST_GLOBAL_SEQUENCE_NUMBER:
                        DriverContext.ullPersistedSequenceNumberAfterBoot = pPersistentInfo->PersistentValues[i].Data;
                        if (ShutdownMarker == DirtyShutdown)//Means that the last shutdown was dirty. We will bump the Seq No.
                            DriverContext.ullTimeStampSequenceNumber = pPersistentInfo->PersistentValues[i].Data + SEQ_NO_BUMP;
                        else//Means the last shutdown was clean. We can use the same Seq no.
                            DriverContext.ullTimeStampSequenceNumber = pPersistentInfo->PersistentValues[i].Data ;
                        InVolDbgPrint(DL_ERROR, DM_REGISTRY, 
                            ("%s: Read Registry %S Sequence Number Value = %#I64x\n",__FUNCTION__,LAST_PERSISTENT_VALUES, pPersistentInfo->PersistentValues[i].Data));
                        break;
                    default :
                        InVolDbgPrint(DL_ERROR, DM_REGISTRY, 
                            ("%s: Read Registry for %S is corrupted.\n", __FUNCTION__, LAST_PERSISTENT_VALUES));
                        break;
                }
            }
        }
    } else {
        InVolDbgPrint(DL_ERROR, DM_REGISTRY, 
            ("%s: ReadBinary for %S has failed\n", __FUNCTION__, LAST_PERSISTENT_VALUES));
        InDskFltWriteEvent(INDSKFLT_ERROR_READ_LAST_PERSISTENT_VALUE_FAILURE, Status);
    }

    return;
}

void SetMaxLockedDataBlocks() {
    /* MaxLockedDataBlocks is set to minimum of DataPoolSize, Num of allocated Data Blocks, MaxSetLockedDataBlocks
       As it should not be greater than Num of allocated Data Blocks and MaxSetLockedDataBlocks.
       If DataPoolSize is reset to lower value then MaxLockedDataBlocks need to reset accordingly. So that Locked pages can get freed up*/
    DriverContext.ulMaxLockedDataBlocks = DriverContext.ulDataBlockAllocated;
    if (DriverContext.ulMaxSetLockedDataBlocks < DriverContext.ulDataBlockAllocated) {
        DriverContext.ulMaxLockedDataBlocks = DriverContext.ulMaxSetLockedDataBlocks;
    }
    if (DriverContext.ulMaxLockedDataBlocks > (DriverContext.ullDataPoolSize / MEGABYTES)) {
        DriverContext.ulMaxLockedDataBlocks = (ULONG)(DriverContext.ullDataPoolSize / MEGABYTES);
    }
}

ULONG GetFreeThresholdForFileWrite(PDEV_CONTEXT DevContext) {
    ULONG ulDaBFreeThresholdForFileWrite = ValidateFreeThresholdForFileWrite(DriverContext.ulDaBFreeThresholdForFileWrite);
    ulDaBFreeThresholdForFileWrite = (DriverContext.ulDataBlockAllocated * ulDaBFreeThresholdForFileWrite) / 100 ;
    ULONG ulNumDaBPerDirtyBlock = DevContext->ulMaxDataSizePerDataModeDirtyBlock / DriverContext.ulDataBlockSize;
    if (ulDaBFreeThresholdForFileWrite < ulNumDaBPerDirtyBlock) {
        ulDaBFreeThresholdForFileWrite = ulNumDaBPerDirtyBlock;
    }
    return ulDaBFreeThresholdForFileWrite;
}

ULONG GetThresholdPerDevForFileWrite(PDEV_CONTEXT DevContext) {
    ULONG ulDaBThresholdPerDevForFileWrite = ValidateThresholdPerDevForFileWrite(DriverContext.ulDaBThresholdPerDevForFileWrite);
    ulDaBThresholdPerDevForFileWrite = ((DriverContext.ulMaxDataSizePerDev / DriverContext.ulDataBlockSize) * ulDaBThresholdPerDevForFileWrite) / 100;
    ULONG ulNumDaBPerDirtyBlock = DevContext->ulMaxDataSizePerDataModeDirtyBlock / DriverContext.ulDataBlockSize;
    if (ulDaBThresholdPerDevForFileWrite < ulNumDaBPerDirtyBlock) {
        ulDaBThresholdPerDevForFileWrite = ulNumDaBPerDirtyBlock;
    } 
    return ulDaBThresholdPerDevForFileWrite;
}

ULONGLONG ReqNonPagePoolMem() {
    /* Assuming IO size of 1K. Calculate number of changes for the given data pool size
       Calculate minimum Dirty Block req which will be total num of changes divided by max changes in dirty block 
       In the similar mannar calculate ChangeBlock and Change Node required
       Calculate Non Page pool req on above assumtion. Add 20% overload*/
    ULONG ulMaxNumOfChanges = ULONG (DriverContext.ullDataPoolSize / KILOBYTES);
#ifdef _WIN64
    ULONG ulMinNumOfDirtyBlocks = ulMaxNumOfChanges / MAX_UDIRTY_CHANGES_V2;
#else
    ULONG ulMinNumOfDirtyBlocks = ulMaxNumOfChanges / MAX_UDIRTY_CHANGES;
#endif
    ULONG ulMinNumOfChangeBlock = ulMaxNumOfChanges / MAX_CHANGES_IN_2K_CB;
    ULONG ulMinNumOfChangeNodes = ulMinNumOfDirtyBlocks;
    ULONGLONG ullReqNonPagePool = (ULONGLONG)(((ULONGLONG)((((ULONGLONG)ulMinNumOfChangeBlock) * CB_SIZE_FOR_2K_CB) + 
            (((ULONGLONG)ulMinNumOfDirtyBlocks) * sizeof(KDIRTY_BLOCK)) + 
            (((ULONGLONG)ulMinNumOfChangeNodes) * sizeof(CHANGE_NODE)))) * NON_PAGE_POOL_OVER_HEAD);
    if (0 != ullReqNonPagePool % MEGABYTES) {
        ullReqNonPagePool = ((ullReqNonPagePool + MEGABYTES) / MEGABYTES) * MEGABYTES;
    }
    return ullReqNonPagePool;
}

ULONG ValidateDataBufferSize(ULONG ulDataBufferSize) {
    //Min 512b, Default 4k, Max No limit, Aligned Byte
    if (0 == ulDataBufferSize) {
        ulDataBufferSize = DEFAULT_DATA_BUFFER_SIZE;
        // Convert buffer size read from KBytes to Bytes.
        ulDataBufferSize *= KILOBYTES;
    }

    // Make sure buffer size is atleast size of SECTOR_SIZE
    if (ulDataBufferSize < SECTOR_SIZE) {
        ulDataBufferSize = SECTOR_SIZE;
    }

    return ulDataBufferSize;
}

ULONG ValidateDataBlockSize(ULONG ulDataBlockSize) {
    //Min 4K, Default 1MB, Max No Lomit, Aligned DataBufferSize
    if (0 == ulDataBlockSize) {
        ulDataBlockSize = DEFAULT_DATA_BLOCK_SIZE;
        // Convert block size read from KBytes to Bytes.	
        ulDataBlockSize *= KILOBYTES;
    }

    if (ulDataBlockSize < PAGE_SIZE) {
        ulDataBlockSize = PAGE_SIZE;
    }

    if (ulDataBlockSize < DriverContext.ulDataBufferSize) {
        ulDataBlockSize = DriverContext.ulDataBufferSize; 
    }

    if (0 != (ulDataBlockSize % DriverContext.ulDataBufferSize)) {
        ulDataBlockSize = (ulDataBlockSize / DriverContext.ulDataBufferSize) * DriverContext.ulDataBufferSize;
    }
    return ulDataBlockSize;
}

ULONG ValidateMaxDataSizePerDataModeDirtyBlock(ULONG ulMaxDataSizePerDataModeDirtyBlock) {
    //Min  DataBlockSize or 1MB, 
    //Default 4 * (DataBlockSize or DEFAULT_DATA_BLOCK_SIZE)
    //Max 16MB, 
    //Aligned DataBlockSize   
    if (0 == ulMaxDataSizePerDataModeDirtyBlock) {
        // DriverContext.ulDataBlockSize is in Bytes already no convertion is required
        ulMaxDataSizePerDataModeDirtyBlock = DEFAULT_NUM_DATA_BLOCKS_PER_SERVICE_REQUEST * DriverContext.ulDataBlockSize;
    }    

    if (ulMaxDataSizePerDataModeDirtyBlock < DriverContext.ulDataBlockSize) {
        ulMaxDataSizePerDataModeDirtyBlock = DriverContext.ulDataBlockSize;
    }
    
    if (ulMaxDataSizePerDataModeDirtyBlock < (MIN_DATA_SIZE_PER_DATA_MODE_DIRTY_BLOCK_IN_MB * MEGABYTES)) {
        ulMaxDataSizePerDataModeDirtyBlock = (MIN_DATA_SIZE_PER_DATA_MODE_DIRTY_BLOCK_IN_MB * MEGABYTES);
    }
    
    if (ulMaxDataSizePerDataModeDirtyBlock > (MAX_DATA_SIZE_PER_DATA_MODE_DIRTY_BLOCK_IN_MB * MEGABYTES)) {
        ulMaxDataSizePerDataModeDirtyBlock = (MAX_DATA_SIZE_PER_DATA_MODE_DIRTY_BLOCK_IN_MB * MEGABYTES);
    }

    ULONGLONG ullDataPoolSizeMB = DriverContext.ullDataPoolSize / MEGABYTES;
    if ((ulMaxDataSizePerDataModeDirtyBlock / MEGABYTES) > (ullDataPoolSizeMB / 8)) {
        ulMaxDataSizePerDataModeDirtyBlock = (ULONG)(ullDataPoolSizeMB / 8) * MEGABYTES;
    }

    if (0 != (ulMaxDataSizePerDataModeDirtyBlock % DriverContext.ulDataBlockSize)) {
        ulMaxDataSizePerDataModeDirtyBlock = (ulMaxDataSizePerDataModeDirtyBlock / DriverContext.ulDataBlockSize) * DriverContext.ulDataBlockSize;
    }
    return ulMaxDataSizePerDataModeDirtyBlock;
}

ULONG ValidateMaxDataSizePerNonDataModeDirtyBlock(ULONG ulMaxDataSizePerNonDataModeDirtyBlock) {
    //Min  DataBlockSize, 
    //Default 64MB
    //Max 64MB, 
    //Aligned bytes
    if (0 == ulMaxDataSizePerNonDataModeDirtyBlock) {
        ulMaxDataSizePerNonDataModeDirtyBlock = DEFAULT_MAX_DATA_SIZE_PER_NON_DATA_MODE_DIRTY_BLOCK;
        // Convert service request data size from KBytes to Bytes	
        ulMaxDataSizePerNonDataModeDirtyBlock *= KILOBYTES;
    }

    if (ulMaxDataSizePerNonDataModeDirtyBlock < DriverContext.ulMaxDataSizePerDataModeDirtyBlock) {
        ulMaxDataSizePerNonDataModeDirtyBlock = DriverContext.ulMaxDataSizePerDataModeDirtyBlock;
    }
    
    if (ulMaxDataSizePerNonDataModeDirtyBlock < (MIN_DATA_SIZE_PER_NON_DATA_MODE_DIRTY_BLOCK_IN_MB * MEGABYTES)) {
        ulMaxDataSizePerNonDataModeDirtyBlock = (MIN_DATA_SIZE_PER_NON_DATA_MODE_DIRTY_BLOCK_IN_MB * MEGABYTES);
    }
    
    if (ulMaxDataSizePerNonDataModeDirtyBlock > (MAX_DATA_SIZE_PER_NON_DATA_MODE_DIRTY_BLOCK_IN_MB * MEGABYTES)) {
        ulMaxDataSizePerNonDataModeDirtyBlock = (MAX_DATA_SIZE_PER_NON_DATA_MODE_DIRTY_BLOCK_IN_MB * MEGABYTES);
    }

    if (0 != (ulMaxDataSizePerNonDataModeDirtyBlock % DriverContext.ulDataBlockSize)) {
        ulMaxDataSizePerNonDataModeDirtyBlock = (ulMaxDataSizePerNonDataModeDirtyBlock / DriverContext.ulDataBlockSize) * DriverContext.ulDataBlockSize;
    }
    return ulMaxDataSizePerNonDataModeDirtyBlock;
}

ULONG ValidateMaxDataSizePerDev(ULONG ulMaxDataSizePerDev) {
    //Min  3 * 4Mb
    //Default 32bit -- 12 * 4Mb, 64 bit -- 56 * 4 MB
    //Max DataPoolSize, 
    //Aligned lMaxDataSizePerDataModeDirtyBlock
    if (0 == ulMaxDataSizePerDev) {
        ulMaxDataSizePerDev = DEFAULT_DATA_DIRTY_BLOCKS_PER_DEV * DriverContext.ulMaxDataSizePerDataModeDirtyBlock;
    }

    if (0 != (ulMaxDataSizePerDev % DriverContext.ulMaxDataSizePerDataModeDirtyBlock)) {
        ulMaxDataSizePerDev = (ulMaxDataSizePerDev / DriverContext.ulMaxDataSizePerDataModeDirtyBlock) * DriverContext.ulMaxDataSizePerDataModeDirtyBlock;
    }

    if (ulMaxDataSizePerDev < (MINIMUM_DATA_DIRTY_BLOCKS_PER_DEV * DriverContext.ulMaxDataSizePerDataModeDirtyBlock)) {
        ulMaxDataSizePerDev = MINIMUM_DATA_DIRTY_BLOCKS_PER_DEV * DriverContext.ulMaxDataSizePerDataModeDirtyBlock;
    }
    if (ulMaxDataSizePerDev > DriverContext.ullDataPoolSize) {
        ulMaxDataSizePerDev = (ULONG)DriverContext.ullDataPoolSize;
    }

    if (0 != (ulMaxDataSizePerDev % DriverContext.ulMaxDataSizePerDataModeDirtyBlock)) {
        ulMaxDataSizePerDev = (ulMaxDataSizePerDev / DriverContext.ulMaxDataSizePerDataModeDirtyBlock) * DriverContext.ulMaxDataSizePerDataModeDirtyBlock;
    }
    return ulMaxDataSizePerDev;
}

ULONG ValidateMaxSetLockedDataBlocks(ULONG ulMaxSetLockedDataBlocks) {
    //Default 32bit --- 30, 64bit --- 60
    if (0 == ulMaxSetLockedDataBlocks) {
        ulMaxSetLockedDataBlocks = DEFAULT_MAX_LOCKED_DATA_BLOCKS;
    }
    ULONG ulMaxDataBlocks = (ULONG)((DriverContext.ullDataPoolSize) / DriverContext.ulDataBlockSize);
    if (ulMaxSetLockedDataBlocks > ulMaxDataBlocks) {
        ulMaxSetLockedDataBlocks = ulMaxDataBlocks;
    }
    return ulMaxSetLockedDataBlocks;
}

ULONG ValidateFreeThresholdForFileWrite(ULONG ulDaBFreeThresholdForFileWrite) {
    if (ulDaBFreeThresholdForFileWrite > 100) {
        ulDaBFreeThresholdForFileWrite = 100;
    }
    return ulDaBFreeThresholdForFileWrite;
}

ULONG ValidateThresholdPerDevForFileWrite(ULONG ulDaBThresholdPerDevForFileWrite) {
    if (ulDaBThresholdPerDevForFileWrite > 100) {
        ulDaBThresholdPerDevForFileWrite = 100;
    }
    return ulDaBThresholdPerDevForFileWrite;
}


ULONGLONG ValidateDataPoolSize(ULONGLONG ullDataPoolSize) {
    //Min  DataBlockSize
    //Default 32bit -- 64Mb, 64 bit -- 256MB
    //Max no limit, 
    //Aligned DataBlockSize
    if (0 == ullDataPoolSize) {
        ullDataPoolSize = DEFAULT_DATA_POOL_SIZE;
        ullDataPoolSize = ullDataPoolSize * MEGABYTES;
    }
    
    if (ullDataPoolSize < DriverContext.ulDataBlockSize) {
        ullDataPoolSize = DriverContext.ulDataBlockSize;
    }

    if (0 != (ullDataPoolSize % DriverContext.ulDataBlockSize)) {
        ullDataPoolSize = (ullDataPoolSize / DriverContext.ulDataBlockSize) * DriverContext.ulDataBlockSize;
    }
    return ullDataPoolSize;
}

void
FreeGlobalPageFileNodes (
    )

/*++

Routine Description:

    Free all the nodes allocated from FreePageFileList

    This function must be called at IRQL <= DISPATCH_LEVEL
--*/
{
    PPAGE_FILE_NODE         pPageFileNode = NULL;    
    PLIST_ENTRY             Entry = NULL;
    KIRQL                   OldIrql = 0;

    KeAcquireSpinLock(&DriverContext.PageFileListLock, &OldIrql);

    while(!IsListEmpty(&DriverContext.FreePageFileList)) {
        DriverContext.ulNumOfFreePageNodes--;
        DriverContext.ulNumOfAllocPageNodes--;
        Entry = RemoveHeadList(&DriverContext.FreePageFileList);
        pPageFileNode = (PPAGE_FILE_NODE)CONTAINING_RECORD((PCHAR)Entry,
                                                           PAGE_FILE_NODE,
                                                           ListEntry);
        InitializeListHead(Entry);
        ExFreePoolWithTag(pPageFileNode, TAG_PAGE_FILE);
    }

    KeReleaseSpinLock(&DriverContext.PageFileListLock, OldIrql);

    return;
}

NTSTATUS
AllocateGlobalPageFileNodes (
    )
/*++

Routine Description:

    PreAllocate Page file nodes in Global FreePageFileList list

    This function must be called at IRQL <= DISPATCH_LEVEL
--*/

{
    NTSTATUS                Status = STATUS_UNSUCCESSFUL;
    PPAGE_FILE_NODE         pPageFileNode = NULL;
    KIRQL                   OldIrql = 0;

    KeAcquireSpinLock(&DriverContext.PageFileListLock, &OldIrql);
    while (DriverContext.ulNumOfFreePageNodes < DEFAULT_MAX_PAGE_FILE_NODES) {
        pPageFileNode = (PPAGE_FILE_NODE) ExAllocatePoolWithTag(NonPagedPool,
                                                                sizeof(PAGE_FILE_NODE), 
                                                                TAG_PAGE_FILE);
        if (NULL == pPageFileNode) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto Cleanup;
        }
        RtlZeroMemory(pPageFileNode, sizeof(PAGE_FILE_NODE));
        InsertTailList(&DriverContext.FreePageFileList, &pPageFileNode->ListEntry);
        DriverContext.ulNumOfFreePageNodes++;
        DriverContext.ulNumOfAllocPageNodes++;

    }
    Status = STATUS_SUCCESS;
Cleanup:
    KeReleaseSpinLock(&DriverContext.PageFileListLock, OldIrql);
    return Status;
}

