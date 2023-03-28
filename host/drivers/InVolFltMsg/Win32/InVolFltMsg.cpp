#include "ntddk.h"
#include "InVolFltLog.h"
extern "C" {
NTSTATUS
DriverEntry(
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath
    );
}
extern "C" NTSTATUS DriverEntry(
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath
    )
{
    ULONG               ulIndex;
    PDRIVER_DISPATCH   *dispatch;

    for (ulIndex = 0, dispatch = DriverObject->MajorFunction;
         ulIndex <= IRP_MJ_MAXIMUM_FUNCTION;
         ulIndex++, dispatch++) {
         *dispatch = NULL;
    }
    DriverObject->DriverExtension->AddDevice            = NULL;
    DriverObject->DriverUnload                          = NULL;

    return(STATUS_SUCCESS);
}