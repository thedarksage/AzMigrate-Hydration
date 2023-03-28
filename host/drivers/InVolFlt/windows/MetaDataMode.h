#pragma once
#include "global.h"
NTSTATUS
AddMetaDataToDevContextNew(
    PDEV_CONTEXT        DevContext,
    PWRITE_META_DATA    pWriteMetaData
    );

/**
 * AddMetaDataToDevContext - Adds meta data to Dev context
 * @param DevContext - FLtDev context to which meta data has to be added
 * @param pWriteMetaData - Meta data to be added to Dev context
 * @param bDataMode -   if TRUE this function is called from data mode
 *                      if FALSE this functin is called from meta-data mode
 * @return STATUS_NO_MEMORY if failed to add meta data to Dev context due
 *                          to lack of memory.
 */
NTSTATUS
AddMetaDataToDevContext(
    PDEV_CONTEXT        DevContext,
    PWRITE_META_DATA    pWriteMetaData,
    BOOLEAN             bDataMode,
    etCModeMetaDataReason eMetaDataModeReason
    );

#ifdef VOLUME_FLT

/**
 * WriteDispatchInMetaDataMode - Called from DispatchWrite
 * @param DeviceObject - Device to which write is performed
 * @param Irp -          IRP conating write information
 * @return NTSTATUS returned from IoCallDriver to lower device
 */
void
WriteDispatchInMetaDataMode(
    PDEVICE_OBJECT  DeviceObject,
    PIRP            Irp
    );
#endif

void
InWriteDispatchInMetaDataMode(
    __in PDEVICE_OBJECT	 DeviceObject,
    __in PIRP            Irp
    );

IO_COMPLETION_ROUTINE InCaptureIOInMetaDataMode;

