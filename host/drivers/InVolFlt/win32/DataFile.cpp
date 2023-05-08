#include <global.h>
#include "DataFile.h"
#include "AsyncIORequest.h"
#include "HelperFunctions.h"
#include "misc.h"

extern "C" {
NTSYSAPI
NTSTATUS
NTAPI
ZwQueryDirectoryFile(
    __in HANDLE FileHandle,
    __in_opt HANDLE Event,
    __in_opt PIO_APC_ROUTINE ApcRoutine,
    __in_opt PVOID ApcContext,
    __out PIO_STATUS_BLOCK IoStatusBlock,
    __out_bcount(Length) PVOID FileInformation,
    __in ULONG Length,
    __in FILE_INFORMATION_CLASS FileInformationClass,
    __in BOOLEAN ReturnSingleEntry,
    __in_opt PUNICODE_STRING FileName,
    __in BOOLEAN RestartScan
    );

NTSYSAPI
NTSTATUS
NTAPI
ZwCreateEvent (
    OUT PHANDLE EventHandle,
    IN ACCESS_MASK DesiredAccess,
    IN POBJECT_ATTRIBUTES ObjectAttributes OPTIONAL,
    IN EVENT_TYPE EventType,
    IN BOOLEAN InitialState
    );

}

CDataFile::CDataFile() : m_File()
{
    m_ullFileOffset = 0;
    m_bFileClosed = false;
    m_bFileOpenSucceeded = false;
    m_bLastWriteOccured = false;
}

CDataFile::~CDataFile()
{
    if (m_bFileOpenSucceeded && !m_bFileClosed) {
        m_File.Close();
        m_bFileClosed;
    }
}

NTSTATUS
CDataFile::Open(PUNICODE_STRING InputFileName, ULONGLONG ullAllocationSize, ULONG ulBufferSize)
{
    NTSTATUS        Status;
    UNICODE_STRING  usFileName;
    PUNICODE_STRING FileName = NULL;

    InMageInitUnicodeString(&usFileName, NULL, 0, 0);

    ASSERT(false == m_bFileOpenSucceeded);
    if (m_bFileOpenSucceeded) {
        Close();
    }

    do {
        if ((TRUE == IS_DRIVE_LETTER(InputFileName->Buffer[0])) && (L':' == InputFileName->Buffer[1])) {
            Status = InMageCopyUnicodeString(&usFileName, DOS_DEVICES_PATH);
            if (!NT_SUCCESS(Status)) {
                break;
            }

            Status = InMageAppendUnicodeString(&usFileName, InputFileName);
            if (!NT_SUCCESS(Status)) {
                break;
            }

            FileName = &usFileName;
        } else {
            FileName = InputFileName;
        }

        //Fix for bug 24005
        Status = m_File.Open(FileName, FILE_WRITE_DATA, FILE_CREATE, FILE_NON_DIRECTORY_FILE | FILE_NO_INTERMEDIATE_BUFFERING, 0, ullAllocationSize);
        if (STATUS_SUCCESS != Status) {
            break;
        }

        m_bFileOpenSucceeded = true;
        m_bFileClosed = false;

    } while ( 0 );

    InMageFreeUnicodeString(&usFileName);

    if (!NT_SUCCESS(Status)) {
        InVolDbgPrint(DL_ERROR, DM_DATA_FILTERING, ("CDataFile::Delete failed with Status = %#x\n", Status));
    }

    return Status;
}

NTSTATUS
CDataFile::Write(PUCHAR Buffer, ULONG BufferSize, ULONG ulLength)
{
    NTSTATUS    Status = STATUS_SUCCESS;
    ULONG       ulWriteSize = ulLength;
    ULONG       ulOffset = 0;

    ASSERT(NULL != Buffer);
    ASSERT(0 != ulLength);
    ASSERT(m_bFileOpenSucceeded);
    ASSERT(false == m_bLastWriteOccured);
    ASSERT(0 == (BufferSize % FOUR_K_SIZE));

    if (!m_bFileOpenSucceeded)
        return STATUS_OPEN_FAILED;

    if (m_bLastWriteOccured)
        return STATUS_UNSUCCESSFUL;
    
    if ((NULL == Buffer) || (0 == ulLength)) {
        return Status;
    }

    // If the buffer is the last buffer, ceil the buffer to 4K boundary
    if (BufferSize != ulLength) {
        ulWriteSize = (ulWriteSize + FOUR_K_SIZE - 1) / FOUR_K_SIZE ;
        ulWriteSize = ulWriteSize * FOUR_K_SIZE;
        m_bLastWriteOccured = true;
    }

    AsyncIORequest  *Request = new AsyncIORequest(&m_File);
    if (Request != NULL) {
        Status = Request->SyncWrite(Buffer, ulWriteSize, m_ullFileOffset); // initiate the write
        InVolDbgPrint(DL_INFO, DM_DATA_FILTERING, ("CDataFile::Write returned with Status = %#x\n", Status));
        ASSERT(STATUS_PENDING != Status);
        Request->Release();
        Request = NULL;
    } else {
        Status = STATUS_NO_MEMORY;
    }

    if (!NT_SUCCESS(Status)) {
        return Status;
    }

    m_ullFileOffset += ulLength;

    return Status;
}

NTSTATUS
CDataFile::Close()
{
    NTSTATUS    Status = STATUS_SUCCESS;

    if (!m_bFileOpenSucceeded) {
        return STATUS_SUCCESS;
    }

    // Set the file size to match the offset.
    Status = m_File.SetFileSize(m_ullFileOffset);
    ASSERT(STATUS_SUCCESS == Status);

    // Close the file
    Status = m_File.Close();
    ASSERT(STATUS_SUCCESS == Status);

    m_bFileClosed = true;
    m_bFileOpenSucceeded = false;
    m_bLastWriteOccured = false;
    m_ullFileOffset = 0;
    return Status;
}

NTSTATUS
CDataFile::Delete()
{
    NTSTATUS    Status;

    if (!m_bFileOpenSucceeded) {
        return STATUS_OPEN_FAILED;
    }

    Status = m_File.DeleteOnClose();
    if (!NT_SUCCESS(Status)) {
        InVolDbgPrint(DL_ERROR, DM_DATA_FILTERING, ("CDataFile::Delete failed with Status = %#x\n", Status));
    } 

    return Status;
}

NTSTATUS
CDataFile::DeleteFile(PUNICODE_STRING InputFileName)
{
    NTSTATUS        Status;
    FileStream      File;
    UNICODE_STRING  usFileName;
    PUNICODE_STRING FileName = NULL;

    InMageInitUnicodeString(&usFileName, NULL, 0, 0);

    if (NULL == InputFileName) {
        return STATUS_INVALID_PARAMETER;
    }

    if (InputFileName->Length <= 0x03) {
        return STATUS_INVALID_PARAMETER;
    }

    do {
        if ((TRUE == IS_DRIVE_LETTER(InputFileName->Buffer[0])) && (L':' == InputFileName->Buffer[1])) {
            Status = InMageCopyUnicodeString(&usFileName, DOS_DEVICES_PATH);
            if (!NT_SUCCESS(Status)) {
                break;
            }

            Status = InMageAppendUnicodeString(&usFileName, InputFileName);
            if (!NT_SUCCESS(Status)) {
                break;
            }

            FileName = &usFileName;
        } else {
            FileName = InputFileName;
        }

        //Fix for bug 24005
        Status = File.Open(FileName, 
                           DELETE, 
                           FILE_OPEN,
                           FILE_NON_DIRECTORY_FILE | FILE_SYNCHRONOUS_IO_ALERT,
                           FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                           0);
        if (!NT_SUCCESS(Status)) {
            LogFileOpenFailure(Status, FileName, __WFILE__, __LINE__);
            break;
        }

        Status = File.DeleteOnClose();
        if (!NT_SUCCESS(Status)) {
            LogDeleteOnCloseFailure(Status, FileName, __WFILE__, __LINE__);
        }

        File.Close(); // ignore return Status

    } while ( 0 );

    InMageFreeUnicodeString(&usFileName);

    if (!NT_SUCCESS(Status)) {
        InVolDbgPrint(DL_ERROR, DM_DATA_FILTERING, ("CDataFile::Delete failed with Status = %#x\n", Status));
    } 

    return Status;
}

NTSTATUS
CDataFile::DeleteDataFilesInDirectory(
    PUNICODE_STRING Dir
    )
{
    NTSTATUS            Status;
    UNICODE_STRING      FileName, RegExp, TempDir;
    OBJECT_ATTRIBUTES   ObjectAttributes;
    PUCHAR              Buffer = NULL;
    const ULONG         ulBufferLength = 0x1000; // 4K
    HANDLE              hDir, hEvent;
    IO_STATUS_BLOCK     IoStatus;
    BOOLEAN             RestartScan = TRUE;
    PKEVENT             Event = NULL;

    Status = ZwCreateEvent(&hEvent, EVENT_ALL_ACCESS, NULL, SynchronizationEvent, FALSE);
    if (STATUS_SUCCESS != Status) {
        return Status;
    }

    Status = ObReferenceObjectByHandle(hEvent, EVENT_ALL_ACCESS, *ExEventObjectType, KernelMode, (PVOID *)&Event, NULL);
    if (STATUS_SUCCESS != Status) {
        ZwClose(hEvent);
        return Status;
    }

    // First open the directory.
    InMageInitUnicodeString(&TempDir, NULL, 0, 0);
    InMageInitUnicodeString(&FileName, NULL, 0, 0);

    InMageCopyDosPathAsNTPath(&TempDir, Dir);

    // Remove Trailing backslash
    while (TempDir.Length && (L'\\' == TempDir.Buffer[TempDir.Length/sizeof(WCHAR) - 1])) {
        TempDir.Length -= sizeof(WCHAR);
    }

    InitializeObjectAttributes ( &ObjectAttributes,
                                    &TempDir,
                                    OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE,
                                    NULL,
                                    NULL );
    Status = ZwOpenFile(&hDir, FILE_LIST_DIRECTORY, &ObjectAttributes, &IoStatus, FILE_SHARE_READ | FILE_SHARE_WRITE, FILE_DIRECTORY_FILE);
    if (STATUS_SUCCESS != Status) {
        // Failed to open directory, do not have anything to cleanup
        return STATUS_SUCCESS;
    }

    // Succeeded in opening directory.
    do {
        Buffer = (PUCHAR)ExAllocatePoolWithTag(PagedPool, ulBufferLength, TAG_GENERIC_PAGED);
        if (NULL == Buffer) {
            Status = STATUS_NO_MEMORY;
            break;
        }

        RtlInitUnicodeString(&RegExp, L"pre_completed_diff*.dat");
        do {
            IoStatus.Information = 0;
            KeResetEvent(Event);
            Status = ZwQueryDirectoryFile(hDir, hEvent, NULL, NULL, &IoStatus, Buffer, ulBufferLength, FileNamesInformation, FALSE, &RegExp, RestartScan);
            if (STATUS_PENDING == Status) {
                Status = KeWaitForSingleObject(Event, Executive, KernelMode, FALSE, NULL);
                ASSERT(STATUS_SUCCESS == Status);
                Status = IoStatus.Status;
            }

            if ((STATUS_SUCCESS == Status) && (0 != IoStatus.Information)) {
                ULONG                       NextEntryOffset = 0;
                // have file names.
                while (NextEntryOffset < IoStatus.Information) {
                    PFILE_NAMES_INFORMATION FileNameInfo = (PFILE_NAMES_INFORMATION)(Buffer + NextEntryOffset);

                    InMageCopyDosPathAsNTPath(&FileName, Dir);
                    InMageAppendUnicodeString(&FileName, FileNameInfo->FileName, FileNameInfo->FileNameLength);
                    InVolDbgPrint(DL_INFO, DM_DATA_FILTERING, ("DeleteDataFilesInDirectory: Filename = %wZ\n", &FileName));
                    CDataFile::DeleteFile(&FileName);

                    NextEntryOffset += FileNameInfo->NextEntryOffset;
                    if (!FileNameInfo->NextEntryOffset) {
                        // Terminating condition. NextEntryOffset of last entry is Zero
                        break;
                    }
                }
            }
            RestartScan = FALSE;
        } while (STATUS_SUCCESS == Status);

        Status = STATUS_SUCCESS;
    } while ( 0 );

    if (NULL != Buffer) {
        ExFreePoolWithTag(Buffer, TAG_GENERIC_PAGED);
    }

    if (NULL != Event) {
        ObDereferenceObject(Event);
    }

    ZwClose(hDir);
    ZwClose(hEvent);
    InMageFreeUnicodeString(&TempDir);
    InMageFreeUnicodeString(&FileName);
    return Status;
}

