#include "CDskCommon.h"

VOID
DumpInputBuffer(
    PIRP    Irp
    )
{
    PIO_STACK_LOCATION  pIoStackLocation = IoGetCurrentIrpStackLocation(Irp);
    ULONG               ulIoControlCode, ulInputBufferSize, ulIndex;
    PUCHAR               pBuffer;
    ulIoControlCode = pIoStackLocation->Parameters.DeviceIoControl.IoControlCode;
    if (METHOD_NEITHER != IOCTL_METHOD(ulIoControlCode)) {
        pBuffer = (PUCHAR)Irp->AssociatedIrp.SystemBuffer;
        ulInputBufferSize = pIoStackLocation->Parameters.DeviceIoControl.InputBufferLength;
        ulIndex = 0;
        while(ulIndex < ulInputBufferSize) {
            InCDskDbgPrint(DL_INFO, DM_IOCTL_INFO,("0x%x ", *(PUCHAR)pBuffer));
            pBuffer++;
            ulIndex++;
            if (0 == (ulIndex % 10)) {
                InCDskDbgPrint(DL_INFO, DM_IOCTL_INFO,("\n"));
            }
        }
    }
}

VOID
DumpUnknownDeviceTypeIOCTLInfo(
    PDEVICE_OBJECT DeviceObject, 
    PIRP    Irp
    )
{
    PIO_STACK_LOCATION  IoStackLocation = IoGetCurrentIrpStackLocation(Irp);
    ULONG               ulIoControlCode, ulInputLength, ulOutputLength;

    ulIoControlCode = IoStackLocation->Parameters.DeviceIoControl.IoControlCode;

    ulInputLength = IoStackLocation->Parameters.DeviceIoControl.InputBufferLength;
    ulOutputLength = IoStackLocation->Parameters.DeviceIoControl.OutputBufferLength;

    InCDskDbgPrint(DL_INFO, DM_IOCTL_INFO,("DeviceObject = %p, IOCTL = 0x%x, In=0x%x, Out=0x%x\n", DeviceObject,
                        ulIoControlCode, ulInputLength, ulOutputLength));

    return;
}

VOID
DumpDiskIOCTLInfo(
    PDEVICE_OBJECT DeviceObject, 
    PIRP    Irp
    )
{
    PIO_STACK_LOCATION  IoStackLocation = IoGetCurrentIrpStackLocation(Irp);
    ULONG               ulIoControlCode, ulInputLength, ulOutputLength;

    ulIoControlCode = IoStackLocation->Parameters.DeviceIoControl.IoControlCode;

    ulInputLength = IoStackLocation->Parameters.DeviceIoControl.InputBufferLength;
    ulOutputLength = IoStackLocation->Parameters.DeviceIoControl.OutputBufferLength;

    InCDskDbgPrint(DL_INFO, DM_IOCTL_INFO,("DeviceObject = %p, ", DeviceObject));

    switch(ulIoControlCode) {
        case IOCTL_DISK_GET_DRIVE_GEOMETRY :
            InCDskDbgPrint(DL_INFO, DM_IOCTL_INFO,("IOCTL = 0x%x(IOCTL_DISK_GET_DRIVE_GEOMETRY) ", ulIoControlCode));   
            break;
        case IOCTL_DISK_GET_PARTITION_INFO :
            InCDskDbgPrint(DL_INFO, DM_IOCTL_INFO,("IOCTL = 0x%x(IOCTL_DISK_GET_PARTITION_INFO) ", ulIoControlCode));   
            break;
        case IOCTL_DISK_SET_PARTITION_INFO :
            InCDskDbgPrint(DL_INFO, DM_IOCTL_INFO,("IOCTL = 0x%x(IOCTL_DISK_SET_PARTITION_INFO) ", ulIoControlCode));   
            break;
        case IOCTL_DISK_GET_DRIVE_LAYOUT :
            InCDskDbgPrint(DL_INFO, DM_IOCTL_INFO,("IOCTL = 0x%x(IOCTL_DISK_GET_DRIVE_LAYOUT) ", ulIoControlCode)); 
            break;
        case IOCTL_DISK_SET_DRIVE_LAYOUT :
            InCDskDbgPrint(DL_INFO, DM_IOCTL_INFO,("IOCTL = 0x%x(IOCTL_DISK_SET_DRIVE_LAYOUT) ", ulIoControlCode)); 
            break;
        case IOCTL_DISK_VERIFY :
            InCDskDbgPrint(DL_INFO, DM_IOCTL_INFO,("IOCTL = 0x%x(IOCTL_DISK_VERIFY) ", ulIoControlCode));   
            break;
        case IOCTL_DISK_FORMAT_TRACKS :
            InCDskDbgPrint(DL_INFO, DM_IOCTL_INFO,("IOCTL = 0x%x(IOCTL_DISK_FORMAT_TRACKS) ", ulIoControlCode));    
            break;
        case IOCTL_DISK_REASSIGN_BLOCKS :
            InCDskDbgPrint(DL_INFO, DM_IOCTL_INFO,("IOCTL = 0x%x(IOCTL_DISK_REASSIGN_BLOCKS) ", ulIoControlCode));  
            break;
        case IOCTL_DISK_PERFORMANCE :
            InCDskDbgPrint(DL_INFO, DM_IOCTL_INFO,("IOCTL = 0x%x(IOCTL_DISK_PERFORMANCE) ", ulIoControlCode));  
            break;
        case IOCTL_DISK_IS_WRITABLE :
            InCDskDbgPrint(DL_INFO, DM_IOCTL_INFO,("IOCTL = 0x%x(IOCTL_DISK_IS_WRITABLE) ", ulIoControlCode));  
            break;
        case IOCTL_DISK_LOGGING :
            InCDskDbgPrint(DL_INFO, DM_IOCTL_INFO,("IOCTL = 0x%x(IOCTL_DISK_LOGGING) ", ulIoControlCode));  
            break;
        case IOCTL_DISK_FORMAT_TRACKS_EX :
            InCDskDbgPrint(DL_INFO, DM_IOCTL_INFO,("IOCTL = 0x%x(IOCTL_DISK_FORMAT_TRACKS_EX) ", ulIoControlCode)); 
            break;
        case IOCTL_DISK_HISTOGRAM_STRUCTURE :
            InCDskDbgPrint(DL_INFO, DM_IOCTL_INFO,("IOCTL = 0x%x(IOCTL_DISK_HISTOGRAM_STRUCTURE) ", ulIoControlCode));  
            break;
        case IOCTL_DISK_HISTOGRAM_DATA :
            InCDskDbgPrint(DL_INFO, DM_IOCTL_INFO,("IOCTL = 0x%x(IOCTL_DISK_HISTOGRAM_DATA) ", ulIoControlCode));   
            break;
        case IOCTL_DISK_HISTOGRAM_RESET :
            InCDskDbgPrint(DL_INFO, DM_IOCTL_INFO,("IOCTL = 0x%x(IOCTL_DISK_HISTOGRAM_RESET) ", ulIoControlCode));  
            break;
        case IOCTL_DISK_REQUEST_STRUCTURE :
            InCDskDbgPrint(DL_INFO, DM_IOCTL_INFO,("IOCTL = 0x%x(IOCTL_DISK_REQUEST_STRUCTURE) ", ulIoControlCode));    
            break;
        case IOCTL_DISK_REQUEST_DATA :
            InCDskDbgPrint(DL_INFO, DM_IOCTL_INFO,("IOCTL = 0x%x(IOCTL_DISK_REQUEST_DATA) ", ulIoControlCode)); 
            break;

#if(_WIN32_WINNT >= 0x0400)

        case IOCTL_DISK_CONTROLLER_NUMBER :
            InCDskDbgPrint(DL_INFO, DM_IOCTL_INFO,("IOCTL = 0x%x(IOCTL_DISK_CONTROLLER_NUMBER) ", ulIoControlCode));    
            break;
        case SMART_GET_VERSION :
            InCDskDbgPrint(DL_INFO, DM_IOCTL_INFO,("IOCTL = 0x%x(SMART_GET_VERSION) ", ulIoControlCode));   
            break;
        case SMART_SEND_DRIVE_COMMAND :
            InCDskDbgPrint(DL_INFO, DM_IOCTL_INFO,("IOCTL = 0x%x(SMART_SEND_DRIVE_COMMAND) ", ulIoControlCode));    
            break;
        case SMART_RCV_DRIVE_DATA :
            InCDskDbgPrint(DL_INFO, DM_IOCTL_INFO,("IOCTL = 0x%x(SMART_RCV_DRIVE_DATA) ", ulIoControlCode));    
            break;

#endif /* _WIN32_WINNT >= 0x0400 */

#if (_WIN32_WINNT >= 0x501)

        case IOCTL_DISK_PERFORMANCE_OFF :
            InCDskDbgPrint(DL_INFO, DM_IOCTL_INFO,("IOCTL = 0x%x(IOCTL_DISK_PERFORMANCE_OFF) ", ulIoControlCode));  
            break;
        case IOCTL_DISK_GET_PARTITION_INFO_EX :
            InCDskDbgPrint(DL_INFO, DM_IOCTL_INFO,("IOCTL = 0x%x(IOCTL_DISK_GET_PARTITION_INFO_EX) ", ulIoControlCode));    
            break;
        case IOCTL_DISK_SET_PARTITION_INFO_EX :
            InCDskDbgPrint(DL_INFO, DM_IOCTL_INFO,("IOCTL = 0x%x(IOCTL_DISK_SET_PARTITION_INFO_EX) ", ulIoControlCode));    
            break;
        case IOCTL_DISK_GET_DRIVE_LAYOUT_EX :
            InCDskDbgPrint(DL_INFO, DM_IOCTL_INFO,("IOCTL = 0x%x(IOCTL_DISK_GET_DRIVE_LAYOUT_EX) ", ulIoControlCode));  
            break;
        case IOCTL_DISK_SET_DRIVE_LAYOUT_EX :
            InCDskDbgPrint(DL_INFO, DM_IOCTL_INFO,("IOCTL = 0x%x(IOCTL_DISK_SET_DRIVE_LAYOUT_EX) ", ulIoControlCode));  
            break;
        case IOCTL_DISK_CREATE_DISK :
            InCDskDbgPrint(DL_INFO, DM_IOCTL_INFO,("IOCTL = 0x%x(IOCTL_DISK_CREATE_DISK) ", ulIoControlCode));  
            break;
        case IOCTL_DISK_GET_LENGTH_INFO :
            InCDskDbgPrint(DL_INFO, DM_IOCTL_INFO,("IOCTL = 0x%x(IOCTL_DISK_GET_LENGTH_INFO) ", ulIoControlCode));  
            break;
        case IOCTL_DISK_GET_DRIVE_GEOMETRY_EX :
            InCDskDbgPrint(DL_INFO, DM_IOCTL_INFO,("IOCTL = 0x%x(IOCTL_DISK_GET_DRIVE_GEOMETRY_EX) ", ulIoControlCode));    
            break;
#if (NTDDI_VERSION < NTDDI_WIN2003)
        case IOCTL_DISK_GET_WRITE_CACHE_STATE :
            InCDskDbgPrint(DL_INFO, DM_IOCTL_INFO,("IOCTL = 0x%x(IOCTL_DISK_GET_WRITE_CACHE_STATE) ", ulIoControlCode));    
#else
        case OBSOLETE_DISK_GET_WRITE_CACHE_STATE :
            InCDskDbgPrint(DL_INFO, DM_IOCTL_INFO,("IOCTL = 0x%x(OBSOLETE_DISK_GET_WRITE_CACHE_STATE) ", ulIoControlCode)); 
#endif /* (NTDDI_VERSION < NTDDI_WIN2003) */
            break;

#endif /* _WIN32_WINNT >= 0x0501 */

#if (_WIN32_WINNT >= 0x0502)
        case IOCTL_DISK_REASSIGN_BLOCKS_EX :
            InCDskDbgPrint(DL_INFO, DM_IOCTL_INFO,("IOCTL = 0x%x(IOCTL_DISK_REASSIGN_BLOCKS_EX) ", ulIoControlCode));   
            break;
        case IOCTL_DISK_COPY_DATA :
            InCDskDbgPrint(DL_INFO, DM_IOCTL_INFO,("IOCTL = 0x%x(IOCTL_DISK_COPY_DATA) ", ulIoControlCode));    
            break;
#endif //_WIN32_WINNT >= 0x0502

#if(_WIN32_WINNT >= 0x0500)

        case IOCTL_DISK_UPDATE_DRIVE_SIZE :
            InCDskDbgPrint(DL_INFO, DM_IOCTL_INFO,("IOCTL = 0x%x(IOCTL_DISK_UPDATE_DRIVE_SIZE) ", ulIoControlCode));    
            break;
        case IOCTL_DISK_GROW_PARTITION :
            InCDskDbgPrint(DL_INFO, DM_IOCTL_INFO,("IOCTL = 0x%x(IOCTL_DISK_GROW_PARTITION) ", ulIoControlCode));   
            break;
        case IOCTL_DISK_GET_CACHE_INFORMATION :
            InCDskDbgPrint(DL_INFO, DM_IOCTL_INFO,("IOCTL = 0x%x(IOCTL_DISK_GET_CACHE_INFORMATION) ", ulIoControlCode));    
            break;
        case IOCTL_DISK_SET_CACHE_INFORMATION :
            InCDskDbgPrint(DL_INFO, DM_IOCTL_INFO,("IOCTL = 0x%x(IOCTL_DISK_SET_CACHE_INFORMATION) ", ulIoControlCode));    
            break;
        case IOCTL_DISK_DELETE_DRIVE_LAYOUT :
            InCDskDbgPrint(DL_INFO, DM_IOCTL_INFO,("IOCTL = 0x%x(IOCTL_DISK_DELETE_DRIVE_LAYOUT) ", ulIoControlCode));  
            break;
        case IOCTL_DISK_FORMAT_DRIVE :
            InCDskDbgPrint(DL_INFO, DM_IOCTL_INFO,("IOCTL = 0x%x(IOCTL_DISK_FORMAT_DRIVE) ", ulIoControlCode)); 
            break;
        case IOCTL_DISK_SENSE_DEVICE :
            InCDskDbgPrint(DL_INFO, DM_IOCTL_INFO,("IOCTL = 0x%x(IOCTL_DISK_SENSE_DEVICE) ", ulIoControlCode)); 
            break;

#endif /* _WIN32_WINNT >= 0x0500 */

#if(_WIN32_WINNT >= 0x0501)
        case IOCTL_DISK_UPDATE_PROPERTIES :
            InCDskDbgPrint(DL_INFO, DM_IOCTL_INFO,("IOCTL = 0x%x(IOCTL_DISK_UPDATE_PROPERTIES) ", ulIoControlCode));    
            break;
#endif /* _WIN32_WINNT >= 0x0501 */

        case IOCTL_DISK_GET_CACHE_SETTING :
            InCDskDbgPrint(DL_INFO, DM_IOCTL_INFO,("IOCTL = 0x%x(IOCTL_DISK_GET_CACHE_SETTING) ", ulIoControlCode));    
            break;
        case IOCTL_DISK_SET_CACHE_SETTING :
            InCDskDbgPrint(DL_INFO, DM_IOCTL_INFO,("IOCTL = 0x%x(IOCTL_DISK_SET_CACHE_SETTING) ", ulIoControlCode));    
            break;
        case IOCTL_DISK_INTERNAL_SET_VERIFY :
            InCDskDbgPrint(DL_INFO, DM_IOCTL_INFO,("IOCTL = 0x%x(IOCTL_DISK_INTERNAL_SET_VERIFY) ", ulIoControlCode));  
            break;
        case IOCTL_DISK_INTERNAL_CLEAR_VERIFY :
            InCDskDbgPrint(DL_INFO, DM_IOCTL_INFO,("IOCTL = 0x%x(IOCTL_DISK_INTERNAL_CLEAR_VERIFY) ", ulIoControlCode));    
            break;
        case IOCTL_DISK_INTERNAL_SET_NOTIFY :
            InCDskDbgPrint(DL_INFO, DM_IOCTL_INFO,("IOCTL = 0x%x(IOCTL_DISK_INTERNAL_SET_NOTIFY) ", ulIoControlCode));  
            break;
        case IOCTL_DISK_CHECK_VERIFY :
            InCDskDbgPrint(DL_INFO, DM_IOCTL_INFO,("IOCTL = 0x%x(IOCTL_DISK_CHECK_VERIFY) ", ulIoControlCode)); 
            break;
        case IOCTL_DISK_MEDIA_REMOVAL :
            InCDskDbgPrint(DL_INFO, DM_IOCTL_INFO,("IOCTL = 0x%x(IOCTL_DISK_MEDIA_REMOVAL) ", ulIoControlCode));    
            break;
        case IOCTL_DISK_EJECT_MEDIA :
            InCDskDbgPrint(DL_INFO, DM_IOCTL_INFO,("IOCTL = 0x%x(IOCTL_DISK_EJECT_MEDIA) ", ulIoControlCode));  
            break;
        case IOCTL_DISK_LOAD_MEDIA :
            InCDskDbgPrint(DL_INFO, DM_IOCTL_INFO,("IOCTL = 0x%x(IOCTL_DISK_LOAD_MEDIA) ", ulIoControlCode));   
            break;
        case IOCTL_DISK_RESERVE :
            InCDskDbgPrint(DL_INFO, DM_IOCTL_INFO,("IOCTL = 0x%x(IOCTL_DISK_RESERVE) ", ulIoControlCode));  
            break;
        case IOCTL_DISK_RELEASE :
            InCDskDbgPrint(DL_INFO, DM_IOCTL_INFO,("IOCTL = 0x%x(IOCTL_DISK_RELEASE) ", ulIoControlCode));  
            break;
        case IOCTL_DISK_FIND_NEW_DEVICES :
            InCDskDbgPrint(DL_INFO, DM_IOCTL_INFO,("IOCTL = 0x%x(IOCTL_DISK_FIND_NEW_DEVICES) ", ulIoControlCode)); 
            break;
        case IOCTL_DISK_GET_MEDIA_TYPES :
            InCDskDbgPrint(DL_INFO, DM_IOCTL_INFO,("IOCTL = 0x%x(IOCTL_DISK_GET_MEDIA_TYPES) ", ulIoControlCode));  
            break;
        case IOCTL_DISK_SIMBAD :
            InCDskDbgPrint(DL_INFO, DM_IOCTL_INFO,("IOCTL = 0x%x(IOCTL_DISK_SIMBAD) ", ulIoControlCode));   
            break;
        default:
            InCDskDbgPrint(DL_INFO, DM_IOCTL_INFO,("IOCTL = 0x%x, ", ulIoControlCode));
            break;
    }

    InCDskDbgPrint(DL_INFO, DM_IOCTL_INFO,("In=0x%x, Out=0x%x\n", ulInputLength, ulOutputLength));

    return;
}

VOID
DumpStorageDevIOCTLInfo(
    PDEVICE_OBJECT DeviceObject, 
    PIRP    Irp
    )
{
    PIO_STACK_LOCATION  IoStackLocation = IoGetCurrentIrpStackLocation(Irp);
    ULONG               ulIoControlCode, ulInputLength, ulOutputLength;

    ulIoControlCode = IoStackLocation->Parameters.DeviceIoControl.IoControlCode;

    ulInputLength = IoStackLocation->Parameters.DeviceIoControl.InputBufferLength;
    ulOutputLength = IoStackLocation->Parameters.DeviceIoControl.OutputBufferLength;

    InCDskDbgPrint(DL_INFO, DM_IOCTL_INFO,("DeviceObject = %p, ", DeviceObject));

    switch(ulIoControlCode) {
        case IOCTL_STORAGE_CHECK_VERIFY:
            InCDskDbgPrint(DL_INFO, DM_IOCTL_INFO,("IOCTL = 0x%x(IOCTL_STORAGE_CHECK_VERIFY) ", ulIoControlCode));
            break;
        case IOCTL_STORAGE_CHECK_VERIFY2:
            InCDskDbgPrint(DL_INFO, DM_IOCTL_INFO,("IOCTL = 0x%x(IOCTL_STORAGE_CHECK_VERIFY2) ", ulIoControlCode));
            break;
        case IOCTL_STORAGE_MEDIA_REMOVAL:
            InCDskDbgPrint(DL_INFO, DM_IOCTL_INFO,("IOCTL = 0x%x(IOCTL_STORAGE_MEDIA_REMOVAL) ", ulIoControlCode));
            break;
        case IOCTL_STORAGE_EJECT_MEDIA:
            InCDskDbgPrint(DL_INFO, DM_IOCTL_INFO,("IOCTL = 0x%x(IOCTL_STORAGE_EJECT_MEDIA) ", ulIoControlCode));
            break;
        case IOCTL_STORAGE_LOAD_MEDIA:
            InCDskDbgPrint(DL_INFO, DM_IOCTL_INFO,("IOCTL = 0x%x(IOCTL_STORAGE_LOAD_MEDIA) ", ulIoControlCode));
            break;
        case IOCTL_STORAGE_LOAD_MEDIA2:
            InCDskDbgPrint(DL_INFO, DM_IOCTL_INFO,("IOCTL = 0x%x(IOCTL_STORAGE_LOAD_MEDIA2) ", ulIoControlCode));
            break;
        case IOCTL_STORAGE_RESERVE:
            InCDskDbgPrint(DL_INFO, DM_IOCTL_INFO,("IOCTL = 0x%x(IOCTL_STORAGE_RESERVE) ", ulIoControlCode));
            break;
        case IOCTL_STORAGE_RELEASE:
            InCDskDbgPrint(DL_INFO, DM_IOCTL_INFO,("IOCTL = 0x%x(IOCTL_STORAGE_RELEASE) ", ulIoControlCode));
            break;
        case IOCTL_STORAGE_FIND_NEW_DEVICES:
            InCDskDbgPrint(DL_INFO, DM_IOCTL_INFO,("IOCTL = 0x%x(IOCTL_STORAGE_FIND_NEW_DEVICES) ", ulIoControlCode));
            break;
        case IOCTL_STORAGE_EJECTION_CONTROL:
            InCDskDbgPrint(DL_INFO, DM_IOCTL_INFO,("IOCTL = 0x%x(IOCTL_STORAGE_EJECTION_CONTROL) ", ulIoControlCode));
            break;
        case IOCTL_STORAGE_MCN_CONTROL:
            InCDskDbgPrint(DL_INFO, DM_IOCTL_INFO,("IOCTL = 0x%x(IOCTL_STORAGE_MCN_CONTROL) ", ulIoControlCode));
            break;
        case IOCTL_STORAGE_GET_MEDIA_TYPES:
            InCDskDbgPrint(DL_INFO, DM_IOCTL_INFO,("IOCTL = 0x%x(IOCTL_STORAGE_GET_MEDIA_TYPES) ", ulIoControlCode));
            break;
        case IOCTL_STORAGE_GET_MEDIA_TYPES_EX:
            InCDskDbgPrint(DL_INFO, DM_IOCTL_INFO,("IOCTL = 0x%x(IOCTL_STORAGE_GET_MEDIA_TYPES_EX) ", ulIoControlCode));
            break;
#if (_WIN32_WINNT >= 0x0501)
        case IOCTL_STORAGE_GET_MEDIA_SERIAL_NUMBER:
            InCDskDbgPrint(DL_INFO, DM_IOCTL_INFO,("IOCTL = 0x%x(IOCTL_STORAGE_GET_MEDIA_SERIAL_NUMBER) ", ulIoControlCode));
            break;
        case IOCTL_STORAGE_GET_HOTPLUG_INFO:
            InCDskDbgPrint(DL_INFO, DM_IOCTL_INFO,("IOCTL = 0x%x(IOCTL_STORAGE_GET_HOTPLUG_INFO) ", ulIoControlCode));
            break;
        case IOCTL_STORAGE_SET_HOTPLUG_INFO:
            InCDskDbgPrint(DL_INFO, DM_IOCTL_INFO,("IOCTL = 0x%x(IOCTL_STORAGE_SET_HOTPLUG_INFO) ", ulIoControlCode));
            break;
        case IOCTL_STORAGE_BREAK_RESERVATION:
            InCDskDbgPrint(DL_INFO, DM_IOCTL_INFO,("IOCTL = 0x%x(IOCTL_STORAGE_BREAK_RESERVATION) ", ulIoControlCode));
            break;
#endif /* (_WIN32_WINNT >= 0x0501) */
        case IOCTL_STORAGE_RESET_BUS:
            InCDskDbgPrint(DL_INFO, DM_IOCTL_INFO,("IOCTL = 0x%x(IOCTL_STORAGE_RESET_BUS) ", ulIoControlCode));
            break;
        case IOCTL_STORAGE_RESET_DEVICE:
            InCDskDbgPrint(DL_INFO, DM_IOCTL_INFO,("IOCTL = 0x%x(IOCTL_STORAGE_RESET_DEVICE) ", ulIoControlCode));
            break;
        case IOCTL_STORAGE_GET_DEVICE_NUMBER:
            InCDskDbgPrint(DL_INFO, DM_IOCTL_INFO,("IOCTL = 0x%x(IOCTL_STORAGE_GET_DEVICE_NUMBER) ", ulIoControlCode));
            break;
        case IOCTL_STORAGE_PREDICT_FAILURE:
            InCDskDbgPrint(DL_INFO, DM_IOCTL_INFO,("IOCTL = 0x%x(IOCTL_STORAGE_PREDICT_FAILURE) ", ulIoControlCode));
            break;
#if (_WIN32_WINNT >= 0x0502)
        case IOCTL_STORAGE_READ_CAPACITY:
            InCDskDbgPrint(DL_INFO, DM_IOCTL_INFO,("IOCTL = 0x%x(IOCTL_STORAGE_READ_CAPACITY) ", ulIoControlCode));
            break;
#endif /* (_WIN32_WINNT >= 0x0502) */
        case IOCTL_STORAGE_QUERY_PROPERTY:
            InCDskDbgPrint(DL_INFO, DM_IOCTL_INFO,("IOCTL = 0x%x(IOCTL_STORAGE_QUERY_PROPERTY) ", ulIoControlCode));
            break;
        case OBSOLETE_IOCTL_STORAGE_RESET_BUS:
            InCDskDbgPrint(DL_INFO, DM_IOCTL_INFO,("IOCTL = 0x%x(OBSOLETE_IOCTL_STORAGE_RESET_BUS) ", ulIoControlCode));
            break;
        case OBSOLETE_IOCTL_STORAGE_RESET_DEVICE:
            InCDskDbgPrint(DL_INFO, DM_IOCTL_INFO,("IOCTL = 0x%x(OBSOLETE_IOCTL_STORAGE_RESET_DEVICE) ", ulIoControlCode));
            break;
        default:
            InCDskDbgPrint(DL_INFO, DM_IOCTL_INFO,("IOCTL = 0x%x, ", ulIoControlCode));
            break;
    }

    InCDskDbgPrint(DL_INFO, DM_IOCTL_INFO,("In=0x%x, Out=0x%x\n", ulInputLength, ulOutputLength));

    return;
}

VOID
DumpScsiIOCTLInfo(
    PDEVICE_OBJECT DeviceObject, 
    PIRP    Irp
    )
{
    PIO_STACK_LOCATION  IoStackLocation = IoGetCurrentIrpStackLocation(Irp);
    ULONG               ulIoControlCode, ulInputLength, ulOutputLength;
    ulIoControlCode = IoStackLocation->Parameters.DeviceIoControl.IoControlCode;

    ulInputLength = IoStackLocation->Parameters.DeviceIoControl.InputBufferLength;
    ulOutputLength = IoStackLocation->Parameters.DeviceIoControl.OutputBufferLength;

    InCDskDbgPrint(DL_INFO, DM_IOCTL_INFO,("DeviceObject = %p, ", DeviceObject));

    switch(ulIoControlCode) {
        case IOCTL_SCSI_PASS_THROUGH:
            InCDskDbgPrint(DL_INFO, DM_IOCTL_INFO,("IOCTL = 0x%x(IOCTL_SCSI_PASS_THROUGH) ", ulIoControlCode));
            break;
        case IOCTL_SCSI_MINIPORT:
            InCDskDbgPrint(DL_INFO, DM_IOCTL_INFO,("IOCTL = 0x%x(IOCTL_SCSI_MINIPORT) ", ulIoControlCode));
            break;
        case IOCTL_SCSI_GET_INQUIRY_DATA:
            InCDskDbgPrint(DL_INFO, DM_IOCTL_INFO,("IOCTL = 0x%x(IOCTL_SCSI_GET_INQUIRY_DATA) ", ulIoControlCode));
            break;
        case IOCTL_SCSI_GET_CAPABILITIES:
            InCDskDbgPrint(DL_INFO, DM_IOCTL_INFO,("IOCTL = 0x%x(IOCTL_SCSI_GET_CAPABILITIES) ", ulIoControlCode));
            break;
        case IOCTL_SCSI_PASS_THROUGH_DIRECT:
            InCDskDbgPrint(DL_INFO, DM_IOCTL_INFO,("IOCTL = 0x%x(IOCTL_SCSI_PASS_THROUGH_DIRECT) ", ulIoControlCode));
            break;
        case IOCTL_SCSI_GET_ADDRESS:
            InCDskDbgPrint(DL_INFO, DM_IOCTL_INFO,("IOCTL = 0x%x(IOCTL_SCSI_GET_ADDRESS) ", ulIoControlCode));
            break;
        case IOCTL_SCSI_RESCAN_BUS:
            InCDskDbgPrint(DL_INFO, DM_IOCTL_INFO,("IOCTL = 0x%x(IOCTL_SCSI_RESCAN_BUS) ", ulIoControlCode));
            break;
        case IOCTL_SCSI_GET_DUMP_POINTERS:
            InCDskDbgPrint(DL_INFO, DM_IOCTL_INFO,("IOCTL = 0x%x(IOCTL_SCSI_GET_DUMP_POINTERS) ", ulIoControlCode));
            break;
        case IOCTL_SCSI_FREE_DUMP_POINTERS:
            InCDskDbgPrint(DL_INFO, DM_IOCTL_INFO,("IOCTL = 0x%x(IOCTL_SCSI_FREE_DUMP_POINTERS) ", ulIoControlCode));
            break;
        case IOCTL_IDE_PASS_THROUGH:
            InCDskDbgPrint(DL_INFO, DM_IOCTL_INFO,("IOCTL = 0x%x(IOCTL_IDE_PASS_THROUGH) ", ulIoControlCode));
            break;
#if (_WIN32_WINNT >= 0x0501)
        case IOCTL_ATA_PASS_THROUGH:
            InCDskDbgPrint(DL_INFO, DM_IOCTL_INFO,("IOCTL = 0x%x(IOCTL_ATA_PASS_THROUGH) ", ulIoControlCode));
            break;
        case IOCTL_ATA_PASS_THROUGH_DIRECT:
            InCDskDbgPrint(DL_INFO, DM_IOCTL_INFO,("IOCTL = 0x%x(IOCTL_ATA_PASS_THROUGH_DIRECT) ", ulIoControlCode));
            break;
#endif /* (_WIN32_WINNT >= 0x0501) */
        default:
            InCDskDbgPrint(DL_INFO, DM_IOCTL_INFO,("IOCTL = 0x%x, ", ulIoControlCode));
            break;
    }

    InCDskDbgPrint(DL_INFO, DM_IOCTL_INFO,("In=0x%x, Out=0x%x\n", ulInputLength, ulOutputLength));

    return;
}
#if (NTDDI_VERSION < NTDDI_VISTA)
VOID
DumpFtDevIOCTLInfo(
    PDEVICE_OBJECT DeviceObject, 
    PIRP    Irp
    )
{
    PIO_STACK_LOCATION  IoStackLocation = IoGetCurrentIrpStackLocation(Irp);
    ULONG               ulIoControlCode, ulInputLength, ulOutputLength;

    ulIoControlCode = IoStackLocation->Parameters.DeviceIoControl.IoControlCode;

    ulInputLength = IoStackLocation->Parameters.DeviceIoControl.InputBufferLength;
    ulOutputLength = IoStackLocation->Parameters.DeviceIoControl.OutputBufferLength;

    InCDskDbgPrint(DL_INFO, DM_IOCTL_INFO,("DeviceObject = %p, ", DeviceObject));

    switch(ulIoControlCode) {
        case FT_INITIALIZE_SET:
            InCDskDbgPrint(DL_INFO, DM_IOCTL_INFO,("IOCTL = 0x%x(FT_INITIALIZE_SET) ", ulIoControlCode));
            break;
        case FT_REGENERATE:
            InCDskDbgPrint(DL_INFO, DM_IOCTL_INFO,("IOCTL = 0x%x(FT_REGENERATE) ", ulIoControlCode));
            break;
        case FT_CONFIGURE:
            InCDskDbgPrint(DL_INFO, DM_IOCTL_INFO,("IOCTL = 0x%x(FT_CONFIGURE) ", ulIoControlCode));
            break;
        case FT_VERIFY:
            InCDskDbgPrint(DL_INFO, DM_IOCTL_INFO,("IOCTL = 0x%x(FT_VERIFY) ", ulIoControlCode));
            break;
        case FT_SECONDARY_READ:
            InCDskDbgPrint(DL_INFO, DM_IOCTL_INFO,("IOCTL = 0x%x(FT_SECONDARY_READ) ", ulIoControlCode));
            break;
        case FT_PRIMARY_READ:
            InCDskDbgPrint(DL_INFO, DM_IOCTL_INFO,("IOCTL = 0x%x(FT_PRIMARY_READ) ", ulIoControlCode));
            break;
        case FT_BALANCED_READ_MODE:
            InCDskDbgPrint(DL_INFO, DM_IOCTL_INFO,("IOCTL = 0x%x(FT_BALANCED_READ_MODE) ", ulIoControlCode));
            break;
        case FT_SYNC_REDUNDANT_COPY:
            InCDskDbgPrint(DL_INFO, DM_IOCTL_INFO,("IOCTL = 0x%x(FT_SYNC_REDUNDANT_COPY) ", ulIoControlCode));
            break;
        case FT_SEQUENTIAL_WRITE_MODE:
            InCDskDbgPrint(DL_INFO, DM_IOCTL_INFO,("IOCTL = 0x%x(FT_SEQUENTIAL_WRITE_MODE) ", ulIoControlCode));
            break;
        case FT_PARALLEL_WRITE_MODE:
            InCDskDbgPrint(DL_INFO, DM_IOCTL_INFO,("IOCTL = 0x%x(FT_PARALLEL_WRITE_MODE) ", ulIoControlCode));
            break;
        case FT_QUERY_SET_STATE:
            InCDskDbgPrint(DL_INFO, DM_IOCTL_INFO,("IOCTL = 0x%x(FT_QUERY_SET_STATE) ", ulIoControlCode));
            break;
        case FT_CLUSTER_SET_MEMBER_STATE:
            InCDskDbgPrint(DL_INFO, DM_IOCTL_INFO,("IOCTL = 0x%x(FT_CLUSTER_SET_MEMBER_STATE) ", ulIoControlCode));
            break;
        case FT_CLUSTER_GET_MEMBER_STATE:
            InCDskDbgPrint(DL_INFO, DM_IOCTL_INFO,("IOCTL = 0x%x(FT_CLUSTER_GET_MEMBER_STATE) ", ulIoControlCode));
            break;
        default:
            InCDskDbgPrint(DL_INFO, DM_IOCTL_INFO,("IOCTL = 0x%x, ", ulIoControlCode));
            break;
    }

    InCDskDbgPrint(DL_INFO, DM_IOCTL_INFO,("In=0x%x, Out=0x%x\n", ulInputLength, ulOutputLength));

    return;
}
#endif
VOID
DumpMountDevIOCTLInfo(
    PDEVICE_OBJECT DeviceObject, 
    PIRP    Irp
    )
{
    PIO_STACK_LOCATION  IoStackLocation = IoGetCurrentIrpStackLocation(Irp);
    ULONG               ulIoControlCode, ulInputLength, ulOutputLength;
    ulIoControlCode = IoStackLocation->Parameters.DeviceIoControl.IoControlCode;

    ulInputLength = IoStackLocation->Parameters.DeviceIoControl.InputBufferLength;
    ulOutputLength = IoStackLocation->Parameters.DeviceIoControl.OutputBufferLength;

    InCDskDbgPrint(DL_INFO, DM_IOCTL_INFO,("DeviceObject = %p, ", DeviceObject));

    switch(ulIoControlCode) {
        case IOCTL_MOUNTDEV_QUERY_UNIQUE_ID:
            InCDskDbgPrint(DL_INFO, DM_IOCTL_INFO,("IOCTL = 0x%x(IOCTL_MOUNTDEV_QUERY_UNIQUE_ID) ", ulIoControlCode));
            break;
        case IOCTL_MOUNTDEV_QUERY_DEVICE_NAME:
            InCDskDbgPrint(DL_INFO, DM_IOCTL_INFO,("IOCTL = 0x%x(IOCTL_MOUNTDEV_QUERY_DEVICE_NAME) ", ulIoControlCode));
            break;
        case IOCTL_MOUNTDEV_QUERY_SUGGESTED_LINK_NAME:
            InCDskDbgPrint(DL_INFO, DM_IOCTL_INFO,("IOCTL = 0x%x(IOCTL_MOUNTDEV_QUERY_SUGGESTED_LINK_NAME) ", ulIoControlCode));
            break;
//      case IOCTL_MOUNTDEV_LINK_CREATED: This is different for 2k and 2k3
        case IOCTL_MOUNTDEV_LINK_CREATED_W2K:
            InCDskDbgPrint(DL_INFO, DM_IOCTL_INFO,("IOCTL = 0x%x(IOCTL_MOUNTDEV_LINK_CREATED_W2K) ", ulIoControlCode));
            break;
        case IOCTL_MOUNTDEV_LINK_CREATED_WNET:
            InCDskDbgPrint(DL_INFO, DM_IOCTL_INFO,("IOCTL = 0x%x(IOCTL_MOUNTDEV_LINK_CREATED_WNET) ", ulIoControlCode));
            break;
        case IOCTL_MOUNTDEV_LINK_DELETED:
            InCDskDbgPrint(DL_INFO, DM_IOCTL_INFO,("IOCTL = 0x%x(IOCTL_MOUNTDEV_LINK_DELETED) ", ulIoControlCode));
            break;
#if (_WIN32_WINNT >= 0x0501)
        case IOCTL_MOUNTDEV_QUERY_STABLE_GUID:
            InCDskDbgPrint(DL_INFO, DM_IOCTL_INFO,("IOCTL = 0x%x(IOCTL_MOUNTDEV_QUERY_STABLE_GUID) ", ulIoControlCode));
            break;
#endif // (_WIN32_WINNT >= 0x0501)
        default:
            InCDskDbgPrint(DL_INFO, DM_IOCTL_INFO,("IOCTL = 0x%x, ", ulIoControlCode));
            break;
    }

    InCDskDbgPrint(DL_INFO, DM_IOCTL_INFO,("In=0x%x, Out=0x%x\n", ulInputLength, ulOutputLength));

    return;
}

VOID
DumpVolumeIOCTLInfo(
    PDEVICE_OBJECT DeviceObject, 
    PIRP    Irp
    )
{
    PIO_STACK_LOCATION  IoStackLocation = IoGetCurrentIrpStackLocation(Irp);
    ULONG               ulIoControlCode, ulInputLength, ulOutputLength;

    ulIoControlCode = IoStackLocation->Parameters.DeviceIoControl.IoControlCode;

    ulInputLength = IoStackLocation->Parameters.DeviceIoControl.InputBufferLength;
    ulOutputLength = IoStackLocation->Parameters.DeviceIoControl.OutputBufferLength;

    InCDskDbgPrint(DL_INFO, DM_IOCTL_INFO,("DeviceObject = %p, ", DeviceObject));

    switch(ulIoControlCode) {
        case IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS :
            InCDskDbgPrint(DL_INFO, DM_IOCTL_INFO,("IOCTL = 0x%x(IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS) ", ulIoControlCode));
            break;
        case IOCTL_VOLUME_SUPPORTS_ONLINE_OFFLINE :
            InCDskDbgPrint(DL_INFO, DM_IOCTL_INFO,("IOCTL = 0x%x(IOCTL_VOLUME_SUPPORTS_ONLINE_OFFLINE) ", ulIoControlCode));
            break;
        case IOCTL_VOLUME_ONLINE :
            InCDskDbgPrint(DL_INFO, DM_IOCTL_INFO,("IOCTL = 0x%x(IOCTL_VOLUME_ONLINE) ", ulIoControlCode));
            break;
        case IOCTL_VOLUME_OFFLINE :
            InCDskDbgPrint(DL_INFO, DM_IOCTL_INFO,("IOCTL = 0x%x(IOCTL_VOLUME_OFFLINE) ", ulIoControlCode));
            DumpInputBuffer(Irp);
            break;
        case IOCTL_VOLUME_IS_OFFLINE :
            InCDskDbgPrint(DL_INFO, DM_IOCTL_INFO,("IOCTL = 0x%x(IOCTL_VOLUME_IS_OFFLINE) ", ulIoControlCode));
            break;
        case IOCTL_VOLUME_IS_IO_CAPABLE :
            InCDskDbgPrint(DL_INFO, DM_IOCTL_INFO,("IOCTL = 0x%x(IOCTL_VOLUME_IS_IO_CAPABLE) ", ulIoControlCode));
            break;
        case IOCTL_VOLUME_QUERY_FAILOVER_SET :
            InCDskDbgPrint(DL_INFO, DM_IOCTL_INFO,("IOCTL = 0x%x(IOCTL_VOLUME_QUERY_FAILOVER_SET) ", ulIoControlCode));
            break;
        case IOCTL_VOLUME_QUERY_VOLUME_NUMBER :
            InCDskDbgPrint(DL_INFO, DM_IOCTL_INFO,("IOCTL = 0x%x(IOCTL_VOLUME_QUERY_VOLUME_NUMBER) ", ulIoControlCode));
            break;
        case IOCTL_VOLUME_LOGICAL_TO_PHYSICAL :
            InCDskDbgPrint(DL_INFO, DM_IOCTL_INFO,("IOCTL = 0x%x(IOCTL_VOLUME_LOGICAL_TO_PHYSICAL) ", ulIoControlCode));
            break;
        case IOCTL_VOLUME_PHYSICAL_TO_LOGICAL :
            InCDskDbgPrint(DL_INFO, DM_IOCTL_INFO,("IOCTL = 0x%x(IOCTL_VOLUME_PHYSICAL_TO_LOGICAL) ", ulIoControlCode));
            break;
#if (_WIN32_WINNT >= 0x0501)
        case IOCTL_VOLUME_IS_PARTITION :
            InCDskDbgPrint(DL_INFO, DM_IOCTL_INFO,("IOCTL = 0x%x(IOCTL_VOLUME_IS_PARTITION) ", ulIoControlCode));
            break;
        case IOCTL_VOLUME_READ_PLEX :     
            InCDskDbgPrint(DL_INFO, DM_IOCTL_INFO,("IOCTL = 0x%x(IOCTL_VOLUME_READ_PLEX) ", ulIoControlCode));
            break;
        case IOCTL_VOLUME_SET_GPT_ATTRIBUTES :
            InCDskDbgPrint(DL_INFO, DM_IOCTL_INFO,("IOCTL = 0x%x(IOCTL_VOLUME_SET_GPT_ATTRIBUTES) ", ulIoControlCode));
            break;
        case IOCTL_VOLUME_GET_GPT_ATTRIBUTES :
            InCDskDbgPrint(DL_INFO, DM_IOCTL_INFO,("IOCTL = 0x%x(IOCTL_VOLUME_GET_GPT_ATTRIBUTES) ", ulIoControlCode));
            break;
        case IOCTL_VOLUME_IS_CLUSTERED:
            InCDskDbgPrint(DL_INFO, DM_IOCTL_INFO,("IOCTL = 0x%x(IOCTL_VOLUME_IS_CLUSTERED) ", ulIoControlCode));
            break;
#endif // (_WIN32_WINNT >= 0x0502)
        default:
            InCDskDbgPrint(DL_INFO, DM_IOCTL_INFO,("IOCTL = 0x%x, ", ulIoControlCode));
            break;
    }

    InCDskDbgPrint(DL_INFO, DM_IOCTL_INFO,("In=0x%x, Out=0x%x\n", ulInputLength, ulOutputLength));

    return;
}

VOID
DumpIoctlInformation(
    PDEVICE_OBJECT DeviceObject, 
    PIRP    Irp
    )
{
    PIO_STACK_LOCATION  IoStackLocation = IoGetCurrentIrpStackLocation(Irp);
    ULONG               ulIoControlCode, ulDeviceType;

    ulIoControlCode = IoStackLocation->Parameters.DeviceIoControl.IoControlCode;
    ulDeviceType = IOCTL_DEVICE_TYPE(ulIoControlCode);

    switch(ulDeviceType) {
        case IOCTL_SCSI_BASE:
            DumpScsiIOCTLInfo(DeviceObject, Irp);
            break;
        case IOCTL_STORAGE_BASE:
            DumpStorageDevIOCTLInfo(DeviceObject, Irp);
            break;
#if (NTDDI_VERSION < NTDDI_VISTA)
        case FTTYPE:
            DumpFtDevIOCTLInfo(DeviceObject, Irp);
            break;
#endif
        case MOUNTDEVCONTROLTYPE:
            DumpMountDevIOCTLInfo(DeviceObject, Irp);
            break;
        case IOCTL_VOLUME_BASE:
            DumpVolumeIOCTLInfo(DeviceObject, Irp);
            break;
        case IOCTL_DISK_BASE:
            DumpDiskIOCTLInfo(DeviceObject, Irp);
            break;
        default:
            DumpUnknownDeviceTypeIOCTLInfo(DeviceObject, Irp);
            break;
    }

    return;
}

