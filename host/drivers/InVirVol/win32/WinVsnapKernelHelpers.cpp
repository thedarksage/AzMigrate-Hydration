//
// Copyright (c) 2005 InMage
// This file contains proprietary and confidential information and
// remains the unpublished property of InMage. Use, disclosure,
// or reproduction is prohibited except as permitted by express
// written license aggreement with InMage.
//
// File       : WinVsnapKernelHelpers.cpp
//
// Description: Windows kernel helper routines.
//

#include "DiskBtreeHelpers.h"
#include "WinVsnapKernelHelpers.h"
#include "VVDebugDefines.h"
#include "VVDriverContext.h"
#include "VVIDevControl.h"
#include "VVDispatch.h"
#include "invirvollog.h"

#ifdef UNICODE
#define InCreateFile InCreateFileW
#else
#define InCreateFile InCreateFileA
#endif

#define MAX_STRING_SIZE    128
#define REPARSE_GUID_DATA_HEADER_SIZE   FIELD_OFFSET(REPARSE_GUID_DATA, REPARSE_GUID_DATA::GenericReparseBuffer)

#define FILE_DEVICE_FILE_SYSTEM         0x00000009
#define FSCTL_SET_ZERO_DATA             CTL_CODE(FILE_DEVICE_FILE_SYSTEM, 50, METHOD_BUFFERED, FILE_WRITE_DATA) // FILE_ZERO_DATA_INFORMATION,
#define FSCTL_GET_REPARSE_POINT         CTL_CODE(FILE_DEVICE_FILE_SYSTEM, 42, METHOD_BUFFERED, FILE_ANY_ACCESS) // REPARSE_DATA_BUFFER

/* Start - Non-Exported windows kernel APIs */
/* Return 1 for unc path */
bool InCheckUNC(const CHAR* FileName) {
    bool ret = 0;
    if ((FileName[0] == '\\') && (FileName[1] == '\\')) {
        ret = 1;
    }
    return ret;
}

bool InCheckUNCW(WCHAR* FileName) {
    bool ret = 0;
    if ((FileName[0] == L'\\') && (FileName[1] == L'\\')) {
        ret = 1;
    }
    return ret;
}

static INSTATUS InCreateFileA(
  const CHAR* FileName,
  ACCESS_MASK DesiredAccess,
  DWORD ShareMode,
  DWORD CreationDisposition,
  DWORD FlagsAndAttributes,
  HANDLE hTemplateFile,
  IN PLARGE_INTEGER  AllocationSize  OPTIONAL,
  WinHdl *Hdl,
  bool EventLog
)
{
    NTSTATUS Status;
    IO_STATUS_BLOCK IoStatus;
    UNICODE_STRING DosFileNameU;
    UNICODE_STRING DosDeviceNameU;
    UNICODE_STRING FileNameU;
    ANSI_STRING FileNameA;
    OBJECT_ATTRIBUTES ObjectAttributes;
    bool bUNC = 0;

    bUNC = InCheckUNC(FileName);
    RtlInitAnsiString(&FileNameA, FileName);
    Status = RtlAnsiStringToUnicodeString(&FileNameU, &FileNameA, TRUE);
    
    if(!NT_SUCCESS(Status))
        return Status;

    PWSTR pFileName = FileNameU.Buffer;
    /* UNC Path for kernel need to be in \\??\\UNC\IpAddr\Folder */
    if (bUNC) {
    	/* Path contents extra slash. Increment pointer to remove extra slash*/
        pFileName++;
        RtlInitUnicodeString(&DosDeviceNameU, UNC_PATH);    
    } else {
        RtlInitUnicodeString(&DosDeviceNameU, DOS_DEVICES_PATH);
    }        
    
    DosFileNameU.Buffer = (PWSTR) ALLOC_MEMORY(FileNameU.Length + DosDeviceNameU.Length + sizeof(WCHAR), PagedPool);
    if(NULL == DosFileNameU.Buffer)
    {
        RtlFreeUnicodeString(&FileNameU);
        Status = STATUS_NO_MEMORY;
        return Status;
    }

    DosFileNameU.Length = FileNameU.Length + DosDeviceNameU.Length + sizeof(WCHAR);
    DosFileNameU.MaximumLength = FileNameU.Length + DosDeviceNameU.Length + sizeof(WCHAR);

    RtlZeroMemory(DosFileNameU.Buffer, FileNameU.Length + DosDeviceNameU.Length + sizeof(WCHAR));

    RtlCopyUnicodeString(&DosFileNameU, &DosDeviceNameU);
    RtlAppendUnicodeToString(&DosFileNameU, pFileName);

    InitializeObjectAttributes(&ObjectAttributes, &DosFileNameU, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, NULL, NULL);
	
	ULONG CreateOption = 0;

	if(Hdl->ShouldBuffer)
		CreateOption = FILE_DEFAULT_CREATE_OPTIONS;
	else
		CreateOption = FILE_DEFAULT_CREATE_OPTIONS | FILE_NO_INTERMEDIATE_BUFFERING;
    
    InVolDbgPrint(DL_VV_INFO, DM_VV_VSNAP, ("FileName %wZ\n", &DosFileNameU));
    // InVolDbgPrint(DL_VV_ERROR, DM_VV_VSNAP, ("Desired Access %x FlagsAndAttributes %x ShareMode %x, CreationDisposition %x, CreateOption %x\n", DesiredAccess, FlagsAndAttributes, ShareMode, CreationDisposition, CreateOption));
	Status = ZwCreateFile(&Hdl->Handle, DesiredAccess, &ObjectAttributes, &IoStatus, AllocationSize, FlagsAndAttributes,
                    ShareMode, CreationDisposition,
                    CreateOption,
                    NULL, 0);
    InVolDbgPrint(DL_VV_ERROR, DM_VV_VSNAP,("Status CreateFile %x\n", Status));
	if (NT_SUCCESS(Status)) {
      Status = ObReferenceObjectByHandle(
					Hdl->Handle,
                    DesiredAccess,
                    *IoFileObjectType,
                    KernelMode,
					(PVOID *) &Hdl->fileObject,
                    NULL);
	}
	else
	{
		Hdl->Handle = NULL;
		Hdl->fileObject = NULL;
		Hdl->osDevice = NULL;
	}

    if (!NT_SUCCESS(Status)) {
        USHORT Length = FileNameU.Length;
        if (bUNC) {
            /* Path contents extra slash. Decrement the length*/
            Length = Length - sizeof(WCHAR);
        }
        /* As filename can be big enough print last INMAGE_EVENTLOG_MAX_FILE_LENGTH wchar in eventlog*/ 
        if (Length > (INMAGE_EVENTLOG_MAX_FILE_LENGTH * sizeof(WCHAR))) {
            pFileName = pFileName + ((Length/sizeof(WCHAR)) - INMAGE_EVENTLOG_MAX_FILE_LENGTH);
            Length = INMAGE_EVENTLOG_MAX_FILE_LENGTH * sizeof(WCHAR);
        }
        InVirVolLogError(DriverContext.ControlDevice, NULL, INVIRVOL_OPEN_FILE_FAILURE, STATUS_SUCCESS,
                pFileName,
                Length,
                (ULONGLONG)Status);        
    }
	
	RtlFreeUnicodeString(&FileNameU);
    RtlFreeUnicodeString(&DosFileNameU);

    return Status;
}

static INSTATUS InCreateFileW(
  WCHAR* FileNameW,
  ACCESS_MASK DesiredAccess,
  DWORD ShareMode,
  DWORD CreationDisposition,
  DWORD FlagsAndAttributes,
  HANDLE hTemplateFile,
  IN PLARGE_INTEGER  AllocationSize  OPTIONAL,
  OUT PHANDLE  FileHandle
)
{
    NTSTATUS Status;
    IO_STATUS_BLOCK IoStatus;
    UNICODE_STRING FileNameU;
    UNICODE_STRING DosDeviceNameU;
    UNICODE_STRING DosFileNameU;
    OBJECT_ATTRIBUTES ObjectAttributes;
    bool bUNC = 0;
    
    RtlInitUnicodeString(&FileNameU, (WCHAR *) FileNameW);

    bUNC = InCheckUNCW(FileNameW);
    PWSTR pFileName = FileNameU.Buffer;
    if (bUNC) {
        pFileName++;
        RtlInitUnicodeString(&DosDeviceNameU, UNC_PATH);    
    } else {
        RtlInitUnicodeString(&DosDeviceNameU, DOS_DEVICES_PATH);
    } 
    
    DosFileNameU.Buffer = (PWSTR) ALLOC_MEMORY(FileNameU.Length + DosDeviceNameU.Length + sizeof(WCHAR), PagedPool);
    DosFileNameU.Length = FileNameU.Length + DosDeviceNameU.Length + sizeof(WCHAR);
    DosFileNameU.MaximumLength = FileNameU.Length + DosDeviceNameU.Length + sizeof(WCHAR);
    
    if(NULL == DosFileNameU.Buffer)
    {
        Status = STATUS_NO_MEMORY;
        return Status;
    }

    RtlZeroMemory(DosFileNameU.Buffer, FileNameU.Length + DosDeviceNameU.Length + sizeof(WCHAR));
    
    RtlCopyUnicodeString(&DosFileNameU, &DosDeviceNameU);
    RtlAppendUnicodeToString(&DosFileNameU, pFileName);

    InitializeObjectAttributes(&ObjectAttributes, &DosFileNameU, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, NULL, NULL);

    Status = ZwCreateFile(FileHandle, DesiredAccess, &ObjectAttributes, &IoStatus, AllocationSize, FlagsAndAttributes,
                    FILE_SHARE_READ | ShareMode, CreationDisposition,
                    FILE_DEFAULT_CREATE_OPTIONS,
                    NULL, 0);
    
    RtlFreeUnicodeString(&DosFileNameU);
    return Status;
}

static INSTATUS InReadFile(
  WinHdl *FileHandle,
  VOID  *ReadBuffer,
  DWORD NumberOfBytesToRead,
  PLARGE_INTEGER ByteOffset,
  ULONG *NumberOfBytesRead,
  LPOVERLAPPED lpOverlapped
)
{
    NTSTATUS Status;
    IO_STATUS_BLOCK IoStatus;
	Status = ZwReadFile(FileHandle->Handle, NULL, NULL, NULL, &IoStatus, ReadBuffer, 
               NumberOfBytesToRead, ByteOffset, NULL);
    
    //ASSERT(NT_SUCCESS(Status));

    if(NumberOfBytesRead != NULL)
        *NumberOfBytesRead = (DWORD) IoStatus.Information;
    return Status;
}

static INSTATUS InWriteFile(
  WinHdl *FileHandle,
  PVOID WriteBuffer,
  DWORD NumberOfBytesToWrite,
  PLARGE_INTEGER ByteOffset,
  ULONG *NumberOfBytesWritten,
  LPOVERLAPPED lpOverlapped
)
{
    NTSTATUS Status;
    IO_STATUS_BLOCK IoStatus;

	Status = ZwWriteFile(FileHandle->Handle, NULL, NULL, NULL, &IoStatus, WriteBuffer, 
            NumberOfBytesToWrite, ByteOffset, NULL);

    if(NumberOfBytesWritten != NULL)
        *NumberOfBytesWritten = (DWORD) IoStatus.Information;
    return Status;
}

static INSTATUS InSetFilePointer(
  WinHdl *FileHandle,
  LARGE_INTEGER DistanceToMove,
  LARGE_INTEGER *NewDistance,
  DWORD dwMoveMethod
)
{
    NTSTATUS Status;
    IO_STATUS_BLOCK IoStatus;
    FILE_POSITION_INFORMATION PosInformation;
    PosInformation.CurrentByteOffset.QuadPart = 0;

    switch(dwMoveMethod)
    {
    case FILE_BEGIN:
        {
            PosInformation.CurrentByteOffset.QuadPart = DistanceToMove.QuadPart;
			Status = ZwSetInformationFile(FileHandle->Handle, &IoStatus,  &PosInformation, 
                            sizeof(FILE_POSITION_INFORMATION),FilePositionInformation);
            break;
        }
    case FILE_END:
        {
            FILE_STANDARD_INFORMATION FileStandardInfo;
			Status = ZwQueryInformationFile(FileHandle->Handle, &IoStatus, &FileStandardInfo, sizeof(FILE_STANDARD_INFORMATION),
                        FileStandardInformation);
            
            //ASSERT(NT_SUCCESS(Status));

            PosInformation.CurrentByteOffset.QuadPart = FileStandardInfo.EndOfFile.QuadPart + DistanceToMove.QuadPart;
			Status = ZwSetInformationFile(FileHandle->Handle, &IoStatus,  &PosInformation, 
                            sizeof(FILE_POSITION_INFORMATION),FilePositionInformation);
            
            NewDistance->QuadPart = FileStandardInfo.EndOfFile.QuadPart + DistanceToMove.QuadPart;
            break;
        }
    case FILE_CURRENT:
        {
			Status = ZwQueryInformationFile(FileHandle->Handle, &IoStatus, &PosInformation, sizeof(FILE_POSITION_INFORMATION),
                            FilePositionInformation);

            PosInformation.CurrentByteOffset.QuadPart += DistanceToMove.QuadPart;
            
			Status = ZwSetInformationFile(FileHandle->Handle, &IoStatus,  &PosInformation, 
                            sizeof(FILE_POSITION_INFORMATION),FilePositionInformation);
            break;
        }
    }

    return Status;
}
INSTATUS QueryFileSize(void *FileHandle,
  PLARGE_INTEGER FileSize)
{
    return InSetFilePointer((WinHdl *)FileHandle,FileSize);
}

static INSTATUS InSetFilePointer(
  WinHdl *FileHandle,
  PLARGE_INTEGER FileSize)
{
    FILE_STANDARD_INFORMATION FileStandardInfo;
    IO_STATUS_BLOCK IoStatus;
	INSTATUS Status = ZwQueryInformationFile(FileHandle->Handle, &IoStatus, &FileStandardInfo, sizeof(FILE_STANDARD_INFORMATION),
                FileStandardInformation);

    if (!NT_SUCCESS(Status))
        return Status;

    FileSize->QuadPart = FileStandardInfo.EndOfFile.QuadPart;

    return Status;
}

static INSTATUS InCloseHandle(
  WinHdl *FileHandle
)
{
	if(FileHandle->fileObject)
	{
		ObDereferenceObject(FileHandle->fileObject);
		FileHandle->fileObject = NULL;
	}
    
	return ZwClose(FileHandle->Handle);
}

/*#ifdef UNICODE
#define InDeleteFile InDeleteFileW
#else
#define InDeleteFile InDeleteFileA
#endif

static INSTATUS InDeleteFileW(PWCHAR FileNameW)
{
    NTSTATUS Status;
    IO_STATUS_BLOCK IoStatus;
    UNICODE_STRING FileNameU;
    UNICODE_STRING DosDeviceNameU;
    UNICODE_STRING DosFileNameU;
    HANDLE  FileHandle;
    OBJECT_ATTRIBUTES ObjectAttributes;

    RtlInitUnicodeString(&DosDeviceNameU, DOS_DEVICES_PATH);
    RtlInitUnicodeString(&FileNameU, (WCHAR *) FileNameW);
    
    DosFileNameU.Buffer = (PWSTR) ALLOC_MEMORY(FileNameU.Length + DosDeviceNameU.Length + sizeof(WCHAR), PagedPool);
    DosFileNameU.Length = FileNameU.Length + DosDeviceNameU.Length + sizeof(WCHAR);
    DosFileNameU.MaximumLength = FileNameU.Length + DosDeviceNameU.Length + sizeof(WCHAR);
    
    if(NULL == DosFileNameU.Buffer)
        return;
    
    RtlZeroMemory(DosFileNameU.Buffer, FileNameU.Length + DosDeviceNameU.Length + sizeof(WCHAR));
    
    RtlCopyUnicodeString(&DosFileNameU, &DosDeviceNameU);
    RtlAppendUnicodeStringToString(&DosFileNameU, &FileNameU);

    InitializeObjectAttributes(&ObjectAttributes, &DosFileNameU, OBJ_CASE_INSENSITIVE, NULL, NULL);

    Status = ZwCreateFile(&FileHandle, GENERIC_READ, &ObjectAttributes, &IoStatus, 
                        NULL, FILE_ATTRIBUTE_NORMAL,
                        FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE, FILE_OPEN,
                        FILE_DELETE_ON_CLOSE  | FILE_DEFAULT_CREATE_OPTIONS,
                        NULL, 0);
        
    if(!NT_SUCCESS(Status))
        return Status;

    ZwClose(FileHandle);
    
    RtlFreeUnicodeString(&DosFileNameU);

	return Status;
}
*/
static INSTATUS InDeleteFileA(PCHAR FileName)
{
    NTSTATUS Status;
    IO_STATUS_BLOCK IoStatus;
    UNICODE_STRING DosFileNameU;
    UNICODE_STRING DosDeviceNameU;
    UNICODE_STRING FileNameU;
    ANSI_STRING FileNameA;
    HANDLE  FileHandle;
    OBJECT_ATTRIBUTES ObjectAttributes;
    bool bUNC = 0;

    bUNC = InCheckUNC(FileName);
    
    RtlInitAnsiString(&FileNameA, FileName);
    RtlAnsiStringToUnicodeString(&FileNameU, &FileNameA, TRUE);

    PWSTR pFileName = FileNameU.Buffer;
    if (bUNC) {
        pFileName++;
        RtlInitUnicodeString(&DosDeviceNameU, UNC_PATH);    
    } else {
        RtlInitUnicodeString(&DosDeviceNameU, DOS_DEVICES_PATH);
    }
    
    DosFileNameU.Buffer = (PWSTR) ALLOC_MEMORY(FileNameU.Length + DosDeviceNameU.Length + sizeof(WCHAR),PagedPool);
    DosFileNameU.Length = FileNameU.Length + DosDeviceNameU.Length + sizeof(WCHAR);
    DosFileNameU.MaximumLength = FileNameU.Length + DosDeviceNameU.Length + sizeof(WCHAR);

    if(NULL == DosFileNameU.Buffer)
        return STATUS_NO_MEMORY;

    RtlZeroMemory(DosFileNameU.Buffer, FileNameU.Length + DosDeviceNameU.Length + sizeof(WCHAR));

    RtlCopyUnicodeString(&DosFileNameU, &DosDeviceNameU);
    RtlAppendUnicodeToString(&DosFileNameU, pFileName);

    InitializeObjectAttributes(&ObjectAttributes, &DosFileNameU, OBJ_CASE_INSENSITIVE , NULL, NULL);

    Status = ZwCreateFile(&FileHandle, GENERIC_READ, &ObjectAttributes, &IoStatus, 
                    NULL, FILE_ATTRIBUTE_NORMAL,
                    FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE, FILE_OPEN,
                    FILE_DELETE_ON_CLOSE  | FILE_DEFAULT_CREATE_OPTIONS,
                    NULL, 0);
    
    if(!NT_SUCCESS(Status))
	{
		RtlFreeUnicodeString(&FileNameU);
		RtlFreeUnicodeString(&DosFileNameU);
        return Status;
	}

    ZwClose(FileHandle);

    RtlFreeUnicodeString(&FileNameU);
    RtlFreeUnicodeString(&DosFileNameU);

    return Status;
}

static NTSTATUS
GenericCompletionRoutine(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context
    )
{
    PKEVENT Event = (PKEVENT)Context;
    KeSetEvent(Event, IO_NO_INCREMENT, FALSE);
    return(STATUS_MORE_PROCESSING_REQUIRED);
}

static NTSTATUS InReadFileRaw(PRAW_READ_HANDLE RawReadHandle,LARGE_INTEGER ByteOffset, ULONG ReadLength, PVOID ReadBuffer, PULONG BytesRead)
{
    NTSTATUS Status = STATUS_SUCCESS;
    ULONG SystemBufferLength            = 0;
    PIRP Irp                            = NULL;
    PIO_STACK_LOCATION IoStackLocation  = NULL;
    IO_STATUS_BLOCK IoStatusBlock;
    KEVENT Event;
    
    KIRQL CurrIrql = KeGetCurrentIrql();

    ASSERT(CurrIrql <= APC_LEVEL);
    ASSERT(NULL != BytesRead);
    ASSERT(NULL != ReadBuffer);
    ASSERT(NULL != RawReadHandle->DeviceObject);
    ASSERT(NULL != RawReadHandle->FileObject);
    
    *BytesRead = 0;
    IoStatusBlock.Status = STATUS_SUCCESS;
    IoStatusBlock.Information = 0;

    Irp = IoAllocateIrp(RawReadHandle->DeviceObject->StackSize, FALSE);
    if(NULL == Irp) 
    {
		InVolDbgPrint(DL_VV_WARNING, DM_VV_DEVICE_CONTROL, ("Vsnap: IoAllocateIrp Failed\n"));
        Status = STATUS_NO_MEMORY;
        return Status;
    }

    KeInitializeEvent(&Event, NotificationEvent, FALSE);

    Irp->AssociatedIrp.SystemBuffer         = ReadBuffer;
    Irp->Tail.Overlay.OriginalFileObject    = RawReadHandle->FileObject;
    Irp->RequestorMode                      = KernelMode;
    Irp->UserIosb                           = &IoStatusBlock;

    IoStackLocation                         = IoGetNextIrpStackLocation(Irp);
    IoStackLocation->MajorFunction          = (UCHAR)IRP_MJ_READ;
    IoStackLocation->MinorFunction          = 0;
    IoStackLocation->DeviceObject           = RawReadHandle->DeviceObject;
    IoStackLocation->FileObject             = RawReadHandle->FileObject;

    IoStackLocation->Parameters.Read.Length = ReadLength;
    IoStackLocation->Parameters.Read.ByteOffset.QuadPart = ByteOffset.QuadPart;

    PMDL mdl = IoAllocateMdl(ReadBuffer, ReadLength, FALSE, TRUE, Irp);
    if(NULL == mdl)
    {
        Status = STATUS_NO_MEMORY;
		InVolDbgPrint(DL_VV_ERROR, DM_VV_DRIVER_INIT, ("Vsnap: IoAllocateMdl Failed:%x\n", Status));
        IoFreeIrp(Irp);
        return Status;
    }

    IoSetCompletionRoutine(Irp, GenericCompletionRoutine, &Event, TRUE, TRUE, TRUE);

    __try{
        MmProbeAndLockPages(mdl, KernelMode, IoWriteAccess);
    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
        Status = GetExceptionCode();
		InVolDbgPrint(DL_VV_ERROR, DM_VV_DRIVER_INIT, ("Vsnap: MmProbeAndLockPages Failed:%x\n", Status));
        FREE_MEMORY(mdl);
        IoFreeIrp(Irp);
        return Status;
    }

    Status = IoCallDriver(RawReadHandle->DeviceObject, Irp);
    if(Status == STATUS_PENDING)
    {
        KeWaitForSingleObject(&Event, Executive, KernelMode, FALSE, NULL);
    }

    if(NT_SUCCESS(Status))
        *BytesRead = (ULONG)(Irp->IoStatus.Information);

    MmUnlockPages(mdl);
    FREE_MEMORY(mdl);
    IoFreeIrp(Irp);
    return Status;
}

static NTSTATUS InCreateFileRaw(CHAR* FileName, PRAW_READ_HANDLE RawReadHandle)
{
    NTSTATUS Status = STATUS_SUCCESS;
    PWCHAR VolumeNameW = NULL;
    UNICODE_STRING VolumeNameU;

    ASSERT(NULL != FileName);
    
    FileName[0] = (char) toupper(FileName[0]);
    ANSI_STRING FileNameA;
    UNICODE_STRING FileNameU;
    WCHAR FileNameW[MAX_STRING_SIZE] = L"\\DosDevices\\Global\\";
    UNICODE_STRING DosFileNameU;

    RtlInitAnsiString(&FileNameA, FileName);
    RtlAnsiStringToUnicodeString(&FileNameU, &FileNameA, TRUE);

    RtlInitUnicodeString(&DosFileNameU, FileNameW);
    DosFileNameU.MaximumLength = MAX_STRING_SIZE * sizeof(WCHAR);
 
    if('\0' == FileName[2] || (('\\' == FileName[2]) && ('\0' == FileName[3])))
    {
            RtlAppendUnicodeStringToString(&DosFileNameU, &FileNameU);
    }
    else
    {   
        VolumeNameW = (PWCHAR)ALLOC_MEMORY((sizeof(WCHAR) * 1024), NonPagedPool);
        if (VolumeNameW)
        {
            RtlZeroMemory(VolumeNameW, (sizeof(WCHAR) * 1024));
            Status = InGetVolumeNameForVolumeMountPoint(&FileNameU, VolumeNameW);
            if(NT_SUCCESS(Status))
            {
                RtlInitUnicodeString(&VolumeNameU, VolumeNameW);
                VolumeNameU.Length -= sizeof(WCHAR);
                RtlAppendUnicodeStringToString(&DosFileNameU, &VolumeNameU); //L"\\DosDevices\\Global\\Volume{53f72f4b-7d10-11db-98d4-000c296584a5}");
            } else {
                goto cleanup_and_exit;
            }
        }
        else
        {
            Status = STATUS_NO_MEMORY;
            goto cleanup_and_exit;
        }
    }
    Status = IoGetDeviceObjectPointer(&DosFileNameU, FILE_READ_ATTRIBUTES, &RawReadHandle->FileObject, &RawReadHandle->DeviceObject);

cleanup_and_exit :
    if (VolumeNameW)
        FREE_MEMORY(VolumeNameW);

    RtlFreeUnicodeString(&FileNameU);
    return Status;
}

static NTSTATUS InCloseFileRaw(PRAW_READ_HANDLE RawReadHandle)
{
    ObDereferenceObject(RawReadHandle->FileObject);
    RawReadHandle->DeviceObject = NULL;
    RawReadHandle->FileObject = NULL;
    return STATUS_SUCCESS;
}
/* End - Non exported windows kernel APIs */

/* Start - Exported windows kernel APIs */
bool OPEN_FILE(const char *FileName, unsigned int OpenMode, unsigned int SharedMode, void **Handle, bool EnableLog, NTSTATUS *ErrStatus, bool EventLog)
{
	ACCESS_MASK DesiredAccess = GENERIC_READ;
	WinHdl *Hdl = NULL;

	if(OpenMode & O_WRONLY)
		DesiredAccess = GENERIC_WRITE;
	else if(OpenMode & O_RDWR)
		DesiredAccess = GENERIC_READ | GENERIC_WRITE;
	
	DWORD Creation = FILE_OPEN;

	if((OpenMode & (O_CREAT | O_EXCL)) == (O_CREAT | O_EXCL))
		Creation = FILE_CREATE;
	else if((OpenMode & (O_CREAT | O_TRUNC)) == (O_CREAT | O_TRUNC))
		Creation = FILE_OVERWRITE_IF;
	else if(OpenMode & O_CREAT)
		Creation = FILE_OPEN_IF;

	DWORD flags = FILE_ATTRIBUTE_NORMAL;
	
	INSTATUS Status = STATUS_SUCCESS;
	
	Hdl = (WinHdl *)ALLOC_MEMORY(sizeof(WinHdl));
	if(!Hdl) {
        if (EnableLog)
            *ErrStatus = (INSTATUS)STATUS_NO_MEMORY; 
		return false;
    }    
	Hdl->fileObject = NULL;
	Hdl->Handle = NULL;
	Hdl->osDevice = NULL;
	
	if(OpenMode & O_BUFFER)
		Hdl->ShouldBuffer = true;
	else
		Hdl->ShouldBuffer = false;

	Status = InCreateFileA(FileName, DesiredAccess, SharedMode, Creation, flags, NULL, NULL, Hdl, EventLog);
	if(!IN_SUCCESS(Status))
	{
		InVolDbgPrint(DL_VV_INFO, DM_VV_VSNAP, ("Vsnap: InCreateFileA failed Status %x\n", Status));
		if (EnableLog)
			*ErrStatus = (INSTATUS)Status; 
		FREE_MEMORY(Hdl);
		*Handle = NULL;
		return false;
	}
	*Handle = (void *)Hdl;

	return true;
}

bool READ_FILE(void *Handle, void *Buffer, unsigned __int64 Offset, unsigned int Length, int *BytesRead, bool EnableLog, NTSTATUS *ErrStatus)
{
	INSTATUS Status = STATUS_SUCCESS;
	LARGE_INTEGER	ByteOffset;

	ByteOffset.QuadPart = Offset;

	Status = InReadFile((WinHdl *)Handle, Buffer, Length, &ByteOffset, (ULONG *)BytesRead, NULL);
	if(!IN_SUCCESS(Status))
	{
		if (EnableLog)
			*ErrStatus = (INSTATUS)Status; 
		InVolDbgPrint(DL_VV_INFO, DM_VV_VSNAP, ("Vsnap: InReadFile failed Status %x\n", Status));
		return false;
	}

	return true;
}

bool WRITE_FILE(void *Handle, void *Buffer, unsigned __int64 Offset, unsigned int Length, int *BytesWritten, bool EnableLog, NTSTATUS *ErrStatus)
{
	INSTATUS Status = STATUS_SUCCESS;
	LARGE_INTEGER	ByteOffset;

	ByteOffset.QuadPart = Offset;
	Status = InWriteFile((WinHdl *)Handle, Buffer, Length, &ByteOffset, (ULONG *)BytesWritten, NULL);
	if(!IN_SUCCESS(Status))
	{   
		if (EnableLog)
			*ErrStatus = (INSTATUS)Status; 
		InVolDbgPrint(DL_VV_INFO, DM_VV_VSNAP, ("Vsnap: InWriteFile failed Status %x\n", Status));
		return false;
	}

	return true;
}

bool SEEK_FILE(void *Handle, unsigned __int64 Offset, unsigned __int64 *NewOffset, unsigned int WhereToSeekFrom)
{
	INSTATUS Status = STATUS_SUCCESS;
	LARGE_INTEGER	DistanceToMove, DistanceMoved;

	DistanceToMove.QuadPart = Offset;
	DistanceMoved.QuadPart = 0;
	
	switch(WhereToSeekFrom)
	{
		case SEEK_SET:
			WhereToSeekFrom = FILE_BEGIN;
			break;
		case SEEK_CUR:
			WhereToSeekFrom = FILE_CURRENT;
			break;
		case SEEK_END: 
			WhereToSeekFrom = FILE_END;
			break;
	}

	Status = InSetFilePointer((WinHdl *)Handle, DistanceToMove, &DistanceMoved, WhereToSeekFrom);
	if(!IN_SUCCESS(Status))
	{
		InVolDbgPrint(DL_VV_INFO, DM_VV_VSNAP, ("Vsnap: InSetFilePointer failed Status %x\n", Status));
		return false;
	}
	
	*NewOffset = (ULONGLONG)DistanceMoved.QuadPart;
	return true;

}

void CLOSE_FILE(void *Handle)
{
	InCloseHandle((WinHdl *)Handle);
	if(Handle != NULL)
		FREE_MEMORY(Handle);
}

bool DELETE_FILE(char *FullFileNamePath)
{
	INSTATUS Status = STATUS_SUCCESS;

	Status = InDeleteFileA(FullFileNamePath);
	if(!IN_SUCCESS(Status))
	{
		InVolDbgPrint(DL_VV_INFO, DM_VV_VSNAP, ("Vsnap: InDeleteFileA failed Status %x\n", Status));
		return false;
	}

	return true;
}

bool OPEN_RAW_FILE(char *FileName, void *Handle)
{
	INSTATUS Status = STATUS_SUCCESS;

	Status = InCreateFileRaw(FileName, (PRAW_READ_HANDLE)Handle);
	if(!IN_SUCCESS(Status))
	{
		InVolDbgPrint(DL_VV_INFO, DM_VV_VSNAP, ("Vsnap: InCreateFileRaw failed Status %x\n", Status));
		return false;
	}

	return true;
}

bool READ_RAW_FILE(void *Handle, void *Buffer, unsigned __int64 Offset, unsigned int Length, int *BytesRead)
{
	INSTATUS Status = STATUS_SUCCESS;
	LARGE_INTEGER DistanceToMove;

	DistanceToMove.QuadPart = Offset;
	Status = InReadFileRaw((PRAW_READ_HANDLE)Handle, DistanceToMove, Length, Buffer, (ULONG *)BytesRead);
	if(!IN_SUCCESS(Status))
	{
		InVirVolLogError(DriverContext.ControlDevice, __LINE__, INVIRVOL_ERR_TARGET_READ_FAILED, STATUS_SUCCESS,
			               Status);
		InVolDbgPrint(DL_VV_INFO, DM_VV_VSNAP, ("Vsnap: InReadFileRaw failed Status %x\n", Status));
		return false;
	}

	return true;
}

void CLOSE_RAW_FILE(void *Handle)
{
	InCloseFileRaw((PRAW_READ_HANDLE)Handle);
}

void *ALLOC_MEMORY(int size)
{
	return ExAllocatePoolWithTag(NonPagedPool, size, VVTAG_GENERIC_NONPAGED);
}

void *ALLOC_MEMORY(SIZE_T size, POOL_TYPE PoolType)
{
  void *tmp = NULL;
    if(PagedPool == PoolType)
        tmp = ExAllocatePoolWithTag(PoolType, size, VVTAG_GENERIC_PAGED);
    else {
            
        tmp = ExAllocatePoolWithTag(PoolType, size,VVTAG_GENERIC_NONPAGED );
        
    }

    return tmp;
           
}

void *ALLOC_MEMORY_WITH_TAG(int size, POOL_TYPE PoolType, ULONG tag)
{
    return ExAllocatePoolWithTag(PoolType, size, tag);
}

void FREE_MEMORY(void *Buffer)
{
	if(Buffer)
		ExFreePool(Buffer);
}

bool ALLOC_RAW_HANDLE(void **RTgtHdl)
{
	*RTgtHdl = (void *)ALLOC_MEMORY(sizeof(RAW_READ_HANDLE));
	if(!*RTgtHdl)
		return false;

	return true;
}

void FREE_RAW_HANDLE(void *RTgtHdl)
{
	FREE_MEMORY(RTgtHdl);
}

void 
InitializeEntryList(PSINGLE_LIST_ENTRY ListHead)
{
    ListHead->Next = NULL;
}

bool
IsSingleListEmpty(PSINGLE_LIST_ENTRY ListHead)
{
	return (ListHead->Next==NULL);
}

PSINGLE_LIST_ENTRY InPopEntryList(PSINGLE_LIST_ENTRY ListHead)
{
    PSINGLE_LIST_ENTRY FirstEntry = NULL;
    
    FirstEntry = ListHead->Next;
    if (FirstEntry != NULL) {     
        ListHead->Next = FirstEntry->Next;
    }
    
    return FirstEntry;
}

void InPushEntryList(PSINGLE_LIST_ENTRY ListHead, PSINGLE_LIST_ENTRY Entry)
{
    Entry->Next = ListHead->Next;
    ListHead->Next = Entry;
}

void InitializeVsnapMutex(VsnapMutex *Mutex)
{
	KeInitializeMutex(&Mutex->MutexObj, 0);
	KeInitializeEvent(&Mutex->Evt, NotificationEvent, FALSE);
	Mutex->Count = 0;
}

void InitializeVsnapRWLock(VsnapRWLock *RwLock)
{
	KeInitializeMutex(&RwLock->MutexObj, 0);
	KeInitializeEvent(&RwLock->Evt, NotificationEvent, FALSE);
	RwLock->Readers = 0;
	RwLock->Writers = 0;
}

void AcquireVsnapMutex(VsnapMutex *Mutex)
{
Loop:
	KeWaitForMutexObject(&Mutex->MutexObj, Executive, KernelMode, FALSE, NULL);
	if(Mutex->Count != 0)
	{
		KeReleaseMutex(&Mutex->MutexObj, FALSE);
		KeWaitForSingleObject(&Mutex->Evt, Executive, KernelMode, FALSE, NULL);
		goto Loop;
	}

	Mutex->Count = 1;
	KeClearEvent(&Mutex->Evt);
	KeReleaseMutex(&Mutex->MutexObj, FALSE);
}

void ReleaseVsnapMutex(VsnapMutex *Mutex)
{
	KeWaitForMutexObject(&Mutex->MutexObj, Executive, KernelMode, FALSE, NULL);
	Mutex->Count = 0;
	KeSetEvent(&Mutex->Evt, IO_NO_INCREMENT, FALSE);
	KeReleaseMutex(&Mutex->MutexObj, FALSE);
}

void AcquireVsnapReadLock(VsnapRWLock *RwLock)
{
Loop:
	KeWaitForMutexObject(&RwLock->MutexObj, Executive, KernelMode, FALSE, NULL);
	if(RwLock->Writers != 0)
	{
		KeReleaseMutex(&RwLock->MutexObj, FALSE);
		KeWaitForSingleObject(&RwLock->Evt, Executive, KernelMode, FALSE, NULL);
		goto Loop;
	}
	RwLock->Readers++;
	KeClearEvent(&RwLock->Evt);
	KeReleaseMutex(&RwLock->MutexObj, FALSE);
}

void AcquireVsnapWriteLock(VsnapRWLock *RwLock)
{
Loop:
	KeWaitForMutexObject(&RwLock->MutexObj, Executive, KernelMode, FALSE, NULL);
	if((RwLock->Readers != 0) || (RwLock->Writers != 0))
	{
		KeReleaseMutex(&RwLock->MutexObj, FALSE);
		KeWaitForSingleObject(&RwLock->Evt, Executive, KernelMode, FALSE, NULL);
		goto Loop;
	}

	RwLock->Writers = 1;
	KeClearEvent(&RwLock->Evt);
	KeReleaseMutex(&RwLock->MutexObj, FALSE);
}

void ReleaseVsnapReadLock(VsnapRWLock *RwLock)
{
	KeWaitForMutexObject(&RwLock->MutexObj, Executive, KernelMode, FALSE, NULL);
	RwLock->Readers--;
	if(RwLock->Readers == 0)
	{
		KeSetEvent(&RwLock->Evt, IO_NO_INCREMENT, FALSE); //Wake up any writers
	}
	KeReleaseMutex(&RwLock->MutexObj, FALSE);
}

void ReleaseVsnapWriteLock(VsnapRWLock *RwLock)
{
	KeWaitForMutexObject(&RwLock->MutexObj, Executive, KernelMode, FALSE, NULL);
	RwLock->Writers = 0;
	KeSetEvent(&RwLock->Evt, IO_NO_INCREMENT, FALSE); //Wake up any writers/readers
	KeReleaseMutex(&RwLock->MutexObj, FALSE);
}

NTSTATUS InGetVolumeNameForVolumeMountPoint(PUNICODE_STRING MountPointU, PWCHAR VolumeName)
{
    ASSERT(NULL != VolumeName);
	
    NTSTATUS Status = STATUS_SUCCESS;
    PIO_STACK_LOCATION IoStackLocation;
    IO_STATUS_BLOCK IoStatus;
    KEVENT Event;
    PDEVICE_OBJECT FileSystemDeviceObject = NULL;
    PFILE_OBJECT FileSystemFileObject = NULL;
    HANDLE hFile = NULL;
	PVOID IOBuffer = NULL;
    OBJECT_ATTRIBUTES ObjectAttributes;
    PIRP FileSystemIrp = NULL;
    UNICODE_STRING NtMountPointU;
    PWCHAR NTMountPointBuffer = NULL;//L"\\??\\";

    NTMountPointBuffer = (PWCHAR)ALLOC_MEMORY(512, NonPagedPool);
    if(!NTMountPointBuffer) {
        Status = STATUS_NO_MEMORY;
        return Status;
    }
    RtlZeroMemory(NTMountPointBuffer, 512);
    RtlCopyMemory(NTMountPointBuffer, L"\\??\\", 8);

    RtlInitUnicodeString(&NtMountPointU, NTMountPointBuffer);
    NtMountPointU.MaximumLength = 512;
    RtlAppendUnicodeStringToString(&NtMountPointU, MountPointU);
    

    InitializeObjectAttributes(&ObjectAttributes, &NtMountPointU, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE , NULL, NULL);

    Status = ZwCreateFile(&hFile, GENERIC_READ | GENERIC_WRITE, &ObjectAttributes, &IoStatus, NULL, 
                    FILE_ATTRIBUTE_NORMAL | FILE_ATTRIBUTE_REPARSE_POINT,
                    FILE_SHARE_READ | FILE_SHARE_WRITE, FILE_OPEN,
                    FILE_OPEN_REPARSE_POINT | FILE_NO_INTERMEDIATE_BUFFERING | FILE_SYNCHRONOUS_IO_NONALERT,
                    NULL, 0);

    if (!NT_SUCCESS(Status)) {
        InVolDbgPrint(DL_VV_WARNING, DM_VV_DEVICE_CONTROL, ("ZwCreateFile Failed: Status:%x\n", Status));
        goto cleanup_and_exit;
    }

    KeInitializeEvent(&Event, NotificationEvent, FALSE);

    IOBuffer = ALLOC_MEMORY(1024, NonPagedPool);
    
    if(NULL == IOBuffer)
        goto cleanup_and_exit;

    Status = ObReferenceObjectByHandle(hFile, GENERIC_READ | GENERIC_WRITE, *IoFileObjectType ,
                KernelMode,
                (PVOID *)&FileSystemFileObject,
                NULL
            );

    FileSystemDeviceObject = IoGetRelatedDeviceObject(FileSystemFileObject);

    FileSystemIrp = IoBuildDeviceIoControlRequest(FSCTL_GET_REPARSE_POINT, 
                    FileSystemDeviceObject,
                    0,
                    0,
                    IOBuffer,
                    1024,
                    FALSE,
                    &Event,
                    &IoStatus);

    if(FileSystemIrp)
    {
        IoStackLocation                                 = IoGetNextIrpStackLocation(FileSystemIrp);
        IoStackLocation->MajorFunction                  = (UCHAR)IRP_MJ_FILE_SYSTEM_CONTROL;
        IoStackLocation->MinorFunction                  = IRP_MN_KERNEL_CALL;
        IoStackLocation->FileObject                     = FileSystemFileObject;
        
        Status = IoCallDriver(FileSystemDeviceObject, FileSystemIrp);

        if(STATUS_PENDING == Status)
        {
            KeWaitForSingleObject(&Event, Executive, KernelMode, FALSE, NULL);
        }

        if(!NT_SUCCESS(Status))
        {
            InVolDbgPrint(DL_VV_WARNING, DM_VV_DEVICE_CONTROL, ("FSCTL_GET_REPARSE_POINT Failed: Status:%x\n", Status));
        }
        else
        {
            InVolDbgPrint(DL_VV_VERBOSE, DM_VV_DEVICE_CONTROL, 
                ("FSCTL_GET_REPARSE_POINT Status Success %x BytesReturned:%x\n", Status, IoStatus.Information));
            ASSERT(IoStatus.Information > 16 );
            if( IoStatus.Information < 16 )
                goto cleanup_and_exit;

            RtlCopyMemory(VolumeName, (PCHAR)IOBuffer + 24, IoStatus.Information - 24);
        }
    }
 

cleanup_and_exit:

    if(NULL != hFile)
    {
        ZwClose(hFile);
        hFile = NULL;
    }

    if(NULL != FileSystemFileObject)
    {
        ObDereferenceObject(FileSystemFileObject);
    }

    if(NULL != IOBuffer)
    {
        FREE_MEMORY(IOBuffer);
    }

    if(NTMountPointBuffer) {
        FREE_MEMORY(NTMountPointBuffer);
    }


    return Status;
}
/* End - Exported windows kernel APIs */