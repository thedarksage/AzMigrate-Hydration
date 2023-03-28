#include "Common.h"
#include "DriverContext.h"
#include "MntMgr.h"
#include "global.h"
#include "ListNode.h"
#include "VBitmap.h"
#include "DataFlt.h"
#include "misc.h"
#include "ChangeNode.h"
#include "VContext.h"
#include "DataFileWriterMgr.h"

DRIVER_CONTEXT  DriverContext;

NTSTATUS
InitializeDriverContext(
    PDRIVER_OBJECT  DriverObject,
    PUNICODE_STRING RegistryPath
    )
{
    NTSTATUS        Status = STATUS_SUCCESS;
    UNICODE_STRING  DeviceName, SymLinkName;
    USHORT          usLength;
    Registry        ParametersKey, KernelTagsKey;
    ULONG           ulVolumeDataCapture, ulVolumeDataFilteringForNewVolumes, ulKernelTagsEnabled;
    ULONG           ulDataFiles, ulDataFilesForNewVolumes, ulVFDisabledForNewVolumes, ulVFDisabledForNewClusterVolumes;
    ULONG32         ulDataPool;
    ULONG           ulWorkerThreadPriority = 0;

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
    InitializeListHead(&DriverContext.OrphanedMappedDataBlockList);
    DriverContext.eServiceState = ecServiceNotStarted;
    KeInitializeSpinLock(&DriverContext.Lock);
    InitializeListHead(&DriverContext.HeadForVolumeContext);
    InitializeListHead(&DriverContext.HeadForBasicDisks);
    InitializeListHead(&DriverContext.HeadForTagNodes);
    KeInitializeSpinLock(&DriverContext.TagListLock);
    KeInitializeEvent(&DriverContext.ServiceThreadWakeEvent, NotificationEvent, FALSE);
    KeInitializeEvent(&DriverContext.ServiceThreadShutdownEvent, NotificationEvent, FALSE);
    //Added for persistent time stamp
    KeInitializeEvent(&DriverContext.PersistentThreadShutdownEvent, NotificationEvent, FALSE);
    KeInitializeMutex(&DriverContext.WriteFilterMutex, 0);
    InitializeListHead(&DriverContext.HeadForVolumeBitmaps);
    DriverContext.ulNumVolumeBitmaps = 0;
	
	DriverContext.bootVolNotificationInfo.bootVolumeDetected = 0;
    DriverContext.bootVolNotificationInfo.pBootVolumeContext = NULL;
	DriverContext.bootVolNotificationInfo.failureCount = 0;
	DriverContext.bootVolNotificationInfo.reinitCount = 0;
	DriverContext.bootVolNotificationInfo.LockUnlockBootVolumeDisable = 0;
	//This is not-signalled or acquired or blocking state.
	KeInitializeEvent(&DriverContext.bootVolNotificationInfo.bootNotifyEvent, NotificationEvent, FALSE);

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

    //Endianess Initialization
    DriverContext.bEndian = TestByteOrder();
#if (_WIN32_WINNT >= 0x501)	
    // Get OS version
    RTL_OSVERSIONINFOW osVersion;
    RtlZeroMemory(&osVersion, sizeof(RTL_OSVERSIONINFOW));

    osVersion.dwOSVersionInfoSize = sizeof(RTL_OSVERSIONINFOW);
    RtlGetVersion(&osVersion);

    DriverContext.osMajorVersion = osVersion.dwMajorVersion;
    DriverContext.osMinorVersion = osVersion.dwMinorVersion;

    // Windows 8/Windows 2012 has version 6.2
    DriverContext.IsWindows8AndLater = ((osVersion.dwMajorVersion > 6) || ((osVersion.dwMajorVersion == 6) && (osVersion.dwMinorVersion > 1)));
#endif
    //Ioctl spin lock initilization
    KeInitializeSpinLock(&DriverContext.MultiVolumeLock);
    DriverContext.DriverImage.MaximumLength = usLength + sizeof(WCHAR);
    DriverContext.DriverImage.Buffer = (PWCHAR)ExAllocatePoolWithTag(NonPagedPool,
                                            DriverContext.DriverImage.MaximumLength,
                                            TAG_GENERIC_NON_PAGED);
    if (NULL == DriverContext.DriverImage.Buffer)
    {
        CleanDriverContext();
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    RtlCopyUnicodeString(&DriverContext.DriverImage, RegistryPath);
    DriverContext.DriverImage.Buffer[DriverContext.DriverImage.Length/sizeof(WCHAR)] = L'\0';

    usLength = RegistryPath->Length + (wcslen(PARAMETERS_KEY)*sizeof(WCHAR));
    DriverContext.DriverParameters.MaximumLength = usLength + sizeof(WCHAR);
    DriverContext.DriverParameters.Buffer = (PWCHAR)ExAllocatePoolWithTag(NonPagedPool,
                                            DriverContext.DriverParameters.MaximumLength,
                                            TAG_GENERIC_NON_PAGED);
    if (NULL == DriverContext.DriverParameters.Buffer)
    {
        CleanDriverContext();
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlCopyUnicodeString(&DriverContext.DriverParameters, RegistryPath);
    RtlAppendUnicodeToString(&DriverContext.DriverParameters, PARAMETERS_KEY);
    DriverContext.DriverImage.Buffer[DriverContext.DriverImage.Length/sizeof(WCHAR)] = L'\0';

    // Maximum Tag space in dirty block is UDIRTY_BLOCK_TAGS_SIZE - predefined tags.
    DriverContext.ulMaxTagBufferSize = UDIRTY_BLOCK_TAGS_SIZE - sizeof(STREAM_REC_HDR_4B) - 
                (FIELD_OFFSET(UDIRTY_BLOCK_V2, uTagList.TagList.TagEndOfList) - FIELD_OFFSET(UDIRTY_BLOCK_V2, uTagList.TagList.TagStartOfList));

// #define ALLOW_UNLOAD 1
#ifndef ALLOW_UNLOAD
    RtlInitUnicodeString(&DeviceName, INMAGE_DEVICE_NAME);
    Status = IoCreateDevice(DriverObject, sizeof(DEVICE_EXTENSION), &DeviceName, FILE_DEVICE_UNKNOWN, 0, FALSE, 
                            &DriverContext.ControlDevice);
    if (!NT_SUCCESS(Status))
    {
        CleanDriverContext();
        return Status;
    }

    DriverContext.ulFlags |= DC_FLAGS_DEVICE_CREATED;

    IoRegisterShutdownNotification(DriverContext.ControlDevice);
    
    RtlZeroMemory(DriverContext.ControlDevice->DeviceExtension, sizeof(DEVICE_EXTENSION));

    RtlInitUnicodeString(&SymLinkName, INMAGE_SYMLINK_NAME);
    Status = IoCreateSymbolicLink(&SymLinkName, &DeviceName);
    if (!NT_SUCCESS(Status))
    {
        CleanDriverContext();
        return Status;
    }

    DriverContext.ulFlags |= DC_FLAGS_SYMLINK_CREATED;
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
    DriverContext.ulNumberofAsyncTagIOCTLs = 0;
    Status = ParametersKey.OpenKeyReadWrite(&DriverContext.DriverParameters);
    if (!NT_SUCCESS(Status)) {
        // Failed to open the key, most probably there is no parameters key in registry.
        // Let me add the key.
        Status = ParametersKey.CreateKeyReadWrite(&DriverContext.DriverParameters);
    }

    if (NT_SUCCESS(Status)) {
        Status = ParametersKey.OpenSubKey(KERNEL_TAGS_KEY, STRING_LEN_NULL_TERMINATED, KernelTagsKey, true);

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
/*        ParametersKey.ReadULONG(BITMAP_8K_GRANULARITY_SIZE,
                                    (ULONG32 *) &DriverContext.Bitmap8KGranularitySize,
                                    DEFAULT_BITMAP_8K_GRANULARITY_SIZE, TRUE);
        ParametersKey.ReadULONG(BITMAP_16K_GRANULARITY_SIZE,
                                    (ULONG32 *) &DriverContext.Bitmap16KGranularitySize,
                                    DEFAULT_BITMAP_16K_GRANULARITY_SIZE, TRUE);
        ParametersKey.ReadULONG(BITMAP_32K_GRANULARITY_SIZE,
                                    (ULONG32 *) &DriverContext.Bitmap32KGranularitySize,
                                    DEFAULT_BITMAP_32K_GRANULARITY_SIZE, TRUE); */
        ParametersKey.ReadULONG(BITMAP_512K_GRANULARITY_SIZE,
                                    (ULONG32 *) &DriverContext.Bitmap512KGranularitySize,
                                    DEFAULT_BITMAP_512K_GRANULARITY_SIZE, TRUE);

        // Registry keys related to data filtering
        ParametersKey.ReadULONG(DATA_BUFFER_SIZE,
                                    (ULONG32 *) &DriverContext.ulDataBufferSize,
                                    DEFAULT_DATA_BUFFER_SIZE,  TRUE);
        DriverContext.ulDataBufferSize *= KILOBYTES;
        ParametersKey.ReadULONG(DATA_BLOCK_SIZE,
                                    (ULONG32 *) &DriverContext.ulDataBlockSize,
                                    DEFAULT_DATA_BLOCK_SIZE, TRUE);
        DriverContext.ulDataBlockSize *= KILOBYTES;
        ParametersKey.ReadULONG(DATA_POOL_SIZE, 
                                    &ulDataPool,
                                    DEFAULT_DATA_POOL_SIZE, TRUE);
        DriverContext.ullDataPoolSize = ((ULONGLONG)ulDataPool) * MEGABYTES;
	
        ParametersKey.ReadULONG(LOCKED_DATA_BLOCKS_FOR_USE_AT_DISPATCH_IRQL,
                                    (ULONG32 *) &DriverContext.ulMaxSetLockedDataBlocks,
                                    DEFAULT_MAX_LOCKED_DATA_BLOCKS, TRUE);
        ParametersKey.ReadULONG(VOLUME_DATA_CAPTURE,
                                    (ULONG32 *) &ulVolumeDataCapture,
                                    DEFAULT_VOLUME_DATA_CAPTURE, TRUE);
        ParametersKey.ReadULONG(VOLUME_DATA_FILTERING_FOR_NEW_VOLUMES,
                                    (ULONG32 *)&ulVolumeDataFilteringForNewVolumes,
                                    DEFAULT_VOLUME_DATA_FILTERING_FOR_NEW_VOLUMES, TRUE);
        ParametersKey.ReadULONG(VOLUME_DATA_FILES,
                                    (ULONG32 *) &ulDataFiles,
                                    DEFAULT_VOLUME_DATA_FILES, TRUE);
        ParametersKey.ReadULONG(VOLUME_DATA_FILES_FOR_NEW_VOLUMES,
                                    (ULONG32 *)&ulDataFilesForNewVolumes,
                                    DEFAULT_VOLUME_DATA_FILES_FOR_NEW_VOLUMES, TRUE);
        ParametersKey.ReadULONG(MAX_DATA_SIZE_PER_DATA_MODE_DIRTY_BLOCK,
                                    (ULONG32 *)&DriverContext.ulMaxDataSizePerDataModeDirtyBlock,
                                    DEFAULT_MAX_DATA_SIZE_PER_DATA_MODE_DIRTY_BLOCK, TRUE);
	    DriverContext.ulMaxDataSizePerDataModeDirtyBlock *= KILOBYTES;
        ParametersKey.ReadULONG(MAX_DATA_SIZE_PER_NON_DATA_MODE_DIRTY_BLOCK,
                                    (ULONG32 *)&DriverContext.ulMaxDataSizePerNonDataModeDirtyBlock,
                                    DEFAULT_MAX_DATA_SIZE_PER_NON_DATA_MODE_DIRTY_BLOCK, TRUE);
        DriverContext.ulMaxDataSizePerNonDataModeDirtyBlock *= KILOBYTES;
        ParametersKey.ReadULONG(VOLUME_DATA_SIZE_LIMIT,
                                    (ULONG32 *)&DriverContext.ulMaxDataSizePerVolume,
                                    DEFAULT_VOLUME_DATA_SIZE_LIMIT, TRUE);
        DriverContext.ulMaxDataSizePerVolume = DriverContext.ulMaxDataSizePerVolume * KILOBYTES;
        ParametersKey.ReadULONG(VOLUME_DATA_TO_DISK_LIMIT_IN_MB,
                                    (ULONG32 *)&DriverContext.ulDataToDiskLimitPerVolumeInMB,
                                    DEFAULT_VOLUME_DATA_TO_DISK_LIMIT_IN_MB, TRUE);
        ParametersKey.ReadULONG(VOLUME_MIN_FREE_DATA_TO_DISK_LIMIT_IN_MB,
                                    (ULONG32 *)&DriverContext.ulMinFreeDataToDiskLimitPerVolumeInMB,
                                    DEFAULT_VOLUME_MIN_FREE_DATA_TO_DISK_LIMIT_IN_MB, TRUE);
        ParametersKey.ReadULONG(VOLUME_DATA_NOTIFY_LIMIT_IN_KB,
                                    (ULONG32 *)&DriverContext.ulDataNotifyThresholdInKB,
                                    DEFAULT_VOLUME_DATA_NOTIFY_LIMIT_IN_KB, TRUE);
        ParametersKey.ReadULONG(MAX_WAIT_TIME_FOR_IRP_COMPLETION_IN_MINUTES,
                                    (ULONG32 *)&DriverContext.ulMaxWaitTimeForIrpCompletion,
                                    DEFAULT_MAX_WAIT_TIME_FOR_IRP_COMPLETION_IN_MINUTES, TRUE);  
        ParametersKey.ReadULONG(VOLUME_FILE_WRITER_THREAD_PRIORITY,
                                    (ULONG32 *)&DriverContext.FileWriterThreadPriority,
                                    DEFAULT_VOLUME_FILE_WRITERS_THREAD_PRIORITY, TRUE);
        ParametersKey.ReadULONG(FREE_THRESHOLD_FOR_FILE_WRITE,
                                    (ULONG32 *)&DriverContext.ulDaBFreeThresholdForFileWrite,
                                    DEFAULT_FREE_THREASHOLD_FOR_FILE_WRITE, TRUE);
        ParametersKey.ReadULONG(VOLUME_THRESHOLD_FOR_FILE_WRITE,
                                    (ULONG32 *)&DriverContext.ulDaBThresholdPerVolumeForFileWrite,
                                    DEFAULT_VOLUME_THRESHOLD_FOR_FILE_WRITE, TRUE);
        ParametersKey.ReadULONG(VOLUME_FILTERING_DISABLED_FOR_NEW_VOLUMES,
                                    (ULONG32 *)&ulVFDisabledForNewVolumes,
                                    DEFAULT_VOLUME_FILTERING_DISABLED_FOR_NEW_VOLUMES, TRUE);
        ParametersKey.ReadULONG(VOLUME_FILTERING_DISABLED_FOR_NEW_CLUSTER_VOLUMES,
                                    (ULONG32 *)&ulVFDisabledForNewClusterVolumes,
                                    DEFAULT_VOLUME_FILTERING_DISABLED_FOR_NEW_CLUSTER_VOLUMES, TRUE);
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

        // Kernel tags related parameters
        ParametersKey.ReadULONG(KERNEL_TAGS_ENABLED,
                                    (ULONG32 *)&ulKernelTagsEnabled,
                                    DEFAULT_KERNEL_TAGS_ENABLED, TRUE);
        if (ulKernelTagsEnabled) {
            DriverContext.bKernelTagsEnabled = TRUE;
        }

        KernelTagsKey.ReadAString(MODE_CHANGE_TAG_DESC_FOR_BITMAP_TO_DATA, STRING_LEN_NULL_TERMINATED, &DriverContext.BitmapToDataModeTag, 
                                  MODE_CHANGE_TAG_DESC_FOR_BITMAP_TO_DATA_DEFAULT_VALUE, TRUE, NonPagedPool);
        DriverContext.usBitmapToDataModeTagLen = ReturnStringLenAInUSHORT(DriverContext.BitmapToDataModeTag);
        KernelTagsKey.ReadAString(MODE_CHANGE_TAG_DESC_FOR_BITMAP_TO_METADATA, STRING_LEN_NULL_TERMINATED, &DriverContext.BitmapToMetaDataModeTag, 
                                  MODE_CHANGE_TAG_DESC_FOR_BITMAP_TO_METADATA_DEFAULT_VALUE, TRUE, NonPagedPool);
        DriverContext.usBitmapToMetaDataModeTagLen = ReturnStringLenAInUSHORT(DriverContext.BitmapToMetaDataModeTag);
//        ParametersKey.ReadWString(MODE_CHANGE_TAG_DESC_FOR_DATA_TO_BITMAP, STRING_LEN_NULL_TERMINATED, &DriverContext.DataToBitmapModeTag, 
//                                  MODE_CHANGE_TAG_DESC_FOR_DATA_TO_BITMAP_DEFAULT_VALUE, TRUE, NonPagedPool);
//        DriverContext.usDataToBitmapModeTagLen = ReturnStringLenWInUSHORT(DriverContext.DataToBitmapModeTag);
        KernelTagsKey.ReadAString(MODE_CHANGE_TAG_DESC_FOR_DATA_TO_METADATA, STRING_LEN_NULL_TERMINATED, &DriverContext.DataToMetaDataModeTag, 
                                  MODE_CHANGE_TAG_DESC_FOR_DATA_TO_METADATA_DEFAULT_VALUE, TRUE, NonPagedPool);
        DriverContext.usDataToMetaDataModeTagLen = ReturnStringLenAInUSHORT(DriverContext.DataToMetaDataModeTag);
//        ParametersKey.ReadWString(MODE_CHANGE_TAG_DESC_FOR_METADATA_TO_BITMAP, STRING_LEN_NULL_TERMINATED, &DriverContext.MetaDataToBitmapModeTag, 
//                                  MODE_CHANGE_TAG_DESC_FOR_METADATA_TO_BITMAP_DEFAULT_VALUE, TRUE, NonPagedPool);
//        DriverContext.usMetaDataToBitmapModeTagLen = ReturnStringLenWInUSHORT(DriverContext.MetaDataToBitmapModeTag);
        KernelTagsKey.ReadAString(MODE_CHANGE_TAG_DESC_FOR_METADATA_TO_DATA, STRING_LEN_NULL_TERMINATED, &DriverContext.MetaDataToDataModeTag, 
                                  MODE_CHANGE_TAG_DESC_FOR_METADATA_TO_DATA_DEFAULT_VALUE, TRUE, NonPagedPool);
        DriverContext.usMetaDataToDataModeTagLen = ReturnStringLenAInUSHORT(DriverContext.MetaDataToDataModeTag);

        ParametersKey.FlushKey();
        ParametersKey.CloseKey();
        KernelTagsKey.CloseKey();
    } else {
        DriverContext.DBHighWaterMarks[ecServiceNotStarted] = DEFAULT_DB_HIGH_WATERMARK_SERVICE_NOT_STARTED;
        DriverContext.DBLowWaterMarkWhileServiceRunning = DEFAULT_DB_LOW_WATERMARK_SERVICE_RUNNING;
        DriverContext.DBHighWaterMarks[ecServiceRunning] = DEFAULT_DB_HIGH_WATERMARK_SERVICE_RUNNING;
        DriverContext.DBHighWaterMarks[ecServiceShutdown] = DEFAULT_DB_HIGH_WATERMARK_SERVICE_SHUTDOWN;
        DriverContext.DBToPurgeWhenHighWaterMarkIsReached = DEFAULT_DB_TO_PURGE_HIGH_WATERMARK_REACHED;
        DriverContext.MaximumBitmapBufferMemory = DEFAULT_MAXIMUM_BITMAP_BUFFER_MEMORY;
        ulVolumeDataCapture = DEFAULT_VOLUME_DATA_CAPTURE;
        ulVolumeDataFilteringForNewVolumes = DEFAULT_VOLUME_DATA_FILTERING_FOR_NEW_VOLUMES;
        DriverContext.FileWriterThreadPriority = DEFAULT_VOLUME_FILE_WRITERS_THREAD_PRIORITY;
        DriverContext.ulDataToDiskLimitPerVolumeInMB = DEFAULT_VOLUME_DATA_TO_DISK_LIMIT_IN_MB;
        DriverContext.ulMinFreeDataToDiskLimitPerVolumeInMB = DEFAULT_VOLUME_MIN_FREE_DATA_TO_DISK_LIMIT_IN_MB;
        DriverContext.ulDataNotifyThresholdInKB = DEFAULT_VOLUME_DATA_NOTIFY_LIMIT_IN_KB;
        DriverContext.ulNumberOfCommonFileWriters = DEFAULT_NUMBER_OF_COMMON_FILE_WRITERS;
        DriverContext.ulNumberOfThreadsPerFileWriter = DEFAULT_NUMBER_OF_THREADS_FOR_EACH_FILE_WRITER;
        DriverContext.ulSecondsBetweenNoMemoryLogEvents = DEFAULT_TIME_IN_SECONDS_BETWEEN_NO_MEMORY_LOG_EVENTS;
        DriverContext.ulDaBFreeThresholdForFileWrite = DEFAULT_FREE_THREASHOLD_FOR_FILE_WRITE;
        DriverContext.ulDaBThresholdPerVolumeForFileWrite = DEFAULT_VOLUME_THRESHOLD_FOR_FILE_WRITE;
        DriverContext.ulSecondsMaxWaitTimeOnFailOver = DEFAULT_MAX_TIME_IN_SECONDS_ON_FAIL_OVER;
        ulVFDisabledForNewVolumes = DEFAULT_VOLUME_FILTERING_DISABLED_FOR_NEW_VOLUMES;
        ulVFDisabledForNewClusterVolumes = DEFAULT_VOLUME_FILTERING_DISABLED_FOR_NEW_CLUSTER_VOLUMES;
        ulWorkerThreadPriority = DEFAULT_WORKER_THREAD_PRIORITY;
        DriverContext.ulMaxNonPagedPool = DEFAULT_MAX_NON_PAGED_POOL_IN_MB;
        DriverContext.ulMaxWaitTimeForIrpCompletion = DEFAULT_MAX_WAIT_TIME_FOR_IRP_COMPLETION_IN_MINUTES;
        DriverContext.ulMaxDataAllocLimitInPercentage = DEFAULT_MAX_DATA_ALLOCATION_LIMIT;
        DriverContext.ulAvailableDataBlockCountInPercentage = DEFAULT_FREE_AND_LOCKED_DATA_BLOCKS_COUNT;
    }

    if(DriverContext.ulMinFreeDataToDiskLimitPerVolumeInMB > DriverContext.ulDataToDiskLimitPerVolumeInMB){
        DriverContext.ulMinFreeDataToDiskLimitPerVolumeInMB = DriverContext.ulDataToDiskLimitPerVolumeInMB;
    }

    if (ulVFDisabledForNewVolumes) {
        DriverContext.bDisableVolumeFilteringForNewVolumes = true;
    }

    if (ulVFDisabledForNewClusterVolumes) {
        DriverContext.bDisableVolumeFilteringForNewClusterVolumes = true;
    }

    if (DriverContext.FileWriterThreadPriority < MINIMUM_VOLUME_FILE_WRITERS_THREAD_PRIORITY) {
        DriverContext.FileWriterThreadPriority = MINIMUM_VOLUME_FILE_WRITERS_THREAD_PRIORITY;
    }

    if (0 == DriverContext.DBToPurgeWhenHighWaterMarkIsReached) {
        DriverContext.DBToPurgeWhenHighWaterMarkIsReached = DEFAULT_DB_TO_PURGE_HIGH_WATERMARK_REACHED;
    }

    if (ulVolumeDataCapture) {
        DriverContext.bEnableDataCapture = true;
    } else {
        DriverContext.bEnableDataCapture = false;
    }

    if (ulVolumeDataFilteringForNewVolumes) {
        DriverContext.bEnableDataFilteringForNewVolumes = TRUE;
    } else {
        DriverContext.bEnableDataFilteringForNewVolumes = FALSE;
    }

    if (ulDataFiles) {
        DriverContext.bEnableDataFiles = true;
    } else {
        DriverContext.bEnableDataFiles = false;
    }

    if (ulDataFilesForNewVolumes) {
        DriverContext.bEnableDataFilesForNewVolumes = true;
    } else {
        DriverContext.bEnableDataFilesForNewVolumes = false;
    }

    DriverContext.bServiceSupportsDataCapture = false;
    DriverContext.bServiceSupportsDataFiles = false;
    DriverContext.bS2SupportsDataFiles = false;
    DriverContext.bS2is64BitProcess = false;
    DriverContext.ulMaxNonPagedPool *= MEGABYTES;
    DriverContext.ulMaxWaitTimeForIrpCompletion *= SECONDS_IN_MINUTE;   

    Status = InitializeWorkQueue(&DriverContext.WorkItemQueue, NULL, ulWorkerThreadPriority);
    if (!NT_SUCCESS(Status))
    {
        CleanDriverContext();
        return Status;
    }

    Status = AllocateAndInitializeDataPool();
    if (!NT_SUCCESS(Status)) {
        Status = STATUS_SUCCESS;
    }
    
    SetSystemTimeChangeNotification();

    return Status;
}

VOID
SetSystemTimeChangeNotification()
{
    PCALLBACK_OBJECT    SystemTimeCallbackObject;
    OBJECT_ATTRIBUTES TimeObjectAttributes;
    UNICODE_STRING TimerName;

    RtlInitUnicodeString(&TimerName, L"\\Callback\\SetSystemTime");
    InitializeObjectAttributes(&TimeObjectAttributes, &TimerName, OBJ_KERNEL_HANDLE, NULL, NULL);

    ExCreateCallback(&SystemTimeCallbackObject, &TimeObjectAttributes, FALSE, FALSE);
    DriverContext.SystemTimeCallbackPointer = ExRegisterCallback(SystemTimeCallbackObject, 
                                                                                        SystemTimeChangeNotification, NULL);
    ObDereferenceObject(SystemTimeCallbackObject);
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
    KIRQL OldIrql;

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

    if (DriverContext.ulFlags & DC_FLAGS_SYMLINK_CREATED) {
        UNICODE_STRING          SymLinkName;

        RtlInitUnicodeString(&SymLinkName, INMAGE_SYMLINK_NAME);
        DriverContext.ulFlags &= ~DC_FLAGS_SYMLINK_CREATED;
        IoDeleteSymbolicLink(&SymLinkName);
    }

    if (DriverContext.ulFlags & DC_FLAGS_DEVICE_CREATED) {
        DriverContext.ulFlags &= ~DC_FLAGS_DEVICE_CREATED;
        IoUnregisterShutdownNotification(DriverContext.ControlDevice);
        IoDeleteDevice(DriverContext.ControlDevice);
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

    return;
}

BOOLEAN
SetDriverContextFlag(ULONG  ulFlags)
{
    KIRQL               OldIrql;
    BOOLEAN             bReturn = TRUE;
    
    KeAcquireSpinLock(&DriverContext.Lock, &OldIrql);
    if (DriverContext.ulFlags & ulFlags)
        bReturn = FALSE;
    else
        DriverContext.ulFlags |= ulFlags;
    KeReleaseSpinLock(&DriverContext.Lock, OldIrql);

    return bReturn;
}

VOID
UnsetDriverContextFlag(ULONG    ulFlags)
{
    KIRQL               OldIrql;
    
    KeAcquireSpinLock(&DriverContext.Lock, &OldIrql);
    DriverContext.ulFlags &= ~ulFlags;
    KeReleaseSpinLock(&DriverContext.Lock, OldIrql);

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
        InVolDbgPrint(DL_INFO, DM_VOLUME_CONTEXT, 
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
    
    return STATUS_SUCCESS;
}

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

VOID
WakeServiceThread(BOOLEAN bDriverContextLockAcquired)
{
    KIRQL   OldIrql;

    if (FALSE == bDriverContextLockAcquired)
        KeAcquireSpinLock(&DriverContext.Lock, &OldIrql);
    
    InVolDbgPrint(DL_INFO, DM_VOLUME_STATE, 
        ("WakeServiceThread: Waking service thread, ServiceState = %s\n", GetServiceStateString() ));
    KeSetEvent(&DriverContext.ServiceThreadWakeEvent, IO_DISK_INCREMENT, FALSE);

    if (FALSE == bDriverContextLockAcquired)
        KeReleaseSpinLock(&DriverContext.Lock, OldIrql);
}

PBASIC_DISK
GetBasicDiskFromDriverContext(ULONG ulDiskSignature, ULONG ulDiskNumber)
{
    KIRQL       OldIrql;
    PBASIC_DISK  pBasicDisk;
    BOOLEAN     bFound = FALSE;

    KeAcquireSpinLock(&DriverContext.Lock, &OldIrql);
    pBasicDisk = (PBASIC_DISK)DriverContext.HeadForBasicDisks.Flink;
    while(pBasicDisk != (PBASIC_DISK)&DriverContext.HeadForBasicDisks) {
        if (ulDiskSignature != 0) {
            if(pBasicDisk->ulDiskSignature == ulDiskSignature) {
                 bFound = TRUE;
                ReferenceBasicDisk(pBasicDisk);
                break;
            }
        } else if((pBasicDisk->ulDiskNumber != 0xfffffff) && (pBasicDisk->ulDiskNumber == ulDiskNumber)) {
            bFound = TRUE;
            ReferenceBasicDisk(pBasicDisk);
            break;
        }
        pBasicDisk = (PBASIC_DISK)pBasicDisk->ListEntry.Flink;
    }
    KeReleaseSpinLock(&DriverContext.Lock, OldIrql);

    if (FALSE == bFound)
        pBasicDisk = NULL;

    return pBasicDisk;
}

PBASIC_DISK
AddBasicDiskToDriverContext(ULONG ulDiskSignature)
{
    KIRQL       OldIrql;
    PBASIC_DISK  pBasicDisk;

    pBasicDisk = GetBasicDiskFromDriverContext(ulDiskSignature);
    if (NULL == pBasicDisk) {
        pBasicDisk = AllocateBasicDisk();
        pBasicDisk->ulDiskSignature = ulDiskSignature;
        ReferenceBasicDisk(pBasicDisk);
        KeAcquireSpinLock(&DriverContext.Lock, &OldIrql);
        InsertHeadList(&DriverContext.HeadForBasicDisks, (PLIST_ENTRY)pBasicDisk);
        KeReleaseSpinLock(&DriverContext.Lock, OldIrql);
    }

    return pBasicDisk;
}

PBASIC_VOLUME
AddBasicVolumeToDriverContext(ULONG ulDiskSignature, PVOLUME_CONTEXT VolumeContext)
{
    PBASIC_DISK      pBasicDisk;
    PBASIC_VOLUME    pBasicVolume;

    pBasicDisk = GetBasicDiskFromDriverContext(ulDiskSignature);
    if (NULL == pBasicDisk)
        return NULL;

    pBasicVolume = AddVolumeToBasicDisk(pBasicDisk, VolumeContext);

    return pBasicVolume;
}


#if (NTDDI_VERSION >= NTDDI_VISTA)

PBASIC_DISK
GetBasicDiskFromDriverContext(GUID  Guid)
{
    KIRQL       OldIrql;
    PBASIC_DISK  pBasicDisk;
    BOOLEAN     bFound = FALSE;

    KeAcquireSpinLock(&DriverContext.Lock, &OldIrql);
    pBasicDisk = (PBASIC_DISK)DriverContext.HeadForBasicDisks.Flink;
    while(pBasicDisk != (PBASIC_DISK)&DriverContext.HeadForBasicDisks) {
        if (sizeof(GUID) == RtlCompareMemory(&pBasicDisk->DiskId, &Guid, sizeof(GUID)) ) {
            bFound = TRUE;
            ReferenceBasicDisk(pBasicDisk);
            break;
        }
        pBasicDisk = (PBASIC_DISK)pBasicDisk->ListEntry.Flink;
    }
    KeReleaseSpinLock(&DriverContext.Lock, OldIrql);

    if (FALSE == bFound)
        pBasicDisk = NULL;

    return pBasicDisk;
}

PBASIC_DISK
AddBasicDiskToDriverContext(GUID  Guid)
{
    KIRQL       OldIrql;
    PBASIC_DISK  pBasicDisk;

    pBasicDisk = GetBasicDiskFromDriverContext(Guid);
    if (NULL == pBasicDisk) {
        pBasicDisk = AllocateBasicDisk();
        RtlCopyMemory(&pBasicDisk->DiskId, &Guid, sizeof(GUID));
        ReferenceBasicDisk(pBasicDisk);
        KeAcquireSpinLock(&DriverContext.Lock, &OldIrql);
        InsertHeadList(&DriverContext.HeadForBasicDisks, (PLIST_ENTRY)pBasicDisk);
        KeReleaseSpinLock(&DriverContext.Lock, OldIrql);
    }

    return pBasicDisk;
}


PBASIC_VOLUME
AddBasicVolumeToDriverContext(GUID  Guid, PVOLUME_CONTEXT VolumeContext)
{
    PBASIC_DISK      pBasicDisk = NULL;
    PBASIC_VOLUME    pBasicVolume = NULL;

    pBasicDisk = GetBasicDiskFromDriverContext(Guid);
    if (NULL == pBasicDisk)
        return NULL;

    pBasicVolume = AddVolumeToBasicDisk(pBasicDisk, VolumeContext);

    return pBasicVolume;
}
#endif

PBASIC_VOLUME
GetBasicVolumeFromDriverContext(PWCHAR pGUID)
{
    PBASIC_DISK     pBasicDisk = NULL;
    PBASIC_VOLUME   pBasicVolume = NULL;
    BOOLEAN         bFound = FALSE;
    KIRQL           OldIrql;

    if (IsListEmpty(&DriverContext.HeadForBasicDisks))
        return NULL;

    KeAcquireSpinLock(&DriverContext.Lock, &OldIrql);
    pBasicDisk = (PBASIC_DISK)DriverContext.HeadForBasicDisks.Flink;
    while ((PLIST_ENTRY)pBasicDisk != &DriverContext.HeadForBasicDisks) {
        pBasicVolume = (PBASIC_VOLUME)pBasicDisk->VolumeList.Flink;
        while ((PLIST_ENTRY)pBasicVolume != &pBasicDisk->VolumeList) {
            if (GUID_SIZE_IN_BYTES == RtlCompareMemory(pGUID, pBasicVolume->GUID, GUID_SIZE_IN_BYTES)) {
                ReferenceBasicVolume(pBasicVolume);
                bFound = TRUE;
                break;
            }

            pBasicVolume = (PBASIC_VOLUME)pBasicVolume->ListEntry.Flink;
        }

        if (TRUE == bFound)
            break;

        pBasicDisk = (PBASIC_DISK)pBasicDisk->ListEntry.Flink;
    }

    KeReleaseSpinLock(&DriverContext.Lock, OldIrql);

    if (TRUE == bFound)
        return pBasicVolume;

    return NULL;
}

NTSTATUS
GetListOfVolumes(PLIST_ENTRY ListHead)
{
    KIRQL       OldIrql;
    NTSTATUS    Status = STATUS_SUCCESS;
    PLIST_NODE  pNode = NULL;
    PVOLUME_CONTEXT VolumeContext = NULL;

    InitializeListHead(ListHead);

    KeAcquireSpinLock(&DriverContext.Lock, &OldIrql);
    VolumeContext = (PVOLUME_CONTEXT)DriverContext.HeadForVolumeContext.Flink;
    while((PLIST_ENTRY)VolumeContext != &DriverContext.HeadForVolumeContext) {
        pNode = AllocateListNode();
        if (NULL == pNode) {
            Status = STATUS_NO_MEMORY;
            break;
        }

        pNode->pData = ReferenceVolumeContext(VolumeContext);
        InsertTailList(ListHead, &pNode->ListEntry);
        VolumeContext = (PVOLUME_CONTEXT)VolumeContext->ListEntry.Flink;
    }
    KeReleaseSpinLock(&DriverContext.Lock, OldIrql);

    if (STATUS_SUCCESS != Status) {
        // Free all nodes.
        while (!IsListEmpty(ListHead)) {
            pNode = (PLIST_NODE)RemoveHeadList(ListHead);
            DereferenceVolumeContext((PVOLUME_CONTEXT)pNode->pData);
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
 * This Function read the Perstent Time stamp and the Sequence Number
 * from the Registry. If the registry is not present it creates the new one with 
 * as the default value given
*/
VOID
GetPersistentValuesFromRegistry(Registry *pParametersKey)
{
    NTSTATUS        Status = STATUS_SUCCESS;
    ULONG           ulcbRead = 0;
    PVOID           pAddress = NULL;
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
                ("and Default Sequence Number %#I64x\n",0));
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
                        if (ShutdownMarker == DirtyShutdown)//means that the last shutdown was dirty. We will bump the time stamp.
                            DriverContext.ullLastTimeStamp = pPersistentInfo->PersistentValues[i].Data + TIME_STAMP_BUMP;
                        else//Means that the last shutdown was clean. We can use the same time stamp.
                            DriverContext.ullLastTimeStamp = pPersistentInfo->PersistentValues[i].Data ;
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
        InMageFltLogError(DriverContext.ControlDevice, __LINE__, INVOLFLT_READ_LAST_PERSISTENT_VALUE_FAILURE,
                          STATUS_SUCCESS, Status);
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

ULONG GetFreeThresholdForFileWrite(PVOLUME_CONTEXT VolumeContext) {
    ULONG ulDaBFreeThresholdForFileWrite = ValidateFreeThresholdForFileWrite(DriverContext.ulDaBFreeThresholdForFileWrite);
    ulDaBFreeThresholdForFileWrite = (DriverContext.ulDataBlockAllocated * ulDaBFreeThresholdForFileWrite) / 100 ;
    ULONG ulNumDaBPerDirtyBlock = VolumeContext->ulMaxDataSizePerDataModeDirtyBlock / DriverContext.ulDataBlockSize;
    if (ulDaBFreeThresholdForFileWrite < ulNumDaBPerDirtyBlock) {
        ulDaBFreeThresholdForFileWrite = ulNumDaBPerDirtyBlock;
    }
    return ulDaBFreeThresholdForFileWrite;
}

ULONG GetThresholdPerVolumeForFileWrite(PVOLUME_CONTEXT VolumeContext) {
    ULONG ulDaBThresholdPerVolumeForFileWrite = ValidateThresholdPerVolumeForFileWrite(DriverContext.ulDaBThresholdPerVolumeForFileWrite);
    ulDaBThresholdPerVolumeForFileWrite = ((DriverContext.ulMaxDataSizePerVolume / DriverContext.ulDataBlockSize) * ulDaBThresholdPerVolumeForFileWrite) / 100;
    ULONG ulNumDaBPerDirtyBlock = VolumeContext->ulMaxDataSizePerDataModeDirtyBlock / DriverContext.ulDataBlockSize;
    if (ulDaBThresholdPerVolumeForFileWrite < ulNumDaBPerDirtyBlock) {
        ulDaBThresholdPerVolumeForFileWrite = ulNumDaBPerDirtyBlock;
    } 
    return ulDaBThresholdPerVolumeForFileWrite;
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

ULONG ValidateMaxDataSizePerVolume(ULONG ulMaxDataSizePerVolume) {
    //Min  3 * 4Mb
    //Default 32bit -- 12 * 4Mb, 64 bit -- 56 * 4 MB
    //Max DataPoolSize, 
    //Aligned lMaxDataSizePerDataModeDirtyBlock
    if (0 == ulMaxDataSizePerVolume) {
        ulMaxDataSizePerVolume = DEFAULT_DATA_DIRTY_BLOCKS_PER_VOLUME * DriverContext.ulMaxDataSizePerDataModeDirtyBlock;
    }

    if (0 != (ulMaxDataSizePerVolume % DriverContext.ulMaxDataSizePerDataModeDirtyBlock)) {
        ulMaxDataSizePerVolume = (ulMaxDataSizePerVolume / DriverContext.ulMaxDataSizePerDataModeDirtyBlock) * DriverContext.ulMaxDataSizePerDataModeDirtyBlock;
    }

    if (ulMaxDataSizePerVolume < (MINIMUM_DATA_DIRTY_BLOCKS_PER_VOLUME * DriverContext.ulMaxDataSizePerDataModeDirtyBlock)) {
        ulMaxDataSizePerVolume = MINIMUM_DATA_DIRTY_BLOCKS_PER_VOLUME * DriverContext.ulMaxDataSizePerDataModeDirtyBlock;
    }
    if (ulMaxDataSizePerVolume > DriverContext.ullDataPoolSize) {
        ulMaxDataSizePerVolume = (ULONG)DriverContext.ullDataPoolSize;
    }

    if (0 != (ulMaxDataSizePerVolume % DriverContext.ulMaxDataSizePerDataModeDirtyBlock)) {
        ulMaxDataSizePerVolume = (ulMaxDataSizePerVolume / DriverContext.ulMaxDataSizePerDataModeDirtyBlock) * DriverContext.ulMaxDataSizePerDataModeDirtyBlock;
    }
    return ulMaxDataSizePerVolume;
}

ULONG ValidateMaxSetLockedDataBlocks(ULONG ulMaxSetLockedDataBlocks) {
    //Default 32bit --- 20, 64bit --- 60
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

ULONG ValidateThresholdPerVolumeForFileWrite(ULONG ulDaBThresholdPerVolumeForFileWrite) {
    if (ulDaBThresholdPerVolumeForFileWrite > 100) {
        ulDaBThresholdPerVolumeForFileWrite = 100;
    }
    return ulDaBThresholdPerVolumeForFileWrite;
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
