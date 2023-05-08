#include "Common.h"
#include "DataFlt.h"
#include "VContext.h"
#include "MetaDataMode.h"
#include "Misc.h"
#include "MntMgr.h"
#include "VBitmap.h"

NTSTATUS
InMageFltIoCompletion(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP           Irp,
    IN PVOID          Context
    )
{
    PCOMPLETION_CONTEXT pCompletionContext = NULL;
    NTSTATUS            Status = Irp->IoStatus.Status;
    NTSTATUS            NewStatus = STATUS_SUCCESS;
    PVOLUME_CONTEXT     pVolumeContext = NULL;
    etContextType       eContextType = (etContextType)*((PCHAR )Context + sizeof(LIST_ENTRY));
    KIRQL OldIrql;
    WRITE_META_DATA MetaData;

    if (eContextType == ecCompletionContext) {
        pCompletionContext = (PCOMPLETION_CONTEXT) Context;
        pVolumeContext = pCompletionContext->VolumeContext;
        MetaData.llOffset = pCompletionContext->llOffset;
        MetaData.ulLength = pCompletionContext->ulLength;
    } else {
        PIO_STACK_LOCATION irpStack = IoGetCurrentIrpStackLocation(Irp);
        pVolumeContext = (PVOLUME_CONTEXT) Context;
        MetaData.llOffset = irpStack->Parameters.Write.ByteOffset.QuadPart;
        MetaData.ulLength = irpStack->Parameters.Write.Length;
        
        ASSERT(irpStack->MajorFunction == IRP_MJ_WRITE);
        if (Irp->PendingReturned) {
            IoMarkIrpPending(Irp);
        }
        InVolDbgPrint(DL_FUNC_TRACE, DM_WRITE | DM_VOLUME_CONTEXT, 
            ("InMageFltIoCompletion: pVolumeContext = %#p, Irp=%#p, length=%#lx, offset=%#I64x, IoStatus.Information = %#x\n",
            pVolumeContext, Irp, irpStack->Parameters.Write.Length,
            irpStack->Parameters.Write.ByteOffset.QuadPart, Irp->IoStatus.Information ));
    }

    ASSERT(pVolumeContext != NULL);

    if (NT_SUCCESS(Irp->IoStatus.Status)) {
        if ((NULL == pVolumeContext->pVolumeBitmap) &&
            (0 == (pVolumeContext->ulFlags & VCF_OPEN_BITMAP_REQUESTED)))
        {
            if (IS_VOLUME_ONLINE(pVolumeContext)) {
                if (CanOpenBitmapFile(pVolumeContext, FALSE)) {
                    InVolDbgPrint(DL_INFO, DM_BITMAP_OPEN | DM_WRITE,
                        ("InMageFltIoCompletion: Requesting service thread to open bitmap for VolumeContext %p, DriveLetter = %c\n",
                                        pVolumeContext, (CHAR)pVolumeContext->wDriveLetter[0]));
                    RequestServiceThreadToOpenBitmap(pVolumeContext);
                }
            } else {
                InVolDbgPrint(DL_ERROR, DM_BITMAP_OPEN | DM_WRITE | DM_CLUSTER,
                    ("InMageFltIoCompletion: Completion called after volume is released VolumeContext is %p, DriveLetter=%c, Status=0x%x\n",
                                    pVolumeContext, (CHAR)pVolumeContext->wDriveLetter[0], Irp->IoStatus.Status));
            }
        }
    } else {
        InVolDbgPrint(DL_INFO, DM_CLUSTER, ("InMageFltIoCompletion: Completion called VolumeContext is %p, DriveLetter=%c, Status=0x%x\n",
                            pVolumeContext, (CHAR)pVolumeContext->wDriveLetter[0], Irp->IoStatus.Status));
    }

    KeAcquireSpinLock(&pVolumeContext->Lock, &OldIrql);
    if (NT_SUCCESS(Irp->IoStatus.Status) && (0 == (pVolumeContext->ulFlags & VCF_FILTERING_STOPPED))) {
        //
        // Update Counters
        //
        for (ULONG ul = 0; ul < MAX_VC_IO_SIZE_BUCKETS; ul++) {
            if (pVolumeContext->ulIoSizeArray[ul]) {
                if (MetaData.ulLength <= pVolumeContext->ulIoSizeArray[ul]) {
                    pVolumeContext->ullIoSizeCounters[ul]++;
                    break;
                }
            } else {
                pVolumeContext->ullIoSizeCounters[ul]++;
                break;
            }
        }

        //
        // Save MetaData
        //
        NewStatus = AddMetaDataToVolumeContextNew(pVolumeContext, &MetaData);
        if (!NT_SUCCESS(NewStatus)) {
            InVolDbgPrint(DL_ERROR, DM_WRITE | DM_DIRTY_BLOCKS, 
                ("InMageFltWrite: Out of NP Pool to store changes (VolumeContext %#p) ulTotalChangesPending = %d, DirtyBlockInQueue = %d\n", 
                pVolumeContext, pVolumeContext->ChangeList.ulTotalChangesPending, pVolumeContext->ChangeList.lDirtyBlocksInQueue));

            SetDBallocFailureError(pVolumeContext);

            QueueWorkerRoutineForSetVolumeOutOfSync(pVolumeContext, ERROR_TO_REG_OUT_OF_MEMORY_FOR_DIRTY_BLOCKS, STATUS_NO_MEMORY, true);

            // Let us free the commit pending dirty blocks too.
            OrphanMappedDataBlocksAndMoveCommitPendingDirtyBlockToMainQueue(pVolumeContext);

            // as we have lost data because NonPagedPool is full
            // dump all the change data to keep the system from crashing
            FreeChangeNodeList(&pVolumeContext->ChangeList);
        }
    }
    KeReleaseSpinLock(&pVolumeContext->Lock, OldIrql);
    DereferenceVolumeContext(pVolumeContext);
    
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

    return Status;
} // InMageFltIoCompletion


// This function is called with volume context lock held.
NTSTATUS
AddMetaDataToVolumeContextNew(
    PVOLUME_CONTEXT     VolumeContext,
    PWRITE_META_DATA    pWriteMetaData
    )
{
    NTSTATUS        Status = STATUS_SUCCESS;
    PKDIRTY_BLOCK   DirtyBlock = NULL;

    InVolDbgPrint(DL_INFO, DM_DATA_FILTERING, ("AddMetaDataToVC: Offset=%#I64x length=%#x\n",
                                               pWriteMetaData->llOffset, pWriteMetaData->ulLength));

    DirtyBlock = GetCurrentDirtyBlockFromChangeList(VolumeContext);

    /*When a change is tracked in metadata mode. We should not change capture mode to metadata un-neccesarily 
     (for example if there enough free data-blocks and pair has not reached Max. Volume limit). 
     We should just capture the new change in metadata mode, ensure that WOS is non-data, do not change 
     capture mode to metadata, close current dirty block if dirty-block is in data-capture-mode 
	 (compensate correclty for the lost data bytes)*/

    if (ecCaptureModeData == VolumeContext->eCaptureMode) {
        if (VolumeContext->eWOState == ecWriteOrderStateData) {
            VCSetCaptureModeToMetadata(VolumeContext, false);
            VCSetWOState(VolumeContext,false,__FUNCTION__);
        }

        if ((NULL != DirtyBlock) && (INVOLFLT_DATA_SOURCE_DATA == DirtyBlock->ulDataSource)) {
            // Close current dirty block if it is in Data capture mode.
            // Make sure that LostBytes are compensated for correctly.
            AddLastChangeTimestampToCurrentNode(VolumeContext, NULL, true);
        }
    }

    ASSERT(VolumeContext->eWOState != ecWriteOrderStateData);
    if ((NULL != DirtyBlock) && (INVOLFLT_DATA_SOURCE_DATA == DirtyBlock->ulDataSource)) {
        ASSERT(0 != DirtyBlock->TagTimeStampOfLastChange.TimeInHundNanoSecondsFromJan1601);
    }
        
    if (NULL != DirtyBlock) {
        if ((INVOLFLT_DATA_SOURCE_META_DATA != DirtyBlock->ulDataSource) ||
             (VolumeContext->eWOState != DirtyBlock->eWOState) ||
             (0 != DirtyBlock->TagTimeStampOfLastChange.TimeInHundNanoSecondsFromJan1601))//Dirty Block is closed earlier. Use a new one
        {
            // Data source is different need a new dirty block.
            DirtyBlock = NULL;
        }
    }

    if (0 == AddMetaDataToDirtyBlock(VolumeContext, DirtyBlock, pWriteMetaData, INVOLFLT_DATA_SOURCE_META_DATA)) {
        Status = STATUS_NO_MEMORY;
    }

    if ((NULL != VolumeContext->DBNotifyEvent) && (VolumeContext->bNotify)) {
        ASSERT(VolumeContext->ChangeList.ullcbTotalChangesPending >= VolumeContext->ullcbChangesPendingCommit);
        ULONGLONG   ullcbTotalChangesPending = VolumeContext->ChangeList.ullcbTotalChangesPending - VolumeContext->ullcbChangesPendingCommit;
        if (ullcbTotalChangesPending >= VolumeContext->ulcbDataNotifyThreshold) {
            VolumeContext->bNotify = false;
            VolumeContext->liTickCountAtNotifyEventSet = KeQueryPerformanceCounter(NULL);
            KeSetEvent(VolumeContext->DBNotifyEvent, EVENT_INCREMENT, FALSE);
        }
    }

    return Status;
}

// This function is called with volume context lock held.
NTSTATUS
AddMetaDataToVolumeContext(
    PVOLUME_CONTEXT     VolumeContext,
    PWRITE_META_DATA    pWriteMetaData,
    BOOLEAN             bDataMode
    )
{
    NTSTATUS        Status = STATUS_SUCCESS;
    PKDIRTY_BLOCK   DirtyBlock = NULL;
    ULONG           ulRecordSize = SVRecordSizeForIOSize(pWriteMetaData->ulLength, VolumeContext->ulMaxDataSizePerDataModeDirtyBlock);

    InVolDbgPrint(DL_INFO, DM_DATA_FILTERING, ("AddMetaDataToVC: Offset=%#I64x length=%#x, ulRecordSize=%#x\n",
                                               pWriteMetaData->llOffset, pWriteMetaData->ulLength, ulRecordSize));
    if (ecCaptureModeData == VolumeContext->eCaptureMode) {
        ASSERT(TRUE == bDataMode);

        VCSetCaptureModeToMetadata(VolumeContext, false);
        VCSetWOState(VolumeContext,false,__FUNCTION__);

        // Need to add a new dirty block for meta data.
        if (0 == AddMetaDataToDirtyBlock(VolumeContext, NULL, pWriteMetaData, INVOLFLT_DATA_SOURCE_META_DATA)) {
            Status = STATUS_NO_MEMORY;
        }

        if (VolumeContext->ullcbDataAlocMissed) {
            ASSERT(VolumeContext->ullcbDataAlocMissed >= ulRecordSize);
            if (VolumeContext->ullcbDataAlocMissed > ulRecordSize)
                VolumeContext->ullcbDataAlocMissed -= ulRecordSize;
            else
                VolumeContext->ullcbDataAlocMissed = 0;
        }
    } else {
        DirtyBlock = GetCurrentDirtyBlockFromChangeList(VolumeContext);
        if (NULL != DirtyBlock) {
            if ((INVOLFLT_DATA_SOURCE_META_DATA != DirtyBlock->ulDataSource) ||
                 (VolumeContext->eWOState != DirtyBlock->eWOState) ||
                 (0 != DirtyBlock->TagTimeStampOfLastChange.TimeInHundNanoSecondsFromJan1601))//Dirty Block is closed earlier. Use a new one
            {
                // Data source is different need a new dirty block.
                DirtyBlock = NULL;
            }
        }

        if (0 == AddMetaDataToDirtyBlock(VolumeContext, DirtyBlock, pWriteMetaData, INVOLFLT_DATA_SOURCE_META_DATA)) {
            Status = STATUS_NO_MEMORY;
        }

        if (TRUE == bDataMode) {
            if (VolumeContext->ullcbDataAlocMissed) {
                ASSERT(VolumeContext->ullcbDataAlocMissed >= ulRecordSize);
                if (VolumeContext->ullcbDataAlocMissed >= ulRecordSize)
                    VolumeContext->ullcbDataAlocMissed -= ulRecordSize;
                else
                    VolumeContext->ullcbDataAlocMissed = 0;
            }
        }
    }

    if ((NULL != VolumeContext->DBNotifyEvent) && (VolumeContext->bNotify)) {
        ASSERT(VolumeContext->ChangeList.ullcbTotalChangesPending >= VolumeContext->ullcbChangesPendingCommit);
        ULONGLONG   ullcbTotalChangesPending = VolumeContext->ChangeList.ullcbTotalChangesPending - VolumeContext->ullcbChangesPendingCommit;
        if (ullcbTotalChangesPending >= VolumeContext->ulcbDataNotifyThreshold) {
            VolumeContext->bNotify = false;
            VolumeContext->liTickCountAtNotifyEventSet = KeQueryPerformanceCounter(NULL);
            KeSetEvent(VolumeContext->DBNotifyEvent, EVENT_INCREMENT, FALSE);
        }
    }

    return Status;
}

void
WriteDispatchInMetaDataMode(
    PDEVICE_OBJECT  DeviceObject,
    PIRP            Irp
    )
{
    ASSERT((NULL != DeviceObject) && (NULL != Irp));

    NTSTATUS            Status = STATUS_SUCCESS;
    PDEVICE_EXTENSION   DeviceExtension = (PDEVICE_EXTENSION)DeviceObject->DeviceExtension;
    PVOLUME_CONTEXT     VolumeContext = DeviceExtension->pVolumeContext;
    PIO_STACK_LOCATION  IoStackLocation = IoGetCurrentIrpStackLocation(Irp);
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
        (VolumeContext->ChangeList.ulTotalChangesPending >= DriverContext.DBHighWaterMarks[DriverContext.eServiceState])) 
    {
        bWakeServiceThread = TRUE;
    }
    
    if (!VolumeContext->IsVolumeUsingAddDevice && DriverContext.IsDispatchEntryPatched) {
        COMPLETION_CONTEXT  *pCompRoutine = (PCOMPLETION_CONTEXT)ExAllocatePoolWithTag(NonPagedPool,
                                                                                       sizeof(COMPLETION_CONTEXT),
                                                                                       TAG_GENERIC_NON_PAGED);
        if (pCompRoutine) {
            RtlZeroMemory(pCompRoutine, sizeof(COMPLETION_CONTEXT));
            ReferenceVolumeContext(VolumeContext);
            pCompRoutine->eContextType = ecCompletionContext;
            pCompRoutine->VolumeContext = VolumeContext;
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
            QueueWorkerRoutineForSetVolumeOutOfSync(VolumeContext, INVOLFLT_ERR_NO_GENERIC_NPAGED_POOL, STATUS_NO_MEMORY, false);   
        }
    } else {
        //
        // Copy current stack to next stack.
        //
        IoCopyCurrentIrpStackLocationToNext(Irp);
        ReferenceVolumeContext(VolumeContext);
        //
        // Set completion routine callback.
        //
        IoSetCompletionRoutine(Irp,
                        InMageFltIoCompletion,
                        VolumeContext,
                        TRUE,
                        TRUE,
                        TRUE);
    }

    if (bWakeServiceThread) {
        InVolDbgPrint(DL_INFO, DM_VOLUME_STATE, 
            ("InMageFltWrite: Waking service thread, ServiceState = %#x\n", DriverContext.eServiceState));
        KeSetEvent(&DriverContext.ServiceThreadWakeEvent, IO_DISK_INCREMENT, FALSE);
    }

    return;
}
