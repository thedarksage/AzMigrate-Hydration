#ifndef _VV_DRIVER_CONTEXT_H_
#define _VV_DRIVER_CONTEXT_H_
#include "VVCommon.h"
#include "VVCmdQueue.h"
#include "VVolumeImage.h"
#include "WinVsnapKernelHelpers.h"

#define DEFAULT_NUMBER_OF_THREADS_FOR_VSNAP_THREAD_POOL		0x20
#define DEFAULT_NUMBER_OF_THREADS_FOR_VVOLUME_THREAD_POOL   0x08
#define NUMBER_OF_THREADS_FOR_VSNAP_THREAD_POOL             L"NumberOfThreadsForVsnapThreadPool"
#define NUMBER_OF_THREADS_FOR_VVOLUME_THREAD_POOL           L"NumberOfThreadsForVVolumeThreadPool"
#define MEMORY_FOR_FILE_HANDLE_CACHE_KB                     L"MemoryForFileHandleCacheInKB" 
#define FLAG_VIRTUAL_VOLUME                                 0x00000001


#define PARAMETERS_KEY                      L"\\Parameters"
// 32(8*4) bytes gives 256 bits.
#define BITMAPSIZE_IN_ULONG     64
#define BITMAPSIZE_IN_BITS      (BITMAPSIZE_IN_ULONG * 8 * sizeof(ULONG))

#define VV_DC_FLAGS_CONTROL_DEVICE_CREATED          0x0001
#define VV_DC_FLAGS_COMMAND_POOL_INITIALIZED        0x0004
#define VV_DC_FLAGS_SIMULATE_OUT_OF_ORDER_WRITE     0x0100

#define VV_DE_MOUNT_SUCCESS                         0x0001
#define VV_DE_MOUNT_LINK_CREATED                    0x0002
#define VV_DE_MOUNT_DOS_DEVICE_NAME_CREATED         0x0004

#define VVOLUME_CMD_INVALID     0x00
#define VVOLUME_CMD_READ        0x01
#define VVOLUME_CMD_WRITE       0x02
#define VVOLUME_MAX_CMDS        0x03
#define MEMORY_FOR_TAG          0x8
#define KILO_BYTE               0x400
#ifdef _WIN64
#define DEFAULT_PAGED_MEM_FILE_HANDLE_CACHE_KB     (0x60)//48
#define MIN_PAGED_MEM_FOR_FILE_HANDLE_CACHE_KB     (0x30)
#define MAX_PAGED_MEM_FOR_FILE_HANDLE_CACHE_KB     (0x180)
#else
#define DEFAULT_PAGED_MEM_FILE_HANDLE_CACHE_KB     (0x40)//32
#define MIN_PAGED_MEM_FOR_FILE_HANDLE_CACHE_KB     (0x20)
#define MAX_PAGED_MEM_FOR_FILE_HANDLE_CACHE_KB     (0x100)
#endif

// maximum number of event log messages to write per time interval
#define EVENTLOG_SQUELCH_MAXIMUM_EVENTS (30)
// time interval in system time units (100ns), 1800 seconds = .5 hour
#define EVENTLOG_SQUELCH_TIME_INTERVAL (10000000i64 * 1800i64)
#if (NTDDI_VERSION >= NTDDI_WS03)
typedef struct _IMPERSONATION_CONTEXT
{
    PSECURITY_CLIENT_CONTEXT pSecurityContext;
    KGUARDED_MUTEX           GuardedMutex;
    bool 	             bSecurityContext;
} IMPERSONATION_CONTEXT, *PIMPERSONATION_CONTEXT;
#endif
typedef struct _DRIVER_CONTEXT {
    UNICODE_STRING  usDriverName;
    UNICODE_STRING  DriverParameters;
    PDRIVER_OBJECT  DriverObject;
    PDEVICE_OBJECT  ControlDevice;
    LIST_ENTRY      DeviceContextHead;
    LIST_ENTRY      CmdQueueList;
    KEVENT          ShutdownEvent;
    KSPIN_LOCK      Lock;
    ULONG           BufferForDeviceBitMap[BITMAPSIZE_IN_ULONG];
    RTL_BITMAP      BitMap;
    HANDLE          hInMageDirectory;
    ULONG           ulFlags;
    NPAGED_LOOKASIDE_LIST   CommandPool;
    NPAGED_LOOKASIDE_LIST   IOCommandPool;
    LONG            lCommandStructsAllocated;
    LONG            lIOCommandStructsAllocated;
    LONG            lVsnapTaskAllocated;

    ULONG           ulControlFlags;
    PCOMMAND_ROUTINE  VVolumeImageCmdRoutines[VVOLUME_MAX_CMDS];
    PCOMMAND_ROUTINE  VVolumeRetentionCmdRoutines[VVOLUME_MAX_CMDS];
    KEVENT          hTestThreadShutdown;
    KMUTEX          hMutex;
    PKTHREAD        OutOfOrderTestThread;
    /**
     * PendingQueueMutex is used to synchrnonize access to pCommandArray global variable
     * defined in VVolumeImageTest.cpp. All context related to out of order test should
     * be consolidated into one structure.
     * */
    KMUTEX          PendingQueueMutex;
    KMUTEX          FileHandleCacheMutex;
    PKTHREAD        *pVsnapPoolThreads;
    PKTHREAD        *pVVolumePoolThreads;
    ULONG           ulNumberOfVsnapThreads;    
    ULONG           ulNumberOfVVolumeThreads;
    ULONG           ulMemoryForFileHandleCache;  
	ULONG           NbrRecentEventLogMessages;
	ULONG           NbrMissingEventLogMessages;
	LARGE_INTEGER   EventLogTimePeriod;
#if (NTDDI_VERSION >= NTDDI_WS03)
    IMPERSONATION_CONTEXT ImpersonatioContext;
#endif
} DRIVER_CONTEXT, *PDRIVER_CONTEXT;

extern DRIVER_CONTEXT   DriverContext;

#define DEVICE_TYPE_CONTROL                     0x01
#define DEVICE_TYPE_VVOLUME_FROM_VOLUME_IMAGE   0x02
#define DEVICE_TYPE_VVOLUME_FROM_RETENTION_LOG  0x04

#define DEVICE_FLAGS_SYMLINK_CREATED  0x0001

typedef struct _RETENTION_CONTEXT
{
    HANDLE          hFile;
}RETENTION_CONTEXT, PRETENTION_CONTEXT;

typedef struct _DEVICE_EXTENSION
{
    LIST_ENTRY      ListEntry;
    ULONG           ulDeviceType;
    ULONG           ulFlags;
    LONG            lRefCount;
    PDEVICE_OBJECT  DeviceObject;
    IO_REMOVE_LOCK  IoRemoveLock;
    union{
        PCOMMAND_QUEUE  CmdQueue;
        PIOCOMMAND_QUEUE IOCmdQueue;
    };
    ULARGE_INTEGER  uliVolumeSize;
    HANDLE          hFile;
    UNICODE_STRING  usFileName;
    UNICODE_STRING  usDeviceName;
    UNICODE_STRING  MountDevUniqueId;
    UNICODE_STRING  usMountDevLink;
    UNICODE_STRING  usMountDevDeviceName;
    PVOID           UniqueVolumeID;
    ULONG           UniqueVolumeIDLength;
    PDEVICE_OBJECT  LowerDevice;
    PVOID           VolumeContext;
    LIST_ENTRY      DiskChangeListHead;
    ULONG           ulVolumeNumber;
	ULONG           ulSectorSize;
    
} DEVICE_EXTENSION, *PDEVICE_EXTENSION;

#define VV_CONTROL_DEVICE_SYMLINK_NAME     L"\\DosDevices\\Global\\InMageVVolume"

#define INMAGE_IS_MOUNTDEV_NAME_VOLUME_GUID(s) (                                            \
    ((s)->NameLength == 0x60 || ((s)->NameLength == 0x62 && (s)->Name[48] == '\\')) &&      \
    (s)->Name[0] == '\\' &&                                                                 \
    ((s)->Name[1] == '?' || (s)->Name[1] == '\\') &&                                        \
    (s)->Name[2] == '?' &&                                                                  \
    (s)->Name[3] == '\\' &&                                                                 \
    (s)->Name[4] == 'V' &&                                                                  \
    (s)->Name[5] == 'o' &&                                                                  \
    (s)->Name[6] == 'l' &&                                                                  \
    (s)->Name[7] == 'u' &&                                                                  \
    (s)->Name[8] == 'm' &&                                                                  \
    (s)->Name[9] == 'e' &&                                                                  \
    (s)->Name[10] == '{' &&                                                                 \
    (s)->Name[19] == '-' &&                                                                 \
    (s)->Name[24] == '-' &&                                                                 \
    (s)->Name[29] == '-' &&                                                                 \
    (s)->Name[34] == '-' &&                                                                 \
    (s)->Name[47] == '}'                                                                    \
    )

NTSTATUS
InitializeDriverContext(PDRIVER_CONTEXT pDriverContext, PDRIVER_OBJECT DriverObject, PUNICODE_STRING    Registry);

NTSTATUS
CleanupDriverContext(PDRIVER_CONTEXT pDriverContext);

VOID
AddDeviceToDeviceList(PDRIVER_CONTEXT pDriverContext, PDEVICE_OBJECT DeviceObject);

#endif // _VV_DRIVER_CONTEXT_H_