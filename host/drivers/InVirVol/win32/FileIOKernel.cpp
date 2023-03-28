#include "VVCommon.h"
#include <stdlib.h>
#include "VVDriverContext.h"
#include <VVIDevControl.h>

#define INMAGE_KERNEL_MODE

#ifdef INMAGE_KERNEL_MODE

#include "FileIO.h"

#ifdef UNICODE
#define InCreateFile InCreateFileW
#else
#define InCreateFile InCreateFileA
#endif

#define MAX_STRING_SIZE    128

INSTATUS InCreateFileA(
  CHAR* FileName,
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
    UNICODE_STRING DosFileNameU;
    UNICODE_STRING DosDeviceNameU;
    UNICODE_STRING FileNameU;
    ANSI_STRING FileNameA;
    OBJECT_ATTRIBUTES ObjectAttributes;

    RtlInitAnsiString(&FileNameA, FileName);
    RtlAnsiStringToUnicodeString(&FileNameU, &FileNameA, TRUE);
    RtlInitUnicodeString(&DosDeviceNameU, DOS_DEVICES_PATH);
    
    DosFileNameU.Buffer = (PWSTR) ALLOC_MEMORY(FileNameU.Length + DosDeviceNameU.Length + sizeof(WCHAR), PagedPool);
    if(NULL == DosFileNameU.Buffer)
    {
        Status = STATUS_NO_MEMORY;
        return Status;
    }
    DosFileNameU.Length = FileNameU.Length + DosDeviceNameU.Length + sizeof(WCHAR);
    DosFileNameU.MaximumLength = FileNameU.Length + DosDeviceNameU.Length + sizeof(WCHAR);

    

    RtlZeroMemory(DosFileNameU.Buffer, FileNameU.Length + DosDeviceNameU.Length + sizeof(WCHAR));

    RtlCopyUnicodeString(&DosFileNameU, &DosDeviceNameU);
    RtlAppendUnicodeStringToString(&DosFileNameU, &FileNameU);

    InitializeObjectAttributes(&ObjectAttributes, &DosFileNameU, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, NULL, NULL);

    Status = ZwCreateFile(FileHandle, DesiredAccess, &ObjectAttributes, &IoStatus, AllocationSize, FlagsAndAttributes,
                    FILE_SHARE_READ | ShareMode, CreationDisposition,
                    FILE_DEFAULT_CREATE_OPTIONS,
                    NULL, 0);
    
    RtlFreeUnicodeString(&FileNameU);
    RtlFreeUnicodeString(&DosFileNameU);

    return Status;
}

INSTATUS InCreateFileW(
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

    RtlInitUnicodeString(&DosDeviceNameU, DOS_DEVICES_PATH);
    RtlInitUnicodeString(&FileNameU, (WCHAR *) FileNameW);
    
    DosFileNameU.Buffer = (PWSTR) ALLOC_MEMORY(FileNameU.Length + DosDeviceNameU.Length + sizeof(WCHAR),PagedPool);
    if(NULL == DosFileNameU.Buffer)
    {
        Status = STATUS_NO_MEMORY;
        return Status;
    }
    DosFileNameU.Length = FileNameU.Length + DosDeviceNameU.Length + sizeof(WCHAR);
    DosFileNameU.MaximumLength = FileNameU.Length + DosDeviceNameU.Length + sizeof(WCHAR);
    


    RtlZeroMemory(DosFileNameU.Buffer, FileNameU.Length + DosDeviceNameU.Length + sizeof(WCHAR));
    
    RtlCopyUnicodeString(&DosFileNameU, &DosDeviceNameU);
    RtlAppendUnicodeStringToString(&DosFileNameU, &FileNameU);

    InitializeObjectAttributes(&ObjectAttributes, &DosFileNameU, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, NULL, NULL);

    Status = ZwCreateFile(FileHandle, DesiredAccess, &ObjectAttributes, &IoStatus, AllocationSize, FlagsAndAttributes,
                    FILE_SHARE_READ | ShareMode, CreationDisposition,
                    FILE_DEFAULT_CREATE_OPTIONS,
                    NULL, 0);
    
    RtlFreeUnicodeString(&DosFileNameU);
    return Status;
}

INSTATUS InReadFile(
  HANDLE FileHandle,
  VOID  *ReadBuffer,
  DWORD NumberOfBytesToRead,
  PLARGE_INTEGER ByteOffset,
  ULONG *NumberOfBytesRead,
  LPOVERLAPPED lpOverlapped
)
{
    NTSTATUS Status;
    IO_STATUS_BLOCK IoStatus;
    Status = ZwReadFile(FileHandle, NULL, NULL, NULL, &IoStatus, ReadBuffer, 
               NumberOfBytesToRead, ByteOffset, NULL);
    
    //ASSERT(NT_SUCCESS(Status));

    if(NumberOfBytesRead != NULL)
        *NumberOfBytesRead = (DWORD) IoStatus.Information;
    return Status;
}

INSTATUS InWriteFile(
  HANDLE FileHandle,
  PVOID WriteBuffer,
  DWORD NumberOfBytesToWrite,
  PLARGE_INTEGER ByteOffset,
  ULONG *NumberOfBytesWritten,
  LPOVERLAPPED lpOverlapped
)
{
    NTSTATUS Status;
    IO_STATUS_BLOCK IoStatus;

    Status = ZwWriteFile(FileHandle, NULL, NULL, NULL, &IoStatus, WriteBuffer, 
            NumberOfBytesToWrite, ByteOffset, NULL);

    if(NumberOfBytesWritten != NULL)
        *NumberOfBytesWritten = (DWORD) IoStatus.Information;
    return Status;
}

INSTATUS InSetFilePointer(
  HANDLE FileHandle,
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
            Status = ZwSetInformationFile(FileHandle, &IoStatus,  &PosInformation, 
                            sizeof(FILE_POSITION_INFORMATION),FilePositionInformation);
            break;
        }
    case FILE_END:
        {
            FILE_STANDARD_INFORMATION FileStandardInfo;
            Status = ZwQueryInformationFile(FileHandle, &IoStatus, &FileStandardInfo, sizeof(FILE_STANDARD_INFORMATION),
                        FileStandardInformation);
            
            //ASSERT(NT_SUCCESS(Status));

            PosInformation.CurrentByteOffset.QuadPart = FileStandardInfo.EndOfFile.QuadPart + DistanceToMove.QuadPart;
            Status = ZwSetInformationFile(FileHandle, &IoStatus,  &PosInformation, 
                            sizeof(FILE_POSITION_INFORMATION),FilePositionInformation);
            
            NewDistance->QuadPart = FileStandardInfo.EndOfFile.QuadPart + DistanceToMove.QuadPart;
            break;
        }
    case FILE_CURRENT:
        {
            Status = ZwQueryInformationFile(FileHandle, &IoStatus, &PosInformation, sizeof(FILE_POSITION_INFORMATION),
                            FilePositionInformation);

            PosInformation.CurrentByteOffset.QuadPart += DistanceToMove.QuadPart;
            
            Status = ZwSetInformationFile(FileHandle, &IoStatus,  &PosInformation, 
                            sizeof(FILE_POSITION_INFORMATION),FilePositionInformation);
            break;
        }
    }

    return Status;
}

INSTATUS InSetFilePointer(
  HANDLE FileHandle,
  PLARGE_INTEGER FileSize)
{
    FILE_STANDARD_INFORMATION FileStandardInfo;
    IO_STATUS_BLOCK IoStatus;
    INSTATUS Status = ZwQueryInformationFile(FileHandle, &IoStatus, &FileStandardInfo, sizeof(FILE_STANDARD_INFORMATION),
                FileStandardInformation);

    if (!NT_SUCCESS(Status))
        return Status;

    FileSize->QuadPart = FileStandardInfo.EndOfFile.QuadPart;

    return Status;
}

INSTATUS InCloseHandle(
  HANDLE FileHandle
)
{
    return ZwClose(FileHandle);
}

#ifdef UNICODE
#define InDeleteFile InDeleteFileW
#else
#define InDeleteFile InDeleteFileA
#endif

NTSTATUS
InDeleteFileW(PWCHAR FileNameW)
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
    if(NULL == DosFileNameU.Buffer) {
        Status = STATUS_NO_MEMORY;
        return Status;
    }
    DosFileNameU.Length = FileNameU.Length + DosDeviceNameU.Length + sizeof(WCHAR);
    DosFileNameU.MaximumLength = FileNameU.Length + DosDeviceNameU.Length + sizeof(WCHAR);
    

    
    RtlZeroMemory(DosFileNameU.Buffer, FileNameU.Length + DosDeviceNameU.Length + sizeof(WCHAR));
    
    RtlCopyUnicodeString(&DosFileNameU, &DosDeviceNameU);
    RtlAppendUnicodeStringToString(&DosFileNameU, &FileNameU);

    InitializeObjectAttributes(&ObjectAttributes, &DosFileNameU, OBJ_CASE_INSENSITIVE, NULL, NULL);

    Status = ZwCreateFile(&FileHandle, GENERIC_READ, &ObjectAttributes, &IoStatus, 
                        NULL, FILE_ATTRIBUTE_NORMAL,
                        FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE, FILE_OPEN,
                        FILE_DELETE_ON_CLOSE  | FILE_DEFAULT_CREATE_OPTIONS,
                        NULL, 0);
        
    
    ZwClose(FileHandle);
    
    RtlFreeUnicodeString(&DosFileNameU);
    return Status;
}

NTSTATUS
InDeleteFileA(PCHAR FileName)
{
    NTSTATUS Status;
    IO_STATUS_BLOCK IoStatus;
    UNICODE_STRING DosFileNameU;
    UNICODE_STRING DosDeviceNameU;
    UNICODE_STRING FileNameU;
    ANSI_STRING FileNameA;
    HANDLE  FileHandle;
    OBJECT_ATTRIBUTES ObjectAttributes;

    RtlInitAnsiString(&FileNameA, FileName);
    RtlAnsiStringToUnicodeString(&FileNameU, &FileNameA, TRUE);
    RtlInitUnicodeString(&DosDeviceNameU, DOS_DEVICES_PATH);
    
    DosFileNameU.Buffer = (PWSTR) ALLOC_MEMORY(FileNameU.Length + DosDeviceNameU.Length + sizeof(WCHAR), PagedPool);
    if(NULL == DosFileNameU.Buffer) {
        Status = STATUS_NO_MEMORY;
        return Status;
    }
    DosFileNameU.Length = FileNameU.Length + DosDeviceNameU.Length + sizeof(WCHAR);
    DosFileNameU.MaximumLength = FileNameU.Length + DosDeviceNameU.Length + sizeof(WCHAR);

   

    RtlZeroMemory(DosFileNameU.Buffer, FileNameU.Length + DosDeviceNameU.Length + sizeof(WCHAR));

    RtlCopyUnicodeString(&DosFileNameU, &DosDeviceNameU);
    RtlAppendUnicodeStringToString(&DosFileNameU, &FileNameU);

    InitializeObjectAttributes(&ObjectAttributes, &DosFileNameU, OBJ_CASE_INSENSITIVE , NULL, NULL);

    Status = ZwCreateFile(&FileHandle, GENERIC_READ, &ObjectAttributes, &IoStatus, 
                    NULL, FILE_ATTRIBUTE_NORMAL,
                    FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE, FILE_OPEN,
                    FILE_DELETE_ON_CLOSE  | FILE_DEFAULT_CREATE_OPTIONS,
                    NULL, 0);
    
    

    ZwClose(FileHandle);

    RtlFreeUnicodeString(&FileNameU);
    RtlFreeUnicodeString(&DosFileNameU);

    return Status;
}

NTSTATUS
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

NTSTATUS InReadFileRaw(PRAW_READ_HANDLE RawReadHandle,LARGE_INTEGER ByteOffset, ULONG ReadLength, PVOID ReadBuffer, PULONG BytesRead)
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
        InVolDbgPrint(DL_VV_WARNING, DM_VV_DEVICE_CONTROL, ("IoAllocateIrp Failed\n"));
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
        InVolDbgPrint(DL_VV_ERROR, DM_VV_DRIVER_INIT, ("IoAllocateMdl Failed:%x\n", Status));
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
        InVolDbgPrint(DL_VV_ERROR, DM_VV_DRIVER_INIT, ("MmProbeAndLockPages Failed:%x\n", Status));
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

NTSTATUS InCreateFileRaw(CHAR* FileName, PRAW_READ_HANDLE RawReadHandle)
{
    NTSTATUS Status = STATUS_SUCCESS;

    ASSERT(NULL != FileName);
    ASSERT('\0' == FileName[2]);
    
    FileName[0] = (char) toupper(FileName[0]);

    ANSI_STRING FileNameA;
    UNICODE_STRING FileNameU;
    WCHAR FileNameW[MAX_STRING_SIZE] = L"\\DosDevices\\Global\\";
    UNICODE_STRING DosFileNameU;

    RtlInitAnsiString(&FileNameA, FileName);
    RtlAnsiStringToUnicodeString(&FileNameU, &FileNameA, TRUE);

    RtlInitUnicodeString(&DosFileNameU, FileNameW);
    DosFileNameU.MaximumLength = MAX_STRING_SIZE * sizeof(WCHAR);
    RtlAppendUnicodeStringToString(&DosFileNameU, &FileNameU);

    Status = IoGetDeviceObjectPointer(&DosFileNameU, FILE_READ_ATTRIBUTES, &RawReadHandle->FileObject, &RawReadHandle->DeviceObject);
    RtlFreeUnicodeString(&FileNameU);
    return Status;
}

NTSTATUS InCloseFileRaw(PRAW_READ_HANDLE RawReadHandle)
{
    ObDereferenceObject(RawReadHandle->FileObject);
    RawReadHandle->DeviceObject = NULL;
    RawReadHandle->FileObject = NULL;
    return STATUS_SUCCESS;
}

#endif