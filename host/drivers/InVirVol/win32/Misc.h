#ifndef __MISC_H__
#define __MISC_H__

#define INMAGE_MEMORY_TAG ' VNI' 

__inline
void 
InitializeEntryList(PSINGLE_LIST_ENTRY ListHead)
{
    ListHead->Next = NULL;
}

__inline
bool
IsSingleListEmpty(PSINGLE_LIST_ENTRY ListHead)
{
	return (ListHead->Next==NULL);
}

__inline
PSINGLE_LIST_ENTRY 
InPopEntryList(PSINGLE_LIST_ENTRY ListHead)
{
    PSINGLE_LIST_ENTRY FirstEntry = NULL;
    
    FirstEntry = ListHead->Next;
    if (FirstEntry != NULL) {     
        ListHead->Next = FirstEntry->Next;
    }
    
    return FirstEntry;
}

#ifdef INMAGE_USER_MODE

__inline
void 
PushEntryList(PSINGLE_LIST_ENTRY ListHead, PSINGLE_LIST_ENTRY Entry)
{
    Entry->Next = ListHead->Next;
    ListHead->Next = Entry;
}

//Memory Allocation Functions

__inline
PVOID 
InAllocateMemory(size_t size, DWORD PoolType = 0)
{
	return malloc(size);
}

__inline
VOID 
InFreeMemory(PVOID Buffer)
{
	return free(Buffer);
}

#else

__inline
PVOID 
InAllocateMemory(size_t size, POOL_TYPE PoolType = PagedPool)
{
	return ExAllocatePoolWithTag(PoolType, size, INMAGE_MEMORY_TAG);
}

__inline
VOID 
InFreeMemory(PVOID Buffer)
{
    ASSERT(Buffer);
	return ExFreePool(Buffer);
    Buffer = NULL;
}

#endif
#endif
