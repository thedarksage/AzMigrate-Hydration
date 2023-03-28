#include "CDskCommon.h"
#include "CDskNode.h"

PCLUSDISK_NODE
GetClusDiskNodeFromSignature(ULONG ulSignature)
{
    PCLUSDISK_NODE  pClusDiskNode = NULL;
    BOOLEAN         bFound;

    ExAcquireFastMutex(&DriverContext.ClusDiskListLock);

    bFound = FALSE;
    pClusDiskNode = (PCLUSDISK_NODE)DriverContext.ClusDiskListHead.Flink;
    while ((PLIST_ENTRY)pClusDiskNode != &DriverContext.ClusDiskListHead) {
#if (NTDDI_VERSION >= NTDDI_VISTA)
        if (ecPartitionStyleMBR == pClusDiskNode->ePartitionStyle) {
#endif
            if (pClusDiskNode->ulDiskSignature == ulSignature) {
                bFound = TRUE;
                break;
            }
#if (NTDDI_VERSION >= NTDDI_VISTA)
        }
#endif
        pClusDiskNode = (PCLUSDISK_NODE)pClusDiskNode->ListEntry.Flink;
    }
    if (TRUE == bFound)
        ReferenceClusDiskNode(pClusDiskNode);
    else
        pClusDiskNode = NULL;

    ExReleaseFastMutex(&DriverContext.ClusDiskListLock);

    return pClusDiskNode;
}

#if (NTDDI_VERSION >= NTDDI_VISTA)
NTSTATUS
AddDiskGUIDToClusDiskList(GUID DiskGuid, PVOID Context)
{
    PCLUSDISK_NODE  pClusDiskNode;

    pClusDiskNode = GetClusDiskNodeFromDiskGuid(DiskGuid);
    if (NULL != pClusDiskNode)
    {
        ASSERT(Context == pClusDiskNode->Context);
        // If context did not match, may be we have to update the context??
        DereferenceClusDiskNode(pClusDiskNode);
        return STATUS_DUPLICATE_OBJECTID;
    }

    pClusDiskNode = AllocateClusDiskNode();
    if (NULL == pClusDiskNode)
        return STATUS_INSUFFICIENT_RESOURCES;

    pClusDiskNode->ePartitionStyle = ecPartitionStyleGPT;
    RtlCopyMemory(&pClusDiskNode->DiskGuid, &DiskGuid, sizeof(GUID));
    pClusDiskNode->Context = Context;

    ExAcquireFastMutex(&DriverContext.ClusDiskListLock);
    InsertHeadList(&DriverContext.ClusDiskListHead, &pClusDiskNode->ListEntry);
    ExReleaseFastMutex(&DriverContext.ClusDiskListLock);

    return STATUS_SUCCESS;
}
PCLUSDISK_NODE
GetClusDiskNodeFromDiskGuid(GUID DiskGuid)
{
    PCLUSDISK_NODE  pClusDiskNode = NULL;
    BOOLEAN         bFound;

    ExAcquireFastMutex(&DriverContext.ClusDiskListLock);

    bFound = FALSE;
    pClusDiskNode = (PCLUSDISK_NODE)DriverContext.ClusDiskListHead.Flink;
    while ((PLIST_ENTRY)pClusDiskNode != &DriverContext.ClusDiskListHead) {
        if (ecPartitionStyleGPT == pClusDiskNode->ePartitionStyle) {
            if (sizeof(GUID) == RtlCompareMemory(&pClusDiskNode->DiskGuid, &DiskGuid, sizeof(GUID))) {
                bFound = TRUE;
                break;
            }
        }

        pClusDiskNode = (PCLUSDISK_NODE)pClusDiskNode->ListEntry.Flink;
    }
    if (TRUE == bFound)
        ReferenceClusDiskNode(pClusDiskNode);
    else
        pClusDiskNode = NULL;

    ExReleaseFastMutex(&DriverContext.ClusDiskListLock);

    return pClusDiskNode;
}
NTSTATUS
RemoveClusGPTDiskFromClusDiskList(GUID DiskGuid)
{
    PCLUSDISK_NODE  pClusDiskNode;

    pClusDiskNode = GetClusDiskNodeFromDiskGuid(DiskGuid);
    if (NULL == pClusDiskNode)
        return STATUS_INVALID_PARAMETER;

    ASSERT(pClusDiskNode == pClusDiskNode);
#if (NTDDI_VERSION >= NTDDI_VISTA)
    ASSERT(pClusDiskNode->ePartitionStyle == ecPartitionStyleGPT);
#endif
    ExAcquireFastMutex(&DriverContext.ClusDiskListLock);
    RemoveEntryList(&pClusDiskNode->ListEntry);
    ExReleaseFastMutex(&DriverContext.ClusDiskListLock);

    // Dereference Once to remove from list, and the other for GetClusDiskNodeFromDiskGuid.
    DereferenceClusDiskNode(pClusDiskNode);
    DereferenceClusDiskNode(pClusDiskNode);
    return STATUS_SUCCESS;
}
#endif
PCLUSDISK_NODE
GetClusDiskNodeFromContext(PVOID Context)
{
    PCLUSDISK_NODE  pClusDiskNode = NULL;
    BOOLEAN         bFound;

    ExAcquireFastMutex(&DriverContext.ClusDiskListLock);

    bFound = FALSE;
    pClusDiskNode = (PCLUSDISK_NODE)DriverContext.ClusDiskListHead.Flink;
    while ((PLIST_ENTRY)pClusDiskNode != &DriverContext.ClusDiskListHead) {
        if (pClusDiskNode->Context == Context) {
            bFound = TRUE;
            break;
        }

        pClusDiskNode = (PCLUSDISK_NODE)pClusDiskNode->ListEntry.Flink;
    }
    if (TRUE == bFound)
        ReferenceClusDiskNode(pClusDiskNode);
    else
        pClusDiskNode = NULL;

    ExReleaseFastMutex(&DriverContext.ClusDiskListLock);

    return pClusDiskNode;
}

NTSTATUS
AddSignatureToClusDiskList(ULONG ulSignature, PVOID Context)
{
    PCLUSDISK_NODE  pClusDiskNode;

    pClusDiskNode = GetClusDiskNodeFromSignature(ulSignature);
    if (NULL != pClusDiskNode)
    {
        ASSERT(Context == pClusDiskNode->Context);
        // If context did not match, may be we have to update the context??
        DereferenceClusDiskNode(pClusDiskNode);
        return STATUS_DUPLICATE_OBJECTID;
    }

    pClusDiskNode = AllocateClusDiskNode();
    if (NULL == pClusDiskNode)
        return STATUS_INSUFFICIENT_RESOURCES;
#if (NTDDI_VERSION >= NTDDI_VISTA)
    pClusDiskNode->ePartitionStyle = ecPartitionStyleMBR;
#else // For win2k3
    pClusDiskNode->State = DISK_ACQUIRED;
#endif
    pClusDiskNode->ulDiskSignature = ulSignature;
    pClusDiskNode->Context = Context;

    ExAcquireFastMutex(&DriverContext.ClusDiskListLock);
    InsertHeadList(&DriverContext.ClusDiskListHead, &pClusDiskNode->ListEntry);
    ExReleaseFastMutex(&DriverContext.ClusDiskListLock);

    return STATUS_SUCCESS;
}

NTSTATUS
RemoveClusDiskFromClusDiskList(ULONG ulSignature)
{
    PCLUSDISK_NODE  pClusDiskNode;

    pClusDiskNode = GetClusDiskNodeFromSignature(ulSignature);
    if (NULL == pClusDiskNode)
        return STATUS_INVALID_PARAMETER;

    ASSERT(pClusDiskNode == pClusDiskNode);
#if (NTDDI_VERSION >= NTDDI_VISTA)
    ASSERT(pClusDiskNode->ePartitionStyle == ecPartitionStyleMBR);
#endif

    ExAcquireFastMutex(&DriverContext.ClusDiskListLock);
    RemoveEntryList(&pClusDiskNode->ListEntry);
    ExReleaseFastMutex(&DriverContext.ClusDiskListLock);

    // Dereference Once to remove from list, and the other for GetClusDiskNodeFromSignature.
    DereferenceClusDiskNode(pClusDiskNode);
    DereferenceClusDiskNode(pClusDiskNode);
    return STATUS_SUCCESS;
}

/*-----------------------------------------------------------------------------
 * CLUSDISK_NODE allocation/ref/deref functions
 *-----------------------------------------------------------------------------
 */

VOID
DeallocateClusDiskNode(PCLUSDISK_NODE pClusDiskNode)
{
    ASSERT(0 == pClusDiskNode->lRefCount);

    ExFreeToPagedLookasideList(&DriverContext.ClusDiskNodePool,
                    pClusDiskNode);

    return;
}

PCLUSDISK_NODE
AllocateClusDiskNode()
{
    PCLUSDISK_NODE  pClusDiskNode = NULL;

    pClusDiskNode = (PCLUSDISK_NODE)ExAllocateFromPagedLookasideList(
                        &DriverContext.ClusDiskNodePool);

    if (NULL != pClusDiskNode) {
        RtlZeroMemory(pClusDiskNode, sizeof(CLUSDISK_NODE));
        pClusDiskNode->lRefCount = 1;
    }

    return pClusDiskNode;
}

PCLUSDISK_NODE
ReferenceClusDiskNode(PCLUSDISK_NODE pClusDiskNode)
{
    ASSERT(1 <= pClusDiskNode->lRefCount);

    InterlockedIncrement(&pClusDiskNode->lRefCount);

    return pClusDiskNode;
}

LONG
DereferenceClusDiskNode(PCLUSDISK_NODE pClusDiskNode)
{
    LONG    lRefCount;
    ASSERT(1 <= pClusDiskNode->lRefCount);

    lRefCount = InterlockedDecrement(&pClusDiskNode->lRefCount);
    if (0 >= lRefCount) {
        DeallocateClusDiskNode(pClusDiskNode);
    }

    return lRefCount;
}
