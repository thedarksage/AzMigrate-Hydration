#include <windows.h>
#include <winnls.h>
#include <stdio.h>
#include <tchar.h>
#include <malloc.h>
#include "ListEntry.h"
#include "InmFltInterface.h"
#include <vector>
#include "DrvUtil.h"
#include "VFltCmds.h"
#include "VVolCmds.h"
#include <ctype.h>
#include "VFltCmdsExec.h"
#include "VFltCmdsParse.h"
#include "VVolCmdsParse.h"
#include "drvstatus.h"
#include "installinvirvol.h"
#include "DBWaitEvent.h"

bool IsDiskFilterRunning = false;
bool IsVolFilterRunning = false;
WCHAR *ConvertLongToWchar(ULONG Value, INT *Count)
{
	HRESULT Status;
	*Count = 0;
	PWCHAR WideChar = (PWCHAR)malloc(MAX_SIZE(ULONG) * sizeof(WCHAR));

	if (!WideChar)return NULL;

	Status = StringCchPrintfW(WideChar, MAX_SIZE(ULONG), L"%lu", Value);
	if (Status != S_OK){
		free(WideChar);
		WideChar = NULL;
	}
	printf(" char : %S \n", WideChar);

	*Count = wcslen((const wchar_t *)WideChar);
	return WideChar;
}

//Caller is supposed to release the memory.

typedef struct _EMK_DISK_SIGNATURE{

	unsigned int DriveCount;
	unsigned long **Signature;
	wchar_t **DriveNames;
}EMK_DISK_SIGNATURE, *PEMK_DISK_SIGNATURE;

typedef bool (*tGetAllDiskNamesAndSignatures)(PEMK_DISK_SIGNATURE);

using namespace std;


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
    { DM_STR_DATA_FILTERING, DM_DATA_FILTERING },
    { DM_STR_TIME_STAMP, DM_TIME_STAMP },
    { DM_STR_WO_STATE, DM_WO_STATE },
    { DM_STR_CAPTURE_MODE, DM_CAPTURE_MODE },
	{ DM_STR_TAGS, DM_TAGS },
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

// Short forms
// --cs     = ClearStatistics
// --cd     = ConfigureDriver
// --sn     = SendShutdownNotify
// --stopf  = StopFiltering
// --startf = StartFiltering
// --psn    = SendProcessStartNotify
// --ps     = PrintStatistics
// --wk     = WaitForKeyword
// --v      = Verbose
// --setio  = SetIoSizes
// --dm     = GetDataMode
// --gdv    = GetDriverVersion
// --gvs    = GetVolumeSize
// --gvwos  = GetVolumeWriteOrderState
// --gvfl   = GetVolumeFlags
// --rs     = ResyncStartNotify
// --re     = ResyncEndNotify
// 
COMMAND_LINE_OPTIONS    sCommandOptions;
LIST_ENTRY              PendingTranListHead;
std::vector<CDBWaitEvent *> DBWaitEventVector;
bool help = false;

PCOMMAND_STRUCT
AllocateCommandStruct();

void
FreeCommandStruct(PCOMMAND_STRUCT pCommandStruct);

bool
DriveLetterToGUID(TCHAR DriveLetter, WCHAR *VolumeGUID, ULONG ulBufLen);
/*
This function is used to display the usage of the drvutil.
It shows the list of the supported command and brief description
about them.
*/

HINSTANCE  HandleToDiskGenericDll = LoadLibrary(L"diskgeneric.dll");
tGetAllDiskNamesAndSignatures pGetDriveDetails = (tGetAllDiskNamesAndSignatures)GetProcAddress(HandleToDiskGenericDll, "GetAllDiskNamesAndSignatures");

void
UsageMain(TCHAR *pcExeName)
{
    _tprintf(_T("\n"));
    _tprintf(_T("USAGE\n  %s [--Command [-CommandOptions]] \n\n"), pcExeName);
    _tprintf(_T("SUPPORTED COMMANDS\n"));
//    _tprintf(_T("\n"));
    _tprintf(_T("  %-37s %s\n"), UsageString(CMDSTR_HELP,CMDSTR_H),UsageString(CMDSTR_GET_DRIVER_VERSION,CMDSTR_GET_DRIVER_VERSION_SF));
    _tprintf(_T("  %-37s %s\n"), UsageString(CMDSTR_VERBOSE, CMDSTR_VERBOSE_SF), UsageString(CMDSTR_WAIT_FOR_KEYWORD, CMDSTR_WAIT_FOR_KEYWORD_SF));
    _tprintf(_T("  %-37s %s\n"), UsageString(CMDSTR_SHUTDOWN_NOTIFY, CMDSTR_SHUTDOWN_NOTIFY_SF), CMDSTR_CHECK_SHUTDOWN_STATUS);
    _tprintf(_T("  %-37s %s\n"), UsageString(CMDSTR_PROCESS_START_NOTIFY, CMDSTR_PROCESS_START_NOTIFY_SF), CMDSTR_CHECK_PROCESS_START_STATUS);
    _tprintf(_T("  %-37s %s\n"), UsageString(CMDSTR_PRINT_STATISTICS, CMDSTR_PRINT_STATISTICS_SF), UsageString(CMDSTR_GET_VOLUME_SIZE, CMDSTR_GET_VOLUME_SIZE_SF));
    _tprintf(_T("  %-37s %s\n"), UsageString(CMDSTR_GET_VOLUME_WRITE_ORDER_STATE, CMDSTR_GET_VOLUME_WRITE_ORDER_STATE_SF), UsageString(CMDSTR_GET_VOLUME_FLAGS, CMDSTR_GET_VOLUME_FLAGS_SF));
    _tprintf(_T("  %-37s %s\n"), UsageString(CMDSTR_GET_VOLUME_FLAGS, CMDSTR_GET_VOLUME_FLAGS_SF), UsageString(CMDSTR_CLEAR_STATISTICS, CMDSTR_CLEAR_STATISTICS_SF));
    _tprintf(_T("  %-37s %s\n"), UsageString(CMDSTR_GET_DATA_MODE, CMDSTR_GET_DATA_MODE_SF), CMDSTR_CLEAR_DIFFS);
    _tprintf(_T("  %-37s %s\n"), UsageString(CMDSTR_STOP_FILTERING, CMDSTR_STOP_FILTERING_SF), UsageString(CMDSTR_START_FILTERING, CMDSTR_START_FILTERING_SF));
    _tprintf(_T("  %-37s %s\n"), UsageString(CMDSTR_RESYNC_START_NOTIFY, CMDSTR_RESYNC_START_NOTIFY_SF), UsageString(CMDSTR_RESYNC_END_NOTIFY, CMDSTR_RESYNC_END_NOTIFY_SF));
    _tprintf(_T("  %-37s %s\n"), CMDSTR_WAIT_TIME, CMDSTR_GET_DB_TRANS);
    _tprintf(_T("  %-37s %s\n"), CMDSTR_WAIT_TIME, CMDSTR_GET_SYNC);    
    _tprintf(_T("  %-37s %s\n"), CMDSTR_GET_DB_TRANS, CMDSTR_COMMIT_DB_TRANS);
    _tprintf(_T("  %-37s %s\n"), CMDSTR_SET_RESYNC_REQUIRED, CMDSTR_SET_VOLUME_FLAGS);
    _tprintf(_T("  %-37s %s\n"), UsageString(CMDSTR_SET_DRIVER_FLAGS, CMDSTR_SET_DRIVER_FLAGS_SF), CMDSTR_SET_DATA_FILE_WRITER_THREAD_PRIORITY);
    _tprintf(_T("  %-37s %s\n"), CMDSTR_SET_WORKER_THREAD_PRIORITY, CMDSTR_SET_DATA_TO_DISK_SIZE_LIMIT);
    _tprintf(_T("  %-37s %s\n"), CMDSTR_SET_MIN_FREE_DATA_TO_DISK_SIZE_LIMIT, CMDSTR_SET_DATA_NOTIFY_SIZE_LIMIT);
    _tprintf(_T("  %-37s %s\n"), UsageString(CMDSTR_CONFIGURE_DRIVER, CMDSTR_CONFIGURE_DRIVER_SF), CMDSTR_SET_DEBUG_INFO);
    _tprintf(_T("  %-37s %s\n"), CMDSTR_TAG_VOLUME,UsageString(CMDSTR_SET_IO_SIZE_ARRAY, CMDSTR_SET_IO_SIZE_ARRAY_SF));
#ifdef _CUSTOM_COMMAND_CODE_
    _tprintf(_T("  %-37s %s\n"), CMDSTR_CUSTOM_COMMAND, CMDSTR_MOUNT_VIRTUAL_VOLUME);
#endif // _CUSTOM_COMMAND_CODE_
    _tprintf(_T("  %-37s %s\n"), CMDSTR_UNMOUNT_VIRTUAL_VOLUME, CMDSTR_CREATE_THREAD);
    _tprintf(_T("  %-37s %s\n"), CMDSTR_WAIT_FOR_THREAD, CMDSTR_END_OF_THREAD);
    _tprintf(_T("  %-37s %s\n"), CMDSTR_START_VIRTUAL_VOLUME, CMDSTR_STOP_VIRTUAL_VOLUME);
    //_tprintf(_T("%-37s %s\n"), CMDSTR_WRITE_STREAM, CMDSTR_READ_STREAM);
    _tprintf(_T("  %-37s %s\n"), CMDSTR_SET_VV_CONTROL_FLAGS, CMDSTR_CONFIGURE_VIRTUAL_VOLUME);
    _tprintf(_T("  %-37s %s\n"), CMDSTR_GET_LCN, CMDSTR_GET_GLOABL_TSSN);
    _tprintf(_T("  %-37s\n"), UsageString(CMDSTR_PRINT_ADDITIONAL_STATS, CMDSTR_PRINT_ADDITIONAL_STATS_SF));
    _tprintf(_T("  %-37s %s\n"), UsageString(CMDSTR_GET_DRIVE_LIST, CMDSTR_GET_DRIVE_LIST_SF), CMDSTR_GET_SYNC);
    _tprintf(_T("  %-37s\n"), CMDSTR_UPDATE_OSVER);
    _tprintf(_T("\nHELP\n"));
    _tprintf(_T("  %s [--Command] -help\n"), pcExeName);
    _tprintf(_T("  %s [--Command] -h\n"), pcExeName);
    return;
}

bool
InitializeCommandLineOptionsWithDefaults(PCOMMAND_LINE_OPTIONS pCommandLineOptions)
{
    if (NULL == pCommandLineOptions) {
        _tprintf(__T("Drvutil Error: %s invalid argument pCommandLineOptions is NULL\n"), __TFUNCTION__);
        return false;
    }

    pCommandLineOptions->bSendShutdownNotification = false;
    pCommandLineOptions->bDisableDataFiltering = false;
    pCommandLineOptions->bDisableDataFiles = false;
    pCommandLineOptions->bSendServiceStartNotification = false;
    pCommandLineOptions->bVerbose = false;
    pCommandLineOptions->bOpenVirtualVolumeControlDevice = false;
    pCommandLineOptions->bOpenVolumeFilterControlDevice = false;
    pCommandLineOptions->ullLastTransactionID = 0;
    pCommandLineOptions->CommandList = new LIST_ENTRY;
    pCommandLineOptions->ThreadCmdStack = new LIST_ENTRY;
    pCommandLineOptions->hThreadMutex = CreateMutex(NULL, FALSE, NULL);
    InitializeListHead(pCommandLineOptions->CommandList);
    InitializeListHead(pCommandLineOptions->ThreadCmdStack);
    return true;
}


DWORD GetDiskLayout(
	HANDLE                          hDisk,
	PDRIVE_LAYOUT_INFORMATION_EX    *ppLayout
	)
{

	PDRIVE_LAYOUT_INFORMATION_EX  pLayout = NULL;
	DWORD                         dwRet = ERROR_SUCCESS;
	BOOL                          bRet = FALSE;
	DWORD                         dwBufferSize = 0L;
	DWORD                         dwBytesReturned = 0L;
	 
	if ((hDisk == NULL) || (ppLayout == NULL))
		return dwRet;

	*ppLayout = NULL;

	dwBufferSize = FIELD_OFFSET(
		DRIVE_LAYOUT_INFORMATION_EX,
		PartitionEntry);
	while (TRUE) {
		
		dwBufferSize += 16 * sizeof(PARTITION_INFORMATION_EX);  

		if (pLayout) {
			free(pLayout);
			pLayout = NULL;
		}

		pLayout = (PDRIVE_LAYOUT_INFORMATION_EX)
			malloc(dwBufferSize);

		if (pLayout == NULL)
		{
			dwRet = ERROR_NOT_ENOUGH_MEMORY;
			break;
		}

		ZeroMemory(pLayout, dwBufferSize);

		bRet = DeviceIoControl(
			hDisk,
			IOCTL_DISK_GET_DRIVE_LAYOUT_EX,
			NULL,
			0,
			pLayout,
			dwBufferSize,
			&dwBytesReturned,
			0);
		if (bRet)
		{
			// If succeeded exit loop.
			dwRet = ERROR_SUCCESS;  
			break;
		}

		dwRet = GetLastError();

		if (dwRet != ERROR_INSUFFICIENT_BUFFER)
		{
			_tprintf(_T("ERROR_INSUFFICIENT_BUFFER \n"));
			break;
		}
	}

	if (dwRet == ERROR_SUCCESS)
	{
		if (NULL != pLayout)			
			*ppLayout = pLayout;
	}
	else
	{
		if (pLayout) {
			free(pLayout);
			pLayout = NULL;
		}
	}

	return dwRet;
}



bool
ParseCommandLineOptions(int argc, TCHAR *argv[], PCOMMAND_LINE_OPTIONS pCommandLineOptions)
{
    int iParsedOptions = 1;
    bool IsPrintStats = false;
    while (argc > iParsedOptions) {
        if ((0 == _tcsicmp(argv[iParsedOptions], CMDSTR_SHUTDOWN_NOTIFY)) ||
            (0 == _tcsicmp(argv[iParsedOptions], CMDSTR_SHUTDOWN_NOTIFY_SF)) )
        {
            pCommandLineOptions->bOpenVolumeFilterControlDevice = true;
            if (false == ParseShutdownNotifyCommand(argv, argc, iParsedOptions, pCommandLineOptions)){
                _tprintf(_T("\nUSAGE\n"));
                _tprintf(_T("  %s %s [Option]\n"),argv[0],CMDSTR_SHUTDOWN_NOTIFY);
                _tprintf(_T("  %s %s [Option]\n"),argv[0],CMDSTR_SHUTDOWN_NOTIFY_SF);
                _tprintf(_T("\nDESCRIPTION\n"));
                _tprintf(_T("  This Command Sends Shutdown notify IOCTL to driver\n"));
                _tprintf(_T("  Cancellation of this command gives driver an indication that the system/ \n  service is getting shutdown\n"));
                _tprintf(_T("\nOPTIONS\n"));
                _tprintf(_T("  %s\n"), CMDOPTSTR_DISABLE_DATA_MODE);
                _tprintf(_T("     Disable data filtering.By default data filtering is enabled.\n\n"));
                _tprintf(_T("  %s\n"), CMDOPTSTR_DISABLE_DATA_FILES);
                _tprintf(_T("     Disable data files.By default data file support is enabled.\n\n"));
                if(help)
                    help=false;
                else
                    return false;
            }
        } else if ((0 == _tcsicmp(argv[iParsedOptions], CMDSTR_GET_DRIVER_VERSION)) ||
                   (0 == _tcsicmp(argv[iParsedOptions], CMDSTR_GET_DRIVER_VERSION_SF))) 
        {
            pCommandLineOptions->bOpenVolumeFilterControlDevice = true;
            if (false == ParseGetDriverVersionCommand(argv, argc, iParsedOptions, pCommandLineOptions)){
                _tprintf(_T("\nUSAGE\n"));
                _tprintf(_T("  %s %s \n"),argv[0],CMDSTR_GET_DRIVER_VERSION);
                _tprintf(_T("  %s %s \n"),argv[0],CMDSTR_GET_DRIVER_VERSION_SF);
                _tprintf(_T("\nDESCRIPTION\n"));
                _tprintf(_T("  This command gets the driver and product version installed on the system\n\n"));
                if(help)
                    help=false;
                else
                    return false;
            }
        } else if ((0 == _tcsicmp(argv[iParsedOptions], CMDSTR_VERBOSE)) ||
                   (0 == _tcsicmp(argv[iParsedOptions], CMDSTR_VERBOSE_SF)) )
        {
            pCommandLineOptions->bVerbose = true;
            if (false == ParseVerboseCommand(argv, argc, iParsedOptions, pCommandLineOptions)){
                _tprintf(_T("\nUSAGE\n"));
                _tprintf(_T("  %s %s \n"),argv[0],CMDSTR_VERBOSE);
                _tprintf(_T("  %s %s \n"),argv[0],CMDSTR_VERBOSE_SF);
                _tprintf(_T("\nDESCRIPTION\n"));
                _tprintf(_T("  This command Turn on verbose printing.\n\n"));
                if(help)
                    help=false;
                else
                    return false;
            }
        } else if (0 == _tcsicmp(argv[iParsedOptions], CMDSTR_CHECK_SHUTDOWN_STATUS)) {
            if (false == ParseCheckShutdownStatusCommand(argv, argc, iParsedOptions, pCommandLineOptions)){
                _tprintf(_T("\nUSAGE\n"));
                _tprintf(_T("  %s %s \n"),argv[0],CMDSTR_CHECK_SHUTDOWN_STATUS);
                _tprintf(_T("\nDESCRIPTION\n"));
                _tprintf(_T("  This command check that the Shutdown Notification is completed or not.\n"));
                _tprintf(_T("  The Use of this command is only Valid only if %s options is\n  specified.\n\n"),CMDSTR_SHUTDOWN_NOTIFY);
                if(help)
                    help=false;
                else
                    return false;
            }
        } else if ((0 == _tcsicmp(argv[iParsedOptions], CMDSTR_PROCESS_START_NOTIFY)) ||
                   (0 == _tcsicmp(argv[iParsedOptions], CMDSTR_PROCESS_START_NOTIFY_SF)) )
        {
            pCommandLineOptions->bOpenVolumeFilterControlDevice = true;
            if (false == ParseServiceStartNotifyCommand(argv, argc, iParsedOptions, pCommandLineOptions)) {
                _tprintf(_T("\nUSAGE\n"));
                _tprintf(_T("  %s %s [Option]\n"),argv[0],CMDSTR_PROCESS_START_NOTIFY);
                _tprintf(_T("  %s %s [Option]\n"),argv[0],CMDSTR_PROCESS_START_NOTIFY_SF);      
                _tprintf(_T("\nDESCRIPTION\n"));
                _tprintf(_T("  Command indicates, user process is capable of absoring data from dirtyblock.\n"));
                _tprintf(_T("  If the user process needs the Dirty block from the driver it has to send\n  this command mandatory.\n"));
                _tprintf(_T("\nOPTIONS\n"));
                _tprintf(_T("  %s\n"), CMDOPTSTR_DISABLE_DATA_FILES);
                _tprintf(_T("     User Process does not support data files. By default data files supported.\n"));  
                if(help)
                    help=false;
                else
                    return false;
            }
        } else if (0 == _tcsicmp(argv[iParsedOptions], CMDSTR_CHECK_PROCESS_START_STATUS)) {
            if (false == ParseCheckServiceStartStatusCommand(argv, argc, iParsedOptions, pCommandLineOptions)) {
                _tprintf(_T("\nUSAGE\n"));
                _tprintf(_T("  %s %s \n"),argv[0],CMDSTR_CHECK_PROCESS_START_STATUS);
                _tprintf(_T("\nDESCRIPTION\n"));
                _tprintf(_T("  This command check that the Service Start Notification is completed or not.\n"));
                _tprintf(_T("  The Use of this command is only Valid only if %s option\n  is specified.\n\n"),CMDSTR_PROCESS_START_NOTIFY);
                if(help)
                    help=false;
                else
                    return false;
            }
        } else if ((0 == _tcsicmp(argv[iParsedOptions], CMDSTR_GET_DATA_MODE)) ||
                   (0 == _tcsicmp(argv[iParsedOptions], CMDSTR_GET_DATA_MODE_SF)) )
        {
            if (false == ParseGetDataModeCommand(argv, argc, iParsedOptions, pCommandLineOptions)){
                _tprintf(_T("\nUSAGE\n"));
                _tprintf(_T("  %s %s [VolumeLetter]\n"),argv[0],CMDSTR_GET_DATA_MODE);
                _tprintf(_T("  %s %s [VolumeLetter]\n"),argv[0],CMDSTR_GET_DATA_MODE_SF);
                _tprintf(_T("\nDESCRIPTION\n"));
                _tprintf(_T("  Command Display the Data mode statistics for the volume specified.\n"));
                _tprintf(_T("  If VolumeLetter is not specified statistics of all volumes are displayed.\n\n"));
                if(help)
                    help=false;
                else
                    return false;
            }
            // This is a work around to display the compact output for drvutil.exe --ps --dm command output
            // and allow only for this case. This is going to be depricated when the tracing is enabled.
            // drvutil.exe --ps --dm --v is used for full verbose printing
            if (IsPrintStats && (3 == argc)) {
                pCommandLineOptions->bCompact = true;
            }
            pCommandLineOptions->bOpenVolumeFilterControlDevice = true;
        } else if ((0 == _tcsicmp(argv[iParsedOptions], CMDSTR_PRINT_STATISTICS)) ||
                   (0 == _tcsicmp(argv[iParsedOptions], CMDSTR_PRINT_STATISTICS_SF)) )
        {
            if (false == ParsePrintStatisticsCommand(argv, argc, iParsedOptions, pCommandLineOptions)){
                _tprintf(_T("\nUSAGE\n"));
                _tprintf(_T("  %s %s [VolumeLetter]\n"),argv[0],CMDSTR_PRINT_STATISTICS);
                _tprintf(_T("  %s %s [VolumeLetter]\n"),argv[0],CMDSTR_PRINT_STATISTICS_SF);
                _tprintf(_T("\nDESCRIPTION\n"));
                _tprintf(_T("  Command Display the General Statistics for the volume specified\n"));
                _tprintf(_T("  If VolumeLetter is not specified Statistics of all volumes are displayed.\n\n"));
                if(help)
                    help=false;
                else
                    return false;
            }
            IsPrintStats = true;
            pCommandLineOptions->bOpenVolumeFilterControlDevice = true;
        } else if ((0 == _tcsicmp(argv[iParsedOptions], CMDSTR_PRINT_ADDITIONAL_STATS)) ||
                  (0 == _tcsicmp(argv[iParsedOptions], CMDSTR_PRINT_ADDITIONAL_STATS_SF)) )
        {
            if (false == ParsePrintAdditionalStatsCommand(argv, argc, iParsedOptions, pCommandLineOptions)){
                _tprintf(_T("\nUSAGE\n"));
                _tprintf(_T("  %s %s VolumeLetter\n"),argv[0],CMDSTR_PRINT_ADDITIONAL_STATS);
                _tprintf(_T("  %s %s VolumeLetter\n"),argv[0],CMDSTR_PRINT_ADDITIONAL_STATS_SF);
                _tprintf(_T("\nDESCRIPTION\n"));
                _tprintf(_T("  Command Display the Additional Volume Stats for the volume specified.\n"));
                if(help)
                    help=false;
                else
                    return false;
            }
            pCommandLineOptions->bOpenVolumeFilterControlDevice = true;
        }else if (0 == _tcsicmp(argv[iParsedOptions], CMDSTR_GET_GLOABL_TSSN))
        {
            if (false == ParsePrintGlobaTSSNCommand(argv, argc, iParsedOptions, pCommandLineOptions)){
                _tprintf(_T("\nUSAGE\n"));
                _tprintf(_T("  %s %s \n"),argv[0],CMDSTR_GET_GLOABL_TSSN);
                _tprintf(_T("\nDESCRIPTION\n"));
                _tprintf(_T("  Command Displays the Global Timestamp and Sequence Number.\n"));
                if(help)
                    help=false;
                else
                    return false;
            }
            pCommandLineOptions->bOpenVolumeFilterControlDevice = true;
		}
		else if ((0 == _tcsicmp(argv[iParsedOptions], CMDSTR_GET_VOLUME_SIZE)) ||
                   (0 == _tcsicmp(argv[iParsedOptions], CMDSTR_GET_VOLUME_SIZE_SF)) )
        {
            if (false == ParseGetVolumeSizeCommand(argv, argc, iParsedOptions, pCommandLineOptions)){
                _tprintf(_T("\nUSAGE\n"));
                _tprintf(_T("  %s %s VolumeLetter \n"),argv[0],CMDSTR_GET_VOLUME_SIZE);
                _tprintf(_T("  %s %s VolumeLetter \n"),argv[0],CMDSTR_GET_VOLUME_SIZE_SF);
                _tprintf(_T("\nDESCRIPTION\n"));
                _tprintf(_T("  Command Display the Volume Size for the volume specified.\n"));
                if(help)
                    help=false;
                else
                    return false;
            }
            pCommandLineOptions->bOpenVolumeFilterControlDevice = true;
        }else if ((0 == _tcsicmp(argv[iParsedOptions], CMDSTR_GET_VOLUME_FLAGS)) ||
                  (0 == _tcsicmp(argv[iParsedOptions], CMDSTR_GET_VOLUME_FLAGS_SF)) )
        {
            if (false == ParseGetVolumeFlagsCommand(argv, argc, iParsedOptions, pCommandLineOptions)){
                _tprintf(_T("\nUSAGE\n"));
                _tprintf(_T("  %s %s VolumeLetter \n"),argv[0],CMDSTR_GET_VOLUME_FLAGS);
                _tprintf(_T("  %s %s VolumeLetter \n"),argv[0],CMDSTR_GET_VOLUME_FLAGS_SF);
                _tprintf(_T("\nDESCRIPTION\n"));
                _tprintf(_T("  Command Display the Flags for the volume specified.\n"));
                if(help)
                    help=false;
                else
                    return false;
            }
            pCommandLineOptions->bOpenVolumeFilterControlDevice = true;
        }else if ((0 == _tcsicmp(argv[iParsedOptions], CMDSTR_GET_VOLUME_WRITE_ORDER_STATE)) ||
                  (0 == _tcsicmp(argv[iParsedOptions], CMDSTR_GET_VOLUME_WRITE_ORDER_STATE_SF)) )
        {
            if (false == ParseGetWriteOrderStateCommand(argv, argc, iParsedOptions, pCommandLineOptions)){
                _tprintf(_T("\nUSAGE\n"));
                _tprintf(_T("  %s %s VolumeLetter\n"),argv[0],CMDSTR_GET_VOLUME_WRITE_ORDER_STATE);
                _tprintf(_T("  %s %s VolumeLetter\n"),argv[0],CMDSTR_GET_VOLUME_WRITE_ORDER_STATE_SF);
                _tprintf(_T("\nDESCRIPTION\n"));
                _tprintf(_T("  Command Display the Volume Write Order State for the volume specified.\n"));
                if(help)
                    help=false;
                else
                    return false;
            }
            pCommandLineOptions->bOpenVolumeFilterControlDevice = true;
        }else if ((0 == _tcsicmp(argv[iParsedOptions], CMDSTR_CLEAR_STATISTICS)) ||
                   (0 == _tcsicmp(argv[iParsedOptions], CMDSTR_CLEAR_STATISTICS_SF)) )
        {
            if (false == ParseClearStatisticsCommand(argv, argc, iParsedOptions, pCommandLineOptions)){
                _tprintf(_T("\nUSAGE\n"));
                _tprintf(_T("  %s %s [VolumeLetter]\n"),argv[0],CMDSTR_CLEAR_STATISTICS);
                _tprintf(_T("  %s %s [VolumeLetter]\n"),argv[0],CMDSTR_CLEAR_STATISTICS_SF);
                _tprintf(_T("\nDESCRIPTION\n"));
                _tprintf(_T("  Command Clears the Statistics for the volume specified.\n"));
                _tprintf(_T("  If VolumeLetter is not specified Statistics of all volumes are Cleared.\n\n"));
                if(help)
                    help=false;
                else
                    return false;
            }
            pCommandLineOptions->bOpenVolumeFilterControlDevice = true;
        } else if (0 == _tcsicmp(argv[iParsedOptions], CMDSTR_CLEAR_DIFFS)) {
            if (false == ParseClearDiffsCommand(argv, argc, iParsedOptions, pCommandLineOptions)){
                _tprintf(_T("\nUSAGE\n"));
                _tprintf(_T("  %s %s VolumeLetter \n"),argv[0],CMDSTR_CLEAR_DIFFS);
                _tprintf(_T("\nDESCRIPTION\n"));
                _tprintf(_T("  This Command Clear the diffs of a Volume representing drive letter.\n"));
                if(help)
                    help=false;
                else
                    return false;
            }
            pCommandLineOptions->bOpenVolumeFilterControlDevice = true;
        } else if ((0 == _tcsicmp(argv[iParsedOptions], CMDSTR_START_FILTERING)) ||
                   (0 == _tcsicmp(argv[iParsedOptions], CMDSTR_START_FILTERING_SF)) )
        {
            if (false == ParseStartFilteringCommand(argv, argc, iParsedOptions, pCommandLineOptions)){
                _tprintf(_T("\nUSAGE\n"));
                _tprintf(_T("  %s %s VolumeLetter\n"),argv[0],CMDSTR_START_FILTERING);
                _tprintf(_T("  %s %s VolumeLetter\n"),argv[0],CMDSTR_START_FILTERING_SF);
                _tprintf(_T("\nDESCRIPTION\n"));
                _tprintf(_T("  This Command Starts the Filtering on a Volume representing drive letter.\n"));
                if(help)
                    help=false;
                else
                    return false;
            }
            pCommandLineOptions->bOpenVolumeFilterControlDevice = true;
        } else if ((0 == _tcsicmp(argv[iParsedOptions], CMDSTR_RESYNC_START_NOTIFY)) ||
                   (0 == _tcsicmp(argv[iParsedOptions], CMDSTR_RESYNC_START_NOTIFY_SF)) )
        {
            if (false == ParseResyncStartNotifyCommand(argv, argc, iParsedOptions, pCommandLineOptions)){
                _tprintf(_T("\nUSAGE\n"));
                _tprintf(_T("  %s %s VolumeLetter\n"),argv[0],CMDSTR_RESYNC_START_NOTIFY);
                _tprintf(_T("  %s %s VolumeLetter\n"),argv[0],CMDSTR_RESYNC_START_NOTIFY_SF);
                _tprintf(_T("\nDESCRIPTION\n"));
                _tprintf(_T("  This Command notifies resync Start on a Volume representing drive letter.\n"));
                if(help)
                    help=false;
                else
                    return false;
            }
            pCommandLineOptions->bOpenVolumeFilterControlDevice = true;
        } else if ((0 == _tcsicmp(argv[iParsedOptions], CMDSTR_RESYNC_END_NOTIFY)) ||
                   (0 == _tcsicmp(argv[iParsedOptions], CMDSTR_RESYNC_END_NOTIFY_SF)) )
        {
            if (false == ParseResyncEndNotifyCommand(argv, argc, iParsedOptions, pCommandLineOptions)){
                _tprintf(_T("\nUSAGE\n"));
                _tprintf(_T("  %s %s VolumeLetter\n"),argv[0],CMDSTR_RESYNC_END_NOTIFY);
                _tprintf(_T("  %s %s VolumeLetter\n"),argv[0],CMDSTR_RESYNC_END_NOTIFY_SF);
                _tprintf(_T("\nDESCRIPTION\n"));
                _tprintf(_T("  This Command notifies resync End on a Volume representing drive letter.\n"));
                if(help)
                    help=false;
                else
                    return false;
            }
            pCommandLineOptions->bOpenVolumeFilterControlDevice = true;
        }
        else if (0 == _tcsicmp(argv[iParsedOptions], CMDSTR_SET_DISK_CLUSTERED)) {
            if (false == ParseSetDiskClusteredCommand(argv, argc, iParsedOptions, pCommandLineOptions)) {
                _tprintf(_T("\nUSAGE\n"));
                _tprintf(_T("  %s %s VolumeLetter\n"), argv[0], CMDSTR_SET_DISK_CLUSTERED);
                _tprintf(_T("\nDESCRIPTION\n"));
                _tprintf(_T("  This Command sets disk clustered.\n"));
                if (help)
                    help = false;
                else
                    return false;
            }
            pCommandLineOptions->bOpenVolumeFilterControlDevice = true;
        }
        else if ((0 == _tcsicmp(argv[iParsedOptions], CMDSTR_STOP_FILTERING)) ||
                   (0 == _tcsicmp(argv[iParsedOptions], CMDSTR_STOP_FILTERING_SF)) )
        {
            if (false == ParseStopFilteringCommand(argv, argc, iParsedOptions, pCommandLineOptions)){
                _tprintf(_T("\nUSAGE\n"));
                _tprintf(_T("  %s %s VolumeLetter/%s [Option]\n"),argv[0],CMDSTR_STOP_FILTERING, CMDOPTSTR_STOP_ALL);
                _tprintf(_T("  %s %s VolumeLetter/%s [Option]\n"),argv[0],CMDSTR_STOP_FILTERING_SF, CMDOPTSTR_STOP_ALL);
                _tprintf(_T("\nDESCRIPTION\n"));
                _tprintf(_T("  This Command Stops the Filtering on a Volume representing drive letter Or Stop Filtering on all volumes if %s specified \n"), CMDOPTSTR_STOP_ALL);
                _tprintf(_T("\nOPTIONS\n"));
                _tprintf(_T("  %s \n     Enforce to stop filtering and Delete Bitmap file for the Volume letter or All volumes \n     specified.\n\n"), CMDOPTSTR_BITMAP_DELETE);
                if(help)
                    help=false;
                else
                    return false;
            }
            pCommandLineOptions->bOpenVolumeFilterControlDevice = true;
        } else if (0 == _tcsicmp(argv[iParsedOptions], CMDSTR_WAIT_TIME)) {
            if (false == ParseWaitTimeCommand(argv, argc, iParsedOptions, pCommandLineOptions)){
                _tprintf(_T("\nUSAGE\n"));
                _tprintf(_T("  %s %s MilliSeconds\n"),argv[0],CMDSTR_WAIT_TIME);
                _tprintf(_T("\nDESCRIPTION\n"));
                _tprintf(_T("  Using this command user can specify the Time to wait before executing next\n  command option\n"));
                if(help)
                    help=false;
                else
                    return false;
            }
        }
        else if (0 == _tcsicmp(argv[iParsedOptions], CMDSTR_GET_SYNC)) {
            if (false == ParseGetSyncCommand_v2(argv, argc, iParsedOptions, pCommandLineOptions)){
                _tprintf(_T("\nUSAGE\n"));                
                _tprintf(_T("   %s  %s [--sn] [--psn] -srcdisk <DiskName> -srcsign <disksig/guid> -tgtvhd VHDfileName [Options] \n"),argv[0],CMDSTR_GET_SYNC);
                _tprintf(_T("\nDESCRIPTION\n"));
                _tprintf(_T("  This command setups replication between src disk and VHD file; it does IR and DR\n"));
                _tprintf(_T("  --psn is required for first disk is being protected \n"));
                _tprintf(_T("\nOPTIONS\n"));
                _tprintf(_T("  %s SKIP Step one.\n"),CMDOPTSTR_SKIP_STEP_ONE);
                _tprintf(_T("  %s Level \n     Print dirtyblock details.\n"), CMDOPTSTR_PRINT_DETAILS);
                _tprintf(_T("     Level = 1(Default) Prints time stamps, number of changes, etc.\n"));
                _tprintf(_T("     Level = 2 Prints each change in dirty block\n\n"));
                _tprintf(_T("  %s STOP replication on Tag. \n"), CMDOPTSTR_STOP_ON_TAG);
                if(help)
                    help=false;
                else
                    return false;
            }
            pCommandLineOptions->bOpenVolumeFilterControlDevice = true;
        } else if (0 == _tcsicmp(argv[iParsedOptions], CMDSTR_GET_DB_TRANS)) {
            if (false == ParseGetDirtyBlocksCommand(argv, argc, iParsedOptions, pCommandLineOptions)){
                _tprintf(_T("\nUSAGE\n"));
                _tprintf(_T("  %s %s VolumeLetter [Option]\n"),argv[0],CMDSTR_GET_DB_TRANS);
                _tprintf(_T("\nDESCRIPTION\n"));
                _tprintf(_T("  This Command is use to Get the Dirty blocks of a volume representing drive\n  letter.\n"));
                _tprintf(_T("\nOPTIONS\n"));
                _tprintf(_T("  %s Seconds \n     Specifies the total run time to get the dirty block.\n\n"), CMDOPTSTR_RUN_FOR_TIME);
                _tprintf(_T("  %s NumberOfDirtyBlocks \n     Number of dirty blocks to get.\n\n"), CMDOPTSTR_NUM_DIRTY_BLOCKS);
                _tprintf(_T("  %s \n     Commit previous.\n\n"), CMDOPTSTR_COMMIT_PREV);
                _tprintf(_T("  %s \n     Commit current.\n\n"), CMDOPTSTR_COMMIT_CURRENT);
                _tprintf(_T("  %s MilliSeconds \n     Commit current with timeout.\n\n"), CMDOPTSTR_COMMIT_TIMEOUT);
                _tprintf(_T("  %s Level \n     Print dirtyblock details.\n"), CMDOPTSTR_PRINT_DETAILS);
                _tprintf(_T("     Level = 1(Default) Prints time stamps, number of changes, etc.\n"));
                _tprintf(_T("     Level = 2 Prints each change in dirty block\n\n"));
                _tprintf(_T("  %s Seconds \n     Poll for dirty blocks, 60 seconds is default (Does not use event\n     notification).\n\n"), CMDOPTSTR_POLL_INTERVAL);
                _tprintf(_T("  %s \n     Reset 'Resync required' if Resync Required flag is returned in dirtyblock.\n\n"), CMDOPTSTR_RESET_RESYNC);
                if(help)
                    help=false;
                else
                    return false;
            }
            pCommandLineOptions->bOpenVolumeFilterControlDevice = true;
        } else if (0 == _tcsicmp(argv[iParsedOptions], CMDSTR_COMMIT_DB_TRANS)) {
            if (false == ParseCommitBlocksCommand(argv, argc, iParsedOptions, pCommandLineOptions)){
                _tprintf(_T("\nUSAGE\n"));
                _tprintf(_T("  %s %s VolumeLetter [Option]\n"),argv[0],CMDSTR_COMMIT_DB_TRANS);
                _tprintf(_T("\nDESCRIPTION\n"));
                _tprintf(_T("  This Command is use to Commit Dirty blocks of a volume representing drive\n  letter.\n"));
                _tprintf(_T("\nOPTIONS\n"));
                _tprintf(_T("  %s \n     Resets the  Resync Required Flag.\n"), CMDOPTSTR_RESET_RESYNC);
                if(help)
                    help=false;
                else
                    return false;
            }
            pCommandLineOptions->bOpenVolumeFilterControlDevice = true;
        } else if (0 == _tcsicmp(argv[iParsedOptions], CMDSTR_SET_RESYNC_REQUIRED)) {
            if (false == ParseSetResyncRequired(argv, argc, iParsedOptions, pCommandLineOptions)) {
                _tprintf(_T("\nUSAGE\n"));
                _tprintf(_T("  %s %s VolumeLetter [Option]\n"),argv[0],CMDSTR_SET_RESYNC_REQUIRED);
                _tprintf(_T("\nDESCRIPTION\n"));
                _tprintf(_T("  This Command Set 'Resync Required' for a volume representing drive letter\n"));
                _tprintf(_T("  with the Error code and  Error Status specified in the suboption.\n"));
                _tprintf(_T("\nOPTIONS\n"));
                _tprintf(_T("  %s \n     ErrorCode (Default is 0x01 \"Out of NonPagedPool for dirty blocks\")\n\n"), CMDOPTSTR_ERROR_CODE);
                _tprintf(_T("  %s \n     ErrorStatus (Default is 0x00)\n\n"), CMDOPTSTR_ERROR_STATUS);
                if(help)
                    help=false;
                else
                    return false;
            }
            pCommandLineOptions->bOpenVolumeFilterControlDevice = true;
        } else if ((0 == _tcsicmp(argv[iParsedOptions], CMDSTR_WAIT_FOR_KEYWORD)) ||
                   (0 == _tcsicmp(argv[iParsedOptions], CMDSTR_WAIT_FOR_KEYWORD_SF)) )
        {
            if (false == ParseWaitForKeywordCommand(argv, argc, iParsedOptions, pCommandLineOptions)){
                _tprintf(_T("\nUSAGE\n"));
                _tprintf(_T("  %s %s KeyWord \n"),argv[0],CMDSTR_WAIT_FOR_KEYWORD);
                _tprintf(_T("  %s %s KeyWord \n"),argv[0],CMDSTR_WAIT_FOR_KEYWORD_SF);
                _tprintf(_T("\nDESCRIPTION\n"));
                _tprintf(_T("  This Command makes the CLI to waits for user to enter the [KeyWord] on\n  separate line.\n"));
                if(help)
                    help=false;
                else
                    return false;
            }
        } else if (0 == _tcsicmp(argv[iParsedOptions], CMDSTR_SET_VOLUME_FLAGS)) {
            if (false == ParseSetVolumeFlagsCommand(argv, argc, iParsedOptions, pCommandLineOptions)){
                _tprintf(_T("\nUSAGE\n"));
                _tprintf(_T("  %s %s VolumeLetter [Option]\n"),argv[0],CMDSTR_SET_VOLUME_FLAGS);
                _tprintf(_T("\nDESCRIPTION\n"));
                _tprintf(_T("  This Command is use to Set flags of a volume representing drive letter.\n"));
                _tprintf(_T("\nOPTIONS\n"));
                _tprintf(_T("  %s \n     Reset flags instead of setting.\n\n"), CMDOPTSTR_RESET_FLAGS);
                _tprintf(_T("  %s \n     Ignore page file writes.\n\n"), CMDOPTSTR_IGNORE_PAGEFILE_WRITES);
                _tprintf(_T("  %s \n     Disable bitmap read.\n\n"), CMDOPTSTR_DISABLE_BITMAP_READ);
                _tprintf(_T("  %s \n     Disable bitamp write.\n\n"), CMDOPTSTR_DISABLE_BITMAP_WRITE);
                _tprintf(_T("  %s \n     Mark the volume online.\n\n"), CMDOPTSTR_MARK_VOLUME_ONLINE);
                _tprintf(_T("  %s \n     Mark the volume read only.\n\n"), CMDOPTSTR_MARK_READ_ONLY);
                _tprintf(_T("  %s \n     Enable Source Volume Rollback(used with mark read only flag).\n\n"), CMDOPTSTR_ENABLE_SOURCE_ROLLBACK);
                _tprintf(_T("  %s \n     Disable data capture.\n\n"), CMDOPTSTR_DISABLE_DATA_CAPTURE);
                _tprintf(_T("  %s \n     Disable data files creation.\n\n"), CMDOPTSTR_DISABLE_DATA_FILES);
                _tprintf(_T("  %s \n     Free data buffers used.\n\n"), CMDOPTSTR_FREE_DATA_BUFFERS);
                _tprintf(_T("  %s \n     Volume Context Fields Persited Flag.\n\n"), CMDOPTSTR_VCONTEXT_FIELDS_PERSISTED);
				_tprintf(_T("  %s \n     Volume Data Log Directory.\n\n"), CMDOPTSTR_VOLUME_DATA_LOG_DIRECTORY);
                if(help)
                    help=false;
                else
                    return false;
            }
            pCommandLineOptions->bOpenVolumeFilterControlDevice = true;
        } else if ((0 == _tcsicmp(argv[iParsedOptions], CMDSTR_SET_DRIVER_FLAGS)) ||
                   (0 == _tcsicmp(argv[iParsedOptions], CMDSTR_SET_DRIVER_FLAGS_SF)) ) 
        {
            if (false == ParseSetDriverFlagsCommand(argv, argc, iParsedOptions, pCommandLineOptions)){
                _tprintf(_T("\nUSAGE\n"));
                _tprintf(_T("  %s %s [Option]\n"),argv[0],CMDSTR_SET_DRIVER_FLAGS);
                _tprintf(_T("  %s %s [Option]\n"),argv[0],CMDSTR_SET_DRIVER_FLAGS_SF);
                _tprintf(_T("\nDESCRIPTION\n"));
                _tprintf(_T("  This Command Set/Reset driver level flags.\n"));
                _tprintf(_T("\nOPTIONS\n"));
                _tprintf(_T("  %s\n     Reset flags instead of setting.\n\n"), CMDOPTSTR_RESET_FLAGS);
                _tprintf(_T("  %s\n     Disable data file support.\n\n"), CMDOPTSTR_DISABLE_DATA_FILES);
                _tprintf(_T("  %s\n     Disable data file support for new volumes.\n\n"), CMDOPTSTR_DISABLE_DATA_FILES_FOR_NV);
                if(help)
                    help=false;
                else
                    return false;
            }
            pCommandLineOptions->bOpenVolumeFilterControlDevice = true;
        } else if (0 == _tcsicmp(argv[iParsedOptions], CMDSTR_SET_DEBUG_INFO)) {
            if (false == ParseSetDebugInfoCommand(argv, argc, iParsedOptions, pCommandLineOptions)){
                _tprintf(_T("\nUSAGE\n"));
                _tprintf(_T("  %s %s [Options]\n"),argv[0],CMDSTR_SET_DEBUG_INFO);
                _tprintf(_T("\nDESCRIPTION\n"));
                _tprintf(_T("  This Command Sets Debugging flags.\n"));
                _tprintf(_T("\nOPTIONS\n"));
                _tprintf(_T("  %s\n     Reset flags instead of setting.\n\n"), CMDOPTSTR_RESET_FLAGS);
                _tprintf(_T("  %s\n     Set debugging level for all.\n\n"), CMDOPTSTR_DEBUG_LEVEL_FORALL);
                _tprintf(_T("  %s\n     Set debugging level for debug modules.\n\n"), CMDOPTSTR_DEBUG_LEVEL);
                _tprintf(_T("  %s DebugModules\n     Specify debugging modules.\n"), CMDOPTSTR_DEBUG_MODULES);
                _tprintf(_T("     DebugModules are: %s, %s, %s, %s,\n"), DM_STR_DRIVER_INIT, DM_STR_DRIVER_UNLOAD, DM_STR_DRIVER_PNP,DM_STR_SHUTDOWN);
                _tprintf(_T("     %s, %s, %s, %s, %s,\n"), DM_STR_VOLUME_ARRIVAL,DM_STR_REGISTRY, DM_STR_VOLUME_CONTEXT,DM_STR_CLUSTER,DM_STR_BITMAP_OPEN);
                _tprintf(_T("     %s, %s, %s, %s, %s, %s,\n"), DM_STR_BITMAP_READ, DM_STR_BITMAP_WRITE,DM_STR_MEM_TRACE, DM_STR_IOCTL_TRACE, DM_STR_POWER,DM_STR_WRITE);
                _tprintf(_T("     %s, %s, %s,%s, %s. \n"),DM_STR_PAGEFILE, DM_STR_DIRTY_BLOCKS, DM_STR_VOLUME_STATE,DM_STR_INMAGE_IOCTL, DM_STR_ALL);
                _tprintf(_T("     \n\n"));
                if(help)
                    help=false;
                else
                    return false;
            }
            pCommandLineOptions->bOpenVolumeFilterControlDevice = true;
        } else if (0 == _tcsicmp(argv[iParsedOptions], CMDSTR_TAG_VOLUME)) {
				if (false == ParseTagVolumeCommand(argv, argc, iParsedOptions, pCommandLineOptions)){
					_tprintf(_T("\nUSAGE\n"));
					_tprintf(_T("  %s %s VolumeLetter[s] Tag[s] [Option]\n"), argv[0], CMDSTR_TAG_VOLUME);
					_tprintf(_T("\nDESCRIPTION\n"));
					_tprintf(_T("  This Command Set Tag for a device representing drive letter/GUID for volume based and GUID for disk based\n"));
					_tprintf(_T("\nOPTIONS\n"));
					_tprintf(_T("  %s\n     Set tags atomically across all specified devices.\n\n"), CMDOPTSTR_ATOMIC);
					_tprintf(_T("  %s\n     Wait till tags are commited/dropped/deleted.\n\n"), CMDOPTSTR_SYNCHRONOUS);
					_tprintf(_T("  %s\n     Prompt (y/n) before waiting.\n\n"), CMDOPTSTR_PROMPT);
					_tprintf(_T("  %s TimesToLoop\n     Number of times to loop to wait.\n\n"), CMDOPTSTR_COUNT);
					_tprintf(_T("  %s\n     Ignore device if filtering stopped.\n\n"), CMDOPTSTR_IGNORE_FILTERING_STOPPED);
					_tprintf(_T("  %s\n     Ignore device if GUID not found.\n\n"), CMDOPTSTR_IGNORE_IF_GUID_NOT_FOUND);
					_tprintf(_T("  %s TimeInSec\n     Time to wait for status update.\n\n"), CMDOPTSTR_TIMEOUT);
					if (help)
						help = false;
					else
						return false;
            }
            pCommandLineOptions->bOpenVolumeFilterControlDevice = true;
        } else if (0 == _tcsicmp(argv[iParsedOptions], CMDSTR_MOUNT_VIRTUAL_VOLUME)) {
            if (false == ParseMountVirtualVolumeCommand(argv, argc, iParsedOptions, pCommandLineOptions)){
                _tprintf(_T("\nUSAGE\n"));
                _tprintf(_T("  %s %s FileName|SourceDriveLetter TargetDriveLetter\n"),argv[0],CMDSTR_MOUNT_VIRTUAL_VOLUME);
                _tprintf(_T("\nDESCRIPTION\n"));
                _tprintf(_T("  This Command Mounts the Virtual Volume from the File Name Containing Volume\n  Image specified to the Target drive letter\n"));
                if(help)
                    help=false;
                else
                    return false;         
            }
            pCommandLineOptions->bOpenVirtualVolumeControlDevice = true;
        } else if (0 == _tcsicmp(argv[iParsedOptions], CMDSTR_CONFIGURE_VIRTUAL_VOLUME)) {
            if (false == ParseConfigureVirtualVolumeCommand(argv, argc, iParsedOptions, pCommandLineOptions)){
                _tprintf(_T("\nUSAGE\n"));
                _tprintf(_T("  %s %s \n"),argv[0],CMDSTR_CONFIGURE_VIRTUAL_VOLUME);
                _tprintf(_T("\nDESCRIPTION\n"));
                _tprintf(_T("  This Command Configures InVirVol Driver.\n"));
                _tprintf(_T("\nOPTIONS\n"));
                _tprintf(_T("  %s \n     Memory used for file handle cache in KB.\n\n"), CMDOPTSTR_MEM_FILE_HANDLE_CACHE_KB);
                
                if(help)
                    help=false;
                else
                    return false;            
            }
            pCommandLineOptions->bOpenVirtualVolumeControlDevice = true;
        } else if (0 == _tcsicmp(argv[iParsedOptions], CMDSTR_STOP_VIRTUAL_VOLUME)) {
            if (false == ParseStopVirtualVolumeCommand(argv, argc, iParsedOptions, pCommandLineOptions)){
                _tprintf(_T("\nUSAGE\n"));
                _tprintf(_T("  %s %s \n"),argv[0],CMDSTR_STOP_VIRTUAL_VOLUME);
                _tprintf(_T("\nDESCRIPTION\n"));
                _tprintf(_T("  This Command Stops the InVirVol Driver.\n"));
                if(help)
                    help=false;
                else
                    return false;            
            }
            pCommandLineOptions->bOpenVirtualVolumeControlDevice = true;
        } else if (0 == _tcsicmp(argv[iParsedOptions], CMDSTR_START_VIRTUAL_VOLUME)) {
            if (false == ParseStartVirtualVolumeCommand(argv, argc, iParsedOptions, pCommandLineOptions)){
                _tprintf(_T("\nUSAGE\n"));
                _tprintf(_T("  %s %s\n"),argv[0],CMDSTR_START_VIRTUAL_VOLUME);
                _tprintf(_T("\nDESCRIPTION\n"));
                _tprintf(_T("  This Command Starts the InVirVol Driver.\n"));
                if(help)
                    help=false;
                else
                    return false;            
            }
            pCommandLineOptions->bOpenVirtualVolumeControlDevice = true;
        } else if (0 == _tcsicmp(argv[iParsedOptions], CMDSTR_UNMOUNT_VIRTUAL_VOLUME)) {
            if (false == ParseUnmountVirtualVolumeCommand(argv, argc, iParsedOptions, pCommandLineOptions)){
                _tprintf(_T("\nUSAGE\n"));
                _tprintf(_T("  %s %s TargetDriveLetter\n"),argv[0],CMDSTR_UNMOUNT_VIRTUAL_VOLUME);
                _tprintf(_T("\nDESCRIPTION\n"));
                _tprintf(_T("  This Command Unmounts the Virtual Volume specified to the Target drive letter.\n"));
                if(help)
                    help=false;
                else
                    return false;            
            }
            pCommandLineOptions->bOpenVirtualVolumeControlDevice = true;            
        } else if (0 == _tcsicmp(argv[iParsedOptions], CMDSTR_CREATE_THREAD)) {
            if (false == ParseCreateThreadCommand(argv, argc, iParsedOptions, pCommandLineOptions)){
                _tprintf(_T("\nUSAGE\n"));
                _tprintf(_T("  %s %s ThreadId\n"),argv[0],CMDSTR_CREATE_THREAD);
                _tprintf(_T("\nDESCRIPTION\n"));
                _tprintf(_T("  This Command Creates the thread with the specified ThreadId.\n"));
                if(help)
                    help=false;
                else
                    return false;            
            }
        }else if (0 == _tcsicmp(argv[iParsedOptions], CMDSTR_END_OF_THREAD)) {
            if (false == ParseEndOfThreadCommand(argv, argc, iParsedOptions, pCommandLineOptions)){
                _tprintf(_T("\nUSAGE\n"));
                _tprintf(_T("  %s %s ThreadId\n"),argv[0],CMDSTR_END_OF_THREAD);
                _tprintf(_T("\nDESCRIPTION\n"));
                _tprintf(_T("  This Command Ends the thread with the specified ThreadId.\n"));
                if(help)
                    help=false;
                else
                    return false;            
            }
        }else if (0 == _tcsicmp(argv[iParsedOptions], CMDSTR_WAIT_FOR_THREAD)) {
            if(false == ParseWaitForThreadCommand(argv, argc, iParsedOptions, pCommandLineOptions)){
                _tprintf(_T("\nUSAGE\n"));
                _tprintf(_T("  %s %s ThreadId\n"),argv[0],CMDSTR_WAIT_FOR_THREAD);
                _tprintf(_T("\nDESCRIPTION\n"));
                _tprintf(_T("  This Command Waits for the thread with the specified ThreadId.\n"));
                if(help)
                    help=false;
                else
                    return false;            
            }
        }else if (0 == _tcsicmp(argv[iParsedOptions], CMDSTR_SET_VV_CONTROL_FLAGS)) {
            if (false == ParseSetControlFlagsCommand(argv, argc, iParsedOptions, pCommandLineOptions)){
                _tprintf(_T("\nUSAGE\n"));
                _tprintf(_T("  %s %s [Options]\n"),argv[0],CMDSTR_SET_VV_CONTROL_FLAGS);
                _tprintf(_T("\nDESCRIPTION\n"));
                _tprintf(_T("  This Command Set flags of a Virtual Volume Control Device.\n"));
                _tprintf(_T("\nOPTIONS\n"));
                _tprintf(_T("  %s\n     Complete the writes on virtual volume out of order.\n\n"), CMDOPTSTR_OUT_OF_ORDER_WRITE);
                _tprintf(_T("  %s\n     Reset flags instead of setting.\n\n"), CMDOPTSTR_RESET_VV_CONTROL_FLAGS);
                if(help)
                    help=false;
                else
                    return false;            
            }
            pCommandLineOptions->bOpenVirtualVolumeControlDevice = true;
        } else if (0 == _tcsicmp(argv[iParsedOptions], CMDSTR_SET_DATA_FILE_WRITER_THREAD_PRIORITY)) {
            if (false == ParseSetDataFileWriterThreadPriority(argv, argc, iParsedOptions, pCommandLineOptions)){
                _tprintf(_T("\nUSAGE\n"));
                _tprintf(_T("  %s %s VolumeLetter [Options]\n"),argv[0],CMDSTR_SET_DATA_FILE_WRITER_THREAD_PRIORITY);
                _tprintf(_T("\nDESCRIPTION\n"));
                _tprintf(_T("  This Command Set data file writer thread's priority for volume.\n"));
                _tprintf(_T("\nOPTIONS\n"));
                _tprintf(_T("  %s Priority\n     Set the value provided as default for new thread's.\n\n"), CMDOPTSTR_SET_DEFAULT);
                _tprintf(_T("  %s Priority\n     Set the value provided as priority for all volumes.\n\n"), CMDOPTSTR_SET_FOR_ALL_VOLUMES);
                if(help)
                    help=false;
                else
                    return false;            
            }
            pCommandLineOptions->bOpenVolumeFilterControlDevice = true;
        } else if (0 == _tcsicmp(argv[iParsedOptions], CMDSTR_SET_WORKER_THREAD_PRIORITY)) {
            if (false == ParseSetWorkerThreadPriority(argv, argc, iParsedOptions, pCommandLineOptions)){
                _tprintf(_T("\nUSAGE\n"));
                _tprintf(_T("  %s %s Priority\n"),argv[0],CMDSTR_SET_WORKER_THREAD_PRIORITY);
                _tprintf(_T("\nDESCRIPTION\n"));
                _tprintf(_T("  This Command Set worker thread's priority.\n"));
                if(help)
                    help=false;
                else
                    return false;            
            }
            pCommandLineOptions->bOpenVolumeFilterControlDevice = true;
        } else if (0 == _tcsicmp(argv[iParsedOptions], CMDSTR_SET_DATA_TO_DISK_SIZE_LIMIT)) {
            if (false == ParseSetDataToDiskSizeLimit(argv, argc, iParsedOptions, pCommandLineOptions)){
                _tprintf(_T("\nUSAGE\n"));
                _tprintf(_T("  %s [%s [VolumeLetter] [Options]]\n"),argv[0],CMDSTR_SET_DATA_TO_DISK_SIZE_LIMIT);
                _tprintf(_T("\nDESCRIPTION\n"));
                _tprintf(_T("  This Command Set volume's data to disk size limit.\n"));
                _tprintf(_T("\nOPTIONS\n"));
                _tprintf(_T("  %s DataToDiskSizeLimitInMB \n     Set the value provided as default for new volumes.\n\n"), CMDOPTSTR_SET_DEFAULT);
                _tprintf(_T("  %s DataToDiskSizeLimitInMB \n     Set the value provided for all volumes.\n"), CMDOPTSTR_SET_FOR_ALL_VOLUMES);
                _tprintf(_T("\nNOTE\n"));
                _tprintf(_T("  If asked to configure for a specific volume and for all volumes, data to disk\n"));
                _tprintf(_T("  size is configured for all volumes, before configuring for the specific volume\n"));
                if(help)
                    help=false;
                else
                    return false;            
            }
            pCommandLineOptions->bOpenVolumeFilterControlDevice = true;
        } else if (0 == _tcsicmp(argv[iParsedOptions], CMDSTR_SET_MIN_FREE_DATA_TO_DISK_SIZE_LIMIT)) {
            if (false == ParseSetDataToMinFreeDiskSizeLimit(argv, argc, iParsedOptions, pCommandLineOptions)){
                _tprintf(_T("\nUSAGE\n"));
                _tprintf(_T("  %s %s [VolumeLetter] [Options]\n"),argv[0],CMDSTR_SET_MIN_FREE_DATA_TO_DISK_SIZE_LIMIT);
                _tprintf(_T("\nDESCRIPTION\n"));
                _tprintf(_T("  This Command Set volume's data to minimum free disk size limit.\n"));
                _tprintf(_T("\nOPTIONS\n"));
                _tprintf(_T("  %s MinFreeDataToDiskSizeLimitInMB \n     Set the value provided as default for new volumes.\n\n"), CMDOPTSTR_SET_DEFAULT);
                _tprintf(_T("  %s MinFreeDataToDiskSizeLimitInMB \n     Set the value provided for all volumes.\n"), CMDOPTSTR_SET_FOR_ALL_VOLUMES);
                _tprintf(_T("\nNOTE\n"));
                _tprintf(_T("  If asked to configure for a specific volume and for all volumes, data to disk\n"));
                _tprintf(_T("  size isconfigured for all volumes, before configuring for the specific volume\n"));
                if(help)
                    help=false;
                else
                    return false;            
            }
            pCommandLineOptions->bOpenVolumeFilterControlDevice = true;
        } else if (0 == _tcsicmp(argv[iParsedOptions], CMDSTR_GET_LCN)) {
            if (false == ParseGetLcn(argv, argc, iParsedOptions, pCommandLineOptions)) {
                _tprintf(_T("\nUSAGE\n"));
                _tprintf(_T("  %s ValidFileName [Options]\n"), CMDSTR_GET_LCN);
                _tprintf(_T("\nDESCRIPTION\n"));
                _tprintf(_T("  This Command Get File LCN.\n"));
                _tprintf(_T("\nOPTIONS\n"));
                _tprintf(_T("  %s VCN \n  Give the start VCN. By Default it is 0.\n\n"), CMDOPTSTR_VCN);
            }
            pCommandLineOptions->bOpenVolumeFilterControlDevice = true;
        
        } else if ((0 == _tcsicmp(argv[iParsedOptions], CMDSTR_GET_DRIVE_LIST)) ||
            (0 == _tcsicmp(argv[iParsedOptions], CMDSTR_GET_DRIVE_LIST_SF))){
            if (true == IsDiskFilterRunning) {
                PEMK_DISK_SIGNATURE pDiskInfo = NULL;
                _tprintf(_T("CMDSTR_GET_DRIVE_LIST \n"));

#define MAX_DRIVES 100
                pDiskInfo = (PEMK_DISK_SIGNATURE)malloc(sizeof(EMK_DISK_SIGNATURE));
                if (NULL == pDiskInfo) {
                    return false;
                }
                else {
                    memset(pDiskInfo, 0, sizeof(EMK_DISK_SIGNATURE));
                    pDiskInfo->DriveNames = (wchar_t **)malloc(MAX_DRIVES * sizeof(wchar_t*));
                    memset(pDiskInfo->DriveNames, 0, MAX_DRIVES * sizeof(wchar_t*));

                    pDiskInfo->Signature = (unsigned long **)malloc(MAX_DRIVES * sizeof(unsigned long*));
                    memset(pDiskInfo->Signature, 0, MAX_DRIVES * sizeof(unsigned long*));
                    pDiskInfo->DriveCount = 0;
                    if ((pDiskInfo->DriveNames == NULL) || (pDiskInfo->Signature == NULL)) {
                        printf(" Mem allocation failure for pDiskInfo\n");
                        return false;
                    }
                }

                bool ret = pGetDriveDetails(pDiskInfo);

                if ((false == ret) || (NULL == pDiskInfo)){
                    _tprintf(_T("Failed to retrieve Disk Drive Details\n"));
                    return false;
                }
                else {
                    _tprintf(_T("Listing Disk Drive details\n"));
                    _tprintf(_T("Drive Count : %d\n"), pDiskInfo->DriveCount);
                    for (unsigned int i = 0; i < pDiskInfo->DriveCount; i++){
                        _tprintf(_T("\nDrive ID[%d]:%s"), i, pDiskInfo->DriveNames[i]);
                        if (NULL != pDiskInfo->Signature[i]) {
                            _tprintf(_T("\nDrive Signature[%d]:%u\n"), i, *pDiskInfo->Signature[i]);
                        }
                        else {
                            // Open device and get GPT disk GUID as WMI call does not retrieve the same
                            PDRIVE_LAYOUT_INFORMATION_EX pLayout = NULL;
                            GUID guid;
                            HANDLE handle = CreateFile(pDiskInfo->DriveNames[i], GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);

                            if (handle != INVALID_HANDLE_VALUE) {
                                if (ERROR_SUCCESS == GetDiskLayout(handle, &pLayout)) {
                                    if (pLayout->PartitionStyle == PARTITION_STYLE_GPT) {
                                        memcpy_s(&guid, sizeof(guid), &pLayout->Gpt.DiskId, sizeof(GUID));
                                        _tprintf(_T("\nDrive GUID[%d]: %08lX-%04hX-%04hX-%02hhX%02hhX-%02hhX%02hhX%02hhX%02hhX%02hhX%02hhX\n "),
                                            i, guid.Data1, guid.Data2, guid.Data3,
                                            guid.Data4[0], guid.Data4[1], guid.Data4[2], guid.Data4[3],
                                            guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7]);
                                        if (pLayout) {
                                            free(pLayout);
                                            pLayout = NULL;
                                        }
                                    }
                                }
                            }
                            else {
                                _tprintf(_T("Failed to get GPT disk GUID; GetLastError() : %d\n"), GetLastError());
                            }
                        }
                    }
                }
                if (pDiskInfo) {
                    for (unsigned int i = 0; i < pDiskInfo->DriveCount; i++)
                        free(pDiskInfo->Signature[i]);
                    free(pDiskInfo->Signature);
                    free(pDiskInfo->DriveNames);

                }
                if (help)
                    help = false;
                else
                    return false;
            }
            else {
                _tprintf(_T("\nInvalid Command for Volume Filter"));
            }
      } else if (0 == _tcsicmp(argv[iParsedOptions], CMDSTR_SET_DATA_NOTIFY_SIZE_LIMIT)) {
            if (false == ParseSetDataNotifySizeLimit(argv, argc, iParsedOptions, pCommandLineOptions)){
                _tprintf(_T("\nUSAGE\n"));
                _tprintf(_T("  %s %s [VolumeLetter] [Options]\n"),argv[0],CMDSTR_SET_DATA_NOTIFY_SIZE_LIMIT);
                _tprintf(_T("\nDESCRIPTION\n"));
                _tprintf(_T("  This Command Set volume's data notify size limit.\n"));
                _tprintf(_T("\nOPTIONS\n"));
                _tprintf(_T("  %s DataNotifySizeLimitInKB \n     Set the value provided as default for new volumes.\n\n"), CMDOPTSTR_SET_DEFAULT);
                _tprintf(_T("  %s DataNotifySizeLimitInKB \n     Set the value provided for all volumes.\n"), CMDOPTSTR_SET_FOR_ALL_VOLUMES);
                _tprintf(_T("\nNOTE\n"));
                _tprintf(_T("  If asked to configure for a specific volume and for all volumes, data notify\n"));
                _tprintf(_T("  size is configured for all volumes, before configuring for the specific volume\n"));
                if(help)
                    help=false;
                else
                    return false;            
            }
            pCommandLineOptions->bOpenVolumeFilterControlDevice = true;
        } else if ((0 == _tcsicmp(argv[iParsedOptions], CMDSTR_CONFIGURE_DRIVER)) ||
                   (0 == _tcsicmp(argv[iParsedOptions], CMDSTR_CONFIGURE_DRIVER_SF)) ) 
        {
            if (false == ParseSetDriverInfo(argv, argc, iParsedOptions, pCommandLineOptions)){
                _tprintf(_T("\nUSAGE\n"));
                _tprintf(_T("  %s %s [Option]\n"),argv[0],CMDSTR_CONFIGURE_DRIVER);
                _tprintf(_T("  %s %s [Option]\n"),argv[0],CMDSTR_CONFIGURE_DRIVER_SF);
                _tprintf(_T("\nDESCRIPTION\n"));
                _tprintf(_T("  This Command configures the driver.\n"));
                _tprintf(_T("\nOPTIONS\n"));
                _tprintf(_T("  %s \n     High water mark changes when service not started.\n\n"), CMDOPTSTR_HIGH_WATER_MARK_SERVICE_NOT_STARTED);
                _tprintf(_T("  %s \n     High water mark changes when service running.\n\n"), CMDOPTSTR_HIGH_WATER_MARK_SERVICE_RUNNING);
                _tprintf(_T("  %s \n     High water mark changes when service shutdown.\n\n"), CMDOPTSTR_HIGH_WATER_MARK_SERVICE_SHUTDOWN);
                _tprintf(_T("  %s \n     Low water mark changes when service running.\n\n"), CMDOPTSTR_LOW_WATER_MARK_SERVICE_RUNNING);
                _tprintf(_T("  %s \n     changes to purge on high water mark reach.\n\n"), CMDOPTSTR_CHANGES_TO_PURGE_ON_HIGH_WATER_MARK);
                _tprintf(_T("  %s \n     Time in seconds between memory alloc failure logs.\n\n"), CMDOPTSTR_TIME_IN_SEC_BETWEEN_MEMORY_ALLOC_FAILURES);
                _tprintf(_T("  %s \n     Max time in secs to wait in unmount of cluster volume to flush changes.\n\n"), CMDOPTSTR_MAX_WAIT_TIME_IN_SEC_ON_FAILOVER);
                _tprintf(_T("  %s \n     Max number of locked data blocks (for writes at DISPATCH_LEVEL).\n\n"), CMDOPTSTR_MAX_LOCKED_DATA_BLOCKS);
                _tprintf(_T("  %s \n     Maximum bound on non-paged pool memory driver allocates in MB.\n\n"), CMDOPTSTR_MAX_NON_PAGED_POOL_IN_MB);
                _tprintf(_T("  %s \n     Maximum Wait time for pending Irps to complete.\n\n"), CMDOPTSTR_MAX_WAIT_TIME_FOR_IRP_COMPLETION_IN_MINUTES);
                _tprintf(_T("  %s \n     Disable Default Filtering State for New Cluster Volumes.\n\n"),CMDOPTSTR_DEFAULT_FILTERING_FOR_NEW_CLUSTER_VOLUMES);
                _tprintf(_T("  %s \n     Enable Data Mode for New Volumes.\n\n"),CMDOPTSTR_DEFAULT_DATA_FILTER_FOR_NEW_VOLUMES);
                _tprintf(_T("  %s \n     Disable Default Filtering State for New Volumes.\n\n"),CMDOPTSTR_DEFAULT_FILTERING_FOR_NEW_VOLUMES);
                _tprintf(_T("  %s \n     Enable Default Data Capturer.\n\n"),CMDOPTSTR_DEFAULT_FILTERING_ENABLE_DATA_CAPTURE);
                _tprintf(_T("  %s \n     Limit on allocated paged pool for a volume below which it can switch to Data Capture mode(In Percentage)"),
                         CMDOPTSTR_MAX_DATA_ALLOCATION_LIMIT);
                _tprintf(_T("\n\n"));
                _tprintf(_T("  %s \n    Limit on total free and locked data blocks available above which a volume can switch to Data Capture Mode (InPercentage).\n\n"),   CMDOPSTR_AVAILABLE_DATA_BLOCK_COUNT);
                _tprintf(_T("  %s \n    Max Coalesced Meta-Data Change Size.\n\n"),   CMDOPSTR_MAX_COALESCED_METADATA_CHANGE_SIZE);
                _tprintf(_T("  %s \n    Validation Level (0 = No Checks; 1 = Add to Event Log; 2 or higher = Cause BugCheck/BSOD + Add to Event Log).\n\n"), CMDOPSTR_DO_VALIDATION_CHECKS);
#ifdef DBG           
                _tprintf(_T("  %s \n    Data Pool Size (In MB).\n\n"),   CMDOPSTR_DATA_POOL_SIZE);
#endif
                _tprintf(_T("  %s \n    Vol Threashold For File Write (InPercentage)\n\n"),   CMDOPSTR_THRESHOLD_FOR_FILE_WRITE);
                _tprintf(_T("  %s \n    Free Threashold For File Write (InPercentage)\n\n"),   CMDOPSTR_FREE_THRESHOLD_FOR_FILE_WRITE);		
                _tprintf(_T("  %s \n    Max Data Pool Size Per Volume (In KB)\n\n"),   CMDOPSTR_MAX_DATA_POOL_SIZE_PER_VOLUME);
                _tprintf(_T("  %s \n    Max Data Size Per Non Data Mode Dirty Block In MB.\n\n"), CMDOPSTR_MAX_DATA_PER_NON_DATA_MODE_DIRTY_BLOCK);
                _tprintf(_T("  %s \n    Max Data Size Per Data Mode Dirty Block In MB.\n\n"), CMDOPSTR_MAX_DATA_PER_DATA_MODE_DIRTY_BLOCK);
                _tprintf(_T("  %s \n    Data Log Directory for New Volumes.\n\n"), CMDOPSTR_DATA_LOG_DIRECTORY);
                _tprintf(_T("  %s \n    Data Log Directory for All Volumes(Existing and New).\n\n"), CMDOPSTR_DATA_LOG_DIRECTORY_ALL);
                if(help)
                    help=false;
                else
                    return false;            
            }
            pCommandLineOptions->bOpenVolumeFilterControlDevice = true;
        } else if ((0 == _tcsicmp(argv[iParsedOptions], CMDSTR_SET_IO_SIZE_ARRAY)) ||
                   (0 == _tcsicmp(argv[iParsedOptions], CMDSTR_SET_IO_SIZE_ARRAY_SF)) ) 
        {
            if (false == ParseSetIoSizeArray(argv, argc, iParsedOptions, pCommandLineOptions)){
                _tprintf(_T("\nUSAGE\n"));
                _tprintf(_T("  %s %s VolumeLetter[s] [iosize1] [iosize2] ... [iosize11]\n"),argv[0],CMDSTR_SET_IO_SIZE_ARRAY);
                _tprintf(_T("  %s %s VolumeLetter[s] [iosize1] [iosize2] ... [iosize11]\n"),argv[0],CMDSTR_SET_IO_SIZE_ARRAY_SF);
                _tprintf(_T("\nDESCRIPTION\n"));
                _tprintf(_T("  This Command Sets io sizes for volume[s] \n"));
                _tprintf(_T("\nOPTIONS\n"));
                _tprintf(_T("  %s\n     Profiles the Read IO.\n\n"), CMDSTR_SET_READ_IO_PROFILING);
                _tprintf(_T("  %s\n     Profiles the write IO.\n\n"), CMDSTR_SET_WRITE_IO_PROFILING);
                if(help)
                    help=false;
                else
                    return false;            
            }
            pCommandLineOptions->bOpenVolumeFilterControlDevice = true;
#ifdef _CUSTOM_COMMAND_CODE_            
        } else if (0 == _tcsicmp(argv[iParsedOptions], CMDSTR_CUSTOM_COMMAND)) {
            if (false == ParseCustomCommand(argv, argc, iParsedOptions, pCommandLineOptions)){
                _tprintf(_T("\nUSAGE\n"));
                _tprintf(_T("  %s %s Option[s]\n"),argv[0],CMDSTR_CUSTOM_COMMAND);
                _tprintf(_T("\nDESCRIPTION\n"));
                _tprintf(_T("  This Command sends the custom command to driver\n"));
                _tprintf(_T("\nOPTIONS\n"));
                _tprintf(_T("  %s parameter1\n     Specify parameter 1.\n\n"), CMDOPTSTR_PARAM1);
                _tprintf(_T("  %s parameter2\n     Specify parameter 2.\n\n"), CMDOPTSTR_PARAM2);
                if(help)
                    help=false;
                else
                    return false;            
            }
            pCommandLineOptions->bOpenVolumeFilterControlDevice = true;
#endif // _CUSTOM_COMMAND_CODE_            
        }
        else if (0 == _tcsicmp(argv[iParsedOptions], CMDSTR_UPDATE_OSVER)) {
            PCOMMAND_STRUCT pCommand = AllocateCommandStruct();
            if (NULL == pCommand) {
                _tprintf(_T("\n%S: Failed to allocate memory for command struct.\n"), __TFUNCTION__);
                return false;
            }
            pCommand->ulCommand = CMD_UPDATE_OSVERSION_DISKS;
            InsertTailList(pCommandLineOptions->CommandList, &pCommand->ListEntry);
            iParsedOptions++;
        }
        else if (0 == _tcsicmp(argv[iParsedOptions], CMDSTR_ENUMERATE_DISKS)) {
            PCOMMAND_STRUCT pCommand = AllocateCommandStruct();
            if (NULL == pCommand) {
                _tprintf(_T("\n%S: Failed to allocate memory for command struct.\n"), __TFUNCTION__);
                return false;
            }
            pCommandLineOptions->bOpenVolumeFilterControlDevice = true;
            pCommand->ulCommand = CMD_ENUMERATE_DISKS;
            InsertTailList(pCommandLineOptions->CommandList, &pCommand->ListEntry);
            iParsedOptions++;
        }
        else if( (0 == _tcsicmp(argv[iParsedOptions], CMDSTR_HELP)) ||
            (0 == _tcsicmp(argv[iParsedOptions], CMDSTR_H))) {
            UsageMain(argv[0]);
            return false;
        }
        else {
            _tprintf(_T("\nUnrecognized option %s\n\n"), argv[iParsedOptions]);
            UsageMain(argv[0]);
            return false;
        }
    }

    return true;
}

DWORD WINAPI ExecuteCommands(LPVOID pContext)
{
    HANDLE  hVFCtrlDevice = INVALID_HANDLE_VALUE;
    HANDLE  hVVCtrlDevice = INVALID_HANDLE_VALUE;
    HANDLE  hShutdownNotification = INVALID_HANDLE_VALUE;
    HANDLE  hServiceStartNotification = INVALID_HANDLE_VALUE;
    PLIST_ENTRY ListHead = (PLIST_ENTRY) pContext;
    DWORD dwRet = CMD_STATUS_SUCCESS;
    const _TCHAR      *deviceName;

    if(ListHead == NULL)
        return CMD_STATUS_SUCCESS;


    deviceName = (IsDiskFilterRunning) ? INMAGE_DISK_FILTER_DOS_DEVICE_NAME : INMAGE_FILTER_DOS_DEVICE_NAME;

     //Try to open both Device names, if you find both of them success fail any IOCTL
     //Set some global flags while one of them is being opened
    if (sCommandOptions.bOpenVolumeFilterControlDevice) {
        if (sCommandOptions.bVerbose) {
            _tprintf(_T("Opening File %s\n"), deviceName);
        }

        hVFCtrlDevice = CreateFile(
            deviceName,
            GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            NULL,
            OPEN_EXISTING,
            FILE_FLAG_OVERLAPPED,
            NULL
            );

        if (INVALID_HANDLE_VALUE == hVFCtrlDevice) {
            _tprintf(_T("CreateFile %s Failed with Error = %#x\n"), deviceName, GetLastError());
            return CMD_STATUS_UNSUCCESSFUL;
        }
    }

    if (sCommandOptions.bOpenVirtualVolumeControlDevice) {
        _tprintf(_T("Opening File %s\n"), VV_CONTROL_DOS_DEVICE_NAME);
        hVVCtrlDevice = OpenVirtualVolumeControlDevice();
        if( INVALID_HANDLE_VALUE == hVVCtrlDevice ) {
            _tprintf(_T("CreateFile %s Failed with Error = %#x\n"), VV_CONTROL_DOS_DEVICE_NAME, GetLastError());
            
            INSTALL_INVIRVOL_DATA InstallData;
            InstallData.DriverName = INVIRVOL_SERVICE;
            TCHAR SystemDir[128];
            GetSystemDirectory(SystemDir, 128);
            InstallData.PathAndFileName = (PTCHAR)malloc(128);
            _tcscpy_s(InstallData.PathAndFileName,128, SystemDir);
            _tcscat_s(InstallData.PathAndFileName,128,_T("\\drivers\\invirvol.sys"));
            DRSTATUS Status = InstallInVirVolDriver(InstallData);
            
            if(!DR_SUCCESS(Status))
            {
                _tprintf(_T("Service install Failed with Error:%#x"), GetLastError());
                return CMD_STATUS_UNSUCCESSFUL;
            }

            START_INVIRVOL_DATA StartData;
            StartData.DriverName = INVIRVOL_SERVICE;

            Status = StartInVirVolDriver(StartData);
            if(!DR_SUCCESS(Status))
            {
                _tprintf(_T("Start Driver Failed with Error:%#x"), GetLastError());
                return CMD_STATUS_UNSUCCESSFUL;
            }

            hVVCtrlDevice = OpenVirtualVolumeControlDevice();
            
            if(INVALID_HANDLE_VALUE == hVVCtrlDevice ) {
                _tprintf(_T("Retry to open Control device Failed with Error = %#x\n"), GetLastError());
                return CMD_STATUS_UNSUCCESSFUL;
            }
        }
    }

    while (false == IsListEmpty(ListHead)) {
        
        PCOMMAND_STRUCT pCommand = (PCOMMAND_STRUCT)RemoveHeadList(ListHead);

        switch(pCommand->ulCommand) {
        case CMD_GET_DRIVER_VERSION:
            GetDriverVersion(hVFCtrlDevice);
            break;
        case CMD_SHUTDOWN_NOTIFY:
            hShutdownNotification = SendShutdownNotification(hVFCtrlDevice);
            if (INVALID_HANDLE_VALUE == hShutdownNotification){
                FreeCommandStruct(pCommand);
                FreeList(ListHead, COMMAND_STRUCT, FreeCommandStruct);
                goto cleanup;
            }

            if (true == HasShutdownNotificationCompleted(hShutdownNotification)) {
                _tprintf(_T("Overlapped IO has completed\n"));
                FreeCommandStruct(pCommand);
                FreeList(ListHead, COMMAND_STRUCT, FreeCommandStruct);
                goto cleanup;
            }
            break;
        case CMD_CHECK_SHUTDOWN_STATUS:
            if (HasShutdownNotificationCompleted(hShutdownNotification)) {
                _tprintf(_T("Shutdown Notification has completed\n"));
            } else {
                _tprintf(_T("Shutdown Notification has not completed\n"));
            }
            break;
        case CMD_CHECK_SERVICE_START_STATUS:
            if (HasServiceStartNotificationCompleted(hServiceStartNotification)) {
                _tprintf(_T("Service start notification has completed\n"));
            } else {
                _tprintf(_T("Service start notification has not completed\n"));
            }
            break;
        case CMD_SET_DATA_FILE_THREAD_PRIORITY:
            SetDataFileThreadPriority(hVFCtrlDevice, &pCommand->data.SetDataFileThreadPriority);
            break;
        case CMD_SET_WORKER_THREAD_PRIORITY:
            SetWorkerThreadPriority(hVFCtrlDevice, &pCommand->data.SetWorkerThreadPriority);
            break;
        case CMD_SET_DATA_TO_DISK_SIZE_LIMIT:
            SetDataToDiskSizeLimit(hVFCtrlDevice, &pCommand->data.SetDataToDiskSizeLimit);
            break;
        case CMD_SET_MIN_FREE_DATA_TO_DISK_SIZE_LIMIT:
            SetMinFreeDataToDiskSizeLimit(hVFCtrlDevice, &pCommand->data.SetMinFreeDataToDiskSizeLimit);
            break;
        case CMD_SET_DATA_NOTIFY_SIZE_LIMIT:
            SetDataNotifySizeLimit(hVFCtrlDevice, &pCommand->data.SetDataNotifySizeLimit);
            break;
        case CMD_SERVICE_START_NOTIFY:
            hServiceStartNotification = SendServiceStartNotification(hVFCtrlDevice, &pCommand->data.ServiceStart);
            if (INVALID_HANDLE_VALUE == hServiceStartNotification) {
                _tprintf(_T("ExecuteCommand: Aborting processing of commands\n"));
                goto cleanup;
            }
            break;
        case CMD_WAIT_FOR_KEYWORD:
            WaitForKeyword(&pCommand->data.KeywordData);
            break;
        case CMD_PRINT_STATISTICS:
            GetDriverStats(hVFCtrlDevice, &pCommand->data.PrintStatisticsData);
            break;
        case CMD_GET_ADDITIONAL_STATS_INFO:
			if (false == IsDiskFilterRunning) {
				GetAdditionalStats(hVFCtrlDevice, &pCommand->data.AdditionalStatsInfo);
			}
			else {
				GetAdditionalStats_v2(hVFCtrlDevice, &pCommand->data.AdditionalStatsInfo);			
			}
            break;
        case CMD_GET_GLOBAL_TSSN:
            GetGlobalTSSN(hVFCtrlDevice);
            break;
        case CMD_GET_WRITE_ORDER_STATE:
            GetWriteOrderState(hVFCtrlDevice, &pCommand->data.GetFilteringState);
            break;
        case CMD_GET_VOLUME_FLAGS:
            GetVolumeFlags(hVFCtrlDevice, &pCommand->data.GetVolumeFlags);
            break;
        case CMD_GET_VOLUME_SIZE:
			if (false == IsDiskFilterRunning)
				GetVolumeSize(hVFCtrlDevice, &pCommand->data.GetVolumeSize);
			else 
				GetVolumeSize_v3(hVFCtrlDevice, &pCommand->data.GetVolumeSize_v2);

            break;
        case CMD_GET_DM:
            if (sCommandOptions.bCompact) {
                GetDataModeCompact(hVFCtrlDevice, &pCommand->data.DataModeData);
            } else
                GetDataMode(hVFCtrlDevice, &pCommand->data.DataModeData);
            break;
        case CMD_CLEAR_STATSTICS:
            ClearVolumeStats(hVFCtrlDevice, &pCommand->data.ClearStatisticsData);
            break;
        case CMD_CLEAR_DIFFS:
            ClearDiffs(hVFCtrlDevice, &pCommand->data.ClearDiffs);
            break;
        case CMD_START_FILTERING:
			if (false == IsDiskFilterRunning) {
				StartFiltering(hVFCtrlDevice, &pCommand->data.StartFiltering);
			}
			else {
				StartFiltering_v2(hVFCtrlDevice, &pCommand->data.StartFiltering);
			}
            break;
        case CMD_SET_DISK_CLUSTERED:
            SetDiskClustered(hVFCtrlDevice, &pCommand->data.SetDiskClustered);
            break;

        case CMD_STOP_FILTERING:
            {
                ULONG ulRet = StopFiltering(hVFCtrlDevice, &pCommand->data.StopFiltering);
                if (ERROR_SUCCESS != ulRet) {
                    dwRet = CMD_STATUS_UNSUCCESSFUL;
                }
            }
            break;
        case CMD_RESYNC_START_NOTIFY:
            ResyncStartNotify(hVFCtrlDevice, &pCommand->data.ResyncStart);
            break;
        case CMD_RESYNC_END_NOTIFY:
            ResyncEndNotify(hVFCtrlDevice, &pCommand->data.ResyncEnd);
            break;
        case CMD_WAIT_TIME:
            Sleep(pCommand->data.TimeOut.ulTimeout);
            break;
        case CMD_GET_DB_TRANS:
            {
                BOOL bIsWow64 = FALSE;
                if(RunningOn64bitVersion(bIsWow64)){
                    if(bIsWow64){
                        _tprintf(_T("DrvUtil is running on 64 bit Machine\n"));
                        GetDBTransV2(hVFCtrlDevice, &pCommand->data.GetDBTrans);
                    }else{
                        GetDBTrans(hVFCtrlDevice, &pCommand->data.GetDBTrans);
                    }
                }else{
                    _tprintf(_T("Execute getdbtrans Failed as RunningOn64bitVersion() failed\n"));
                }
            }
            break;
        case CMD_COMMIT_DB_TRANS:
            CommitDBTrans(hVFCtrlDevice, &pCommand->data.CommitDBTran);
            break;
        case CMD_SET_RESYNC_REQUIRED:
            SetResyncRequired(hVFCtrlDevice, &pCommand->data.SetResyncRequired);
            break;
        case CMD_SET_VOLUME_FLAGS:
            SetVolumeFlags(hVFCtrlDevice, &pCommand->data.VolumeFlagsData);
            break;
        case CMD_SET_DRIVER_FLAGS:
            SetDriverFlags(hVFCtrlDevice, &pCommand->data.DriverFlagsData);
            break;
        case CMD_SET_DRIVER_CONFIG:
            SetDriverConfig(hVFCtrlDevice, &pCommand->data.SetDriverConfig);
            break;
#ifdef _CUSTOM_COMMAND_CODE_            
        case CMD_CUSTOM_COMMAND:
            SendCustomCommand(hVFCtrlDevice, &pCommand->data.CustomCommandData);
            break;
#endif // _CUSTOM_COMMAND_CODE_            
        case CMD_SET_DEBUG_INFO:
            SetDebugInfo(hVFCtrlDevice, &pCommand->data.DebugInfoData);
            break;
        case CMD_TAG_VOLUME:
			if (false == IsDiskFilterRunning)
			    TagVolume(hVFCtrlDevice, &pCommand->data.TagVolumeInputData);
			else
				TagDisk(hVFCtrlDevice, &pCommand->data.TagVolumeInputData);
            break;
        case CMD_SYNCHRONOUS_TAG_VOLUME:
            TagSynchronousToVolume(hVFCtrlDevice, &pCommand->data.TagVolumeInputData);
            break;
        case CMD_MOUNT_VIRTUAL_VOLUME:
            //MountVirtualVolumeFromRetentionLog(hVVCtrlDevice, &pCommand->data.MountVirtualVolumeData);
            MountVirtualVolume(hVVCtrlDevice, &pCommand->data.MountVirtualVolumeData);
            break;
        case CMD_UNMOUNT_VIRTUAL_VOLUME:
            UnmountVirtualVolume(hVVCtrlDevice, &pCommand->data.UnmountVirtualVolumeData);
            break;
        case CMD_STOP_VIRTUAL_VOLUME:
            StopVirtualVolumeDriver(hVVCtrlDevice, &pCommand->data.StopVirtualVolumeData);
            break;
        case CMD_CONFIGURE_VIRTUAL_VOLUME:
            ConfigureVirtualVolumeDriver(hVVCtrlDevice, &pCommand->data.ConfigureVirtualVolumeData);
            break;
            
        case CMD_START_VIRTUAL_VOLUME:
            StartVirtualVolumeDriver(hVVCtrlDevice, &pCommand->data.StartVirtualVolumeData);
            break;
        case CMD_CREATE_THREAD:
            CreateCmdThread(pCommand, ListHead);
            break;
        case CMD_END_OF_THREAD:
            //Do Nothing
            break;
        case CMD_WAIT_FOR_THREAD:
            WaitForThread(pCommand);
            break;
        case CMD_WRITE_STREAM:
            WriteStream(pCommand);
            break;
        case CMD_READ_STREAM:
            ReadStream(hVFCtrlDevice, &pCommand->data.ReadStreamData);
            break;
        case CMD_SET_VV_CONTROL_FLAGS:
            SetControlFlags(hVVCtrlDevice, &pCommand->data.ControlFlagsInput);
            break;
        case CMD_SET_IO_SIZE_ARRAY:
            SetIoSizeArray(hVFCtrlDevice, &pCommand->data.SetIoSizes);
            break;
        case CMD_UPDATE_OSVERSION_DISKS:
            UpdateOSVersionForDisks();
            break;
        case CMD_ENUMERATE_DISKS:
            dwRet = EnumerateDisks(hVFCtrlDevice);
            break;
        case CMD_GET_SYNC:
        {
			if (true == IsDiskFilterRunning) {
				GetSync_v2(hVFCtrlDevice, &pCommand->data.GetDBTrans);
			} else {
				_tprintf(_T("Invalid Command for volume filter"));
			}

        }
            break;
        case CMD_GET_LCN:
        {
            GetLcn(hVFCtrlDevice, &pCommand->data.GetLcn);
        }
            break;
        default:
            break;
        }

        FreeCommandStruct(pCommand);
    }
cleanup:
    if (INVALID_HANDLE_VALUE != hVFCtrlDevice)
        CloseHandle(hVFCtrlDevice);
    
    if (INVALID_HANDLE_VALUE != hVVCtrlDevice)
        CloseHandle(hVVCtrlDevice);

    if (INVALID_HANDLE_VALUE != hShutdownNotification)
        CloseShutdownNotification(hShutdownNotification);

    if (INVALID_HANDLE_VALUE != hServiceStartNotification) {
        CloseServiceStartNotification(hServiceStartNotification);
    }


    delete (PLIST_ENTRY)pContext;

    _tprintf(__T("Exit: %s return code: %d\n"), __TFUNCTION__, dwRet);
    return dwRet;
}


int
_tmain(int argc, TCHAR *argv[])
{
    init_inm_safe_c_apis();
    int ret = 1;
    DWORD dwRet = CMD_STATUS_UNSUCCESSFUL;
#define STRING_BUFFER_SIZE_IN_CHARS 0x50
    TCHAR   StringBuffer[STRING_BUFFER_SIZE_IN_CHARS];
    SYSTEMTIME  LocalTime;

    // Printing time and date information.
    GetLocalTime(&LocalTime);
    if (GetDateFormat(LOCALE_USER_DEFAULT, DATE_LONGDATE, &LocalTime, NULL, StringBuffer, STRING_BUFFER_SIZE_IN_CHARS))
        _tprintf(_T("%s"), StringBuffer);
    if (GetTimeFormat(LOCALE_USER_DEFAULT, 0, &LocalTime, NULL, StringBuffer, STRING_BUFFER_SIZE_IN_CHARS))
        _tprintf(_T("  %s\n"), StringBuffer);

    if(argc<2){
        UsageMain(argv[0]);
        return ret;
    }

    HANDLE hVFCtrlDevice = NULL;
    
    hVFCtrlDevice = CreateFile(
        INMAGE_FILTER_DOS_DEVICE_NAME,
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_EXISTING,
        FILE_FLAG_OVERLAPPED,
        NULL
        );

    if (INVALID_HANDLE_VALUE == hVFCtrlDevice) { 
        IsVolFilterRunning = false;
    }
    else {
        _tprintf(_T("Attempting to open Volume Control Device is successful \n"));
        IsVolFilterRunning = true;

    }

    hVFCtrlDevice = CreateFile(
        INMAGE_DISK_FILTER_DOS_DEVICE_NAME,
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_EXISTING,
        FILE_FLAG_OVERLAPPED,
        NULL
        );

    if (INVALID_HANDLE_VALUE == hVFCtrlDevice) {
        _tprintf(_T("Try to open Disk filter control device %s Failed with Error = %#x\n"), INMAGE_DISK_FILTER_DOS_DEVICE_NAME, GetLastError());
        return CMD_STATUS_UNSUCCESSFUL;
    }
    else {
        IsDiskFilterRunning = true;
    }

    if ((true == IsVolFilterRunning) && (true == IsDiskFilterRunning)) {
        _tprintf(_T("UNEXPECTED ; Both disk and volume Filters are running, this is unsupported scenario"));
        return ret;
    }
    if (true == IsDiskFilterRunning) {
        if (!HandleToDiskGenericDll){
            _tprintf(_T("Error : Drvutil requires diskgeneric.dll to list disk details\n "));
            return ret;
        }
    }
    InitializeListHead(&PendingTranListHead);
    InitializeCommandLineOptionsWithDefaults(&sCommandOptions);
    if (false == ParseCommandLineOptions(argc, argv, &sCommandOptions)) {
        return ret;
    }

    if (!IsListEmpty(sCommandOptions.CommandList))
    dwRet = ExecuteCommands(sCommandOptions.CommandList);

    if (CMD_STATUS_SUCCESS == dwRet) {
        ret = 0;
    }

    // Cleanup wait event vector.
    for (vector<CDBWaitEvent *>::iterator iter = DBWaitEventVector.begin(); iter != DBWaitEventVector.end(); ++iter) {
        (*iter)->CloseHandle();
        delete *iter;
        *iter = NULL;
    }
    _tprintf(__T("Exit: %s\n"), __TFUNCTION__);
    return ret;
}

/*------------------------ COMMAND_STRUCT ----------------------*/
PCOMMAND_STRUCT
AllocateCommandStruct()
{
    PCOMMAND_STRUCT pCommandStruct;

    pCommandStruct = (PCOMMAND_STRUCT)malloc(sizeof(COMMAND_STRUCT));
    if (NULL != pCommandStruct) {
        memset(pCommandStruct, 0, sizeof(COMMAND_STRUCT));
    }

    return pCommandStruct;
}

void
FreeCommandStruct(PCOMMAND_STRUCT pCommandStruct)
{
    free(pCommandStruct);
}


bool
MountPointToGUID(PTCHAR VolumeMountPoint, WCHAR *VolumeGUID, ULONG ulBufLen)
{       
    PWCHAR   MountPoint = NULL ;
    WCHAR   UniqueVolumeName[60];

    if(IS_MOUNT_POINT(VolumeMountPoint))
    {
        ULONG   MountPointLength = (ULONG)_tcslen(VolumeMountPoint) + 1 ;//+ sizeof(WCHAR);
        if(VolumeMountPoint[_tcslen(VolumeMountPoint) -1] != _T('\\'))
            ++MountPointLength;
        MountPoint  = (PWCHAR)malloc(MountPointLength*sizeof(WCHAR));
        tcstowcs(MountPoint, MountPointLength*sizeof(WCHAR), VolumeMountPoint);
        MountPoint[MountPointLength - 2] = _T('\\');
        MountPoint[MountPointLength - 1] = _T('\0');        
    }
    else
    {
        MountPoint = (PWCHAR)malloc(4*sizeof(WCHAR));
        MountPoint[0] = (WCHAR)VolumeMountPoint[0];
        MountPoint[1] = L':';
        MountPoint[2] = L'\\';
        MountPoint[3] = L'\0';
    }
    if (ulBufLen  < (GUID_SIZE_IN_CHARS * sizeof(WCHAR)))
        return false;

    if (0 == GetVolumeNameForVolumeMountPointW((WCHAR *)MountPoint, (WCHAR *)UniqueVolumeName, 60)) {
        _tprintf(_T("MountPointToGUID failed\n"));
        return false;
    }

	inm_memcpy_s(VolumeGUID, (GUID_SIZE_IN_CHARS * sizeof(WCHAR)), &UniqueVolumeName[11], (GUID_SIZE_IN_CHARS * sizeof(WCHAR)));
    if (sCommandOptions.bVerbose) {
        _tprintf(_T("%s Mount Point is mapped to Unique volume %s\n"), VolumeMountPoint, UniqueVolumeName); 
    }
        
    free(MountPoint);
    return true;
}

size_t tcstowcs(PWCHAR wstring, size_t szMaxLength, PTCHAR tstring)
{
    size_t size = 0;
#ifdef UNICODE
    wcscpy_s(wstring, szMaxLength, tstring);
    size = wcslen(wstring) * sizeof(WCHAR);
#else
    size = mbstowcs(wstring, tstring, _tcslen(tstring));
#endif
    return size;
}

size_t tcstombs(PCHAR string, size_t szMaxLength, PTCHAR tstring)
{
    size_t size = 0;
#ifdef UNICODE
    size = wcstombs(string, tstring, _tcslen(tstring));
#else
    strcpy_s(string, szMaxLength, tstring);
    size = strlen(string);
#endif
    return size;
}

size_t mbstotcs(PTCHAR tstring, size_t szMaxLength, PCHAR string)
{
    size_t size = 0;
#ifdef UNICODE
    size = mbstowcs(tstring, string, strlen(string));
#else
    strcpy_s(tstring, szMaxLength, string);
    size = strlen(string);
#endif
    return size;
}
size_t wcstotcs(PTCHAR tstring, size_t szMaxLength, PWCHAR string)
{
    size_t size = 0;
#ifdef UNICODE
    wcscpy_s(tstring, szMaxLength, string);
    size = wcslen(string);
#else
    size = wcstombs(tstring, string, wcslen(string) + 1);   
#endif
    return size;
}

BOOL 
RunningOn64bitVersion(BOOL &bIsWow64)
{
    typedef BOOL (WINAPI *LPFN_ISWOW64PROCESS) (HANDLE, PBOOL);
    LPFN_ISWOW64PROCESS fnIsWow64Process = (LPFN_ISWOW64PROCESS)GetProcAddress(GetModuleHandle(L"kernel32"),"IsWow64Process");
    if (NULL != fnIsWow64Process){
        if(fnIsWow64Process(GetCurrentProcess(),&bIsWow64)){
            return TRUE;
        }else{
            _tprintf(_T("RunningOn64bitVersion failed as IsWow64Process failed with error : %d\n"), GetLastError());
            return TRUE;
        }
    }else{
        _tprintf(_T("RunningOn64bitVersion failed as GetProcAddress failed\n"));
        return FALSE;
    }

}

BOOL
IsGUID(TCHAR* GUID)
{
    if ((_tcslen(GUID) != GUID_SIZE_IN_CHARS)) {
        return false;
    } 
    for(int indx = 0; indx < GUID_SIZE_IN_CHARS; indx++) {
        if(!( (GUID[indx] >= _T('a') && GUID[indx] <= _T('f')) || (GUID[indx] >= _T('A') && GUID[indx] <= _T('F')) || 
            ( GUID[indx] >= _T('0') && GUID[indx] <= _T('9')) )) {
            if (!(IS_HYPHEN(GUID, indx))) {
                _tprintf(_T("IsGUID: GUID %s format is Incorrect\n"),GUID);
                return false;
            }
        }
    }
    return true;
}
/*
Description : This functions assumes GUID is valid as it received from the user and just copies to input buffer
*/


BOOL
GetVolumeGUID(PTCHAR DiskSigGUID, WCHAR *GUID, ULONG BufLen, char* FuncStr, wchar_t* CmdStr, wchar_t* drivename )
{
    if (true == IsDiskFilterRunning) {
        if (NULL != DiskSigGUID) {
            //_tprintf(_T("size of sig/guid : %d \n"), (wcslen(DiskSigGUID) * sizeof(WCHAR)));
            // Just copy GUID or signature directly
            StringCchPrintf(GUID, BufLen, L"%s", DiskSigGUID);

            if (sCommandOptions.bVerbose) {
                _tprintf(_T("GetVolumeGUID: Source Disk Name %s \n"), GUID);
                printf(" %s \n", FuncStr);
            }

            return true;
        }
        else {
            _tprintf(_T("GetVolumeGUID: ALERT : Found NULL, %s : % \n"), GUID, FuncStr);
            return true;
        }
        return true;
    }
	else if (false == IsDiskFilterRunning)
	{
		if (IsGUID(DiskSigGUID)) {
			inm_memcpy_s(GUID, BufLen, DiskSigGUID, BufLen);
		}
		else {
			if (!((IS_DRIVE_LETTER(DiskSigGUID[0]) && IS_DRIVE_LETTER_PAR(DiskSigGUID))
				|| IS_MOUNT_POINT(DiskSigGUID))) {
				_tprintf(_T("%hs: Expecting MountPoint instead found Invalid parameter %s for Command %s\n"),
					FuncStr, DiskSigGUID, CmdStr);
				return false;
			}
			if (false == MountPointToGUID(DiskSigGUID, GUID, BufLen)) {
				_tprintf(_T("%s: MountPointToGUID failed to convert MountPoint(%s) to GUID"), FuncStr, DiskSigGUID);
				return false;
			}
		}
	}
    return true;
}
//DBF -END