#ifndef VSNAP_WIN_KERNEL_MODE
#ifndef _FILE_IO_H_
#define _FILE_IO_H_

#ifdef INMAGE_KERNEL_MODE

#define INSTATUS NTSTATUS
#define IN_SUCCESS(Status) (NT_SUCCESS(Status))
#define INMAGE_STRING_SIZE 256
#define DWORD ULONG
#define LPOVERLAPPED PVOID

#define FILE_BEGIN          0
#define FILE_CURRENT        1
#define FILE_END            2
#define OPEN_EXISTING FILE_OPEN
#define CREATE_ALWAYS   FILE_OVERWRITE_IF

#define FILE_DEFAULT_CREATE_OPTIONS     FILE_NON_DIRECTORY_FILE | FILE_SYNCHRONOUS_IO_NONALERT

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

INSTATUS InCreateFileA(
  CHAR* FileNameA,
  ACCESS_MASK DesiredAccess,
  DWORD ShareMode,
  DWORD CreationDisposition,
  DWORD FlagsAndAttributes,
  HANDLE hTemplateFile,
  IN PLARGE_INTEGER AllocationSize OPTIONAL,
  OUT PHANDLE  FileHandle
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

#else

#define INSTATUS BOOL
#define IN_SUCCESS (Status) (Status)
#define INMAGE_STRING_SIZE 512
const ULONG FILE_DEFAULT_CREATE_OPTIONS = 0;

INSTATUS InCreateFile(
  TCHAR* FileNameA,
  ACCESS_MASK DesiredAccess,
  DWORD ShareMode,
  DWORD CreationDisposition,
  DWORD FlagsAndAttributes,
  HANDLE hTemplateFile,
  IN PLARGE_INTEGER  AllocationSize  OPTIONAL,
  OUT PHANDLE  FileHandle,
  IN ULONG  CreateOptions
);

#endif 

//Common Section 

INSTATUS InReadFile(
  HANDLE FileHandle,
  VOID* ReadBuffer,
  DWORD NumberOfBytesToRead,
  PLARGE_INTEGER ByteOffset,
  ULONG* NumberOfBytesRead,
  LPOVERLAPPED lpOverlapped
);

INSTATUS InWriteFile(
  HANDLE FileHandle,
  PVOID  WriteBuffer,
  DWORD NumberOfBytesToWrite,
  PLARGE_INTEGER ByteOffset,
  ULONG *NumberOfBytesWritten,
  LPOVERLAPPED lpOverlapped
);

INSTATUS InSetFilePointer(
  HANDLE FileHandle,
  LARGE_INTEGER DistanceToMove,
  LARGE_INTEGER *NewDistance,
  DWORD dwMoveMethod
);

INSTATUS InCloseHandle(
  HANDLE FileHandle
);

VOID
InDeleteFileA(PCHAR FileName);

VOID
InDeleteFileW(PWCHAR FileNameW);

NTSTATUS InCreateFileRaw(CHAR* FileName, PRAW_READ_HANDLE RawReadHandle);
NTSTATUS InReadFileRaw(PRAW_READ_HANDLE RawReadHandle,LARGE_INTEGER ByteOffset, ULONG ReadLength, PVOID ReadBuffer, PULONG BytesRead);
NTSTATUS InCloseFileRaw(PRAW_READ_HANDLE RawReadHandle);

#endif
#endif
