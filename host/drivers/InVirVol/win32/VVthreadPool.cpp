//#include "VVDevControl.h"

#include "VVthreadPool.h"
#include "VVDriverContext.h"
#include "VsnapKernel.h"
#include <ntdddisk.h>
//#include "VVDevControl.h"
//#include "VVCmdQueue.h"

extern DRIVER_CONTEXT  DriverContext;
extern LIST_ENTRY	*ParentVolumeListHead;
extern VsnapRWLock *ParentVolumeListLock;


#define FILE_DEVICE_FILE_SYSTEM         0x00000009
#define FSCTL_SET_ZERO_DATA             CTL_CODE(FILE_DEVICE_FILE_SYSTEM, 50, METHOD_BUFFERED, FILE_WRITE_DATA) // FILE_ZERO_DATA_INFORMATION,

THREAD_POOL		*VsnapIOThreadPool = NULL;
THREAD_POOL_INFO	*VsnapIOThreadPoolInfo = NULL;

THREAD_POOL		*VsnapUpdatePool = NULL;
THREAD_POOL_INFO	*VsnapUpdatePoolInfo = NULL;

THREAD_POOL		*VVolumeIOThreadPool = NULL;
THREAD_POOL_INFO	*VVolumeIOThreadPoolInfo = NULL;



int 
InitThreadPool(void) 
{
	ULONG 	            i = 0,retval = 0,fail_threads = 0;
    HANDLE              hThread;
    NTSTATUS            Status;
    
        //Initialize update thread  pool
    DriverContext.pVsnapPoolThreads = (PKTHREAD *)ALLOC_MEMORY_WITH_TAG(sizeof(PKTHREAD)*DriverContext.ulNumberOfVsnapThreads, NonPagedPool, VVTAG_THREAD_POOL);
    if ( !DriverContext.pVsnapPoolThreads ) {
		retval = -1;
		goto out;
	}

    DriverContext.pVVolumePoolThreads = (PKTHREAD *)ALLOC_MEMORY_WITH_TAG(sizeof(PKTHREAD)*DriverContext.ulNumberOfVVolumeThreads, NonPagedPool, VVTAG_THREAD_POOL);
    if ( !DriverContext.pVVolumePoolThreads ) {
		retval = -1;
		goto out;
	}

	VsnapUpdatePool = (THREAD_POOL *)ALLOC_MEMORY_WITH_TAG(sizeof(THREAD_POOL), NonPagedPool, VVTAG_THREAD_POOL);
	if ( !VsnapUpdatePool ) {
		retval = -1;
		goto out;
	}

    VsnapUpdatePoolInfo = (THREAD_POOL_INFO *)ALLOC_MEMORY_WITH_TAG(sizeof(THREAD_POOL_INFO), NonPagedPool, VVTAG_THREAD_POOL);
	if ( !VsnapUpdatePoolInfo ) {
		
		retval = -1;
		goto out;
	}
    
    RtlZeroMemory(VsnapUpdatePool, sizeof(THREAD_POOL));
    RtlZeroMemory(VsnapUpdatePoolInfo, sizeof(THREAD_POOL_INFO));

    VsnapUpdatePoolInfo->rwLock = (VsnapRWLock *)ALLOC_MEMORY_WITH_TAG(sizeof(VsnapRWLock), NonPagedPool, VVTAG_THREAD_POOL);

    if(!VsnapUpdatePoolInfo->rwLock) {
        retval = -1;
        goto out;
    }

	
    VsnapUpdatePoolInfo->pThreadPool = VsnapUpdatePool;
    InitializeVsnapRWLock(VsnapUpdatePoolInfo->rwLock);
	

	KeInitializeEvent(&VsnapUpdatePool->TaskNotifyEvent, SynchronizationEvent, FALSE);

    ExInitializeFastMutex(&VsnapUpdatePool->hMutex);
    InitializeListHead(&VsnapUpdatePool->GlobalListHead);


    // Initialize thread pool for Vsnap IOs
	VsnapIOThreadPool = (THREAD_POOL *)ALLOC_MEMORY_WITH_TAG(sizeof(THREAD_POOL), NonPagedPool, VVTAG_THREAD_POOL);
	if ( !VsnapIOThreadPool ) {
        retval = -1;
		goto out;
	}
	
	VsnapIOThreadPoolInfo = (THREAD_POOL_INFO *)ALLOC_MEMORY_WITH_TAG(sizeof(THREAD_POOL_INFO), NonPagedPool, VVTAG_THREAD_POOL);
	if ( !VsnapIOThreadPoolInfo ) {
		retval = -1;
		goto out;
	}

    RtlZeroMemory(VsnapIOThreadPool, sizeof(THREAD_POOL));
    RtlZeroMemory(VsnapIOThreadPoolInfo, sizeof(THREAD_POOL_INFO));



    VsnapIOThreadPoolInfo->rwLock = (VsnapRWLock *)ALLOC_MEMORY_WITH_TAG(sizeof(VsnapRWLock), NonPagedPool, VVTAG_THREAD_POOL);

    if(!VsnapIOThreadPoolInfo->rwLock) {
        retval = -1;
        goto out;
    }

	
    VsnapIOThreadPoolInfo->pThreadPool = VsnapIOThreadPool;
    InitializeVsnapRWLock(VsnapIOThreadPoolInfo->rwLock);
	
    //INM_ATOMIC_SET(&vdev_ops_pool_info->tpi_num_threads, 0);
	//INM_INIT_LOCK(&vdev_ops_pool_info->tpi_rwlock);

	KeInitializeEvent(&VsnapIOThreadPool->TaskNotifyEvent, SynchronizationEvent, FALSE);
    //init_waitqueue_head(&vdev_ops_pool->tp_wait_queue);

    ExInitializeFastMutex(&VsnapIOThreadPool->hMutex);
	//INM_INIT_SPINLOCK(&vdev_ops_pool->tp_lock);
    InitializeListHead(&VsnapIOThreadPool->GlobalListHead);
	//INIT_LIST_HEAD(&vdev_ops_pool->tp_task_list_head);
	//vdev_ops_pool->tp_num_tasks = 0;


    // Initialize thread pool for VVolumes IOs
	VVolumeIOThreadPool = (THREAD_POOL *)ALLOC_MEMORY_WITH_TAG(sizeof(THREAD_POOL), NonPagedPool, VVTAG_THREAD_POOL);
	if ( !VVolumeIOThreadPool ) {
        retval = -1;
		goto out;
	}
	
	VVolumeIOThreadPoolInfo = (THREAD_POOL_INFO *)ALLOC_MEMORY_WITH_TAG(sizeof(THREAD_POOL_INFO), NonPagedPool, VVTAG_THREAD_POOL);
	if ( !VVolumeIOThreadPoolInfo ) {
		retval = -1;
		goto out;
	}

    RtlZeroMemory(VVolumeIOThreadPool, sizeof(THREAD_POOL));
    RtlZeroMemory(VVolumeIOThreadPoolInfo, sizeof(THREAD_POOL_INFO));



    VVolumeIOThreadPoolInfo->rwLock = (VsnapRWLock *)ALLOC_MEMORY_WITH_TAG(sizeof(VsnapRWLock), NonPagedPool, VVTAG_THREAD_POOL);

    if(!VVolumeIOThreadPoolInfo->rwLock) {
        retval = -1;
        goto out;
    }

	
    VVolumeIOThreadPoolInfo->pThreadPool = VVolumeIOThreadPool;
    InitializeVsnapRWLock(VVolumeIOThreadPoolInfo->rwLock);
	
    //INM_ATOMIC_SET(&vdev_ops_pool_info->tpi_num_threads, 0);
	//INM_INIT_LOCK(&vdev_ops_pool_info->tpi_rwlock);

	KeInitializeEvent(&VVolumeIOThreadPool->TaskNotifyEvent, SynchronizationEvent, FALSE);
    //init_waitqueue_head(&vdev_ops_pool->tp_wait_queue);

    ExInitializeFastMutex(&VVolumeIOThreadPool->hMutex);
	//INM_INIT_SPINLOCK(&vdev_ops_pool->tp_lock);
    InitializeListHead(&VVolumeIOThreadPool->GlobalListHead);
	//INIT_LIST_HEAD(&vdev_ops_pool->tp_task_list_head);
	//vdev_ops_pool->tp_num_tasks = 0;

	for ( i = 0; i < DriverContext.ulNumberOfVsnapThreads; i++ ) {
	        Status = PsCreateSystemThread(&hThread, THREAD_ALL_ACCESS, NULL, NULL, NULL, VsnapIOWorkerThread, VsnapIOThreadPoolInfo);
            if (NT_SUCCESS(Status))
            {
                InterlockedIncrement(&VsnapIOThreadPoolInfo->ulCountThreads);
                Status = ObReferenceObjectByHandle(hThread, SYNCHRONIZE, NULL, KernelMode, (PVOID *)&(DriverContext.pVsnapPoolThreads[i]), NULL);
                if (NT_SUCCESS(Status))
                {
                    ZwClose(hThread);
                }
            } 
    }

	
	for ( i = 0; i < DriverContext.ulNumberOfVVolumeThreads; i++ ) {
	        Status = PsCreateSystemThread(&hThread, THREAD_ALL_ACCESS, NULL, NULL, NULL, VVolumeIOWorkerThread, VVolumeIOThreadPoolInfo);
            if (NT_SUCCESS(Status))
            {
                InterlockedIncrement(&VVolumeIOThreadPoolInfo->ulCountThreads);
                Status = ObReferenceObjectByHandle(hThread, SYNCHRONIZE, NULL, KernelMode, (PVOID *)&(DriverContext.pVVolumePoolThreads[i]), NULL);
                if (NT_SUCCESS(Status))
                {
                    ZwClose(hThread);
                }
            }        
	}

	
    return retval;

out:
    FREE_MEMORY(DriverContext.pVsnapPoolThreads);
    FREE_MEMORY(DriverContext.pVVolumePoolThreads);

    if(VsnapUpdatePoolInfo)
        FREE_MEMORY(VsnapUpdatePoolInfo->rwLock);
    FREE_MEMORY(VsnapUpdatePool);
    FREE_MEMORY(VsnapUpdatePoolInfo);

    if(VVolumeIOThreadPoolInfo)
        FREE_MEMORY(VVolumeIOThreadPoolInfo->rwLock);
    FREE_MEMORY(VVolumeIOThreadPool);
    FREE_MEMORY(VVolumeIOThreadPoolInfo);
    
    if(VsnapIOThreadPoolInfo)
        FREE_MEMORY(VsnapIOThreadPoolInfo->rwLock);
    FREE_MEMORY(VsnapIOThreadPool);
    FREE_MEMORY(VsnapIOThreadPoolInfo);

    VsnapUpdatePool = NULL;
    VsnapUpdatePoolInfo = NULL;
    VVolumeIOThreadPool = NULL;
    VVolumeIOThreadPoolInfo = NULL;
    VsnapIOThreadPool = NULL;
    VsnapIOThreadPoolInfo = NULL;
	return retval;

}


void
KillThread(THREAD_POOL_INFO *tPoolInfo)
{
	THREAD_POOL	*tPool = tPoolInfo->pThreadPool;
	VSNAP_TASK	*task = NULL;
	unsigned long 	flags;

	// We assume all synchronisation is done in caller 
	if (InterlockedCompareExchange(&tPoolInfo->ulCountThreads, 0, 0) ) {
		
		task = AllocateTask();
		if ( !task ) {
		
			return;
		}

		task->usTaskId = TASK_KILL_THREAD;
		ADD_TASK_TO_POOL(tPool, task);
	} else {
		
	}
}


bool
UnInitThreadPool(void) 
{



    AcquireVsnapWriteLock(VsnapIOThreadPoolInfo->rwLock);
    ReleaseVsnapWriteLock(VsnapIOThreadPoolInfo->rwLock);

    FREE_MEMORY(VsnapIOThreadPoolInfo->rwLock);
    FREE_MEMORY(VsnapIOThreadPoolInfo);
    FREE_MEMORY(VsnapIOThreadPool);
    


	// Wait for all thread to die 
    //doubt: whats the purpose of this lock
	//INM_WRITE_LOCK(&vdev_ops_pool_info->tpi_rwlock);
	//INM_WRITE_UNLOCK(&vdev_ops_pool_info->tpi_rwlock);

    AcquireVsnapWriteLock(VVolumeIOThreadPoolInfo->rwLock);
    ReleaseVsnapWriteLock(VVolumeIOThreadPoolInfo->rwLock);

	//DEV_DBG("Cleanup Resources");
	//INM_FREE(&vdev_ops_pool_info, sizeof(thread_pool_info_t));
	//INM_FREE(&vdev_ops_pool, sizeof(thread_pool_t));
    FREE_MEMORY(VVolumeIOThreadPoolInfo->rwLock);
    FREE_MEMORY(VVolumeIOThreadPoolInfo);
    FREE_MEMORY(VVolumeIOThreadPool);

    AcquireVsnapWriteLock(VsnapUpdatePoolInfo->rwLock);
    ReleaseVsnapWriteLock(VsnapUpdatePoolInfo->rwLock);

    FREE_MEMORY(VsnapUpdatePoolInfo->rwLock);
    FREE_MEMORY(VsnapUpdatePoolInfo);
    FREE_MEMORY(VsnapUpdatePool);

    VsnapUpdatePool = NULL;
    VsnapUpdatePoolInfo = NULL;
    VVolumeIOThreadPool = NULL;
    VVolumeIOThreadPoolInfo = NULL;
    VsnapIOThreadPool = NULL;
    VsnapIOThreadPoolInfo = NULL;
	

    return true;


}

void DestroyThread(void)
{
    int     num_threads;
    int     i = 0;

    
	num_threads = InterlockedCompareExchange(&VsnapIOThreadPoolInfo->ulCountThreads, 0, 0);
	
	for ( i = 0; i < num_threads; i++ ) {
	    KillThread(VsnapIOThreadPoolInfo);
	}

    
    num_threads = InterlockedCompareExchange(&VVolumeIOThreadPoolInfo->ulCountThreads, 0, 0);
	
	for ( i = 0; i < num_threads; i++ ) {
		KillThread(VVolumeIOThreadPoolInfo);
	}
}

void
VsnapIOWorkerThread(PVOID Buffer) 
{
	int          retval;
	VSNAP_TASK   *task;
	unsigned long    flags;
    THREAD_POOL_INFO *tPoolinfo = (THREAD_POOL_INFO *)Buffer;
    NTSTATUS        Status;
#if (NTDDI_VERSION >= NTDDI_WS03)
    NTSTATUS        StatusImpersonation; 
#endif
    bool            bWaitThread = true;
#if (NTDDI_VERSION >= NTDDI_WS03)
    bool            bImpersonation = 0;
#endif

	//DEV_DBG("Started new thread");
	//INM_READ_LOCK(&tp_info->tpi_rwlock);
	//INM_ATOMIC_INC(&tp_info->tpi_num_threads);

    AcquireVsnapReadLock(tPoolinfo->rwLock);
    
	
	for (;;) {
		
        if(bWaitThread) {
		    Status = WAIT_FOR_NEXT_TASK(tPoolinfo->pThreadPool);
		    if (!NT_SUCCESS(Status)) {
                break;
            }
        }

	 	// Its here means there is a task that needs processing
	 	
		GET_TASK_FROM_POOL(tPoolinfo->pThreadPool, task);
		
        if (!task ) {
            bWaitThread = true;
			continue;
        }
#if (NTDDI_VERSION >= NTDDI_WS03)
        if (!bImpersonation) {
            StatusImpersonation = VVImpersonate(&DriverContext.ImpersonatioContext, NULL);
            if (NT_SUCCESS(StatusImpersonation)) {
                bImpersonation = 1;
            }
        }
#endif
		switch( task->usTaskId ) {
			case TASK_BTREE_LOOKUP:
			
					VsnapVolumeBtreeLookup(task);
					break;

			case TASK_WRITE:
			
					VSnapVolumeWriteData(task);
					break;
			
			case TASK_RETENTION_IO:
			
					VsnapVolumeReadRetentionFile(task);
					break;
			
			case TASK_START_QUEUED_IO:
			        VsnapStartQueuedIo(task);
					break;

			case TASK_KILL_THREAD:
			
			
                    DeAllocateTask(task);
            
                    InterlockedDecrement(&tPoolinfo->ulCountThreads);
			
                    ReleaseVsnapReadLock(tPoolinfo->rwLock);
                    KeSetEvent(&tPoolinfo->pThreadPool->TaskNotifyEvent, EVENT_INCREMENT, FALSE);
					goto out;

			default:
				//DBG("Unknown task");
				//ALERT();
                break;
		}

        bWaitThread = false;
	}				

out:	
#if (NTDDI_VERSION >= NTDDI_WS03)
    if (bImpersonation) {
        SeStopImpersonatingClient();
    }
#endif
	return;

}

void VSnapVolumeWriteData(VSNAP_TASK  *task)
{
    PIO_COMMAND_STRUCT     pCommand = NULL;
    pCommand = (PIO_COMMAND_STRUCT)(task->TaskData);
    VsnapInfo *Vsnap = (VsnapInfo *)pCommand->Vsnap;
    NTSTATUS        Status = STATUS_SUCCESS;
    PVOID           pWriteBuffer;
    IO_STATUS_BLOCK IoStatus;
    PIRP            Irp = pCommand->Cmd.Write.Irp;
    KIRQL           OldIrql;
	ULONG			BytesWritten = 0;
	int				iSuccess = true;
    
    InVolDbgPrint(DL_VV_VERBOSE, DM_VV_WRITE, 
                  ("VSnapVolumeWriteData: Entered\n"));

    Irp->IoStatus.Information = 0;
    Irp->IoStatus.Status = Status;

    AcquireVsnapReadLock(pCommand->Vsnap->ParentPtr->TargetRWLock);

    if(pCommand->Cmd.Write.ulLength != 0)
    {
        pWriteBuffer = MmGetSystemAddressForMdlSafe(Irp->MdlAddress, NormalPagePriority);
        if (NULL == pWriteBuffer) {
            pCommand->CmdStatus = STATUS_INSUFFICIENT_RESOURCES;
            goto out;
        }
		
        if(pCommand->Vsnap->AccessType == VSNAP_READ_ONLY)
            goto out;

        iSuccess =  VsnapVolumeWrite(pWriteBuffer, (ULONGLONG)pCommand->Cmd.Write.ByteOffset.QuadPart, 
			pCommand->Cmd.Write.ulLength, &BytesWritten, pCommand->Vsnap->DevExtension->VolumeContext);

        if(!iSuccess) {
            Status = STATUS_UNSUCCESSFUL;
        }

        pCommand->CmdStatus = Status;
        Irp->IoStatus.Information = BytesWritten;
        
    }
out:
    ReleaseVsnapReadLock(pCommand->Vsnap->ParentPtr->TargetRWLock);
    VsnapVolumeEndIO(task);
        InVolDbgPrint(DL_VV_VERBOSE, DM_VV_WRITE, 
                  ("VSnapVolumeWriteData: Exited\n"));

    return;
}


void VsnapVolumeEndIO(VSNAP_TASK  *task)
{
    int                  iIoType = task->usTaskId;
    VSNAP_READ_RETENTION *ReadRetention;
    PIO_COMMAND_STRUCT   pIOCommand = NULL;

    VsnapInfo            *Vsnap = NULL;
    PDEVICE_EXTENSION    pDevExtension = NULL;
    KIRQL                OldIrql;
    int                  PendingIO, InflightIO;
  
    
    if(iIoType == TASK_RETENTION_IO) {
        ReadRetention = (VSNAP_READ_RETENTION *)task->TaskData;
        pIOCommand = (PIO_COMMAND_STRUCT)ReadRetention->pBuffer;
    } else {
        pIOCommand = (PIO_COMMAND_STRUCT)task->TaskData;
    }

    Vsnap = pIOCommand->Vsnap;
    ExAcquireFastMutex(&Vsnap->hMutex);
    InflightIO = --Vsnap->IOsInFlight;
    PendingIO = Vsnap->IOPending;
    if(iIoType == TASK_WRITE) {
        Vsnap->IOWriteInProgress = false;        
    }
    
    ExReleaseFastMutex(&Vsnap->hMutex);

    DereferenceIOCommand(pIOCommand);  

    if(iIoType == TASK_RETENTION_IO)
        FREE_MEMORY(task->TaskData);

    if( !InflightIO) {

        if(0 != PendingIO) {
            task->usTaskId = TASK_START_QUEUED_IO;
            task->TaskData = (PVOID)Vsnap;
            //ADD_TASK_TO_POOL(VsnapIOThreadPool, task);
            VsnapStartQueuedIo(task);
        } 
    }

	if ( InflightIO || !PendingIO)
		    DeAllocateTask(task);
}


void VsnapVolumeBtreeLookup(VSNAP_TASK  *task)
{
    
    PIO_COMMAND_STRUCT       pIOCommand  = (PIO_COMMAND_STRUCT)task->TaskData;
    PDEVICE_EXTENSION        DevExtension = pIOCommand->Vsnap->DevExtension;
    NTSTATUS                 Status         = STATUS_SUCCESS;
    int                      iSuccess       = true;
    bool                     AllocationFailed= false;
    PVOID                    pReadBuffer    = NULL,pBuffer = NULL;
    ULONG                    BytesRead; 
    int                      iBytesRead;
    PIRP                     Irp            = pIOCommand->Cmd.Read.Irp;
    IO_STATUS_BLOCK          IoStatus;
    VsnapOffsetSplitInfo     *OffsetInfo;

    InVolDbgPrint(DL_VV_VERBOSE, DM_VV_READ, 
                  ("VsnapVolumeRetentionIO: Entered - \n"));
    
    pReadBuffer = MmGetSystemAddressForMdlSafe(Irp->MdlAddress, NormalPagePriority);
    
    if (NULL == pReadBuffer) {
        pIOCommand->CmdStatus = STATUS_INSUFFICIENT_RESOURCES;
        goto out;        
    }

    pIOCommand->ulFlags |= FLAG_READ_INTO_TEMPORARY_BUFFER;

    pBuffer = ALLOC_MEMORY(pIOCommand->Cmd.Read.ulLength, PagedPool);
    if(!pBuffer) {
        AllocationFailed = true;
        pIOCommand->ulFlags &= ~FLAG_READ_INTO_TEMPORARY_BUFFER;
    }
    pIOCommand->pBuffer = pBuffer;
    pIOCommand->pOrigBuffer = pReadBuffer;
    AcquireVsnapReadLock(pIOCommand->Vsnap->ParentPtr->TargetRWLock);

    iSuccess = VsnapVolumeRead(AllocationFailed?(PCHAR)pReadBuffer:(PCHAR)pBuffer, (ULONGLONG)pIOCommand->Cmd.Read.ByteOffset.QuadPart, 
        pIOCommand->Cmd.Read.ulLength, &BytesRead, DevExtension->VolumeContext, 
        &pIOCommand->HeadForVsnapRetentionData, &pIOCommand->TargetReadRequired);
    
    if(!iSuccess) {
        ReleaseVsnapReadLock(pIOCommand->Vsnap->ParentPtr->TargetRWLock);
        ASSERT(IsSingleListEmpty(&pIOCommand->HeadForVsnapRetentionData));        
        goto out;
    } else {
        if (pIOCommand->TargetReadRequired ) {
            if(pIOCommand->Vsnap->ParentPtr->IsFile) {
                if(!READ_FILE(pIOCommand->Vsnap->ParentPtr->SparseFileHandle, AllocationFailed?(PCHAR)pReadBuffer:(PCHAR)pBuffer, 
                             (ULONGLONG)pIOCommand->Cmd.Read.ByteOffset.QuadPart, 
                             pIOCommand->Cmd.Read.ulLength, &iBytesRead))
                {
                    ReleaseVsnapReadLock(pIOCommand->Vsnap->ParentPtr->TargetRWLock);
                    iSuccess = 0;
                    while(!IsSingleListEmpty(&pIOCommand->HeadForVsnapRetentionData))
                    {
                        OffsetInfo = (VsnapOffsetSplitInfo *)InPopEntryList(&pIOCommand->HeadForVsnapRetentionData);
                        FREE_MEMORY(OffsetInfo);
                    }
                    goto out;
                }
                
            } else if(pIOCommand->Vsnap->ParentPtr->IsMultiPartSparseFile){
                NTSTATUS ErrStatus= STATUS_SUCCESS;
                if (! ReadFromMultiPartSparseFile(pIOCommand->Vsnap->ParentPtr->MultiPartSparseFileHandle, 
                                                  AllocationFailed?(PCHAR)pReadBuffer:(PCHAR)pBuffer, 
                                                  (ULONGLONG)pIOCommand->Cmd.Read.ByteOffset.QuadPart, 
                                                  pIOCommand->Cmd.Read.ulLength, 
                                                  &iBytesRead,
                                                  pIOCommand->Vsnap->ParentPtr->ChunkSize,
                                                  &ErrStatus)) 
                {
                    if (!NT_SUCCESS(ErrStatus)) {
                        PWCHAR MountDevLinkGUID = &pIOCommand->Vsnap->DevExtension->usMountDevLink.Buffer[DEV_LINK_GUID_OFFSET];
                        InVirVolLogError(DriverContext.ControlDevice, __LINE__, INVIRVOL_VOLPACK_READ_FAILURE, ErrStatus,
                                         MountDevLinkGUID,
                                         GUID_LEN_WITH_BRACE,
                                         (ULONGLONG)pIOCommand->Cmd.Read.ByteOffset.QuadPart,
                                         (ULONGLONG)pIOCommand->Cmd.Read.ulLength,
                                         pIOCommand->Vsnap->SnapShotId);
                    }
                    ReleaseVsnapReadLock(pIOCommand->Vsnap->ParentPtr->TargetRWLock);
                    iSuccess = 0;
                    while(!IsSingleListEmpty(&pIOCommand->HeadForVsnapRetentionData))
                    {
                        OffsetInfo = (VsnapOffsetSplitInfo *)InPopEntryList(&pIOCommand->HeadForVsnapRetentionData);
                        FREE_MEMORY(OffsetInfo);
                    }
                    goto out;
                }
            }
            else {
              if(!VsnapReadFromVolume(pIOCommand->Vsnap->ParentPtr->VolumeName, pIOCommand->Vsnap->ParentPtr->VolumeSectorSize, 
                                      AllocationFailed?(PCHAR)pReadBuffer:(PCHAR)pBuffer, 
                                      (ULONGLONG)pIOCommand->Cmd.Read.ByteOffset.QuadPart, 
                                      pIOCommand->Cmd.Read.ulLength, &BytesRead))
                {
                    ReleaseVsnapReadLock(pIOCommand->Vsnap->ParentPtr->TargetRWLock);
                    iSuccess = 0;
                    while(!IsSingleListEmpty(&pIOCommand->HeadForVsnapRetentionData))
                    {
                        OffsetInfo = (VsnapOffsetSplitInfo *)InPopEntryList(&pIOCommand->HeadForVsnapRetentionData);
                        FREE_MEMORY(OffsetInfo);
                    }
                    goto out;
                }
            }
                
        }

       ReleaseVsnapReadLock(pIOCommand->Vsnap->ParentPtr->TargetRWLock);
        
        if(!IsSingleListEmpty(&pIOCommand->HeadForVsnapRetentionData)) {
           iSuccess = VsnapVolumeCreateRetentionTask(task);
        }     
       
    }
out:
    if(!iSuccess)
    {
        pIOCommand->CmdStatus = STATUS_UNSUCCESSFUL;
    }
    ASSERT(IsSingleListEmpty(&pIOCommand->HeadForVsnapRetentionData));

    
    VsnapVolumeEndIO(task);
    
    InVolDbgPrint(DL_VV_VERBOSE, DM_VV_READ, 
                  ("VsnapVolumeRetentionIO: Exited - \n"));


}


void VsnapStartQueuedIo(VSNAP_TASK  *ptask)
{
    PIO_COMMAND_STRUCT          pIOCommand    = NULL;
    VsnapInfo                  *Vsnap = (VsnapInfo *)ptask->TaskData;;
    KIRQL                       OldIrql;
    int                         iIoType,count;
    PVSNAP_TASK                 task = NULL;
    bool                        bUndoRemove = false,bFreeTask = false;
    ULONG                       PendingIO = 0;   
    
    
    DeAllocateTask(ptask);
    for(count = 0;;count++)
    {
        task = AllocateTask();
        if(!task) { 
            
            return;
        }
       
        ExAcquireFastMutex(&Vsnap->hMutex);
        
        pIOCommand = RemoveIOCommandFromQueue(Vsnap->DevExtension->IOCmdQueue);
        if (NULL == pIOCommand) {
            ExReleaseFastMutex(&Vsnap->hMutex);
            DeAllocateTask(task);
            return;
        }

        if(pIOCommand->ulCommand  == VVOLUME_CMD_READ)
            task->usTaskId = TASK_BTREE_LOOKUP;
        else if(pIOCommand->ulCommand == VVOLUME_CMD_WRITE)
            task->usTaskId = TASK_WRITE;

        iIoType = task->usTaskId;
        task->TaskData = (PVOID)pIOCommand;

        if(!Vsnap->IOWriteInProgress) {
            if(iIoType == TASK_WRITE) 
            {
                if(count > 0) {
                    UndoRemoveIOCommandFromQueue(Vsnap->DevExtension->IOCmdQueue, pIOCommand);
                    bFreeTask = true;
                    
                } else if (Vsnap->IOsInFlight == 0) {
                    Vsnap->IOWriteInProgress = true;
                    Vsnap->IOPending--;
                    Vsnap->IOsInFlight++;
                    ADD_TASK_TO_POOL(VsnapIOThreadPool, task);

                } else {
                    UndoRemoveIOCommandFromQueue(Vsnap->DevExtension->IOCmdQueue, pIOCommand);
                    bFreeTask = true;
                    
                }
            } else {            
                Vsnap->IOPending--;
                Vsnap->IOsInFlight++;
                ADD_TASK_TO_POOL(VsnapIOThreadPool, task);
            }
        }
        else {
            UndoRemoveIOCommandFromQueue(Vsnap->DevExtension->IOCmdQueue, pIOCommand);
            bFreeTask = true;
            
            
        }

        ExReleaseFastMutex(&Vsnap->hMutex);

        if(bFreeTask) {
            DeAllocateTask(task);
            break;
        }
        
        switch(iIoType)
        {
            case TASK_WRITE:                
                return;
            case TASK_BTREE_LOOKUP:
                break;
        }

    }
}



int VsnapVolumeCreateRetentionTask(VSNAP_TASK  *task) 
{
    PIO_COMMAND_STRUCT pIOCommand = (PIO_COMMAND_STRUCT)task->TaskData;
    VsnapOffsetSplitInfo   *OffsetInfo;
    VSNAP_TASK             *pTask = NULL;
    VSNAP_READ_RETENTION   *ReadRetentionOffset = NULL;
    KIRQL                   OldIrql;

    while(!IsSingleListEmpty(&pIOCommand->HeadForVsnapRetentionData))
    {
        OffsetInfo = (VsnapOffsetSplitInfo *)InPopEntryList(&pIOCommand->HeadForVsnapRetentionData);
        ReadRetentionOffset = (VSNAP_READ_RETENTION *)ALLOC_MEMORY(sizeof(VSNAP_READ_RETENTION), PagedPool);
        if(!ReadRetentionOffset) {
            goto out;
        }
        pTask = AllocateTask();
        if(!pTask) {
            goto out;
        }
        ReadRetentionOffset->pBuffer = (PVOID)ReferenceIOCommand(pIOCommand);
        RtlCopyMemory(&ReadRetentionOffset->VsnapRetdata, &OffsetInfo->Offset, sizeof(VsnapKeyData));
        pTask->usTaskId = TASK_RETENTION_IO;
        pTask->TaskData = (PVOID)ReadRetentionOffset;
        

        ExAcquireFastMutex(&pIOCommand->Vsnap->hMutex);
        ADD_TASK_TO_POOL_HEAD(VsnapIOThreadPool, pTask);
        pIOCommand->Vsnap->IOsInFlight++;
        ExReleaseFastMutex(&pIOCommand->Vsnap->hMutex);

        if(OffsetInfo) {
            FREE_MEMORY(OffsetInfo);
        }
        OffsetInfo = NULL;
        ReadRetentionOffset = NULL;
        pTask = NULL;
    }
    
    return 1;

out:
    
    FREE_MEMORY(OffsetInfo);
    
    if(ReadRetentionOffset)
        FREE_MEMORY(ReadRetentionOffset);

    while(!IsSingleListEmpty(&pIOCommand->HeadForVsnapRetentionData))
    {
        OffsetInfo = (VsnapOffsetSplitInfo *)InPopEntryList(&pIOCommand->HeadForVsnapRetentionData);
        FREE_MEMORY(OffsetInfo);
    }
            
return 0;
}


void VsnapVolumeReadRetentionFile(VSNAP_TASK  *task)
{
    PVSNAP_READ_RETENTION          PVsnapRetIO = (PVSNAP_READ_RETENTION)task->TaskData;
    PIO_COMMAND_STRUCT pIOCommand = (PIO_COMMAND_STRUCT)PVsnapRetIO->pBuffer;
    PVOID              ActualBuffer = NULL;
    bool               ReturnValue = true;

    InVolDbgPrint(DL_VV_VERBOSE, DM_VV_READ, 
                  ("VsnapVolumeReadRetentionFile: Entered - \n"));

    if(pIOCommand->ulFlags & FLAG_READ_INTO_TEMPORARY_BUFFER)
        ActualBuffer = pIOCommand->pBuffer;
    else
        ActualBuffer = pIOCommand->pOrigBuffer;

    ReturnValue = VsnapOpenAndReadDataFromLog(ActualBuffer, PVsnapRetIO->VsnapRetdata.Offset, PVsnapRetIO->VsnapRetdata.FileOffset, 
        PVsnapRetIO->VsnapRetdata.Length, PVsnapRetIO->VsnapRetdata.FileId, pIOCommand->Vsnap);

    if(!ReturnValue)
        pIOCommand->CmdStatus = STATUS_UNSUCCESSFUL;

    VsnapVolumeEndIO(task);
    
    InVolDbgPrint(DL_VV_VERBOSE, DM_VV_READ, 
                  ("VsnapVolumeReadRetentionFile: Exited - \n"));

    return;
    
    
}

void VsnapUpdateWorkerThread(PVOID Buffer) 
{
	int          retval;
	VSNAP_TASK   *task;
	unsigned long    flags;
    VsnapInfo        *Vsnap = (VsnapInfo *)Buffer;
    bool              bShutdownThread = false;
    PVOID           WaitObjects[2];
    NTSTATUS        Status;
#if (NTDDI_VERSION >= NTDDI_WS03)
    NTSTATUS        StatusImpersonation;
#endif
    bool            bWaitThread = true; 
#if (NTDDI_VERSION >= NTDDI_WS03)
    bool bImpersonation = 0;
#endif
    WaitObjects[0] = &VsnapUpdatePool->TaskNotifyEvent;
    WaitObjects[1] = &Vsnap->KillThreadEvent;

    AcquireVsnapReadLock(VsnapUpdatePoolInfo->rwLock);
    InterlockedIncrement(&VsnapUpdatePoolInfo->ulCountThreads);
	
	do {
		if(bWaitThread) {
            Status = KeWaitForMultipleObjects(2, WaitObjects, WaitAny, Executive, KernelMode, FALSE, NULL, NULL);
            if (!NT_SUCCESS(Status)) {       
                break;
            }
        }

        if (STATUS_WAIT_1 == Status) {
            
            bShutdownThread = TRUE;
            InterlockedDecrement(&VsnapUpdatePoolInfo->ulCountThreads);
            ReleaseVsnapReadLock(VsnapUpdatePoolInfo->rwLock);
            return;
         
        }

        if (STATUS_WAIT_0 == Status) 
        {
	        GET_TASK_FROM_POOL(VsnapUpdatePoolInfo->pThreadPool, task);
		
            if (!task ) {
                bWaitThread = true;
			    continue;
            }
#if (NTDDI_VERSION >= NTDDI_WS03)
            if (!bImpersonation) {
                StatusImpersonation = VVImpersonate(&DriverContext.ImpersonatioContext, NULL);
                if (NT_SUCCESS(StatusImpersonation)) {
                    bImpersonation = 1;
                }
            }
#endif

		    switch( task->usTaskId ) {
			    case TASK_UPDATE_VSNAP:
                
			        VsnapUpdateMaps(task);
                    DeAllocateTask(task);
                    break;

			    default:
				//DBG("Unknown task");
				//ALERT();
                    break;
		    }
            bWaitThread = false;
        }
	}while(bShutdownThread == false);				
#if (NTDDI_VERSION >= NTDDI_WS03)
    if (bImpersonation) {
        SeStopImpersonatingClient();
    }
#endif
	return;

}

bool VsnapQueueUpdateTask(struct _UPDATE_VSNAP_VOLUME_INPUT *UpdateInfo, PIRP   Irp,bool *IsIOComplete)
{
    VsnapParentVolumeList	          *Parent = NULL;
    VsnapInfo                         *Vsnap = NULL;
    VSNAP_UPDATE_TASK                 *UpdateTask = NULL;
    VSNAP_TASK                        *task;
    ULONG                             BytesRead = 0;
    PVOID                             Buffer = NULL;
    bool                              Success = true;
    NTSTATUS                          Status = STATUS_SUCCESS;

    AcquireVsnapReadLock(ParentVolumeListLock);
    Parent = VsnapFindParentFromVolumeName(UpdateInfo->ParentVolumeName);

    if(NULL != Parent)
    {
        UpdateTask = (VSNAP_UPDATE_TASK*)AllocateUpdateCommand();
        if(UpdateTask == NULL) {

            ReleaseVsnapReadLock(ParentVolumeListLock);
            return false;
        }
        UpdateTask->InputData = UpdateInfo;
        UpdateTask->Irp = Irp;
        Vsnap = (VsnapInfo *)Parent->VsnapHead.Flink;
        if((Vsnap != NULL) && (Vsnap != (VsnapInfo *)&Parent->VsnapHead)) {
            if(UpdateInfo->Cow) {
                Buffer = (char *)ALLOC_MEMORY(UpdateInfo->Length, NonPagedPool);
                if(!Buffer)
                {
                    ReleaseVsnapReadLock(ParentVolumeListLock);
                //RELEASE_RW_LOCK(ParentVolumeListLock);
                //LEAVE_CRITICAL_REGION;
                    Buffer = NULL;
                    UpdateTask->CmdStatus = STATUS_UNSUCCESSFUL;
                    if(DereferenceUpdateCommand(UpdateTask))
                        *IsIOComplete = true;
                    return false;
                
                }
                if(Parent->IsFile) {
                    if(!READ_FILE(Vsnap->ParentPtr->SparseFileHandle, Buffer, UpdateInfo->VolOffset, UpdateInfo->Length, (int*)&BytesRead, true, &Status ))
                        {
                            InVolDbgPrint(DL_VV_ERROR, DM_VV_VSNAP, ("Vsnap: READ_FILE Failed in VsnapQueueUpdateTask\n"));
                            goto Cleanup;
                        }
            
                } else if(Parent->IsMultiPartSparseFile) {
                    
                    if (! ReadFromMultiPartSparseFile(Vsnap->ParentPtr->MultiPartSparseFileHandle, Buffer,  UpdateInfo->VolOffset, UpdateInfo->Length,
                                                      (int *)&BytesRead, Vsnap->ParentPtr->ChunkSize, &Status))
                    {
                            InVolDbgPrint(DL_VV_ERROR, DM_VV_VSNAP, ("Vsnap: ReadFromMultiPartSparseFile Failed in VsnapQueueUpdateTask\n"));
                            goto Cleanup;
                    }

                }else
                    VsnapReadFromVolume(UpdateInfo->ParentVolumeName, Parent->VolumeSectorSize,(PCHAR) Buffer, UpdateInfo->VolOffset, UpdateInfo->Length, &BytesRead);


Cleanup:       if((!NT_SUCCESS(Status)) || (BytesRead != UpdateInfo->Length))
                {    
                    if (!NT_SUCCESS(Status)) {
                        InVirVolLogError(DriverContext.ControlDevice, __LINE__, INVIRVOL_VOLPACK_READ_FAILURE, Status,
                                         &Vsnap->DevExtension->usMountDevLink.Buffer[DEV_LINK_GUID_OFFSET], 
                                         GUID_LEN_WITH_BRACE,
                                         UpdateInfo->VolOffset,
                                         (ULONGLONG)UpdateInfo->Length,
                                         Vsnap->SnapShotId);
                    }

                    FREE_MEMORY(Buffer);
                    ReleaseVsnapReadLock(ParentVolumeListLock);
                    //RELEASE_RW_LOCK(ParentVolumeListLock);
                    //LEAVE_CRITICAL_REGION;
                    Buffer = NULL;
                    UpdateTask->CmdStatus = STATUS_UNSUCCESSFUL;
                    if(DereferenceUpdateCommand(UpdateTask))
                        *IsIOComplete = true;
                    return false;
                }
            }

        }
    } else {
        ReleaseVsnapReadLock(ParentVolumeListLock);
        *IsIOComplete = false;
        return true;
    }

    UpdateTask->Buffer = Buffer;

    Vsnap = (VsnapInfo *)Parent->VsnapHead.Flink;
    while((Vsnap != NULL) && (Vsnap != (VsnapInfo *)&Parent->VsnapHead))
    {
        task = AllocateTask();
        if(!task) {
            Success = false;
            UpdateTask->CmdStatus = STATUS_UNSUCCESSFUL;
            Vsnap = (VsnapInfo*)Vsnap->LinkNext.Flink;
            continue;
        }
        /* Take reference on vsnap */
        if(!ACQUIRE_REMOVE_LOCK(&Vsnap->VsRemoveLock))
        {
            Success = false;
            UpdateTask->CmdStatus = STATUS_UNSUCCESSFUL; 
            Vsnap = (VsnapInfo*)Vsnap->LinkNext.Flink;
            DeAllocateTask(task);
            continue;                
        }
        task->Vsnap = Vsnap;
        task->usTaskId = TASK_UPDATE_VSNAP;
        ReferenceUpdateCommand(UpdateTask);
        task->TaskData = (PVOID)UpdateTask;
        ADD_TASK_TO_POOL(VsnapUpdatePool, task);
        Vsnap = (VsnapInfo*)Vsnap->LinkNext.Flink;
        //reference the buffer update task for every iteration of this loop
    }

    ReleaseVsnapReadLock(ParentVolumeListLock);
    IoMarkIrpPending(Irp); 
    DereferenceUpdateCommand(UpdateTask);
    *IsIOComplete = true;

    //if(*IsIOComplete == true)
    //return true instead of Success because any how we have to return STATUS_PENDING from dispatch routine
    return true;
    //else
      //  return true;
    
}

void VsnapUpdateMaps(VSNAP_TASK *task)
{
    VSNAP_UPDATE_TASK       *UpdateTask = (VSNAP_UPDATE_TASK *)task->TaskData;
    ULONG					BytesWritten = 0, ulFileid = 0, ulIndex = 0;
    VsnapInfo			    *Vsnap = task->Vsnap;
    etSearchResult          eResult = ecStatusNotFound;
    ULONGLONG               ullCurrentTS = 0, ullSTS = 0;
    bool                    err = false;

#if DBG
    Vsnap->ullNumberOfUpdates++;
#endif

    AcquireVsnapWriteLock(Vsnap->ParentPtr->TargetRWLock);
    if(UpdateTask->InputData->Cow) {
        VsnapWriteDataUsingMap(UpdateTask->Buffer, UpdateTask->InputData->VolOffset, UpdateTask->InputData->Length, &BytesWritten, Vsnap, true);
    } else {
            AcquireVsnapWriteLock(Vsnap->BtreeMapLock);
            //ACQUIRE_EXCLUSIVE_WRITE_LOCK(Vsnap->BtreeMapLock);
            if(!Vsnap->IsSparseRetentionEnabled) {
                if(Vsnap->LastDataFileName == NULL)
                {
                    Vsnap->LastDataFileName = (char *) ALLOC_MEMORY(MAX_RETENTION_DATA_FILENAME_LENGTH, NonPagedPool);
                    if(!Vsnap->LastDataFileName)
                    {
                        InVolDbgPrint(DL_VV_ERROR, DM_VV_VSNAP, ("Vsnap: Memory alloc failed in VsnapUpdateMaps\n"));
                        ReleaseVsnapWriteLock(Vsnap->BtreeMapLock);
                        goto Out;
                    }
                    RtlZeroMemory(Vsnap->LastDataFileName, sizeof(MAX_RETENTION_DATA_FILENAME_LENGTH));
                    STRING_COPY(Vsnap->LastDataFileName,  UpdateTask->InputData->DataFileName, MAX_RETENTION_DATA_FILENAME_LENGTH);
                } else if((0 != _strnicmp(Vsnap->LastDataFileName, UpdateTask->InputData->DataFileName, 
                                MAX_RETENTION_DATA_FILENAME_LENGTH)))
                    {
                        STRING_COPY(Vsnap->LastDataFileName, UpdateTask->InputData->DataFileName, MAX_RETENTION_DATA_FILENAME_LENGTH);
                        SetNextDataFileId(Vsnap,false);
                    }
            } else {
                ullSTS = ConvertStringToULLSTS(UpdateTask->InputData->DataFileName, false);
                if(ullSTS && (Vsnap->FileNameCache->ullCreationTS < ullSTS)) {
                    
                        ReleaseVsnapWriteLock(Vsnap->BtreeMapLock);
                        goto Out;
                }

                ullCurrentTS = ConvertStringToULLETS(UpdateTask->InputData->DataFileName, true);
                if(Vsnap->FileNameCache->ullCurrentTS != ullCurrentTS) {
                    Vsnap->FileNameCache->ullCurrentTS = ullCurrentTS;
                    DestroyCache(Vsnap);
                    Vsnap->FileNameCache->ulForwardFileId = Vsnap->DataFileIdUpdated != 0? (Vsnap->DataFileId + 1):Vsnap->DataFileId;
                    Vsnap->FileNameCache->ulBackwardFileId = 0;
                    SetNextDataFileId(Vsnap, true);
                    ASSERT(((Vsnap->DataFileId - 1) * MAX_RETENTION_DATA_FILENAME_LENGTH) == Vsnap->ullFileSize);
                } else {
                    eResult = SearchIntoCache(Vsnap, ConvertStringToULLHash(UpdateTask->InputData->DataFileName), &ulFileid, &ulIndex);
                    if(ecStatusNotFound == eResult) {
                        SetNextDataFileId(Vsnap, false);
                    } 
                }
            }

    
         
            VsnapUpdateMap(Vsnap, UpdateTask->InputData->VolOffset, UpdateTask->InputData->Length, eResult == ecStatusFound?ulFileid : Vsnap->DataFileId,
                    UpdateTask->InputData->FileOffset, UpdateTask->InputData->DataFileName,ulIndex);

            ReleaseVsnapWriteLock(Vsnap->BtreeMapLock);
    }

Out:
    /*Bug 22336 :- Release Parent lock than REMOVE_LOCK*/
    ReleaseVsnapWriteLock(Vsnap->ParentPtr->TargetRWLock);
    RELEASE_REMOVE_LOCK(&Vsnap->VsRemoveLock);
    DereferenceUpdateCommand(UpdateTask);
}

void
VVolumeIOWorkerThread(PVOID Buffer) 
{
	int          retval;
	VSNAP_TASK   *task;
	unsigned long    flags;
    THREAD_POOL_INFO *tPoolinfo = (THREAD_POOL_INFO *)Buffer;
    NTSTATUS        Status;
#if (NTDDI_VERSION >= NTDDI_WS03)
    NTSTATUS        StatusImpersonation;
#endif
    bool            bWaitThread = true; 
#if (NTDDI_VERSION >= NTDDI_WS03)
    bool            bImpersonation = 0;
#endif
	//DEV_DBG("Started new thread");
	//INM_READ_LOCK(&tp_info->tpi_rwlock);
	//INM_ATOMIC_INC(&tp_info->tpi_num_threads);

    AcquireVsnapReadLock(tPoolinfo->rwLock);
    
	
	for (;;) {
		
		if(bWaitThread) {
		    Status = WAIT_FOR_NEXT_TASK(tPoolinfo->pThreadPool);
		    if (!NT_SUCCESS(Status)) {
                break;
            }
        }
	 	// Its here means there is a task that needs processing
	 	
		GET_TASK_FROM_POOL(tPoolinfo->pThreadPool, task);
		
		if (!task ) {
            bWaitThread = true;
			continue;
        }
#if (NTDDI_VERSION >= NTDDI_WS03)
        if (!bImpersonation) {
            StatusImpersonation = VVImpersonate(&DriverContext.ImpersonatioContext, NULL);
            if (NT_SUCCESS(StatusImpersonation)) {
                bImpersonation = 1;
            }
        }
#endif

		switch( task->usTaskId ) {
			case TASK_READ_VVOLUME:

					VVolumeReadData(task);
					break;


			case TASK_WRITE_VVOLUME:
					VVolumeWriteData(task);
					break;
                    
			case TASK_START_QUEUED_IO:
                    VVolumeStartQueuedIo(task);
					break;

			case TASK_KILL_THREAD:
					//DEV_DBG("Killing thread");
					//INM_FREE(&task, sizeof(vsnap_task_t));
                    DeAllocateTask(task);
                    //INM_ATOMIC_DEC(&tp_info->tpi_num_threads);
                    InterlockedDecrement(&tPoolinfo->ulCountThreads);
					//INM_READ_UNLOCK(&tp_info->tpi_rwlock);
                    ReleaseVsnapReadLock(tPoolinfo->rwLock);
                    KeSetEvent(&tPoolinfo->pThreadPool->TaskNotifyEvent, EVENT_INCREMENT, FALSE);
					goto out;

			default:
				//DBG("Unknown task");
				//ALERT();
                break;
		}
         
        bWaitThread = false;
	}				

out:	
#if (NTDDI_VERSION >= NTDDI_WS03)
    if (bImpersonation) {
        SeStopImpersonatingClient();
    }
#endif
	return;

}

void VVolumeWriteData(VSNAP_TASK  *task)
{
    PIO_COMMAND_STRUCT  pIOCommand                      = (PIO_COMMAND_STRUCT)task->TaskData;
    PDEVICE_EXTENSION   DevExtension                    = pIOCommand->VVolume->DevExtension;
    NTSTATUS        Status = STATUS_SUCCESS;
    PVOID           pWriteBuffer;
    IO_STATUS_BLOCK IoStatus;
    PIRP            Irp = pIOCommand->Cmd.Write.Irp;

    IoStatus.Status = STATUS_SUCCESS;
    IoStatus.Information = 0;

    Irp->IoStatus.Information = 0;
    Irp->IoStatus.Status = Status;


    if(pIOCommand->Cmd.Write.ulLength != 0)
    {
        pWriteBuffer = MmGetSystemAddressForMdlSafe(Irp->MdlAddress, NormalPagePriority);
        if (NULL == pWriteBuffer) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto cleanup_and_exit;
            
        }

        BOOLEAN ZeroBlock = TRUE;
        ULONG WriteLength = pIOCommand->Cmd.Write.ulLength;
        UCHAR AllOnes = 0xFF;
        while(WriteLength)
        {
            if(((PUCHAR)pWriteBuffer)[--WriteLength] & AllOnes)
            {
                ZeroBlock = FALSE;
                break;
            }
        }

        if(pIOCommand->Cmd.Write.ByteOffset.QuadPart < 512)   //Boot Sector
        {
            //Replicated volume length could be different than the target volume Legnth.
            //Calculate new volume Length.
            DevExtension->uliVolumeSize.QuadPart = *(PLONGLONG ((PCHAR)pWriteBuffer + 40)) * 512;
        }

        if(ZeroBlock)
        {
            PIO_STACK_LOCATION IoStackLocation;
            KEVENT Event;
            PDEVICE_OBJECT FileSystemDeviceObject = NULL;
            PFILE_OBJECT FileSystemFileObject = NULL;
            HANDLE hFile = NULL;
            OBJECT_ATTRIBUTES ObjectAttributes;
            PIRP FileSystemIrp = NULL;
            
            InitializeObjectAttributes(&ObjectAttributes, &DevExtension->usFileName, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE , NULL, NULL);

            Status = ZwCreateFile(&hFile, GENERIC_READ | GENERIC_WRITE, &ObjectAttributes, &IoStatus, NULL, 
                            FILE_ATTRIBUTE_NORMAL | FILE_WRITE_ATTRIBUTES,
                            FILE_SHARE_READ | FILE_SHARE_WRITE, FILE_OPEN,
                            FILE_NON_DIRECTORY_FILE | FILE_NO_INTERMEDIATE_BUFFERING | FILE_SYNCHRONOUS_IO_NONALERT,
                            NULL, 0);

            if (!NT_SUCCESS(Status)) {
                InVolDbgPrint(DL_VV_WARNING, DM_VV_DEVICE_CONTROL, ("ZwCreateFile Failed: Status:%x\n", Status));
                goto cleanup_and_exit;
            }

            KeInitializeEvent(&Event, NotificationEvent, FALSE);

            FILE_ZERO_DATA_INFORMATION FileZeroDataInformation;
            FileZeroDataInformation.FileOffset.QuadPart = pIOCommand->Cmd.Write.ByteOffset.QuadPart;
            FileZeroDataInformation.BeyondFinalZero.QuadPart = pIOCommand->Cmd.Write.ByteOffset.QuadPart + pIOCommand->Cmd.Write.ulLength;

            Status = ObReferenceObjectByHandle(hFile, GENERIC_READ | GENERIC_WRITE, *IoFileObjectType ,
                        KernelMode,
                        (PVOID *)&FileSystemFileObject,
                        NULL
                    );

			if (!NT_SUCCESS(Status)) {
                InVolDbgPrint(DL_VV_WARNING, DM_VV_DEVICE_CONTROL, ("ObReferenceObjectByHandle Failed: Status:%x\n", Status));
				ZwClose(hFile);
                goto cleanup_and_exit;
            }

            FileSystemDeviceObject = IoGetRelatedDeviceObject(FileSystemFileObject);

            FileSystemIrp = IoBuildDeviceIoControlRequest(FSCTL_SET_ZERO_DATA, 
                            FileSystemDeviceObject,
                            &FileZeroDataInformation,
                            sizeof(FILE_ZERO_DATA_INFORMATION),
                            NULL,
                            0,
                            FALSE,
                            &Event,
                            &IoStatus);

            if(FileSystemIrp)
            {
                IoStackLocation                                 = IoGetNextIrpStackLocation(FileSystemIrp);
                IoStackLocation->MajorFunction                  = (UCHAR)IRP_MJ_FILE_SYSTEM_CONTROL;
                IoStackLocation->MinorFunction                  = IRP_MN_KERNEL_CALL;
                IoStackLocation->FileObject                     = FileSystemFileObject;
                
                Status = IoCallDriver(FileSystemDeviceObject, FileSystemIrp);

                if(STATUS_PENDING == Status)
                {
                    KeWaitForSingleObject(&Event, Executive, KernelMode, FALSE, NULL);
                }

                if(!NT_SUCCESS(Status))
                {
                    InVolDbgPrint(DL_VV_WARNING, DM_VV_DEVICE_CONTROL, ("FSCTL_SET_ZERO_DATA Failed: Status:%x\n", Status));
                }
                else
                {
                    InVolDbgPrint(DL_VV_WARNING, DM_VV_DEVICE_CONTROL, 
                        ("FSCTL_SET_ZERO_DATA Status Success %x BytesReturned:%x\n", Status, IoStatus.Information));
                    IoStatus.Information = pIOCommand->Cmd.Write.ulLength;
                }
            }
			else
			{
				 Status = STATUS_INSUFFICIENT_RESOURCES; 

			}

            ObDereferenceObject(FileSystemFileObject);
            ZwClose(hFile);
        }
        else
        {
            Status = ZwWriteFile(DevExtension->hFile, NULL, NULL, NULL, &IoStatus, pWriteBuffer, 
                pIOCommand->Cmd.Write.ulLength, &pIOCommand->Cmd.Write.ByteOffset, NULL);
        }

        pIOCommand->CmdStatus = Status;
        Irp->IoStatus.Information = IoStatus.Information;

    }

    
cleanup_and_exit:
    
    VVolumeEndIO(task);    
    return;


}
void VVolumeReadData(VSNAP_TASK  *task)
{
    bool                AllocationFailed                = false;
    PIO_COMMAND_STRUCT  pIOCommand                      = (PIO_COMMAND_STRUCT)task->TaskData;
    PVOID               pReadBuffer,pBuffer             = NULL;
    NTSTATUS            Status                          = STATUS_SUCCESS;
    PDEVICE_EXTENSION   DevExtension = pIOCommand->VVolume->DevExtension;

    IO_STATUS_BLOCK IoStatus;
    
    pReadBuffer = MmGetSystemAddressForMdlSafe(pIOCommand->Cmd.Read.Irp->MdlAddress, NormalPagePriority);
    if (NULL == pReadBuffer) {
        pIOCommand->CmdStatus = STATUS_INSUFFICIENT_RESOURCES;
        goto cleanup_and_exit;
    }


    pIOCommand->ulFlags |= FLAG_READ_INTO_TEMPORARY_BUFFER;

    pBuffer = ALLOC_MEMORY(pIOCommand->Cmd.Read.ulLength, PagedPool);
    if(!pBuffer) {
        AllocationFailed = true;
        pIOCommand->ulFlags &= ~FLAG_READ_INTO_TEMPORARY_BUFFER;
    }
    pIOCommand->pBuffer = pBuffer;
    pIOCommand->pOrigBuffer = pReadBuffer;
    
    Status = ZwReadFile(DevExtension->hFile, NULL, NULL, NULL, &IoStatus, AllocationFailed?(PCHAR)pReadBuffer:(PCHAR)pBuffer, 
              pIOCommand->Cmd.Read.ulLength, &pIOCommand->Cmd.Read.ByteOffset, NULL);
   
    pIOCommand->CmdStatus = Status;
cleanup_and_exit:
    
    VVolumeEndIO(task);    
    return;
    
 
}
void VVolumeEndIO(VSNAP_TASK  *task)
{
    PIO_COMMAND_STRUCT                   pIOCommand = (PIO_COMMAND_STRUCT)task->TaskData;
    VVolumeInfo                          *VVolume    = pIOCommand->VVolume;
    int                                  iIoType    = task->usTaskId;
    int                                  PendingIO, InflightIO;
    KIRQL                OldIrql;    

    
    ExAcquireFastMutex(&VVolume->hMutex); 
    InflightIO = --VVolume->IOsInFlight;
    PendingIO = VVolume->IOPending;
    if(iIoType == TASK_WRITE_VVOLUME) {
        VVolume->IOWriteInProgress = false;        
    }
    
    ExReleaseFastMutex(&VVolume->hMutex);
    
    DereferenceIOCommand(pIOCommand);

    if( !InflightIO) {

        if(0 != PendingIO) {
            task->usTaskId = TASK_START_QUEUED_IO;
            task->TaskData = (PVOID)VVolume;
            //ADD_TASK_TO_POOL(VVolumeIOThreadPool, task);
            VVolumeStartQueuedIo(task);


        } 
    }

	if ( InflightIO || !PendingIO)
		    DeAllocateTask(task);

}
void VVolumeStartQueuedIo(VSNAP_TASK  *ptask)
{
    
    PIO_COMMAND_STRUCT          pIOCommand    = NULL;
    VVolumeInfo                 *VVolume = (VVolumeInfo *)ptask->TaskData;
    KIRQL                       OldIrql;
    int                         iIoType,count;
    PVSNAP_TASK                 task = NULL;
    bool                        bUndoRemove = false,bFreeTask = false;
    ULONG                       PendingIO = 0;   

    
    DeAllocateTask(ptask);
    for(count = 0;;count++)
    {
        task = AllocateTask();
        if(!task) { 
            
            return;
        }
       
        ExAcquireFastMutex(&VVolume->hMutex); 
        
        pIOCommand = RemoveIOCommandFromQueue(VVolume->DevExtension->IOCmdQueue);
        if (NULL == pIOCommand) {
            ExReleaseFastMutex(&VVolume->hMutex);
            DeAllocateTask(task);
            return;
        }

        if(pIOCommand->ulCommand  == VVOLUME_CMD_READ)
            task->usTaskId = TASK_READ_VVOLUME;
        else if(pIOCommand->ulCommand == VVOLUME_CMD_WRITE)
            task->usTaskId = TASK_WRITE_VVOLUME;

        iIoType = task->usTaskId;
        task->TaskData = (PVOID)pIOCommand;

        if(!VVolume->IOWriteInProgress) {
            if(iIoType == TASK_WRITE_VVOLUME) 
            {
                if(count > 0) {
                    UndoRemoveIOCommandFromQueue(VVolume->DevExtension->IOCmdQueue, pIOCommand);
                    bFreeTask = true;
                    
                } else if (VVolume->IOsInFlight == 0) {
                    VVolume->IOWriteInProgress = true;
                    VVolume->IOPending--;
                    VVolume->IOsInFlight++;
                    ADD_TASK_TO_POOL(VVolumeIOThreadPool, task);

                } else {
                    UndoRemoveIOCommandFromQueue(VVolume->DevExtension->IOCmdQueue, pIOCommand);
                    bFreeTask = true;
                    
                }
            } else {            
                VVolume->IOPending--;
                VVolume->IOsInFlight++;
                ADD_TASK_TO_POOL(VVolumeIOThreadPool, task);
            }
        }
        else {
            UndoRemoveIOCommandFromQueue(VVolume->DevExtension->IOCmdQueue, pIOCommand);
            bFreeTask = true;          
        }

        ExReleaseFastMutex(&VVolume->hMutex);

        if(bFreeTask) {
            DeAllocateTask(task);
            break;
        }
        
        switch(iIoType)
        {
            case TASK_WRITE_VVOLUME:                
                return;
            case TASK_READ_VVOLUME:
                break;
        }

    }
}

bool 
ReadFromMultiPartSparseFile(void **Handle, void *Buffer, ULONGLONG Offset, ULONG Length, int *BytesRead, ULONGLONG ChunkSize, NTSTATUS *ErrStatus)
{
    ULONGLONG StartOfIO = Offset;
    ULONGLONG EndOfIO = StartOfIO + Length - 1, tEndOfIO;
    ULONG FileHandleCount = 0, StartFileHandleIndex = 0;
    ULONGLONG FileOffset = 0;
    int pBytesRead = 0, PartialLen = 0;
   
    // We are retuning Read IRP with length '0' as success in Dispatch Routine
    if ((ChunkSize == 0) || (EndOfIO < StartOfIO)) {
        *ErrStatus = STATUS_UNSUCCESSFUL;
        return false;
    }
    StartFileHandleIndex = (ULONG)(StartOfIO / ChunkSize);

    FileHandleCount = 1;
    tEndOfIO = EndOfIO;

    while (tEndOfIO/ChunkSize != StartOfIO/ChunkSize) {
        FileHandleCount++;
        tEndOfIO -= ChunkSize;
    }

    if (FileHandleCount == 1) {
        FileOffset = StartOfIO - (StartFileHandleIndex  * ChunkSize);

        if (!READ_FILE((void *)Handle[StartFileHandleIndex], Buffer, FileOffset, Length, BytesRead, true, ErrStatus)) {
            return false;
        }
    } else if(FileHandleCount > 1) {
        FileOffset = StartOfIO - (StartFileHandleIndex  * ChunkSize);
        PartialLen = (int)(ChunkSize - FileOffset);

        if (!READ_FILE((void *)Handle[StartFileHandleIndex], Buffer, FileOffset, PartialLen, &pBytesRead, true, ErrStatus)) {
            return false;
        }
        ASSERT(pBytesRead == PartialLen);
        *BytesRead = pBytesRead;

        int ReadLen = 0, BufferIndex = 0;
        char* ActualBuffer;

        BufferIndex = PartialLen;
        PartialLen = Length - PartialLen;

        while(PartialLen) {
            FileOffset = 0;
            ActualBuffer = (char *)Buffer + BufferIndex;
            if ((ULONGLONG)PartialLen >= ChunkSize) {
                // IRP read length MAX value is ULONG, This case happens only if chunksize is small.
                // if Chunk size is greater than ULONG value(i.e 4GB), anyhow length can not be greater than 4GB and fits in the else
                // condition
                ReadLen = (int)ChunkSize;
                PartialLen -= (int)ChunkSize;
                BufferIndex += (int)ChunkSize;
            } else {
                ReadLen = PartialLen;
                PartialLen = 0;
            }

            if (!READ_FILE((void *)Handle[++StartFileHandleIndex], ActualBuffer, FileOffset, ReadLen, &pBytesRead, true, ErrStatus)) {
                return false;
            }
            *BytesRead += pBytesRead;
        }
    }
    ASSERT(Length == *BytesRead);
    if (Length != *BytesRead) {
        *ErrStatus = STATUS_UNSUCCESSFUL;
        return false;
    }
    return true;
}
