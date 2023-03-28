#include "Common.h"
#include "TagNode.h"
#include "InmFltInterface.h"

PTAG_NODE
AllocateTagNode(PTAG_INFO TagInfo, PIRP Irp)
{
    PTAG_NODE   TagNode = NULL; 
    ULONG       ulSize;
    ULONG       ulNumberOfVolumes = TagInfo->ulNumberOfDevices;

    if (0 == ulNumberOfVolumes) {
        return NULL;
    }

    if (1 == ulNumberOfVolumes) {
        ulSize = sizeof(TAG_NODE);
    } else {
        ulSize = ((ulNumberOfVolumes - 1)*sizeof(etTagStatus)) + sizeof(TAG_NODE);
    }

    TagNode = (PTAG_NODE)ExAllocatePoolWithTag(NonPagedPool, ulSize, TAG_TAG_NODE); 
    if (NULL == TagNode) {
        return NULL;
    } else {
        RtlZeroMemory(TagNode, ulSize);
    }

    TagNode->ulNoOfVolumes = ulNumberOfVolumes;
    TagNode->Irp = Irp;
    TagNode->TagStatus = ecTagStatusPending;
    RtlCopyMemory(&TagNode->TagGUID, &TagInfo->TagGUID, sizeof(GUID));
    for (ULONG i = 0; i < ulNumberOfVolumes; i++) {
        switch (TagInfo->TagStatus[i].Status) {
        case STATUS_NOT_FOUND:
            TagNode->VolumeStatus[i] = ecTagStatusInvalidGUID;
            TagNode->ulNoOfVolumesIgnored++;
            break;
        case STATUS_INVALID_DEVICE_STATE:
            TagNode->VolumeStatus[i] = ecTagStatusFilteringStopped;
            TagNode->ulNoOfVolumesIgnored++;
            break;
        default:
            TagNode->VolumeStatus[i] = ecTagStatusPending;
            break;
        }
    }

    ASSERT(TagNode->ulNoOfVolumesIgnored != TagNode->ulNoOfVolumes);
    return TagNode;
}

void
DeallocateTagNode(PTAG_NODE TagNode)
{
    if (NULL != TagNode) {
        ExFreePoolWithTag(TagNode, TAG_TAG_NODE);
    }

    return;
}

PTAG_NODE
RemoveTagNodeFromList(GUID &TagGUID)
{
    PTAG_NODE   TagNode = NULL;
    KIRQL       OldIrql;

    KeAcquireSpinLock(&DriverContext.TagListLock, &OldIrql);
    PLIST_ENTRY Entry = DriverContext.HeadForTagNodes.Flink;
    while (Entry != &DriverContext.HeadForTagNodes) {
        PTAG_NODE   Temp = (PTAG_NODE)Entry;

        if (sizeof(GUID) == RtlCompareMemory(&TagGUID, &Temp->TagGUID, sizeof(GUID))) {
            RemoveEntryList(Entry);
            TagNode = Temp;
            break;
        }

        Entry = Entry->Flink;
    }
    KeReleaseSpinLock(&DriverContext.TagListLock, OldIrql);

    return TagNode;
}

PTAG_NODE
RemoveTagNodeFromList(PIRP Irp)
{
    PTAG_NODE   TagNode = NULL;
    KIRQL       OldIrql;
    
    KeAcquireSpinLock(&DriverContext.TagListLock, &OldIrql);
    PLIST_ENTRY Entry = DriverContext.HeadForTagNodes.Flink;
    while (Entry != &DriverContext.HeadForTagNodes) {
        PTAG_NODE   Temp = (PTAG_NODE)Entry;

        if (Irp == Temp->Irp) {
            RemoveEntryList(Entry);
            TagNode = Temp;
            break;
        }

        Entry = Entry->Flink;
    }
    KeReleaseSpinLock(&DriverContext.TagListLock, OldIrql);

    return TagNode;
}

VOID
InMageFltCancelTagVolumeIrp (
    IN PDEVICE_OBJECT   DeviceObject,
    IN PIRP             Irp
    )
{
    PTAG_NODE   TagNode = NULL;
	UNREFERENCED_PARAMETER(DeviceObject);
    
    IoReleaseCancelSpinLock(Irp->CancelIrql);

    TagNode = RemoveTagNodeFromList(Irp);
    if (NULL != TagNode) {
        ASSERT(Irp == TagNode->Irp);

        DeallocateTagNode(TagNode);
        Irp->IoStatus.Status = STATUS_CANCELLED;
        Irp->IoStatus.Information = 0;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
    }

    return;
}

NTSTATUS
AddTagNodeToList(PTAG_INFO TagInfo, PIRP Irp, etTagState eTagState)
{
    ASSERT(NULL != Irp);

    KIRQL       OldIrql;
    NTSTATUS    Status = STATUS_SUCCESS;
                    
    PTAG_NODE   TagNode = AllocateTagNode(TagInfo, Irp);                
    if (NULL == TagNode) {
        return STATUS_NO_MEMORY;
    }

    ASSERT(NULL == TagNode->pTagInfo);
    if (ecTagStateWaitForCommitSnapshot == eTagState) {
        UpdateLockedDataBlockList(DriverContext.ulMaxLockedDataBlocks + TagInfo->ulNumberOfDevices);
        TagNode->pTagInfo = TagInfo;
    }

    KeAcquireSpinLock(&DriverContext.TagListLock, &OldIrql);    

    IoSetCancelRoutine(Irp, InMageFltCancelTagVolumeIrp );
    if (Irp->Cancel && (NULL != IoSetCancelRoutine(Irp, NULL))){
        // Irp Already cancelled.
        Status = STATUS_CANCELLED;   
    } else {
        IoMarkIrpPending(Irp);
        InsertTailList(&DriverContext.HeadForTagNodes, &TagNode->ListEntry);
		DriverContext.ulNumberofAsyncTagIOCTLs++;
    }

    KeReleaseSpinLock(&DriverContext.TagListLock, OldIrql);

    if (STATUS_SUCCESS != Status) {
        ASSERT(STATUS_CANCELLED == Status);
        DeallocateTagNode(TagNode);
    }

    return Status;
}

PTAG_NODE
AddStatusAndReturnNodeIfComplete(GUID &TagGUID, ULONG ulIndex, etTagStatus eTagStatus)
{
    PTAG_NODE   TagNode = NULL;
    KIRQL       OldIrql;

    KeAcquireSpinLock(&DriverContext.TagListLock, &OldIrql);
    PLIST_ENTRY Entry = DriverContext.HeadForTagNodes.Flink;
    while (Entry != &DriverContext.HeadForTagNodes) {
        PTAG_NODE   Temp = (PTAG_NODE)Entry;
        if (Temp->pTagInfo) {
            // This tag is not applied yet. Waiting for commit snapshot
            Entry = Entry->Flink;
            continue;
        }

        if (sizeof(GUID) == RtlCompareMemory(&TagGUID, &Temp->TagGUID, sizeof(GUID))) {
            ASSERT(ulIndex < Temp->ulNoOfVolumes);
            ASSERT(Temp->VolumeStatus[ulIndex] == ecTagStatusPending);
            Temp->VolumeStatus[ulIndex] = eTagStatus;
            if (ecTagStatusCommited == eTagStatus) {
                Temp->ulNoOfTagsCommited++;
                if ((Temp->ulNoOfTagsCommited + Temp->ulNoOfVolumesIgnored) == Temp->ulNoOfVolumes) {
                    TagNode = Temp;
                }
            } else {
                TagNode = Temp;
            }
            break;
        }

        Entry = Entry->Flink;
    }

    if (NULL != TagNode) {
        TagNode->TagStatus = eTagStatus;
        RemoveEntryList(&TagNode->ListEntry);
        IoSetCancelRoutine(TagNode->Irp, NULL);
    }

    KeReleaseSpinLock(&DriverContext.TagListLock, OldIrql);

    return TagNode;
}

PTAG_NODE
AddStatusAndReturnNodeIfComplete(PTAG_INFO TagInfo)
{
    ASSERT(TagInfo->ulFlags & TAG_INFO_FLAGS_WAIT_FOR_COMMIT);

    PTAG_NODE   TagNode = NULL, Temp = NULL;
    KIRQL       OldIrql;

    KeAcquireSpinLock(&DriverContext.TagListLock, &OldIrql);

    PLIST_ENTRY Entry = DriverContext.HeadForTagNodes.Flink;
    while (Entry != &DriverContext.HeadForTagNodes) {
        Temp = (PTAG_NODE)Entry;
        if (Temp->pTagInfo) {
            // This tag is not applied yet. Waiting for commit snapshot
            Entry = Entry->Flink;
            continue;
        }
        if (sizeof(GUID) == RtlCompareMemory(&TagInfo->TagGUID, &Temp->TagGUID, sizeof(GUID))) {
            TagNode = Temp;            
            break;
        }
        Entry = Entry->Flink;
    }

    // If TagNode with matching GUID is found, assign it to temp.
    Temp = TagNode;
    TagNode = NULL;
    if (NULL != Temp) {
        for (ULONG i = 0; i < TagInfo->ulNumberOfDevices; i++) {
            if (STATUS_PENDING == TagInfo->TagStatus[i].Status) {
                Temp->VolumeStatus[i] = ecTagStatusUnattempted;
                // Do not set Temp->TagStatus. This status is STATUS_PENDING as
                // error happened while applying tag for another volume, and 
                // tag processing for this volume is aborted.
                TagNode = Temp;
            } else if (STATUS_INVALID_DEVICE_STATE == TagInfo->TagStatus[i].Status) {
                // Filtering is stoped for this volume.
                Temp->ulNoOfVolumesIgnored++;
                Temp->VolumeStatus[i] = ecTagStatusFilteringStopped;
                if (0 == (TagInfo->ulFlags & TAG_INFO_FLAGS_IGNORE_IF_FILTERING_STOPPED)) {
                    Temp->TagStatus = ecTagStatusFilteringStopped;
                    TagNode = Temp;
                } else {
                    if ((Temp->ulNoOfTagsCommited + Temp->ulNoOfVolumesIgnored) == Temp->ulNoOfVolumes) {
                        if (ecTagStatusPending == Temp->TagStatus) {
                            if (Temp->ulNoOfVolumesIgnored == Temp->ulNoOfVolumes) {
                                Temp->TagStatus = ecTagStatusFilteringStopped;
                            } else {
                                Temp->TagStatus = ecTagStatusCommited;
                            }
                        }
                        TagNode = Temp;
                    }
                }
            } else if ((STATUS_SUCCESS != TagInfo->TagStatus[i].Status) &&
                       (STATUS_NOT_FOUND != TagInfo->TagStatus[i].Status))
            {
                Temp->VolumeStatus[i] = ecTagStatusFailure;
                Temp->TagStatus = ecTagStatusFailure;
                TagNode = Temp;
            }
        }
    }

    if (NULL != TagNode) {
        ASSERT(ecTagStatusPending != TagNode->TagStatus);
        if (TagNode->TagStatus == ecTagStatusPending) {
            TagNode->TagStatus = ecTagStatusUnattempted;
        }
        RemoveEntryList(&TagNode->ListEntry);
        IoSetCancelRoutine(TagNode->Irp, NULL);
    }

    KeReleaseSpinLock(&DriverContext.TagListLock, OldIrql);

    return TagNode;
}

void
CopyStatusToOutputBuffer(PTAG_NODE TagNode, PTAG_VOLUME_STATUS_OUTPUT_DATA OutputData)
{
    OutputData->ulNoOfVolumes = TagNode->ulNoOfVolumes;
    OutputData->Status = TagNode->TagStatus;
    for (ULONG i = 0; i < TagNode->ulNoOfVolumes; i++) {
        OutputData->VolumeStatus[i] = TagNode->VolumeStatus[i];
    }

    return;
}

NTSTATUS
CopyStatusToOutputBuffer(GUID &TagGUID, PTAG_VOLUME_STATUS_OUTPUT_DATA OutputData, ULONG ulOutputDataSize)
{
    PTAG_NODE   TagNode = NULL;
    KIRQL       OldIrql;
    NTSTATUS    Status = STATUS_SUCCESS;

    KeAcquireSpinLock(&DriverContext.TagListLock, &OldIrql);
    PLIST_ENTRY Entry = DriverContext.HeadForTagNodes.Flink;
    while (Entry != &DriverContext.HeadForTagNodes) {
        PTAG_NODE   Temp = (PTAG_NODE)Entry;

        if (sizeof(GUID) == RtlCompareMemory(&TagGUID, &Temp->TagGUID, sizeof(GUID))) {
            TagNode = Temp;
            break;
        }

        Entry = Entry->Flink;
    }

    if (NULL == TagNode) {
        Status = STATUS_INVALID_PARAMETER;
    } else {
        if (ulOutputDataSize < SIZE_OF_TAG_VOLUME_STATUS_OUTPUT_DATA(TagNode->ulNoOfVolumes)) {
            Status = STATUS_BUFFER_TOO_SMALL;
        } else {
            CopyStatusToOutputBuffer(TagNode, OutputData);
        }
    }

    KeReleaseSpinLock(&DriverContext.TagListLock, OldIrql);

    return Status;
}


