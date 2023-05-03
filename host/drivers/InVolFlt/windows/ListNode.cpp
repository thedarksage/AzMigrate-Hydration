#include "Common.h"
#include "ListNode.h"

PLIST_NODE
AllocateListNode()
{
    PLIST_NODE      pListNode;

    pListNode = (PLIST_NODE)ExAllocateFromNPagedLookasideList(&DriverContext.ListNodePool);
    if (pListNode)
    {
        RtlZeroMemory(pListNode, sizeof(LIST_NODE));
        pListNode->lRefCount = 1;
    }

    return pListNode;
}

VOID
DeallocateListNode(PLIST_NODE  pListNode)
{
    ASSERT(pListNode->lRefCount <= 0x00);
    //Cleanup
    InitializeListHead(&pListNode->ListEntry);
    ExFreeToNPagedLookasideList(&DriverContext.ListNodePool, pListNode);

    return;
}

LONG
DereferenceListNode( PLIST_NODE pListNode )
{
    LONG    lRefCount;

    ASSERT(pListNode->lRefCount >= 0x01);

    lRefCount = InterlockedDecrement(&pListNode->lRefCount);
    if (lRefCount <= 0x00)
    {
        DeallocateListNode(pListNode);
    }

    return lRefCount;
}
