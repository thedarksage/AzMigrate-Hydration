// generic code for classes

//
// Copyright InMage Systems 2004
//

#include "global.h"

PVOID __cdecl malloc(ULONG32 x,  ULONG32 id, POOL_TYPE pool)
{
    ASSERT((NonPagedPool == pool) || (PagedPool == pool));
    PVOID buffer = ExAllocatePoolWithTag(pool, x, id);

    if (buffer) {
        //
        // clear our buffer to zero
        //
        RtlZeroMemory(buffer, x);
    }

    return buffer;
}


void __cdecl free(PVOID x)
{
    if (x != NULL) { // the c++ spec requires freeing a null pointer
        ExFreePool(x);
    }
}


void * __cdecl operator new(size_t size)
{
    return (PVOID) malloc((ULONG32)size, (ULONG32)TAG_GENERIC_NON_PAGED, NonPagedPool);
}

void * __cdecl operator new[](size_t size)
{
    return (PVOID)malloc((ULONG32)size, (ULONG32)TAG_GENERIC_NON_PAGED, NonPagedPool);
}

void * __cdecl operator new(size_t size,  void *location)
{
	UNREFERENCED_PARAMETER(size);
    return location;
}

void *__cdecl operator new( size_t size, ULONG32 tag, POOL_TYPE pool)
{
    return (PVOID)malloc((ULONG32)size, (ULONG32)tag, pool);
}

void __cdecl operator delete(void * pVoid)
{
    free(pVoid);
}

// For Visual Studio 2017 compatibility
#ifdef _WIN64
void __cdecl operator delete(void * pVoid, unsigned __int64 uiBlockSize)
#else
void __cdecl operator delete(void * pVoid, unsigned int uiBlockSize)
#endif
{
    UNREFERENCED_PARAMETER(uiBlockSize);
    free(pVoid);
}

