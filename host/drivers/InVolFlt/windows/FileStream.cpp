// this class handles opening and closing file objects

//
// Copyright InMage Systems 2004
//

#include "global.h"
#include "misc.h"

FileStream::FileStream()
{
TRC
    m_iosbCreateStatus.Information = 0;
    m_iosbCreateStatus.Status = STATUS_OPEN_FAILED;

    fileHandle = 0; 
    fileObject = NULL;
    osDevice = NULL;
}

FileStream::~FileStream()
{
TRC
}

NTSTATUS
FileStream::Open(
    PUNICODE_STRING unicodeName,
    ULONG32 accessMode,
    ULONG32 createDisposition,
    ULONG32 createOptions,
    ULONG32 sharingMode,
    LONGLONG allocationSize
    )
{
TRC
    NTSTATUS status;
    OBJECT_ATTRIBUTES ObjectAttributes;

    USHORT CompInfo = COMPRESSION_FORMAT_NONE;
    IO_STATUS_BLOCK IoStatusBlock;
    PKEVENT Event = NULL;
    HANDLE EventHandle = 0;
    bool error = true;
    bool open_file = false;
    OBJECT_ATTRIBUTES EventAttributes;
    MARK_HANDLE_INFO markHandleInfo = { 0 };
    HANDLE volObjHandle = NULL;

   InitializeObjectAttributes ( &ObjectAttributes,
                                unicodeName,
                                OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE,
                                NULL,
                                NULL );
   
   InitializeObjectAttributes (
       &EventAttributes,
       NULL, OBJ_KERNEL_HANDLE, NULL, NULL);

   status = ZwCreateEvent(&EventHandle, 0, &EventAttributes, SynchronizationEvent, FALSE);

   if (!NT_SUCCESS(status)) {
        InVolDbgPrint( DL_ERROR, DM_FILE_OPEN, 
            ("FileStream::Open: Error in Create Event Status Code %x\n", status));
        goto ERROR;
   }

   status = ObReferenceObjectByHandle(EventHandle, 0, *ExEventObjectType, KernelMode, (PVOID *)&Event, NULL);

   if (!NT_SUCCESS(status)) {
        InVolDbgPrint( DL_ERROR, DM_FILE_OPEN, 
            ("FileStream::Open: Error in ObReferenceObjectByHandle for Event Status Code %x\n", status));
        goto ERROR;
   }

   status = ZwCreateFile( &fileHandle,
                            accessMode,  
                            &ObjectAttributes,
                            &m_iosbCreateStatus,
                            (PLARGE_INTEGER)&allocationSize,
                            FILE_ATTRIBUTE_NORMAL,
                            sharingMode,
                            createDisposition,
                            createOptions,
                            NULL,  // eabuffer
                            0 );   // ealength

   if (!NT_SUCCESS(status)) {
        InVolDbgPrint( DL_ERROR, DM_FILE_OPEN, 
            ("FileStream::Open: Error in Create File %wZ Status Code %x\n", unicodeName, status));
        goto ERROR;
   }

   open_file = true;

   status = ObReferenceObjectByHandle(
                    fileHandle,
                    0,
                    *IoFileObjectType,
                    KernelMode,
                    (PVOID *) &fileObject,
                    NULL);

   if (!NT_SUCCESS(status)) {
        InVolDbgPrint( DL_ERROR, DM_FILE_OPEN, 
            ("FileStream::Open: Error in ObReferenceObjectByHandle File %wZ Status Code %x\n", unicodeName, status));
        goto ERROR;
   }

   osDevice = IoGetRelatedDeviceObject(fileObject);

   status = GetVolumeHandle(osDevice, false, &volObjHandle, NULL);

   if (status != STATUS_SUCCESS) {
       InVolDbgPrint(DL_ERROR, DM_FILE_OPEN,
           ("FileStream::Open: Error in opening volume handle for devObj %#p of File %wZ Status Code %x\n",
           osDevice, unicodeName, status));

       goto ERROR;
   }

   //Make Compression OFF when file is opened. Else FileSystem would create newer IRP for AsyncObject Writes.
   status = ZwFsControlFile(
        fileHandle,
        EventHandle,
        NULL,
        NULL,
        &IoStatusBlock,
        FSCTL_SET_COMPRESSION,
        &CompInfo,
        sizeof(USHORT), 
        NULL, 
        0);

    if (status == STATUS_PENDING) {
        /* Wait for completion */
        KeWaitForSingleObject(Event,
            Executive,
            KernelMode,
            FALSE,
            NULL);

        status = IoStatusBlock.Status;
    }

    if (!NT_SUCCESS(status)) {
        InDskFltWriteEvent(INDSKFLT_ERROR_FILE_OPEN_COMPRESS_OFF, unicodeName->Buffer, status);

        InVolDbgPrint(DL_ERROR, DM_FILE_OPEN,
            ("FileStream::Open: Error in Setting compress off for File %wZ Status Code %x\n",
            unicodeName, status));

        goto ERROR;
    }

    markHandleInfo.UsnSourceInfo = USN_SOURCE_DATA_MANAGEMENT;
    markHandleInfo.VolumeHandle = volObjHandle;
    markHandleInfo.HandleInfo = MARK_HANDLE_PROTECT_CLUSTERS;

    KeResetEvent(Event);

    status = ZwFsControlFile(
        fileHandle,
        EventHandle,
        NULL,
        NULL,
        &IoStatusBlock,
        FSCTL_MARK_HANDLE,
        &markHandleInfo,
        sizeof(markHandleInfo),
        NULL,
        0);

    if (status == STATUS_PENDING) {
        /* Wait for completion */
        KeWaitForSingleObject(Event,
            Executive,
            KernelMode,
            FALSE,
            NULL);

        status = IoStatusBlock.Status;
    }

    if (!NT_SUCCESS(status)) {
        InDskFltWriteEvent(INDSKFLT_ERROR_FILE_OPEN_MARK_PROTECT_CLUSTERS, unicodeName->Buffer, status);

        InVolDbgPrint(DL_ERROR, DM_FILE_OPEN,
            ("FileStream::Open: Error in marking handle to protect clusters of File %wZ Status Code %x\n",
            unicodeName, status));

        goto ERROR;
    }

    error = false;

ERROR: 
    SafeCloseHandle(volObjHandle);

   if (EventHandle) {
        if (NULL != Event) {
            ObDereferenceObject(Event);
        }
        ZwClose(EventHandle);
   }

   if (error) {
       NT_ASSERT(status != STATUS_SUCCESS);

        if (open_file) {
            if (fileObject) {
                ObDereferenceObject(fileObject);
            }
            ZwClose(fileHandle);		
        }

        fileHandle = 0;
        fileObject = NULL;
        osDevice = NULL;
   }

   return status;
}

NTSTATUS FileStream::SetFileSize(LONGLONG llFileSize)
{
    FILE_END_OF_FILE_INFORMATION    EndOfFile;
    NTSTATUS    Status;
    IO_STATUS_BLOCK IoStatusBlock;

    EndOfFile.EndOfFile.QuadPart = llFileSize;
    Status = ZwSetInformationFile(fileHandle, &IoStatusBlock, &EndOfFile, sizeof(FILE_END_OF_FILE_INFORMATION), FileEndOfFileInformation);

    return Status;
}

NTSTATUS FileStream::Close()
{
TRC

NTSTATUS status;

    status = STATUS_SUCCESS;

    if (fileObject)
        ObDereferenceObject(fileObject);
    if (fileHandle)
        status = ZwClose(fileHandle);

    fileHandle = 0;
    fileObject = NULL;
    osDevice = NULL;

    return status;
}

NTSTATUS FileStream::DeleteOnClose()
{
NTSTATUS status;
FILE_DISPOSITION_INFORMATION dispositionInfo;
IO_STATUS_BLOCK     iosbStatus;

    if (0 == fileHandle) {
        return STATUS_INVALID_HANDLE;
    }

    RtlZeroMemory(&dispositionInfo, sizeof(dispositionInfo));

    dispositionInfo.DeleteFile = TRUE;

    status = ZwSetInformationFile(fileHandle, 
                                  &iosbStatus, 
                                  &dispositionInfo, 
                                  sizeof(dispositionInfo), 
                                  FileDispositionInformation);

    return status;
}

/*
// get the file size
NTSTATUS FileStream::Size(LONGLONG * fileEof)
{
NTSTATUS status;
FILE_STANDARD_INFORMATION standardInfo;
IO_STATUS_BLOCK     iosbStatus;

    if (0 == fileHandle) {
        return STATUS_INVALID_HANDLE;
    }

    RtlZeroMemory(&standardInfo, sizeof(standardInfo));

    status = ZwQueryInformationFile(fileHandle, 
                                  &iosbStatus, 
                                  &standardInfo, 
                                  sizeof(standardInfo), 
                                  FileStandardInformation);
    if (NT_SUCCESS(status)) {
        *fileEof = standardInfo.EndOfFile.QuadPart;
    } else {
        *fileEof = 0;
    }

    return status;
}
*/
// decide if an IRP is on this file
bool FileStream::IsOurWriteAtSizeAndOffset(PIRP Irp, ULONG32 size, ULONG64 offset)
{
TRC
PIO_STACK_LOCATION topIrpStack;
bool  IsOurWrite;

    // walk down the irp and find the originating file in the first irp stack slot
    topIrpStack = (PIO_STACK_LOCATION)((PCHAR)Irp + sizeof(IRP)) + (Irp->StackCount - 1);

    IsOurWrite = ((topIrpStack->FileObject == fileObject) &&
                (topIrpStack->MajorFunction == IRP_MJ_WRITE) &&
                (topIrpStack->Parameters.Write.Length == size) &&
                ((ULONG64)topIrpStack->Parameters.Write.ByteOffset.QuadPart == offset));

    // Since this function is protected by DriverContext.WriteFilteringObject, below error might be logged for subsequent disk
    // for which bitmap writes are still in progress, need a rate control 
    if ((!IsOurWrite) && (DriverContext.MaxLogForLearnIrpFailures < MAX_COUNT_FOR_LEARNIRP_FAILURE)) {
        InDskFltWriteEvent(INDSKFLT_ERROR_LEARN_PHYSICAL_IO_VALIDATION1,
                            topIrpStack->FileObject,
                            fileObject,
                            topIrpStack->MajorFunction,
                            topIrpStack->Parameters.Write.Length,
                            size,
                            topIrpStack->Parameters.Write.ByteOffset.QuadPart,
                            offset);
        DriverContext.MaxLogForLearnIrpFailures++;
    }
    return IsOurWrite;
}

void * __cdecl  FileStream::operator new(size_t size) {
	return (PVOID)malloc((ULONG32)size, (ULONG32)TAG_FILESTREAM_OBJECT, NonPagedPool);
}
