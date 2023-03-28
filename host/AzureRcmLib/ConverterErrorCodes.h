/*
+------------------------------------------------------------------------------------+
Copyright(c) Microsoft Corp. 2016
+------------------------------------------------------------------------------------+
File        :   ConverterErrorCodes.h

Description :   error codes that are returned from Converter class objects.

+------------------------------------------------------------------------------------+
*/

#ifndef _CONVERTER_ERROR_CODES_H
#define _CONVERTER_ERROR_CODES_H

#define CONVERTER_MODULE_BASE_ERROR_CODE 0x6000

namespace RcmClientLib
{
    /// \brief status codes returned by the converter class
    typedef enum ConverterErrorCodes {
        CONVERTER_NO_PARENT_DISK_FOR_VOLUME = CONVERTER_MODULE_BASE_ERROR_CODE,
        CONVERTER_PARTITION_FOUND_IN_TOP_LELVEL_VOLUMESUMMARIES,
        CONVERTER_DEVICE_ID_NOT_BASED_ON_SIGNATURE,
        CONVERTER_FOUND_DUPLICATE_DISK_ID,
        CONVERTER_FOUND_DUPLICATE_VOLUME_ID,
        CONVERTER_FOUND_NO_SYSTEM_DISKS,
        CONVERTER_FOUND_NO_BOOT_DISKS,
        CONVERTER_FOUND_NO_ROOT_DISKS,
        CONVERTER_DISK_CHILD_IS_NOT_PARTITION,
        CONVERTER_FOUND_MULTIPLE_CONTROLLERS_FOR_DATA_DISKS,
        CONVERTER_PERSISTENT_NAME_NOT_FOUND_BOOT_DISK,
        CONVERTER_PERSISTENT_NAME_NOT_FOUND_DATA_DISK,
        CONVERTER_CLUSTER_DISK_CHILD_IS_NOT_PARTITION,
        CONVERTER_FOUND_NO_CLUSTER_DISK,
        CONVERTER_NO_PARENT_CLUSTER_DISK_FOR_VOLUME,
        CONVERTER_FOUND_DUPLICATE_CLUSTER_DISK_ID,
        CONVERTER_FOUND_DUPLICATE_CLUSTER_VOLUME_ID
    } CONVERTER_STATUS;
}
#endif