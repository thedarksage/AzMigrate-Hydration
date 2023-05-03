#define MAX_DISK_NAME 50

LPVOID
PmHeapAlloc(
__in HANDLE hHeap,
__in DWORD  dwFlags,
__in SIZE_T dwBytes
);

BOOL
PmHeapFree(
__in HANDLE hHeap,
__in DWORD  dwFlags,
__in LPVOID lpMem
);

HRESULT
GetBootDiskNumber(
_Out_ ULONG *pcDisk,
_Outptr_result_buffer_(*pcDisk) ULONG **ppDisks
);



