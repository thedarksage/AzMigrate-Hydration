#ifndef _SDOWNAPP_H__
#define _SDOWNAPP_H__

#include "InMageDebug.h"

typedef struct _PENDING_TRANSACTION
{
    LIST_ENTRY  ListEntry;
    ULONGLONG   ullTransactionID;
    WCHAR       VolumeGUID[GUID_SIZE_IN_CHARS];
} PENDING_TRANSACTION, *PPENDING_TRANSACTION;

typedef struct _SHUTDOWN_NOTIFY_DATA {
#define SND_FLAGS_COMMAND_SENT      0x0001
#define SND_FLAGS_COMMAND_COMPLETED 0x0002
    HANDLE          hDevice;    // This is the handle of the driver, passed to SendShutdownNotify.
    HANDLE          hEvent;     // This is the event passed in overlapped IO for completion notification.
    ULONG           ulFlags;    // Flags to tack the state.
    OVERLAPPED      Overlapped; // This is used to send overlapped IO to driver for shtudown notification.
} SHUTDOWN_NOTIFY_DATA, *PSHUTDOWN_NOTIFY_DATA;

/*
 * SendShutdownNotification function returns HANDLE which is an address of
 * SHUTDOWN_NOTIFY_DATA data structure
 */
HANDLE
SendShutdownNotification(HANDLE hDevice);

bool
HasShutdownNotificationCompleted(HANDLE hShutdownNotification);

void
CloseShutdownNotification(HANDLE hShutdownNotification);

#define DM_STR_DRIVER_INIT      _T("DmDriverInit")
#define DM_STR_DRIVER_UNLOAD    _T("DmDriverUnload")
#define DM_STR_DRIVER_PNP       _T("DmDriverPnP")
#define DM_STR_PAGEFILE         _T("DmPagefile")
#define DM_STR_VOLUME_ARRIVAL   _T("DmVolumeArrival")
#define DM_STR_SHUTDOWN         _T("DmShutdown")
#define DM_STR_REGISTRY         _T("DmRegistry")
#define DM_STR_VOLUME_CONTEXT   _T("DmVolumeContext")
#define DM_STR_CLUSTER          _T("DmCluster")
#define DM_STR_BITMAP_OPEN      _T("DmBitmapOpen")
#define DM_STR_BITMAP_READ      _T("DmBitmapRead")
#define DM_STR_BITMAP_WRITE     _T("DmBitmapWrite")
#define DM_STR_MEM_TRACE        _T("DmMemTrace")
#define DM_STR_IOCTL_TRACE      _T("DmIoctlTrace")
#define DM_STR_POWER            _T("DmPower")
#define DM_STR_WRITE            _T("DmWrite")
#define DM_STR_DIRTY_BLOCKS     _T("DmDirtyBlocks")
#define DM_STR_VOLUME_STATE     _T("DmVolumeState")
#define DM_STR_INMAGE_IOCTL     _T("DmInMageIoctl")
#define DM_STR_ALL              _T("DmAll")

#define DL_STR_NONE         _T("DlNone")
#define DL_STR_ERROR        _T("DlError")
#define DL_STR_WARNING      _T("DlWarning")
#define DL_STR_INFO         _T("DlInfo")
#define DL_STR_VERBOSE      _T("DlVerbose")
#define DL_STR_FUNC_TRACE   _T("DlFuncTrace")
#define DL_STR_TRACE_L1     _T("DlTraceL1")
#define DL_STR_TRACE_L2     _T("DlTraceL2")
#define DL_STR_UNKNOWN      _T("DlUnknown")

typedef struct _STRING_TO_ULONG
{
    TCHAR   *pString;
    ULONG   ulValue;
} STRING_TO_ULONG, *PSTRING_TO_ULONG;

STRING_TO_ULONG ModuleStringMapping[] = {
	{ DM_STR_DRIVER_INIT, DM_DRIVER_INIT },
	{ DM_STR_DRIVER_UNLOAD, DM_DRIVER_UNLOAD },
	{ DM_STR_DRIVER_PNP, DM_DRIVER_PNP },
	{ DM_STR_PAGEFILE, DM_PAGEFILE },
	{ DM_STR_VOLUME_ARRIVAL, DM_VOLUME_ARRIVAL },
	{ DM_STR_SHUTDOWN, DM_SHUTDOWN },
	{ DM_STR_REGISTRY, DM_REGISTRY },
	{ DM_STR_VOLUME_CONTEXT, DM_VOLUME_CONTEXT },
	{ DM_STR_CLUSTER, DM_CLUSTER },
	{ DM_STR_BITMAP_OPEN, DM_BITMAP_OPEN },
	{ DM_STR_BITMAP_READ, DM_BITMAP_READ },
	{ DM_STR_BITMAP_WRITE, DM_BITMAP_WRITE },
	{ DM_STR_MEM_TRACE, DM_MEM_TRACE },
	{ DM_STR_IOCTL_TRACE, DM_IOCTL_TRACE },
	{ DM_STR_POWER, DM_POWER },
	{ DM_STR_WRITE, DM_WRITE },
	{ DM_STR_DIRTY_BLOCKS, DM_DIRTY_BLOCKS },
	{ DM_STR_VOLUME_STATE, DM_VOLUME_STATE },
	{ DM_STR_INMAGE_IOCTL, DM_INMAGE_IOCTL },
	{ DM_STR_ALL, DM_ALL },
    { NULL, 0}
};

STRING_TO_ULONG LevelStringMapping[] = {
	{ DL_STR_NONE, DL_NONE },
	{ DL_STR_ERROR, DL_ERROR },
	{ DL_STR_WARNING, DL_WARNING },
	{ DL_STR_INFO, DL_INFO },
	{ DL_STR_VERBOSE, DL_VERBOSE },
	{ DL_STR_FUNC_TRACE, DL_FUNC_TRACE },
	{ DL_STR_TRACE_L1, DL_TRACE_L1 },
	{ DL_STR_TRACE_L2, DL_TRACE_L2 },
    { NULL, 0}
};

#define CMDOPT_SHUTDOWN_NOTIFY          _T("--SendShutdownNotify")
#define CMDOPT_WAIT_FOR_KEYWORD         _T("--WaitForKeyword")
#define CMDOPT_CHECK_SHUTDOWN_STATUS    _T("--CheckShutdownStatus")
#define CMDOPT_PRINT_STATISTICS         _T("--PrintStatistics")
#define CMDOPT_CLEAR_STATISTICS         _T("--ClearStatistics")
#define CMDOPT_CLEAR_DIFFS              _T("--ClearDiffs")
#define CMDOPT_WAIT_TIME                _T("--WaitTime")
#define CMDOPT_COMMIT_DB_TRANS          _T("--CommitDBTrans")
#define CMDOPT_STOP_FILTERING           _T("--StopFiltering")
#define CMDOPT_START_FILTERING          _T("--StartFiltering")

#define CMDOPT_SET_DEBUG_INFO           _T("--SetDebugInfo")
#define CMDSUBOPT_DEBUG_LEVEL_FORALL    _T("--DebugLevelForAll")
#define CMDSUBOPT_DEBUG_LEVEL           _T("--DebugLevel")
#define CMDSUBOPT_DEBUG_MODULES         _T("--DebugModules")


#define CMDOPT_GET_DB_TRANS             _T("--GetDBTrans")
#define CMDSUBOPT_COMMIT_PREV           _T("-CommitPrev")
#define CMDSUBOPT_COMMIT_CURRENT        _T("-CommitCurr")
#define CMDSUBOPT_COMMIT_TIMEOUT        _T("-CommitWaitTime")
#define CMDSUBOPT_PRINT_DETAILS         _T("-PrintDetails")

#define CMDOPT_SET_VOLUME_FLAGS             _T("--SetVolumeFlags")
#define CMDSUBOPT_RESET_FLAGS               _T("-ResetFlags")
#define CMDSUBOPT_IGNORE_PAGEFILE_WRITES    _T("-IgnorePageFileWrites")
#define CMDSUBOPT_DISABLE_BITMAP_READ       _T("-DisableBitmapRead")
#define CMDSUBOPT_DISABLE_BITMAP_WRITE      _T("-DisableBitmapWrite")
#define CMDSUBOPT_MARK_READ_ONLY            _T("-MarkReadOnly")

typedef struct _VOLUME_FLAGS_DATA {
    WCHAR   VolumeGUID[GUID_SIZE_IN_CHARS];
    TCHAR   DriveLetter;
    ULONG   ulVolumeFlags;
    etBitOperation  eBitOperation;
} VOLUME_FLAGS_DATA, *PVOLUME_FLAGS_DATA;

typedef struct _TIME_OUT_DATA {
    ULONG   ulTimeout;
} TIME_OUT_DATA, *PTIME_OUT_DATA;

typedef struct _CLEAR_STATISTICS_DATA {
    WCHAR   VolumeGUID[GUID_SIZE_IN_CHARS];
    TCHAR   DriveLetter;
} CLEAR_STATISTICS_DATA, *PCLEAR_STATISTICS_DATA;

typedef struct _CLEAR_DIFFERENTIALS_DATA {
    WCHAR   VolumeGUID[GUID_SIZE_IN_CHARS];
    TCHAR   DriveLetter;
} CLEAR_DIFFERENTIALS_DATA, *PCLEAR_DIFFERENTIALS_DATA;

typedef struct _START_FILTERING_DATA {
    WCHAR   VolumeGUID[GUID_SIZE_IN_CHARS];
    TCHAR   DriveLetter;
} START_FILTERING_DATA, *PSTART_FILTERING_DATA;

typedef struct _STOP_FILTERING_DATA {
    WCHAR   VolumeGUID[GUID_SIZE_IN_CHARS];
    TCHAR   DriveLetter;
} STOP_FILTERING_DATA, *PSTOP_FILTERING_DATA;

typedef struct _GET_DB_TRANS_DATA {
    WCHAR   VolumeGUID[GUID_SIZE_IN_CHARS];
    bool    CommitPreviousTrans;
    bool    CommitCurrentTrans;
    bool    PrintDetails;
    ULONG   ulWaitTimeBeforeCurrentTransCommit;
    TCHAR   DriveLetter;
} GET_DB_TRANS_DATA, *PGET_DB_TRANS_DATA;

typedef struct _COMMIT_DB_TRANS_DATA {
    WCHAR   VolumeGUID[GUID_SIZE_IN_CHARS];
    TCHAR   DriveLetter;
} COMMIT_DB_TRANS_DATA, *PCOMMIT_DB_TRANS_DATA;

typedef struct _DEBUG_INFO_DATA {
    DEBUG_INFO  DebugInfo;
} DEBUG_INFO_DATA, *PDEBUG_INFO_DATA;

#define KEYWORD_SIZE    80

typedef struct _WAIT_FOR_KEYWORD_DATA {
    TCHAR   Keyword[KEYWORD_SIZE + 1];
} WAIT_FOR_KEYWORD_DATA, *PWAIT_FOR_KEYWORD_DATA;

typedef struct _PRINT_STATISTICS_DATA {
    WCHAR   VolumeGUID[GUID_SIZE_IN_CHARS];
    TCHAR   DriveLetter;
} PRINT_STATISTICS_DATA, *PPRINT_STATISTICS_DATA;

#define CMD_CHECK_SHUTDOWN_STATUS       0x0001
#define CMD_PRINT_STATISTICS            0x0002
#define CMD_CLEAR_DIFFS                 0x0003
#define CMD_WAIT_TIME                   0x0004
#define CMD_GET_DB_TRANS                0x0005
#define CMD_COMMIT_DB_TRANS             0x0006
#define CMD_WAIT_FOR_KEYWORD            0x0007
#define CMD_STOP_FILTERING              0x0008
#define CMD_START_FILTERING             0x0009
#define CMD_SET_VOLUME_FLAGS            0x000A
#define CMD_CLEAR_STATSTICS             0x000B
#define CMD_SET_DEBUG_INFO              0x000C

typedef struct _COMMAND_STRUCT {
    LIST_ENTRY      ListEntry;
    ULONG           ulCommand;
    union {
        TIME_OUT_DATA               TimeOut;
        CLEAR_DIFFERENTIALS_DATA    ClearDiffs;
        START_FILTERING_DATA        StartFiltering;
        STOP_FILTERING_DATA         StopFiltering;
        GET_DB_TRANS_DATA           GetDBTrans;
        COMMIT_DB_TRANS_DATA        CommitDBTran;
        WAIT_FOR_KEYWORD_DATA       KeywordData;
        VOLUME_FLAGS_DATA           VolumeFlagsData;
        CLEAR_STATISTICS_DATA       ClearStatisticsData;
        DEBUG_INFO_DATA             DebugInfoData;
        PRINT_STATISTICS_DATA       PrintStatisticsData;
    } data;
} COMMAND_STRUCT, *PCOMMAND_STRUCT;


/*
 *-------------------------------------------------------------------------------------------------
 * Routines to handle PENDING_TRANSACTIONS
 *-------------------------------------------------------------------------------------------------
 */

// This function Removes the pending transaction from the list and returns pending transaction ID
// Returs ERROR_SUCCESS if found, ERROR_NOT_FOUND if not found
ULONG
GetPendingTransactionID(PLIST_ENTRY ListHead, PWCHAR VolumeGUID, ULONGLONG &ullTransactionId);

// This function retuns pending transaction ID with out removing it from list.
// Returs ERROR_SUCCESS if found, ERROR_NOT_FOUND if not found
ULONG
GetPendingTransactionIDRef(PLIST_ENTRY ListHead, PWCHAR VolumeGUID, ULONGLONG &ullTransactionId);

// This function adds pending transaction to list.
// returns ERROR_ALREADY_EXISTS - if there is a pending transaction for this VolumeGUID and does
// not match with the new pending transaction ID
// returns ERROR_SUCCESS - if there is no pending transaction for this VolumeGUID or the existing
// pending transaction matches the new pending transaction ID.
ULONG
AddPendingTransaction(PLIST_ENTRY ListHead, PWCHAR VolumeGUID, ULONGLONG ullTransactionId);

#endif // _SDOWNAPP_H__