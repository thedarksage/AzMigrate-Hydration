//
// Copyright (c) 2005 InMage
// This file contains proprietary and confidential information and
// remains the unpublished property of InMage. Use, disclosure,
// or reproduction is prohibited except as permitted by express
// written license aggreement with InMage.
//
// File       : WinVsnapKernelHelpers.h
//
// Description: Windows kernel helper routines.
//

#ifndef _WIN_VSNAP_KERNEL_HELPERS_H
#define _WIN_VSNAP_KERNEL_HELPERS_H

#ifdef VSNAP_WIN_KERNEL_MODE

#include "VVCommon.h"

#define INSTATUS			NTSTATUS
#define IN_SUCCESS(Status)	(NT_SUCCESS(Status))
#define INMAGE_STRING_SIZE	256
#define DWORD				ULONG
#define LPOVERLAPPED		PVOID

#define INMAGE_EVENTLOG_MAX_FILE_LENGTH  50

#define FILE_BEGIN          0
#define FILE_CURRENT        1
#define FILE_END            2

#define O_RDONLY       0x0000  /* open for reading only */
#define O_WRONLY       0x0001  /* open for writing only */
#define O_RDWR         0x0002  /* open for reading and writing */
#define O_APPEND       0x0008  /* writes done at eof */

#define O_CREAT        0x0100  /* create and open file */
#define O_TRUNC        0x0200  /* open and truncate */
#define O_EXCL         0x0400  /* open only if file doesn't already exist */

#define O_BUFFER		0x800 /* Used internally */
#define CREATE_NEW          1
#define CREATE_ALWAYS       2
#define OPEN_EXISTING       3
#define OPEN_ALWAYS         4
#define TRUNCATE_EXISTING   5

#define FILE_DEFAULT_CREATE_OPTIONS     FILE_SYNCHRONOUS_IO_NONALERT | FILE_WRITE_THROUGH | FILE_NON_DIRECTORY_FILE
//#define FILE_DEFAULT_CREATE_OPTIONS     FILE_ATTRIBUTE_NORMAL



typedef struct RAW_READ_HANDLE
{
    PDEVICE_OBJECT  DeviceObject;
    PFILE_OBJECT    FileObject;
}_RAW_READ_HANDLE, *PRAW_READ_HANDLE;

#ifdef UNICODE
#define InCreateFile InCreateFileW
#else
#define InCreateFile InCreateFileA
#endif

struct WinHdlTag
{
	HANDLE			Handle;
	PDEVICE_OBJECT	osDevice;
    PFILE_OBJECT	fileObject;
	bool			ShouldBuffer;
};

typedef struct WinHdlTag WinHdl;

INSTATUS InCreateFileA(
  CHAR* FileNameA,
  ACCESS_MASK DesiredAccess,
  DWORD ShareMode,
  DWORD CreationDisposition,
  DWORD FlagsAndAttributes,
  HANDLE hTemplateFile,
  IN PLARGE_INTEGER AllocationSize OPTIONAL,
  WinHdl *Handle,
  bool EventLog = false
);

 INSTATUS InCreateFileW(
  WCHAR* pFileName,
  ACCESS_MASK DesiredAccess,
  DWORD ShareMode,
  DWORD CreationDisposition,
  DWORD FlagsAndAttributes,
  HANDLE hTemplateFile,
  IN PLARGE_INTEGER  AllocationSize  OPTIONAL,
  OUT PHANDLE  FileHandle
);

static INSTATUS InReadFile(
  WinHdl *FileHandle,
  VOID* ReadBuffer,
  DWORD NumberOfBytesToRead,
  PLARGE_INTEGER ByteOffset,
  ULONG* NumberOfBytesRead,
  LPOVERLAPPED lpOverlapped
);

static INSTATUS InWriteFile(
  WinHdl *FileHandle,
  PVOID  WriteBuffer,
  DWORD NumberOfBytesToWrite,
  PLARGE_INTEGER ByteOffset,
  ULONG *NumberOfBytesWritten,
  LPOVERLAPPED lpOverlapped
);

INSTATUS QueryFileSize(void *FileHandle,
  PLARGE_INTEGER FileSize);

static INSTATUS InSetFilePointer(
  WinHdl *FileHandle,
  LARGE_INTEGER DistanceToMove,
  LARGE_INTEGER *NewDistance,
  DWORD dwMoveMethod
);
static INSTATUS InSetFilePointer(
  WinHdl *FileHandle,
  PLARGE_INTEGER FileSize);



INSTATUS InCloseHandle(
  WinHdl *FileHandle
);

static INSTATUS InDeleteFileA(PCHAR FileName);

static INSTATUS InDeleteFileW(PWCHAR FileNameW);

static NTSTATUS InCreateFileRaw(CHAR* FileName, PRAW_READ_HANDLE RawReadHandle);
static NTSTATUS InReadFileRaw(PRAW_READ_HANDLE RawReadHandle,LARGE_INTEGER ByteOffset, ULONG ReadLength, PVOID ReadBuffer, PULONG BytesRead);
static NTSTATUS InCloseFileRaw(PRAW_READ_HANDLE RawReadHandle);
NTSTATUS InGetVolumeNameForVolumeMountPoint(PUNICODE_STRING MountPointU, PWCHAR VolumeName);

struct VsnapMutexTag 
{
	int		Count;
	KMUTEX	MutexObj;
	KEVENT	Evt;
};

typedef struct VsnapMutexTag VsnapMutex;

struct VsnapRWLockTag
{
	int		Readers;
	int		Writers;
	KMUTEX	MutexObj;
	KEVENT	Evt;
};

typedef struct VsnapRWLockTag VsnapRWLock;

typedef KMUTEX			MUTEX;
typedef KMUTEX*			PMUTEX;
typedef ERESOURCE		RW_LOCK;
typedef ERESOURCE*		PRW_LOCK;
typedef IO_REMOVE_LOCK	REMOVE_LOCK;
typedef IO_REMOVE_LOCK*	PREMOVE_LOCK;

/* Declaring kernel mode locking primitives. */
#define INIT_MUTEX(x)					KeInitializeMutex(x, 0)
#define ACQUIRE_MUTEX(x)				KeWaitForMutexObject(x, Executive, KernelMode, FALSE, NULL)
#define RELEASE_MUTEX(x)				KeReleaseMutex(x, FALSE)

#define INIT_RW_LOCK(x)					ExInitializeResourceLite(x)
#define ACQUIRE_SHARED_READ_LOCK(x)		ExAcquireResourceSharedLite(x, TRUE)
#define ACQUIRE_EXCLUSIVE_WRITE_LOCK(x)	ExAcquireResourceExclusiveLite(x, TRUE)
#define RELEASE_RW_LOCK(x)				ExReleaseResourceLite(x)
#define DELETE_RW_LOCK(x)				ExDeleteResource(x)

#define INIT_REMOVE_LOCK(x)				IoInitializeRemoveLock(x, NULL, 0, 0)
#define ACQUIRE_REMOVE_LOCK(x)			IN_SUCCESS(IoAcquireRemoveLock(x, NULL))
#define RELEASE_REMOVE_LOCK(x)			IoReleaseRemoveLock(x, NULL)
#define RELEASE_REMOVE_WAIT_LOCK(x)		IoReleaseRemoveLockAndWait(x, NULL)

#define ENTER_CRITICAL_REGION			KeEnterCriticalRegion()
#define LEAVE_CRITICAL_REGION			KeLeaveCriticalRegion()

inline void STRING_COPY(char *dest, char *src, SIZE_T len)
{
	RtlStringCbCopyA(dest, len, src);
}

inline void STRING_CAT(char *str1, char *str2, int len)
{
	RtlStringCbCatA(str1, len, str2);
}

#define STRING_PRINTF(x)	\
{							\
	RtlStringCchPrintfA x;	\
}

inline int STRING_CMP(char *str1, char *str2, size_t sz)
{
	return _strnicmp(str1, str2, sz);
}

inline void MEM_COPY(char *dst, char *src, size_t sz)
{
	RtlCopyMemory(dst, src, sz);
	return;
}

void InitializeVsnapMutex(VsnapMutex *);
void InitializeVsnapRWLock(VsnapRWLock *);
void AcquireVsnapMutex(VsnapMutex *);
void ReleaseVsnapMutex(VsnapMutex *);
void AcquireVsnapReadLock(VsnapRWLock *);
void AcquireVsnapWriteLock(VsnapRWLock *);
void ReleaseVsnapReadLock(VsnapRWLock *);
void ReleaseVsnapWriteLock(VsnapRWLock *);

void *ALLOC_MEMORY(int);

void *ALLOC_MEMORY(SIZE_T, POOL_TYPE);
void *ALLOC_MEMORY_WITH_TAG(int, POOL_TYPE, ULONG);
void FREE_MEMORY(void *);
bool ALLOC_RAW_HANDLE(void **);
void FREE_RAW_HANDLE(void *);

#endif //VSNAP_WIN_KERNEL_MODE

#endif //_WIN_VSNAP_KERNEL_HELPERS_H