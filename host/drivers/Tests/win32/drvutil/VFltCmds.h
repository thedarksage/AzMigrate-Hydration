#ifndef _VFLTCMDS_H_
#define _VFLTCMDS_H_

#include "InmFltInterface.h"
#include "InMageDebug.h"

typedef struct _PENDING_TRANSACTION
{
    LIST_ENTRY  ListEntry;
    ULONGLONG   ullTransactionID;
    bool        bResetResync;
    WCHAR       VolumeGUID[GUID_SIZE_IN_CHARS];
} PENDING_TRANSACTION, *PPENDING_TRANSACTION;

typedef struct _SHUTDOWN_NOTIFY_DATA {
#define SND_FLAGS_COMMAND_SENT      0x0001
#define SND_FLAGS_COMMAND_COMPLETED 0x0002
    HANDLE          hVFCtrlDevice;    // This is the handle of the driver, passed to SendShutdownNotify.
    HANDLE          hEvent;     // This is the event passed in overlapped IO for completion notification.
    ULONG           ulFlags;    // Flags to tack the state.
    OVERLAPPED      Overlapped; // This is used to send overlapped IO to driver for shtudown notification.
} SHUTDOWN_NOTIFY_DATA, *PSHUTDOWN_NOTIFY_DATA;

typedef struct _SERVICE_START_NOTIFY_DATA {
#define SERVICE_START_ND_FLAGS_COMMAND_SENT         0x0001
#define SERVICE_START_ND_FLAGS_COMMAND_COMPLETED    0x0002
    HANDLE          hVFCtrlDevice;      // This is the handle of the driver, passed to SendServiceStartNotify
    HANDLE          hEvent;             // This is the event passed in overlapped IO for completion notification.
    ULONG           ulFlags;            // Flags to tracks state
    OVERLAPPED      Overlapped;         // This is used to send overlapped IO to driver for service start notification.
} SERVICE_START_NOTIFY_DATA, *PSERVICE_START_NOTIFY_DATA;

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

#define cat(x,y)              x y
#define xcat(x,y)            cat(x,y)
#define UsageString(x,z)  xcat(xcat(xcat(x, CMDSTR_SF),z),CMDSTR_SF1)

#define CMDSTR_SF   _T(" (")
#define CMDSTR_SF1 _T(")")
#define CMDSTR_BLANK _T(" ")

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
#define DM_STR_DATA_FILTERING   _T("DmDataFiltering")
#define DM_STR_TIME_STAMP       _T("DmTimeStamp")
#define DM_STR_CAPTURE_MODE     _T("DmCaptureMode")
#define DM_STR_WO_STATE         _T("DmWoState")
#define DM_STR_TAGS             _T("DmTags")
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

#define CMDSTR_HELP _T("-help")
#define CMDSTR_H       _T("-h")
#define CMDSTR_GET_DRIVER_VERSION       _T("--GetDriverVersion")
#define CMDSTR_GET_DRIVER_VERSION_SF    _T("--gdv")
#define CMDSTR_SHUTDOWN_NOTIFY          _T("--SendShutdownNotify")
#define CMDSTR_SHUTDOWN_NOTIFY_SF       _T("--sn")
#define CMDOPTSTR_DISABLE_DATA_MODE     _T("-DisableDataMode")
// _T("-DisableDataFiles") is used for shutdown notify
#define CMDSTR_CHECK_SHUTDOWN_STATUS    _T("--CheckShutdownStatus")
#define CMDSTR_PROCESS_START_NOTIFY     _T("--SendProcessStartNotify")
#define CMDSTR_PROCESS_START_NOTIFY_SF  _T("--psn")

// _T("-DisableDataFiles") is used for service start notify
#define CMDSTR_CHECK_PROCESS_START_STATUS     _T("--CheckProcessStartStatus")
#define CMDSTR_GET_DATA_MODE            _T("--GetDataMode")
#define CMDSTR_GET_DATA_MODE_SF         _T("--dm")
#define CMDSTR_PRINT_STATISTICS         _T("--PrintStatistics")
#define CMDSTR_PRINT_STATISTICS_SF      _T("--ps")
#define CMDSTR_PRINT_ADDITIONAL_STATS   _T("--PrintAdditionalStatistics")
#define CMDSTR_PRINT_ADDITIONAL_STATS_SF _T("--pas")
#define CMDSTR_GET_GLOABL_TSSN           _T("--GetGlobalTimeStamp")
#define CMDSTR_GET_VOLUME_SIZE          _T("--GetVolumeSize")
#define CMDSTR_GET_VOLUME_SIZE_SF       _T("--gvs")
#define CMDSTR_GET_VOLUME_WRITE_ORDER_STATE       _T("--GetVolumeWriteOrderState")
#define CMDSTR_GET_VOLUME_WRITE_ORDER_STATE_SF    _T("--gvwos")
#define CMDSTR_GET_VOLUME_FLAGS         _T("--GetVolumeFlags")
#define CMDSTR_GET_VOLUME_FLAGS_SF      _T("--gvfl")
#define CMDSTR_CLEAR_STATISTICS         _T("--ClearStatistics")
#define CMDSTR_CLEAR_STATISTICS_SF      _T("--cs")
#define CMDSTR_CLEAR_DIFFS              _T("--ClearDiffs")
#define CMDSTR_COMMIT_DB_TRANS          _T("--CommitDBTrans")
#define CMDSTR_STOP_FILTERING           _T("--StopFiltering")
#define CMDSTR_STOP_FILTERING_SF        _T("--stopf")
#define CMDSTR_START_FILTERING          _T("--StartFiltering")
#define CMDSTR_START_FILTERING_SF       _T("--startf")
#define CMDSTR_RESYNC_START_NOTIFY      _T("--ResyncStartNotify")
#define CMDSTR_RESYNC_START_NOTIFY_SF   _T("--rs")
#define CMDSTR_RESYNC_END_NOTIFY        _T("--ResyncEndNotify")
#define CMDSTR_SET_DISK_CLUSTERED        _T("--SetDiskClustered")

#define CMDSTR_RESYNC_END_NOTIFY_SF     _T("--re")
#define CMDSTR_TAG_VOLUME               _T("--TagDevice")
#define CMDOPTSTR_ATOMIC                _T("-Atomic")
#define CMDOPTSTR_SYNCHRONOUS           _T("-Sync")

#define CMDOPTSTR_IGNORE_FILTERING_STOPPED  _T("-IgnoreIfFilteringStopped")
#define CMDOPTSTR_IGNORE_IF_GUID_NOT_FOUND  _T("-IgnoreIfGUIDnotFound")
#define CMDOPTSTR_TIMEOUT               _T("-TimeOut")
#define CMDSTR_SET_IO_SIZE_ARRAY        _T("--SetIoSizes")
#define CMDSTR_SET_IO_SIZE_ARRAY_SF     _T("--setio")
#define CMDSTR_SET_READ_IO_PROFILING    _T("-r")
#define CMDSTR_SET_WRITE_IO_PROFILING   _T("-w")

#define CMDOPTSTR_COUNT                 _T("-n")
#define CMDOPTSTR_PROMPT                _T("-prompt")
#define CMDOPTSTR_BITMAP_DELETE         _T("-DeleteBitmap")
#define CMDOPTSTR_STOP_ALL              _T("-StopAll")

#define CMDSTR_SET_RESYNC_REQUIRED      _T("--SetResyncRequired")
#define CMDOPTSTR_ERROR_CODE            _T("-ErrorCode")
#define CMDOPTSTR_ERROR_STATUS          _T("-ErrorStatus")

#define CMDSTR_SET_DATA_FILE_WRITER_THREAD_PRIORITY     _T("--SetDataFileWriterThreadPriority")
#define CMDSTR_GET_DATA_FILE_WRITER_THREAD_PRIORITY     _T("--GetDataFileWriterThreadPriority")
#define CMDSTR_SET_DATA_NOTIFY_SIZE_LIMIT               _T("--SetDataNotifySizeLimit")
#define CMDSTR_SET_DATA_TO_DISK_SIZE_LIMIT              _T("--SetDataToDiskSizeLimit")
#define CMDSTR_SET_MIN_FREE_DATA_TO_DISK_SIZE_LIMIT     _T("--SetMinFreeDataToDiskSizeLimit")
#define CMDOPTSTR_SET_DEFAULT                           _T("-SetDefault")
#define CMDOPTSTR_SET_FOR_ALL_VOLUMES                   _T("-SetForAllVolumes")

#define CMDSTR_SET_WORKER_THREAD_PRIORITY     _T("--SetWorkerThreadPriority")

#define CMDSTR_CONFIGURE_DRIVER                         _T("--ConfigureDriver")
#define CMDSTR_CONFIGURE_DRIVER_SF                      _T("--cd")
#define CMDOPTSTR_HIGH_WATER_MARK_SERVICE_NOT_STARTED   _T("-hwmServiceNotStarted")
#define CMDOPTSTR_HIGH_WATER_MARK_SERVICE_RUNNING       _T("-hwmServiceRunning")
#define CMDOPTSTR_HIGH_WATER_MARK_SERVICE_SHUTDOWN      _T("-hwmServiceShutdown")
#define CMDOPTSTR_LOW_WATER_MARK_SERVICE_RUNNING        _T("-lwmServiceRunning")
#define CMDOPTSTR_CHANGES_TO_PURGE_ON_HIGH_WATER_MARK   _T("-ChangesToPurgeOnhwm")
#define CMDOPTSTR_TIME_IN_SEC_BETWEEN_MEMORY_ALLOC_FAILURES _T("-SecsBetweenMemoryAllocFailureEvents")
#define CMDOPTSTR_MAX_WAIT_TIME_IN_SEC_ON_FAILOVER      _T("-SecsMaxWaitTimeOnFailover")
#define CMDOPTSTR_MAX_LOCKED_DATA_BLOCKS                _T("-MaxLockedDataBlocks")
#define CMDOPTSTR_MAX_NON_PAGED_POOL_IN_MB              _T("-MaxNonPagedPoolInMB")
#define CMDOPTSTR_MAX_WAIT_TIME_FOR_IRP_COMPLETION_IN_MINUTES      _T("-MaxWaitTimeForIrpCompletionInMinutes")
#define CMDOPTSTR_DEFAULT_FILTERING_FOR_NEW_CLUSTER_VOLUMES        _T("-DisableDefaultFilteringForNewClusterVolumes")
#define CMDOPTSTR_DEFAULT_DATA_FILTER_FOR_NEW_VOLUMES   _T("-EnableDataFilteringForNewVolume")
#define CMDOPTSTR_DEFAULT_FILTERING_FOR_NEW_VOLUMES     _T("-DisableDefaultFilteringForNewVolumes")
#define CMDOPTSTR_DEFAULT_FILTERING_ENABLE_DATA_CAPTURE _T("-EnableDefaultDataCapture")
#define CMDOPTSTR_MAX_DATA_ALLOCATION_LIMIT             _T("-MaxDataAllocationLimitInPercentage")
#define CMDOPSTR_AVAILABLE_DATA_BLOCK_COUNT             _T("-AvailableDataBlockLimitInPercentage")
#define CMDOPSTR_MAX_COALESCED_METADATA_CHANGE_SIZE	_T("-MaxCoalescedMetaDataChangeSize")
#define CMDOPSTR_DO_VALIDATION_CHECKS	_T("-ValidationLevel")
#define CMDOPSTR_DATA_POOL_SIZE         _T("-DataPoolSize")
#define CMDOPSTR_THRESHOLD_FOR_FILE_WRITE _T("-ThresholdForFileWrite")
#define CMDOPSTR_FREE_THRESHOLD_FOR_FILE_WRITE _T("-FreeThresholdForFileWrite")
#define CMDOPSTR_MAX_DATA_POOL_SIZE_PER_VOLUME _T("-MaxDataPoolSizePerVolume")
#define CMDOPSTR_MAX_DATA_PER_NON_DATA_MODE_DIRTY_BLOCK _T("-MaxDataPerNonDataModeDirtyBlock")
#define CMDOPSTR_MAX_DATA_PER_DATA_MODE_DIRTY_BLOCK _T("-MaxDataPerDataModeDirtyBlock")
#define CMDOPSTR_DATA_LOG_DIRECTORY _T("-DataLogDirectory")
#define CMDOPSTR_DATA_LOG_DIRECTORY_ALL _T("-DataLogDirectoryAll")
#define CMDOPTSTR_FS_FLUSH_PRE_SHUTDOWN  _T("-FsFlushPreShutdown")

#define CMDSTR_SET_DEBUG_INFO           _T("--SetDebugInfo")
#define CMDOPTSTR_DEBUG_LEVEL_FORALL    _T("-DebugLevelForAll")
#define CMDOPTSTR_DEBUG_LEVEL           _T("-DebugLevel")
#define CMDOPTSTR_DEBUG_MODULES         _T("-DebugModules")

#define CMDSTR_GET_SYNC                 _T("--ReplicateDisk")
#define CMDSTR_GET_DB_TRANS             _T("--GetDBTrans")
#define CMDOPTSTR_POLL_INTERVAL         _T("-Poll")
#define CMDOPTSTR_RUN_FOR_TIME          _T("-RunForTime")
#define CMDOPTSTR_NUM_DIRTY_BLOCKS      _T("-NumDirtyBlocks")
#define CMDOPTSTR_COMMIT_PREV           _T("-CommitPrev")
#define CMDOPTSTR_COMMIT_CURRENT        _T("-CommitCurr")
#define CMDOPTSTR_RESET_RESYNC          _T("-ResetResync")
#define CMDOPTSTR_COMMIT_TIMEOUT        _T("-CommitWaitTime")
#define CMDOPTSTR_PRINT_DETAILS         _T("-PrintDetails")
#define CMDPOPSTR_SRC_DISK              _T("-srcdisk")
#define CMDOPSTR_SRC_SIGN               _T("-srcsign")
#define CMDOPSTR_TARGET_VHD_FILE        _T("-tgtvhd")

#define CMDOPTSTR_SKIP_STEP_ONE         _T("-SkipStepOne")
#define CMDOPTSTR_STOP_ON_TAG           _T("-StopReplicationOnTag")
//DBF -START
#define CMDOPTSTR_DO_STEP_ONE         _T("-DoStepOne")
//DBF -END
#define DETAIL_PRINT_BASIC              1
#define DETAIL_PRINT_CHANGES            2
#define DEFAULT_GET_DB_POLL_INTERVAL    60  // 60 seconds

#define CMDSTR_SET_VOLUME_FLAGS             _T("--SetVolumeFlags")
#define CMDOPTSTR_RESET_FLAGS               _T("-ResetFlags")
#define CMDOPTSTR_IGNORE_PAGEFILE_WRITES    _T("-IgnorePageFileWrites")
#define CMDOPTSTR_DISABLE_BITMAP_READ       _T("-DisableBitmapRead")
#define CMDOPTSTR_DISABLE_BITMAP_WRITE      _T("-DisableBitmapWrite")
#define CMDOPTSTR_MARK_READ_ONLY            _T("-MarkReadOnly")
#define CMDOPTSTR_MARK_VOLUME_ONLINE        _T("-MarkVolumeOnline")
#define CMDOPTSTR_ENABLE_SOURCE_ROLLBACK    _T("-EnableSourceRollback")
#define CMDOPTSTR_DISABLE_DATA_CAPTURE      _T("-DisableDataCapture")
#define CMDOPTSTR_DISABLE_DATA_FILES        _T("-DisableDataFiles")
#define CMDOPTSTR_FREE_DATA_BUFFERS         _T("-FreeDataBuffers")
#define CMDOPTSTR_VCONTEXT_FIELDS_PERSISTED _T("-VContextFieldsPersisted")
#define CMDOPTSTR_VOLUME_DATA_LOG_DIRECTORY _T("-DataLogDirectory")

#define CMDSTR_SET_DRIVER_FLAGS             _T("--SetDriverFlags")
#define CMDSTR_SET_DRIVER_FLAGS_SF          _T("--setdf")
#define CMDOPTSTR_DISABLE_DATA_FILES_FOR_NV _T("-DisableDataFilesForNewVolumes")

#define CMDSTR_GET_LCN                   _T("--GetLcn")
#define CMDSTR_GET_DRIVE_LIST            _T("--GetDriveList")
#define CMDSTR_GET_DRIVE_LIST_SF         _T("--gdl")
#define CMDOPTSTR_VCN                    _T("-VCN")

#define CMDSTR_UPDATE_OSVER             _T("--UpdateOSVersion")

#define CMDSTR_ENUMERATE_DISKS             _T("--EnumerateDisks")

#ifdef _CUSTOM_COMMAND_CODE_
#define CMDSTR_CUSTOM_COMMAND               _T("--CustomCommand")
#define CMDOPTSTR_PARAM1                    _T("-p1")
#define CMDOPTSTR_PARAM2                    _T("-p2")
#endif // _CUSTOM_COMMAND_CODE_

typedef struct _VOLUME_FLAGS_DATA {
    WCHAR   VolumeGUID[GUID_SIZE_IN_CHARS];
    PTCHAR  MountPoint;
    ULONG   ulVolumeFlags;
    etBitOperation  eBitOperation;
    struct {
        USHORT  usLength;                                   // 0x68 Filename length in bytes not including NULL
        WCHAR   FileName[UDIRTY_BLOCK_MAX_DATA_LOG_DIRECTORY];       // 0x6A
    } DataFile;
} VOLUME_FLAGS_DATA, *PVOLUME_FLAGS_DATA;

typedef struct _DRIVER_FLAGS_DATA {
    ULONG   ulDriverFlags;
    etBitOperation  eBitOperation;
} DRIVER_FLAGS_DATA, *PDRIVER_FLAGS_DATA;

typedef struct _CLEAR_STATISTICS_DATA {
    WCHAR   VolumeGUID[GUID_SIZE_IN_CHARS];
    PTCHAR  MountPoint;
} CLEAR_STATISTICS_DATA, *PCLEAR_STATISTICS_DATA;

typedef struct _CLEAR_DIFFERENTIALS_DATA {
    WCHAR   VolumeGUID[GUID_SIZE_IN_CHARS];
    PTCHAR  MountPoint;
} CLEAR_DIFFERENTIALS_DATA, *PCLEAR_DIFFERENTIALS_DATA;

typedef struct _START_FILTERING_DATA {
    WCHAR   VolumeGUID[GUID_SIZE_IN_CHARS];
    PTCHAR  MountPoint;
} START_FILTERING_DATA, *PSTART_FILTERING_DATA;

typedef struct _SET_DISK_CLUSTERED {
    WCHAR   VolumeGUID[GUID_SIZE_IN_CHARS];
    PTCHAR  MountPoint;
} SET_DISK_CLUSTERED, * PSET_DISK_CLUSTERED;

typedef struct _STOP_FILTERING_DATA {
    WCHAR   VolumeGUID[GUID_SIZE_IN_CHARS];
    PTCHAR  MountPoint;
    bool    bdeleteBitmapFile;
	bool    bStopAll;
} STOP_FILTERING_DATA, *PSTOP_FILTERING_DATA;

typedef struct _RESYNC_START_NOTIFY_DATA {
    WCHAR   VolumeGUID[GUID_SIZE_IN_CHARS];
    PTCHAR   MountPoint;
} RESYNC_START_NOTIFY_DATA, *PRESYNC_START_NOTIFY_DATA;

typedef struct _RESYNC_END_NOTIFY_DATA {
    WCHAR   VolumeGUID[GUID_SIZE_IN_CHARS];
    PTCHAR   MountPoint;
} RESYNC_END_NOTIFY_DATA, *PRESYNC_END_NOTIFY_DATA;

typedef struct _TSSNSEQID
{
    ULONG     ulContinuationId;
    ULONG     ulSequenceNumber;
    ULONGLONG ullTimeStamp;
}TSSNSEQID, *PTSSNSEQID;

typedef struct _TSSNSEQIDV2
{
    ULONG     ulContinuationId;
    ULONGLONG ullSequenceNumber;
    ULONGLONG ullTimeStamp;
}TSSNSEQIDV2, *PTSSNSEQIDV2;

typedef struct _GET_DB_TRANS_DATA {
    WCHAR   VolumeGUID[GUID_SIZE_IN_CHARS];
    bool    CommitPreviousTrans;
    bool    CommitCurrentTrans;
    bool    PrintDetails;
    bool    bPollForDirtyBlocks;
    bool    bWaitIfTSO;
    bool    bResetResync;    
    ULONG   ulLevelOfDetail;
    ULONG   ulWaitTimeBeforeCurrentTransCommit;
    ULONG   ulRunTimeInMilliSeconds;
    ULONG   ulPollIntervalInMilliSeconds;
    ULONG   ulNumDirtyBlocks;
    PTCHAR  MountPoint;
    // Required for Sync
    bool    sync;
    bool    resync_req;    
    bool    skip_step_one;
    DWORD BufferSize;
    HANDLE hFileSource; // Source File
    TCHAR tcSourceName[256]; // Source disk
    HANDLE hFileDest; // Dest File 
    TCHAR tcDestName[256]; // Dest disk
    WCHAR   DestVolumeGUID[GUID_SIZE_IN_CHARS]; 
    PTCHAR  DestMountPoint;
    TSSNSEQID   FCSequenceNumber;
    TSSNSEQID   LCSequenceNumber;
    TSSNSEQIDV2 FCSequenceNumberV2;
    TSSNSEQIDV2 LCSequenceNumberV2;    
    char * chpBuff;
	bool    serviceshutdownnotify;
	bool    processstartnotify;
	bool    bStopReplicationOnTag;
} GET_DB_TRANS_DATA, *PGET_DB_TRANS_DATA;

typedef struct _COMMIT_DB_TRANS_DATA {
    WCHAR   VolumeGUID[GUID_SIZE_IN_CHARS];
    PTCHAR  MountPoint;    
    bool    bResetResync;
} COMMIT_DB_TRANS_DATA, *PCOMMIT_DB_TRANS_DATA;

typedef struct _SET_RESYNC_REQUIRED_DATA {
    WCHAR   VolumeGUID[GUID_SIZE_IN_CHARS];
    PTCHAR  MountPoint;
    ULONG   ulErrorCode;
    ULONG   ulErrorStatus;
} SET_RESYNC_REQUIRED_DATA, *PSET_RESYNC_REQUIRED_DATA;

typedef struct _DEBUG_INFO_DATA {
    DEBUG_INFO  DebugInfo;
} DEBUG_INFO_DATA, *PDEBUG_INFO_DATA;

typedef struct _PRINT_STATISTICS_DATA {
    WCHAR   VolumeGUID[GUID_SIZE_IN_CHARS];
    PTCHAR  MountPoint;
} PRINT_STATISTICS_DATA, *PPRINT_STATISTICS_DATA;

typedef struct _ADDITIONAL_STATS_INFO {
    WCHAR   VolumeGUID[GUID_SIZE_IN_CHARS];
    PTCHAR  MountPoint;
} ADDITIONAL_STATS_INFO, *PADDITIONAL_STATS_INFO;

typedef struct _GLOBAL_TIMESTAMP
{
    ULONGLONG TimeInHundNanoSecondsFromJan1601;
    ULONGLONG ullSequenceNumber;
} GLOBAL_TIMESTAMP, *PGLOBAL_TIMESTAMP;

typedef struct _GET_VOLUME_SIZE
{
    WCHAR   VolumeGUID[GUID_SIZE_IN_CHARS];
    PTCHAR  MountPoint;
}GET_VOLUME_SIZE, *PGET_VOLUME_SIZE;

typedef struct _GET_VOLUME_SIZE_V2
{
	WCHAR   *VolumeGUID;
	PTCHAR  MountPoint;
	DWORD   inputlen;
}GET_VOLUME_SIZE_V2, *PGET_VOLUME_SIZE_V2;

typedef struct _GET_VOLUME_FLAGS
{
    WCHAR   VolumeGUID[GUID_SIZE_IN_CHARS];
    PTCHAR  MountPoint;
}GET_VOLUME_FLAGS, *PGET_VOLUME_FLAGS;

typedef struct _GET_FILTERING_STATE
{
    WCHAR   VolumeGUID[GUID_SIZE_IN_CHARS];
    PTCHAR  MountPoint;
}GET_FILTERING_STATE, *PGET_FILTERING_STATE;

typedef struct _DATA_MODE_DATA {
    WCHAR   VolumeGUID[GUID_SIZE_IN_CHARS];
    PTCHAR   MountPoint;
} DATA_MODE_DATA, *PDATA_MODE_DATA;

typedef struct _TAG_VOLUME_INPUT_DATA {
    ULONG   ulLengthOfBuffer;
    PCHAR   pBuffer;
    GUID    TagGUID;
    ULONG   ulNumberOfVolumes;
    ULONG   ulTimeOut;
    ULONG   ulCount;
    bool    bPrompt;
}TAG_VOLUME_INPUT_DATA, *PTAG_VOLUME_INPUT_DATA;

typedef struct _SET_IO_SIZE_ARRAY_INPUT_DATA {
    WCHAR   VolumeGUID[GUID_SIZE_IN_CHARS];
    PTCHAR  MountPoint;
    ULONG   ulNumValues;
    ULONG   ulIoSizeArray[USER_MODE_MAX_IO_BUCKETS];
    ULONG   Flags;
} SET_IO_SIZE_ARRAY_INPUT_DATA, *PSET_IO_SIZE_ARRAY_INPUT_DATA;

typedef struct _DRIVER_CONFIG_DATA {
    ULONG   ulFlags1;
    ULONG   ulFlags2;
    ULONG   ulhwmServiceNotStarted;
    ULONG   ulhwmServiceRunning;
    ULONG   ulhwmServiceShutdown;
    ULONG   ullwmServiceRunning;
    ULONG   ulChangesToPergeOnhwm;
    ULONG   ulSecsBetweenMemAllocFailureEvents;
    ULONG   ulSecsMaxWaitTimeForFailOver;
    ULONG   ulMaxLockedDataBlocks;
    ULONG   ulMaxNonPagedPoolInMB;
    ULONG   ulMaxWaitTimeForIrpCompletionInMinutes;
    bool    bDisableVolumeFilteringForNewClusterVolumes;
    bool    bEnableFSFlushPreShutdown;                   // 0x31
    UCHAR   ucReserved[2];
    ULONG   ulAvailableDataBlockCountInPercentage;
    ULONG   ulMaxDataAllocLimitInPercentage;
    ULONG   ulMaxCoalescedMetaDataChangeSize;
    ULONG   ulValidationLevel;
    ULONG   ulDataPoolSize;
    ULONG   ulMaxDataSizePerVolume;
    ULONG   ulDaBFreeThresholdForFileWrite;
    ULONG   ulDaBThresholdPerVolumeForFileWrite;
    bool    bEnableDataFilteringForNewVolumes;
    bool    bDisableVolumeFilteringForNewVolumes;
    bool    bEnableDataCapture;
    UCHAR   ucReserved_1[1];
    ULONG   ulMaxDataSizePerNonDataModeDirtyBlock;
    ULONG   ulMaxDataSizePerDataModeDirtyBlock;
    struct {
        USHORT  usLength;                                   // 0x68 Filename length in bytes not including NULL
        WCHAR   FileName[UDIRTY_BLOCK_MAX_DATA_LOG_DIRECTORY];       // 0x6A
    } DataFile;
    
} DRIVER_CONFIG_DATA, *PDRIVER_CONFIG_DATA;

#ifdef _CUSTOM_COMMAND_CODE_
typedef struct _CUSTOM_COMMAND_DATA {
    ULONG   ulParam1;
    ULONG   ulParam2;
} CUSTOM_COMMAND_DATA, *PCUSTOM_COMMAND_DATA;
#endif // _CUSTOM_COMMAND_CODE_

typedef struct _DRIVER_GET_LCN {
    TCHAR *FileName;
    USHORT usLength;
    ULONG StartingVcn;
} DRIVER_GET_LCN, *PDRIVER_GET_LCN;

#endif _VFLTCMDS_H_
