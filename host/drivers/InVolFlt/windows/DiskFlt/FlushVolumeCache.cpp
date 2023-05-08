#include "FltFunc.h"
#include "misc.h"
#include "VContext.h"

NTSTATUS
InDskFltCreateFlushVolumeCachesThread()
/*
Routine Description:
    This function creates system threads and associated resources to flush volumes

Arguments:
    None

Return Value:
    STATUS_INVALID_DEVICE_STATE
        If flush thread resources are already created.

    STATUS_INSUFFICIENT_RESOURCES
        If memory allocation for thread resources couldn't be allocated.

    STATUS_SUCCESS
        All thread and associated resources are created successfully.
*/
{
    NTSTATUS            Status = STATUS_SUCCESS;
    ULONG               ulIndex = 0;
    HANDLE              hThread = NULL;
    PKTHREAD            *pFlushVolumeCacheThreads = NULL;
    PKEVENT             hFlushThreadTerminated = NULL;
    KIRQL               oldIrql;

    // Don't handle error conditions
    if (DriverContext.pFlushVolumeCacheThreads || DriverContext.hFlushThreadTerminated) {
        Status = STATUS_INVALID_DEVICE_STATE;
        goto Cleanup;
    }

    pFlushVolumeCacheThreads = (PKTHREAD*) ExAllocatePoolWithTag(NonPagedPool, DriverContext.ulNumFlushVolumeThreads * sizeof(PKTHREAD), TAG_FS_FLUSH);
    if (NULL == pFlushVolumeCacheThreads) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        InDskFltWriteEvent(INDSKFLT_ERROR_MEM_ALLOC_FLUSH_VOL_THREADS);
        goto Cleanup;
    }

    hFlushThreadTerminated = (PKEVENT)ExAllocatePoolWithTag(NonPagedPool, DriverContext.ulNumFlushVolumeThreads * sizeof(KEVENT), TAG_FS_FLUSH);
    if (NULL == hFlushThreadTerminated) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        InDskFltWriteEvent(INDSKFLT_ERROR_MEM_ALLOC_FLUSH_WAIT_EVENTS);
        goto Cleanup;
    }

    KeAcquireSpinLock(&DriverContext.Lock, &oldIrql);
    DriverContext.pFlushVolumeCacheThreads = pFlushVolumeCacheThreads;
    DriverContext.hFlushThreadTerminated = hFlushThreadTerminated;
    KeReleaseSpinLock(&DriverContext.Lock, oldIrql);

    pFlushVolumeCacheThreads = NULL;
    hFlushThreadTerminated = NULL;

    for (ulIndex = 0; ulIndex < DriverContext.ulNumFlushVolumeThreads; ulIndex++) {
        DriverContext.pFlushVolumeCacheThreads[ulIndex] = NULL;

        KeInitializeEvent(&DriverContext.hFlushThreadTerminated[ulIndex], NotificationEvent, FALSE);

        // Create System Thread for flushing file system
        Status = PsCreateSystemThread(&hThread,
            THREAD_ALL_ACCESS,
            NULL,
            NULL,
            NULL,
            InDskFltFlushVolumeCache,
            &DriverContext.hFlushThreadTerminated[ulIndex]);

        if (!NT_SUCCESS(Status))
        {
            DriverContext.pFlushVolumeCacheThreads[ulIndex] = NULL;
            goto Cleanup;
        }

        Status = ObReferenceObjectByHandle(hThread, SYNCHRONIZE, NULL, KernelMode, (PVOID *)&DriverContext.pFlushVolumeCacheThreads[ulIndex], NULL);
        ZwClose(hThread);
        if (!NT_SUCCESS(Status)) {
            DriverContext.pFlushVolumeCacheThreads[ulIndex] = NULL;
            goto Cleanup;
        }

        DriverContext.ulNumFlushVolumeThreadsInProgress++;
    }

Cleanup:
    SafeExFreePoolWithTag(pFlushVolumeCacheThreads, TAG_FS_FLUSH);
    SafeExFreePoolWithTag(hFlushThreadTerminated, TAG_FS_FLUSH);

    if (!NT_SUCCESS(Status))
    {
        KeSetEvent(&DriverContext.StopFSFlushEvent, IO_NO_INCREMENT, FALSE);
    }

    return Status;
}

VOID
InDskFltFlushVolumeCache(
    _In_    PVOID Context
)
/*
Routine Description:
    This function implements flush volume thread functionality.
    Each thread waits for either start or, stop flush event.
        If it receives stop flush, 
            It de-allocates all resources allocated.
            Sets event to indicate that it has exited.
            it exits.
        If it receives start flush,
            It keeps on flushing next volume from Volumes list until,
                It volume list is empty
                Or, It receives stop flush event.
            As part of stop flush it does cleanup as done for stop flush.
Arguments:
    Context
        Event that need to be set once this thread exits.
Return Value:
    None
*/
{
    NTSTATUS                Status;
    PKEVENT                 waitObjects[2];
    BOOLEAN                 exit = FALSE;
    PVOLUME_NODE            pVolumeNode = NULL;
    LARGE_INTEGER           TimeOut = { 0 };
    PLARGE_INTEGER          pTimeOut = NULL;
    PLIST_ENTRY             listEntry;
    KIRQL                   oldIrql;
    ULONG                   ulNumberOfEventsToWait = 2;
    PKEVENT                 TerminateEvent = NULL;
    PVOLUME_DISK_EXTENTS    pVolExtents = NULL;
    DWORD                   dwVolExtentSize = 0;

    TerminateEvent = (PKEVENT)Context;

    dwVolExtentSize = sizeof(VOLUME_DISK_EXTENTS) + MAX_NUM_DISKS_SUPPORTED * sizeof(((PVOLUME_DISK_EXTENTS)0)->Extents[0]);

    pVolExtents = (PVOLUME_DISK_EXTENTS)ExAllocatePoolWithTag(NonPagedPool, dwVolExtentSize, TAG_FS_FLUSH);
    if (NULL == pVolExtents) {
        // Memory Allocation Failed
        InDskFltWriteEvent(INDSKFLT_ERROR_MEM_ALLOC_FAILURE_VOL_EXTENTS);
        goto Cleanup;
    }

    waitObjects[0] = &DriverContext.StopFSFlushEvent;
    waitObjects[1] = &DriverContext.StartFSFlushEvent;

    do
    {
        Status = KeWaitForMultipleObjects(
                        ulNumberOfEventsToWait,
                        (PVOID *)&waitObjects[0],
                        WaitAny,
                        Executive,
                        KernelMode,
                        FALSE,
                        pTimeOut,
                        NULL);

        if (STATUS_WAIT_0 == Status) {
            exit = true;
            break;
        }

        // Now wait for only shutdown event
        ulNumberOfEventsToWait = 1;
        pTimeOut = &TimeOut;

        KeAcquireSpinLock(&DriverContext.Lock, &oldIrql);
        exit = IsListEmpty(&DriverContext.VolumeNameList);
        if (!exit) {
            listEntry = RemoveHeadList(&DriverContext.VolumeNameList);
            pVolumeNode = (PVOLUME_NODE)listEntry;
        }
        KeReleaseSpinLock(&DriverContext.Lock, oldIrql);

        if (!exit) {
            InDskFltFlushVolumeBuffer(pVolumeNode->VolumeName.Buffer, 
                                      DriverContext.pProtectedDiskNumbers,
                                      DriverContext.ulNumProtectedDisks,
                                      pVolExtents,
                                      dwVolExtentSize);

            ExFreePoolWithTag(pVolumeNode, TAG_FS_FLUSH);
        }
    } while (!exit);

Cleanup:
    SafeExFreePool(pVolExtents, TAG_FS_FLUSH);
    InDskFltWriteEvent(INDSKFLT_INFO_TERMINATE_FLUSH_THREAD, (ULONG)PsGetCurrentThreadId());
    KeSetEvent(TerminateEvent, IO_NO_INCREMENT, FALSE);
    PsTerminateSystemThread(STATUS_SUCCESS);
}

VOID 
CleanupFlushVolumeNodes()
/*
Routine Description:
    Cleanup routine for flush volume threads. It does following,
        Releases all flush volume thread references
        De-Allocates all thread resources.
        Removes all volume node from global list and moves it into a local list.
        It releases all resources allocated for volume nodes.

Arguments:
    None

Return Value:
    None
*/
{
    PLIST_ENTRY     listEntry = NULL;
    KIRQL           oldIrql;
    LIST_ENTRY      VolumeList;
    ULONG           ulIdx = 0;

    // Signal All Volume threads to stop
    KeSetEvent(&DriverContext.StopFSFlushEvent, IO_NO_INCREMENT, FALSE);

    InitializeListHead(&VolumeList);

    // Close all reference to volume cache thread
    KeAcquireSpinLock(&DriverContext.Lock, &oldIrql);

    // Free All thread references
    for (ulIdx = 0; ulIdx < DriverContext.ulNumFlushVolumeThreadsInProgress; ulIdx++) {
        SafeObDereferenceObject(DriverContext.pFlushVolumeCacheThreads[ulIdx]);
    }

    // Copy Volume node list to another list
    while (!IsListEmpty(&DriverContext.VolumeNameList)) {
        listEntry = RemoveHeadList(&DriverContext.VolumeNameList);
        InsertTailList(&VolumeList, listEntry);
    }

    KeReleaseSpinLock(&DriverContext.Lock, oldIrql);

    // Remove all volume node entry
    while (!IsListEmpty(&VolumeList)) {
        PVOLUME_NODE VolumeNode = (PVOLUME_NODE)RemoveHeadList(&VolumeList);
        ExFreePoolWithTag(VolumeNode, TAG_FS_FLUSH);
    }
}

BOOLEAN
InDskFltFlushVolumeBuffer(
    _In_    PWCHAR                  VolumeSymName,
    _In_    PULONG                  pProtectedDiskNumbers,
    _In_    ULONG                   ulNumProtectedDisks,
    _In_    PVOLUME_DISK_EXTENTS    pVolExtents,
    _In_    DWORD                   dwVolExtentsSize
    )
/*
Routine Description:
    Function to flush a given volume. It does following,
        Figures if current volume need to flushed.
        Following volumes are ignored for flush,
            Remote/Network Volume
            CDROM
            RAM disk
            Non-Fixed volume
        It figures all disk extents for given volume. 
        If any of these disk is protected, 
            it flushes the volume using ZwFlushBuffersFile.

Arguments:
    VolumeSymName
        Symbolic name for the volume.

    pProtectedDiskNumbers
        An array containing disk number which are either proteted or, 
                    storage space disk.
    ulNumProtectedDisks
        Number of elements in pProtectedDiskNumbers.
    pVolExtents
        A temporary buffer used to get Disk Extents array for volume.
    dwVolExtentsSize
        Size of pVolExtents array.

Return Value:
    TRUE
        If volume is flushed successfully.

    FALSE
        Any internal api error encountered.
        None of disks for this volume is protected.
        ZwFlushBuffersFile failed.
    */
{
    BOOLEAN                         bShouldFlush = FALSE;
    NTSTATUS                        Status = STATUS_SUCCESS;
    PFILE_OBJECT                    pFileObj = NULL;
    OBJECT_ATTRIBUTES               ObjectAttributes;
    UNICODE_STRING                  usVolumeDeviceSymbolicLink;
    HANDLE                          FileHandle = NULL;
    IO_STATUS_BLOCK                 IoStatus;
    FILE_FS_DEVICE_INFORMATION      DeviceInfo;
    PDEVICE_OBJECT                  RelatedDeviceObject = NULL;
    PDEVICE_OBJECT                  FsDeviceObject = NULL;
    PDEVICE_OBJECT                  VolumeDeviceObject = NULL;

    RtlInitUnicodeString(&usVolumeDeviceSymbolicLink, VolumeSymName);
    if (0 == usVolumeDeviceSymbolicLink.Length) {
        goto Cleanup;
    }

    InitializeObjectAttributes(&ObjectAttributes,
        &usVolumeDeviceSymbolicLink,
        OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE,
        NULL,
        NULL);

    Status = ZwOpenFile(&FileHandle,
                        FILE_GENERIC_READ|FILE_GENERIC_WRITE,
                        &ObjectAttributes,
                        &IoStatus,
                        FILE_SHARE_READ|FILE_SHARE_WRITE,
                        FILE_SYNCHRONOUS_IO_NONALERT
                        );

    if (!NT_SUCCESS(Status)) {
        // Log an event
        InDskFltWriteEvent(INDSKFLT_ERROR_OPEN_VOL_HANDLE, VolumeSymName, Status);
        FileHandle = NULL;
        goto Cleanup;
    }

    // Query volume properties
    Status = ZwQueryVolumeInformationFile(FileHandle, &IoStatus, &DeviceInfo, sizeof(DeviceInfo), FileFsDeviceInformation);
    if (!NT_SUCCESS(Status)) {
        // Log an error
        InDskFltWriteEvent(INDSKFLT_ERROR_QUERY_VOLUME_INFORMATION, VolumeSymName, Status);
        goto Cleanup;
    }

    // Ignore remote volumes
    if (DeviceInfo.Characteristics & FILE_REMOTE_DEVICE) {
        InDskFltWriteEvent(INDSKFLT_INFO_SKIP_FLUSH_REMOTE_DRIVE, VolumeSymName);
        goto Cleanup;
    }

    // Ignore Network Volume
    // Ignore CDROM
    // Ignore Ram Disk
    if ((FILE_DEVICE_DISK != DeviceInfo.DeviceType) &&
        (FILE_DEVICE_DISK_FILE_SYSTEM != DeviceInfo.DeviceType)) {
        InDskFltWriteEvent(INDSKFLT_INFO_SKIP_FLUSH_NON_FIXED_DRIVE, VolumeSymName);
        goto Cleanup;
    }

    // Ignore removable media
    if (TEST_ALL_FLAGS(DeviceInfo.Characteristics, FILE_REMOVABLE_MEDIA)) {
        InDskFltWriteEvent(INDSKFLT_INFO_SKIP_FLUSH_REMOVABLE_DRIVE, VolumeSymName);
        goto Cleanup;
    }

    // Get File Object
    Status = ObReferenceObjectByHandle(FileHandle,
                                        0,
                                        *IoFileObjectType,
                                        KernelMode,
                                        (PVOID *)&pFileObj,
                                        NULL);

    if (!NT_SUCCESS(Status)) {
        InDskFltWriteEvent(INDSKFLT_ERROR_GET_FILE_OBJECT_FAILURE, VolumeSymName, Status);
        goto Cleanup;
    }

    RelatedDeviceObject = IoGetRelatedDeviceObject(pFileObj);
    if (!RelatedDeviceObject) {
        InDskFltWriteEvent(INDSKFLT_ERROR_VOLUME_INVALID_FILE_OBJECT, VolumeSymName);
        goto Cleanup;
    }

    FsDeviceObject = IoGetDeviceAttachmentBaseRef(RelatedDeviceObject);
    if (!FsDeviceObject) {
        InDskFltWriteEvent(INDSKFLT_ERROR_VOLUME_INVALID_FS_DEVICE_OBJECT, VolumeSymName);
        goto Cleanup;
    }

    Status = IoGetDiskDeviceObject(FsDeviceObject, &VolumeDeviceObject);
    if (!VolumeDeviceObject) {
        InDskFltWriteEvent(INDSKFLT_ERROR_GET_DEVICE_OBJECT_FAILURE, VolumeSymName, Status);
        goto Cleanup;
    }

    RtlZeroMemory(pVolExtents, dwVolExtentsSize);
    Status = InMageSendDeviceControl(VolumeDeviceObject,
                                     IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS,
                                     NULL,
                                     0,
                                     pVolExtents,
                                     dwVolExtentsSize,
                                     &IoStatus);
    if (!NT_SUCCESS(Status)) {
        // Log an event
        InDskFltWriteEvent(INDSKFLT_ERROR_GET_VOL_EXTENTS, VolumeSymName, Status);
        goto Cleanup;
    }

    // Flush volume only when one of disks in this volume is protected
    for (ULONG ulIdx = 0; ulIdx < pVolExtents->NumberOfDiskExtents; ulIdx++) {
        ULONG ulDiskNum = pVolExtents->Extents[ulIdx].DiskNumber;
        for (ULONG ulIdx1 = 0; ulIdx1 < ulNumProtectedDisks; ulIdx1++) {
            if (ulDiskNum == pProtectedDiskNumbers[ulIdx1]) {
                bShouldFlush = TRUE;
                break;
            }
        }

        if (bShouldFlush) {
            break;
        }
    }

    if (!bShouldFlush) {
        goto Cleanup;
    }

    Status = ZwFlushBuffersFile(FileHandle, &IoStatus);
    if (!NT_SUCCESS(Status)) {
        InDskFltWriteEvent(INDSKFLT_ERROR_FLUSH_VOLUME, VolumeSymName, Status);
        goto Cleanup;
    }

    bShouldFlush = TRUE;
    InDskFltWriteEvent(INDSKFLT_INFO_FLUSH_VOLUME, VolumeSymName, Status);
Cleanup:
    SafeObDereferenceObject(VolumeDeviceObject);
    SafeObDereferenceObject(FsDeviceObject);
    SafeObDereferenceObject(pFileObj);
    SafeCloseHandle(FileHandle);
    return bShouldFlush;
}

void
InDskFltFlushFileBuffers()
/*
Routine Description:
    Top level function to implement flush functionality.
    Steps,
        Allocate all flush thread and associated resources.
        Prepares an array consisting of all protected and storage space disks.
        Queries all volume symbolic links and prepares volume nodes list.
        Waits for all flush threads to return in given time.
        Sends stop event to all threads.
        Do thread and associated resources.

Arguments:
    None

Return Value:
    None
*/
{
    NTSTATUS                        Status = STATUS_SUCCESS;
    PWCHAR                          SymLinkList = NULL;
    PWCHAR                          SymLinkListIter = NULL;
    PULONG                          pProtectedDiskNumbers = NULL;
    PDEV_CONTEXT                    pDevContext = NULL;
    PLIST_ENTRY                     pDevExtList = NULL;
    KIRQL                           oldIrql;
    ULONG                           ulNumProtectedDisks = 0;

    PKEVENT                         *waitObjects = NULL;
    PKWAIT_BLOCK                    WaitBlocks = NULL;
    LARGE_INTEGER                   TimeOut;

    // Define timeout for flush
    TimeOut.QuadPart = (-1) * (DriverContext.liFsFlushTimeout.QuadPart) * (HUNDRED_NANO_SECS_IN_SEC);

    // If number of flush threads were not created properly
    // Don't flush
    KeAcquireSpinLock(&DriverContext.Lock, &oldIrql);
    if (DriverContext.ulNumFlushVolumeThreads != DriverContext.ulNumFlushVolumeThreadsInProgress) {
        KeReleaseSpinLock(&DriverContext.Lock, oldIrql);
        goto Cleanup;
    }
    KeReleaseSpinLock(&DriverContext.Lock, oldIrql);

    InDskFltWriteEvent(INDSKFLT_INFO_FLUSH_FILE_BUFFERS);

    // Prepare list of protected disk numbers.
    pProtectedDiskNumbers = (PULONG)ExAllocatePoolWithTag(NonPagedPool, MAX_NUM_DISKS_SUPPORTED * sizeof(ULONG), TAG_FS_FLUSH);
    if (NULL == pProtectedDiskNumbers) {
        // Memory Allocated failed
        InDskFltWriteEvent(INDSKFLT_ERROR_MEM_ALLOC_FAILURE_DISK_NUMS);
        goto Cleanup;
    }

    KeAcquireSpinLock(&DriverContext.Lock, &oldIrql);
    SafeExFreePoolWithTag(DriverContext.pProtectedDiskNumbers, TAG_FS_FLUSH);

    DriverContext.pProtectedDiskNumbers = pProtectedDiskNumbers;
    KeReleaseSpinLock(&DriverContext.Lock, oldIrql);

    waitObjects = (PKEVENT*)ExAllocatePoolWithTag(NonPagedPool, DriverContext.ulNumFlushVolumeThreads * sizeof(PKEVENT), TAG_FS_FLUSH);
    if (NULL == waitObjects) {
        InDskFltWriteEvent(INDSKFLT_ERROR_MEM_ALLOC_FAILURE_WAIT_OBJECTS);
        goto Cleanup;
    }

    WaitBlocks = (PKWAIT_BLOCK)ExAllocatePoolWithTag(NonPagedPool, DriverContext.ulNumFlushVolumeThreads * sizeof(KWAIT_BLOCK), TAG_FS_FLUSH);
    if (NULL == WaitBlocks) {
        InDskFltWriteEvent(INDSKFLT_ERROR_MEM_ALLOC_FAILURE_WAIT_BLOCKS);
        goto Cleanup;
    }

    Status = GetDeviceSymLinkList((LPGUID)&GUID_DEVINTERFACE_VOLUME, &SymLinkList);
    if (!NT_SUCCESS(Status) || (NULL == SymLinkList)) {
        // Failed to get Device Sym list
        InDskFltWriteEvent(INDSKFLT_ERROR_GET_SYMLINKLIST_FAILURE, Status);
        goto Cleanup;
    }

    KeAcquireSpinLock(&DriverContext.Lock, &oldIrql);

    pDevExtList = DriverContext.HeadForDevContext.Flink;

    for (; pDevExtList != &DriverContext.HeadForDevContext; pDevExtList = pDevExtList->Flink)
    {
        pDevContext = (PDEV_CONTEXT)pDevExtList;

        KeAcquireSpinLockAtDpcLevel(&pDevContext->Lock);

        // Ignore disks which
        //      don't have a device number OR
        //      disk is not filtered and it is not a storage space disk.
        // agent protects only physical disk.
        // For storage space we indskflt don't have mapping of physical and space disk.
        // Volumes are on storage space and not on physical disk.
        // Hence we do need to flush any volume which are on storage space
        if (!TEST_FLAG(pDevContext->ulFlags, DCF_DEVNUM_OBTAINED) ||
            (TEST_FLAG(pDevContext->ulFlags, DCF_FILTERING_STOPPED) &&
            (DriverContext.IsWindows8AndLater && 
            pDevContext->pDeviceDescriptor &&
            (BusTypeSpaces != pDevContext->pDeviceDescriptor->BusType))))
        {
            KeReleaseSpinLockFromDpcLevel(&pDevContext->Lock);
            continue;
        }

        pProtectedDiskNumbers[ulNumProtectedDisks] = pDevContext->ulDevNum;
        ++ulNumProtectedDisks;
        KeReleaseSpinLockFromDpcLevel(&pDevContext->Lock);
    }

    DriverContext.ulNumProtectedDisks = ulNumProtectedDisks;
    KeReleaseSpinLock(&DriverContext.Lock, oldIrql);

    if (0 == ulNumProtectedDisks) {
        InDskFltWriteEvent(INDSKFLT_INFO_NO_PROTECTED_DISKS);
        goto Cleanup;
    }

    SymLinkListIter = SymLinkList;

    while (0 != wcslen(SymLinkListIter)) {
        PVOLUME_NODE         VolumeNode;

        VolumeNode = (PVOLUME_NODE)ExAllocatePoolWithTag(NonPagedPool, sizeof(VOLUME_NODE), TAG_FS_FLUSH);
        if (NULL == VolumeNode) {
            InDskFltWriteEvent(INDSKFLT_ERROR_MEM_ALLOC_FAILURE_VOL_NODE);
            goto Cleanup;
        }

        InitializeListHead(&VolumeNode->ListEntry);
        RtlInitUnicodeString(&VolumeNode->VolumeName, SymLinkListIter);

        KeAcquireSpinLock(&DriverContext.Lock, &oldIrql);
        InsertTailList(&DriverContext.VolumeNameList, &VolumeNode->ListEntry);
        KeReleaseSpinLock(&DriverContext.Lock, oldIrql);

        SymLinkListIter += wcslen(SymLinkListIter) + 1;
    }

    KeAcquireSpinLock(&DriverContext.Lock, &oldIrql);
    if (IsListEmpty(&DriverContext.VolumeNameList)) {
        KeReleaseSpinLock(&DriverContext.Lock, oldIrql);
        goto Cleanup;
    }
    KeReleaseSpinLock(&DriverContext.Lock, oldIrql);

    for (ULONG ulIdx = 0; ulIdx < DriverContext.ulNumFlushVolumeThreads; ulIdx++) {
        waitObjects[ulIdx] = &DriverContext.hFlushThreadTerminated[ulIdx];
    }

    // Set event for threads to flush fs cache
    KeSetEvent(&DriverContext.StartFSFlushEvent, IO_DISK_INCREMENT, FALSE);

    // Wait for all threads to return for specified timeout
    Status = KeWaitForMultipleObjects(DriverContext.ulNumFlushVolumeThreadsInProgress,
                    (PVOID *)waitObjects,
                    WaitAll,
                    Executive,
                    KernelMode,
                    FALSE,
                    &TimeOut,
                    WaitBlocks);

    InDskFltWriteEvent(INDSKFLT_INFO_FLUSHED_FILE_SYSTEM, Status);

Cleanup:
    CleanupFlushVolumeNodes();
    SafeExFreePool(SymLinkList);
    SafeExFreePoolWithTag(WaitBlocks, TAG_FS_FLUSH);
    SafeExFreePoolWithTag(waitObjects, TAG_FS_FLUSH);
    return;
}

