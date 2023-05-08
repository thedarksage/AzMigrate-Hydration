typedef struct __WRITE_THREAD_PARAMS_R {
	WCHAR	  strVolumeSymbolicLinkName[100];
	ULONGLONG ullTotalData;
	ULONG	  ulNMaxIOs;
	ULONG	  ulMinIOSz; // no. of blocks
	ULONG	  ulMaxIOSz; // no. of blocks
} WRITE_THREAD_PARAMS_R, *PWRITE_THREAD_PARAMS_R;

typedef struct __READ_THREAD_PARAMS_R {
	HANDLE hDriver;
	WCHAR  volGUID[GUID_SIZE_IN_CHARS+1];
} READ_THREAD_PARAMS_R, *PREAD_THREAD_PARAMS_R;