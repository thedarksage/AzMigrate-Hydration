#pragma once
typedef struct _LIST_NODE
{
    LIST_ENTRY  ListEntry;
    LONG        lRefCount;
    PVOID       pData;
} LIST_NODE, *PLIST_NODE;

PLIST_NODE
AllocateListNode();

LONG
DereferenceListNode(PLIST_NODE  pListNode);

