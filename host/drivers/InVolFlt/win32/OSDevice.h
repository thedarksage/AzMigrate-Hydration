#ifndef _INMAGE_OS_DEVICE_H_
#define _INMAGE_OS_DEVICE_H_

typedef enum _DEVICE_PNP_STATE {

    NotStarted = 0,         // Not started yet
    Started,                // Device has received the START_DEVICE IRP
    StopPending,            // Device has received the QUERY_STOP IRP
    Stopped,                // Device has received the STOP_DEVICE IRP
    RemovePending,          // Device has received the QUERY_REMOVE IRP
    SurpriseRemovePending,  // Device has received the SURPRISE_REMOVE IRP
    Deleted,                 // Device has received the REMOVE_DEVICE IRP
    
    //Fix for bug 25726. Place this at the end.
    LimboState              //This can only  happen when our driver misses a PNP remove and devstack flattened thereof.
} DEVICE_PNP_STATE;

// abstract class for an OS device object, by default all requests get rejected
class OSDevice : public BaseClass {
public:
    void * __cdecl operator new(size_t size) {return malloc(size, 'DOmI', NonPagedPool);};
    OSDevice();
    OSDevice(PDRIVER_OBJECT driver);
    virtual ~OSDevice();
    virtual NTSTATUS CreateDevice(PCWSTR nameString);
    virtual NTSTATUS DeleteDevice();
    virtual NTSTATUS Dispatch(PIRP Irp);
    PDEVICE_OBJECT GetDeviceObject(void) { return osDevice;};
    // IRP MAJOR dispatch functions
    virtual NTSTATUS Create_Dispatch(PIRP Irp){ return STATUS_INVALID_DEVICE_REQUEST;};
    virtual NTSTATUS Create_named_pipe_Dispatch(PIRP Irp){ return STATUS_INVALID_DEVICE_REQUEST;};
    virtual NTSTATUS Close_Dispatch(PIRP Irp){ return STATUS_INVALID_DEVICE_REQUEST;};
    virtual NTSTATUS Read_Dispatch(PIRP Irp){ return STATUS_INVALID_DEVICE_REQUEST;};
    virtual NTSTATUS Write_Dispatch(PIRP Irp){ return STATUS_INVALID_DEVICE_REQUEST;};
    virtual NTSTATUS Query_information_Dispatch(PIRP Irp){ return STATUS_INVALID_DEVICE_REQUEST;};
    virtual NTSTATUS Set_information_Dispatch(PIRP Irp){ return STATUS_INVALID_DEVICE_REQUEST;};
    virtual NTSTATUS Query_ea_Dispatch(PIRP Irp){ return STATUS_INVALID_DEVICE_REQUEST;};
    virtual NTSTATUS Set_ea_Dispatch(PIRP Irp){ return STATUS_INVALID_DEVICE_REQUEST;};
    virtual NTSTATUS Flush_buffers_Dispatch(PIRP Irp){ return STATUS_INVALID_DEVICE_REQUEST;};
    virtual NTSTATUS Query_volume_information_Dispatch(PIRP Irp){ return STATUS_INVALID_DEVICE_REQUEST;};
    virtual NTSTATUS Set_volume_information_Dispatch(PIRP Irp){ return STATUS_INVALID_DEVICE_REQUEST;};
    virtual NTSTATUS Directory_control_Dispatch(PIRP Irp){ return STATUS_INVALID_DEVICE_REQUEST;};
    virtual NTSTATUS File_system_control_Dispatch(PIRP Irp){ return STATUS_INVALID_DEVICE_REQUEST;};
    virtual NTSTATUS Device_control_Dispatch(PIRP Irp){ return STATUS_INVALID_DEVICE_REQUEST;};
    virtual NTSTATUS Internal_device_control_Dispatch(PIRP Irp){ return STATUS_INVALID_DEVICE_REQUEST;};
    virtual NTSTATUS Shutdown_Dispatch(PIRP Irp){ return STATUS_INVALID_DEVICE_REQUEST;};
    virtual NTSTATUS Lock_control_Dispatch(PIRP Irp){ return STATUS_INVALID_DEVICE_REQUEST;};
    virtual NTSTATUS Cleanup_Dispatch(PIRP Irp){ return STATUS_INVALID_DEVICE_REQUEST;};
    virtual NTSTATUS Create_mailslot_Dispatch(PIRP Irp){ return STATUS_INVALID_DEVICE_REQUEST;};
    virtual NTSTATUS Query_security_Dispatch(PIRP Irp){ return STATUS_INVALID_DEVICE_REQUEST;};
    virtual NTSTATUS Set_security_Dispatch(PIRP Irp){ return STATUS_INVALID_DEVICE_REQUEST;};
    virtual NTSTATUS Power_Dispatch(PIRP Irp){ return STATUS_INVALID_DEVICE_REQUEST;};
    virtual NTSTATUS System_control_Dispatch(PIRP Irp){ return STATUS_INVALID_DEVICE_REQUEST;};
    virtual NTSTATUS Device_change_Dispatch(PIRP Irp){ return STATUS_INVALID_DEVICE_REQUEST;};
    virtual NTSTATUS Query_quota_Dispatch(PIRP Irp){ return STATUS_INVALID_DEVICE_REQUEST;};
    virtual NTSTATUS Set_quota_Dispatch(PIRP Irp){ return STATUS_INVALID_DEVICE_REQUEST;};
    virtual NTSTATUS Pnp_Dispatch(PIRP Irp){ return STATUS_INVALID_DEVICE_REQUEST;};
protected:
    virtual void Initialize(void){}; // set subclass specific values in object
    PDRIVER_OBJECT osDriver;
    PDEVICE_OBJECT osDevice;
    IO_REMOVE_LOCK removeLock;
    KEVENT pagingPathCountEvent;
    tInterlockedLong  pagingPathCount;
    DEVICE_PNP_STATE devicePnPState;
    DEVICE_PNP_STATE devicePreviousPnPState;
    // thse are used when creating a device
    GUID deviceClassGUID;
    ULONG32 deviceFlags;
    DEVICE_TYPE deviceType;
    PCUNICODE_STRING securityDescriptorString;
    BOOLEAN exclusiveDevice;

}; 

#endif // _INMAGE_OS_DEVICE_H_
