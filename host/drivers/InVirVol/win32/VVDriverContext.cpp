#include "RegistryHelper.h"
#include "VVCommon.h"
#include "VVDriverContext.h"
#include "VVDevControl.h"
#include "VVolumeExport.h"
#include "VVDispatch.h"
#include "VVolumeRetentionLog.h"
#include "VVolumeRetentionTest.h"
#include "VVIDevControl.h"
#include "VsnapKernelAPI.h"

extern THREAD_POOL		*VsnapIOThreadPool;
NTSTATUS
InitializeDriverContext(
    PDRIVER_CONTEXT pDriverContext,
    PDRIVER_OBJECT DriverObject,
    PUNICODE_STRING    Registry)
{
    NTSTATUS            Status = STATUS_SUCCESS;
    UNICODE_STRING      usInmageDeviceDirectory, usControlDeviceName, usControlDeviceSymLinkName;
    PDEVICE_OBJECT      ControlDevice = NULL;
    PDEVICE_EXTENSION   DevExtension = NULL;
    PCOMMAND_QUEUE      CmdQueue = NULL;
    OBJECT_ATTRIBUTES   ObjectAttributes;
    KIRQL               OldIrql;
    USHORT              usLength;
    HANDLE              hRegistry = NULL;
    ULONG               ulValue;

// Asserts if IRQL is greater than APC_LEVEL.
PAGED_CODE();


    RtlZeroMemory(pDriverContext, sizeof(DRIVER_CONTEXT));
    

    InitializeListHead(&pDriverContext->DeviceContextHead);
    InitializeListHead(&pDriverContext->CmdQueueList);
    KeInitializeSpinLock(&pDriverContext->Lock);
    KeInitializeMutex(&pDriverContext->hMutex, 0);
    KeInitializeEvent(&pDriverContext->ShutdownEvent, NotificationEvent, FALSE);
    RtlInitializeBitMap(&pDriverContext->BitMap, pDriverContext->BufferForDeviceBitMap, BITMAPSIZE_IN_BITS);

    ExInitializeNPagedLookasideList(&DriverContext.CommandPool, NULL, NULL, 0, sizeof(COMMAND_STRUCT), VVTAG_COMMAND_STRUCT, 0 );
    ExInitializeNPagedLookasideList(&DriverContext.IOCommandPool, NULL, NULL, 0, sizeof(IO_COMMAND_STRUCT), VVTAG_COMMAND_IO_STRUCT, 0 );
    pDriverContext->ulFlags |= VV_DC_FLAGS_COMMAND_POOL_INITIALIZED;

    pDriverContext->usDriverName.Buffer = (PWSTR)ALLOC_MEMORY(Registry->MaximumLength, PagedPool);
    if (NULL == pDriverContext->usDriverName.Buffer) {
        return STATUS_NO_MEMORY;
    }

    pDriverContext->usDriverName.MaximumLength = Registry->MaximumLength;
    RtlCopyUnicodeString(&pDriverContext->usDriverName, Registry);

    usLength = Registry->Length + (wcslen(PARAMETERS_KEY)*sizeof(WCHAR));
    pDriverContext->DriverParameters.MaximumLength = usLength + sizeof(WCHAR);
    pDriverContext->DriverParameters.Buffer = (PWCHAR)ALLOC_MEMORY(pDriverContext->DriverParameters.MaximumLength,
                                            PagedPool);
    if (NULL == pDriverContext->DriverParameters.Buffer) {
        return STATUS_NO_MEMORY;
    }


    RtlCopyUnicodeString(&pDriverContext->DriverParameters, Registry);
    RtlAppendUnicodeToString(&pDriverContext->DriverParameters, PARAMETERS_KEY);
    pDriverContext->DriverParameters.Buffer[pDriverContext->DriverParameters.Length/sizeof(WCHAR)] = L'\0';
    pDriverContext->ulNumberOfVsnapThreads = DEFAULT_NUMBER_OF_THREADS_FOR_VSNAP_THREAD_POOL;
    pDriverContext->ulNumberOfVVolumeThreads = DEFAULT_NUMBER_OF_THREADS_FOR_VVOLUME_THREAD_POOL;
    pDriverContext->ulMemoryForFileHandleCache = DEFAULT_PAGED_MEM_FILE_HANDLE_CACHE_KB * KILO_BYTE - MEMORY_FOR_TAG;
    KeInitializeMutex(&pDriverContext->FileHandleCacheMutex, 0);
    Status = OpenKeyReadWrite(&pDriverContext->DriverParameters, &hRegistry, TRUE);
    if(NT_SUCCESS(Status)) {
        ReadULONG(hRegistry, NUMBER_OF_THREADS_FOR_VSNAP_THREAD_POOL, &ulValue, DEFAULT_NUMBER_OF_THREADS_FOR_VSNAP_THREAD_POOL, TRUE);
        if((ulValue >= 1) && (ulValue <= 64)) {
            pDriverContext->ulNumberOfVsnapThreads = ulValue;
            WriteULONG(hRegistry, NUMBER_OF_THREADS_FOR_VSNAP_THREAD_POOL, DEFAULT_NUMBER_OF_THREADS_FOR_VSNAP_THREAD_POOL);
        }

        ReadULONG(hRegistry, NUMBER_OF_THREADS_FOR_VVOLUME_THREAD_POOL, &ulValue, DEFAULT_NUMBER_OF_THREADS_FOR_VVOLUME_THREAD_POOL, TRUE);
        
        if((ulValue >= 1) && (ulValue <= 32)) {
            pDriverContext->ulNumberOfVVolumeThreads = ulValue;
            WriteULONG(hRegistry, NUMBER_OF_THREADS_FOR_VVOLUME_THREAD_POOL, DEFAULT_NUMBER_OF_THREADS_FOR_VVOLUME_THREAD_POOL);
        }

        ReadULONG(hRegistry, MEMORY_FOR_FILE_HANDLE_CACHE_KB, &ulValue, DEFAULT_PAGED_MEM_FILE_HANDLE_CACHE_KB, TRUE);
        if((ulValue >= MIN_PAGED_MEM_FOR_FILE_HANDLE_CACHE_KB) && (ulValue <= MAX_PAGED_MEM_FOR_FILE_HANDLE_CACHE_KB)) {
            pDriverContext->ulMemoryForFileHandleCache = ulValue * KILO_BYTE - MEMORY_FOR_TAG;
        } else {
            WriteULONG(hRegistry, MEMORY_FOR_FILE_HANDLE_CACHE_KB, DEFAULT_PAGED_MEM_FILE_HANDLE_CACHE_KB);
        }        

        CloseKey(hRegistry);
    
    }



    // Create temporary inmage device directory.
    RtlInitUnicodeString(&usInmageDeviceDirectory, VV_INMAGE_DEVICE_DIRECTORY);
    InitializeObjectAttributes(&ObjectAttributes, &usInmageDeviceDirectory, OBJ_PERMANENT, NULL, NULL);
    Status = ZwCreateDirectoryObject(&pDriverContext->hInMageDirectory, DIRECTORY_ALL_ACCESS, &ObjectAttributes);
    if (!NT_SUCCESS(Status)) {
        return STATUS_NO_MEMORY;
    }
    ZwMakeTemporaryObject(pDriverContext->hInMageDirectory);

    // Create Control Device to send IOCTLS
    RtlInitUnicodeString(&usControlDeviceName, VV_CONTROL_DEVICE_NAME);
    Status = IoCreateDevice(DriverObject, sizeof(DEVICE_EXTENSION), &usControlDeviceName, FILE_DEVICE_UNKNOWN, 0, FALSE, 
                            &ControlDevice);
    if ((!NT_SUCCESS(Status)) || (NULL == ControlDevice))
    {
        return STATUS_NO_MEMORY;
    }

    DevExtension = (PDEVICE_EXTENSION)ControlDevice->DeviceExtension;
    RtlZeroMemory(DevExtension, sizeof(DEVICE_EXTENSION));
    DevExtension->DeviceObject = ControlDevice;
    DevExtension->ulDeviceType = DEVICE_TYPE_CONTROL;
    IoInitializeRemoveLock(&DevExtension->IoRemoveLock, VVTAG_CTRLDEV_REMOVE_LOCK, 0, 0);

    pDriverContext->ControlDevice = ControlDevice;
    InsertHeadList(&pDriverContext->DeviceContextHead, &DevExtension->ListEntry);

    DevExtension->CmdQueue = AllocateCmdQueue(DevExtension, CONTROL_DEVICE_CMD_QUEUE_DESCRIPTION, VolumeExportCmdRoutine);
    if (NULL == DevExtension->CmdQueue) {
        return STATUS_UNSUCCESSFUL;
    }

    // Create symbolic link for user code to send IOCTLS.
    RtlInitUnicodeString(&usControlDeviceSymLinkName, VV_CONTROL_DEVICE_SYMLINK_NAME);
    Status = IoCreateSymbolicLink(&usControlDeviceSymLinkName, &usControlDeviceName);
    if (!NT_SUCCESS(Status))
    {
        return STATUS_NO_MEMORY;
    }

    DevExtension->ulFlags |= DEVICE_FLAGS_SYMLINK_CREATED;
    DevExtension->UniqueVolumeID = NULL;
    DevExtension->UniqueVolumeIDLength = 0;

    pDriverContext->DriverObject = DriverObject;
    pDriverContext->VVolumeImageCmdRoutines[VVOLUME_CMD_INVALID] = NULL;
    pDriverContext->VVolumeImageCmdRoutines[VVOLUME_CMD_READ] = VVolumeImageProcessReadRequest;
    pDriverContext->VVolumeImageCmdRoutines[VVOLUME_CMD_WRITE] = VVolumeImageProcessWriteRequest;

    KeInitializeEvent(&pDriverContext->hTestThreadShutdown, NotificationEvent, FALSE);
    KeInitializeMutex(&pDriverContext->PendingQueueMutex, 0);
#if (NTDDI_VERSION >= NTDDI_WS03)
    KeInitializeGuardedMutex(&pDriverContext->ImpersonatioContext.GuardedMutex);
#endif
    pDriverContext->VVolumeRetentionCmdRoutines[VVOLUME_CMD_INVALID] = NULL;
    pDriverContext->VVolumeRetentionCmdRoutines[VVOLUME_CMD_READ] = VVolumeRetentionProcessReadRequest;
    pDriverContext->VVolumeRetentionCmdRoutines[VVOLUME_CMD_WRITE] = VVolumeRetentionProcessWriteRequest;
  
    if(!VsnapInit()){
        return STATUS_UNSUCCESSFUL;
    }

    if(InitThreadPool() < 0) {
        return STATUS_UNSUCCESSFUL;        
    }

    return Status;
}

VOID
AddDeviceToDeviceList(PDRIVER_CONTEXT pDriverContext, PDEVICE_OBJECT DeviceObject)
{
    KIRQL               OldIrql;
    PDEVICE_EXTENSION   DeviceExtension;

    DeviceExtension = (PDEVICE_EXTENSION)DeviceObject->DeviceExtension;
    ASSERT(DeviceExtension->DeviceObject == DeviceObject);
    
    KeWaitForSingleObject(&DriverContext.hMutex, Executive, KernelMode, FALSE, NULL);
    InsertTailList(&pDriverContext->DeviceContextHead, &DeviceExtension->ListEntry);
    KeReleaseMutex(&DriverContext.hMutex, FALSE);
    return;
}

NTSTATUS
CleanupDriverContext(PDRIVER_CONTEXT pDriverContext)
{
    NTSTATUS            Status = STATUS_SUCCESS;
    UNICODE_STRING      usControlDeviceSymLinkName;
    PDEVICE_EXTENSION   DevExtension;
    KIRQL               OldIrql;
    BOOLEAN             bListIsEmpty;
    PCOMMAND_QUEUE      CmdQueue;
    PKWAIT_BLOCK         WaitBlocks = NULL;

    PAGED_CODE();
    // Set Shutdown event to stop the threads.
    KeSetEvent(&pDriverContext->ShutdownEvent, IO_NO_INCREMENT, FALSE);
    

    if(VsnapIOThreadPool) {
        
        DestroyThread();

        WaitBlocks = (PKWAIT_BLOCK )ALLOC_MEMORY(sizeof(KWAIT_BLOCK) * DriverContext.ulNumberOfVsnapThreads, NonPagedPool);
        if(!WaitBlocks) {
        goto out;
        }

        KeWaitForMultipleObjects(DriverContext.ulNumberOfVsnapThreads, (PVOID *)pDriverContext->pVsnapPoolThreads, WaitAll, Executive , KernelMode , FALSE, NULL, WaitBlocks);
        for(ULONG count = 0; count < DriverContext.ulNumberOfVsnapThreads; count++) {
            ObDereferenceObject(pDriverContext->pVsnapPoolThreads[count]);
            pDriverContext->pVsnapPoolThreads[count] = NULL;
        }

        FREE_MEMORY(WaitBlocks);
        WaitBlocks = NULL;
        if(DriverContext.pVsnapPoolThreads)
            FREE_MEMORY(DriverContext.pVsnapPoolThreads);

        WaitBlocks = (PKWAIT_BLOCK )ALLOC_MEMORY(sizeof(KWAIT_BLOCK) * DriverContext.ulNumberOfVVolumeThreads, NonPagedPool);
        if(!WaitBlocks) {
            goto out;
        }
    
        KeWaitForMultipleObjects(DriverContext.ulNumberOfVVolumeThreads, (PVOID *)pDriverContext->pVVolumePoolThreads, WaitAll, Executive , KernelMode , FALSE, NULL, WaitBlocks);
        for(ULONG count = 0; count < DriverContext.ulNumberOfVVolumeThreads; count++) {
            ObDereferenceObject(pDriverContext->pVVolumePoolThreads[count]);
            pDriverContext->pVVolumePoolThreads[count] = NULL;
        }
        

        FREE_MEMORY(WaitBlocks);
        if(DriverContext.pVVolumePoolThreads)
            FREE_MEMORY(DriverContext.pVVolumePoolThreads);

        UnInitThreadPool();
   }


out:
    // Wait to stop all threads.
    bListIsEmpty = FALSE;
    while (FALSE == bListIsEmpty) {
        KeAcquireSpinLock(&pDriverContext->Lock, &OldIrql);
        if (IsListEmpty(&pDriverContext->CmdQueueList)) {
            bListIsEmpty = TRUE;
            CmdQueue = NULL;
        } else {
            CmdQueue = (PCOMMAND_QUEUE)RemoveHeadList(&pDriverContext->CmdQueueList);
            CmdQueue->ulFlags |= CMDQUEUE_FLAGS_THREAD_IS_SHUTDOWN;
        }
        KeReleaseSpinLock(&pDriverContext->Lock, OldIrql);

        if (CmdQueue) {
            // Wait for thread to terminate.
            KeWaitForSingleObject(CmdQueue->Thread, Executive, KernelMode, FALSE, NULL);
            ObDereferenceObject(CmdQueue->Thread);
            CmdQueue->Thread = NULL;
            DereferenceCmdQueue(CmdQueue);
            CmdQueue = NULL;
        }
    }

    // Wait to remove all devices.
    bListIsEmpty = FALSE;
    while (FALSE == bListIsEmpty) {
        KeWaitForSingleObject(&DriverContext.hMutex, Executive, KernelMode, FALSE, NULL);
        if (IsListEmpty(&pDriverContext->DeviceContextHead)) {
            bListIsEmpty = TRUE;
            DevExtension = NULL;
        } else {
            DevExtension = (PDEVICE_EXTENSION)RemoveHeadList(&pDriverContext->DeviceContextHead);
        }
        KeReleaseMutex(&DriverContext.hMutex, FALSE);

        if (NULL != DevExtension) {
            Status = IoAcquireRemoveLock(&DevExtension->IoRemoveLock, (PVOID)VVDriverUnload);
            ASSERT(NT_SUCCESS(Status));
            IoReleaseRemoveLockAndWait(&DevExtension->IoRemoveLock, (PVOID)VVDriverUnload);

            ASSERT(NULL != DevExtension->CmdQueue);

            PCOMMAND_QUEUE  pCmdQueue = DevExtension->CmdQueue;
            ASSERT(pCmdQueue->lRefCount >= 0x01);

            if (0 == InterlockedDecrement(&pCmdQueue->lRefCount)) {
                FREE_MEMORY(pCmdQueue);
                DevExtension->CmdQueue = NULL;
            }

            switch(DevExtension->ulDeviceType) {
                case DEVICE_TYPE_CONTROL:
                    if (DevExtension->ulFlags & DEVICE_FLAGS_SYMLINK_CREATED) {
                        RtlInitUnicodeString(&usControlDeviceSymLinkName, VV_CONTROL_DEVICE_SYMLINK_NAME);
                        DevExtension->ulFlags &= ~DEVICE_FLAGS_SYMLINK_CREATED;
                        IoDeleteSymbolicLink(&usControlDeviceSymLinkName);
                    }
                    IoDeleteDevice(pDriverContext->ControlDevice);
                    pDriverContext->ControlDevice = NULL;
                    break;
                case DEVICE_TYPE_VVOLUME_FROM_VOLUME_IMAGE:
                    if(DevExtension->ulFlags & VV_DE_MOUNT_SUCCESS)
                        VVDeleteVolumeMountPoint(DevExtension);

                    if (DevExtension->hFile) {
                        ZwClose(DevExtension->hFile);
                        DevExtension->hFile = NULL;
                    }
                    if (DevExtension->usFileName.Buffer) {
                        FREE_MEMORY(DevExtension->usFileName.Buffer);
                        RtlInitUnicodeString(&DevExtension->usFileName, NULL);
                    }
                    IoDeleteDevice(DevExtension->DeviceObject);
                    DevExtension->DeviceObject = NULL;
                    break;
                case DEVICE_TYPE_VVOLUME_FROM_RETENTION_LOG:
                    if (DevExtension->hFile) {
                        ZwClose(DevExtension->hFile);
                        DevExtension->hFile = NULL;
                    }
                    if(DevExtension->ulFlags & VV_DE_MOUNT_SUCCESS)
                        VVDeleteVolumeMountPoint(DevExtension);

                    if(DevExtension->MountDevUniqueId.Buffer)
                    {
                        FREE_MEMORY(DevExtension->MountDevUniqueId.Buffer);
                        RtlInitUnicodeString(&DevExtension->MountDevUniqueId, NULL);
                    } 

                    if(DevExtension->usDeviceName.Buffer)
                    {
                        FREE_MEMORY(DevExtension->usDeviceName.Buffer);
                        RtlInitUnicodeString(&DevExtension->usDeviceName, NULL);
                    }

                    if(DevExtension->UniqueVolumeID)
                    {
                        FREE_MEMORY(DevExtension->UniqueVolumeID);
                        DevExtension->UniqueVolumeID = NULL;
                    }

                    IoDeleteDevice(DevExtension->DeviceObject);
                    DevExtension->DeviceObject = NULL;
                    break;
                default:
                    ASSERT(0);
                    break;
            }
        }
    }
    
    VsnapExit();

    if(pDriverContext->OutOfOrderTestThread != NULL)
    {
        KeWaitForSingleObject(&pDriverContext->OutOfOrderTestThread, Executive, KernelMode, FALSE, NULL);
        
        KeWaitForSingleObject(&pDriverContext->hMutex, Executive, KernelMode, FALSE, NULL);
        
        ObDereferenceObject(DriverContext.OutOfOrderTestThread);
        pDriverContext->OutOfOrderTestThread = NULL;

        KeReleaseMutex(&pDriverContext->hMutex, Executive);
    }

    // Have to wait for all commands to be freed.
    if (pDriverContext->ulFlags & VV_DC_FLAGS_COMMAND_POOL_INITIALIZED) {
        ExDeleteNPagedLookasideList(&DriverContext.CommandPool);
        ExDeleteNPagedLookasideList(&DriverContext.IOCommandPool);
        pDriverContext->ulFlags&= ~VV_DC_FLAGS_COMMAND_POOL_INITIALIZED;
    }

    if (NULL != pDriverContext->usDriverName.Buffer) {
        FREE_MEMORY(pDriverContext->usDriverName.Buffer);
        pDriverContext->usDriverName.Buffer = NULL;
        pDriverContext->usDriverName.MaximumLength = 0;
        pDriverContext->usDriverName.Length = 0;
    }

    if (NULL != pDriverContext->DriverParameters.Buffer) {
        FREE_MEMORY(pDriverContext->DriverParameters.Buffer);
        pDriverContext->DriverParameters.Buffer = NULL;
        pDriverContext->DriverParameters.MaximumLength = 0;
        pDriverContext->DriverParameters.Length = 0;
    }
#if (NTDDI_VERSION >= NTDDI_WS03)
    if (pDriverContext->ImpersonatioContext.bSecurityContext) {
        pDriverContext->ImpersonatioContext.bSecurityContext = 0;
        SeDeleteClientSecurity(pDriverContext->ImpersonatioContext.pSecurityContext);
        pDriverContext->ImpersonatioContext.pSecurityContext = NULL;
    }
#endif
    pDriverContext->DriverObject = NULL;
    if (pDriverContext->hInMageDirectory) {
        ZwClose(pDriverContext->hInMageDirectory);
        pDriverContext->hInMageDirectory = NULL;
    }

    return Status;
}
