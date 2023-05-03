#ifndef _INMAGE_CLUSDISK_FILTER__H
#define _INMAGE_CLUSDISK_FILTER__H

#include <ntddscsi.h>

#define MAX_REINITIALIZE_COUNT 20

#define DRVCXT_FLAGS_CD_POOL_INITIALZIED    0x00000001
#define DRVCXT_FLAGS_DEVICE_ATTACHED        0x00000010

#define REG_CLUSDISK_SIGNATURES_KEY     L"\\REGISTRY\\MACHINE\\SYSTEM\\CurrentControlSet\\Services\\ClusDisk\\Parameters\\Signatures"

typedef struct _DRIVER_CONTEXT {
    // has to 8-byte aligned
    FAST_MUTEX              ClusDiskListLock;
    LIST_ENTRY              ClusDiskListHead;
    PAGED_LOOKASIDE_LIST    ClusDiskNodePool;

    UNICODE_STRING          DriverRegPath;

    PDRIVER_OBJECT          DriverObject;
    PDEVICE_OBJECT          ClusDisk0FiDO;
    PDEVICE_OBJECT          ClusDisk0PDO;

    // Data about InMageFilter Control device
    PDEVICE_OBJECT          InMageFilterDO;
    PFILE_OBJECT            InMageFilterFO;

    ULONG                   ulFlags;
} DRIVER_CONTEXT, *PDRIVER_CONTEXT;

#define IOCTL_CLUSTER_ACQUIRE_SCSI_DEVICE   CTL_CODE(IOCTL_SCSI_BASE, 0x0508, METHOD_BUFFERED, FILE_WRITE_ACCESS)
#define IOCTL_CLUSTER_RELEASE_SCSI_DEVICE   CTL_CODE(IOCTL_SCSI_BASE, 0x0509, METHOD_BUFFERED, FILE_WRITE_ACCESS)
#define IOCTL_CLUSTER_DETACH                CTL_CODE(IOCTL_SCSI_BASE, 0x0506, METHOD_BUFFERED, FILE_WRITE_ACCESS)
#define IOCTL_CLUSTER_ATTACH                CTL_CODE(IOCTL_SCSI_BASE, 0x0505, METHOD_BUFFERED, FILE_WRITE_ACCESS)
#define IOCTL_CLUSTER_ATTACH_EX             CTL_CODE(IOCTL_SCSI_BASE, 0x0526, METHOD_BUFFERED, FILE_WRITE_ACCESS)

typedef struct _DEVICE_EXTENSION {

    //
    // Back pointer to device object
    //

    PDEVICE_OBJECT DeviceObject;

    //
    // Target Device Object
    //

    PDEVICE_OBJECT TargetDeviceObject;

} DEVICE_EXTENSION, *PDEVICE_EXTENSION;

#define DEVICE_EXTENSION_SIZE sizeof(DEVICE_EXTENSION)

#define DM_DRIVER_INIT      0x0001
#define DM_IOCTL_INFO       0x0002
#define DM_VOLUME_ACQUIRE   0x0004
#define DM_VOLUME_RELEASE   0x0008
#define DM_DISK_DETACH      0x0010
#define DM_DISK_ATTACH      0x0020

#define DL_NONE         0x00000000
#define DL_ERROR        0x00000001
#define DL_WARNING      0x00000002
#define DL_INFO         0x00000003
#define DL_VERBOSE      0x00000004
#define DL_FUNC_TRACE   0x00000005
#define DL_TRACE_L1     0x00000006
#define DL_TRACE_L2     0x00000007

// In Windows 2008 version of failover clustering. Cluster disk device name
// is changed to \Device\ClusDisk
// Vista and Windows 2008 are using same kernel, so lets use NTDDI_VISTA to
// define a different device name.
#if (NTDDI_VERSION >= NTDDI_VISTA)
#define CLUSTER_DEVICE_NAME     L"\\Device\\ClusDisk"
#else
#define CLUSTER_DEVICE_NAME     L"\\Device\\ClusDisk0"
#endif // NTDDI_VERSION >= NTDDI_VISTA

extern ULONG    ulDebugLevelForAll;
extern ULONG    ulDebugLevel;
extern ULONG    ulDebugMask;

#if DBG
#define InCDskDbgPrint(Level, Mod, x)                       \
{                                                           \
    if ((ulDebugLevelForAll >= Level) ||                    \
        ((ulDebugMask & Mod) && (ulDebugLevel >= Level)))   \
        DbgPrint x;                                         \
}
#else
#define InCDskDbgPrint(Level, Mod, x)  
#endif // DBG

#if (NTDDI_VERSION >= NTDDI_VISTA)
// In windows 2012, START_RESERVE struct has 2 Unknown may be ULONG bytes(guess, not disassembled) extra than Windows 2008 
#define SupportBytes 2 * sizeof(ULONG)

typedef enum {
    ecPartitionStyleMBR = 0,
    ecPartitionStyleGPT = 1,
    ecPartitionStyleUnknown = 0x0FA0,
} etPartitionStyle;
typedef struct _DISK_INFO
{
    etPartitionStyle   ePartitionStyle;
    union {
        ULONG   ulSignature;
        GUID    Guid;
    };
} DISK_INFO, *PDISK_INFO;
// Windows 2012 has StructSize 0x68 
// > = Windows Vista has StructSize 0x60
typedef struct _START_RESERVE
{
    ULONG           StructSize;         // 0x00: This field is always set to value 0x60. This is the size of this structure
    ULONG           ulUnKnown_0;        // 0x04: This value is saved in DISK_RESERVE structure at offset 0x68
    DISK_INFO       DiskInfo;           // 0x08: This has the Disk Partition Style and Disk Signature or GUID
    ULONG           DeviceNumber;       // 0x1C: This is the ID in \Device\HardDisk%d
                                        // This can be obtained from IOCTL_STORAGE_GET_DEVICE_NUMBER
    ULONG           ulUnknown[16];      // 0x20: Did not disassemble these fileds yet.
} START_RESERVE, *PSTART_RESERVE;

#endif // NTDDI_VERSION >= NTDDI_VISTA
#endif // _INMAGE_CLUSDISK_FILTER__H