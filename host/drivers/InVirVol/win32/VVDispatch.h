#ifndef _VV_DISPATCH_H_
#define _VV_DISPATCH_H_

extern "C"  NTSTATUS
DriverEntry(
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath
    );

VOID
VVDriverUnload(IN PDRIVER_OBJECT DriverObject);

NTSTATUS VVDispatchCreate(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp);
NTSTATUS VVDispatchClose(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp);
NTSTATUS VVDispatchRead(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp);
NTSTATUS VVDispatchWrite(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp);
NTSTATUS VVDispatchDeviceControl(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp);
NTSTATUS VVAddDevice(PDRIVER_OBJECT DriverObject, PDEVICE_OBJECT pdo);
NTSTATUS VVDispatchPnp(PDEVICE_OBJECT DeviceObject,PIRP Irp);
NTSTATUS VVDispatchFlushBuffers(PDEVICE_OBJECT DeviceObject,PIRP Irp);
NTSTATUS BypassCheckBootIni();
void VsnapLogError(PDRIVER_OBJECT DriverObject,ULONG uniqueId, PWCHAR str);
NTSTATUS
VVHandleDeviceControlForCommonIOCTLCodes(PDEVICE_OBJECT DeviceObject, PIRP Irp);
VOID
InVirVolLogError(
    IN PDEVICE_OBJECT deviceObject,
    IN ULONG uniqueId,
    IN NTSTATUS messageId,
    IN NTSTATUS finalStatus,
    IN PCWSTR string1 = NULL,
    IN ULONG maxLength1 = 0,
    IN PCWSTR string2 = NULL,
    IN ULONG maxLength2 = 0,
    IN PCWSTR string3 = NULL,
    IN ULONG maxLength3 = 0,
    IN PCWSTR string4 = NULL,
    IN ULONG maxLength4 = 0
    );


VOID
InVirVolLogError( 
    IN PDEVICE_OBJECT deviceObject,
    IN ULONG uniqueId,
    IN NTSTATUS messageId,
    IN NTSTATUS finalStatus,
    IN ULONG InsertionValue1,
    IN ULONG InsertionValue2 = 0
    );

VOID
InVirVolLogError(
    IN PDEVICE_OBJECT deviceObject,
    IN ULONG uniqueId,
    IN NTSTATUS messageId,
    IN NTSTATUS finalStatus,
    IN PCWSTR string1,
    IN ULONG maxLength1,
    IN PCWSTR string2,
    IN ULONG maxLength2,
    IN ULONGLONG InsertionValue3
    );

VOID
InVirVolLogError(
    IN PDEVICE_OBJECT deviceObject,
    IN ULONG uniqueId,
    IN NTSTATUS messageId,
    IN NTSTATUS finalStatus,
    IN PCWSTR string1,
    IN ULONG maxLength1,
    IN ULONG InsertionValue2,
    IN ULONGLONG InsertionValue3,
    IN ULONG InsertionValue4 = 0
    );

VOID
InVirVolLogError(
    IN PDEVICE_OBJECT deviceObject,
    IN ULONG uniqueId,
    IN NTSTATUS messageId,
    IN NTSTATUS finalStatus,
    IN PCWSTR string1,
    IN ULONG maxLength1,
    IN ULONGLONG InsertionValue2,
    IN ULONGLONG InsertionValue3 = 0,
    IN ULONGLONG InsertionValue4 = 0
    );
#endif // _VV_DISPATCH_H_