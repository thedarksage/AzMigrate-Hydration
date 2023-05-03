
#define INITGUID
#include "FltFunc.h"
#include "VContext.h"
#include "ListNode.h"
#include <ntddscsi.h>
#include <scsi.h>
//#include <srb.h>

#ifdef _WIN64
NTSTATUS
DeviceIoControlSCSIPassThrough(
    _In_    PDEVICE_OBJECT  DeviceObject,
    _Inout_ PIRP            Irp
)
/*
Routine Description:
    Handler for SCSI direct writes.
        Applications can use direct writes SCSI command to write to disk.
        ASR filter doesnt support scsi direct writes.
        This function traps scsi direct writes and logs.
        This logging will help us to rule out any DI issue due to scsi write.

    Arguments:
        DeviceObject            Indskflt Disk device object.
        Irp                     Irp that contains user mode request.

Return Value:
    NTSTATUS.
*/
{
    PCDB                      Cdb = NULL;
    ULONG                     CdbLength = 0;
    PIO_STACK_LOCATION        IrpSp = IoGetCurrentIrpStackLocation(Irp);
    PVOID                     PassThrough = NULL;
    ULONGLONG                 SectorCount = 0;
    LARGE_INTEGER             SectorOffset = { 0 };
    ULONG                     Size = 0;
    NTSTATUS                  Status = STATUS_SUCCESS;
    ULONG                       ulIoControlCode;
    LARGE_INTEGER               liTimeStamp = { 0 };
    LONGLONG                    llDelayInLogging = ((LONGLONG)LOGGING_DELAY_SCSI_DIRECT_CMD_SECS * (LONGLONG)HUNDRED_NANO_SECS_IN_SEC);
    PDEVICE_EXTENSION           pDeviceExtension = (PDEVICE_EXTENSION)DeviceObject->DeviceExtension;
    PDEV_CONTEXT                pDevContext = pDeviceExtension->pDevContext;

    SectorOffset.QuadPart = 0;

    ulIoControlCode = IrpSp->Parameters.DeviceIoControl.IoControlCode;
    PassThrough = Irp->AssociatedIrp.SystemBuffer;

    BOOLEAN is32BitProcess = IoIs32bitProcess(Irp);

    if (IOCTL_SCSI_PASS_THROUGH_EX == ulIoControlCode) {
        Size = (is32BitProcess) ? sizeof(SCSI_PASS_THROUGH32_EX) : sizeof(SCSI_PASS_THROUGH_EX);
    }
    else if (IOCTL_SCSI_PASS_THROUGH_DIRECT_EX == ulIoControlCode) {
        Size = (is32BitProcess) ? sizeof(SCSI_PASS_THROUGH_DIRECT32_EX) : sizeof(SCSI_PASS_THROUGH_DIRECT_EX);
    }
    else if (IOCTL_SCSI_PASS_THROUGH == ulIoControlCode) {
        Size = (is32BitProcess) ? sizeof(SCSI_PASS_THROUGH32) : sizeof(SCSI_PASS_THROUGH);
    }
    else if (IOCTL_SCSI_PASS_THROUGH_DIRECT == ulIoControlCode) {
        Size = (is32BitProcess) ? sizeof(SCSI_PASS_THROUGH_DIRECT32) : sizeof(SCSI_PASS_THROUGH_DIRECT);
    }
    else {
        Status = STATUS_NOT_SUPPORTED;
        goto cleanup;
    }

    if (IrpSp->Parameters.DeviceIoControl.InputBufferLength < Size) {
        Status = STATUS_INVALID_PARAMETER;
        goto cleanup;
    }

    if (IOCTL_SCSI_PASS_THROUGH_EX == ulIoControlCode) {
        if (is32BitProcess) {
            CdbLength = ((PSCSI_PASS_THROUGH32_EX)PassThrough)->CdbLength;
            Cdb = (PCDB)((PSCSI_PASS_THROUGH32_EX)PassThrough)->Cdb;
            Size = FIELD_OFFSET(SCSI_PASS_THROUGH32_EX, Cdb[CdbLength]);
        }
        else {
            CdbLength = ((PSCSI_PASS_THROUGH_EX)PassThrough)->CdbLength;
            Cdb = (PCDB)((PSCSI_PASS_THROUGH_EX)PassThrough)->Cdb;
            Size = FIELD_OFFSET(SCSI_PASS_THROUGH_EX, Cdb[CdbLength]);
        }
    }
    else if (IOCTL_SCSI_PASS_THROUGH_DIRECT_EX == ulIoControlCode) {
        if (is32BitProcess) {
            CdbLength = ((PSCSI_PASS_THROUGH_DIRECT32_EX)PassThrough)->CdbLength;
            Cdb = (PCDB)((PSCSI_PASS_THROUGH_DIRECT32_EX)PassThrough)->Cdb;
            Size = FIELD_OFFSET(SCSI_PASS_THROUGH_DIRECT32_EX, Cdb[CdbLength]);
        }
        else {
            CdbLength = ((PSCSI_PASS_THROUGH_DIRECT_EX)PassThrough)->CdbLength;
            Cdb = (PCDB)((PSCSI_PASS_THROUGH_DIRECT_EX)PassThrough)->Cdb;
            Size = FIELD_OFFSET(SCSI_PASS_THROUGH_DIRECT_EX, Cdb[CdbLength]);
        }
    }
    else if (IOCTL_SCSI_PASS_THROUGH == ulIoControlCode) {
        if (is32BitProcess) {
            CdbLength = ((PSCSI_PASS_THROUGH32)PassThrough)->CdbLength;
            Cdb = (PCDB)((PSCSI_PASS_THROUGH32)PassThrough)->Cdb;
        }
        else {
            CdbLength = ((PSCSI_PASS_THROUGH)PassThrough)->CdbLength;
            Cdb = (PCDB)((PSCSI_PASS_THROUGH)PassThrough)->Cdb;
        }
    }
    else if (IOCTL_SCSI_PASS_THROUGH_DIRECT == ulIoControlCode) {
        if (is32BitProcess) {
            CdbLength = ((PSCSI_PASS_THROUGH_DIRECT32)PassThrough)->CdbLength;
            Cdb = (PCDB)((PSCSI_PASS_THROUGH_DIRECT32)PassThrough)->Cdb;
        }
        else {
            CdbLength = ((PSCSI_PASS_THROUGH_DIRECT)PassThrough)->CdbLength;
            Cdb = (PCDB)((PSCSI_PASS_THROUGH_DIRECT)PassThrough)->Cdb;
        }
    }

    if (IrpSp->Parameters.DeviceIoControl.InputBufferLength < Size) {
        Status = STATUS_INVALID_PARAMETER;
        goto cleanup;
    }

    switch (Cdb->AsByte[0]) {
    case SCSIOP_WRITE_SAME:
    case SCSIOP_WRITE_VERIFY:
        if (CdbLength < CDB10GENERIC_LENGTH) {
            Status = STATUS_INVALID_PARAMETER;
            goto cleanup;
        }

        SectorOffset.QuadPart = (ULONGLONG)
            (Cdb->CDB10.LogicalBlockByte0 << 24 |
                Cdb->CDB10.LogicalBlockByte1 << 16 |
                Cdb->CDB10.LogicalBlockByte2 << 8 |
                Cdb->CDB10.LogicalBlockByte3);

        SectorCount = (ULONGLONG)
            (Cdb->CDB10.TransferBlocksMsb << 8 |
                Cdb->CDB10.TransferBlocksLsb);
        break;

    case SCSIOP_WRITE_SAME16:
    case SCSIOP_WRITE_VERIFY16:
        if (CdbLength < 16) {
            Status = STATUS_INVALID_PARAMETER;
            goto cleanup;
        }

        REVERSE_BYTES_QUAD(&SectorOffset.QuadPart,
            Cdb->CDB16.LogicalBlock);

        REVERSE_BYTES(&SectorCount,
            Cdb->CDB16.TransferLength);
        break;

    default:
        goto cleanup;
    }

    KeQuerySystemTime(&liTimeStamp);


    if ((liTimeStamp.QuadPart - DriverContext.liLastScsiDirectCmdTS.QuadPart) > llDelayInLogging) {
        InterlockedExchange64(&DriverContext.liLastScsiDirectCmdTS.QuadPart, liTimeStamp.QuadPart);
        InDskFltWriteEventWithDevCtxt(INDSKFLT_ERROR_SCSI_DIRECT_WRITE, pDevContext, SectorOffset.QuadPart, SectorCount);
    }

cleanup:
    return Status;
}


NTSTATUS
DeviceIoControlStoragePersistentReserveOut(
    _In_    PDEVICE_OBJECT  DeviceObject,
    _Inout_ PIRP            Irp,
    IN BOOLEAN              bNoRebootLayer,
    IN PDRIVER_DISPATCH     pDispatchEntryFunction
)
/*
Routine Description:
    Handler for IOCTL_STORAGE_PERSISTENT_RESERVE_OUT.
        Cluster sends IOCTL_STORAGE_PERSISTENT_RESERVE_OUT
            To move shared disk from one node to another node.
            In case of owner node crashes, another node use
            this IOCLT to take ownership of disk.
    Arguments:
        DeviceObject            Indskflt Disk device object.
        Irp                     Irp that contains user mode request.
        bNoRebootLayer          If it is reboot or no-reboot mode.
        pDispatchEntryFunction  In case of noreboot mode, Disk dispatch routine.
Return Value:
    NTSTATUS.
*/
{
    PIO_STACK_LOCATION          IrpSp = IoGetCurrentIrpStackLocation(Irp);
    PDEVICE_EXTENSION           pDeviceExtension = (PDEVICE_EXTENSION)DeviceObject->DeviceExtension;
    PDEV_CONTEXT                pDevContext = pDeviceExtension->pDevContext;
    PPERSISTENT_RESERVE_COMMAND prCommand = NULL;
    NTSTATUS                    Status = STATUS_SUCCESS;

    ASSERT(IOCTL_STORAGE_PERSISTENT_RESERVE_OUT != IrpSp->Parameters.DeviceIoControl.IoControlCode);

    if (!IS_DISK_CLUSTERED(pDevContext)) {
        goto Cleanup;
    }

    if (IrpSp->Parameters.DeviceIoControl.InputBufferLength < sizeof(PERSISTENT_RESERVE_COMMAND)) {
        goto Cleanup;
    }

    prCommand = (PPERSISTENT_RESERVE_COMMAND)Irp->AssociatedIrp.SystemBuffer;
    InDskFltWriteEventWithDevCtxt(INDSKFLT_INFO_SCSI_PR_OUT_COMMAND, pDevContext,
        prCommand->PR_OUT.ServiceAction,
        prCommand->PR_OUT.Type, prCommand->PR_OUT.Scope);

Cleanup:
    InSkipCurrentIrpStackLocation(Irp, bNoRebootLayer);
    Status = InCallDriver(pDevContext->TargetDeviceObject,
        Irp,
        bNoRebootLayer,
        pDispatchEntryFunction);

    if (IS_DISK_CLUSTERED(pDevContext)) {
        SetDiskClusterState(pDevContext);
    }

    return Status;
}

#endif