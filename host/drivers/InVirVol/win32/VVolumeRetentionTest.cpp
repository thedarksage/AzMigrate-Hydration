#include <VVCommon.h>
#include <VVDriverContext.h>
#include <VVolumeRetentionTest.h>
#include <DiskChange.h>

#define INMAGE_KERNEL_MODE
#include <FileIO.h>

VOID
VVolumeRetentionProcessReadRequestTest(PDEVICE_EXTENSION DevExtension, PCOMMAND_STRUCT pCommand)
{
    PVOID           pReadBuffer;
    PIRP            Irp = pCommand->Cmd.Read.Irp;
    IO_STATUS_BLOCK IoStatus;
    NTSTATUS        Status = STATUS_SUCCESS;

    pReadBuffer = MmGetSystemAddressForMdlSafe(Irp->MdlAddress, NormalPagePriority);
    if (NULL == pReadBuffer) {
        Irp->IoStatus.Status = pCommand->CmdStatus = STATUS_INSUFFICIENT_RESOURCES;
        Irp->IoStatus.Information = 0;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        IoReleaseRemoveLock(&DevExtension->IoRemoveLock, Irp);
        return;
    }

    Status = ZwReadFile(DevExtension->hFile, NULL, NULL, NULL, &IoStatus, pReadBuffer, 
               pCommand->Cmd.Read.ulLength, &pCommand->Cmd.Read.ByteOffset, NULL);

    Irp->IoStatus.Status = pCommand->CmdStatus = Status;
    Irp->IoStatus.Information = IoStatus.Information;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    IoReleaseRemoveLock(&DevExtension->IoRemoveLock, Irp);
}

VOID
VVolumeRetentionProcessReadRequestDispatch(PDEVICE_EXTENSION DevExtension, PCOMMAND_STRUCT pCommand)
{
    PVOID           pReadBuffer;
    PIRP            Irp = pCommand->Cmd.Read.Irp;
    IO_STATUS_BLOCK IoStatus;
    NTSTATUS        Status = STATUS_SUCCESS;
    KIRQL           OldIrql;

    pReadBuffer = MmGetSystemAddressForMdlSafe(Irp->MdlAddress, NormalPagePriority);
    if (NULL == pReadBuffer) {
        Irp->IoStatus.Status = pCommand->CmdStatus = STATUS_INSUFFICIENT_RESOURCES;
        Irp->IoStatus.Information = 0;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        IoReleaseRemoveLock(&DevExtension->IoRemoveLock, Irp);
        return;
    }

    DISK_CHANGE Change;
    Change.ByteOffset = pCommand->Cmd.Read.ByteOffset;
    Change.Length = pCommand->Cmd.Read.ulLength;
    Change.DataSource = NULL;

    LIST_ENTRY SplitDiskChangeList;
    InitializeListHead(&SplitDiskChangeList);

    KeAcquireSpinLock(&DriverContext.Lock, &OldIrql);
    SplitDiskChange(&Change, &DevExtension->DiskChangeListHead, &SearchChangeInDBList, &SplitDiskChangeList);
    KeReleaseSpinLock(&DriverContext.Lock, OldIrql);
    
    while(!IsListEmpty(&SplitDiskChangeList))
    {
        PDISK_CHANGE CurChange = (PDISK_CHANGE) RemoveHeadList(&SplitDiskChangeList);
        if(CurChange->DataSource == NULL)
        {

            Status = ZwReadFile(DevExtension->hFile, NULL, NULL, NULL, &IoStatus, 
                (PCHAR)pReadBuffer + (CurChange->ByteOffset.QuadPart - pCommand->Cmd.Read.ByteOffset.QuadPart),
                CurChange->Length, &CurChange->ByteOffset, NULL);
            InVolDbgPrint(DL_VV_INFO, DM_VV_READ, ("Reading From Volume: ByteOffset:%I64x Length:%#x", CurChange->ByteOffset.QuadPart, 
                        CurChange->Length));
        }
        else
        {
            RtlCopyMemory((PCHAR)pReadBuffer + (CurChange->ByteOffset.QuadPart - pCommand->Cmd.Read.ByteOffset.QuadPart), 
                        CurChange->DataSource->GetData(CurChange->DataSource) ,CurChange->Length);
            InVolDbgPrint(DL_VV_INFO, DM_VV_READ, ("Reading From Memory: ByteOffset:%I64x Length:%#x", CurChange->ByteOffset.QuadPart, 
                        CurChange->Length));
        }

        if(CurChange->DataSource)
            ExFreePool(CurChange->DataSource);
        
        ExFreePool(CurChange);
    }

    Irp->IoStatus.Status = pCommand->CmdStatus = Status;
    Irp->IoStatus.Information = IoStatus.Information;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    IoReleaseRemoveLock(&DevExtension->IoRemoveLock, Irp);
}

VOID
VVolumeRetentionProcessWriteRequestDispatch(PDEVICE_EXTENSION DevExtension, PCOMMAND_STRUCT pCommand)
{
    NTSTATUS        Status = STATUS_SUCCESS;
    PVOID           pWriteBuffer;
    IO_STATUS_BLOCK IoStatus;
    PIRP            Irp = pCommand->Cmd.Write.Irp;
    KIRQL           OldIrql;

    Irp->IoStatus.Information = 0;
    Irp->IoStatus.Status = Status;

    if(pCommand->Cmd.Write.ulLength != 0)
    {
        pWriteBuffer = MmGetSystemAddressForMdlSafe(Irp->MdlAddress, NormalPagePriority);
        if (NULL == pWriteBuffer) {
            Irp->IoStatus.Status = pCommand->CmdStatus = STATUS_INSUFFICIENT_RESOURCES;
            Irp->IoStatus.Information = 0;
            goto Cleanup_and_exit;
        }

        PDISK_CHANGE DiskChange  = NULL;
        DiskChange = (PDISK_CHANGE)ExAllocatePoolWithTag(NonPagedPool, sizeof(DISK_CHANGE), VVTAG_GENERIC_NONPAGED);
        if (NULL == DiskChange) {
            Irp->IoStatus.Status = pCommand->CmdStatus = STATUS_INSUFFICIENT_RESOURCES;
            Irp->IoStatus.Information = 0;
            goto Cleanup_and_exit;
        }
        DiskChange->ByteOffset   = pCommand->Cmd.Write.ByteOffset;
        DiskChange->Length       = pCommand->Cmd.Write.ulLength;
        DiskChange->DataSource   = (PDATA_SOURCE) ExAllocatePoolWithTag(NonPagedPool, sizeof(DATA_SOURCE), VVTAG_GENERIC_NONPAGED);;
        DiskChange->DataSource->GetData   = GetMemoryBufferData;
        DiskChange->DataSource->Type    = DATA_SOURCE_MEMORY_BUFFER;
        DiskChange->DataSource->u.MemoryBuffer.Address  = (PCHAR)ExAllocatePoolWithTag(PagedPool, pCommand->Cmd.Write.ulLength, VVTAG_GENERIC_PAGED);
        RtlCopyMemory(DiskChange->DataSource->u.MemoryBuffer.Address, pWriteBuffer, pCommand->Cmd.Write.ulLength);

        KeAcquireSpinLock(&DriverContext.Lock, &OldIrql);
        InsertHeadList(&DevExtension->DiskChangeListHead, &DiskChange->ListEntry);
        KeReleaseSpinLock(&DriverContext.Lock, OldIrql);

        Irp->IoStatus.Status = pCommand->CmdStatus = Status;
        Irp->IoStatus.Information = pCommand->Cmd.Write.ulLength;
    }
Cleanup_and_exit:
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    IoReleaseRemoveLock(&DevExtension->IoRemoveLock, Irp);
    return;
}

NTSTATUS MountVolume(PVOID InputBuffer, ULONG Length, HANDLE *hFile, LONGLONG *VolumeSize)
{
    size_t cbInputSize;
    WCHAR *FileName = NULL;
    ULONG cbFileNameSize = 0;
    IO_STATUS_BLOCK IoStatus;
    FILE_STANDARD_INFORMATION FileStandardInfo;
    NTSTATUS Status;

    InputBuffer = (PCHAR)InputBuffer;

    *VolumeSize = *(PLONGLONG)InputBuffer;

    InputBuffer = (PCHAR)InputBuffer + sizeof(LONGLONG);

    Length = Length - sizeof(LONGLONG);

    Status = RtlStringCbLengthW((NTSTRSAFE_PCWSTR)InputBuffer, Length, &cbInputSize);
    if (STATUS_SUCCESS != Status) {
        return Status;
    }

    // Add null terminator to cbInputSize
    cbInputSize += sizeof(WCHAR);

    cbFileNameSize = cbInputSize;

    FileName = (PWCHAR)ExAllocatePoolWithTag(NonPagedPool, cbFileNameSize, VVTAG_GENERIC_NONPAGED);
    if (NULL == FileName) {
        Status = STATUS_NO_MEMORY;
        return Status;
    }
    
    Status = RtlStringCbCopyW(FileName, cbFileNameSize, (NTSTRSAFE_PCWSTR)InputBuffer);
    ASSERT(STATUS_SUCCESS == Status);
    if (Status != STATUS_SUCCESS) {
        Status = STATUS_UNSUCCESSFUL;
        return Status;
    }

    Status = InCreateFileW(FileName, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, FILE_OPEN, FILE_ATTRIBUTE_NORMAL, NULL, NULL, hFile);
    
    if (!NT_SUCCESS(Status))
        return Status;

    /*
    Status = ZwQueryInformationFile(*hFile, &IoStatus, &FileStandardInfo, sizeof(FILE_STANDARD_INFORMATION),
                    FileStandardInformation);

        if (!NT_SUCCESS(Status))
        return Status;
    */
    //*VolumeSize = FileStandardInfo.EndOfFile.QuadPart;
    ExFreePool(FileName);

    return Status;
}
