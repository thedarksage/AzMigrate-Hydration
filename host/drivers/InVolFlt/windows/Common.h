#pragma once

extern "C"
{
#include "ntifs.h"
#include "ntddk.h"
#include "ntverp.h"
#include "ntdddisk.h"
#include "stdarg.h"
#include "stdio.h"
#include <ntddvol.h>
#include <ntintsafe.h>
#include <wdmsec.h> // for IoCreateDeviceSecure


#include <mountdev.h>
#include <mountmgr.h>
#define NTSTRSAFE_LIB
#include <ntstrsafe.h>
}

#include "InMageDebug.h"

// this .h is automatically generated from the DiskFltEvents.man file
#include "DiskFltEvents.h"


//Fix for Bug 28568
#define INITIALIZE_PNP_STATE(_Data_)    \
        (_Data_)->DevicePnPState =  NotStarted;\
        (_Data_)->PreviousPnPState = NotStarted;

#define SET_NEW_PNP_STATE(_Data_, _state_) \
        (_Data_)->PreviousPnPState =  (_Data_)->DevicePnPState;\
        (_Data_)->DevicePnPState = (_state_);

#define RESTORE_PREVIOUS_PNP_STATE(_Data_)   \
        (_Data_)->DevicePnPState =   (_Data_)->PreviousPnPState;

#define CLEAR_FLAG(_f, _b)         (  (_f) &= ~(_b))
#define GET_FLAG(_f, _b)           (  (_f) &   (_b))
#define SET_FLAG(_f, _b)           (  (_f) |=  (_b))
#define TEST_FLAG(_f, _b)          ((((_f) &   (_b)) != 0) ? TRUE : FALSE)
#define TEST_ALL_FLAGS(_f, _b)     ((((_f) &   (_b)) == (_b)) ? TRUE : FALSE)

// Define all the tags used in the driver here.
#define TAG_ASYNCIO_OBJECT              'IAnI'
#define TAG_BASECLASS_OBJECT            'CBnI'
#define TAG_BASIC_DISK                  'DBnI'
#define TAG_BASIC_VOLUME                'VBnI'
#define TAG_BITMAP_WORK_ITEM            'WBnI'
#define TAG_BITRUN_LENOFFSET_POOL       'OBnI'
#define TAG_CHANGE_NODE_POOL            'NCnI'
#define TAG_CHANGE_BLOCK_POOL           'BCnI'
#define TAG_DIRTY_BLOCKS_POOL           'BDnI'
#define TAG_DATA_BLOCK                  'bDnI'
#define TAG_DATA_POOL_CHUNK             'PDnI'
#define TAG_FILESTREAM_OBJECT           'SFnI'
#define TAG_FSSEGMENTMAPPER_OBJECT      'MFnI'
#define TAG_FILE_WRITER_ENTRY           'WFnI'
#define TAG_FILE_WRITER_THREAD_ENTRY    'TFnI'
#define TAG_GENERIC_NON_PAGED           'NGnI'
#define TAG_GENERIC_PAGED               'PGnI'
#define TAG_IOBUFFER_OBJECT             'BInI'
#define TAG_IOBUFFER_DATA               'DInI'      // This is paged pool memory.
#define TAG_NONPAGED_ALLOCATED          'ANnI'
#define TAG_NODE_LIST_POOL              'LNnI'
#define TAG_REGISTRY_OBJECT             'GRnI'
#define TAG_REGISTRY_DATA               'DRnI'
#define TAG_REGISTRY_MULTISZ            'MRnI'
#define TAG_SEGMENTEDBITMAP_OBJECT      'BSnI'
#define TAG_TAG_BUFFER                  'aTnI'
#define TAG_DEV_BITMAP                  'MBnI'
#define TAG_DEV_CONTEXT                 'CDnI'
#define TAG_VALUE_DISTRIBUTION          'DVnI'
#define TAG_VALUE_LOG                   'LVnI'
#define TAG_DEV_NODE                    'NVnI'
#define TAG_WORK_QUEUE_ENTRY            'EWnI'
#define TAG_WRITER_NODE                 'NWnI'
#define TAG_TAG_NODE                    'NTnI'
#define TAG_FS_FLUSH                    'SFnI'

//Fix for Bug 28568
#define TAG_REMOVE_LOCK                 'LRnI'
#define TAG_DEVICE_INFO                 'IDnI'
#define TAG_PAGE_FILE                   'FPnI'
#define TAG_DB_TAG_INFO                 'ITdT'
#define TAG_COMPLETION_CONTEXT_POOL     'PCnI'
#define TAG_TELEMETRY_INFO              'TTnI'
#define TAG_FILE_RAW_IO_WRAPPER         'WRnI'
#define TAG_FILE_RAW_IO_VALIDATION      'VRnI'
#define TAG_BITMAP_HEADER               'HBnI'
#define TAG_FS_QUERY                    'QFnI'
#define TAG_DISKSTATE_POOL              'tAtS'

#define REG_CLUSTER_KEY                 L"\\REGISTRY\\MACHINE\\Cluster"
#define REG_CLUSDISK_KEY                L"\\REGISTRY\\MACHINE\\SYSTEM\\CurrentControlSet\\Services\\ClusDisk"
#define REG_CLUSDISK_SIGNATURES_KEY     L"\\REGISTRY\\MACHINE\\SYSTEM\\CurrentControlSet\\Services\\ClusDisk\\Parameters\\Signatures"
#define REG_CLUSDISK_PARAMETERS_KEY     L"\\REGISTRY\\MACHINE\\SYSTEM\\CurrentControlSet\\Services\\ClusDisk\\Parameters"
#define DOS_DEVICES_VOLUME_PREFIX       L"\\DosDevices\\Volume{"
#define SYSTEM_ROOT_PATH                L"\\SystemRoot\\"

// Log file name format is Prefix + GUID + Suffix
#define LOG_FILE_NAME_PREFIX            L"InMage-"
#define LOG_FILE_NAME_SUFFIX_VOLUME     L".VolumeLog"
#define LOG_FILE_NAME_SUFFIX_DISK       L".DiskLog"
#define LOG_FILE_NAME_FOR_CLUSTER_VOLUME    LOG_FILE_NAME_PREFIX L"ClusterVolume" LOG_FILE_NAME_SUFFIX

#define DISK_NAME_PREFIX                L"\\Device\\Harddisk"

#define STRING_LEN_NULL_TERMINATED      0xFFFFFFFF

#define MAX_NUM_DRIVE_LETTERS   26

#define GIGABYTES           (1024*1024*1024)
#define MEGABYTES           (0x100000)      // (1024*1024)
#define KILOBYTES           (0x400)         // (1024)
#define FIVE_TWELVE_K_SIZE          (0x80000)
#define TWO_FIFTY_SIX_K_SIZE        (0x40000)
#define ONE_TWENTY_EIGHT_K_SIZE     (0x20000)
#define SIXTY_FOUR_K_SIZE           (0x10000)
#define THIRTY_TWO_K_SIZE           (0x8000)
#define SIXTEEN_K_SIZE              (0x4000)
#define EIGHT_K_SIZE                (0x2000)
#define FOUR_K_SIZE                 (0x1000)
#define HUNDRED_NANO_SECS_IN_MILLISEC    (10 * 1000)
#define HUNDRED_NANO_SECS_IN_SEC    (10 * 1000 * 1000)
#define SECONDS_IN_MINUTE           (60)

#define SECTOR_SIZE         512

#define SINGLE_MAX_WRITE_LENGTH     0x10000 // (64 KBytes)
#define MAX_LOG_PATHNAME            (0x200)

bool
InMageFltHasEventLimitReached(
);

// TODO-SanKumar-1608: read these values from registry key.

// maximum number of event log messages to write per time interval
#define EVENTLOG_SQUELCH_MAXIMUM_EVENTS (200)

// time interval in system time units (100ns), 7200 seconds = 2 hours
#define EVENTLOG_SQUELCH_TIME_INTERVAL (10000000i64 * 7200i64)

// channels to which the squelching is applied; other channels are unlimited
#define IS_EVENTLOG_SQUELCHED_CHANNEL(Channel) (INDSKFLT_CHANNEL_ADMIN == Channel)

// check if the logging should be squelched
#define IS_EVENTLOG_SQUELCH_NEEDED(Descriptor) (IS_EVENTLOG_SQUELCHED_CHANNEL(Descriptor.Channel) ? \
                                                InMageFltHasEventLimitReached() \
                                                : false)


// TODO-SanKumar-1608: enable the use of Activity Id to group the actions.
// currently ignored by passing NULL in the event write helpers.

// performing the below XPAND(x) trivia operation due to Visual C++ behavior:
// http://stackoverflow.com/questions/5134523/msvc-doesnt-expand-va-args-correctly
#define XPAND(x) x

// suppressing side effect warning of the fix for the above mentioned bug.
// suppresssing pointer to unsigned long conversion.
#pragma warning (disable:4302)
#pragma warning (disable:4311)
#pragma warning (disable:4002)

// all parameters should be from system-space memory (and non-paged memory, if IRQL >= DISPATCH_LEVEL)
#define InDskFltWriteEvent(Descriptor, ...) (IS_EVENTLOG_SQUELCH_NEEDED(Descriptor) ? \
                                             STATUS_IMPLEMENTATION_LIMIT \
                                             : XPAND(EventWrite##Descriptor(NULL, __VA_ARGS__)))

// write InDskFlt Etw event with Disk number and Disk ID
// all parameters should be from system-space memory (and non-paged memory, if IRQL >= DISPATCH_LEVEL)
#define InDskFltWriteEventWithDevCtxt(Descriptor, DeviceContext, ...)   InDskFltWriteEvent(Descriptor, \
                                                                            (NULL != DeviceContext)? (DeviceContext)->ulDevNum : MAXULONG, \
                                                                            (NULL != DeviceContext)? (DeviceContext)->wDevID : L"", \
                                                                            __VA_ARGS__)


extern ULONG    ulDebugLevelForAll;
extern ULONG    ulDebugLevel;
extern ULONG    ulDebugMask;

#if DBG
#define InVolDbgPrint(Level, Mod, x)                            \
{                                                               \
    if ((ulDebugLevelForAll >= (Level)) ||                      \
        ((ulDebugMask & (Mod)) && (ulDebugLevel >= (Level))))   \
        DbgPrint x;                                             \
}
#else
#define InVolDbgPrint(Level, Mod, x)  
#endif // DBG

#if DBG
// Undocumented Calls. This call should be used in debug code to easily find the process name until
// driver has it's own implementation
extern "C" {
    PCHAR   PsGetProcessImageFileName(PEPROCESS Process);
}
#endif

#define STATUS_INMAGE_OBJECT_CREATED       ((NTSTATUS)0x28000001)

#include "InmFltInterface.h"
#include "WorkQueue.h"
#include "DriverContext.h"
#include "Bypass.h"

