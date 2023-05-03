#ifndef _DRVUTIL_H__
#define _DRVUTIL_H__

#include <vector>
#include <map>
#include "VFltCmds.h"
#include "VVolCmds.h"
#include <strsafe.h>
#include "inmsafecapis.h"

#ifndef UNICODE  
typedef std::string             String;
typedef std::stringstream       StringStream;
#define __TFUNCTION__           __FUNCTION__
#define _tmemset                memset
#else
typedef std::wstring            String;
typedef std::wstringstream      StringStream;
#define __TFUNCTION__           __FUNCTIONW__
#define _tmemset                wmemset
#endif

#define INDSKFLT_KEY _T("SYSTEM\\CurrentControlSet\\Services\\indskflt\\Parameters")
#define KERNEL_TAGS     _T("KernelTags")

#define OS_MAJOR_VERSION_KEY        _T("OsMajorVersion")
#define OS_MINOR_VERSION_KEY        _T("OsMinorVersion")

typedef struct _STRING_TO_ULONG
{
    TCHAR   *pString;
    ULONG   ulValue;
} STRING_TO_ULONG, *PSTRING_TO_ULONG;

#define IS_MOUNT_POINT(x)       ((_tcslen(x) > 3) && x[1] == _T(':') && x[2] == _T('\\')) 
#define IS_DRIVE_LETTER(x)  ( ((x >= _T('a')) && (x <= _T('z'))) || ((x >= _T('A')) && (x <= _T('Z'))) )
#define IS_SUB_OPTION(x)    ( (_tcslen(x) >= 2) && (_T('-') == (x)[0]) && (_T('-') != (x)[1]) )
#define IS_OPTION(x)        ( (_tcslen(x) >= 1) && (_T('-') == (x)[0]) )
#define IS_MAIN_OPTION(x)   ( (_tcslen(x) >= 2) && (_T('-') == (x)[0]) && (_T('-') == (x)[1]) )
#define IS_DRIVE_LETTER_PAR(x)  ( ((_tcslen(x) == 1) || (_tcslen(x) == 2)) && (_T('-') != (x)[0]))
#define IS_COMMAND(x)           ( (_tcslen(x) > 1) ? ((_T('-') == (x)[0]) && (_T('-') == (x)[1])) : (_T('-') == (x)[0]))
#define IS_COMMAND_PAR(x)       (_T('-') != (x)[0])
#define IS_NOT_COMMAND_PAR(x)   (_T('-') == (x)[0])
#define IS_HYPHEN(x,y)     ( ((y == 8) || (y == 13) || (y == 18) || (y == 23)) &&  ((x)[y] == _T('-')) )
#define CMDSTR_VERBOSE                      _T("--Verbose")
#define CMDSTR_VERBOSE_SF                   _T("--v")
#define CMDSTR_WAIT_TIME                    _T("--WaitTime")
#define CMDSTR_WAIT_FOR_KEYWORD             _T("--WaitForKeyword")
#define CMDSTR_WAIT_FOR_KEYWORD_SF          _T("--wk")

#define CMDSTR_CREATE_THREAD                _T("--CreateThread")
#define CMDSTR_END_OF_THREAD                _T("--EndThread")
#define CMDSTR_WAIT_FOR_THREAD              _T("--WaitForThread")

#define CMDSTR_WRITE_STREAM                 _T("--WriteStream")
#define CMDOPTSTR_MAX_IO_COUNT              _T("-MaxIoCount")
#define CMDOPTSTR_MIN_IO_SIZE               _T("-MinIoSize")
#define CMDOPTSTR_MAX_IO_SIZE               _T("-MaxIoSize")
#define CMDOPTSTR_SECONDS_TO_RUN            _T("-TimeToRun")
#define CMDOPTSTR_WRITES_PER_TAG            _T("-WritesPerTag")
#define CMDSTR_READ_STREAM                  _T("--ReadStream")

typedef struct _SERVICE_START_DATA {
    bool bDisableDataFiles;
}SERVICE_START_DATA, *PSERVICE_START_DATA;

typedef struct _TIME_OUT_DATA {
    ULONG   ulTimeout;
} TIME_OUT_DATA, *PTIME_OUT_DATA;

#define KEYWORD_SIZE    80

typedef struct _WAIT_FOR_KEYWORD_DATA {
    TCHAR   Keyword[KEYWORD_SIZE + 1];
} WAIT_FOR_KEYWORD_DATA, *PWAIT_FOR_KEYWORD_DATA;

typedef struct _CREATE_THREAD_DATA {
    PLIST_ENTRY   ListHead;
    PTCHAR        ThreadId;
} CREATE_THREAD_DATA, *PCREATE_THREAD_DATA;

typedef struct _END_OF_THREAD_DATA {
    PTCHAR   ThreadId;
} END_OF_THREAD_DATA, *PEND_OF_THREAD_DATA;

typedef struct _WAIT_FOR_THREAD_DATA {
    std::vector<PTCHAR>   *pvThreadId;
} WAIT_FOR_THREAD_DATA, *PWAIT_FOR_THREAD_DATA;

typedef struct _WRITE_STREAM_DATA {
    ULONG nMaxIOs;
    ULONG nMinIOSz;
    ULONG nMaxIOSz;
    ULONG nSecsToRun;
    ULONG nWritesPerTag;
    PTCHAR MountPoint;
    WCHAR   VolumeGUID[GUID_SIZE_IN_CHARS];
}WRITE_STREAM_DATA, *PWRITE_STREAM_DATA;

typedef struct _READ_STREAM_DATA {
    PTCHAR  MountPoint;
    WCHAR   VolumeGUID[GUID_SIZE_IN_CHARS];
}READ_STREAM_DATA, *PREAD_STREAM_DATA;

#define SET_DATA_FILE_THREAD_PRIORITY_DATA_VALID_GUID   0x0001

typedef struct _SET_DATA_FILE_THREAD_PRIORITY_DATA {
    PTCHAR  MountPoint;
    WCHAR   VolumeGUID[GUID_SIZE_IN_CHARS];
    ULONG   ulFlags;
    ULONG   ulPriority;
    ULONG   DefaultPriority;
    ULONG   PriorityForAllVolumes;
} SET_DATA_FILE_THREAD_PRIORITY_DATA, *PSET_DATA_FILE_THREAD_PRIORITY_DATA;

#define SET_WORKER_THREAD_PRIORITY_DATA_VALID_PRIORITY   0x0001
typedef struct _SET_WORKER_THREAD_PRIORITY_DATA {
    ULONG   ulFlags;
    ULONG   ulPriority;
} SET_WORKER_THREAD_PRIORITY_DATA, *PSET_WORKER_THREAD_PRIORITY_DATA;

#define SET_DATA_TO_DISK_SIZE_LIMIT_DATA_VALID_GUID   0x0001

typedef struct _SET_DATA_TO_DISK_SIZE_LIMIT_DATA {
    PTCHAR  MountPoint;
    WCHAR   VolumeGUID[GUID_SIZE_IN_CHARS];
    ULONG   ulFlags;
    ULONG   ulDataToDiskSizeLimit;
    ULONG   DefaultDataToDiskSizeLimit;
    ULONG   DataToDiskSizeLimitForAllVolumes;
} SET_DATA_TO_DISK_SIZE_LIMIT_DATA, *PSET_DATA_TO_DISK_SIZE_LIMIT_DATA;

#define SET_MIN_FREE_DATA_TO_DISK_SIZE_LIMIT_DATA_VALID_GUID   0x0001

typedef struct _SET_MIN_FREE_DATA_TO_DISK_SIZE_LIMIT_DATA {
    PTCHAR  MountPoint;
    WCHAR   VolumeGUID[GUID_SIZE_IN_CHARS];
    ULONG   ulFlags;
    ULONG   ulMinFreeDataToDiskSizeLimit;
    ULONG   DefaultMinFreeDataToDiskSizeLimit;
    ULONG   MinFreeDataToDiskSizeLimitForAllVolumes;
} SET_MIN_FREE_DATA_TO_DISK_SIZE_LIMIT_DATA, *PSET_MIN_FREE_DATA_TO_DISK_SIZE_LIMIT_DATA;

#define SET_DATA_NOTIFY_SIZE_LIMIT_DATA_VALID_GUID   0x0001

typedef struct _SET_DATA_NOTIFY_SIZE_LIMIT_DATA {
    PTCHAR  MountPoint;
    WCHAR   VolumeGUID[GUID_SIZE_IN_CHARS];
    ULONG   ulFlags;
    ULONG   ulLimit;
    ULONG   ulDefaultLimit;
    ULONG   ulLimitForAllVolumes;
} SET_DATA_NOTIFY_SIZE_LIMIT_DATA, *PSET_DATA_NOTIFY_SIZE_LIMIT_DATA;

/*
//Following code doesn't compile
typedef basic_string<TCHAR, std::char_traits<TCHAR>, std::allocator<TCHAR> > TString;
*/

class TString
{
    PTCHAR data;
    size_t length;
public:
    TString()
    {
        length = 0;
        data = NULL;
    }

    TString(TCHAR *ptr)
    {
        length = _tcslen(ptr);
        data = (PTCHAR) malloc((length + 1) * sizeof(TCHAR));
		inm_memcpy_s(data, (length + 1) * sizeof(TCHAR), ptr, (length + 1) * sizeof(TCHAR));
    }

    TString(const TString &t)
    {
        length = _tcslen(t.data);
        data = (PTCHAR) malloc((length + 1) * sizeof(TCHAR));
		inm_memcpy_s(data, (length + 1) * sizeof(TCHAR), t.data, (length + 1) * sizeof(TCHAR));
    }

    const TString &operator = (TString &t)
    {
        if(data != NULL)
        {
            free(data);
        }

        length = _tcslen(t.data);
        data = (PTCHAR) malloc((length + 1) * sizeof(TCHAR));
		inm_memcpy_s(data, (length + 1) * sizeof(TCHAR), t.data, (length + 1) * sizeof(TCHAR));

        return *this;
    }

    bool operator == (TString s) const
    {
        if(0 == _tcsicmp(s.data, this->data))
            return true;
        return false;
    }

    bool operator < (const TString &s) const
    {
        if(0 > _tcsicmp(this->data, s.data))
            return true;
        return false;
    }

    bool operator > (const TString &s) const
    {
        if(0 < _tcsicmp(this->data, s.data))
            return true;
        return false;
    }

    const PTCHAR tc_str() const
    {
        PTCHAR buffer = (const PTCHAR) malloc((length + 1) * sizeof(TCHAR));
		inm_memcpy_s(buffer, (length + 1) * sizeof(TCHAR), data, (length + 1) * sizeof(TCHAR));
        return buffer;
    }

    size_t size() const
    {
        return length;
    }

    ~TString()
    {
        free(data);
        length = 0;
        data = NULL;
    }
};

#define CMD_CHECK_SHUTDOWN_STATUS           0x0001
#define CMD_PRINT_STATISTICS                0x0002
#define CMD_CLEAR_DIFFS                     0x0003
#define CMD_WAIT_TIME                       0x0004
#define CMD_GET_DB_TRANS                    0x0005
#define CMD_COMMIT_DB_TRANS                 0x0006
#define CMD_WAIT_FOR_KEYWORD                0x0007
#define CMD_STOP_FILTERING                  0x0008
#define CMD_START_FILTERING                 0x0009
#define CMD_SET_VOLUME_FLAGS                0x000A
#define CMD_CLEAR_STATSTICS                 0x000B
#define CMD_SET_DEBUG_INFO                  0x000C
#define CMD_TAG_VOLUME                      0x000D
#define CMD_SHUTDOWN_NOTIFY                 0x000E
#define CMD_SERVICE_START_NOTIFY            0x000F
#define CMD_CHECK_SERVICE_START_STATUS      0x0010
#define CMD_SET_DATA_FILE_THREAD_PRIORITY   0x0011
#define CMD_SET_DATA_TO_DISK_SIZE_LIMIT     0x0012
#define CMD_SET_IO_SIZE_ARRAY               0x0013
#define CMD_SET_DRIVER_FLAGS                0x0014
#define CMD_SET_DATA_NOTIFY_SIZE_LIMIT      0x0015
#define CMD_SET_DRIVER_CONFIG               0x0016
#define CMD_SET_RESYNC_REQUIRED             0x0017
#define CMD_GET_DM                          0x0018
#define CMD_GET_DRIVER_VERSION              0x0019
#define CMD_RESYNC_START_NOTIFY             0x0020
#define CMD_RESYNC_END_NOTIFY               0x002A
#define CMD_SET_WORKER_THREAD_PRIORITY      0x002B
#define CMD_CUSTOM_COMMAND                  0x002C
#define CMD_SET_MIN_FREE_DATA_TO_DISK_SIZE_LIMIT     0x002D
#define CMD_GET_VOLUME_SIZE                 0x002E
#define CMD_GET_VOLUME_FLAGS                0x002F
#define CMD_GET_WRITE_ORDER_STATE           0x0030
#define CMD_SYNCHRONOUS_TAG_VOLUME          0x0031
#define CMD_GET_ADDITIONAL_STATS_INFO       0x0032
#define CMD_GET_GLOBAL_TSSN                 0x0033
#define CMD_GET_LCN                         0x0034
#define CMD_GET_DRIVE_LIST                  0x0035 
#define CMD_UPDATE_OSVERSION_DISKS          0x0036
#define CMD_ENUMERATE_DISKS                 0x0037
#define CMD_SET_DISK_CLUSTERED              0x0038


#define CMD_MOUNT_VIRTUAL_VOLUME        0x0101
#define CMD_UNMOUNT_VIRTUAL_VOLUME      0x0102
#define CMD_SET_VV_CONTROL_FLAGS        0x0103
#define CMD_STOP_VIRTUAL_VOLUME         0x0104
#define CMD_START_VIRTUAL_VOLUME        0x0105
#define CMD_CONFIGURE_VIRTUAL_VOLUME    0x0106

#define CMD_CREATE_THREAD               0x0201
#define CMD_END_OF_THREAD               0x0202
#define CMD_WAIT_FOR_THREAD             0x0203

#define CMD_WRITE_STREAM                0x0301
#define CMD_READ_STREAM                 0x0302
#define CMD_GET_SYNC                    0x0303


#define CMD_STATUS_SUCCESS              0x0001
#define CMD_STATUS_UNSUCCESSFUL         0x0002

typedef struct _COMMAND_STRUCT {
    LIST_ENTRY      ListEntry;
    ULONG           ulCommand;
    union {
        SERVICE_START_DATA          ServiceStart;
        TIME_OUT_DATA               TimeOut;
        CLEAR_DIFFERENTIALS_DATA    ClearDiffs;
        START_FILTERING_DATA        StartFiltering;
        STOP_FILTERING_DATA         StopFiltering;
        SET_DISK_CLUSTERED          SetDiskClustered;
        RESYNC_START_NOTIFY_DATA    ResyncStart;
        RESYNC_END_NOTIFY_DATA      ResyncEnd;
        GET_DB_TRANS_DATA           GetDBTrans;
        COMMIT_DB_TRANS_DATA        CommitDBTran;
        WAIT_FOR_KEYWORD_DATA       KeywordData;
        VOLUME_FLAGS_DATA           VolumeFlagsData;
        DRIVER_FLAGS_DATA           DriverFlagsData;
        CLEAR_STATISTICS_DATA       ClearStatisticsData;
        DEBUG_INFO_DATA             DebugInfoData;
        PRINT_STATISTICS_DATA       PrintStatisticsData;
        ADDITIONAL_STATS_INFO       AdditionalStatsInfo;
        GET_FILTERING_STATE         GetFilteringState;
        GET_VOLUME_FLAGS            GetVolumeFlags;
        GET_VOLUME_SIZE             GetVolumeSize;
		GET_VOLUME_SIZE_V2          GetVolumeSize_v2;
        DATA_MODE_DATA              DataModeData;
        TAG_VOLUME_INPUT_DATA       TagVolumeInputData;
        SET_DATA_FILE_THREAD_PRIORITY_DATA  SetDataFileThreadPriority;
        SET_WORKER_THREAD_PRIORITY_DATA     SetWorkerThreadPriority;
        SET_DATA_TO_DISK_SIZE_LIMIT_DATA    SetDataToDiskSizeLimit;
        SET_MIN_FREE_DATA_TO_DISK_SIZE_LIMIT_DATA SetMinFreeDataToDiskSizeLimit;
        SET_DATA_NOTIFY_SIZE_LIMIT_DATA     SetDataNotifySizeLimit;
        SET_IO_SIZE_ARRAY_INPUT_DATA        SetIoSizes;
        SET_RESYNC_REQUIRED_DATA            SetResyncRequired;
        DRIVER_CONFIG_DATA                  SetDriverConfig;
        MOUNT_VIRTUAL_VOLUME_DATA   MountVirtualVolumeData;
        UNMOUNT_VIRTUAL_VOLUME_DATA UnmountVirtualVolumeData;

        CREATE_THREAD_DATA          CreateThreadData;
        END_OF_THREAD_DATA          EndOfThreadData;
        WAIT_FOR_THREAD_DATA        WaitForThreadData;

        WRITE_STREAM_DATA           WriteStreamData;
        READ_STREAM_DATA            ReadStreamData;
        STOP_VIRTUAL_VOLUME_DATA    StopVirtualVolumeData;
        START_VIRTAL_VOLUME_DATA    StartVirtualVolumeData;
        CONFIGURE_VIRTUAL_VOLUME_DATA ConfigureVirtualVolumeData;

        CONTROL_FLAGS_INPUT         ControlFlagsInput;
		GET_DB_TRANS_DATA           GetSync;
        DRIVER_GET_LCN              GetLcn;
#ifdef _CUSTOM_COMMAND_CODE_
        CUSTOM_COMMAND_DATA         CustomCommandData;
#endif // _CUSTOM_COMMAND_CODE_        
    } data;
} COMMAND_STRUCT, *PCOMMAND_STRUCT;

typedef struct _COMMAND_LIST_ENTRY_NODE {
    LIST_ENTRY ListEntry;
    PLIST_ENTRY ListHead;
    TString CurrentThreadId;
}COMMAND_LIST_ENTRY_NODE, *PCOMMAND_LIST_ENTRY_NODE;

typedef struct _COMMAND_LINE_OPTIONS {
    bool                        bSendShutdownNotification;
    bool                        bDisableDataFiltering;
    bool                        bDisableDataFiles;
    bool                        bSendServiceStartNotification;
    bool                        bOpenVolumeFilterControlDevice;
    bool                        bOpenVirtualVolumeControlDevice;
    bool                        bVerbose;
    bool                        bCompact;
    ULONGLONG                   ullLastTransactionID;
    PLIST_ENTRY                 CommandList;
    PLIST_ENTRY                 ThreadCmdStack;
    HANDLE                      hThreadMutex;
    std::map<TString, HANDLE>   ThreadMap;
} COMMAND_LINE_OPTIONS, *PCOMMAND_LINE_OPTIONS;

void
CreateCmdThread(PCOMMAND_STRUCT pCommand, PLIST_ENTRY ListHead);
DWORD WINAPI ExecuteCommands(LPVOID pContext);

PCOMMAND_STRUCT
AllocateCommandStruct();

void
FreeCommandStruct(PCOMMAND_STRUCT pCommandStruct);

bool
MountPointToGUID(PTCHAR VolumeMountPoint, WCHAR *VolumeGUID, ULONG ulBufLen);
size_t tcstowcs(PWCHAR wstring, size_t szMaxLength, PTCHAR tstring);
size_t tcstombs(PCHAR string, size_t szMaxLength, PTCHAR tstring);
size_t mbstotcs(PTCHAR tstring, size_t szMaxLength, PCHAR string);
size_t wcstotcs(PTCHAR tstring, size_t szMaxLength, PWCHAR string);

BOOL 
RunningOn64bitVersion(BOOL &bIsWow64);

BOOL 
IsGUID(TCHAR* GUID);

BOOL
GetVolumeGUID(PTCHAR VolumeMountPoint, WCHAR *VolumeGUID, ULONG BufLen, char *FuncStr, wchar_t* CmdStr = NULL, wchar_t* drivename = NULL);

#define MAX_SIZE(type) ((CHAR_BIT * sizeof(type)) /3 + 2)

WCHAR *ConvertLongToWchar(ULONG Value, INT *Count);
typedef BOOL(*tGetDriveGeometry)(DISK_GEOMETRY*, wchar_t*);
typedef bool(*tGetSCISIDiskCapacityByName)(wchar_t*, DWORD&, DWORD&);
typedef INT64(*tReadSectorsFromDiskDriveAndWriteToFile)(wchar_t*, INT64, INT64, wchar_t*, int);


#endif // _DRVUTIL_H__
