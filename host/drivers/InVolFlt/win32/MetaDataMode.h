#ifndef _INVOLFLT_DRIVER_META_DATA_MODE_H_
#define _INVOLFLT_DRIVER_META_DATA_MODE_H_

NTSTATUS
AddMetaDataToVolumeContextNew(
    PVOLUME_CONTEXT     VolumeContext,
    PWRITE_META_DATA    pWriteMetaData
    );

/**
 * AddMetaDataToVolumeContext - Adds meta data to volume context
 * @param VolumeContext - Volumes context to which meta data has to be added
 * @param pWriteMetaData - Meta data to be added to volume context
 * @param bDataMode -   if TRUE this function is called from data mode
 *                      if FALSE this functin is called from meta-data mode
 * @return STATUS_NO_MEMORY if failed to add meta data to volume context due
 *                          to lack of memory.
 */
NTSTATUS
AddMetaDataToVolumeContext(
    PVOLUME_CONTEXT     VolumeContext,
    PWRITE_META_DATA    pWriteMetaData,
    BOOLEAN             bDataMode
    );

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

#endif // _INVOLFLT_DRIVER_META_DATA_MODE_H_