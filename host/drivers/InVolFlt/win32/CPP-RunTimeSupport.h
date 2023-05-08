


// these are the kernel heap managers that are not class specific
PVOID __cdecl malloc(ULONG32 x,  ULONG32 id, POOL_TYPE pool);
void __cdecl free(PVOID x);
