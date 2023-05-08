#ifndef _VFLTCMDSEXEC_H_
#define _VFLTCMDSEXEC_H_

#include "DrvUtil.h"
#include "SVDStream.h"

#if !defined(TCHAR_ARRAY_SIZE)
#define TCHAR_ARRAY_SIZE( a ) ( sizeof( a ) / sizeof( a[ 0 ] ) )
#endif

#define VOLUME_NAME_PREFIX    _T("\\\\?\\Volume{")
#define VOLUME_NAME_POSTFIX   _T("}\\")

#define READ_BUFFER_SIZE 8388608 //8MB
#define WAIT_TSO_NODATA 10
#define WAIT_TSO_DATA 5

#define DiskClass _T("SYSTEM\\CurrentControlSet\\Control\\Class\\{4D36E967-E325-11CE-BFC1-08002BE10318}")
#define VolumeClass _T("SYSTEM\\CurrentControlSet\\Control\\Class\\{71a27cdd-812a-11d0-bec7-08002be2092f}")

#define LowerFilters _T("LowerFilters")
#define UpperFilters _T("UpperFilters")

typedef struct _SOURCESTREAM
{
    CDataStream * DataStream;
    size_t  StreamOffset;
}SOURCESTREAM, *PSOURCESTREAM;

PCOMMAND_STRUCT
AllocateCommandStruct();

void
FreeCommandStruct(PCOMMAND_STRUCT pCommandStruct);

bool
DriveLetterToGUID(TCHAR DriveLetter, WCHAR *VolumeGUID, ULONG ulBufLen);

bool
InitializeCommandLineOptionsWithDefaults(PCOMMAND_LINE_OPTIONS pCommandLineOptions);
void
WriteStream(PCOMMAND_STRUCT pCommand);
void
ReadStream(HANDLE hVFCtrlDevice, PREAD_STREAM_DATA pReadStreamData);
TCHAR*
UlongToDebugLevelString(ULONG ulValue);
TCHAR *
BitMapStateString(
    etVBitmapState eBitMapState
    );
TCHAR *
DataSourceToString(
    ULONG   ulDataSource
    );
TCHAR *
ServiceStateString(
    etServiceState eServiceState
    );
void
GetDriverVersion(HANDLE hVFCtrlDevice);
void
ClearDiffs(HANDLE hVFCtrlDevice, PCLEAR_DIFFERENTIALS_DATA pClearDifferentialsData);
void
SetDiskClustered(HANDLE hVFCtrlDevice, PSET_DISK_CLUSTERED setDiskClustered);
void
ClearVolumeStats(HANDLE hVFCtrlDevice, PCLEAR_STATISTICS_DATA pClearStatistcsData);
ULONG
StartFiltering(HANDLE hVFCtrlDevice, PSTART_FILTERING_DATA pStartFilteringData);
ULONG
StartFiltering_v2(HANDLE hVFCtrlDevice, PSTART_FILTERING_DATA pStartFilteringData);
ULONG
StopFiltering(HANDLE hVFCtrlDevice, PSTOP_FILTERING_DATA pStopFilteringData);
void
ResyncEndNotify(HANDLE hVFCtrlDevice, PRESYNC_END_NOTIFY_DATA pResyncEnd);
void
ResyncStartNotify(HANDLE hVFCtrlDevice, PRESYNC_START_NOTIFY_DATA pResyncStart);
void
GetDriverStats(HANDLE hVFCtrlDevice, PPRINT_STATISTICS_DATA pPrintStatisticsData);

void
GetGlobalTSSN(HANDLE hVFCtrlDevice);

void 
GetAdditionalStats(HANDLE hVFCtrlDevice, PADDITIONAL_STATS_INFO pAdditionalStatsInfo);

void
GetAdditionalStats_v2(HANDLE hVFCtrlDevice, PADDITIONAL_STATS_INFO pAdditionalStatsInfo);
void
GetWriteOrderState(HANDLE hVFCtrlDevice, PGET_FILTERING_STATE pGetFilteringState);

void 
GetVolumeFlags(HANDLE hVFCtrlDevice, PGET_VOLUME_FLAGS pGetVolumeFlags);

void
GetVolumeSize(HANDLE hVFCtrlDevice, PGET_VOLUME_SIZE pGetVolumeSize);

void
GetVolumeSize_v2(HANDLE hVFCtrlDevice, PGET_VOLUME_SIZE pGetVolumeSize);

void
GetVolumeSize_v3(HANDLE hVFCtrlDevice, PGET_VOLUME_SIZE_V2 pGetVolumeSize);

void
GetDataMode(HANDLE hVFCtrlDevice, PDATA_MODE_DATA pDataModeData);
void
GetDataModeCompact(HANDLE hVFCtrlDevice, PDATA_MODE_DATA pDataModeData);
void 
CommitDBTrans(HANDLE hVFCtrlDevice,PCOMMIT_DB_TRANS_DATA pCommitDBTran);
void 
SetResyncRequired(HANDLE hVFCtrlDevice,PSET_RESYNC_REQUIRED_DATA pSetResyncRequiredData);
ULONG
SetVolumeFlags(HANDLE hVFCtrlDevice,PVOLUME_FLAGS_DATA pVolumeFlagsData);
ULONG
SetDriverFlags(HANDLE hVFCtrlDevice,PDRIVER_FLAGS_DATA pDriverFlagsData);
ULONG
SetDriverConfig(HANDLE hVFCtrlDevice,PDRIVER_CONFIG_DATA pDriverConfig);
#ifdef _CUSTOM_COMMAND_CODE_
ULONG
SendCustomCommand(HANDLE hVFCtrlDevice,PCUSTOM_COMMAND_DATA pCustomCommanData);
#endif // _CUSTOM_COMMAND_CODE_
ULONG
TagVolume(HANDLE hVFCtrlDevice,PTAG_VOLUME_INPUT_DATA pTagVolumeInputData);
ULONG
TagDisk(HANDLE hVFCtrlDevice, PTAG_VOLUME_INPUT_DATA pTagVolumeInputData);
ULONG
TagSynchronousToVolume(HANDLE hVFCtrlDevice,PTAG_VOLUME_INPUT_DATA pTagVolumeInputData);
ULONG
SetDebugInfo(HANDLE hVFCtrlDevice,PDEBUG_INFO_DATA pDebugInfoData);
ULONG
GetDBTrans(HANDLE hVFCtrlDevice,PGET_DB_TRANS_DATA pDBTranData);
ULONG
GetDBTransV2(HANDLE hVFCtrlDevice,PGET_DB_TRANS_DATA pDBTranData);
ULONG
SetDataFileThreadPriority(HANDLE hVFCtrlDevice, PSET_DATA_FILE_THREAD_PRIORITY_DATA pSetDataFileThreadPriority);
ULONG
SetWorkerThreadPriority(HANDLE hVFCtrlDevice, PSET_WORKER_THREAD_PRIORITY_DATA pSetWorkerThreadPriority);
ULONG
SetDataToDiskSizeLimit(HANDLE hVFCtrlDevice, PSET_DATA_TO_DISK_SIZE_LIMIT_DATA pSetDataToDiskSizeLimit);
ULONG
SetMinFreeDataToDiskSizeLimit(HANDLE hVFCtrlDevice, PSET_MIN_FREE_DATA_TO_DISK_SIZE_LIMIT_DATA pSetMinFreeDataToDiskSizeLimit);
ULONG
SetDataNotifySizeLimit(HANDLE hVFCtrlDevice, PSET_DATA_NOTIFY_SIZE_LIMIT_DATA pSetDataToDiskSizeLimit);
void
SetIoSizeArray(HANDLE hVFCtrlDevice, PSET_IO_SIZE_ARRAY_INPUT_DATA pIoSizeArrayData);
int GetSync(HANDLE hVFCtrlDevice, PGET_DB_TRANS_DATA pDBTranData);

int GetSync_v2(HANDLE hVFCtrlDevice, PGET_DB_TRANS_DATA pDBTranData);

int GetLcn(HANDLE hVFCtrlDevice, PDRIVER_GET_LCN pDriverGetLcn);

void
MountVirtualVolume(HANDLE hVVCtrlDevice,PMOUNT_VIRTUAL_VOLUME_DATA pMountVirtualVolumeData);
void
WaitForKeyword(PWAIT_FOR_KEYWORD_DATA pKeywordData);
void 
WaitForThread(PCOMMAND_STRUCT pCommand);
void
CreateCmdThread(PCOMMAND_STRUCT pCommand, PLIST_ENTRY ListHead);
PCOMMAND_STRUCT
AllocateCommandStruct();
void
FreeCommandStruct(PCOMMAND_STRUCT pCommandStruct);
bool
DriveLetterToGUID(TCHAR DriveLetter, WCHAR *VolumeGUID, ULONG ulBufLen);
PPENDING_TRANSACTION
AllocatePendingTransaction();
void
DeallocatePendingTransaction(PPENDING_TRANSACTION pPendingTrans);
PPENDING_TRANSACTION
GetPendingTransactionRef(PLIST_ENTRY ListHead, PWCHAR VolumeGUID);
PPENDING_TRANSACTION
GetPendingTransaction(PLIST_ENTRY ListHead, PWCHAR VolumeGUID);
/*
 *-------------------------------------------------------------------------------------------------
 * Routines to handle PENDING_TRANSACTIONS
 *-------------------------------------------------------------------------------------------------
 */

// This function Removes the pending transaction from the list and returns pending transaction ID
// Returs ERROR_SUCCESS if found, ERROR_NOT_FOUND if not found
ULONG
GetPendingTransactionID(PLIST_ENTRY ListHead, PWCHAR VolumeGUID, ULONGLONG &ullTransactionId, bool &bResetResync);

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
AddPendingTransaction(PLIST_ENTRY ListHead, PWCHAR VolumeGUID, ULONGLONG ullTransactionId, bool bResetResync);

/**
 * Routines to handle Dirty block wait event
 * */

/**
 * This function returns the existing event handle for the volume. if event does not
 * exist function returns NULL.
 * */
bool
CreateNewDBWaitEventForVolume(std::wstring& VolumeGUID, HANDLE &hEvent);

HANDLE
SendShutdownNotification(HANDLE hVFCtrlDevice);
bool
HasShutdownNotificationCompleted(HANDLE hShutdownNotification);
void
CloseShutdownNotification(HANDLE hShutdownNotification);
HANDLE
SendServiceStartNotification(HANDLE hVFCtrlDevice, PSERVICE_START_DATA pServiceStart);
bool
HasServiceStartNotificationCompleted(HANDLE hServiceStartNotification);
void
CloseServiceStartNotification(HANDLE hServiceStartNotification);

// misc
void
GetUniqueVolumeName(PTCHAR UniqueVolumeName, PWCHAR VolumeGUID, size_t UniqueVolNameSize);

std::string&
ConvertULLtoString(ULONGLONG   ullSize, std::string& str);

bool
VerifyDifferentialsOrder(PTSSNSEQID pCurrentStartTimeStamp, PTSSNSEQID pPrevTSSequenceNumber);

bool
VerifyDifferentialsOrderV2(PTSSNSEQIDV2 pCurrentStartTimeStamp, PTSSNSEQIDV2 pPrevTSSequenceNumber);

bool DataSync (PGET_DB_TRANS_DATA  pDBTranData, PSOURCESTREAM pSourceStream, SV_LONGLONG ByteOffset, SV_LONGLONG length);

ULONG ValidateTimeStampSeqV2 (PGET_DB_TRANS_DATA pDBTranData, PUDIRTY_BLOCK_V2 pDirtyBlock, PTSSNSEQIDV2 ptempPrevTSSequenceNumberV2);

ULONG ValidateTimeStampSeq (PGET_DB_TRANS_DATA pDBTranData, PUDIRTY_BLOCK pDirtyBlock, PTSSNSEQID ptempPrevTSSequenceNumber);

void PrintDriverStats(PVOLUME_STATS_DATA  pVolumeStatsData, DWORD dwReturn, int IsOlderDriver);

void PrintDriverStatsCompact(PVOLUME_STATS_DATA  pVolumeStatsData, DWORD dwReturn, int IsOlderDriver);

void GetFilterKeys(LPCTSTR Class, LPCTSTR Value, char* PrintStr);

LONG UpdateOSVersionForDisks();

DWORD   EnumerateDisks(HANDLE hDriverControlDevice);
#endif //_VFLTCMDSEXEC_H_
