//
// Copyright InMage Systems 2005
//

// As our driver is a boot driver, this file implements bypass functions so the driver can
// still be loaded and be assured it will not break
//
// The code in here is EXACTLY the code from the MSFT Toaster filter example, it's hopefully bug free
//

NTSTATUS
BypassCheckBootIni();

NTSTATUS
BypassAddDevice(
    IN PDRIVER_OBJECT DriverObject,
    IN PDEVICE_OBJECT PhysicalDeviceObject
    );

NTSTATUS
BypassDispatchPnp (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
BypassDispatchPower(
    IN PDEVICE_OBJECT    DeviceObject,
    IN PIRP              Irp
    );

VOID
BypassUnload(
    IN PDRIVER_OBJECT DriverObject
    );

NTSTATUS
BypassPass (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
BypassStartCompletionRoutine(
    IN PDEVICE_OBJECT   DeviceObject,
    IN PIRP             Irp,
    IN PVOID            Context
    );

NTSTATUS
BypassDeviceUsageNotificationCompletionRoutine(
    IN PDEVICE_OBJECT   DeviceObject,
    IN PIRP             Irp,
    IN PVOID            Context
    );
