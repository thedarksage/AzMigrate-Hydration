#include <windows.h>
#include <stdio.h>
#include <tchar.h>
#include <malloc.h>
#include "ListEntry.h"
#include "involflt.h"
#include "Sdownapp.h"

TCHAR * ServiceStringArray[] = 
{
    _T("Service State Unknown"),
    _T("Service Not Started"),
    _T("Service Running"),
    _T("Service Shutdown")
};

TCHAR * BitMapStateStringArray[] = 
{
	_T("ecVBitmapStateUnInitialized"),
	_T("ecVBitmapStateOpened"),
	_T("ecVBitmapStateReadStarted"),
	_T("ecVBitmapStateReadPaused"),
	_T("ecVBitmapStateReadCompleted"),
	_T("ecVBitmapStateAddingChanges"),
	_T("ecVBitmapStateClosed"),
	_T("ecVBitmapStateReadError"),
	_T("ecVBitmapStateInternalError"),
};

TCHAR * DataSourceStringArray[] =
{
    _T("DataSourceUnknown"),
    _T("DataSourceBitmap"),
    _T("DataSourceMetaData"),
    _T("DataSourceData")
};

typedef struct _COMMAND_LINE_OPTIONS {
    bool bSendShutdownNotification;
    ULONGLONG   ullLastTransactionID;
    LIST_ENTRY  CommandList;
} COMMAND_LINE_OPTIONS, *PCOMMAND_LINE_OPTIONS;

#define IS_DRIVE_LETTER(x)  ( ((x >= _T('a')) && (x <= _T('z'))) || ((x >= _T('A')) && (x <= _T('Z'))) )
#define IS_SUB_OPTION(x)    ( (_tcslen(x) >= 2) && (_T('-') == (x)[0]) && (_T('-') != (x)[1]) )
#define IS_OPTION(x)        ( (_tcslen(x) >= 1) && (_T('-') == (x)[0]) )
#define IS_DRIVE_LETTER_PAR(x)  ( ((_tcslen(x) == 1) || (_tcslen(x) == 2)) && (_T('-') != (x)[0]))
#define IS_COMMAND(x)       ( (_tcslen(x) > 1) ? ((_T('-') == (x)[0]) && (_T('-') == (x)[1])) : (_T('-') == (x)[0]))
#define IS_COMMAND_PAR(x)   (_T('-') != (x)[0])

COMMAND_LINE_OPTIONS    sCommandOptions;
LIST_ENTRY              PendingTranListHead;

PCOMMAND_STRUCT
AllocateCommandStruct();

void
FreeCommandStruct(PCOMMAND_STRUCT pCommandStruct);

bool
DriveLetterToGUID(TCHAR DriveLetter, WCHAR *VolumeGUID, ULONG ulBufLen);

void
Usage(TCHAR *pcExeName)
{
    _tprintf(_T("Usage: %s %s [Commands]\n"), pcExeName, CMDOPT_SHUTDOWN_NOTIFY);
    _tprintf(_T("Commands are - \n"));
    _tprintf(_T("    %s KeyWord - This command waits for user to enter the keyword on separate line\n"), CMDOPT_WAIT_FOR_KEYWORD);
    _tprintf(_T("    %s - This command is valid only if %s options is specified\n"), CMDOPT_CHECK_SHUTDOWN_STATUS, CMDOPT_SHUTDOWN_NOTIFY);
    _tprintf(_T("    %s [VolumeLetter] - Prints statistics\n"), CMDOPT_PRINT_STATISTICS);
    _tprintf(_T("                        If VolumeLetter is not specified statistics of all volumes are displayed.\n"));
    _tprintf(_T("    %s [VolumeLetter] - Clear statistics.\n"), CMDOPT_CLEAR_STATISTICS);
    _tprintf(_T("                        If VolumeLetter is not specified statistics of all volumes are cleared.\n"));
    _tprintf(_T("    %s VolumeLetter - Clear diffs of a volume represting drive letter\n"), CMDOPT_CLEAR_DIFFS);
    _tprintf(_T("    %s VolumeLetter - Stop filtering volume represting drive letter\n"), CMDOPT_STOP_FILTERING);
    _tprintf(_T("    %s VolumeLetter - Start filterin volume represting drive letter\n"), CMDOPT_START_FILTERING);
    _tprintf(_T("    %s MilliSeconds - Time to wait before executing next command option\n"), CMDOPT_WAIT_TIME);
    _tprintf(_T("    %s VolumeLetter - Get Dirty blocks of a volume represting drive letter\n"), CMDOPT_GET_DB_TRANS);
    _tprintf(_T("        %s - Commit previous\n"), CMDSUBOPT_COMMIT_PREV);
    _tprintf(_T("        %s - Commit current\n"), CMDSUBOPT_COMMIT_CURRENT);
    _tprintf(_T("        %s MilliSecons - Commit current with timeout\n"), CMDSUBOPT_COMMIT_TIMEOUT);
    _tprintf(_T("        %s - Print dirtyblock details\n"), CMDSUBOPT_PRINT_DETAILS); 
    _tprintf(_T("    %s VolumeLetter - Commit Dirty blocks of a volume represting drive letter\n"), CMDOPT_COMMIT_DB_TRANS);
    _tprintf(_T("    %s VolumeLetter - Set flags of a volume represting drive letter\n"), CMDOPT_SET_VOLUME_FLAGS);
    _tprintf(_T("        %s - Reset flags instead of setting\n"), CMDSUBOPT_RESET_FLAGS);
    _tprintf(_T("        %s - Ignore page file writes\n"), CMDSUBOPT_IGNORE_PAGEFILE_WRITES);
    _tprintf(_T("        %s - Disable bitmap read\n"), CMDSUBOPT_DISABLE_BITMAP_READ);
    _tprintf(_T("        %s - Disable bitamp write\n"), CMDSUBOPT_DISABLE_BITMAP_WRITE);
    _tprintf(_T("        %s - Mark the volume read only\n"), CMDSUBOPT_MARK_READ_ONLY);
    _tprintf(_T("    %s - Sets Debugging flags\n"), CMDOPT_SET_DEBUG_INFO);
    _tprintf(_T("        %s - Reset flags instead of setting\n"), CMDSUBOPT_RESET_FLAGS);
    _tprintf(_T("        %s - Set debugging level for all\n"), CMDSUBOPT_DEBUG_LEVEL_FORALL);
    _tprintf(_T("        %s - Set debugging level for debug modules\n"), CMDSUBOPT_DEBUG_LEVEL);
    _tprintf(_T("        %s DebugModules- Specify debugging modules\n"), CMDSUBOPT_DEBUG_MODULES);
    _tprintf(_T("        DebugModules are %s %s %s %s\n"), DM_STR_DRIVER_INIT, DM_STR_DRIVER_UNLOAD, DM_STR_DRIVER_PNP, DM_STR_PAGEFILE);
    _tprintf(_T("                         %s %s %s %s\n"), DM_STR_VOLUME_ARRIVAL, DM_STR_SHUTDOWN, DM_STR_REGISTRY, DM_STR_VOLUME_CONTEXT);
    _tprintf(_T("                         %s %s %s %s\n"), DM_STR_CLUSTER, DM_STR_BITMAP_OPEN, DM_STR_BITMAP_READ, DM_STR_BITMAP_WRITE);
    _tprintf(_T("                         %s %s %s %s\n"), DM_STR_MEM_TRACE, DM_STR_IOCTL_TRACE, DM_STR_POWER, DM_STR_WRITE);
    _tprintf(_T("                         %s %s %s %s\n"), DM_STR_DIRTY_BLOCKS, DM_STR_VOLUME_STATE, DM_STR_INMAGE_IOCTL, DM_STR_ALL);

    return;
}

bool
InitializeCommandLineOptionsWithDefaults(PCOMMAND_LINE_OPTIONS pCommandLineOptions)
{
    if (NULL == pCommandLineOptions)
        return false;

    pCommandLineOptions->bSendShutdownNotification = false;
    InitializeListHead(&pCommandLineOptions->CommandList);
    return true;
}

bool
ParseWaitForKeywordCommand(TCHAR *argv[], int argc, int &iParsedOptions, PCOMMAND_LINE_OPTIONS pCommandLineOptions)
{
    PCOMMAND_STRUCT     pCommandStruct = NULL;

    iParsedOptions++;
    pCommandStruct = AllocateCommandStruct();
    if (NULL == pCommandStruct) {
        _tprintf(_T("ParseWaitForKeywordCommand: AllocateCommandStruct failed\n"));
        return false;
    }
    // This option should have another parameter
    pCommandStruct->ulCommand = CMD_WAIT_FOR_KEYWORD;
    if (argc <= iParsedOptions) {
        _tprintf(_T("ParseWaitForKeywordCommand: missed parameter for %s"), CMDOPT_WAIT_FOR_KEYWORD);
        return false;
    }

    if (IS_COMMAND(argv[iParsedOptions])) {
        _tprintf(_T("ParseWaitForKeywordCommand: Expecting Keyword instead found command parameter %s for Command %s\n"), 
                                    argv[iParsedOptions], CMDOPT_WAIT_FOR_KEYWORD);
        return false;
    }

    _tcsncpy_s(pCommandStruct->data.KeywordData.Keyword, ARRAYSIZE(pCommandStruct->data.KeywordData.Keyword), argv[iParsedOptions], KEYWORD_SIZE);
    pCommandStruct->data.KeywordData.Keyword[KEYWORD_SIZE] = _T('\0');
    iParsedOptions++;

    InsertTailList(&pCommandLineOptions->CommandList, &pCommandStruct->ListEntry);
    return true;
}

bool
ParseClearDiffsCommand(TCHAR *argv[], int argc, int &iParsedOptions, PCOMMAND_LINE_OPTIONS pCommandLineOptions)
{
    PCOMMAND_STRUCT     pCommandStruct = NULL;
    TCHAR               DriveLetter;

    iParsedOptions++;

    pCommandStruct = AllocateCommandStruct();
    if (NULL == pCommandStruct) {
        _tprintf(_T("ParseClearDiffsCommand: AllocateCommandStruct failed\n"));
        return false;
    }
    // This option should have another parameter
    pCommandStruct->ulCommand = CMD_CLEAR_DIFFS;
    if (argc <= iParsedOptions) {
        _tprintf(_T("ParseClearDiffsCommand: missed parameter for %s\n"), CMDOPT_CLEAR_DIFFS);
        return false;
    }
    if (!IS_DRIVE_LETTER_PAR(argv[iParsedOptions])) {
        _tprintf(_T("ParseClearDiffsCommand: Expecting DriveLetter instead found Invalid parameter %s for Command %s\n"), 
                                    argv[iParsedOptions], CMDOPT_CLEAR_DIFFS);
        return false;
    }

    DriveLetter = argv[iParsedOptions][0];
    if (!IS_DRIVE_LETTER(DriveLetter)) {
        _tprintf(_T("ParseClearDiffsCommand: Expecting DriveLetter instead found Invalid parameter %s for Command %s\n"), 
                                    argv[iParsedOptions], CMDOPT_CLEAR_DIFFS);
        return false;
    }

    if (false == DriveLetterToGUID(DriveLetter, pCommandStruct->data.ClearDiffs.VolumeGUID,
                        sizeof(pCommandStruct->data.ClearDiffs.VolumeGUID)) ) 
    {
        _tprintf(_T("ParseClearDiffsCommand: DriveLetterToGUID failed to convert DriveLetter(%c) to GUID"), DriveLetter);
        return false;
    }

    pCommandStruct->data.ClearDiffs.DriveLetter = DriveLetter;
    iParsedOptions++;

    InsertTailList(&pCommandLineOptions->CommandList, &pCommandStruct->ListEntry);
    return true;
}

bool
ParseStopFilteringCommand(TCHAR *argv[], int argc, int &iParsedOptions, PCOMMAND_LINE_OPTIONS pCommandLineOptions)
{
    PCOMMAND_STRUCT     pCommandStruct = NULL;
    TCHAR               DriveLetter;

    iParsedOptions++;

    pCommandStruct = AllocateCommandStruct();
    if (NULL == pCommandStruct) {
        _tprintf(_T("ParseStopFilteringCommand: AllocateCommandStruct failed\n"));
        return false;
    }
    // This option should have another parameter
    pCommandStruct->ulCommand = CMD_STOP_FILTERING;
    if (argc <= iParsedOptions) {
        _tprintf(_T("ParseStopFilteringCommand: missed parameter for CMD_STOP_FILTERING"));
        return false;
    }
    if (!IS_DRIVE_LETTER_PAR(argv[iParsedOptions])) {
        _tprintf(_T("ParseStopFilteringCommand: Expecting DriveLetter instead found Invalid parameter %s for Command %s\n"), 
                                    argv[iParsedOptions], CMD_STOP_FILTERING);
        return false;
    }

    DriveLetter = argv[iParsedOptions][0];
    if (!IS_DRIVE_LETTER(DriveLetter)) {
        _tprintf(_T("ParseStopFilteringCommand: Expecting DriveLetter instead found Invalid parameter %s for Command %s\n"), 
                                    argv[iParsedOptions], CMD_STOP_FILTERING);
        return false;
    }

    if (false == DriveLetterToGUID(DriveLetter, pCommandStruct->data.StopFiltering.VolumeGUID,
                        sizeof(pCommandStruct->data.StopFiltering.VolumeGUID)) ) 
    {
        _tprintf(_T("ParseStopFilteringCommand: DriveLetterToGUID failed to convert DriveLetter(%c) to GUID"), DriveLetter);
        return false;
    }

    pCommandStruct->data.StopFiltering.DriveLetter = DriveLetter;
    iParsedOptions++;

    InsertTailList(&pCommandLineOptions->CommandList, &pCommandStruct->ListEntry);
    return true;
}

bool
ParseStartFilteringCommand(TCHAR *argv[], int argc, int &iParsedOptions, PCOMMAND_LINE_OPTIONS pCommandLineOptions)
{
    PCOMMAND_STRUCT     pCommandStruct = NULL;
    TCHAR               DriveLetter;

    iParsedOptions++;

    pCommandStruct = AllocateCommandStruct();
    if (NULL == pCommandStruct) {
        _tprintf(_T("ParseStartFilteringCommand: AllocateCommandStruct failed\n"));
        return false;
    }
    // This option should have another parameter
    pCommandStruct->ulCommand = CMD_START_FILTERING;
    if (argc <= iParsedOptions) {
        _tprintf(_T("ParseStartFilteringCommand: missed parameter for %s\n"), CMDOPT_START_FILTERING);
        return false;
    }
    if (!IS_DRIVE_LETTER_PAR(argv[iParsedOptions])) {
        _tprintf(_T("ParseStartFilteringCommand: Expecting DriveLetter instead found Invalid parameter %s for Command %s\n"), 
                                    argv[iParsedOptions], CMDOPT_START_FILTERING);
        return false;
    }

    DriveLetter = argv[iParsedOptions][0];
    if (!IS_DRIVE_LETTER(DriveLetter)) {
        _tprintf(_T("ParseStartFilteringCommand: Expecting DriveLetter instead found Invalid parameter %s for Command %s\n"), 
                                    argv[iParsedOptions], CMDOPT_START_FILTERING);
        return false;
    }

    if (false == DriveLetterToGUID(DriveLetter, pCommandStruct->data.StartFiltering.VolumeGUID,
                        sizeof(pCommandStruct->data.StartFiltering.VolumeGUID)) ) 
    {
        _tprintf(_T("ParseStartFilteringCommand: DriveLetterToGUID failed to convert DriveLetter(%c) to GUID"), DriveLetter);
        return false;
    }

    pCommandStruct->data.StartFiltering.DriveLetter = DriveLetter;
    iParsedOptions++;

    InsertTailList(&pCommandLineOptions->CommandList, &pCommandStruct->ListEntry);
    return true;
}

bool
ParseCheckShutdownStatusCommand(TCHAR *argv[], int argc, int &iParsedOptions, PCOMMAND_LINE_OPTIONS pCommandLineOptions)
{
    PCOMMAND_STRUCT     pCommandStruct = NULL;

    iParsedOptions++;

    if (false == pCommandLineOptions->bSendShutdownNotification) {
        _tprintf(_T("ParseCheckShutdownStatusCommand: %s command without option %s\n"), 
            CMDOPT_CHECK_SHUTDOWN_STATUS, CMDOPT_SHUTDOWN_NOTIFY);    
        return false;
    }
    pCommandStruct = AllocateCommandStruct();
    if (NULL == pCommandStruct) {
        _tprintf(_T("ParseCheckShutdownStatusCommand: AllocateCommandStruct failed\n"));
        return false;
    }

    pCommandStruct->ulCommand = CMD_CHECK_SHUTDOWN_STATUS;
    InsertTailList(&pCommandLineOptions->CommandList, &pCommandStruct->ListEntry);
    return true;
}

bool
ParsePrintStatisticsCommand(TCHAR *argv[], int argc, int &iParsedOptions, PCOMMAND_LINE_OPTIONS pCommandLineOptions)
{
    PCOMMAND_STRUCT     pCommandStruct = NULL;
    TCHAR               DriveLetter;

    iParsedOptions++;

    pCommandStruct = AllocateCommandStruct();
    if (NULL == pCommandStruct) {
        _tprintf(_T("ParsePrintStatisticsCommand: AllocateCommandStruct failed\n"));
        return false;
    }
    pCommandStruct->ulCommand = CMD_PRINT_STATISTICS;

    // This command has optional drive letter as a parameter.
    pCommandStruct->data.PrintStatisticsData.DriveLetter = L'\0';
    if (argc <= iParsedOptions) {
        // no volume letter specified
        InsertTailList(&pCommandLineOptions->CommandList, &pCommandStruct->ListEntry);
        return true;
    }

    if (!IS_DRIVE_LETTER_PAR(argv[iParsedOptions])) {
        // no volume letter specified
        InsertTailList(&pCommandLineOptions->CommandList, &pCommandStruct->ListEntry);
        return true;
    }

    DriveLetter = argv[iParsedOptions][0];
    
    if (!IS_DRIVE_LETTER(DriveLetter)) {
        // no volume letter specified
        _tprintf(_T("ParsePrintStatisticsCommand: Expecting DriveLetter instead found Invalid parameter %s for Command %s\n"), 
                                    argv[iParsedOptions], CMDOPT_PRINT_STATISTICS);
        return false;
    }

    if (false == DriveLetterToGUID(DriveLetter, pCommandStruct->data.PrintStatisticsData.VolumeGUID,
                        sizeof(pCommandStruct->data.PrintStatisticsData.VolumeGUID)) ) 
    {
        _tprintf(_T("ParsePrintStatisticsCommand: DriveLetterToGUID failed to convert DriveLetter(%c) to GUID"), DriveLetter);
        return false;
    }

    pCommandStruct->data.PrintStatisticsData.DriveLetter = DriveLetter;
    iParsedOptions++;
    InsertTailList(&pCommandLineOptions->CommandList, &pCommandStruct->ListEntry);
    return true;
}

bool
ParseClearStatisticsCommand(TCHAR *argv[], int argc, int &iParsedOptions, PCOMMAND_LINE_OPTIONS pCommandLineOptions)
{
    PCOMMAND_STRUCT     pCommandStruct = NULL;
    TCHAR               DriveLetter;

    iParsedOptions++;

    pCommandStruct = AllocateCommandStruct();
    if (NULL == pCommandStruct) {
        _tprintf(_T("ParsePrintStatisticsCommand: AllocateCommandStruct failed\n"));
        return false;
    }
    pCommandStruct->ulCommand = CMD_CLEAR_STATSTICS;

    // This command has optional drive letter as a parameter.
    pCommandStruct->data.ClearStatisticsData.DriveLetter = L'\0';
    if (argc <= iParsedOptions) {
        // no volume letter specified
        InsertTailList(&pCommandLineOptions->CommandList, &pCommandStruct->ListEntry);
        return true;
    }

    if (!IS_DRIVE_LETTER_PAR(argv[iParsedOptions])) {
        // no volume letter specified
        InsertTailList(&pCommandLineOptions->CommandList, &pCommandStruct->ListEntry);
        return true;
    }

    DriveLetter = argv[iParsedOptions][0];
    if (!IS_DRIVE_LETTER(DriveLetter)) {
        _tprintf(_T("ParseClearStatisticsCommand: Expecting DriveLetter instead found Invalid parameter %s for Command %s\n"), 
                                    argv[iParsedOptions], CMDOPT_CLEAR_STATISTICS);
        return false;
    }

    if (false == DriveLetterToGUID(DriveLetter, pCommandStruct->data.ClearStatisticsData.VolumeGUID,
                        sizeof(pCommandStruct->data.ClearStatisticsData.VolumeGUID)) ) 
    {
        _tprintf(_T("ParseClearStatisticsCommand: DriveLetterToGUID failed to convert DriveLetter(%c) to GUID"), DriveLetter);
        return false;
    }

    pCommandStruct->data.ClearStatisticsData.DriveLetter = DriveLetter;

    iParsedOptions++;
    InsertTailList(&pCommandLineOptions->CommandList, &pCommandStruct->ListEntry);
    return true;
}

bool
ParseWaitTimeCommand(TCHAR *argv[], int argc, int &iParsedOptions, PCOMMAND_LINE_OPTIONS pCommandLineOptions)
{
    PCOMMAND_STRUCT     pCommandStruct = NULL;

    iParsedOptions++;

    pCommandStruct = AllocateCommandStruct();
    if (NULL == pCommandStruct) {
        _tprintf(_T("ParseWaitTimeCommand: AllocateCommandStruct failed\n"));
        return false;
    }
    // This option should have another parameter
    pCommandStruct->ulCommand = CMD_WAIT_TIME;
    if (argc <= iParsedOptions) {
        _tprintf(_T("ParseWaitTimeCommand: missed parameter for CMD_WAIT_TIME"));
        return false;
    }
    if (_T('-') == argv[iParsedOptions][0]) {
        _tprintf(_T("ParseWaitTimeCommand: missed parameter for CMD_WAIT_TIME"));
        return false;
    }
    pCommandStruct->data.TimeOut.ulTimeout = _tstoi(argv[iParsedOptions]);
    if (0 == pCommandStruct->data.TimeOut.ulTimeout) {
        _tprintf(_T("ParseWaitTimeCommand: Invalid parameter %s for CMD_WAIT_TIME"), argv[iParsedOptions]);
        return false;
    }
    iParsedOptions++;

    InsertTailList(&pCommandLineOptions->CommandList, &pCommandStruct->ListEntry);
    return true;
}

bool
ConvertDebugLevelStringToNum(TCHAR *pString, ULONG &ulDebugLevel)
{
    int     i;

    for (i = 0; NULL != LevelStringMapping[i].pString; i++) {
		if (0 == _tcsicmp(pString, LevelStringMapping[i].pString)) {
			ulDebugLevel = LevelStringMapping[i].ulValue;   
            return true;
		}
    }

    _tprintf(_T("ConvertDebugLevelStringToNum: invalid debug level %s\n"), pString);

    return false;
}

TCHAR*
UlongToDebugLevelString(ULONG ulValue)
{
    int     i;

    for (i = 0; NULL != LevelStringMapping[i].pString; i++) {
		if (ulValue == LevelStringMapping[i].ulValue) {
            return LevelStringMapping[i].pString;
		}
    }

    return DL_STR_UNKNOWN;
}

bool
GetDebugModules(TCHAR *argv[], int argc, int &iParsedOptions, ULONG &ulDebugModules)
{
    int     i;

    while ((argc > iParsedOptions) && IS_COMMAND_PAR(argv[iParsedOptions])) {
        for (i = 0; NULL != ModuleStringMapping[i].pString; i++) {
		    if (0 == _tcsicmp(argv[iParsedOptions], ModuleStringMapping[i].pString)) {
			    ulDebugModules |= ModuleStringMapping[i].ulValue;   
                break;
		    }
        }

        if (NULL == ModuleStringMapping[i].pString) {
            _tprintf(_T("GetDebugModules: Invalid debug module %s\n"), argv[iParsedOptions]);
            return false;
        }

        iParsedOptions++;
    }

    return true;
}

bool
ParseSetDebugInfoCommand(TCHAR *argv[], int argc, int &iParsedOptions, PCOMMAND_LINE_OPTIONS pCommandLineOptions)
{
    PCOMMAND_STRUCT     pCommandStruct = NULL;

    iParsedOptions++;

    pCommandStruct = AllocateCommandStruct();
    if (NULL == pCommandStruct) {
        _tprintf(_T("ParseSetDebugInfoCommand: AllocateCommandStruct failed\n"));
        return false;
    }

    pCommandStruct->ulCommand = CMD_SET_DEBUG_INFO;

    while((argc > iParsedOptions) && IS_SUB_OPTION(argv[iParsedOptions])) {
        if (0 == _tcsicmp(argv[iParsedOptions], CMDSUBOPT_RESET_FLAGS)) {
            iParsedOptions++;
            pCommandStruct->data.DebugInfoData.DebugInfo.ulDebugInfoFlags |= DEBUG_INFO_FLAGS_RESET_MODULES;
        } else if (0 == _tcsicmp(argv[iParsedOptions], CMDSUBOPT_DEBUG_LEVEL_FORALL)) {
            iParsedOptions++;
            if ((argc > iParsedOptions) && IS_COMMAND_PAR(argv[iParsedOptions])) {
                if (false == ConvertDebugLevelStringToNum(argv[iParsedOptions], pCommandStruct->data.DebugInfoData.DebugInfo.ulDebugLevelForAll))
                    return false;
            } else {
                _tprintf(_T("ParseSetDebugInfoCommand: Failed to find DebugLevel parameter\n"));
                return false;
            }
            pCommandStruct->data.DebugInfoData.DebugInfo.ulDebugInfoFlags |= DEBUG_INFO_FLAGS_SET_LEVEL_ALL;
            iParsedOptions++; //DebugLevel
        } else if (0 == _tcsicmp(argv[iParsedOptions], CMDSUBOPT_DEBUG_LEVEL)) {
            iParsedOptions++;
            if ((argc > iParsedOptions) && IS_COMMAND_PAR(argv[iParsedOptions])) {
                if (false == ConvertDebugLevelStringToNum(argv[iParsedOptions], pCommandStruct->data.DebugInfoData.DebugInfo.ulDebugLevelForMod))
                    return false;
            } else {
                _tprintf(_T("ParseSetDebugInfoCommand: Failed to find DebugLevel parameter\n"));
                return false;
            }
            pCommandStruct->data.DebugInfoData.DebugInfo.ulDebugInfoFlags |= DEBUG_INFO_FLAGS_SET_LEVEL;
            iParsedOptions++;   // DebugLevel
        } else if (0 == _tcsicmp(argv[iParsedOptions], CMDSUBOPT_DEBUG_MODULES)) {
            iParsedOptions++;
            if (false == GetDebugModules(argv, argc, iParsedOptions, pCommandStruct->data.DebugInfoData.DebugInfo.ulDebugModules))
                return false;
        }
    }

    InsertTailList(&pCommandLineOptions->CommandList, &pCommandStruct->ListEntry);
    return true;
}

bool
ParseSetVolumeFlagsCommand(TCHAR *argv[], int argc, int &iParsedOptions, PCOMMAND_LINE_OPTIONS pCommandLineOptions)
{
    PCOMMAND_STRUCT     pCommandStruct = NULL;
    TCHAR               DriveLetter;

    iParsedOptions++;

    pCommandStruct = AllocateCommandStruct();
    if (NULL == pCommandStruct) {
        _tprintf(_T("ParseSetVolumeFlagsCommand: AllocateCommandStruct failed\n"));
        return false;
    }

    pCommandStruct->ulCommand = CMD_SET_VOLUME_FLAGS;
    // This command has drive letter as a parameter.
    if (argc <= iParsedOptions) {
        _tprintf(_T("ParseSetVolumeFlagsCommand: missed drive letter parameter for %s\n"), CMDOPT_SET_VOLUME_FLAGS);
        return false;
    }
    if (!IS_DRIVE_LETTER_PAR(argv[iParsedOptions])) {
        _tprintf(_T("ParseSetVolumeFlagsCommand: Expecting DriveLetter instead found Invalid parameter %s for Command %s\n"), 
                                    argv[iParsedOptions], CMDOPT_SET_VOLUME_FLAGS);
        return false;
    }

    DriveLetter = argv[iParsedOptions][0];
    if (!IS_DRIVE_LETTER(DriveLetter)) {
        _tprintf(_T("ParseSetVolumeFlagsCommand: Expecting DriveLetter instead found Invalid parameter %s for Command %s\n"), 
                                    argv[iParsedOptions], CMDOPT_SET_VOLUME_FLAGS);
        return false;
    }

    if (false == DriveLetterToGUID(DriveLetter, pCommandStruct->data.VolumeFlagsData.VolumeGUID,
                        sizeof(pCommandStruct->data.VolumeFlagsData.VolumeGUID)) ) 
    {
        _tprintf(_T("ParseSetVolumeFlagsCommand: DriveLetterToGUID failed to convert DriveLetter(%c) to GUID"), DriveLetter);
        return false;
    }
    pCommandStruct->data.VolumeFlagsData.DriveLetter = DriveLetter;
    pCommandStruct->data.VolumeFlagsData.ulVolumeFlags = 0;
    pCommandStruct->data.VolumeFlagsData.eBitOperation = ecBitOpSet;

    iParsedOptions++;
    while((argc > iParsedOptions) && IS_SUB_OPTION(argv[iParsedOptions])) {
        if (0 == _tcsicmp(argv[iParsedOptions], CMDSUBOPT_IGNORE_PAGEFILE_WRITES)) {
            pCommandStruct->data.VolumeFlagsData.ulVolumeFlags |= VOLUME_FLAG_IGNORE_PAGEFILE_WRITES;
        } else if (0 == _tcsicmp(argv[iParsedOptions], CMDSUBOPT_DISABLE_BITMAP_READ)) {
            pCommandStruct->data.VolumeFlagsData.ulVolumeFlags |= VOLUME_FLAG_DISABLE_BITMAP_READ;
        } else if (0 == _tcsicmp(argv[iParsedOptions], CMDSUBOPT_DISABLE_BITMAP_WRITE)) {
            pCommandStruct->data.VolumeFlagsData.ulVolumeFlags |= VOLUME_FLAG_DISABLE_BITMAP_WRITE;
        } else if (0 == _tcsicmp(argv[iParsedOptions], CMDSUBOPT_MARK_READ_ONLY)) {
            pCommandStruct->data.VolumeFlagsData.ulVolumeFlags |= VOLUME_FLAG_READ_ONLY;
        } else if (0 == _tcsicmp(argv[iParsedOptions], CMDSUBOPT_RESET_FLAGS)) {
            pCommandStruct->data.VolumeFlagsData.eBitOperation = ecBitOpReset;
        }
        iParsedOptions++;
    }

    InsertTailList(&pCommandLineOptions->CommandList, &pCommandStruct->ListEntry);
    return true;
}

bool
ParseGetDirtyBlocksCommand(TCHAR *argv[], int argc, int &iParsedOptions, PCOMMAND_LINE_OPTIONS pCommandLineOptions)
{
    PCOMMAND_STRUCT     pCommandStruct = NULL;
    TCHAR               DriveLetter;

    iParsedOptions++;

    pCommandStruct = AllocateCommandStruct();
    if (NULL == pCommandStruct) {
        _tprintf(_T("ParseGetDirtyBlocksCommand: AllocateCommandStruct failed\n"));
        return false;
    }

    pCommandStruct->ulCommand = CMD_GET_DB_TRANS;
    // This command has drive letter as a parameter.
    if (argc <= iParsedOptions) {
        _tprintf(_T("ParseGetDirtyBlocksCommand: missed parameter for CMDOPT_GET_DB_TRANS"));
        return false;
    }
    if (!IS_DRIVE_LETTER_PAR(argv[iParsedOptions])) {
        _tprintf(_T("ParseGetDirtyBlocksCommand: Expecting DriveLetter instead found Invalid parameter %s for Command %s\n"), 
                                    argv[iParsedOptions], CMDOPT_GET_DB_TRANS);
        return false;
    }

    DriveLetter = argv[iParsedOptions][0];
    if (!IS_DRIVE_LETTER(DriveLetter)) {
        _tprintf(_T("ParseGetDirtyBlocksCommand: Expecting DriveLetter instead found Invalid parameter %s for Command %s\n"), 
                                    argv[iParsedOptions], CMDOPT_GET_DB_TRANS);
        return false;
    }

    if (false == DriveLetterToGUID(DriveLetter, pCommandStruct->data.GetDBTrans.VolumeGUID,
                        sizeof(pCommandStruct->data.GetDBTrans.VolumeGUID)) ) 
    {
        _tprintf(_T("ParseGetDirtyBlocksCommand: DriveLetterToGUID failed to convert DriveLetter(%c) to GUID"), DriveLetter);
        return false;
    }
    pCommandStruct->data.GetDBTrans.DriveLetter = DriveLetter;

    iParsedOptions++;
    while((argc > iParsedOptions) && IS_SUB_OPTION(argv[iParsedOptions])) {
        if (0 == _tcsicmp(argv[iParsedOptions], CMDSUBOPT_COMMIT_PREV)) {
            pCommandStruct->data.GetDBTrans.CommitPreviousTrans = true;
        } else if (0 == _tcsicmp(argv[iParsedOptions], CMDSUBOPT_COMMIT_CURRENT)) {
            pCommandStruct->data.GetDBTrans.CommitCurrentTrans = true;
        } else if (0 == _tcsicmp(argv[iParsedOptions], CMDSUBOPT_PRINT_DETAILS)) {
            pCommandStruct->data.GetDBTrans.PrintDetails = true;
        } else if (0 == _tcsicmp(argv[iParsedOptions], CMDSUBOPT_COMMIT_TIMEOUT)) {
            pCommandStruct->data.GetDBTrans.CommitCurrentTrans = true;
            // This sub option should have a parameter.
            iParsedOptions++;
            if (IS_OPTION(argv[iParsedOptions])) {
                _tprintf(_T("ParseGetDirtyBlocksCommand: Invalid parameter %s for CMDSUBOPT_COMMIT_TIMEOUT\n"),
                                argv[iParsedOptions]);
                return false;
            }
            pCommandStruct->data.GetDBTrans.ulWaitTimeBeforeCurrentTransCommit =  _tstoi(argv[iParsedOptions]);
        }
        iParsedOptions++;
    }

    InsertTailList(&pCommandLineOptions->CommandList, &pCommandStruct->ListEntry);
    return true;
}

bool
ParseCommitBlocksCommand(TCHAR *argv[], int argc, int &iParsedOptions, PCOMMAND_LINE_OPTIONS pCommandLineOptions)
{
    PCOMMAND_STRUCT     pCommandStruct = NULL;
    TCHAR               DriveLetter;

    iParsedOptions++;

    pCommandStruct = AllocateCommandStruct();
    if (NULL == pCommandStruct) {
        _tprintf(_T("ParseCommitBlocksCommand: AllocateCommandStruct failed\n"));
        return false;
    }

    pCommandStruct->ulCommand = CMD_COMMIT_DB_TRANS;
    // This command has drive letter as a parameter.
    if (argc <= iParsedOptions) {
        _tprintf(_T("ParseCommitBlocksCommand: missed parameter for CMDOPT_COMMIT_DB_TRANS"));
        return false;
    }
    if (!IS_DRIVE_LETTER_PAR(argv[iParsedOptions])) {
        _tprintf(_T("ParseCommitBlocksCommand: Expecting DriveLetter instead found Invalid parameter %s for Command %s\n"), 
                                    argv[iParsedOptions], CMDOPT_COMMIT_DB_TRANS);
        return false;
    }

    DriveLetter = argv[iParsedOptions][0];
    if (!IS_DRIVE_LETTER(DriveLetter)) {
        _tprintf(_T("ParseCommitBlocksCommand: Expecting DriveLetter instead found Invalid parameter %s for Command %s\n"), 
                                    argv[iParsedOptions], CMDOPT_COMMIT_DB_TRANS);
        return false;
    }

    if (false == DriveLetterToGUID(DriveLetter, pCommandStruct->data.CommitDBTran.VolumeGUID,
                        sizeof(pCommandStruct->data.CommitDBTran.VolumeGUID)) ) 
    {
        _tprintf(_T("ParseCommitBlocksCommand: DriveLetterToGUID failed to convert DriveLetter(%c) to GUID"), DriveLetter);
        return false;
    }

    pCommandStruct->data.CommitDBTran.DriveLetter = DriveLetter;
    iParsedOptions++;

    InsertTailList(&pCommandLineOptions->CommandList, &pCommandStruct->ListEntry);
    return true;
}

bool
ParseCommandLineOptions(int argc, TCHAR *argv[], PCOMMAND_LINE_OPTIONS pCommandLineOptions)
{
    int iParsedOptions = 1;

    while (argc > iParsedOptions) {
        if (0 == _tcsicmp(argv[iParsedOptions], CMDOPT_SHUTDOWN_NOTIFY)) {
            pCommandLineOptions->bSendShutdownNotification = true;
            iParsedOptions++;
        } else if (0 == _tcsicmp(argv[iParsedOptions], CMDOPT_CHECK_SHUTDOWN_STATUS)) {
            if (false == ParseCheckShutdownStatusCommand(argv, argc, iParsedOptions, pCommandLineOptions))
                return false;
        } else if (0 == _tcsicmp(argv[iParsedOptions], CMDOPT_PRINT_STATISTICS)) {
            if (false == ParsePrintStatisticsCommand(argv, argc, iParsedOptions, pCommandLineOptions))
                return false;
        } else if (0 == _tcsicmp(argv[iParsedOptions], CMDOPT_CLEAR_STATISTICS)) {
            if (false == ParseClearStatisticsCommand(argv, argc, iParsedOptions, pCommandLineOptions))
                return false;
        } else if (0 == _tcsicmp(argv[iParsedOptions], CMDOPT_CLEAR_DIFFS)) {
            if (false == ParseClearDiffsCommand(argv, argc, iParsedOptions, pCommandLineOptions))
                return false;
        } else if (0 == _tcsicmp(argv[iParsedOptions], CMDOPT_START_FILTERING)) {
            if (false == ParseStartFilteringCommand(argv, argc, iParsedOptions, pCommandLineOptions))
                return false;
        } else if (0 == _tcsicmp(argv[iParsedOptions], CMDOPT_STOP_FILTERING)) {
            if (false == ParseStopFilteringCommand(argv, argc, iParsedOptions, pCommandLineOptions))
                return false;
        } else if (0 == _tcsicmp(argv[iParsedOptions], CMDOPT_WAIT_TIME)) {
            if (false == ParseWaitTimeCommand(argv, argc, iParsedOptions, pCommandLineOptions))
                return false;
        } else if (0 == _tcsicmp(argv[iParsedOptions], CMDOPT_GET_DB_TRANS)) {
            if (false == ParseGetDirtyBlocksCommand(argv, argc, iParsedOptions, pCommandLineOptions))
                return false;
        } else if (0 == _tcsicmp(argv[iParsedOptions], CMDOPT_COMMIT_DB_TRANS)) {
            if (false == ParseCommitBlocksCommand(argv, argc, iParsedOptions, pCommandLineOptions))
                return false;
        } else if (0 == _tcsicmp(argv[iParsedOptions], CMDOPT_WAIT_FOR_KEYWORD)) {
            if (false == ParseWaitForKeywordCommand(argv, argc, iParsedOptions, pCommandLineOptions))
                return false;
        } else if (0 == _tcsicmp(argv[iParsedOptions], CMDOPT_SET_VOLUME_FLAGS)) {
            if (false == ParseSetVolumeFlagsCommand(argv, argc, iParsedOptions, pCommandLineOptions))
                return false;
        } else if (0 == _tcsicmp(argv[iParsedOptions], CMDOPT_SET_DEBUG_INFO)) {
            if (false == ParseSetDebugInfoCommand(argv, argc, iParsedOptions, pCommandLineOptions))
                return false;
        } else {
            return false;
        }

    }

    if (IsListEmpty(&pCommandLineOptions->CommandList))
        return false;

    return true;
}

TCHAR *
BitMapStateString(
    etVBitmapState eBitMapState
    )
{
    TCHAR *BitMapStateString = NULL;
    switch (eBitMapState) {
		case ecVBitmapStateUnInitialized:
		case ecVBitmapStateOpened:
		case ecVBitmapStateReadStarted:
		case ecVBitmapStateReadPaused:
		case ecVBitmapStateReadCompleted:
		case ecVBitmapStateAddingChanges:
		case ecVBitmapStateClosed:
		case ecVBitmapStateReadError:
		case ecVBitmapStateInternalError:
            BitMapStateString = BitMapStateStringArray[eBitMapState];
            break;
        default:
            BitMapStateString = _T("ecBitMapStateUnknown");
            break;
    }

    return BitMapStateString;
}

TCHAR *
DataSourceToString(
    ULONG   ulDataSource
    )
{
    TCHAR *DataSourceString = NULL;
    switch(ulDataSource) {
		case INVOLFLT_DATA_SOURCE_BITMAP:
		case INVOLFLT_DATA_SOURCE_META_DATA:
		case INVOLFLT_DATA_SOURCE_DATA:
            DataSourceString = DataSourceStringArray[ulDataSource];
			break;
		case INVOLFLT_DATA_SOURCE_UNDEFINED:
        default:
            DataSourceString = DataSourceStringArray[INVOLFLT_DATA_SOURCE_UNDEFINED];
			break;
    }

    return DataSourceString;
}

TCHAR *
ServiceStateString(
    etServiceState eServiceState
    )
{
    TCHAR *ServiceString = NULL;

    switch (eServiceState) {
        case ecServiceNotStarted:
        case ecServiceRunning:
        case ecServiceShutdown:
            ServiceString = ServiceStringArray[eServiceState];
            break;
        default:
            ServiceString = ServiceStringArray[ecServiceUnInitiliazed];
            break;
    }
    
    return ServiceString;
}

void
ClearDiffs(HANDLE hDevice, PCLEAR_DIFFERENTIALS_DATA pClearDifferentialsData)
{
	// Add one more char for NULL.
	BOOL	bResult;
	DWORD	dwReturn;

    bResult = DeviceIoControl(
                     hDevice,
                     IOCTL_INMAGE_CLEAR_DIFFERENTIALS,
                     pClearDifferentialsData->VolumeGUID,
                     sizeof(WCHAR) * GUID_SIZE_IN_CHARS,
                     NULL,
                     0,
                     &dwReturn,	
                     NULL        // Overlapped
                     );
	if (bResult)
        _tprintf(_T("Returned Success from Clear Bitmap DeviceIoControl call for volume %c:\n"), pClearDifferentialsData->DriveLetter);
	else
        _tprintf(_T("Returned Failed from Clear Bitmap DeviceIoControl call for volume %c:\n"), pClearDifferentialsData->DriveLetter);

	return;
}

void
ClearVolumeStats(HANDLE hDevice, PCLEAR_STATISTICS_DATA pClearStatistcsData)
{
	// Add one more char for NULL.
	BOOL	bResult;
	DWORD	dwReturn, dwInputSize = 0;
    PVOID   pInput = NULL;

    if (L'\0' != pClearStatistcsData->DriveLetter) {
        pInput = pClearStatistcsData->VolumeGUID;
        dwInputSize = sizeof(WCHAR) * GUID_SIZE_IN_CHARS;
    }

    bResult = DeviceIoControl(
                     hDevice,
                     IOCTL_INMAGE_CLEAR_VOLUME_STATS,
                     pInput,
                     dwInputSize,
                     NULL,
                     0,
                     &dwReturn,	
                     NULL        // Overlapped
                     );
    if (L'\0' == pClearStatistcsData->DriveLetter) {
	    if (bResult)
            _tprintf(_T("Returned Success from Clear Stats DeviceIoControl call for all volumes\n"));
	    else
            _tprintf(_T("Returned Failed from Clear Stats DeviceIoControl call for all volumes\n"));
    } else {
	    if (bResult)
            _tprintf(_T("Returned Success from Clear Stats DeviceIoControl call for volume %c:\n"), pClearStatistcsData->DriveLetter);
	    else
            _tprintf(_T("Returned Failed from Clear Stats DeviceIoControl call for volume %c:\n"), pClearStatistcsData->DriveLetter);
    }

	return;
}

void
StartFiltering(HANDLE hDevice, PSTART_FILTERING_DATA pStartFilteringData)
{
	// Add one more char for NULL.
	BOOL	bResult;
	DWORD	dwReturn;

    bResult = DeviceIoControl(
                     hDevice,
                     IOCTL_INMAGE_START_FILTERING_DEVICE,
                     pStartFilteringData->VolumeGUID,
                     sizeof(WCHAR) * GUID_SIZE_IN_CHARS,
                     NULL,
                     0,
                     &dwReturn,	
                     NULL        // Overlapped
                     );
	if (bResult)
        _tprintf(_T("Returned Success from start filtering DeviceIoControl call for volume %c:\n"), pStartFilteringData->DriveLetter);
	else
        _tprintf(_T("Returned Failed from start filtering DeviceIoControl call for volume %c:\n"), pStartFilteringData->DriveLetter);

	return;
}

void
StopFiltering(HANDLE hDevice, PSTOP_FILTERING_DATA pStopFilteringData)
{
	// Add one more char for NULL.
	BOOL	bResult;
	DWORD	dwReturn;

    bResult = DeviceIoControl(
                     hDevice,
                     IOCTL_INMAGE_STOP_FILTERING_DEVICE,
                     pStopFilteringData->VolumeGUID,
                     sizeof(WCHAR) * GUID_SIZE_IN_CHARS,
                     NULL,
                     0,
                     &dwReturn,	
                     NULL        // Overlapped
                     );
	if (bResult)
        _tprintf(_T("Returned Success from stop filtering DeviceIoControl call for volume %c:\n"), pStopFilteringData->DriveLetter);
	else
        _tprintf(_T("Returned Failed from stop filtering DeviceIoControl call for volume %c:\n"), pStopFilteringData->DriveLetter);

	return;
}

void
GetDriverStats(HANDLE hDevice, PPRINT_STATISTICS_DATA pPrintStatisticsData)
{
	// Add one more char for NULL.
	BOOL	bResult;
	DWORD	dwReturn, ulNumVolumes, dwOutputSize;
    PVOLUME_STATS_DATA  pVolumeStatsData;

    pVolumeStatsData = (PVOLUME_STATS_DATA)malloc(sizeof(VOLUME_STATS_DATA));
    if (NULL == pVolumeStatsData) {
        printf("GetDriverStats: Failed in memory allocation\n");
        return;
    }

    if(L'\0' == pPrintStatisticsData->DriveLetter) {
        bResult = DeviceIoControl(
                        hDevice,
                        IOCTL_INMAGE_GET_VOLUME_STATS,
                        NULL,
                        0,
                        pVolumeStatsData,
                        sizeof(VOLUME_STATS_DATA),
                        &dwReturn,	
                        NULL        // Overlapped
                        );
    } else {
        bResult = DeviceIoControl(
                        hDevice,
                        IOCTL_INMAGE_GET_VOLUME_STATS,
                        pPrintStatisticsData->VolumeGUID,
                        sizeof(WCHAR) * GUID_SIZE_IN_CHARS,
                        pVolumeStatsData,
                        sizeof(VOLUME_STATS_DATA),
                        &dwReturn,	
                        NULL        // Overlapped
                        );
    }

	if (bResult) {
        if ((L'\0' == pPrintStatisticsData->DriveLetter) && 
            (pVolumeStatsData->ulTotalVolumes > pVolumeStatsData->ulVolumesReturned)) 
        {
            // We asked for all volumes and buffer wasn't enough.
            ulNumVolumes = pVolumeStatsData->ulTotalVolumes;
            if (ulNumVolumes >= 1)
                ulNumVolumes--;

            free(pVolumeStatsData);
            dwOutputSize = sizeof(VOLUME_STATS_DATA) + (ulNumVolumes * sizeof(VOLUME_STATS));
            pVolumeStatsData = (PVOLUME_STATS_DATA)malloc(dwOutputSize);
            bResult = DeviceIoControl(
                            hDevice,
                            IOCTL_INMAGE_GET_VOLUME_STATS,
                            NULL,
                            0,
                            pVolumeStatsData,
                            dwOutputSize,
                            &dwReturn,	
                            NULL        // Overlapped
                            );
        }
    }

	if (bResult) {
        _tprintf(_T("Total Number of Volumes = %lu\n"), pVolumeStatsData->ulTotalVolumes);
        _tprintf(_T("Number of Volumes Data Returned = %lu\n"), pVolumeStatsData->ulVolumesReturned);
        _tprintf(_T("Service State is %s\n"), ServiceStateString(pVolumeStatsData->eServiceState));
        _tprintf(_T("Number of Dirty Blocks Allocated is %ld\n"), pVolumeStatsData->lDirtyBlocksAllocated);
        
        ulNumVolumes = 0;
        while (ulNumVolumes < pVolumeStatsData->ulVolumesReturned) {
            _tprintf(_T("\nIndex = %lu\n"), ulNumVolumes);
            wprintf(L"DriveLetter = %c: VolumeGUID = %.*s.\n", pVolumeStatsData->VolumeArray[ulNumVolumes].DriveLetter, 
                    GUID_SIZE_IN_CHARS, pVolumeStatsData->VolumeArray[ulNumVolumes].VolumeGUID);
            _tprintf(_T("Volume Size = %I64u (%I64x)\n"), pVolumeStatsData->VolumeArray[ulNumVolumes].ulVolumeSize.QuadPart,
                                             pVolumeStatsData->VolumeArray[ulNumVolumes].ulVolumeSize.QuadPart);
            _tprintf(_T("Changes returned = %I64u\n"), pVolumeStatsData->VolumeArray[ulNumVolumes].uliChangesReturnedToService.QuadPart);
            _tprintf(_T("Changes read from bitmap = %I64u\n"), pVolumeStatsData->VolumeArray[ulNumVolumes].uliChangesReadFromBitMap.QuadPart);
            _tprintf(_T("Changes writen to bitmap = %I64u\n"), pVolumeStatsData->VolumeArray[ulNumVolumes].uliChangesWrittenToBitMap.QuadPart);
            _tprintf(_T("Changes queued for writing = %lu\n"), pVolumeStatsData->VolumeArray[ulNumVolumes].ulChangesQueuedForWriting);

            _tprintf(_T("Bytes of changes Queued for writing = %I64u\n"), pVolumeStatsData->VolumeArray[ulNumVolumes].ulicbChangesQueuedForWriting.QuadPart);
            _tprintf(_T("Bytes of changes written = %I64u\n"), pVolumeStatsData->VolumeArray[ulNumVolumes].ulicbChangesWrittenToBitMap.QuadPart);
            _tprintf(_T("Bytes of changes read from bitmap = %I64u\n"), pVolumeStatsData->VolumeArray[ulNumVolumes].ulicbChangesReadFromBitMap.QuadPart);
            _tprintf(_T("Bytes of changes returned = %I64u\n"), pVolumeStatsData->VolumeArray[ulNumVolumes].ulicbChangesReturnedToService.QuadPart);
            _tprintf(_T("Bytes of changes pending commit = %I64u\n"), pVolumeStatsData->VolumeArray[ulNumVolumes].ulicbChangesPendingCommit.QuadPart);
            _tprintf(_T("Bytes of changes reverted from pending commit = %I64u\n"), pVolumeStatsData->VolumeArray[ulNumVolumes].ulicbChangesReverted.QuadPart);

            _tprintf(_T("Pending changes in queue = %lu\n"), pVolumeStatsData->VolumeArray[ulNumVolumes].ulPendingChanges);
            _tprintf(_T("Pending changes commit = %lu\n"), pVolumeStatsData->VolumeArray[ulNumVolumes].ulChangesPendingCommit);
            _tprintf(_T("Changes reverted from pending commit = %lu\n"), pVolumeStatsData->VolumeArray[ulNumVolumes].ulChangesReverted);
            _tprintf(_T("Bit map write errors = %lu\n"), pVolumeStatsData->VolumeArray[ulNumVolumes].lNumBitMapWriteErrors);
            _tprintf(_T("Bit map open errors = %l\n"), pVolumeStatsData->VolumeArray[ulNumVolumes].lNumBitmapOpenErrors);
            _tprintf(_T("Dirty Blocks in Queue = %ld\n"), pVolumeStatsData->VolumeArray[ulNumVolumes].lDirtyBlocksInQueue);
            _tprintf(_T("Bit map read state = %s\n"), BitMapStateString(pVolumeStatsData->VolumeArray[ulNumVolumes].eVBitmapState));
            if (0 != pVolumeStatsData->VolumeArray[ulNumVolumes].ulVolumeFlags) {
                _tprintf(_T("Volume Flags are\n"));
                if (pVolumeStatsData->VolumeArray[ulNumVolumes].ulVolumeFlags & VSF_BITMAP_NOT_OPENED)
                    _tprintf(_T("\tVSF_BITMAP_NOT_OPENED\n"));
                if (pVolumeStatsData->VolumeArray[ulNumVolumes].ulVolumeFlags & VSF_BITMAP_READ_NOT_STARTED)
                    _tprintf(_T("\tVSF_BITMAP_READ_NOT_STARTED\n"));
                if (pVolumeStatsData->VolumeArray[ulNumVolumes].ulVolumeFlags & VSF_BITMAP_READ_IN_PROGRESS)
                    _tprintf(_T("\tVSF_BITMAP_READ_IN_PROGRESS\n"));
                if (pVolumeStatsData->VolumeArray[ulNumVolumes].ulVolumeFlags & VSF_BITMAP_READ_PAUSED)
                    _tprintf(_T("\tVSF_BITMAP_READ_PAUSED\n"));
                if (pVolumeStatsData->VolumeArray[ulNumVolumes].ulVolumeFlags & VSF_BITMAP_READ_COMPLETE)
                    _tprintf(_T("\tVSF_BITMAP_READ_COMPLETE\n"));
                if (pVolumeStatsData->VolumeArray[ulNumVolumes].ulVolumeFlags & VSF_BITMAP_READ_ERROR)
                    _tprintf(_T("\tVSF_BITMAP_READ_ERROR\n"));
                if (pVolumeStatsData->VolumeArray[ulNumVolumes].ulVolumeFlags & VSF_BITMAP_WRITE_ERROR)
                    _tprintf(_T("\tVSF_BITMAP_WRITE_ERROR\n"));
                if (pVolumeStatsData->VolumeArray[ulNumVolumes].ulVolumeFlags & VSF_FILTERING_STOPPED)
                    _tprintf(_T("\tVSF_FILTERING_STOPPED\n"));
                if (pVolumeStatsData->VolumeArray[ulNumVolumes].ulVolumeFlags & VSF_BITMAP_WRITE_DISABLED)
                    _tprintf(_T("\tVSF_BITMAP_WRITE_DISABLED\n"));
                if (pVolumeStatsData->VolumeArray[ulNumVolumes].ulVolumeFlags & VSF_BITMAP_READ_DISABLED)
                    _tprintf(_T("\tVSF_BITMAP_READ_DISABLED\n"));
            }
            ulNumVolumes++;
        }
    }
	else
		_tprintf(_T("Returned Failed from GetDriverStats\n"));

    free(pVolumeStatsData);

	return;
}

void
CommitDBTrans(
    HANDLE                  hDevice,
    PCOMMIT_DB_TRANS_DATA   pCommitDBTran
    )
{
    COMMIT_TRANSACTION  CommitTrans;
    ULONG   ulError;
	BOOL	bResult;
	DWORD	dwReturn;

    memcpy(CommitTrans.VolumeGUID, pCommitDBTran->VolumeGUID, GUID_SIZE_IN_CHARS * sizeof(WCHAR));
    ulError = GetPendingTransactionID(&PendingTranListHead, pCommitDBTran->VolumeGUID, CommitTrans.ulTransactionID.QuadPart);
    if (ERROR_SUCCESS == ulError) {
        bResult = DeviceIoControl(
                        hDevice,
                        IOCTL_INMAGE_COMMIT_DIRTY_BLOCKS_TRANS,
                        &CommitTrans,
                        sizeof(COMMIT_TRANSACTION),
                        NULL,
                        0,
                        &dwReturn,	
                        NULL        // Overlapped
                        );
    	if (bResult) {
            wprintf(L"Commiting pending transaction on Volume %.*s Succeeded\n", GUID_SIZE_IN_CHARS, CommitTrans.VolumeGUID);
        } else {
            wprintf(L"Commiting pending transaction on Volume %.*s Failed\n", GUID_SIZE_IN_CHARS, CommitTrans.VolumeGUID);
        }
    } else {
        wprintf(L"Failed to find pending transaction on Volume %.*s Failed\n", GUID_SIZE_IN_CHARS, CommitTrans.VolumeGUID);
    }

	return;
}

ULONG
SetVolumeFlags(
    HANDLE              hDevice,
    PVOLUME_FLAGS_DATA  pVolumeFlagsData
    )
{
    VOLUME_FLAGS_INPUT  VolumeFlagsInput;
    VOLUME_FLAGS_OUTPUT VolumeFlagsOutput;
	BOOL	bResult;
	DWORD	dwReturn;

    memcpy(VolumeFlagsInput.VolumeGUID, pVolumeFlagsData->VolumeGUID, GUID_SIZE_IN_CHARS * sizeof(WCHAR));
    VolumeFlagsInput.ulVolumeFlags = pVolumeFlagsData->ulVolumeFlags;
    VolumeFlagsInput.eOperation = pVolumeFlagsData->eBitOperation;

    bResult = DeviceIoControl(
                    hDevice,
                    IOCTL_INMAGE_SET_VOLUME_FLAGS,
                    &VolumeFlagsInput,
                    sizeof(VOLUME_FLAGS_INPUT),
                    &VolumeFlagsOutput,
                    sizeof(VOLUME_FLAGS_OUTPUT),
                    &dwReturn,	
                    NULL        // Overlapped
                    );
    if (bResult) {
        _tprintf(_T("SetVolumeFlags: Set flags on Volume %c: Succeeded\n"), pVolumeFlagsData->DriveLetter);
    } else {
        _tprintf(_T("SetVolumeFlags: Set flags on Volume %c: Failed\n"),  pVolumeFlagsData->DriveLetter);
    }

    if (sizeof(VOLUME_FLAGS_OUTPUT) != dwReturn) {
        _tprintf(_T("SetVolumeFlags: dwReturn(%#x) did not match VOLUME_FLAGS_OUTPUT size(%#x)\n"), dwReturn, sizeof(VOLUME_FLAGS_OUTPUT));
        return ERROR_GEN_FAILURE;
    }

    if (VolumeFlagsOutput.ulVolumeFlags & VOLUME_FLAG_IGNORE_PAGEFILE_WRITES)
        _tprintf(_T("SetVolumeFlags: Currently VOLUME_FLAG_IGNORE_PAGEFILE_WRITES is set\n"));

    if (VolumeFlagsOutput.ulVolumeFlags & VOLUME_FLAG_DISABLE_BITMAP_READ)
        _tprintf(_T("SetVolumeFlags: Currently VOLUME_FLAG_DISABLE_BITMAP_READ is set\n"));

    if (VolumeFlagsOutput.ulVolumeFlags & VOLUME_FLAG_DISABLE_BITMAP_WRITE)
        _tprintf(_T("SetVolumeFlags: Currently VOLUME_FLAG_DISABLE_BITMAP_WRITE is set\n"));

    if (VolumeFlagsOutput.ulVolumeFlags & VOLUME_FLAG_READ_ONLY)
        _tprintf(_T("SetVolumeFlags: Currently VOLUME_FLAG_READ_ONLY is set\n"));

    return ERROR_SUCCESS;
}

ULONG
SetDebugInfo(
    HANDLE              hDevice,
    PDEBUG_INFO_DATA    pDebugInfoData
    )
{
    DEBUG_INFO  DebugInfoOutput;
	BOOL	bResult;
	DWORD	dwReturn;

    bResult = DeviceIoControl(
                    hDevice,
                    IOCTL_INMAGE_SET_DEBUG_INFO,
                    &pDebugInfoData->DebugInfo,
                    sizeof(DEBUG_INFO),
                    &DebugInfoOutput,
                    sizeof(DEBUG_INFO),
                    &dwReturn,	
                    NULL        // Overlapped
                    );
    if (bResult) {
        _tprintf(_T("SetDebugInfo: Set debug info Succeeded\n"));
    } else {
        _tprintf(_T("SetDebugInfo: Set debug info Failed\n"));
        return ERROR_GEN_FAILURE;
    }

    if (sizeof(DEBUG_INFO) != dwReturn) {
        _tprintf(_T("SetDebugInfo: dwReturn(%#x) did not match DEBUG_INFO size(%#x)\n"), dwReturn, sizeof(DEBUG_INFO));
        return ERROR_GEN_FAILURE;
    }

    // Print the current debug info.
    _tprintf(_T("DebugLevelForAll = %s\n"), UlongToDebugLevelString(DebugInfoOutput.ulDebugLevelForAll));
    _tprintf(_T("DebugLevelForModules = %s\n"), UlongToDebugLevelString(DebugInfoOutput.ulDebugLevelForMod));

    for (int i = 0; NULL != ModuleStringMapping[i].pString; i++) {
        if (0 != DebugInfoOutput.ulDebugModules) {
            _tprintf(_T("Following are debug modules\n"));
        } else {
            _tprintf(_T("No Debug Modules are set\n"));
        }

        if (ModuleStringMapping[i].ulValue == (DebugInfoOutput.ulDebugModules & ModuleStringMapping[i].ulValue)) {
            _tprintf(_T("\t\t%s\n"), ModuleStringMapping[i].pString);
        }
    }

    return ERROR_SUCCESS;
}

ULONG
GetDBTrans(
    HANDLE              hDevice,
    PGET_DB_TRANS_DATA  pDBTranData
    )
{
    PDIRTY_BLOCKS   pDirtyBlocks;
    COMMIT_TRANSACTION  CommitTrans;
	BOOL	bResult;
	DWORD	dwReturn;
    ULONG    ulError;

    pDirtyBlocks = (PDIRTY_BLOCKS)malloc(sizeof(DIRTY_BLOCKS));

    if (NULL == pDirtyBlocks) {
        _tprintf(_T("GetDBTrans: Memory allocation failed for DIRTY_BLOCKS\n"));
        return ERROR_OUTOFMEMORY;
    }

    memcpy(CommitTrans.VolumeGUID, pDBTranData->VolumeGUID, GUID_SIZE_IN_CHARS * sizeof(WCHAR));

    if (pDBTranData->CommitPreviousTrans) {
        ulError = GetPendingTransactionID(&PendingTranListHead, pDBTranData->VolumeGUID, CommitTrans.ulTransactionID.QuadPart);
        if (ERROR_SUCCESS != ulError) {
            _tprintf(_T("GetDBTrans: Failed to find pending transaction on Volume %c: Failed\n"), pDBTranData->DriveLetter);
        }
    }

    bResult = DeviceIoControl(
                    hDevice,
                    IOCTL_INMAGE_GET_DIRTY_BLOCKS_TRANS,
                    &CommitTrans,
                    sizeof(COMMIT_TRANSACTION),
                    pDirtyBlocks,
                    sizeof(DIRTY_BLOCKS),
                    &dwReturn,	
                    NULL        // Overlapped
                    );
    if (bResult) {
        _tprintf(_T("GetDBTrans: Get dirty blocks transaction on Volume %c: Succeeded\n"), pDBTranData->DriveLetter);
    } else {
        _tprintf(_T("GetDBTrans: Get dirty blocks pending transaction on Volume %c: Failed\n"),  pDBTranData->DriveLetter);
    }

    if (sizeof(DIRTY_BLOCKS) != dwReturn) {
        _tprintf(_T("GetDBTrans: dwReturn(%#x) did not match DIRTY_BLOCKS size(%#x)\n"), dwReturn, sizeof(DIRTY_BLOCKS));
        return ERROR_GEN_FAILURE;
    }

    if (0 == pDirtyBlocks->cChanges) {
        _tprintf(_T("GetDBTrans: No more changes to return\n"));
        return ERROR_SUCCESS;
    }

    if (true == pDBTranData->PrintDetails) {
        TIME_ZONE_INFORMATION   TimeZone;
        SYSTEMTIME              SystemTime, FirstChangeLT, LastChangeLT;

        GetTimeZoneInformation(&TimeZone);
        FileTimeToSystemTime((FILETIME *)&pDirtyBlocks->TagTimeStampOfFirstChange.TimeInHundNanoSecondsFromJan1601,
                        &SystemTime);
        SystemTimeToTzSpecificLocalTime(&TimeZone, &SystemTime, &FirstChangeLT);

        FileTimeToSystemTime((FILETIME *)&pDirtyBlocks->TagTimeStampOfLastChange.TimeInHundNanoSecondsFromJan1601,
                        &SystemTime);
        SystemTimeToTzSpecificLocalTime(&TimeZone, &SystemTime, &LastChangeLT);
        
        _tprintf(_T("GetDBTrans: ulTransactionID = %#I64x\n"), pDirtyBlocks->uliTransactionID.QuadPart);
        _tprintf(_T("GetDBTrans: Number of Changes = %#x\n"), pDirtyBlocks->cChanges);
        _tprintf(_T("GetDBTrans: Data source = %s\n"), DataSourceToString(pDirtyBlocks->TagDataSource.ulDataSource));
        _tprintf(_T("GetDBTrans: TimeStamp of first change : SeqID = %#x, %02d/%02d/%04d %02d:%02d:%02d:%04d\n"),
                            pDirtyBlocks->TagTimeStampOfFirstChange.ulSequenceNumber,
                            FirstChangeLT.wMonth, FirstChangeLT.wDay, FirstChangeLT.wYear,
                            FirstChangeLT.wHour, FirstChangeLT.wMinute, FirstChangeLT.wSecond, FirstChangeLT.wMilliseconds);
        _tprintf(_T("GetDBTrans: TimeStamp of last change : SeqID = %#x, %02d/%02d/%04d %02d:%02d:%02d:%04d\n"),
                            pDirtyBlocks->TagTimeStampOfLastChange.ulSequenceNumber,
                            LastChangeLT.wMonth, LastChangeLT.wDay, LastChangeLT.wYear,
                            LastChangeLT.wHour, LastChangeLT.wMinute, LastChangeLT.wSecond, LastChangeLT.wMilliseconds);
    }

    if (0 != pDBTranData->ulWaitTimeBeforeCurrentTransCommit) {
        Sleep(pDBTranData->ulWaitTimeBeforeCurrentTransCommit);
    }

    if ((0 != pDBTranData->ulWaitTimeBeforeCurrentTransCommit) || (true == pDBTranData->CommitCurrentTrans))
    {
        CommitTrans.ulTransactionID.QuadPart = pDirtyBlocks->uliTransactionID.QuadPart;
        bResult = DeviceIoControl(
                        hDevice,
                        IOCTL_INMAGE_COMMIT_DIRTY_BLOCKS_TRANS,
                        &CommitTrans,
                        sizeof(COMMIT_TRANSACTION),
                        NULL,
                        0,
                        &dwReturn,	
                        NULL        // Overlapped
                        );
    	if (bResult) {
            wprintf(L"Commiting pending transaction on Volume %c: Succeeded\n", pDBTranData->DriveLetter);
        } else {
            wprintf(L"Commiting pending transaction on Volume %c: Failed\n", pDBTranData->DriveLetter);
        }
    }

    return ERROR_SUCCESS;
}

void
WaitForKeyword(PWAIT_FOR_KEYWORD_DATA pKeywordData)
{
    TCHAR   Input[81];

    while (0 != _tcsicmp(pKeywordData->Keyword, Input)) {
        memset(Input, 0, sizeof(TCHAR) * 81);
        _tprintf(_T("Waiting for keyword %s\n"), pKeywordData->Keyword);
        _tscanf(_T("%80s"), Input);
    }

    return;
}

void
_tmain(int argc, TCHAR *argv[])
{
	HANDLE	hDevice = INVALID_HANDLE_VALUE;
    HANDLE  hShutdownNotification = INVALID_HANDLE_VALUE;
	BOOL	bDone = FALSE;

    InitializeListHead(&PendingTranListHead);
    InitializeCommandLineOptionsWithDefaults(&sCommandOptions);
    if (false == ParseCommandLineOptions(argc, argv, &sCommandOptions)) {
        Usage(argv[0]);
        return;
    }

	_tprintf(_T("Opening File %s\n"), INMAGE_FILTER_DOS_DEVICE_NAME);

	hDevice = CreateFile (
						INMAGE_FILTER_DOS_DEVICE_NAME,
						GENERIC_READ | GENERIC_WRITE,
						FILE_SHARE_READ | FILE_SHARE_WRITE,
						NULL,
						OPEN_EXISTING,
						FILE_FLAG_OVERLAPPED,
						NULL
						);

    if( INVALID_HANDLE_VALUE == hDevice ) {
        _tprintf(_T("CreateFile Failed with Error = %#x\n"), GetLastError());
        return;
    }

    if (sCommandOptions.bSendShutdownNotification) {
        hShutdownNotification = SendShutdownNotification(hDevice);
        if (INVALID_HANDLE_VALUE == hShutdownNotification)
            goto cleanup;

        if (true == HasShutdownNotificationCompleted(hShutdownNotification)) {
    		_tprintf(_T("Overlapped IO has completed\n"));
            goto cleanup;
        }
    }

    while (false == IsListEmpty(&sCommandOptions.CommandList)) {
        PCOMMAND_STRUCT pCommand;

        pCommand = (PCOMMAND_STRUCT)RemoveHeadList(&sCommandOptions.CommandList);
        switch(pCommand->ulCommand) {
            case CMD_CHECK_SHUTDOWN_STATUS:
                if (HasShutdownNotificationCompleted(hShutdownNotification)) {
                    _tprintf(_T("Shutdown Notification has completed\n"));
                } else {
                    _tprintf(_T("Shutdown Notification has not completed\n"));
                }
                break;
            case CMD_WAIT_FOR_KEYWORD:
                WaitForKeyword(&pCommand->data.KeywordData);
                break;
            case CMD_PRINT_STATISTICS:
                GetDriverStats(hDevice, &pCommand->data.PrintStatisticsData);
                break;
            case CMD_CLEAR_STATSTICS:
                ClearVolumeStats(hDevice, &pCommand->data.ClearStatisticsData);
                break;
            case CMD_CLEAR_DIFFS:
                ClearDiffs(hDevice, &pCommand->data.ClearDiffs);
                break;
            case CMD_START_FILTERING:
                StartFiltering(hDevice, &pCommand->data.StartFiltering);
                break;
            case CMD_STOP_FILTERING:
                StopFiltering(hDevice, &pCommand->data.StopFiltering);
                break;
            case CMD_WAIT_TIME:
                Sleep(pCommand->data.TimeOut.ulTimeout);
                break;
            case CMD_GET_DB_TRANS:
                GetDBTrans(hDevice, &pCommand->data.GetDBTrans);
                break;
            case CMD_COMMIT_DB_TRANS:
                CommitDBTrans(hDevice, &pCommand->data.CommitDBTran);
                break;
            case CMD_SET_VOLUME_FLAGS:
                SetVolumeFlags(hDevice, &pCommand->data.VolumeFlagsData);
                break;
            case CMD_SET_DEBUG_INFO:
                SetDebugInfo(hDevice, &pCommand->data.DebugInfoData);
                break;
            default:
                break;
        }

        FreeCommandStruct(pCommand);
    }

cleanup:
    if (INVALID_HANDLE_VALUE != hDevice)
        CloseHandle(hDevice);
    
    if (INVALID_HANDLE_VALUE != hShutdownNotification)
        CloseShutdownNotification(hShutdownNotification);

	return;
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
DriveLetterToGUID(TCHAR DriveLetter, WCHAR *VolumeGUID, ULONG ulBufLen)
{
    WCHAR   MountPoint[4];
    WCHAR   UniqueVolumeName[60];

    MountPoint[0] = (WCHAR)DriveLetter;
    MountPoint[1] = L':';
    MountPoint[2] = L'\\';
    MountPoint[3] = L'\0';

    if (ulBufLen  < (GUID_SIZE_IN_CHARS * sizeof(WCHAR)))
        return false;

    if (0 == GetVolumeNameForVolumeMountPointW((WCHAR *)MountPoint, (WCHAR *)UniqueVolumeName, 60)) {
        _tprintf(_T("DriveLetterToGUID failed\n"));
        return false;
    }

    memcpy(VolumeGUID, &UniqueVolumeName[11], (GUID_SIZE_IN_CHARS * sizeof(WCHAR)));

    return true;
}

/*
 *-------------------------------------------------------------------------------------------------
 * Routines to handle PENDING_TRANSACTIONS
 *-------------------------------------------------------------------------------------------------
 */
PPENDING_TRANSACTION
AllocatePendingTransaction()
{
    PPENDING_TRANSACTION    pPendingTrans = NULL;

    pPendingTrans = (PPENDING_TRANSACTION)malloc(sizeof(PENDING_TRANSACTION));

    if (NULL != pPendingTrans) {
        memset(pPendingTrans, 0, sizeof(PENDING_TRANSACTION));

    }

    return pPendingTrans;
}

void
DeallocatePendingTransaction(PPENDING_TRANSACTION pPendingTrans)
{
    if (NULL != pPendingTrans) {
        free (pPendingTrans);
    }
}

PPENDING_TRANSACTION
GetPendingTransactionRef(PLIST_ENTRY ListHead, PWCHAR VolumeGUID)
{
    PPENDING_TRANSACTION    pPendingTrans = NULL;
    PLIST_ENTRY             pListEntry;

    pListEntry = ListHead->Flink;
    while (pListEntry != ListHead) {
        if (0 == _wcsnicmp(VolumeGUID, ((PPENDING_TRANSACTION)pListEntry)->VolumeGUID, GUID_SIZE_IN_CHARS))
            return (PPENDING_TRANSACTION) pListEntry;

        pListEntry = pListEntry->Flink;
    }

    return NULL;
}

PPENDING_TRANSACTION
GetPendingTransaction(PLIST_ENTRY ListHead, PWCHAR VolumeGUID)
{
    PPENDING_TRANSACTION    pPendingTrans = NULL;

    pPendingTrans = GetPendingTransactionRef(ListHead, VolumeGUID);

    if (NULL != pPendingTrans) {
        RemoveEntryList(&pPendingTrans->ListEntry);
    }

    return pPendingTrans;
}

ULONG
GetPendingTransactionID(PLIST_ENTRY ListHead, PWCHAR VolumeGUID, ULONGLONG &ullTransactionId)
{
    PPENDING_TRANSACTION    pPendingTrans = NULL;

    pPendingTrans = GetPendingTransaction(ListHead, VolumeGUID);
    if (NULL != pPendingTrans) {
        ullTransactionId = pPendingTrans->ullTransactionID;
        DeallocatePendingTransaction(pPendingTrans);
        return ERROR_SUCCESS;
    }

    ullTransactionId = 0;
    return ERROR_NOT_FOUND;
}

ULONG
GetPendingTransactionIDRef(PLIST_ENTRY ListHead, PWCHAR VolumeGUID, ULONGLONG &ullTransactionId)
{
    PPENDING_TRANSACTION    pPendingTrans = NULL;

    pPendingTrans = GetPendingTransactionRef(ListHead, VolumeGUID);
    if (NULL != pPendingTrans) {
        ullTransactionId = pPendingTrans->ullTransactionID;
        return ERROR_SUCCESS;
    }

    ullTransactionId = 0;
    return ERROR_NOT_FOUND;
}

ULONG
AddPendingTransaction(PLIST_ENTRY ListHead, PWCHAR VolumeGUID, ULONGLONG ullTransactionId)
{
    PPENDING_TRANSACTION    pPendingTrans = NULL;

    pPendingTrans = GetPendingTransactionRef(ListHead, VolumeGUID);
    if (NULL == pPendingTrans) {
        pPendingTrans = AllocatePendingTransaction();
        memcpy(pPendingTrans->VolumeGUID, VolumeGUID, GUID_SIZE_IN_CHARS * sizeof(WCHAR));
        pPendingTrans->ullTransactionID = ullTransactionId;

        InsertHeadList(ListHead, &pPendingTrans->ListEntry);

        return ERROR_SUCCESS;
    } else {
        // There is a transaction for this GUID
        if (ullTransactionId == pPendingTrans->ullTransactionID) {
            // TransactionID matched so lets just leave it
            return ERROR_SUCCESS;
        } else {
            wprintf(L"Old TransactionID %#I64X for GUID %.*s does not match new transcation ID %#I64X\n",
                pPendingTrans->ullTransactionID, GUID_SIZE_IN_CHARS, VolumeGUID, ullTransactionId);
            pPendingTrans->ullTransactionID = ullTransactionId;
            return ERROR_ALREADY_EXISTS;
        }
    }
}

/*
 *-------------------------------------------------------------------------------------------------
 * Routines to handle SHUTDOWN Notifications
 *-------------------------------------------------------------------------------------------------
 */
HANDLE
SendShutdownNotification(HANDLE hDevice)
{
    PSHUTDOWN_NOTIFY_DATA   pShutdownNotifyData = (PSHUTDOWN_NOTIFY_DATA)INVALID_HANDLE_VALUE;
	BOOL	bResult;
	DWORD	dwReturn;

    pShutdownNotifyData = (PSHUTDOWN_NOTIFY_DATA)malloc(sizeof(SHUTDOWN_NOTIFY_DATA));
    if (NULL == pShutdownNotifyData)
        return INVALID_HANDLE_VALUE;

    memset(pShutdownNotifyData, 0, sizeof(SHUTDOWN_NOTIFY_DATA));

    pShutdownNotifyData->hDevice = hDevice;
	pShutdownNotifyData->hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (NULL == pShutdownNotifyData->hEvent)
	{
        _tprintf(_T("SendShutdownNotification: CreateEvent Failed with Error = %#x\n"), GetLastError());
		goto cleanup;
	}
    
    pShutdownNotifyData->Overlapped.hEvent = pShutdownNotifyData->hEvent;
	_tprintf(_T("Sending Shutdown Notify IRP\n"));
    bResult = DeviceIoControl(
                     hDevice,
                     IOCTL_INMAGE_SERVICE_SHUTDOWN_NOTIFY,
                     NULL,
                     0,
                     NULL,
                     0,
                     &dwReturn,	
                     &pShutdownNotifyData->Overlapped        // Overlapped
                     );	
    if (0 == bResult) {
        if (ERROR_IO_PENDING != GetLastError())
        {
            _tprintf(_T("SendShutdownNotification: DeviceIoControl failed with error = %#x\n"), GetLastError());
            goto cleanup;
        }
    }

    pShutdownNotifyData->ulFlags |= SND_FLAGS_COMMAND_SENT;

    return (HANDLE)pShutdownNotifyData;
cleanup:
    if (NULL == pShutdownNotifyData)
        return INVALID_HANDLE_VALUE;

    if (NULL != pShutdownNotifyData->hEvent)
        CloseHandle(pShutdownNotifyData->hEvent);

    free(pShutdownNotifyData);

    return INVALID_HANDLE_VALUE;
}

bool
HasShutdownNotificationCompleted(HANDLE hShutdownNotification)
{
    PSHUTDOWN_NOTIFY_DATA   pShutdownNotifyData;

    if (INVALID_HANDLE_VALUE == hShutdownNotification)
        return false;

    pShutdownNotifyData = (PSHUTDOWN_NOTIFY_DATA)hShutdownNotification;

    if ((pShutdownNotifyData->ulFlags & SND_FLAGS_COMMAND_SENT) &&
        ( 0 == (pShutdownNotifyData->ulFlags & SND_FLAGS_COMMAND_COMPLETED)))
    {
        if (HasOverlappedIoCompleted(&pShutdownNotifyData->Overlapped)) {
            pShutdownNotifyData->ulFlags |= SND_FLAGS_COMMAND_COMPLETED;
        }
    }

    if (pShutdownNotifyData->ulFlags & SND_FLAGS_COMMAND_COMPLETED)
        return true;

    return false;
}

void
CloseShutdownNotification(HANDLE hShutdownNotification)
{
    PSHUTDOWN_NOTIFY_DATA   pShutdownNotifyData;

    if (INVALID_HANDLE_VALUE == hShutdownNotification)
        return;

    pShutdownNotifyData = (PSHUTDOWN_NOTIFY_DATA)hShutdownNotification;

    if ((pShutdownNotifyData->ulFlags & SND_FLAGS_COMMAND_SENT) &&
        ( 0 == (pShutdownNotifyData->ulFlags & SND_FLAGS_COMMAND_COMPLETED)))
    {
        if (FALSE == HasOverlappedIoCompleted(&pShutdownNotifyData->Overlapped)) {
            CancelIo(pShutdownNotifyData->hDevice);
            pShutdownNotifyData->ulFlags |= SND_FLAGS_COMMAND_COMPLETED;
        }
    }

    if (NULL != pShutdownNotifyData->hEvent)
        CloseHandle(pShutdownNotifyData->hEvent);

    free(pShutdownNotifyData);

    return;
}

