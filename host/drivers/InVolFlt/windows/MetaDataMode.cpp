#include "Common.h"
#include "VContext.h"
#include "MetaDataMode.h"
#include "Misc.h"
#ifdef VOLUME_FLT
#include "MntMgr.h"
#endif
#include "VBitmap.h"
#ifdef VOLUME_FLT
NTSTATUS
InMageFltIoCompletion(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP           Irp,
    IN PVOID          Context
    )
{
    NTSTATUS            Status = Irp->IoStatus.Status;
    NTSTATUS            NewStatus = STATUS_SUCCESS;
    PDEV_CONTEXT        pDevContext = NULL;
    KIRQL OldIrql;
    WRITE_META_DATA MetaData;
#ifndef VOLUME_NO_REBOOT_SUPPORT
	UNREFERENCED_PARAMETER(DeviceObject);
#else
    PCOMPLETION_CONTEXT pCompletionContext = NULL;
    etContextType       eContextType = (etContextType)*((PCHAR )Context + sizeof(LIST_ENTRY));

    if (eContextType == ecCompletionContext) {
        pCompletionContext = (PCOMPLETION_CONTEXT) Context;
        pDevContext = pCompletionContext->DevContext;
        MetaData.llOffset = pCompletionContext->llOffset;
        MetaData.ulLength = pCompletionContext->ulLength;
    } else {
#endif
        PIO_STACK_LOCATION irpStack = IoGetCurrentIrpStackLocation(Irp);
        pDevContext = (PDEV_CONTEXT) Context;
        MetaData.llOffset = irpStack->Parameters.Write.ByteOffset.QuadPart;
        MetaData.ulLength = irpStack->Parameters.Write.Length;
        
        ASSERT(irpStack->MajorFunction == IRP_MJ_WRITE);
        if (Irp->PendingReturned) {
            IoMarkIrpPending(Irp);
        }
        InVolDbgPrint(DL_FUNC_TRACE, DM_WRITE | DM_DEV_CONTEXT, 
            ("InMageFltIoCompletion: pDevContext = %#p, Irp=%#p, length=%#lx, offset=%#I64x, IoStatus.Information = %#x\n",
            pDevContext, Irp, irpStack->Parameters.Write.Length,
            irpStack->Parameters.Write.ByteOffset.QuadPart, Irp->IoStatus.Information ));
#ifdef VOLUME_NO_REBOOT_SUPPORT
    }
#endif
    ASSERT(pDevContext != NULL);

    if (NT_SUCCESS(Irp->IoStatus.Status)) {
        if ((NULL == pDevContext->pDevBitmap) &&
            (0 == (pDevContext->ulFlags & DCF_OPEN_BITMAP_REQUESTED)))
        {
#ifdef VOLUME_CLUSTER_SUPPORT
            if (IS_VOLUME_ONLINE(pDevContext)) {
#endif
                if (CanOpenBitmapFile(pDevContext, FALSE)) {
                    InVolDbgPrint(DL_INFO, DM_BITMAP_OPEN | DM_WRITE,
                        ("InMageFltIoCompletion: Requesting service thread to open bitmap for DevContext %p, DevNum=%s\n",
                                        pDevContext, pDevContext->chDevNum));
                    RequestServiceThreadToOpenBitmap(pDevContext);
                }
#ifdef VOLUME_CLUSTER_SUPPORT
            } else {
                InVolDbgPrint(DL_ERROR, DM_BITMAP_OPEN | DM_WRITE | DM_CLUSTER,
                    ("InMageFltIoCompletion: Completion called after volume is released DevContext is %p, DevNum=%s, Status=0x%x\n",
                                    pDevContext, pDevContext->chDevNum, Irp->IoStatus.Status));
            }
#endif
        }
    } else {
        InVolDbgPrint(DL_INFO, DM_CLUSTER, ("InMageFltIoCompletion: Completion called DevContext is %p, DevNum=%s, Status=0x%x\n",
                            pDevContext, pDevContext->chDevNum, Irp->IoStatus.Status));
    }

    KeAcquireSpinLock(&pDevContext->Lock, &OldIrql);
    if (NT_SUCCESS(Irp->IoStatus.Status) && (0 == (pDevContext->ulFlags & DCF_FILTERING_STOPPED))) {
        //
        // Update Counters
        //
        for (ULONG ul = 0; ul < MAX_DC_IO_SIZE_BUCKETS; ul++) {
            if (pDevContext->ulIoSizeArray[ul]) {
                if (MetaData.ulLength <= pDevContext->ulIoSizeArray[ul]) {
                    pDevContext->ullIoSizeCounters[ul]++;
                    break;
                }
            } else {
                pDevContext->ullIoSizeCounters[ul]++;
                break;
            }
        }

        //
        // Save MetaData
        //
        NewStatus = AddMetaDataToDevContextNew(pDevContext, &MetaData);
        if (!NT_SUCCESS(NewStatus)) {
            InVolDbgPrint(DL_ERROR, DM_WRITE | DM_DIRTY_BLOCKS, 
                ("InMageFltWrite: Out of NP Pool to store changes (DevContext %#p) ulTotalChangesPending = %d, DirtyBlockInQueue = %d\n", 
                pDevContext, pDevContext->ChangeList.ulTotalChangesPending, pDevContext->ChangeList.lDirtyBlocksInQueue));

            SetDBallocFailureError(pDevContext);

            QueueWorkerRoutineForSetDevOutOfSync(pDevContext, (ULONG)ERROR_TO_REG_OUT_OF_MEMORY_FOR_DIRTY_BLOCKS, STATUS_NO_MEMORY, true);

            // Let us free the commit pending dirty blocks too.
            OrphanMappedDataBlocksAndMoveCommitPendingDirtyBlockToMainQueue(pDevContext);

            // as we have lost data because NonPagedPool is full
            // dump all the change data to keep the system from crashing
            FreeChangeNodeList(&pDevContext->ChangeList);
        }
    }
    KeReleaseSpinLock(&pDevContext->Lock, OldIrql);
    DereferenceDevContext(pDevContext);
#ifdef VOLUME_NO_REBOOT_SUPPORT
    if (eContextType == ecCompletionContext) {
        InVolDbgPrint(DL_INFO, DM_CLUSTER, ("InMageFltIoCompletion:  Info = %#x, Status = %#x\n",
                Irp->IoStatus.Information, Irp->IoStatus.Status ));
        // Complete the original IRP
        if (pCompletionContext->CompletionRoutine) {
            PIO_STACK_LOCATION IoNextStackLocation = IoGetNextIrpStackLocation(Irp);
            IoNextStackLocation->CompletionRoutine = pCompletionContext->CompletionRoutine;
            IoNextStackLocation->Context = pCompletionContext->Context;
            IoNextStackLocation->Control = pCompletionContext->Control;
            ASSERT(pCompletionContext->Control & (SL_INVOKE_ON_SUCCESS | SL_INVOKE_ON_ERROR | SL_INVOKE_ON_CANCEL));
            Status = (*pCompletionContext->CompletionRoutine)(DeviceObject, Irp, pCompletionContext->Context);

        }
        ExFreePool(pCompletionContext);          
    }
#endif
    return Status;
} // InMageFltIoCompletion
#endif

NTSTATUS
InCaptureIOInMetaDataMode(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP           Irp,
    IN PVOID          Context
    )
/*++
    Similar to InMageFltIoCompletion
--*/
{
    NTSTATUS            Status = Irp->IoStatus.Status;
    NTSTATUS            NewStatus = STATUS_SUCCESS;
    KIRQL OldIrql;
    WRITE_META_DATA MetaData;
    PWRITE_COMPLETION_FIELDS pWriteCompFields = (PWRITE_COMPLETION_FIELDS)Context;
    PDEV_CONTEXT        pDevContext = pWriteCompFields->pDevContext;

    UNREFERENCED_PARAMETER(DeviceObject);

    ASSERT(pDevContext != NULL);

    MetaData.llOffset = pWriteCompFields->llOffset;
    MetaData.ulLength = pWriteCompFields->ulLength;

    InVolDbgPrint(DL_FUNC_TRACE, DM_WRITE | DM_DEV_CONTEXT, 
            ("%s: pDevContext = %#p, Irp=%#p, length=%#lx, offset=%#I64x, IoStatus.Information = %#x\n",
            __FUNCTION__,
            pDevContext, Irp, MetaData.ulLength,
            MetaData.llOffset, Irp->IoStatus.Information ));

    if (NT_SUCCESS(Irp->IoStatus.Status)) {
        if ((NULL == pDevContext->pDevBitmap) &&
            (0 == (pDevContext->ulFlags & DCF_OPEN_BITMAP_REQUESTED)))
        {
            if (CanOpenBitmapFile(pDevContext, FALSE)) {
                InVolDbgPrint(DL_INFO, DM_BITMAP_OPEN | DM_WRITE,
                        ("%s: Requesting service thread to open bitmap for DevContext %p, DevNum=%s\n",
                                        __FUNCTION__, pDevContext, pDevContext->chDevNum));
                RequestServiceThreadToOpenBitmap(pDevContext);
            }
        }
    } else {
        InVolDbgPrint(DL_INFO, DM_CLUSTER, ("%s: Completion called DevContext is %p, DevNum=%s, Status=0x%x\n",
                            __FUNCTION__, pDevContext, pDevContext->chDevNum, Irp->IoStatus.Status));
    }

    KeAcquireSpinLock(&pDevContext->Lock, &OldIrql);
    if (NT_SUCCESS(Irp->IoStatus.Status) && (0 == (pDevContext->ulFlags & DCF_FILTERING_STOPPED))) {

        if (Irp->IoStatus.Information != MetaData.ulLength)
        {
            InDskFltWriteEventWithDevCtxt(INDSKFLT_ERROR_PARTIAL_IO_DETAILED,
                pDevContext,
                Irp->IoStatus.Information,
                MetaData.llOffset,
                MetaData.ulLength,
                ecCaptureModeMetaData);

            QueueWorkerRoutineForSetDevOutOfSync(pDevContext,
                MSG_INDSKFLT_WARN_PARTIAL_IO_Message,
                Irp->IoStatus.Status,
                true);
        }

        //
        // Update Counters
        //
        pDevContext->ullTotalTrackedBytes += MetaData.ulLength;
        pDevContext->ullTotalIoCount++;
        for (ULONG ul = 0; ul < MAX_DC_IO_SIZE_BUCKETS; ul++) {
            if (pDevContext->ulIoSizeArray[ul]) {
                if (MetaData.ulLength <= pDevContext->ulIoSizeArray[ul]) {
                    pDevContext->ullIoSizeCounters[ul]++;
                    break;
                }
            } else {
                pDevContext->ullIoSizeCounters[ul]++;
                break;
            }
        }

        //
        // Save MetaData
        //
        NewStatus = AddMetaDataToDevContextNew(pDevContext, &MetaData);
        if (!NT_SUCCESS(NewStatus)) {
            InVolDbgPrint(DL_ERROR, DM_WRITE | DM_DIRTY_BLOCKS, 
                ("%s: Out of NP Pool to store changes (DevContext %#p) ulTotalChangesPending = %d, DirtyBlockInQueue = %d\n", 
                __FUNCTION__, pDevContext, pDevContext->ChangeList.ulTotalChangesPending, pDevContext->ChangeList.lDirtyBlocksInQueue));

            SetDBallocFailureError(pDevContext);

            QueueWorkerRoutineForSetDevOutOfSync(pDevContext, (ULONG)ERROR_TO_REG_OUT_OF_MEMORY_FOR_DIRTY_BLOCKS, STATUS_NO_MEMORY, true);

            // Let us free the commit pending dirty blocks too.
            OrphanMappedDataBlocksAndMoveCommitPendingDirtyBlockToMainQueue(pDevContext);

            // as we have lost data because NonPagedPool is full
            // dump all the change data to keep the system from crashing
            FreeChangeNodeList(&pDevContext->ChangeList, ecNonPagedPoolLimitHitMDMode);
        }
    }
    KeReleaseSpinLock(&pDevContext->Lock, OldIrql);
    DereferenceDevContext(pDevContext);
    return Status;
}


// This function is called with FDC context lock held.
NTSTATUS
AddMetaDataToDevContextNew(
    PDEV_CONTEXT     DevContext,
    PWRITE_META_DATA    pWriteMetaData
    )
{
    NTSTATUS        Status = STATUS_SUCCESS;
    PKDIRTY_BLOCK   DirtyBlock = NULL;

    InVolDbgPrint(DL_INFO, DM_DATA_FILTERING, ("AddMetaDataToVC: Offset=%#I64x length=%#x\n",
                                               pWriteMetaData->llOffset, pWriteMetaData->ulLength));

    DirtyBlock = GetCurrentDirtyBlockFromChangeList(DevContext);

    /*When a change is tracked in metadata mode. We should not change capture mode to metadata un-neccesarily 
     (for example if there enough free data-blocks and pair has not reached Max. Dev limit). 
     We should just capture the new change in metadata mode, ensure that WOS is non-data, do not change 
     capture mode to metadata, close current dirty block if dirty-block is in data-capture-mode 
	 (compensate correclty for the lost data bytes)*/

    if (ecCaptureModeData == DevContext->eCaptureMode) {
        if (DevContext->eWOState == ecWriteOrderStateData) {
            VCSetCaptureModeToMetadata(DevContext, false);
            VCSetWOState(DevContext, false, __FUNCTION__, ecMDReasonMetaDataModeCompletionpath);
        }

        if ((NULL != DirtyBlock) && (INFLTDRV_DATA_SOURCE_DATA == DirtyBlock->ulDataSource)) {
            // Close current dirty block if it is in Data capture mode.
            // Make sure that LostBytes are compensated for correctly.
            AddLastChangeTimestampToCurrentNode(DevContext, NULL, true);
        }
    }

    ASSERT(DevContext->eWOState != ecWriteOrderStateData);
    if ((NULL != DirtyBlock) && (INFLTDRV_DATA_SOURCE_DATA == DirtyBlock->ulDataSource)) {
        ASSERT(0 != DirtyBlock->TagTimeStampOfLastChange.TimeInHundNanoSecondsFromJan1601);
    }
        
    if (NULL != DirtyBlock) {
        if ((INFLTDRV_DATA_SOURCE_META_DATA != DirtyBlock->ulDataSource) ||
             (DevContext->eWOState != DirtyBlock->eWOState) ||
             (0 != DirtyBlock->TagTimeStampOfLastChange.TimeInHundNanoSecondsFromJan1601))//Dirty Block is closed earlier. Use a new one
        {
            // Data source is different need a new dirty block.
            DirtyBlock = NULL;
        }
    }

    if (0 == AddMetaDataToDirtyBlock(DevContext, DirtyBlock, pWriteMetaData, INFLTDRV_DATA_SOURCE_META_DATA)) {
        Status = STATUS_NO_MEMORY;
    }

    if ((NULL != DevContext->DBNotifyEvent) && (DevContext->bNotify)) {
        ASSERT(DevContext->ChangeList.ullcbTotalChangesPending >= DevContext->ullcbChangesPendingCommit);
        if (DirtyBlockReadyForDrain(DevContext)) {
            DevContext->bNotify = false;
            DevContext->liTickCountAtNotifyEventSet = KeQueryPerformanceCounter(NULL);
            KeSetEvent(DevContext->DBNotifyEvent, EVENT_INCREMENT, FALSE);
        }
    }

    return Status;
}

// This function is called with Device context lock held.
NTSTATUS
AddMetaDataToDevContext(
    PDEV_CONTEXT     DevContext,
    PWRITE_META_DATA    pWriteMetaData,
    BOOLEAN             bDataMode,
    etCModeMetaDataReason eMetaDataModeReason
    )
{
    NTSTATUS        Status = STATUS_SUCCESS;
    PKDIRTY_BLOCK   DirtyBlock = NULL;
    ULONG           ulRecordSize = SVRecordSizeForIOSize(pWriteMetaData->ulLength, DevContext->ulMaxDataSizePerDataModeDirtyBlock);

    InVolDbgPrint(DL_INFO, DM_DATA_FILTERING, ("AddMetaDataToVC: Offset=%#I64x length=%#x, ulRecordSize=%#x\n",
                                               pWriteMetaData->llOffset, pWriteMetaData->ulLength, ulRecordSize));
    if (ecCaptureModeData == DevContext->eCaptureMode) {
        ASSERT(TRUE == bDataMode);

        VCSetCaptureModeToMetadata(DevContext, false);
        VCSetWOState(DevContext, false, __FUNCTION__, eMetaDataModeReason);

        // Need to add a new dirty block for meta data.
        if (0 == AddMetaDataToDirtyBlock(DevContext, NULL, pWriteMetaData, INFLTDRV_DATA_SOURCE_META_DATA)) {
            Status = STATUS_NO_MEMORY;
        }

        if (DevContext->ullcbDataAlocMissed) {
            ASSERT(DevContext->ullcbDataAlocMissed >= ulRecordSize);
            if (DevContext->ullcbDataAlocMissed > ulRecordSize)
                DevContext->ullcbDataAlocMissed -= ulRecordSize;
            else
                DevContext->ullcbDataAlocMissed = 0;
        }
    } else {
        DirtyBlock = GetCurrentDirtyBlockFromChangeList(DevContext);
        if (NULL != DirtyBlock) {
            if ((INFLTDRV_DATA_SOURCE_META_DATA != DirtyBlock->ulDataSource) ||
                 (DevContext->eWOState != DirtyBlock->eWOState) ||
                 (0 != DirtyBlock->TagTimeStampOfLastChange.TimeInHundNanoSecondsFromJan1601))//Dirty Block is closed earlier. Use a new one
            {
                // Data source is different need a new dirty block.
                DirtyBlock = NULL;
            }
        }

        if (0 == AddMetaDataToDirtyBlock(DevContext, DirtyBlock, pWriteMetaData, INFLTDRV_DATA_SOURCE_META_DATA)) {
            Status = STATUS_NO_MEMORY;
        }

        if (TRUE == bDataMode) {
            if (DevContext->ullcbDataAlocMissed) {
                ASSERT(DevContext->ullcbDataAlocMissed >= ulRecordSize);
                if (DevContext->ullcbDataAlocMissed >= ulRecordSize)
                    DevContext->ullcbDataAlocMissed -= ulRecordSize;
                else
                    DevContext->ullcbDataAlocMissed = 0;
            }
        }
    }

    if ((NULL != DevContext->DBNotifyEvent) && (DevContext->bNotify)) {
        ASSERT(DevContext->ChangeList.ullcbTotalChangesPending >= DevContext->ullcbChangesPendingCommit);
        if (DirtyBlockReadyForDrain(DevContext)) {
            DevContext->bNotify = false;
            DevContext->liTickCountAtNotifyEventSet = KeQueryPerformanceCounter(NULL);
            KeSetEvent(DevContext->DBNotifyEvent, EVENT_INCREMENT, FALSE);
        }
    }

    return Status;
}

#ifdef VOLUME_FLT
void
WriteDispatchInMetaDataMode(
    PDEVICE_OBJECT  DeviceObject,
    PIRP            Irp
    )
{
    ASSERT((NULL != DeviceObject) && (NULL != Irp));

    PDEVICE_EXTENSION   DeviceExtension = (PDEVICE_EXTENSION)DeviceObject->DeviceExtension;
    PDEV_CONTEXT     DevContext = DeviceExtension->pDevContext;
#ifdef VOLUME_NO_REBOOT_SUPPORT
    PIO_STACK_LOCATION  IoStackLocation = IoGetCurrentIrpStackLocation(Irp);
#endif
    BOOLEAN             bWakeServiceThread = FALSE;

    ASSERT(NULL != DeviceExtension->TargetDeviceObject);

    // If Non Page pool consumption exceeds 80% of Max. Non Page Pool Limit,
    // write changes to Bitmap. This is to avoid pair resync which occurs if we reach
    // max. non-page limit.
    if ((0 != DriverContext.ulMaxNonPagedPool) && (DriverContext.lNonPagedMemoryAllocated >= 0)) {
        ULONGLONG NonPagePoolLimit = DriverContext.ulMaxNonPagedPool * 8;
        NonPagePoolLimit /= 10;
        if ((ULONG) DriverContext.lNonPagedMemoryAllocated >= (ULONG) NonPagePoolLimit) {
            bWakeServiceThread = TRUE;
        }    
    }

    if (DriverContext.DBHighWaterMarks[DriverContext.eServiceState] &&
        (DevContext->ChangeList.ulTotalChangesPending >= DriverContext.DBHighWaterMarks[DriverContext.eServiceState])) 
    {
        bWakeServiceThread = TRUE;
    }
#ifdef VOLUME_NO_REBOOT_SUPPORT
    if (!DevContext->IsDevUsingAddDevice && DriverContext.IsDispatchEntryPatched) {
        COMPLETION_CONTEXT  *pCompRoutine = (PCOMPLETION_CONTEXT)ExAllocatePoolWithTag(NonPagedPool,
                                                                                       sizeof(COMPLETION_CONTEXT),
                                                                                       TAG_GENERIC_NON_PAGED);
        if (pCompRoutine) {
            RtlZeroMemory(pCompRoutine, sizeof(COMPLETION_CONTEXT));
            ReferenceDevContext(DevContext);
            pCompRoutine->eContextType = ecCompletionContext;
            pCompRoutine->DevContext = DevContext;
            pCompRoutine->llOffset = IoStackLocation->Parameters.Write.ByteOffset.QuadPart;
            pCompRoutine->ulLength = IoStackLocation->Parameters.Write.Length;
            
            pCompRoutine->CompletionRoutine = IoStackLocation->CompletionRoutine;
            pCompRoutine->Context = IoStackLocation->Context;
            pCompRoutine->Control = IoStackLocation->Control;

            IoStackLocation->CompletionRoutine = InMageFltIoCompletion;
            IoStackLocation->Context = pCompRoutine;
            IoStackLocation->Control = SL_INVOKE_ON_SUCCESS;
            IoStackLocation->Control |= SL_INVOKE_ON_ERROR;
            IoStackLocation->Control |= SL_INVOKE_ON_CANCEL;
        } else {
            // We should mark for resync at first time when we are out of non paged memory
            QueueWorkerRoutineForSetDevOutOfSync(DevContext, MSG_INDSKFLT_ERROR_NO_GENERIC_NPAGED_POOL_Message, STATUS_NO_MEMORY, false);
        }
    } else {
#endif
        //
        // Copy current stack to next stack.
        //
        IoCopyCurrentIrpStackLocationToNext(Irp);
        ReferenceDevContext(DevContext);
        //
        // Set completion routine callback.
        //
        IoSetCompletionRoutine(Irp,
                        InMageFltIoCompletion,
                        DevContext,
                        TRUE,
                        TRUE,
                        TRUE);
#ifdef VOLUME_NO_REBOOT_SUPPORT
    }
#endif
    if (bWakeServiceThread) {
        InVolDbgPrint(DL_INFO, DM_DEVICE_STATE, 
            ("InMageFltWrite: Waking service thread, ServiceState = %#x\n", DriverContext.eServiceState));
        KeSetEvent(&DriverContext.ServiceThreadWakeEvent, IO_DISK_INCREMENT, FALSE);
    }

    return;
}
#endif

void
InWriteDispatchInMetaDataMode(
    IN PDEVICE_OBJECT  DeviceObject,
    IN PIRP            Irp
    )
/*++
    Similar to WriteDispatchInMetaDataMode
--*/

{
    ASSERT(NULL != DeviceObject);

    UNREFERENCED_PARAMETER(Irp);

    PDEVICE_EXTENSION   DeviceExtension = (PDEVICE_EXTENSION)DeviceObject->DeviceExtension;
    PDEV_CONTEXT        DevContext = DeviceExtension->pDevContext;
    BOOLEAN             bWakeServiceThread = FALSE;

    ASSERT(NULL != DeviceExtension->TargetDeviceObject);

    // If Non Page pool consumption exceeds 80% of Max. Non Page Pool Limit,
    // write changes to Bitmap. This is to avoid pair resync which occurs if we reach
    // max. non-page limit.
    if ((0 != DriverContext.ulMaxNonPagedPool) && (DriverContext.lNonPagedMemoryAllocated >= 0)) {
        ULONGLONG NonPagePoolLimit = DriverContext.ulMaxNonPagedPool * 8;
        NonPagePoolLimit /= 10;
        if ((ULONG) DriverContext.lNonPagedMemoryAllocated >= (ULONG) NonPagePoolLimit) {
            bWakeServiceThread = TRUE;
        }
    }

    if (DriverContext.DBHighWaterMarks[DriverContext.eServiceState] &&
        (DevContext->ChangeList.ulTotalChangesPending >= DriverContext.DBHighWaterMarks[DriverContext.eServiceState])) 
    {
        bWakeServiceThread = TRUE;
    }
    if (bWakeServiceThread) {
        InVolDbgPrint(DL_INFO, DM_DEVICE_STATE, 
            ("InMageFltWrite: Waking service thread, ServiceState = %#x\n", DriverContext.eServiceState));
        KeSetEvent(&DriverContext.ServiceThreadWakeEvent, IO_DISK_INCREMENT, FALSE);
    }

    return;
}
