#include <windows.h>
#include <stdio.h>
#include <tchar.h>
#include <malloc.h>
#include "ListEntry.h"
#include "InmFltInterface.h"
#include <map>
#include <vector>
#include <rpc.h>
#include <algorithm>
#include "drvutil.h"
#include "VFltCmdsParse.h"

using namespace std;

extern STRING_TO_ULONG ModuleStringMapping[];
extern STRING_TO_ULONG LevelStringMapping[];
extern bool help;
extern COMMAND_LINE_OPTIONS     sCommandOptions;

ULONG
GetSizeFromString(TCHAR *szValue)
{
    float   fSize;
    ULONG   ulSize;
    ULONG   ulLen = (ULONG)_tcslen(szValue);

    _stscanf(szValue, _T("%f"), &fSize);

    if (ulLen > 1) {
        if (0 == _tcsicmp(&szValue[ulLen-1], _T("m"))) {
            ulSize = (ULONG) (fSize * 1024 * 1024);
        } else if (0 == _tcsicmp(&szValue[ulLen-1], _T("k"))) {
            ulSize = (ULONG) (fSize * 1024);
        } else {
            ulSize = (ULONG)fSize;
        }
    } else {
        ulSize = (ULONG)fSize;
    }
    return ulSize;
}

bool
ParseWaitForKeywordCommand(TCHAR *argv[], int argc, int &iParsedOptions, PCOMMAND_LINE_OPTIONS pCommandLineOptions)
{
    PCOMMAND_STRUCT     pCommandStruct = NULL;

    iParsedOptions++;

    if ((argc > iParsedOptions) && IS_SUB_OPTION(argv[iParsedOptions])) {
        if (0 == _tcsicmp(argv[iParsedOptions], CMDSTR_HELP)||
            (0 == _tcsicmp(argv[iParsedOptions], CMDSTR_H))) {
            iParsedOptions++;
            help = true;
            return false;
        }
    }
    pCommandStruct = AllocateCommandStruct();
    if (NULL == pCommandStruct) {
        _tprintf(_T("ParseWaitForKeywordCommand: AllocateCommandStruct failed\n"));
        return false;
    }
    // This option should have another parameter
    pCommandStruct->ulCommand = CMD_WAIT_FOR_KEYWORD;
    if (argc <= iParsedOptions) {
        _tprintf(_T("ParseWaitForKeywordCommand: missed parameter for %s"), CMDSTR_WAIT_FOR_KEYWORD);
        return false;
    }

    if (IS_COMMAND(argv[iParsedOptions])) {
        _tprintf(_T("ParseWaitForKeywordCommand: Expecting Keyword instead found command parameter %s for Command %s\n"), 
                                    argv[iParsedOptions], CMDSTR_WAIT_FOR_KEYWORD);
        return false;
    }

    StringCchPrintf(pCommandStruct->data.KeywordData.Keyword, ARRAYSIZE(pCommandStruct->data.KeywordData.Keyword), L"%s", argv[iParsedOptions]);

    pCommandStruct->data.KeywordData.Keyword[KEYWORD_SIZE] = _T('\0');
    iParsedOptions++;

    InsertTailList(pCommandLineOptions->CommandList, &pCommandStruct->ListEntry);
    return true;
}

bool
ParseClearDiffsCommand(TCHAR *argv[], int argc, int &iParsedOptions, PCOMMAND_LINE_OPTIONS pCommandLineOptions)
{
    PCOMMAND_STRUCT     pCommandStruct = NULL;

    iParsedOptions++;

    if ((argc > iParsedOptions) && IS_SUB_OPTION(argv[iParsedOptions])) {
        if (0 == _tcsicmp(argv[iParsedOptions], CMDSTR_HELP)||
            (0 == _tcsicmp(argv[iParsedOptions], CMDSTR_H))) {
            iParsedOptions++;
            help = true;
            return false;
        }
    }
    pCommandStruct = AllocateCommandStruct();
    if (NULL == pCommandStruct) {
        _tprintf(_T("ParseClearDiffsCommand: AllocateCommandStruct failed\n"));
        return false;
    }
    // This option should have another parameter
    pCommandStruct->ulCommand = CMD_CLEAR_DIFFS;
    if (argc <= iParsedOptions) {
        _tprintf(_T("ParseClearDiffsCommand: missed parameter for %s\n"), CMDSTR_CLEAR_DIFFS);
        return false;
    }
	
    if (!GetVolumeGUID(argv[iParsedOptions], pCommandStruct->data.ClearDiffs.VolumeGUID, 
        sizeof(pCommandStruct->data.ClearDiffs.VolumeGUID), "ParseClearDiffsCommand",
        CMDSTR_CLEAR_DIFFS))
    {
        return false;
    }
    pCommandStruct->data.ClearDiffs.MountPoint = argv[iParsedOptions];
    iParsedOptions++;

    InsertTailList(pCommandLineOptions->CommandList, &pCommandStruct->ListEntry);
    return true;
}

bool
ParseStopFilteringCommand(TCHAR *argv[], int argc, int &iParsedOptions, PCOMMAND_LINE_OPTIONS pCommandLineOptions)
{
    PCOMMAND_STRUCT     pCommandStruct = NULL;

    iParsedOptions++;

    if ((argc > iParsedOptions) && IS_SUB_OPTION(argv[iParsedOptions])) {
        if (0 == _tcsicmp(argv[iParsedOptions], CMDSTR_HELP)||
            (0 == _tcsicmp(argv[iParsedOptions], CMDSTR_H))) {
            iParsedOptions++;
            help = true;
            return false;
        }
    }
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

    if ((IS_SUB_OPTION(argv[iParsedOptions])) && 
            (0 == _tcsicmp(argv[iParsedOptions], CMDOPTSTR_STOP_ALL))) {
        pCommandStruct->data.StopFiltering.bStopAll = true;
    } else {	    
        if (!GetVolumeGUID(argv[iParsedOptions], pCommandStruct->data.StopFiltering.VolumeGUID, 
            sizeof(pCommandStruct->data.StopFiltering.VolumeGUID), "ParseStopFilteringCommand",
            CMDSTR_STOP_FILTERING))
        {
            return false;
        }
        pCommandStruct->data.StopFiltering.MountPoint = argv[iParsedOptions];
    }	
    iParsedOptions++;

    InsertTailList(pCommandLineOptions->CommandList, &pCommandStruct->ListEntry);

   if ((argc > iParsedOptions) && IS_SUB_OPTION(argv[iParsedOptions])) {
        if (0 == _tcsicmp(argv[iParsedOptions], CMDOPTSTR_BITMAP_DELETE)) {
            iParsedOptions++;
            pCommandStruct->data.StopFiltering.bdeleteBitmapFile= true;
        }
   }
    return true;
}

bool
ParseStartFilteringCommand(TCHAR *argv[], int argc, int &iParsedOptions, PCOMMAND_LINE_OPTIONS pCommandLineOptions)
{
    PCOMMAND_STRUCT     pCommandStruct = NULL;

    iParsedOptions++;

    if ((argc > iParsedOptions) && IS_SUB_OPTION(argv[iParsedOptions])) {
        if (0 == _tcsicmp(argv[iParsedOptions], CMDSTR_HELP)||
            (0 == _tcsicmp(argv[iParsedOptions], CMDSTR_H))) {
            iParsedOptions++;
            help = true;
            return false;
        }
    }
    pCommandStruct = AllocateCommandStruct();
    if (NULL == pCommandStruct) {
        _tprintf(_T("ParseStartFilteringCommand: AllocateCommandStruct failed\n"));
        return false;
    }
    // This option should have another parameter
    pCommandStruct->ulCommand = CMD_START_FILTERING;
    if (argc <= iParsedOptions) {
        _tprintf(_T("ParseStartFilteringCommand: missed parameter for %s\n"), CMDSTR_START_FILTERING);
        return false;
    }
    if (!GetVolumeGUID(argv[iParsedOptions], pCommandStruct->data.StartFiltering.VolumeGUID, 
        sizeof(pCommandStruct->data.StartFiltering.VolumeGUID), "ParseStartFilteringCommand",
        CMDSTR_START_FILTERING))
    {
        return false;
    }

    pCommandStruct->data.StartFiltering.MountPoint = argv[iParsedOptions];
    iParsedOptions++;

    InsertTailList(pCommandLineOptions->CommandList, &pCommandStruct->ListEntry);
    return true;
}

bool
ParseSetDiskClusteredCommand(TCHAR* argv[], int argc, int& iParsedOptions, PCOMMAND_LINE_OPTIONS pCommandLineOptions)
{
    PCOMMAND_STRUCT     pCommandStruct = NULL;

    iParsedOptions++;

    if ((argc > iParsedOptions) && IS_SUB_OPTION(argv[iParsedOptions])) {
        if (0 == _tcsicmp(argv[iParsedOptions], CMDSTR_HELP) ||
            (0 == _tcsicmp(argv[iParsedOptions], CMDSTR_H))) {
            iParsedOptions++;
            help = true;
            return false;
        }
    }
    pCommandStruct = AllocateCommandStruct();
    if (NULL == pCommandStruct) {
        _tprintf(_T("ParseStartFilteringCommand: AllocateCommandStruct failed\n"));
        return false;
    }
    // This option should have another parameter
    pCommandStruct->ulCommand = CMD_SET_DISK_CLUSTERED;
    if (argc <= iParsedOptions) {
        _tprintf(_T("ParseStartFilteringCommand: missed parameter for %s\n"), CMDSTR_SET_DISK_CLUSTERED);
        return false;
    }
    if (!GetVolumeGUID(argv[iParsedOptions], pCommandStruct->data.SetDiskClustered.VolumeGUID,
        sizeof(pCommandStruct->data.SetDiskClustered.VolumeGUID), "ParseSetDiskClusteredCommand",
        CMDSTR_SET_DISK_CLUSTERED))
    {
        return false;
    }

    pCommandStruct->data.SetDiskClustered.MountPoint = argv[iParsedOptions];
    iParsedOptions++;

    InsertTailList(pCommandLineOptions->CommandList, &pCommandStruct->ListEntry);
    return true;
}

bool
ParseResyncStartNotifyCommand(TCHAR *argv[], int argc, int &iParsedOptions, PCOMMAND_LINE_OPTIONS pCommandLineOptions)
{
    PCOMMAND_STRUCT     pCommandStruct = NULL;

    iParsedOptions++;

    if ((argc > iParsedOptions) && IS_SUB_OPTION(argv[iParsedOptions])) {
        if (0 == _tcsicmp(argv[iParsedOptions], CMDSTR_HELP)||
            (0 == _tcsicmp(argv[iParsedOptions], CMDSTR_H))) {
            iParsedOptions++;
            help = true;
            return false;
        }
    }
    pCommandStruct = AllocateCommandStruct();
    if (NULL == pCommandStruct) {
        _tprintf(_T("ParseResyncStartNotifyCommand: AllocateCommandStruct failed\n"));
        return false;
    }
    // This option should have another parameter
    pCommandStruct->ulCommand = CMD_RESYNC_START_NOTIFY;
    if (argc <= iParsedOptions) {
        _tprintf(_T("ParseResyncStartNotifyCommand: missed parameter for %s\n"), CMDSTR_RESYNC_START_NOTIFY);
        return false;
    }
    if (!GetVolumeGUID(argv[iParsedOptions], pCommandStruct->data.ResyncStart.VolumeGUID, 
        sizeof(pCommandStruct->data.ResyncStart.VolumeGUID), "ParseResyncStartNotifyCommand",
        CMDSTR_RESYNC_START_NOTIFY))
    {
        return false;
    }

    pCommandStruct->data.ResyncStart.MountPoint = argv[iParsedOptions];
    iParsedOptions++;

    InsertTailList(pCommandLineOptions->CommandList, &pCommandStruct->ListEntry);
    return true;
}

bool
ParseResyncEndNotifyCommand(TCHAR *argv[], int argc, int &iParsedOptions, PCOMMAND_LINE_OPTIONS pCommandLineOptions)
{
    PCOMMAND_STRUCT     pCommandStruct = NULL;

    iParsedOptions++;

    if ((argc > iParsedOptions) && IS_SUB_OPTION(argv[iParsedOptions])) {
        if (0 == _tcsicmp(argv[iParsedOptions], CMDSTR_HELP)||
            (0 == _tcsicmp(argv[iParsedOptions], CMDSTR_H))) {
            iParsedOptions++;
            help = true;
            return false;
        }
    }
    pCommandStruct = AllocateCommandStruct();
    if (NULL == pCommandStruct) {
        _tprintf(_T("ParseResyncEndNotifyCommand: AllocateCommandStruct failed\n"));
        return false;
    }
    // This option should have another parameter
    pCommandStruct->ulCommand = CMD_RESYNC_END_NOTIFY;
    if (argc <= iParsedOptions) {
        _tprintf(_T("ParseResyncEndNotifyCommand: missed parameter for %s\n"), CMDSTR_RESYNC_END_NOTIFY);
        return false;
    }

    if (!GetVolumeGUID(argv[iParsedOptions], pCommandStruct->data.ResyncEnd.VolumeGUID, 
        sizeof(pCommandStruct->data.ResyncEnd.VolumeGUID), "ParseResyncEndNotifyCommand",
        CMDSTR_RESYNC_END_NOTIFY))
    {
        return false;
    }

    pCommandStruct->data.ResyncEnd.MountPoint = argv[iParsedOptions];
    iParsedOptions++;

    InsertTailList(pCommandLineOptions->CommandList, &pCommandStruct->ListEntry);
    return true;
}

bool
ParseSetIoSizeArray(TCHAR *argv[], int argc, int &iParsedOptions, PCOMMAND_LINE_OPTIONS pCommandLineOptions)
{
    PCOMMAND_STRUCT     pCommandStruct = NULL;   
    PTCHAR              MountPoint = NULL ; 

    iParsedOptions++;

    if ((argc > iParsedOptions) && IS_SUB_OPTION(argv[iParsedOptions])) {
        if (0 == _tcsicmp(argv[iParsedOptions], CMDSTR_HELP)||
            (0 == _tcsicmp(argv[iParsedOptions], CMDSTR_H))) {
            iParsedOptions++;
            help = true;
            return false;
        }
    }
    pCommandStruct = AllocateCommandStruct();
    if (NULL == pCommandStruct) {
        _tprintf(_T("ParseSetIoSizeArray: AllocateCommandStruct failed\n"));
        return false;
    }
    // This option should have another parameter
    pCommandStruct->ulCommand = CMD_SET_IO_SIZE_ARRAY;
    if (argc <= iParsedOptions) {
        _tprintf(_T("ParseSetIoSizeArray: missed parameter for %s\n"), CMDSTR_SET_IO_SIZE_ARRAY);
        return false;
    }

    pCommandStruct->data.SetIoSizes.Flags = 0;

    if(IS_SUB_OPTION(argv[iParsedOptions])) {
        if(0 == _tcsicmp(argv[iParsedOptions], CMDSTR_SET_READ_IO_PROFILING)){
            pCommandStruct->data.SetIoSizes.Flags |= SET_IO_SIZE_ARRAY_READ_INPUT;
        }
        else if(0 == _tcsicmp(argv[iParsedOptions], CMDSTR_SET_WRITE_IO_PROFILING)){
                pCommandStruct->data.SetIoSizes.Flags |= SET_IO_SIZE_ARRAY_WRITE_INPUT;
        }
    }

    while ((argc > iParsedOptions) && !IS_COMMAND(argv[iParsedOptions])) {
        if(IS_SUB_OPTION(argv[iParsedOptions])) {
            if(0 == _tcsicmp(argv[iParsedOptions], CMDSTR_SET_READ_IO_PROFILING)){
                pCommandStruct->data.SetIoSizes.Flags |= SET_IO_SIZE_ARRAY_READ_INPUT;
            }
            else if(0 == _tcsicmp(argv[iParsedOptions], CMDSTR_SET_WRITE_IO_PROFILING)){
                    pCommandStruct->data.SetIoSizes.Flags |= SET_IO_SIZE_ARRAY_WRITE_INPUT;
            }
            else 
                break;
        }
        else if (MountPoint == NULL) {
            MountPoint = argv[iParsedOptions];
            if (!GetVolumeGUID(MountPoint, pCommandStruct->data.SetIoSizes.VolumeGUID, 
                sizeof(pCommandStruct->data.SetIoSizes.VolumeGUID), "ParseSetIoSizeArray",
                CMDSTR_SET_IO_SIZE_ARRAY))
            {
                return false;
            }
            pCommandStruct->data.SetIoSizes.MountPoint = MountPoint ;
        } else {
            if (pCommandStruct->data.SetIoSizes.ulNumValues < (USER_MODE_MAX_IO_BUCKETS - 1)) {
                pCommandStruct->data.SetIoSizes.ulIoSizeArray[pCommandStruct->data.SetIoSizes.ulNumValues] = GetSizeFromString(argv[iParsedOptions]);
                pCommandStruct->data.SetIoSizes.ulNumValues++;
            } else {
                _tprintf(_T("ParseSetIoSizeArray: Number of io sizes exceeded %d\n"), USER_MODE_MAX_IO_BUCKETS - 1);
                return false;
            }
        }
        
        iParsedOptions++;
    }

    if (0 == pCommandStruct->data.SetIoSizes.ulNumValues) {
        _tprintf(_T("ParseSetIoSizeArray: no io sizes are passed for %s\n"), CMDSTR_SET_IO_SIZE_ARRAY);
        return false;        
    }

    if(0 == (pCommandStruct->data.SetIoSizes.Flags &  SET_IO_SIZE_ARRAY_READ_INPUT) && 
            0 == (pCommandStruct->data.SetIoSizes.Flags & SET_IO_SIZE_ARRAY_WRITE_INPUT))
    {
        pCommandStruct->data.SetIoSizes.Flags = SET_IO_SIZE_ARRAY_READ_INPUT | SET_IO_SIZE_ARRAY_WRITE_INPUT;
    }

    if (pCommandStruct->data.SetIoSizes.ulNumValues > 0x01) {
        sort(&pCommandStruct->data.SetIoSizes.ulIoSizeArray[0], &pCommandStruct->data.SetIoSizes.ulIoSizeArray[pCommandStruct->data.SetIoSizes.ulNumValues]);
    }
    InsertTailList(pCommandLineOptions->CommandList, &pCommandStruct->ListEntry);
    return true;
}

bool
ParseShutdownNotifyCommand(TCHAR *argv[], int argc, int &iParsedOptions, PCOMMAND_LINE_OPTIONS pCommandLineOptions)
{
    PCOMMAND_STRUCT     pCommandStruct = NULL;

    UNREFERENCED_PARAMETER(argc);
    UNREFERENCED_PARAMETER(argv);

    iParsedOptions++;
    if ((argc > iParsedOptions) && IS_SUB_OPTION(argv[iParsedOptions])) {
        if ((0 == _tcsicmp(argv[iParsedOptions], CMDSTR_HELP))||
            (0 == _tcsicmp(argv[iParsedOptions], CMDSTR_H))){
                iParsedOptions++;
                help = true;
                return false;
            }
    }
    pCommandLineOptions->bSendShutdownNotification = true;

    pCommandStruct = AllocateCommandStruct();
    if (NULL == pCommandStruct) {
        _tprintf(_T("ParseCheckShutdownStatusCommand: AllocateCommandStruct failed\n"));
        return false;
    }
    pCommandStruct->ulCommand = CMD_SHUTDOWN_NOTIFY;

    InsertTailList(pCommandLineOptions->CommandList, &pCommandStruct->ListEntry);

    if ((argc > iParsedOptions) && IS_SUB_OPTION(argv[iParsedOptions])) {
        if (0 == _tcsicmp(argv[iParsedOptions], CMDOPTSTR_DISABLE_DATA_MODE)) {
            iParsedOptions++;
            pCommandLineOptions->bDisableDataFiltering = true;
        } else if (0 == _tcsicmp(argv[iParsedOptions], CMDOPTSTR_DISABLE_DATA_FILES)) {
            iParsedOptions++;
            pCommandLineOptions->bDisableDataFiles = true;
        }
    }
    return true;
}

bool
ParseGetDriverVersionCommand(TCHAR *argv[], int argc, int &iParsedOptions, PCOMMAND_LINE_OPTIONS pCommandLineOptions)
{
    PCOMMAND_STRUCT     pCommandStruct = NULL;

    UNREFERENCED_PARAMETER(argc);
    UNREFERENCED_PARAMETER(argv);

    iParsedOptions++;

    if ((argc > iParsedOptions) && IS_SUB_OPTION(argv[iParsedOptions])) {
        if (0 == _tcsicmp(argv[iParsedOptions], CMDSTR_HELP)||
            (0 == _tcsicmp(argv[iParsedOptions], CMDSTR_H))) {
            iParsedOptions++;
            help = true;
            return false;
        }
    }

    pCommandStruct = AllocateCommandStruct();
    if (NULL == pCommandStruct) {
        _tprintf(_T("ParseGetDriverVesionCommand: AllocateCommandStruct failed\n"));
        return false;
    }

    pCommandStruct->ulCommand = CMD_GET_DRIVER_VERSION;
    InsertTailList(pCommandLineOptions->CommandList, &pCommandStruct->ListEntry);
    return true;
}

bool
ParseVerboseCommand(TCHAR *argv[], int argc, int &iParsedOptions, PCOMMAND_LINE_OPTIONS pCommandLineOptions)
{

    UNREFERENCED_PARAMETER(argc);
    UNREFERENCED_PARAMETER(argv);

    iParsedOptions++;

    if ((argc > iParsedOptions) && IS_SUB_OPTION(argv[iParsedOptions])) {
        if (0 == _tcsicmp(argv[iParsedOptions], CMDSTR_HELP)||
            (0 == _tcsicmp(argv[iParsedOptions], CMDSTR_H))) {
            iParsedOptions++;
            help = true;
            return false;
        }
    }
    return true;
}

bool
ParseServiceStartNotifyCommand(TCHAR *argv[], int argc, int &iParsedOptions, PCOMMAND_LINE_OPTIONS pCommandLineOptions)
{
    PCOMMAND_STRUCT     pCommandStruct = NULL;

    UNREFERENCED_PARAMETER(argc);
    UNREFERENCED_PARAMETER(argv);

    iParsedOptions++;

    if ((argc > iParsedOptions) && IS_SUB_OPTION(argv[iParsedOptions])) {
        if (0 == _tcsicmp(argv[iParsedOptions], CMDSTR_HELP)||
            (0 == _tcsicmp(argv[iParsedOptions], CMDSTR_H))) {
            iParsedOptions++;
            help = true;
            return false;
        }
    }

    pCommandLineOptions->bSendServiceStartNotification = true;

    pCommandStruct = AllocateCommandStruct();
    if (NULL == pCommandStruct) {
        _tprintf(_T("ParseServiceStartNotifyCommand: AllocateCommandStruct failed\n"));
        return false;
    }
    pCommandStruct->ulCommand = CMD_SERVICE_START_NOTIFY;
    pCommandStruct->data.ServiceStart.bDisableDataFiles = false;

    if ((argc > iParsedOptions) && IS_SUB_OPTION(argv[iParsedOptions])) {
        if (0 == _tcsicmp(argv[iParsedOptions], CMDOPTSTR_DISABLE_DATA_FILES)) {
            iParsedOptions++;
            pCommandStruct->data.ServiceStart.bDisableDataFiles = true;
        }
    }

    InsertTailList(pCommandLineOptions->CommandList, &pCommandStruct->ListEntry);

    return true;
}

bool
ParseCreateThreadCommand(TCHAR *argv[], int argc, int &iParsedOptions, PCOMMAND_LINE_OPTIONS pCommandLineOptions)
{
    PCOMMAND_STRUCT     pCommandStruct = NULL;
    size_t              stIdLength = 0;
    iParsedOptions++;

    if ((argc > iParsedOptions) && IS_SUB_OPTION(argv[iParsedOptions])) {
        if (0 == _tcsicmp(argv[iParsedOptions], CMDSTR_HELP)||
            (0 == _tcsicmp(argv[iParsedOptions], CMDSTR_H))) {
            iParsedOptions++;
            help = true;
            return false;
        }
    }
    if (argc <= iParsedOptions) {
        _tprintf(_T("ParseCreateThreadCommand: missed parameter for %s\n"), CMDSTR_CREATE_THREAD);
        return false;
    }

    if (IS_NOT_COMMAND_PAR(argv[iParsedOptions])) {
        _tprintf(_T("ParseCreateThreadCommand: missed parameter for %s\n"), CMDSTR_CREATE_THREAD);
        return false;
    }

    pCommandStruct = AllocateCommandStruct();
    if (NULL == pCommandStruct) {
        _tprintf(_T("ParseCreateThreadCommand: AllocateCommandStruct failed\n"));
        return false;
    }
    // This option should have another parameter
    pCommandStruct->ulCommand = CMD_CREATE_THREAD;
    
    stIdLength = _tcslen(argv[iParsedOptions])+ 1;
    pCommandStruct->data.CreateThreadData.ThreadId = (PTCHAR) malloc(sizeof(TCHAR) * stIdLength);
	inm_memcpy_s(pCommandStruct->data.CreateThreadData.ThreadId, stIdLength * sizeof(TCHAR), argv[iParsedOptions], stIdLength * sizeof(TCHAR));

    pCommandStruct->data.CreateThreadData.ListHead = new LIST_ENTRY;
    InitializeListHead(pCommandStruct->data.CreateThreadData.ListHead);

    iParsedOptions++;

    InsertTailList(pCommandLineOptions->CommandList, &pCommandStruct->ListEntry);
    //push head of current list on top of stack
    
    PCOMMAND_LIST_ENTRY_NODE ListNode = new COMMAND_LIST_ENTRY_NODE;
    InitializeListHead(&ListNode->ListEntry);
    ListNode->ListHead = pCommandLineOptions->CommandList;
    ListNode->CurrentThreadId = (TString) pCommandStruct->data.CreateThreadData.ThreadId;
    InsertHeadList(pCommandLineOptions->ThreadCmdStack, &ListNode->ListEntry);

    //Change list head to new list head
    pCommandLineOptions->CommandList = pCommandStruct->data.CreateThreadData.ListHead;
    return true;
}

bool
ParseEndOfThreadCommand(TCHAR *argv[], int argc, int &iParsedOptions, PCOMMAND_LINE_OPTIONS pCommandLineOptions)
{
    PCOMMAND_STRUCT     pCommandStruct = NULL;
    size_t              stIdLength = 0;
    iParsedOptions++;

    if ((argc > iParsedOptions) && IS_SUB_OPTION(argv[iParsedOptions])) {
        if (0 == _tcsicmp(argv[iParsedOptions], CMDSTR_HELP)||
            (0 == _tcsicmp(argv[iParsedOptions], CMDSTR_H))) {
            iParsedOptions++;
            help = true;
            return false;
        }
    }
    if (argc <= iParsedOptions) {
        _tprintf(_T("ParseEndOfThreadCommand: missed parameter for %s\n"), CMDSTR_END_OF_THREAD);
        return false;
    }

    if (IS_NOT_COMMAND_PAR(argv[iParsedOptions])) {
        _tprintf(_T("ParseEndOfThreadCommand: missed parameter for %s\n"), CMDSTR_END_OF_THREAD);
        return false;
    }

    pCommandStruct = AllocateCommandStruct();
    if (NULL == pCommandStruct) {
        _tprintf(_T("ParseEndOfThreadCommand: AllocateCommandStruct failed\n"));
        return false;
    }
    // This option should have another parameter
    pCommandStruct->ulCommand = CMD_END_OF_THREAD;
    
    if(IsListEmpty(pCommandLineOptions->ThreadCmdStack)){
        _tprintf(_T("ParseEndOfThreadCommand: Start of thread does not match end of thread\n"));
        FreeCommandStruct(pCommandStruct);
        return false;
    }

    //pop top of stack 
    PCOMMAND_LIST_ENTRY_NODE pNode = (PCOMMAND_LIST_ENTRY_NODE)RemoveHeadList(pCommandLineOptions->ThreadCmdStack);
    if(0 != _tcsicmp(pNode->CurrentThreadId.tc_str(), argv[iParsedOptions]))
    {
        _tprintf(_T("ParseEndOfThreadCommand: Thread id of EndOfThread does not match with the ThreadId of CreateThread\n"));
        FreeCommandStruct(pCommandStruct);
        return false;
    }

    stIdLength = _tcslen(argv[iParsedOptions])+ 1;
    pCommandStruct->data.EndOfThreadData.ThreadId = (PTCHAR) malloc(sizeof(TCHAR) * stIdLength);
	inm_memcpy_s(pCommandStruct->data.EndOfThreadData.ThreadId, stIdLength * sizeof(TCHAR), argv[iParsedOptions], stIdLength * sizeof(TCHAR));

    iParsedOptions++;

    InsertTailList(pCommandLineOptions->CommandList, &pCommandStruct->ListEntry);

    //restore the list head
    pCommandLineOptions->CommandList = pNode->ListHead;

    delete pNode;
    return true;
}

bool
ParseWaitForThreadCommand(TCHAR *argv[], int argc, int &iParsedOptions, PCOMMAND_LINE_OPTIONS pCommandLineOptions)
{
    PCOMMAND_STRUCT     pCommandStruct = NULL;
    iParsedOptions++;

    if ((argc > iParsedOptions) && IS_SUB_OPTION(argv[iParsedOptions])) {
        if (0 == _tcsicmp(argv[iParsedOptions], CMDSTR_HELP)||
            (0 == _tcsicmp(argv[iParsedOptions], CMDSTR_H))) {
            iParsedOptions++;
            help = true;
            return false;
        }
    }
    if (argc <= iParsedOptions) {
        _tprintf(_T("ParseWaitForThreadCommand: missed parameter for %s\n"), CMDSTR_WAIT_FOR_THREAD);
        return false;
    }

    pCommandStruct = AllocateCommandStruct();
    if (NULL == pCommandStruct) {
        _tprintf(_T("ParseWaitForThreadCommand: AllocateCommandStruct failed\n"));
        return false;
    }
    // This option should have another parameter
    pCommandStruct->ulCommand = CMD_WAIT_FOR_THREAD;
    
    pCommandStruct->data.WaitForThreadData.pvThreadId = new vector<PTCHAR>;
    while(iParsedOptions < argc && IS_COMMAND_PAR(argv[iParsedOptions])) {
        PTCHAR ThreadId = NULL;
        size_t stIdLength = 0;

        stIdLength = _tcslen(argv[iParsedOptions])+ 1;
        ThreadId = (PTCHAR) malloc(sizeof(TCHAR) * stIdLength);
		inm_memcpy_s(ThreadId, stIdLength * sizeof(TCHAR), argv[iParsedOptions], stIdLength * sizeof(TCHAR));
        pCommandStruct->data.WaitForThreadData.pvThreadId->push_back(ThreadId);
        iParsedOptions++;
    }

    if(pCommandStruct->data.WaitForThreadData.pvThreadId->size() == 0){
        _tprintf(_T("ParseWaitForThreadCommand: Argument thread id missing for CMD_WAIT_FOR_THREAD\n"));
        FreeCommandStruct(pCommandStruct);
        return false;
    }

    InsertTailList(pCommandLineOptions->CommandList, &pCommandStruct->ListEntry);
    return true;
}

void
InitializeWriteStreamOptions(WRITE_STREAM_DATA &WriteStreamData)
{
    WriteStreamData.MountPoint = NULL;
    WriteStreamData.nMaxIOs = 0;
    WriteStreamData.nMaxIOSz = 0;
    WriteStreamData.nMinIOSz = 0;
    WriteStreamData.nSecsToRun = 0;
    WriteStreamData.nWritesPerTag = 0;
}

bool
ParseWriteStreamCommand(TCHAR *argv[], int argc, int &iParsedOptions, PCOMMAND_LINE_OPTIONS pCommandLineOptions)
{
    PCOMMAND_STRUCT     pCommandStruct = NULL;

    iParsedOptions++;

    if (argc <= iParsedOptions) {
        _tprintf(_T("ParseWriteStreamCommand: missed parameter for CMD_WRITE_STREAM\n"));
        return false;
    }

    pCommandStruct = AllocateCommandStruct();
    if (NULL == pCommandStruct) {
        _tprintf(_T("ParseWriteStreamCommand: AllocateCommandStruct failed\n"));
        return false;
    }
    // This option should have another parameter
    pCommandStruct->ulCommand = CMD_WRITE_STREAM;
    
    if (!GetVolumeGUID(argv[iParsedOptions], pCommandStruct->data.WriteStreamData.VolumeGUID, 
        sizeof(pCommandStruct->data.WriteStreamData.VolumeGUID), "ParseWriteStreamCommand",
        CMDSTR_WRITE_STREAM))
    {
        return false;
    }

    
    InitializeWriteStreamOptions(pCommandStruct->data.WriteStreamData);
    pCommandStruct->data.WriteStreamData.MountPoint = argv[iParsedOptions];

    iParsedOptions++;
    while((argc > iParsedOptions) && IS_SUB_OPTION(argv[iParsedOptions])) {
        if (0 == _tcsicmp(argv[iParsedOptions], CMDOPTSTR_MAX_IO_COUNT)) {
            iParsedOptions++;
            pCommandStruct->data.WriteStreamData.nMaxIOs = _ttoi(argv[iParsedOptions]);
        } else if (0 == _tcsicmp(argv[iParsedOptions], CMDOPTSTR_MIN_IO_SIZE)) {
            iParsedOptions++;
            pCommandStruct->data.WriteStreamData.nMinIOSz = _ttoi(argv[iParsedOptions]);
        } else if (0 == _tcsicmp(argv[iParsedOptions], CMDOPTSTR_MAX_IO_SIZE)) {
            iParsedOptions++;
            pCommandStruct->data.WriteStreamData.nMaxIOSz = _ttoi(argv[iParsedOptions]);
        } else if (0 == _tcsicmp(argv[iParsedOptions], CMDOPTSTR_SECONDS_TO_RUN)) {
            iParsedOptions++;
            pCommandStruct->data.WriteStreamData.nSecsToRun = _ttoi(argv[iParsedOptions]);
        } else if (0 == _tcsicmp(argv[iParsedOptions], CMDOPTSTR_WRITES_PER_TAG)) {
            iParsedOptions++;
            pCommandStruct->data.WriteStreamData.nWritesPerTag = _ttoi(argv[iParsedOptions]);
        }
        iParsedOptions++;
    }

    InsertTailList(pCommandLineOptions->CommandList, &pCommandStruct->ListEntry);
    return true;
}

bool
ParseReadStreamCommand(TCHAR *argv[], int argc, int &iParsedOptions, PCOMMAND_LINE_OPTIONS pCommandLineOptions)
{
    PCOMMAND_STRUCT     pCommandStruct = NULL;

    UNREFERENCED_PARAMETER(argc);
    UNREFERENCED_PARAMETER(argv);

    iParsedOptions++;

    pCommandStruct = AllocateCommandStruct();
    if (NULL == pCommandStruct) {
        _tprintf(_T("ParseReadStreamCommand: AllocateCommandStruct failed\n"));
        return false;
    }
    pCommandStruct->ulCommand = CMD_READ_STREAM;

    if (!GetVolumeGUID(argv[iParsedOptions], pCommandStruct->data.ReadStreamData.VolumeGUID, 
        sizeof(pCommandStruct->data.ReadStreamData.VolumeGUID), "ParseReadStreamCommand",
        CMDSTR_READ_STREAM))
    {
        return false;
    }

    pCommandStruct->data.ReadStreamData.MountPoint = argv[iParsedOptions];    

    InsertTailList(pCommandLineOptions->CommandList, &pCommandStruct->ListEntry);
    iParsedOptions++;
    return true;
}

bool
ParseCheckShutdownStatusCommand(TCHAR *argv[], int argc, int &iParsedOptions, PCOMMAND_LINE_OPTIONS pCommandLineOptions)
{
    PCOMMAND_STRUCT     pCommandStruct = NULL;

    UNREFERENCED_PARAMETER(argc);
    UNREFERENCED_PARAMETER(argv);

    iParsedOptions++;

    if ((argc > iParsedOptions) && IS_SUB_OPTION(argv[iParsedOptions])) {
        if (0 == _tcsicmp(argv[iParsedOptions], CMDSTR_HELP)||
            (0 == _tcsicmp(argv[iParsedOptions], CMDSTR_H))) {
            iParsedOptions++;
            help = true;
            return false;
        }
    }

    if (false == pCommandLineOptions->bSendShutdownNotification) {
        _tprintf(_T("ParseCheckShutdownStatusCommand: %s command without option %s\n"), 
            CMDSTR_CHECK_SHUTDOWN_STATUS, CMDSTR_SHUTDOWN_NOTIFY);    
        return false;
    }
    pCommandStruct = AllocateCommandStruct();
    if (NULL == pCommandStruct) {
        _tprintf(_T("ParseCheckShutdownStatusCommand: AllocateCommandStruct failed\n"));
        return false;
    }

    pCommandStruct->ulCommand = CMD_CHECK_SHUTDOWN_STATUS;
    InsertTailList(pCommandLineOptions->CommandList, &pCommandStruct->ListEntry);
    return true;
}

bool
ParseCheckServiceStartStatusCommand(TCHAR *argv[], int argc, int &iParsedOptions, PCOMMAND_LINE_OPTIONS pCommandLineOptions)
{
    PCOMMAND_STRUCT     pCommandStruct = NULL;

    UNREFERENCED_PARAMETER(argc);
    UNREFERENCED_PARAMETER(argv);

    iParsedOptions++;

    if ((argc > iParsedOptions) && IS_SUB_OPTION(argv[iParsedOptions])) {
        if (0 == _tcsicmp(argv[iParsedOptions], CMDSTR_HELP)||
            (0 == _tcsicmp(argv[iParsedOptions], CMDSTR_H))) {
            iParsedOptions++;
            help = true;
            return false;
        }
    }

    if (false == pCommandLineOptions->bSendServiceStartNotification) {
        _tprintf(_T("ParseCheckServiceStartStatusCommand: %s command without option %s\n"), 
            CMDSTR_CHECK_PROCESS_START_STATUS, CMDSTR_PROCESS_START_NOTIFY);    
        return false;
    }

    pCommandStruct = AllocateCommandStruct();
    if (NULL == pCommandStruct) {
        _tprintf(_T("ParseCheckServiceStartStatusCommand: AllocateCommandStruct failed\n"));
        return false;
    }

    pCommandStruct->ulCommand = CMD_CHECK_SERVICE_START_STATUS;
    InsertTailList(pCommandLineOptions->CommandList, &pCommandStruct->ListEntry);
    return true;
}


bool
ParseGetVolumeSizeCommand(TCHAR *argv[], int argc, int &iParsedOptions, PCOMMAND_LINE_OPTIONS pCommandLineOptions)
{
    PCOMMAND_STRUCT     pCommandStruct = NULL;

    iParsedOptions++;

    if ((argc > iParsedOptions) && IS_SUB_OPTION(argv[iParsedOptions])) {
        if (0 == _tcsicmp(argv[iParsedOptions], CMDSTR_HELP)||
            (0 == _tcsicmp(argv[iParsedOptions], CMDSTR_H))) {
            iParsedOptions++;
            help = true;
            return false;
        }
    }
    pCommandStruct = AllocateCommandStruct();
    if (NULL == pCommandStruct) {
        _tprintf(_T("ParsePrintStatisticsCommand: AllocateCommandStruct failed\n"));
        return false;
    }

    pCommandStruct->ulCommand = CMD_GET_VOLUME_SIZE;

    pCommandStruct->data.GetVolumeSize_v2.MountPoint = NULL;
    if ((argc <= iParsedOptions) || IS_NOT_COMMAND_PAR(argv[iParsedOptions])) {
        // no volume letter specified
        _tprintf(_T("ParseGetVolumeSizeCommand: missing MountPoint/DriveLetter for Command %s\n"), 
            CMDSTR_GET_VOLUME_SIZE);
        return false;
    }
	
	size_t len = wcslen(argv[iParsedOptions]) * sizeof(WCHAR);
	// Allocate pointer with required size
	pCommandStruct->data.GetVolumeSize_v2.VolumeGUID = (WCHAR*) malloc(wcslen(argv[iParsedOptions]) * sizeof(WCHAR));
		
	if (NULL == pCommandStruct->data.GetVolumeSize_v2.VolumeGUID)
		return false;
	else
		_tprintf(_T("size of id : %d \n"), (wcslen(argv[iParsedOptions]) * sizeof(WCHAR)));

	/*if (!GetVolumeGUID(argv[iParsedOptions], pCommandStruct->data.GetVolumeSize_v2.VolumeGUID,
		(wcslen(argv[iParsedOptions]) * sizeof(WCHAR), "ParseGetVolumeSizeCommand",
		CMDSTR_GET_VOLUME_SIZE))
		{
		return false;
		}*/
	pCommandStruct->data.GetVolumeSize_v2.VolumeGUID = argv[iParsedOptions];
	pCommandStruct->data.GetVolumeSize_v2.inputlen = (DWORD)(len + 2);

    if (sCommandOptions.bVerbose)
    _tprintf(_T("GetVolumeGUID: Source Disk Name %S"), pCommandStruct->data.GetVolumeSize_v2.VolumeGUID);
    pCommandStruct->data.GetVolumeSize_v2.MountPoint = argv[iParsedOptions];
    iParsedOptions++;
    InsertTailList(pCommandLineOptions->CommandList, &pCommandStruct->ListEntry);
    return true;

}

bool
ParseGetVolumeFlagsCommand(TCHAR *argv[], int argc, int &iParsedOptions, PCOMMAND_LINE_OPTIONS pCommandLineOptions)
{
    PCOMMAND_STRUCT     pCommandStruct = NULL;

    iParsedOptions++;

    if ((argc > iParsedOptions) && IS_SUB_OPTION(argv[iParsedOptions])) {
        if (0 == _tcsicmp(argv[iParsedOptions], CMDSTR_HELP)||
            (0 == _tcsicmp(argv[iParsedOptions], CMDSTR_H))) {
            iParsedOptions++;
            help = true;
            return false;
        }
    }
    pCommandStruct = AllocateCommandStruct();
    if (NULL == pCommandStruct) {
        _tprintf(_T("ParsePrintStatisticsCommand: AllocateCommandStruct failed\n"));
        return false;
    }

    pCommandStruct->ulCommand = CMD_GET_VOLUME_FLAGS;

    pCommandStruct->data.GetVolumeFlags.MountPoint = NULL;
    if ((argc <= iParsedOptions) || IS_NOT_COMMAND_PAR(argv[iParsedOptions])) {
        // no volume letter specified
        _tprintf(_T("ParseGetVolumeFlagsCommand: missing MountPoint/DriveLetter for Command %s\n"), 
            CMDSTR_GET_VOLUME_FLAGS);
         return false;
    }

    if (!GetVolumeGUID(argv[iParsedOptions], pCommandStruct->data.GetVolumeFlags.VolumeGUID, 
        sizeof(pCommandStruct->data.GetVolumeFlags.VolumeGUID), "ParseGetVolumeFlagsCommand",
        CMDSTR_GET_VOLUME_FLAGS))
    {
        return false;
    }

    pCommandStruct->data.GetVolumeFlags.MountPoint = argv[iParsedOptions];
    iParsedOptions++;
    InsertTailList(pCommandLineOptions->CommandList, &pCommandStruct->ListEntry);
    return true;

}



bool
ParseGetWriteOrderStateCommand(TCHAR *argv[], int argc, int &iParsedOptions, PCOMMAND_LINE_OPTIONS pCommandLineOptions)
{
    PCOMMAND_STRUCT     pCommandStruct = NULL;

    iParsedOptions++;

    if ((argc > iParsedOptions) && IS_SUB_OPTION(argv[iParsedOptions])) {
        if (0 == _tcsicmp(argv[iParsedOptions], CMDSTR_HELP)||
            (0 == _tcsicmp(argv[iParsedOptions], CMDSTR_H))) {
            iParsedOptions++;
            help = true;
            return false;
        }
    }
    pCommandStruct = AllocateCommandStruct();
    if (NULL == pCommandStruct) {
        _tprintf(_T("ParseGetWriteOrderStateCommand: AllocateCommandStruct failed\n"));
        return false;
    }

    pCommandStruct->ulCommand = CMD_GET_WRITE_ORDER_STATE;

    pCommandStruct->data.GetFilteringState.MountPoint = NULL;
    if ((argc <= iParsedOptions) || IS_NOT_COMMAND_PAR(argv[iParsedOptions])) {
        // no volume letter specified
        _tprintf(_T("ParseGetWriteOrderStateCommand: missing MountPoint/DriveLetter for Command %s\n"), 
            CMDSTR_GET_VOLUME_WRITE_ORDER_STATE);
         return false;
    }

    if (!GetVolumeGUID(argv[iParsedOptions], pCommandStruct->data.GetFilteringState.VolumeGUID, 
        sizeof(pCommandStruct->data.GetFilteringState.VolumeGUID), "ParseGetWriteOrderStateCommand",
        CMDSTR_GET_VOLUME_WRITE_ORDER_STATE))
    {
        return false;
    }

    pCommandStruct->data.GetFilteringState.MountPoint = argv[iParsedOptions];
    iParsedOptions++;
    InsertTailList(pCommandLineOptions->CommandList, &pCommandStruct->ListEntry);
    return true;

}


bool
ParsePrintStatisticsCommand(TCHAR *argv[], int argc, int &iParsedOptions, PCOMMAND_LINE_OPTIONS pCommandLineOptions)
{
    PCOMMAND_STRUCT     pCommandStruct = NULL;

    iParsedOptions++;

    if ((argc > iParsedOptions) && IS_SUB_OPTION(argv[iParsedOptions])) {
        if (0 == _tcsicmp(argv[iParsedOptions], CMDSTR_HELP)||
            (0 == _tcsicmp(argv[iParsedOptions], CMDSTR_H))) {
            iParsedOptions++;
            help = true;
            return false;
        }
    }

    pCommandStruct = AllocateCommandStruct();
    if (NULL == pCommandStruct) {
        _tprintf(_T("ParsePrintStatisticsCommand: AllocateCommandStruct failed\n"));
        return false;
    }
    pCommandStruct->ulCommand = CMD_PRINT_STATISTICS;

    // This command has optional drive letter as a parameter.
    pCommandStruct->data.PrintStatisticsData.MountPoint = NULL;
    if ((argc <= iParsedOptions) || IS_NOT_COMMAND_PAR(argv[iParsedOptions])) {
        // no volume letter specified
        InsertTailList(pCommandLineOptions->CommandList, &pCommandStruct->ListEntry);
        return true;
    }

    if (!GetVolumeGUID(argv[iParsedOptions], pCommandStruct->data.PrintStatisticsData.VolumeGUID, 
        sizeof(pCommandStruct->data.PrintStatisticsData.VolumeGUID), "ParsePrintStatisticsCommand",
        CMDSTR_PRINT_STATISTICS))
    {
        return false;
    }

    pCommandStruct->data.PrintStatisticsData.MountPoint = argv[iParsedOptions];

    iParsedOptions++;
    InsertTailList(pCommandLineOptions->CommandList, &pCommandStruct->ListEntry);
    return true;
}

bool
ParsePrintAdditionalStatsCommand(TCHAR *argv[], int argc, int &iParsedOptions, PCOMMAND_LINE_OPTIONS pCommandLineOptions)
{
    PCOMMAND_STRUCT     pCommandStruct = NULL;

    iParsedOptions++;

    if ((argc > iParsedOptions) && IS_SUB_OPTION(argv[iParsedOptions])) {
        if (0 == _tcsicmp(argv[iParsedOptions], CMDSTR_HELP)||
            (0 == _tcsicmp(argv[iParsedOptions], CMDSTR_H))) {
            iParsedOptions++;
            help = true;
            return false;
        }
    }
    pCommandStruct = AllocateCommandStruct();
    if (NULL == pCommandStruct) {
        _tprintf(_T("ParsePrintAdditionalStatsCommand: AllocateCommandStruct failed\n"));
        return false;
    }

    pCommandStruct->ulCommand = CMD_GET_ADDITIONAL_STATS_INFO;

    pCommandStruct->data.AdditionalStatsInfo.MountPoint = NULL;
    if ((argc <= iParsedOptions) || IS_NOT_COMMAND_PAR(argv[iParsedOptions])) {
        // no volume letter specified
        _tprintf(_T("ParsePrintAdditionalStatsCommand: missing MountPoint/DriveLetter for Command %s\n"), 
            CMDSTR_PRINT_ADDITIONAL_STATS);
         return false;
    }

    if (!GetVolumeGUID(argv[iParsedOptions], pCommandStruct->data.AdditionalStatsInfo.VolumeGUID, 
        sizeof(pCommandStruct->data.AdditionalStatsInfo.VolumeGUID), "ParsePrintAdditionalStatsCommand",
        CMDSTR_PRINT_ADDITIONAL_STATS))
    {
        return false;
    }

    pCommandStruct->data.AdditionalStatsInfo.MountPoint = argv[iParsedOptions];
    iParsedOptions++;
    InsertTailList(pCommandLineOptions->CommandList, &pCommandStruct->ListEntry);
    return true;

}
bool
ParsePrintGlobaTSSNCommand(TCHAR *argv[], int argc, int &iParsedOptions, PCOMMAND_LINE_OPTIONS pCommandLineOptions)
{
    PCOMMAND_STRUCT     pCommandStruct = NULL;

    UNREFERENCED_PARAMETER(argc);
    UNREFERENCED_PARAMETER(argv);

    iParsedOptions++;

    if ((argc > iParsedOptions) && IS_SUB_OPTION(argv[iParsedOptions])) {
        if (0 == _tcsicmp(argv[iParsedOptions], CMDSTR_HELP)||
            (0 == _tcsicmp(argv[iParsedOptions], CMDSTR_H))) {
            iParsedOptions++;
            help = true;
            return false;
        }
    }

    pCommandStruct = AllocateCommandStruct();
    if (NULL == pCommandStruct) {
        _tprintf(_T("ParsePrintGlobaTSSNCommand: AllocateCommandStruct failed\n"));
        return false;
    }

    pCommandStruct->ulCommand = CMD_GET_GLOBAL_TSSN;
    InsertTailList(pCommandLineOptions->CommandList, &pCommandStruct->ListEntry);
    return true;

}

bool
ParseGetDataModeCommand(TCHAR *argv[], int argc, int &iParsedOptions, PCOMMAND_LINE_OPTIONS pCommandLineOptions)
{
    PCOMMAND_STRUCT     pCommandStruct = NULL;

    iParsedOptions++;

    if ((argc > iParsedOptions) && IS_SUB_OPTION(argv[iParsedOptions])) {
        if (0 == _tcsicmp(argv[iParsedOptions], CMDSTR_HELP)||
            (0 == _tcsicmp(argv[iParsedOptions], CMDSTR_H))) {
            iParsedOptions++;
            help = true;
            return false;
        }
    }
    
    pCommandStruct = AllocateCommandStruct();
    if (NULL == pCommandStruct) {
        _tprintf(_T("ParseGetDataModeCommand: AllocateCommandStruct failed\n"));
        return false;
    }
    pCommandStruct->ulCommand = CMD_GET_DM;

    // This command has optional drive letter as a parameter.
    pCommandStruct->data.DataModeData.MountPoint = NULL;
    if ((argc <= iParsedOptions) || IS_NOT_COMMAND_PAR(argv[iParsedOptions])) {
        // no volume letter specified
        InsertTailList(pCommandLineOptions->CommandList, &pCommandStruct->ListEntry);
        return true;
    }

    if (!GetVolumeGUID(argv[iParsedOptions], pCommandStruct->data.DataModeData.VolumeGUID, 
        sizeof(pCommandStruct->data.DataModeData.VolumeGUID), "ParseGetDataModeCommand",
        CMDSTR_GET_DATA_MODE))
    {
        return false;
    }

    pCommandStruct->data.DataModeData.MountPoint = argv[iParsedOptions];
    iParsedOptions++;
    InsertTailList(pCommandLineOptions->CommandList, &pCommandStruct->ListEntry);
    return true;
}

bool
ParseClearStatisticsCommand(TCHAR *argv[], int argc, int &iParsedOptions, PCOMMAND_LINE_OPTIONS pCommandLineOptions)
{
    PCOMMAND_STRUCT     pCommandStruct = NULL;

    iParsedOptions++;

    if ((argc > iParsedOptions) && IS_SUB_OPTION(argv[iParsedOptions])) {
        if (0 == _tcsicmp(argv[iParsedOptions], CMDSTR_HELP)||
            (0 == _tcsicmp(argv[iParsedOptions], CMDSTR_H))) {
            iParsedOptions++;
            help = true;
            return false;
        }
    }
    pCommandStruct = AllocateCommandStruct();
    if (NULL == pCommandStruct) {
        _tprintf(_T("ParseClearStatisticsCommand: AllocateCommandStruct failed\n"));
        return false;
    }
    pCommandStruct->ulCommand = CMD_CLEAR_STATSTICS;

    // This command has optional drive letter as a parameter.
pCommandStruct->data.ClearStatisticsData.MountPoint = NULL;
    if (argc <= iParsedOptions) {
        // no volume letter specified
        InsertTailList(pCommandLineOptions->CommandList, &pCommandStruct->ListEntry);
        return true;
    }

    if (!GetVolumeGUID(argv[iParsedOptions], pCommandStruct->data.ClearStatisticsData.VolumeGUID, 
        sizeof(pCommandStruct->data.ClearStatisticsData.VolumeGUID), "ParseClearStatisticsCommand",
        CMDSTR_CLEAR_STATISTICS))
    {
        return false;
    }

    pCommandStruct->data.ClearStatisticsData.MountPoint = argv[iParsedOptions];

    iParsedOptions++;
    InsertTailList(pCommandLineOptions->CommandList, &pCommandStruct->ListEntry);
    return true;
}

bool
ParseWaitTimeCommand(TCHAR *argv[], int argc, int &iParsedOptions, PCOMMAND_LINE_OPTIONS pCommandLineOptions)
{
    PCOMMAND_STRUCT     pCommandStruct = NULL;

    iParsedOptions++;

    if ((argc > iParsedOptions) && IS_SUB_OPTION(argv[iParsedOptions])) {
        if (0 == _tcsicmp(argv[iParsedOptions], CMDSTR_HELP)||
            (0 == _tcsicmp(argv[iParsedOptions], CMDSTR_H))) {
            iParsedOptions++;
            help = true;
            return false;
        }
    }
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
    pCommandStruct->data.TimeOut.ulTimeout = _tcstoul(argv[iParsedOptions], NULL, 0);
    if (0 == pCommandStruct->data.TimeOut.ulTimeout) {
        _tprintf(_T("ParseWaitTimeCommand: Invalid parameter %s for CMD_WAIT_TIME"), argv[iParsedOptions]);
        return false;
    }
    iParsedOptions++;

    InsertTailList(pCommandLineOptions->CommandList, &pCommandStruct->ListEntry);
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

    if ((argc > iParsedOptions) && IS_SUB_OPTION(argv[iParsedOptions])) {
        if (0 == _tcsicmp(argv[iParsedOptions], CMDSTR_HELP)||
            (0 == _tcsicmp(argv[iParsedOptions], CMDSTR_H))) {
            iParsedOptions++;
            help = true;
            return false;
        }
    }
    pCommandStruct = AllocateCommandStruct();
    if (NULL == pCommandStruct) {
        _tprintf(_T("ParseSetDebugInfoCommand: AllocateCommandStruct failed\n"));
        return false;
    }

    pCommandStruct->ulCommand = CMD_SET_DEBUG_INFO;

    while((argc > iParsedOptions) && IS_SUB_OPTION(argv[iParsedOptions])) {
        if (0 == _tcsicmp(argv[iParsedOptions], CMDOPTSTR_RESET_FLAGS)) {
            iParsedOptions++;
            pCommandStruct->data.DebugInfoData.DebugInfo.ulDebugInfoFlags |= DEBUG_INFO_FLAGS_RESET_MODULES;
        } else if (0 == _tcsicmp(argv[iParsedOptions], CMDOPTSTR_DEBUG_LEVEL_FORALL)) {
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
        } else if (0 == _tcsicmp(argv[iParsedOptions], CMDOPTSTR_DEBUG_LEVEL)) {
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
        } else if (0 == _tcsicmp(argv[iParsedOptions], CMDOPTSTR_DEBUG_MODULES)) {
            iParsedOptions++;
            if (false == GetDebugModules(argv, argc, iParsedOptions, pCommandStruct->data.DebugInfoData.DebugInfo.ulDebugModules))
                return false;
        }
    }

    InsertTailList(pCommandLineOptions->CommandList, &pCommandStruct->ListEntry);
    return true;
}
/*
DBF_Support : This command needs support for accepting the disk signature, for now let's use GUID only
*/
bool ParseTagVolumeCommand(TCHAR *argv[], int argc, int &iParsedOptions, PCOMMAND_LINE_OPTIONS pCommandLineOptions)
{
    PCOMMAND_STRUCT     pCommandStruct = NULL;
    vector<WCHAR*> vVolume;
    vector<TCHAR*> vTag;
    ULONG ulTotalTagLength = 0;
    ULONG ulBytesCopied = 0;
    ULONG i = 0;
    PCHAR pBuffer = NULL;
    ULONG ulNumberOfVolumes = 0;
    ULONG ulNumberOfTags = 0;
    ULONG ulSize = 0;
    ULONG ulFlags = TAG_VOLUME_INPUT_FLAGS_NO_WAIT_FOR_COMMIT;
    ULONG ulTimeOut = INFINITE, ulCount = 1;
    iParsedOptions++;
    bool    bPrompt = false;

    if ((argc > iParsedOptions) && IS_SUB_OPTION(argv[iParsedOptions])) {
        if (0 == _tcsicmp(argv[iParsedOptions], CMDSTR_HELP)||
            (0 == _tcsicmp(argv[iParsedOptions], CMDSTR_H))) {
            iParsedOptions++;
            help = true;
            return false;
        }
    }
    while(iParsedOptions < argc)
    {
        if (IS_MAIN_OPTION(argv[iParsedOptions]))
            break;

        if (IS_SUB_OPTION(argv[iParsedOptions])) {
            if (0 == _tcsicmp(argv[iParsedOptions], CMDOPTSTR_ATOMIC)) {
                ulFlags |= TAG_VOLUME_INPUT_FLAGS_ATOMIC_TO_VOLUME_GROUP;
            } else if (0 == _tcsicmp(argv[iParsedOptions], CMDOPTSTR_SYNCHRONOUS)) {
                ulFlags &= ~TAG_VOLUME_INPUT_FLAGS_NO_WAIT_FOR_COMMIT;
            } else if (0 == _tcsicmp(argv[iParsedOptions], CMDOPTSTR_IGNORE_FILTERING_STOPPED)) {
                ulFlags |= TAG_VOLUME_INPUT_FLAGS_NO_ERROR_IF_FILTERING_STOPPED;
            } else if (0 == _tcsicmp(argv[iParsedOptions], CMDOPTSTR_IGNORE_IF_GUID_NOT_FOUND)) {
                ulFlags |= TAG_VOLUME_INPUT_FLAGS_NO_ERROR_IF_GUID_NOT_FOUND;
            } else if (0 == _tcsicmp(argv[iParsedOptions], CMDOPTSTR_COUNT)) {
                iParsedOptions++;
                if (iParsedOptions >= argc) {
                    _tprintf(_T("ParseTagVolumeCommand: missing parameter for %s\n"),
                                CMDOPTSTR_COUNT);
                    return false;
                }

                if (IS_OPTION(argv[iParsedOptions])) {
                    _tprintf(_T("ParseTagVolumeCommand: Invalid parameter %s for %s\n"),
                                argv[iParsedOptions], CMDOPTSTR_COUNT);
                    return false;
                }
                ulCount =  _tcstoul(argv[iParsedOptions], NULL, 0);
            } else if (0 == _tcsicmp(argv[iParsedOptions], CMDOPTSTR_PROMPT)) {
                bPrompt = true;
            } else if (0 == _tcsicmp(argv[iParsedOptions], CMDOPTSTR_TIMEOUT)) {
                iParsedOptions++;
                if (iParsedOptions >= argc) {
                    _tprintf(_T("ParseTagVolumeCommand: missing parameter for %s\n"),
                                CMDOPTSTR_TIMEOUT);
                    return false;
                }

                if (IS_OPTION(argv[iParsedOptions])) {
                    _tprintf(_T("ParseTagVolumeCommand: Invalid parameter %s for %s\n"),
                                argv[iParsedOptions], CMDOPTSTR_TIMEOUT);
                    return false;
                }
                ulTimeOut =  _tcstoul(argv[iParsedOptions], NULL, 0);
            } else {
                _tprintf(_T("Invalid sub option %s for command %s\n"), argv[iParsedOptions], CMDSTR_TAG_VOLUME);
                return FALSE;
            }
        } else {
            if(IsGUID(argv[iParsedOptions])) {
                WCHAR *VolumeGUID = new WCHAR[GUID_SIZE_IN_CHARS];
				inm_memcpy_s(VolumeGUID, GUID_SIZE_IN_CHARS * sizeof(WCHAR), argv[iParsedOptions], GUID_SIZE_IN_CHARS * sizeof(WCHAR));
                vVolume.push_back(VolumeGUID);
            }
            else if( (IS_DRIVE_LETTER_PAR(argv[iParsedOptions]) && IS_DRIVE_LETTER(argv[iParsedOptions][0]))                 
                || IS_MOUNT_POINT(argv[iParsedOptions])) {
                WCHAR *VolumeGUID = new WCHAR[GUID_SIZE_IN_CHARS];
                if (true == MountPointToGUID(argv[iParsedOptions], VolumeGUID, GUID_SIZE_IN_CHARS * sizeof(WCHAR)) ) 
                {
                    vVolume.push_back(VolumeGUID);
                }
            } else {
                vTag.push_back(argv[iParsedOptions]);
                ulTotalTagLength += (ULONG) _tcslen(argv[iParsedOptions]) + 1;
            }
        }
        iParsedOptions++;
    }

    if (vTag.size() <= 0) {
        _tprintf(_T("%s command needs atleast one tag\n"), CMDSTR_TAG_VOLUME);
        return FALSE;
    }

    if (vVolume.size() <= 0) {
        _tprintf(_T("%s command needs atleast one volume\n"), CMDSTR_TAG_VOLUME);
        return FALSE;
    }

    UUID    TagGUID;
    if (!(ulFlags & TAG_VOLUME_INPUT_FLAGS_NO_WAIT_FOR_COMMIT)){
        RPC_STATUS  Status = RPC_S_OK;
        Status = UuidCreateSequential (&TagGUID);
        if ((Status == RPC_S_OK)||(Status == RPC_S_UUID_LOCAL_ONLY)) {
            printf("GUID = %08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x\n",TagGUID.Data1, TagGUID.Data2, TagGUID.Data3, TagGUID.Data4[0],TagGUID.Data4[1],
                                                       TagGUID.Data4[2], TagGUID.Data4[3], TagGUID.Data4[4], TagGUID.Data4[5], TagGUID.Data4[6], TagGUID.Data4[7]);
        } else {
            printf("ParseTagVolumeCommand: Failed to create GUID using UuidCreateSequential\n");
            return false;
        }
    }

    pCommandStruct = AllocateCommandStruct();
    if (NULL == pCommandStruct) {
        _tprintf(_T("ParseTagVolumeCommand: AllocateCommandStruct failed\n"));
        return false;
    }

    if (ulFlags & TAG_VOLUME_INPUT_FLAGS_NO_WAIT_FOR_COMMIT){
        ulSize = ulTotalTagLength * sizeof(TCHAR) + 
                    vTag.size() * sizeof(USHORT) + 
                    vVolume.size() * GUID_SIZE_IN_CHARS * sizeof(WCHAR) + 
                    sizeof(ULONG) * 3;
        pCommandStruct->ulCommand = CMD_TAG_VOLUME;
    } else {
        ulSize = (ULONG) (sizeof(SYNC_TAG_INPUT_DATA) + 
                          ulTotalTagLength * sizeof(TCHAR) + 
                          vTag.size() * sizeof(USHORT) + 
                          vVolume.size() * GUID_SIZE_IN_CHARS * sizeof(WCHAR) + 
                          sizeof(ULONG));
        pCommandStruct->ulCommand = CMD_SYNCHRONOUS_TAG_VOLUME;
        pCommandStruct->data.TagVolumeInputData.bPrompt = bPrompt;
        pCommandStruct->data.TagVolumeInputData.ulCount = ulCount;
        pCommandStruct->data.TagVolumeInputData.ulTimeOut = ulTimeOut * 1000; // Convert seconds to Milli-seconds.
		inm_memcpy_s(&pCommandStruct->data.TagVolumeInputData.TagGUID, sizeof(GUID), &TagGUID, sizeof(GUID));
    }

    pCommandStruct->data.TagVolumeInputData.pBuffer = (PCHAR) malloc(ulSize);
    pBuffer = pCommandStruct->data.TagVolumeInputData.pBuffer;

    RtlZeroMemory(pBuffer, ulSize);
    
    if( pCommandStruct->ulCommand == CMD_SYNCHRONOUS_TAG_VOLUME){
		inm_memcpy_s(pBuffer, ulSize - ulBytesCopied, &TagGUID, sizeof(GUID));
        ulBytesCopied += sizeof(GUID);
    }

	inm_memcpy_s(pBuffer + ulBytesCopied, ulSize - ulBytesCopied, &ulFlags, sizeof(ULONG));

    ulBytesCopied += sizeof(ULONG);

    //Copy # of GUIDs
    ulNumberOfVolumes = (ULONG) vVolume.size();
	inm_memcpy_s(pBuffer + ulBytesCopied, ulSize - ulBytesCopied, &ulNumberOfVolumes, sizeof(ULONG));

    pCommandStruct->data.TagVolumeInputData.ulNumberOfVolumes = ulNumberOfVolumes;

    ulBytesCopied += sizeof(ULONG);

    //Copy all GUIDs
    for(i = 0; i < vVolume.size(); i++)
    {
		inm_memcpy_s(pBuffer + ulBytesCopied, ulSize - ulBytesCopied, vVolume[i], sizeof(WCHAR)* GUID_SIZE_IN_CHARS);
        ulBytesCopied += sizeof(WCHAR) * GUID_SIZE_IN_CHARS;
    }

    ulNumberOfTags = (ULONG) vTag.size();
	inm_memcpy_s(pBuffer + ulBytesCopied, ulSize - ulBytesCopied, &ulNumberOfTags, sizeof(ULONG));
    ulBytesCopied += sizeof(ULONG);

    //Copy all Tags
    for(i = 0; i < vTag.size(); i++)
    {
        ULONG ulTagLenth = (ULONG) sizeof(TCHAR) * (_tcslen(vTag[i]) + 1);
        //Copy tag length
		inm_memcpy_s(pBuffer + ulBytesCopied, ulSize - ulBytesCopied, &ulTagLenth, sizeof(USHORT));
        ulBytesCopied += sizeof(USHORT);

        //Copy the tag along with terminating null character
		inm_memcpy_s(pBuffer + ulBytesCopied, ulSize - ulBytesCopied, vTag[i], sizeof(TCHAR)* (_tcslen(vTag[i]) + 1));
        ulBytesCopied += (ULONG) sizeof(TCHAR) * (_tcslen(vTag[i]) + 1);
    }

    pCommandStruct->data.TagVolumeInputData.ulLengthOfBuffer = ulBytesCopied;

    InsertTailList(pCommandLineOptions->CommandList, &pCommandStruct->ListEntry);

    for(ULONG i = 0; i < vVolume.size(); i++)
    {
        delete[] vVolume[i];
    }

    return true;
}

bool
ParseSetVolumeFlagsCommand(TCHAR *argv[], int argc, int &iParsedOptions, PCOMMAND_LINE_OPTIONS pCommandLineOptions)
{
    PCOMMAND_STRUCT     pCommandStruct = NULL;

    iParsedOptions++;
   
    if ((argc > iParsedOptions) && IS_SUB_OPTION(argv[iParsedOptions])) {
        if (0 == _tcsicmp(argv[iParsedOptions], CMDSTR_HELP)||
            (0 == _tcsicmp(argv[iParsedOptions], CMDSTR_H))) {
            iParsedOptions++;
            help = true;
            return false;
        }
    }
    pCommandStruct = AllocateCommandStruct();
    if (NULL == pCommandStruct) {
        _tprintf(_T("ParseSetVolumeFlagsCommand: AllocateCommandStruct failed\n"));
        return false;
    }

    pCommandStruct->ulCommand = CMD_SET_VOLUME_FLAGS;
    // This command has drive letter as a parameter.
    if (argc <= iParsedOptions) {
        _tprintf(_T("ParseSetVolumeFlagsCommand: missed drive letter parameter for %s\n"), CMDSTR_SET_VOLUME_FLAGS);
        return false;
    }
    if (!GetVolumeGUID(argv[iParsedOptions], pCommandStruct->data.VolumeFlagsData.VolumeGUID, 
        sizeof(pCommandStruct->data.VolumeFlagsData.VolumeGUID), "ParseSetVolumeFlagsCommand",
        CMDSTR_SET_VOLUME_FLAGS))
    {
        return false;
    }

    pCommandStruct->data.VolumeFlagsData.MountPoint = argv[iParsedOptions];
    pCommandStruct->data.VolumeFlagsData.ulVolumeFlags = 0;
    pCommandStruct->data.VolumeFlagsData.eBitOperation = ecBitOpSet;

    iParsedOptions++;
    while((argc > iParsedOptions) && IS_SUB_OPTION(argv[iParsedOptions])) {
        if (0 == _tcsicmp(argv[iParsedOptions], CMDOPTSTR_IGNORE_PAGEFILE_WRITES)) {
            pCommandStruct->data.VolumeFlagsData.ulVolumeFlags |= VOLUME_FLAG_IGNORE_PAGEFILE_WRITES;
        } else if (0 == _tcsicmp(argv[iParsedOptions], CMDOPTSTR_DISABLE_BITMAP_READ)) {
            pCommandStruct->data.VolumeFlagsData.ulVolumeFlags |= VOLUME_FLAG_DISABLE_BITMAP_READ;
        } else if (0 == _tcsicmp(argv[iParsedOptions], CMDOPTSTR_DISABLE_BITMAP_WRITE)) {
            pCommandStruct->data.VolumeFlagsData.ulVolumeFlags |= VOLUME_FLAG_DISABLE_BITMAP_WRITE;
        } else if (0 == _tcsicmp(argv[iParsedOptions], CMDOPTSTR_MARK_READ_ONLY)) {
            pCommandStruct->data.VolumeFlagsData.ulVolumeFlags |= VOLUME_FLAG_READ_ONLY;
        } else if (0 == _tcsicmp(argv[iParsedOptions], CMDOPTSTR_DISABLE_DATA_CAPTURE)) {
            pCommandStruct->data.VolumeFlagsData.ulVolumeFlags |= VOLUME_FLAG_DISABLE_DATA_CAPTURE;
        } else if (0 == _tcsicmp(argv[iParsedOptions], CMDOPTSTR_FREE_DATA_BUFFERS)) {
            pCommandStruct->data.VolumeFlagsData.ulVolumeFlags |= VOLUME_FLAG_FREE_DATA_BUFFERS;
        } else if (0 == _tcsicmp(argv[iParsedOptions], CMDOPTSTR_DISABLE_DATA_FILES)) {
            pCommandStruct->data.VolumeFlagsData.ulVolumeFlags |= VOLUME_FLAG_DISABLE_DATA_FILES;
        } else if (0 == _tcsicmp(argv[iParsedOptions], CMDOPTSTR_RESET_FLAGS)) {
            pCommandStruct->data.VolumeFlagsData.eBitOperation = ecBitOpReset;
        } else if (0 == _tcsicmp(argv[iParsedOptions], CMDOPTSTR_VCONTEXT_FIELDS_PERSISTED)) {
            pCommandStruct->data.VolumeFlagsData.ulVolumeFlags |= VOLUME_FLAG_VCFIELDS_PERSISTED;
        } else if (0 == _tcsicmp(argv[iParsedOptions], CMDOPTSTR_ENABLE_SOURCE_ROLLBACK)) {
            pCommandStruct->data.VolumeFlagsData.ulVolumeFlags |= VOLUME_FLAG_ENABLE_SOURCE_ROLLBACK;
        } else if (0 == _tcsicmp(argv[iParsedOptions], CMDOPTSTR_MARK_VOLUME_ONLINE)) {
            pCommandStruct->data.VolumeFlagsData.ulVolumeFlags |= VOLUME_FLAG_ONLINE;
        } else if (0 == _tcsicmp(argv[iParsedOptions], CMDOPTSTR_VOLUME_DATA_LOG_DIRECTORY)) {
            pCommandStruct->data.VolumeFlagsData.ulVolumeFlags |= VOLUME_FLAG_DATA_LOG_DIRECTORY;
            iParsedOptions++;
            if (argc <= iParsedOptions) {
                _tprintf(_T("\n%S: value missing for option %s\n"), __TFUNCTION__, CMDOPTSTR_VOLUME_DATA_LOG_DIRECTORY);
                return false;
            }
            size_t usLength = _tcslen(argv[iParsedOptions]);
            TCHAR *path = argv[iParsedOptions];
            if (!ValidatePath(path, usLength)) {
                _tprintf(_T("\n%S: Incorrect value set for option %s. Given Path is %s\n"), __TFUNCTION__, CMDOPTSTR_VOLUME_DATA_LOG_DIRECTORY, path);
                return false;
            }
            pCommandStruct->data.VolumeFlagsData.DataFile.usLength = usLength * sizeof(TCHAR);
            memmove(pCommandStruct->data.VolumeFlagsData.DataFile.FileName, path, usLength * sizeof(WCHAR));
            pCommandStruct->data.VolumeFlagsData.DataFile.FileName[usLength] = L'\0'; 
        } else {
            _tprintf(_T("ParseSetVolumeFlagsCommand: Unrecognized option %s\n"), argv[iParsedOptions]);
            return false;
        }

        iParsedOptions++;
    }

    InsertTailList(pCommandLineOptions->CommandList, &pCommandStruct->ListEntry);
    return true;
}

bool
ParseSetDriverFlagsCommand(TCHAR *argv[], int argc, int &iParsedOptions, PCOMMAND_LINE_OPTIONS pCommandLineOptions)
{
    PCOMMAND_STRUCT     pCommandStruct = NULL;

    iParsedOptions++;

    if ((argc > iParsedOptions) && IS_SUB_OPTION(argv[iParsedOptions])) {
        if (0 == _tcsicmp(argv[iParsedOptions], CMDSTR_HELP)||
            (0 == _tcsicmp(argv[iParsedOptions], CMDSTR_H))) {
            iParsedOptions++;
            help = true;
            return false;
        }
    }
    pCommandStruct = AllocateCommandStruct();
    if (NULL == pCommandStruct) {
        _tprintf(_T("ParseSetDriverFlagsCommand: AllocateCommandStruct failed\n"));
        return false;
    }

    pCommandStruct->ulCommand = CMD_SET_DRIVER_FLAGS;
    pCommandStruct->data.DriverFlagsData.ulDriverFlags = 0;
    pCommandStruct->data.DriverFlagsData.eBitOperation = ecBitOpSet;

    while((argc > iParsedOptions) && IS_SUB_OPTION(argv[iParsedOptions])) {
        if (0 == _tcsicmp(argv[iParsedOptions], CMDOPTSTR_RESET_FLAGS)) {
            pCommandStruct->data.DriverFlagsData.eBitOperation = ecBitOpReset;
        } else if (0 == _tcsicmp(argv[iParsedOptions], CMDOPTSTR_DISABLE_DATA_FILES)) {
            pCommandStruct->data.DriverFlagsData.ulDriverFlags |= DRIVER_FLAG_DISABLE_DATA_FILES;
        } else if (0 == _tcsicmp(argv[iParsedOptions], CMDOPTSTR_DISABLE_DATA_FILES_FOR_NV)) {
            pCommandStruct->data.DriverFlagsData.ulDriverFlags |= DRIVER_FLAG_DISABLE_DATA_FILES_FOR_NEW_VOLUMES;
        } else {
            _tprintf(_T("ParseSetDriverFlagsCommand: Unrecognized option %s\n"), argv[iParsedOptions]);
            return false;
        }
        iParsedOptions++;
    }

    InsertTailList(pCommandLineOptions->CommandList, &pCommandStruct->ListEntry);
    return true;
}
bool
ParseGetSyncCommand(TCHAR *argv[], int argc, int &iParsedOptions, PCOMMAND_LINE_OPTIONS pCommandLineOptions)
{
	PCOMMAND_STRUCT     pCommandStruct = NULL;

	iParsedOptions++;

	if ((argc > iParsedOptions) && IS_SUB_OPTION(argv[iParsedOptions])) {
		if (0 == _tcsicmp(argv[iParsedOptions], CMDSTR_HELP) ||
			(0 == _tcsicmp(argv[iParsedOptions], CMDSTR_H))) {
			iParsedOptions++;
			help = true;
			return false;
		}
	}
	pCommandStruct = AllocateCommandStruct();
	if (NULL == pCommandStruct) {
		_tprintf(_T("ParseGetSyncCommand: AllocateCommandStruct failed\n"));
		return false;
	}

	pCommandStruct->ulCommand = CMD_GET_SYNC;
	// This command has drive letter as a parameter.
	if (argc <= (iParsedOptions + 1)) {
		_tprintf(_T("ParseGetSyncCommand: missed parameter for CMDSTR_GET_SYNC"));
		return false;
	}
	if (!GetVolumeGUID(argv[iParsedOptions], pCommandStruct->data.GetDBTrans.VolumeGUID,
		sizeof(pCommandStruct->data.GetDBTrans.VolumeGUID), "ParseGetSyncCommand",
		CMDSTR_GET_DB_TRANS))
	{
		return false;
	}
	pCommandStruct->data.GetDBTrans.MountPoint = argv[iParsedOptions];

	//Required for CreateDevice
    StringCchPrintf(pCommandStruct->data.GetDBTrans.tcSourceName, ARRAYSIZE(pCommandStruct->data.GetDBTrans.tcSourceName), _T("\\\\.\\%s"), argv[iParsedOptions]);

	iParsedOptions++;
	if (!GetVolumeGUID(argv[iParsedOptions], pCommandStruct->data.GetDBTrans.DestVolumeGUID,
		sizeof(pCommandStruct->data.GetDBTrans.DestVolumeGUID), "ParseGetSyncCommand",
		CMDSTR_GET_DB_TRANS))
	{
		return false;
	}
	pCommandStruct->data.GetDBTrans.DestMountPoint = argv[iParsedOptions];

    StringCchPrintf(pCommandStruct->data.GetDBTrans.tcDestName, ARRAYSIZE(pCommandStruct->data.GetDBTrans.tcDestName), _T("\\\\.\\%s"), argv[iParsedOptions]);

	// Default wait for 65 milsec and set waitiftso, commitcurr, sync, resync_req
	pCommandStruct->data.GetDBTrans.ulRunTimeInMilliSeconds = 65;
	pCommandStruct->data.GetDBTrans.ulNumDirtyBlocks = 0;
	pCommandStruct->data.GetDBTrans.bPollForDirtyBlocks = false;
	pCommandStruct->data.GetDBTrans.bWaitIfTSO = true;
	pCommandStruct->data.GetDBTrans.bResetResync = false;
	pCommandStruct->data.GetDBTrans.CommitCurrentTrans = true;
	pCommandStruct->data.GetDBTrans.sync = true;
	pCommandStruct->data.GetDBTrans.resync_req = true;
	pCommandStruct->data.GetDBTrans.skip_step_one = false;

	iParsedOptions++;
	while ((argc > iParsedOptions) && IS_SUB_OPTION(argv[iParsedOptions])) {
		if (0 == _tcsicmp(argv[iParsedOptions], CMDOPTSTR_PRINT_DETAILS)) {
			pCommandStruct->data.GetDBTrans.PrintDetails = true;
			// Check if level is specified for this option.
			if ((argc > (iParsedOptions + 1)) && (IS_COMMAND_PAR(argv[iParsedOptions + 1]))) {
				iParsedOptions++;
				pCommandStruct->data.GetDBTrans.ulLevelOfDetail = _ttol(argv[iParsedOptions]);
			}
			else {
				pCommandStruct->data.GetDBTrans.ulLevelOfDetail = 1;
			}
		}
		else if (0 == _tcsicmp(argv[iParsedOptions], CMDOPTSTR_SKIP_STEP_ONE)) {
			pCommandStruct->data.GetDBTrans.skip_step_one = true;
			pCommandStruct->data.GetDBTrans.resync_req = false;
		}
		else {
			_tprintf(_T("ParseGetSyncCommand: Invalid option %s for command %s\n"),
				argv[iParsedOptions], CMDSTR_GET_DB_TRANS);
			return false;
		}
		iParsedOptions++;
	}

	InsertTailList(pCommandLineOptions->CommandList, &pCommandStruct->ListEntry);
	return true;
}
/* 
drvutil.exe  --GetSync -srcdisk <DiskName> -srcsign <disksig/guid> -tgtvhd VHDfileName[Option]
options : skip_step_one
          printdetails
/*
1. Validate input
2. If it matches copy the disk name to the internal structure
3. Validate whether the file name has an extension with.vhd file
4. If it matches copy the target file name to the internal structure
5. Make other initialization and insert to the list
*/
bool
ParseGetSyncCommand_v2(TCHAR *argv[], int argc, int &iParsedOptions, PCOMMAND_LINE_OPTIONS pCommandLineOptions)
{
    PCOMMAND_STRUCT     pCommandStruct = NULL;

	//int temp = argc;
	//int temp1 = iParsedOptions;

	//printf("Argument count : %d ; Agument Index : %d \n", temp, temp1);

	//while (temp > temp1) {
	//	_tprintf(_T(" %s "), argv[temp1]);
	//	temp1++;
	//}
	//_tprintf(_T("\n"));
	
    iParsedOptions++;

    if ((argc > iParsedOptions) && IS_SUB_OPTION(argv[iParsedOptions])) {
        if (0 == _tcsicmp(argv[iParsedOptions], CMDSTR_HELP)||
            (0 == _tcsicmp(argv[iParsedOptions], CMDSTR_H))) {
            iParsedOptions++;
            help = true;
            return false;
        }
    }
    pCommandStruct = AllocateCommandStruct();
    if (NULL == pCommandStruct) {
        _tprintf(_T("ParseGetSyncCommand: AllocateCommandStruct failed\n"));
        return false;
    }

    pCommandStruct->ulCommand = CMD_GET_SYNC;
    
    pCommandStruct->data.GetDBTrans.serviceshutdownnotify = false;
    if ((0 == _tcsicmp(argv[iParsedOptions], CMDSTR_SHUTDOWN_NOTIFY)) ||
        (0 == _tcsicmp(argv[iParsedOptions], CMDSTR_SHUTDOWN_NOTIFY_SF))) {
        //_tprintf(_T("--sn\n"));
        pCommandStruct->data.GetDBTrans.serviceshutdownnotify = true;
        iParsedOptions++;
    }
    else {
        _tprintf(_T(" Wrong switch, expected --sn \n"));
    }

    pCommandStruct->data.GetDBTrans.processstartnotify = false;
    if ((0 == _tcsicmp(argv[iParsedOptions], CMDSTR_PROCESS_START_NOTIFY)) ||
        (0 == _tcsicmp(argv[iParsedOptions], CMDSTR_PROCESS_START_NOTIFY_SF))) {
        //_tprintf(_T("--psn\n"));
        pCommandStruct->data.GetDBTrans.processstartnotify = true;
        iParsedOptions++;
    }
    else {
        _tprintf(_T(" Wrong switch, expected --psn \n"));
    }

    // Copy src disk name which has string form \\.\PHYSICALDRIVE#
    if (0 == _tcsicmp(argv[iParsedOptions], CMDPOPSTR_SRC_DISK)) {		
        iParsedOptions++;
    } else  {
        return false;
    }
    StringCchCopy(pCommandStruct->data.GetDBTrans.tcSourceName, 
              sizeof(pCommandStruct->data.GetDBTrans.tcSourceName),
                  argv[iParsedOptions]);
        //_tprintf(_T("ParseGetSyncCommand: Source Disk Name %s for command %s\n"),
        //pCommandStruct->data.GetDBTrans.tcSourceName, _T("ParseGetSynCommand"));
    iParsedOptions++;

    // Assume user has retrieved the Disk Signature through GetDriveList command and input through this command
    if (0 == _tcsicmp(argv[iParsedOptions], CMDOPSTR_SRC_SIGN)) {
        iParsedOptions++;
    } else  {
        return false;
    }	
    StringCchCopy(pCommandStruct->data.GetDBTrans.VolumeGUID,
        sizeof(pCommandStruct->data.GetDBTrans.VolumeGUID),
        argv[iParsedOptions]);
    //_tprintf(_T("ParseGetSyncCommand: Source signature %s for command %s\n"),
        //pCommandStruct->data.GetDBTrans.VolumeGUID, _T("ParseGetSynCommand"));
    iParsedOptions++;

    // Now the turn for target VHD file, Copy target VHD File path e.g : e:\test.vhd..
    if (0 == _tcsicmp(argv[iParsedOptions], CMDOPSTR_TARGET_VHD_FILE)) {
        iParsedOptions++;
    } else  {
        return false;
    }
    HRESULT res;
    res = StringCchCopy(pCommandStruct->data.GetDBTrans.tcDestName,
        sizeof(pCommandStruct->data.GetDBTrans.tcDestName),
        argv[iParsedOptions]);

    if (S_OK == res) {
        //_tprintf(_T("ParseGetSyncCommand: Target file Name %s\n"), pCommandStruct->data.GetDBTrans.tcDestName);
    } else {		
        _tprintf(_T("ParseGetSyncCommand: Parsing Failed, check the usage %s\n"),
            argv[iParsedOptions]);
//		_tprintf(_T(" 3\n"));
        return false;
    }

    _tprintf(_T(" Parsing ReplicateDiskCommand is succesful\n"));
    /* Configuration required to do continuous GetDB 
    wait for 65 seconds or do get db based on the pending changes
    Except a TSO after 65 seconds tenure or a DirtyBlock/Data file with or without tag		
    */
  
    // Default wait for 65 milsec and set waitiftso, commitcurr, sync, resync_req
    pCommandStruct->data.GetDBTrans.ulRunTimeInMilliSeconds = 0;
    pCommandStruct->data.GetDBTrans.ulNumDirtyBlocks = 0;
    pCommandStruct->data.GetDBTrans.bPollForDirtyBlocks = false;
    pCommandStruct->data.GetDBTrans.bWaitIfTSO = true;
    pCommandStruct->data.GetDBTrans.bResetResync = false;
    pCommandStruct->data.GetDBTrans.CommitCurrentTrans = true;
    pCommandStruct->data.GetDBTrans.sync = true;
    pCommandStruct->data.GetDBTrans.resync_req = true;
    pCommandStruct->data.GetDBTrans.skip_step_one = false;
	pCommandStruct->data.GetDBTrans.bStopReplicationOnTag = false;

    iParsedOptions++;
    while((argc > iParsedOptions) && IS_SUB_OPTION(argv[iParsedOptions])) {
        if (0 == _tcsicmp(argv[iParsedOptions], CMDOPTSTR_PRINT_DETAILS)) {
            pCommandStruct->data.GetDBTrans.PrintDetails = true;
            // Check if level is specified for this option.
            if ((argc > (iParsedOptions + 1)) && (IS_COMMAND_PAR(argv[iParsedOptions + 1])) ) {
                iParsedOptions++;
                pCommandStruct->data.GetDBTrans.ulLevelOfDetail = _ttol(argv[iParsedOptions]);
            } else {
                pCommandStruct->data.GetDBTrans.ulLevelOfDetail = 1;
            }
        } else if (0 == _tcsicmp(argv[iParsedOptions], CMDOPTSTR_SKIP_STEP_ONE)) {
            pCommandStruct->data.GetDBTrans.skip_step_one = true;
            pCommandStruct->data.GetDBTrans.resync_req = false;
		} else if (0 == _tcsicmp(argv[iParsedOptions], CMDOPTSTR_DO_STEP_ONE)) {
			pCommandStruct->data.GetDBTrans.skip_step_one = false;
		} else if (argv[iParsedOptions], CMDOPTSTR_STOP_ON_TAG) {
			_tprintf(_T("ParseGetSyncCommand: stop replication on TAG IoCTL %s\n"));
			pCommandStruct->data.GetDBTrans.bStopReplicationOnTag = true;
        } else {
            _tprintf(_T("ParseGetSyncCommand: Invalid option %s\n"),
                     argv[iParsedOptions]);
            return false;
        }
        iParsedOptions++;
    }

    InsertTailList(pCommandLineOptions->CommandList, &pCommandStruct->ListEntry);
    return true;
}

bool
ParseGetDirtyBlocksCommand(TCHAR *argv[], int argc, int &iParsedOptions, PCOMMAND_LINE_OPTIONS pCommandLineOptions)
{
    PCOMMAND_STRUCT     pCommandStruct = NULL;

    iParsedOptions++;

    if ((argc > iParsedOptions) && IS_SUB_OPTION(argv[iParsedOptions])) {
        if (0 == _tcsicmp(argv[iParsedOptions], CMDSTR_HELP)||
            (0 == _tcsicmp(argv[iParsedOptions], CMDSTR_H))) {
            iParsedOptions++;
            help = true;
            return false;
        }
    }
    pCommandStruct = AllocateCommandStruct();
    if (NULL == pCommandStruct) {
        _tprintf(_T("ParseGetDirtyBlocksCommand: AllocateCommandStruct failed\n"));
        return false;
    }

    pCommandStruct->ulCommand = CMD_GET_DB_TRANS;
    // This command has drive letter as a parameter.
    if (argc <= iParsedOptions) {
        _tprintf(_T("ParseGetDirtyBlocksCommand: missed parameter for CMDSTR_GET_DB_TRANS"));
        return false;
    }
    if (!GetVolumeGUID(argv[iParsedOptions], pCommandStruct->data.GetDBTrans.VolumeGUID, 
        sizeof(pCommandStruct->data.GetDBTrans.VolumeGUID), "ParseGetDirtyBlocksCommand",
        CMDSTR_GET_DB_TRANS))
    {
        return false;
    }
    pCommandStruct->data.GetDBTrans.MountPoint = argv[iParsedOptions];
    pCommandStruct->data.GetDBTrans.ulRunTimeInMilliSeconds = 0;
    pCommandStruct->data.GetDBTrans.ulNumDirtyBlocks = 0;
    pCommandStruct->data.GetDBTrans.bPollForDirtyBlocks = false;
    pCommandStruct->data.GetDBTrans.bWaitIfTSO = false;
    pCommandStruct->data.GetDBTrans.bResetResync = false;

    iParsedOptions++;
    while((argc > iParsedOptions) && IS_SUB_OPTION(argv[iParsedOptions])) {
        if (0 == _tcsicmp(argv[iParsedOptions], CMDOPTSTR_NUM_DIRTY_BLOCKS)) {
            iParsedOptions++;
            if (iParsedOptions >= argc) {
                _tprintf(_T("ParseGetDirtyBlocksCommand: missing parameter for %s\n"),
                                CMDOPTSTR_NUM_DIRTY_BLOCKS);
                return false;
            }
            if (IS_OPTION(argv[iParsedOptions])) {
                _tprintf(_T("ParseGetDirtyBlocksCommand: Invalid parameter %s for %s\n"), 
                         argv[iParsedOptions], CMDOPTSTR_NUM_DIRTY_BLOCKS);
                return false;
            }
            pCommandStruct->data.GetDBTrans.ulNumDirtyBlocks = _tcstoul(argv[iParsedOptions], NULL, 0);
            pCommandStruct->data.GetDBTrans.bWaitIfTSO = true;
        } else if (0 == _tcsicmp(argv[iParsedOptions], CMDOPTSTR_POLL_INTERVAL)) {
            pCommandStruct->data.GetDBTrans.bPollForDirtyBlocks = true;
            if ((argc > (iParsedOptions + 1)) && (IS_COMMAND_PAR(argv[iParsedOptions + 1])) ) {
                iParsedOptions++;
                pCommandStruct->data.GetDBTrans.ulPollIntervalInMilliSeconds = _ttol(argv[iParsedOptions]) * 1000;
            } else {
                pCommandStruct->data.GetDBTrans.ulPollIntervalInMilliSeconds = DEFAULT_GET_DB_POLL_INTERVAL * 1000;
            }
        } else if (0 == _tcsicmp(argv[iParsedOptions], CMDOPTSTR_RUN_FOR_TIME)) {
            iParsedOptions++;
            if (iParsedOptions >= argc) {
                _tprintf(_T("ParseGetDirtyBlocksCommand: missing parameter for %s\n"),
                                CMDOPTSTR_RUN_FOR_TIME);
                return false;
            }
            if (IS_OPTION(argv[iParsedOptions])) {
                _tprintf(_T("ParseGetDirtyBlocksCommand: Invalid parameter %s for %s\n"), 
                         argv[iParsedOptions], CMDOPTSTR_RUN_FOR_TIME);
                return false;
            }
            pCommandStruct->data.GetDBTrans.ulRunTimeInMilliSeconds = _tcstoul(argv[iParsedOptions], NULL, 0) * 1000;
            if (0 == pCommandStruct->data.GetDBTrans.ulRunTimeInMilliSeconds) {
                _tprintf(_T("ParseGetDirtyBlocksCommand: Invalid parameter %s for %s\n"), 
                         argv[iParsedOptions], CMDOPTSTR_RUN_FOR_TIME);
                return false;
            }
            pCommandStruct->data.GetDBTrans.bWaitIfTSO = true;
        } else if (0 == _tcsicmp(argv[iParsedOptions], CMDOPTSTR_COMMIT_PREV)) {
            pCommandStruct->data.GetDBTrans.CommitPreviousTrans = true;
        } else if (0 == _tcsicmp(argv[iParsedOptions], CMDOPTSTR_RESET_RESYNC)) {
            pCommandStruct->data.GetDBTrans.bResetResync = true;
        } else if (0 == _tcsicmp(argv[iParsedOptions], CMDOPTSTR_COMMIT_CURRENT)) {
            pCommandStruct->data.GetDBTrans.CommitCurrentTrans = true;
        } else if (0 == _tcsicmp(argv[iParsedOptions], CMDOPTSTR_PRINT_DETAILS)) {
            pCommandStruct->data.GetDBTrans.PrintDetails = true;
            // Check if level is specified for this option.
            if ((argc > (iParsedOptions + 1)) && (IS_COMMAND_PAR(argv[iParsedOptions + 1])) ) {
                iParsedOptions++;
                pCommandStruct->data.GetDBTrans.ulLevelOfDetail = _ttol(argv[iParsedOptions]);
            } else {
                pCommandStruct->data.GetDBTrans.ulLevelOfDetail = 1;
            }
        } else if (0 == _tcsicmp(argv[iParsedOptions], CMDOPTSTR_COMMIT_TIMEOUT)) {
            pCommandStruct->data.GetDBTrans.CommitCurrentTrans = true;
            // This sub option should have a parameter.
            iParsedOptions++;
            if (iParsedOptions >= argc) {
                _tprintf(_T("ParseGetDirtyBlocksCommand: missing parameter for %s\n"),
                                CMDOPTSTR_COMMIT_TIMEOUT);
                return false;
            }

            if (IS_OPTION(argv[iParsedOptions])) {
                _tprintf(_T("ParseGetDirtyBlocksCommand: Invalid parameter %s for %s\n"),
                                argv[iParsedOptions], CMDOPTSTR_COMMIT_TIMEOUT);
                return false;
            }
            pCommandStruct->data.GetDBTrans.ulWaitTimeBeforeCurrentTransCommit =  _tcstoul(argv[iParsedOptions], NULL, 0);
        } else {
            _tprintf(_T("ParseGetDirtyBlocksCommand: Invalid option %s for command %s\n"),
                     argv[iParsedOptions], CMDSTR_GET_DB_TRANS);
            return false;
        }
        iParsedOptions++;
    }

    InsertTailList(pCommandLineOptions->CommandList, &pCommandStruct->ListEntry);
    return true;
}

bool
ParseSetResyncRequired(TCHAR *argv[], int argc, int &iParsedOptions, PCOMMAND_LINE_OPTIONS pCommandLineOptions)
{
    PCOMMAND_STRUCT     pCommandStruct = NULL;

    iParsedOptions++;

    if ((argc > iParsedOptions) && IS_SUB_OPTION(argv[iParsedOptions])) {
        if (0 == _tcsicmp(argv[iParsedOptions], CMDSTR_HELP)||
            (0 == _tcsicmp(argv[iParsedOptions], CMDSTR_H))) {
            iParsedOptions++;
            help = true;
            return false;
        }
    }
    pCommandStruct = AllocateCommandStruct();
    if (NULL == pCommandStruct) {
        _tprintf(_T("ParseSetResyncRequired: AllocateCommandStruct failed\n"));
        return false;
    }

    pCommandStruct->ulCommand = CMD_SET_RESYNC_REQUIRED;
    // This command has drive letter as a parameter.
    if (argc <= iParsedOptions) {
        _tprintf(_T("ParseSetResyncRequired: missed parameter for CMDSTR_SET_RESYNC_REQUIRED"));
        return false;
    }
  
    if (!GetVolumeGUID(argv[iParsedOptions], pCommandStruct->data.SetResyncRequired.VolumeGUID, 
        sizeof(pCommandStruct->data.SetResyncRequired.VolumeGUID), "ParseSetResyncRequired",
        CMDSTR_SET_RESYNC_REQUIRED))
    {
        return false;
    }
    pCommandStruct->data.SetResyncRequired.MountPoint = argv[iParsedOptions];
    pCommandStruct->data.SetResyncRequired.ulErrorCode = 0x01;
    pCommandStruct->data.SetResyncRequired.ulErrorStatus = 0x00;
    iParsedOptions++;

    while((argc > iParsedOptions) && IS_SUB_OPTION(argv[iParsedOptions])) {
        if (0 == _tcsicmp(argv[iParsedOptions], CMDOPTSTR_ERROR_CODE)) {
            iParsedOptions++;
            if (iParsedOptions >= argc) {
                _tprintf(_T("ParseSetResyncRequired: missing parameter for %s\n"),
                                CMDOPTSTR_ERROR_CODE);
                return false;
            }
            if (IS_OPTION(argv[iParsedOptions])) {
                _tprintf(_T("ParseSetResyncRequired: Invalid parameter %s for %s\n"), 
                         argv[iParsedOptions], CMDOPTSTR_ERROR_CODE);
                return false;
            }
            pCommandStruct->data.SetResyncRequired.ulErrorCode = _tcstoul(argv[iParsedOptions], NULL, 0);
            if (!pCommandStruct->data.SetResyncRequired.ulErrorCode) {
                _tprintf(_T("ParseSetResyncRequired: Invalid parameter %s for %s\n"), 
                         argv[iParsedOptions], CMDOPTSTR_ERROR_CODE);
                return false;
            }
        } else if (0 == _tcsicmp(argv[iParsedOptions], CMDOPTSTR_ERROR_STATUS)) {
            iParsedOptions++;
            if (iParsedOptions >= argc) {
                _tprintf(_T("ParseSetResyncRequired: missing parameter for %s\n"),
                                CMDOPTSTR_ERROR_STATUS);
                return false;
            }
            if (IS_OPTION(argv[iParsedOptions])) {
                _tprintf(_T("ParseSetResyncRequired: Invalid parameter %s for %s\n"), 
                         argv[iParsedOptions], CMDOPTSTR_ERROR_STATUS);
                return false;
            }
            
            pCommandStruct->data.SetResyncRequired.ulErrorStatus = _tcstoul(argv[iParsedOptions], NULL, 0);
        } else {
            _tprintf(_T("ParseCommitBlocksCommand: Invalid option %s for command %s\n"),
                     argv[iParsedOptions], CMDSTR_COMMIT_DB_TRANS);
            return false;
        }
        iParsedOptions++;        
    }

    InsertTailList(pCommandLineOptions->CommandList, &pCommandStruct->ListEntry);
    return true;
}

bool
ParseCommitBlocksCommand(TCHAR *argv[], int argc, int &iParsedOptions, PCOMMAND_LINE_OPTIONS pCommandLineOptions)
{
    PCOMMAND_STRUCT     pCommandStruct = NULL;

    iParsedOptions++;

    if ((argc > iParsedOptions) && IS_SUB_OPTION(argv[iParsedOptions])) {
        if (0 == _tcsicmp(argv[iParsedOptions], CMDSTR_HELP)||
            (0 == _tcsicmp(argv[iParsedOptions], CMDSTR_H))) {
            iParsedOptions++;
            help = true;
            return false;
        }
    }
    pCommandStruct = AllocateCommandStruct();
    if (NULL == pCommandStruct) {
        _tprintf(_T("ParseCommitBlocksCommand: AllocateCommandStruct failed\n"));
        return false;
    }

    pCommandStruct->ulCommand = CMD_COMMIT_DB_TRANS;
    // This command has drive letter as a parameter.
    if (argc <= iParsedOptions) {
        _tprintf(_T("ParseCommitBlocksCommand: missed parameter for CMDSTR_COMMIT_DB_TRANS"));
        return false;
    }
    if (!GetVolumeGUID(argv[iParsedOptions], pCommandStruct->data.CommitDBTran.VolumeGUID, 
        sizeof(pCommandStruct->data.CommitDBTran.VolumeGUID), "ParseCommitBlocksCommand",
        CMDSTR_COMMIT_DB_TRANS))
    {
        return false;
    }

    pCommandStruct->data.CommitDBTran.MountPoint = argv[iParsedOptions];
    pCommandStruct->data.CommitDBTran.bResetResync = false;
    iParsedOptions++;

    while((argc > iParsedOptions) && IS_SUB_OPTION(argv[iParsedOptions])) {
        if (0 == _tcsicmp(argv[iParsedOptions], CMDOPTSTR_RESET_RESYNC)) {
            pCommandStruct->data.CommitDBTran.bResetResync = true;
        } else {
            _tprintf(_T("ParseCommitBlocksCommand: Invalid option %s for command %s\n"),
                     argv[iParsedOptions], CMDSTR_COMMIT_DB_TRANS);
            return false;
        }
        iParsedOptions++;        
    }

    InsertTailList(pCommandLineOptions->CommandList, &pCommandStruct->ListEntry);
    return true;
}

bool
ParseSetDataFileWriterThreadPriority(TCHAR *argv[], int argc, int &iParsedOptions, PCOMMAND_LINE_OPTIONS pCommandLineOptions)
{
    PCOMMAND_STRUCT     pCommandStruct = NULL;
    PTCHAR              MountPoint = NULL;

    iParsedOptions++;

    if ((argc > iParsedOptions) && IS_SUB_OPTION(argv[iParsedOptions])) {
        if (0 == _tcsicmp(argv[iParsedOptions], CMDSTR_HELP)||
            (0 == _tcsicmp(argv[iParsedOptions], CMDSTR_H))) {
            iParsedOptions++;
            help = true;
            return false;
        }
    }
    pCommandStruct = AllocateCommandStruct();
    if (NULL == pCommandStruct) {
        _tprintf(_T("%S: AllocateCommandStruct failed\n"), __TFUNCTION__);
        return false;
    }

    pCommandStruct->ulCommand = CMD_SET_DATA_FILE_THREAD_PRIORITY;
    memset(&pCommandStruct->data.SetDataFileThreadPriority, 0, sizeof(SET_DATA_FILE_THREAD_PRIORITY_DATA));

    while((argc > iParsedOptions) && (!IS_MAIN_OPTION(argv[iParsedOptions]))) {
        if ((MountPoint == NULL))
        {
            MountPoint = argv[iParsedOptions];
            if (!GetVolumeGUID(argv[iParsedOptions], pCommandStruct->data.SetDataFileThreadPriority.VolumeGUID, 
                sizeof(pCommandStruct->data.SetDataFileThreadPriority.VolumeGUID), "ParseSetDataFileWriterThreadPriority",
                CMDSTR_SET_DATA_FILE_WRITER_THREAD_PRIORITY))
            {
                return false;
            }
            pCommandStruct->data.SetDataFileThreadPriority.MountPoint = argv[iParsedOptions];
            pCommandStruct->data.SetDataFileThreadPriority.ulFlags |= SET_DATA_FILE_THREAD_PRIORITY_DATA_VALID_GUID;
            iParsedOptions++;
            if (argc <= iParsedOptions) {
                _tprintf(_T("%S: value missing for MountPoint %s \n"), __TFUNCTION__, MountPoint);
                return false;
            }

            pCommandStruct->data.SetDataFileThreadPriority.ulPriority = _ttol(argv[iParsedOptions]);
        } else if ((0 == pCommandStruct->data.SetDataFileThreadPriority.DefaultPriority) &&
                   (0 == _tcsicmp(argv[iParsedOptions], CMDOPTSTR_SET_DEFAULT))) 
        {
            iParsedOptions++;
            if (argc <= iParsedOptions) {
                _tprintf(_T("%S: value missing for option %s\n"), __TFUNCTION__, CMDOPTSTR_SET_DEFAULT);
                return false;
            }

            pCommandStruct->data.SetDataFileThreadPriority.DefaultPriority = _ttol(argv[iParsedOptions]);
        } else if ((0 == pCommandStruct->data.SetDataFileThreadPriority.PriorityForAllVolumes) &&
            (0 == _tcsicmp(argv[iParsedOptions], CMDOPTSTR_SET_FOR_ALL_VOLUMES)))
        {
            iParsedOptions++;
            if (argc <= iParsedOptions) {
                _tprintf(_T("%S: value missing for option %s\n"), __TFUNCTION__, CMDOPTSTR_SET_FOR_ALL_VOLUMES);
                return false;
            }

            pCommandStruct->data.SetDataFileThreadPriority.PriorityForAllVolumes = _ttol(argv[iParsedOptions]);
        } else {
            _tprintf(_T("%S: invalid option %s found\n"), __TFUNCTION__, argv[iParsedOptions]);
            return false;
        }

        iParsedOptions++;
    }

    InsertTailList(pCommandLineOptions->CommandList, &pCommandStruct->ListEntry);
    return true;
}

bool
ParseSetWorkerThreadPriority(TCHAR *argv[], int argc, int &iParsedOptions, PCOMMAND_LINE_OPTIONS pCommandLineOptions)
{
    PCOMMAND_STRUCT     pCommandStruct = NULL;
    TCHAR               DriveLetter = _T('\0');

    iParsedOptions++;

    if ((argc > iParsedOptions) && IS_SUB_OPTION(argv[iParsedOptions])) {
        if (0 == _tcsicmp(argv[iParsedOptions], CMDSTR_HELP)||
            (0 == _tcsicmp(argv[iParsedOptions], CMDSTR_H))) {
            iParsedOptions++;
            help = true;
            return false;
        }
    }
    pCommandStruct = AllocateCommandStruct();
    if (NULL == pCommandStruct) {
        _tprintf(_T("%S: AllocateCommandStruct failed\n"), __TFUNCTION__);
        return false;
    }

    pCommandStruct->ulCommand = CMD_SET_WORKER_THREAD_PRIORITY;
    memset(&pCommandStruct->data.SetWorkerThreadPriority, 0, sizeof(SET_WORKER_THREAD_PRIORITY_DATA));

    while ((argc > iParsedOptions) && IS_COMMAND_PAR(argv[iParsedOptions])) {
        pCommandStruct->data.SetWorkerThreadPriority.ulFlags |= SET_WORKER_THREAD_PRIORITY_VALID_PRIORITY;
        pCommandStruct->data.SetWorkerThreadPriority.ulPriority = _ttol(argv[iParsedOptions]);
        iParsedOptions++;
    }

    InsertTailList(pCommandLineOptions->CommandList, &pCommandStruct->ListEntry);
    return true;
}

bool
ParseSetDataToDiskSizeLimit(TCHAR *argv[], int argc, int &iParsedOptions, PCOMMAND_LINE_OPTIONS pCommandLineOptions)
{
    PCOMMAND_STRUCT     pCommandStruct = NULL;
    PTCHAR              MountPoint = NULL ;

    iParsedOptions++;

    if ((argc > iParsedOptions) && IS_SUB_OPTION(argv[iParsedOptions])) {
        if (0 == _tcsicmp(argv[iParsedOptions], CMDSTR_HELP)||
            (0 == _tcsicmp(argv[iParsedOptions], CMDSTR_H))) {
            iParsedOptions++;
            help = true;
            return false;
        }
    }

    pCommandStruct = AllocateCommandStruct();
    if (NULL == pCommandStruct) {
        _tprintf(_T("%S: AllocateCommandStruct failed\n"), __TFUNCTION__);
        return false;
    }

    pCommandStruct->ulCommand = CMD_SET_DATA_TO_DISK_SIZE_LIMIT;
    memset(&pCommandStruct->data.SetDataToDiskSizeLimit, 0, sizeof(SET_DATA_TO_DISK_SIZE_LIMIT_DATA));

    while((argc > iParsedOptions) && (!IS_MAIN_OPTION(argv[iParsedOptions]))) {
        if ((MountPoint == NULL))
        {
            MountPoint = argv[iParsedOptions];
            if (!GetVolumeGUID(argv[iParsedOptions], pCommandStruct->data.SetDataToDiskSizeLimit.VolumeGUID, 
                sizeof(pCommandStruct->data.SetDataToDiskSizeLimit.VolumeGUID), "ParseSetDataToDiskSizeLimit",
                CMDSTR_SET_DATA_TO_DISK_SIZE_LIMIT))
            {
                return false;
            }
            pCommandStruct->data.SetDataToDiskSizeLimit.MountPoint = argv[iParsedOptions];
            pCommandStruct->data.SetDataToDiskSizeLimit.ulFlags |= SET_DATA_TO_DISK_SIZE_LIMIT_DATA_VALID_GUID;

            iParsedOptions++;
            if (argc <= iParsedOptions) {
                _tprintf(_T("%S: value missing for MountPoint %s\n"), __TFUNCTION__, MountPoint);
                return false;
            }

            pCommandStruct->data.SetDataToDiskSizeLimit.ulDataToDiskSizeLimit = _ttol(argv[iParsedOptions]);
        } else if ((0 == pCommandStruct->data.SetDataToDiskSizeLimit.DefaultDataToDiskSizeLimit) &&
                   (0 == _tcsicmp(argv[iParsedOptions], CMDOPTSTR_SET_DEFAULT))) 
        {
            iParsedOptions++;
            if (argc <= iParsedOptions) {
                _tprintf(_T("%S: value missing for option %s\n"), __TFUNCTION__, CMDOPTSTR_SET_DEFAULT);
                return false;
            }

            pCommandStruct->data.SetDataToDiskSizeLimit.DefaultDataToDiskSizeLimit = _ttol(argv[iParsedOptions]);
        } else if ((0 == pCommandStruct->data.SetDataToDiskSizeLimit.DataToDiskSizeLimitForAllVolumes) &&
            (0 == _tcsicmp(argv[iParsedOptions], CMDOPTSTR_SET_FOR_ALL_VOLUMES)))
        {
            iParsedOptions++;
            if (argc <= iParsedOptions) {
                _tprintf(_T("%S: value missing for option %s\n"), __TFUNCTION__, CMDOPTSTR_SET_FOR_ALL_VOLUMES);
                return false;
            }

            pCommandStruct->data.SetDataToDiskSizeLimit.DataToDiskSizeLimitForAllVolumes = _ttol(argv[iParsedOptions]);
        } else {
            _tprintf(_T("%S: invalid option %s found\n"), __TFUNCTION__, argv[iParsedOptions]);
            return false;
        }

        iParsedOptions++;
    }

    InsertTailList(pCommandLineOptions->CommandList, &pCommandStruct->ListEntry);
    return true;
}

bool
ParseSetDataToMinFreeDiskSizeLimit(TCHAR *argv[], int argc, int &iParsedOptions, PCOMMAND_LINE_OPTIONS pCommandLineOptions)
{
    PCOMMAND_STRUCT     pCommandStruct = NULL;
    PTCHAR              MountPoint = NULL ;

    iParsedOptions++;

    if ((argc > iParsedOptions) && IS_SUB_OPTION(argv[iParsedOptions])) {
        if (0 == _tcsicmp(argv[iParsedOptions], CMDSTR_HELP)||
            (0 == _tcsicmp(argv[iParsedOptions], CMDSTR_H))) {
            iParsedOptions++;
            help = true;
            return false;
        }
    }
    pCommandStruct = AllocateCommandStruct();
    if (NULL == pCommandStruct) {
        _tprintf(_T("%S: AllocateCommandStruct failed\n"), __TFUNCTION__);
        return false;
    }

    pCommandStruct->ulCommand = CMD_SET_MIN_FREE_DATA_TO_DISK_SIZE_LIMIT;
    memset(&pCommandStruct->data.SetMinFreeDataToDiskSizeLimit, 0, sizeof(SET_MIN_FREE_DATA_TO_DISK_SIZE_LIMIT_DATA));

    while((argc > iParsedOptions) && (!IS_MAIN_OPTION(argv[iParsedOptions]))) {
        if ((MountPoint == NULL))
        {
            MountPoint = argv[iParsedOptions];
            if (!GetVolumeGUID(argv[iParsedOptions], pCommandStruct->data.SetMinFreeDataToDiskSizeLimit.VolumeGUID, 
                sizeof(pCommandStruct->data.SetMinFreeDataToDiskSizeLimit.VolumeGUID), "ParseSetDataToMinFreeDiskSizeLimit",
                CMDSTR_SET_MIN_FREE_DATA_TO_DISK_SIZE_LIMIT))
            {
                return false;
            }
            pCommandStruct->data.SetMinFreeDataToDiskSizeLimit.MountPoint = argv[iParsedOptions];
            pCommandStruct->data.SetMinFreeDataToDiskSizeLimit.ulFlags |= SET_MIN_FREE_DATA_TO_DISK_SIZE_LIMIT_DATA_VALID_GUID;

            iParsedOptions++;
            if (argc <= iParsedOptions) {
                _tprintf(_T("%S: value missing for MountPoint %s\n"), __TFUNCTION__, MountPoint);
                return false;
            }

            pCommandStruct->data.SetMinFreeDataToDiskSizeLimit.ulMinFreeDataToDiskSizeLimit = _ttol(argv[iParsedOptions]);
        } else if ((0 == pCommandStruct->data.SetMinFreeDataToDiskSizeLimit.DefaultMinFreeDataToDiskSizeLimit) &&
                   (0 == _tcsicmp(argv[iParsedOptions], CMDOPTSTR_SET_DEFAULT))) 
        {
            iParsedOptions++;
            if (argc <= iParsedOptions) {
                _tprintf(_T("%S: value missing for option %s\n"), __TFUNCTION__, CMDOPTSTR_SET_DEFAULT);
                return false;
            }

            pCommandStruct->data.SetMinFreeDataToDiskSizeLimit.DefaultMinFreeDataToDiskSizeLimit = _ttol(argv[iParsedOptions]);
        } else if ((0 == pCommandStruct->data.SetMinFreeDataToDiskSizeLimit.MinFreeDataToDiskSizeLimitForAllVolumes) &&
            (0 == _tcsicmp(argv[iParsedOptions], CMDOPTSTR_SET_FOR_ALL_VOLUMES)))
        {
            iParsedOptions++;
            if (argc <= iParsedOptions) {
                _tprintf(_T("%S: value missing for option %s\n"), __TFUNCTION__, CMDOPTSTR_SET_FOR_ALL_VOLUMES);
                return false;
            }

            pCommandStruct->data.SetMinFreeDataToDiskSizeLimit.MinFreeDataToDiskSizeLimitForAllVolumes = _ttol(argv[iParsedOptions]);
        } else {
            _tprintf(_T("%S: invalid option %s found\n"), __TFUNCTION__, argv[iParsedOptions]);
            return false;
        }

        iParsedOptions++;
    }

    InsertTailList(pCommandLineOptions->CommandList, &pCommandStruct->ListEntry);
    return true;
}

bool
ParseSetDataNotifySizeLimit(TCHAR *argv[], int argc, int &iParsedOptions, PCOMMAND_LINE_OPTIONS pCommandLineOptions)
{
    PCOMMAND_STRUCT     pCommandStruct = NULL;
    PTCHAR              MountPoint  = NULL ;
    

    iParsedOptions++;

    if ((argc > iParsedOptions) && IS_SUB_OPTION(argv[iParsedOptions])) {
        if (0 == _tcsicmp(argv[iParsedOptions], CMDSTR_HELP)||
            (0 == _tcsicmp(argv[iParsedOptions], CMDSTR_H))) {
            iParsedOptions++;
            help = true;
            return false;
        }
    }
    pCommandStruct = AllocateCommandStruct();
    if (NULL == pCommandStruct) {
        _tprintf(_T("%S: AllocateCommandStruct failed\n"), __TFUNCTION__);
        return false;
    }

    pCommandStruct->ulCommand = CMD_SET_DATA_NOTIFY_SIZE_LIMIT;
    memset(&pCommandStruct->data.SetDataNotifySizeLimit, 0, sizeof(SET_DATA_NOTIFY_SIZE_LIMIT_DATA));

    while((argc > iParsedOptions) && (!IS_MAIN_OPTION(argv[iParsedOptions]))) {
        if ((MountPoint == NULL))
        {
            MountPoint = argv[iParsedOptions];
            if (!GetVolumeGUID(argv[iParsedOptions], pCommandStruct->data.SetDataNotifySizeLimit.VolumeGUID, 
                sizeof(pCommandStruct->data.SetDataNotifySizeLimit.VolumeGUID), "ParseSetDataNotifySizeLimit",
                CMDSTR_SET_DATA_NOTIFY_SIZE_LIMIT))
            {
                return false;
            }
            pCommandStruct->data.SetDataNotifySizeLimit.MountPoint = argv[iParsedOptions];
            pCommandStruct->data.SetDataNotifySizeLimit.ulFlags |= SET_DATA_NOTIFY_SIZE_LIMIT_DATA_VALID_GUID;

            iParsedOptions++;
            if (argc <= iParsedOptions) {
                _tprintf(_T("%S: value missing for MountPoint %s \n"), __TFUNCTION__, MountPoint);
                return false;
            }

            pCommandStruct->data.SetDataNotifySizeLimit.ulLimit = _ttol(argv[iParsedOptions]);
        } else if ((0 == pCommandStruct->data.SetDataNotifySizeLimit.ulDefaultLimit) &&
                   (0 == _tcsicmp(argv[iParsedOptions], CMDOPTSTR_SET_DEFAULT))) 
        {
            iParsedOptions++;
            if (argc <= iParsedOptions) {
                _tprintf(_T("%S: value missing for option %s\n"), __TFUNCTION__, CMDOPTSTR_SET_DEFAULT);
                return false;
            }

            pCommandStruct->data.SetDataNotifySizeLimit.ulDefaultLimit = _ttol(argv[iParsedOptions]);
        } else if ((0 == pCommandStruct->data.SetDataNotifySizeLimit.ulLimitForAllVolumes) &&
            (0 == _tcsicmp(argv[iParsedOptions], CMDOPTSTR_SET_FOR_ALL_VOLUMES)))
        {
            iParsedOptions++;
            if (argc <= iParsedOptions) {
                _tprintf(_T("%S: value missing for option %s\n"), __TFUNCTION__, CMDOPTSTR_SET_FOR_ALL_VOLUMES);
                return false;
            }

            pCommandStruct->data.SetDataNotifySizeLimit.ulLimitForAllVolumes = _ttol(argv[iParsedOptions]);
        } else {
            _tprintf(_T("%S: invalid option %s found\n"), __TFUNCTION__, argv[iParsedOptions]);
            return false;
        }

        iParsedOptions++;
    }

    if (pCommandStruct->data.SetDataNotifySizeLimit.ulFlags & SET_DATA_NOTIFY_SIZE_LIMIT_DATA_VALID_GUID) {
        if (0 == pCommandStruct->data.SetDataNotifySizeLimit.ulLimit) {
            _tprintf(_T("%S: Invalid value specified for volume\n"), __TFUNCTION__);
            return false;
        }
    }

    InsertTailList(pCommandLineOptions->CommandList, &pCommandStruct->ListEntry);
    return true;
}

bool
ParseSetDriverInfo(TCHAR *argv[], int argc, int &iParsedOptions, PCOMMAND_LINE_OPTIONS pCommandLineOptions)
{
    PCOMMAND_STRUCT     pCommandStruct = NULL;

    iParsedOptions++;

    if ((argc > iParsedOptions) && IS_SUB_OPTION(argv[iParsedOptions])) {
        if (0 == _tcsicmp(argv[iParsedOptions], CMDSTR_HELP)||
            (0 == _tcsicmp(argv[iParsedOptions], CMDSTR_H))) {
            iParsedOptions++;
            help = true;
            return false;
        }
    }
    pCommandStruct = AllocateCommandStruct();
    if (NULL == pCommandStruct) {
        _tprintf(_T("ParseSetControlFlagsCommand: AllocateCommandStruct failed\n"));
        return false;
    }

    pCommandStruct->ulCommand = CMD_SET_DRIVER_CONFIG;

    pCommandStruct->data.SetDriverConfig.ulFlags1 = 0;
    pCommandStruct->data.SetDriverConfig.ulFlags2 = 0;

    if((argc > iParsedOptions) && IS_SUB_OPTION(argv[iParsedOptions])) {
        if (0 == _tcsicmp(argv[iParsedOptions], CMDOPTSTR_HIGH_WATER_MARK_SERVICE_NOT_STARTED)) {
            pCommandStruct->data.SetDriverConfig.ulFlags1 |= DRIVER_CONFIG_FLAGS_HWM_SERVICE_NOT_STARTED;
            iParsedOptions++;
            if (argc <= iParsedOptions) {
                _tprintf(_T("\n%S: value missing for option %s\n"), __TFUNCTION__, CMDOPTSTR_HIGH_WATER_MARK_SERVICE_NOT_STARTED);
                return false;
            }

            pCommandStruct->data.SetDriverConfig.ulhwmServiceNotStarted = GetSizeFromString(argv[iParsedOptions]);
            if (pCommandStruct->data.SetDriverConfig.ulhwmServiceNotStarted < 0) {
                _tprintf(_T("\n%S: Incorrect value set for option %s\n"), __TFUNCTION__, CMDOPTSTR_HIGH_WATER_MARK_SERVICE_NOT_STARTED);
                return false;
            }
        } else if (0 == _tcsicmp(argv[iParsedOptions], CMDOPTSTR_HIGH_WATER_MARK_SERVICE_RUNNING)) {
            pCommandStruct->data.SetDriverConfig.ulFlags1 |= DRIVER_CONFIG_FLAGS_HWM_SERVICE_RUNNING;
            iParsedOptions++;
            if (argc <= iParsedOptions) {
                _tprintf(_T("\n%S: value missing for option %s\n"), __TFUNCTION__, CMDOPTSTR_HIGH_WATER_MARK_SERVICE_RUNNING);
                return false;
            }

            pCommandStruct->data.SetDriverConfig.ulhwmServiceRunning = GetSizeFromString(argv[iParsedOptions]);
            if (pCommandStruct->data.SetDriverConfig.ulhwmServiceRunning < 0) {
                _tprintf(_T("\n%S: Incorrect value set for option %s\n"), __TFUNCTION__, CMDOPTSTR_HIGH_WATER_MARK_SERVICE_RUNNING);
                return false;
            }
        } else if (0 == _tcsicmp(argv[iParsedOptions], CMDOPTSTR_HIGH_WATER_MARK_SERVICE_SHUTDOWN)) {
            pCommandStruct->data.SetDriverConfig.ulFlags1 |= DRIVER_CONFIG_FLAGS_HWM_SERVICE_SHUTDOWN;
            iParsedOptions++;
            if (argc <= iParsedOptions) {
                _tprintf(_T("\n%S: value missing for option %s\n"), __TFUNCTION__, CMDOPTSTR_HIGH_WATER_MARK_SERVICE_SHUTDOWN);
                return false;
            }

            pCommandStruct->data.SetDriverConfig.ulhwmServiceShutdown = GetSizeFromString(argv[iParsedOptions]);
            if (pCommandStruct->data.SetDriverConfig.ulhwmServiceShutdown < 0) {
                _tprintf(_T("\n%S: Incorrect value set for option %s\n"), __TFUNCTION__, CMDOPTSTR_HIGH_WATER_MARK_SERVICE_SHUTDOWN);
                return false;
            }
        } else if (0 == _tcsicmp(argv[iParsedOptions], CMDOPTSTR_LOW_WATER_MARK_SERVICE_RUNNING)) {
            pCommandStruct->data.SetDriverConfig.ulFlags1 |= DRIVER_CONFIG_FLAGS_LWM_SERVICE_RUNNING;
            iParsedOptions++;
            if (argc <= iParsedOptions) {
                _tprintf(_T("\n%S: value missing for option %s\n"), __TFUNCTION__, CMDOPTSTR_LOW_WATER_MARK_SERVICE_RUNNING);
                return false;
            }

            pCommandStruct->data.SetDriverConfig.ullwmServiceRunning = GetSizeFromString(argv[iParsedOptions]);
            if (pCommandStruct->data.SetDriverConfig.ullwmServiceRunning < 0) {
                _tprintf(_T("\n%S: Incorrect value set for option %s\n"), __TFUNCTION__, CMDOPTSTR_LOW_WATER_MARK_SERVICE_RUNNING);
                return false;
            }
        } else if (0 == _tcsicmp(argv[iParsedOptions], CMDOPTSTR_CHANGES_TO_PURGE_ON_HIGH_WATER_MARK)) {
            pCommandStruct->data.SetDriverConfig.ulFlags1 |= DRIVER_CONFIG_FLAGS_CHANGES_TO_PURGE_ON_HWM;
            iParsedOptions++;
            if (argc <= iParsedOptions) {
                _tprintf(_T("\n%S: value missing for option %s\n"), __TFUNCTION__, CMDOPTSTR_CHANGES_TO_PURGE_ON_HIGH_WATER_MARK);
                return false;
            }

            pCommandStruct->data.SetDriverConfig.ulChangesToPergeOnhwm = GetSizeFromString(argv[iParsedOptions]);
            if (pCommandStruct->data.SetDriverConfig.ulChangesToPergeOnhwm < 0) {
                _tprintf(_T("\n%S: Incorrect value set for option %s\n"), __TFUNCTION__, CMDOPTSTR_CHANGES_TO_PURGE_ON_HIGH_WATER_MARK);
                return false;
            }
        } else if (0 == _tcsicmp(argv[iParsedOptions], CMDOPTSTR_TIME_IN_SEC_BETWEEN_MEMORY_ALLOC_FAILURES)) {
            pCommandStruct->data.SetDriverConfig.ulFlags1 |= DRIVER_CONFIG_FLAGS_TIME_BETWEEN_MEM_FAIL_LOG;
            iParsedOptions++;
            if (argc <= iParsedOptions) {
                _tprintf(_T("\n%S: value missing for option %s\n"), __TFUNCTION__, CMDOPTSTR_TIME_IN_SEC_BETWEEN_MEMORY_ALLOC_FAILURES);
                return false;
            }

            pCommandStruct->data.SetDriverConfig.ulSecsBetweenMemAllocFailureEvents = _ttol(argv[iParsedOptions]);
            if (_ttol(argv[iParsedOptions]) < 0) {
                _tprintf(_T("\n%S: Incorrect value set for option %s\n"), __TFUNCTION__, CMDOPTSTR_TIME_IN_SEC_BETWEEN_MEMORY_ALLOC_FAILURES);
                return false;
            }
        } else if (0 == _tcsicmp(argv[iParsedOptions], CMDOPTSTR_MAX_WAIT_TIME_IN_SEC_ON_FAILOVER)) {
            pCommandStruct->data.SetDriverConfig.ulFlags1 |= DRIVER_CONFIG_FLAGS_MAX_WAIT_TIME_ON_FAILOVER;
            iParsedOptions++;
            if (argc <= iParsedOptions) {
                _tprintf(_T("\n%S: value missing for option %s\n"), __TFUNCTION__, CMDOPTSTR_MAX_WAIT_TIME_IN_SEC_ON_FAILOVER);
                return false;
            }

            pCommandStruct->data.SetDriverConfig.ulSecsMaxWaitTimeForFailOver = _ttol(argv[iParsedOptions]);
            if (_ttol(argv[iParsedOptions]) < 0) {
                _tprintf(_T("\n%S: Incorrect value set for option %s\n"), __TFUNCTION__, CMDOPTSTR_MAX_WAIT_TIME_IN_SEC_ON_FAILOVER);
                return false;
            }
        } else if (0 == _tcsicmp(argv[iParsedOptions], CMDOPTSTR_MAX_LOCKED_DATA_BLOCKS)) {
            pCommandStruct->data.SetDriverConfig.ulFlags1 |= DRIVER_CONFIG_FLAGS_MAX_LOCKED_DATA_BLOCKS;
            iParsedOptions++;
            if (argc <= iParsedOptions) {
                _tprintf(_T("\n%S: value missing for option %s\n"), __TFUNCTION__, CMDOPTSTR_MAX_LOCKED_DATA_BLOCKS);
                return false;
            }

            pCommandStruct->data.SetDriverConfig.ulMaxLockedDataBlocks = _ttol(argv[iParsedOptions]);
            if (_ttol(argv[iParsedOptions]) < 0) {
                _tprintf(_T("\n%S: Incorrect value set for option %s\n"), __TFUNCTION__, CMDOPTSTR_MAX_LOCKED_DATA_BLOCKS);
                return false;
            }
        } else if (0 == _tcsicmp(argv[iParsedOptions], CMDOPTSTR_MAX_NON_PAGED_POOL_IN_MB)) {
            pCommandStruct->data.SetDriverConfig.ulFlags1 |= DRIVER_CONFIG_FLAGS_MAX_NON_PAGED_POOL;
            iParsedOptions++;
            if (argc <= iParsedOptions) {
                _tprintf(_T("\n%S: value missing for option %s\n"), __TFUNCTION__, CMDOPTSTR_MAX_NON_PAGED_POOL_IN_MB);
                return false;
            }

            pCommandStruct->data.SetDriverConfig.ulMaxNonPagedPoolInMB = _ttol(argv[iParsedOptions]);
            if (_ttol(argv[iParsedOptions]) < 0) {
                _tprintf(_T("\n%S: Incorrect value set for option %s\n"), __TFUNCTION__, CMDOPTSTR_MAX_NON_PAGED_POOL_IN_MB);
                return false;		    
            }
        } else if (0 == _tcsicmp(argv[iParsedOptions], CMDOPTSTR_MAX_WAIT_TIME_FOR_IRP_COMPLETION_IN_MINUTES)) {
            pCommandStruct->data.SetDriverConfig.ulFlags1 |= DRIVER_CONFIG_FLAGS_MAX_WAIT_TIME_FOR_IRP_COMPLETION;
            iParsedOptions++;
            if (argc <= iParsedOptions) {
                _tprintf(_T("\n%S: value missing for option %s\n"), __TFUNCTION__, CMDOPTSTR_MAX_WAIT_TIME_FOR_IRP_COMPLETION_IN_MINUTES);
                return false;
            }

            pCommandStruct->data.SetDriverConfig.ulMaxWaitTimeForIrpCompletionInMinutes = _ttol(argv[iParsedOptions]);
            if (_ttol(argv[iParsedOptions]) < 0) {
                _tprintf(_T("\n%S: Incorrect value set for option %s\n"), __TFUNCTION__, CMDOPTSTR_MAX_WAIT_TIME_FOR_IRP_COMPLETION_IN_MINUTES);
                return false;
            }
        } else if (0 == _tcsicmp(argv[iParsedOptions], CMDOPTSTR_DEFAULT_FILTERING_FOR_NEW_CLUSTER_VOLUMES)) {
            pCommandStruct->data.SetDriverConfig.ulFlags1 |= DRIVER_CONFIG_FLAGS_DEFAULT_FILTERING_FOR_NEW_CLUSTER_VOLUMES;
            iParsedOptions++;
            if (argc <= iParsedOptions) {
                _tprintf(_T("\n%S: value missing for option %s\n"), __TFUNCTION__, CMDOPTSTR_DEFAULT_FILTERING_FOR_NEW_CLUSTER_VOLUMES);
                return false;
            }

            pCommandStruct->data.SetDriverConfig.bDisableVolumeFilteringForNewClusterVolumes = (_ttol(argv[iParsedOptions]) > 0? true : false);
        } else if (0 == _tcsicmp(argv[iParsedOptions], CMDOPTSTR_DEFAULT_DATA_FILTER_FOR_NEW_VOLUMES)) {
            pCommandStruct->data.SetDriverConfig.ulFlags1 |= DRIVER_CONFIG_FLAGS_DATA_FILTER_FOR_NEW_VOLUMES;
            iParsedOptions++;
            if (argc <= iParsedOptions) {
                _tprintf(_T("\n%S: value missing for option %s\n"), __TFUNCTION__, CMDOPTSTR_DEFAULT_DATA_FILTER_FOR_NEW_VOLUMES);
                return false;
            }

            pCommandStruct->data.SetDriverConfig.bEnableDataFilteringForNewVolumes = (_ttol(argv[iParsedOptions]) > 0? true : false);
        } else if (0 == _tcsicmp(argv[iParsedOptions], CMDOPTSTR_DEFAULT_FILTERING_FOR_NEW_VOLUMES)) {
            pCommandStruct->data.SetDriverConfig.ulFlags1 |= DRIVER_CONFIG_FLAGS_FITERING_DISABLED_FOR_NEW_VOLUMES;
            iParsedOptions++;
            if (argc <= iParsedOptions) {
                _tprintf(_T("\n%S: value missing for option %s\n"), __TFUNCTION__, CMDOPTSTR_DEFAULT_FILTERING_FOR_NEW_VOLUMES);
                return false;
            }

            pCommandStruct->data.SetDriverConfig.bDisableVolumeFilteringForNewVolumes = (_ttol(argv[iParsedOptions]) > 0? true : false);
        } else if (0 == _tcsicmp(argv[iParsedOptions], CMDOPTSTR_DEFAULT_FILTERING_ENABLE_DATA_CAPTURE)) {
            pCommandStruct->data.SetDriverConfig.ulFlags1 |= DRIVER_CONFIG_FLAGS_ENABLE_DATA_CAPTURE;
            iParsedOptions++;
            if (argc <= iParsedOptions) {
                _tprintf(_T("\n%S: value missing for option %s\n"), __TFUNCTION__, CMDOPTSTR_DEFAULT_FILTERING_ENABLE_DATA_CAPTURE);
                return false;
            }

            pCommandStruct->data.SetDriverConfig.bEnableDataCapture = (_ttol(argv[iParsedOptions]) > 0? true : false);
        } else if (0 == _tcsicmp(argv[iParsedOptions], CMDOPTSTR_MAX_DATA_ALLOCATION_LIMIT)) {
            pCommandStruct->data.SetDriverConfig.ulFlags1 |= DRIVER_CONFIG_FLAGS_MAX_DATA_ALLOCATION_LIMIT;
            iParsedOptions++;
            if (argc <= iParsedOptions) {
                _tprintf(_T("\n%S: value missing for option %s\n"), __TFUNCTION__, CMDOPTSTR_MAX_DATA_ALLOCATION_LIMIT);
                return false;
            }

            pCommandStruct->data.SetDriverConfig.ulMaxDataAllocLimitInPercentage = _ttol(argv[iParsedOptions]);
            if ((_ttol(argv[iParsedOptions]) < 0) || (_ttol(argv[iParsedOptions]) > 100)) {
                _tprintf(_T("\n%S: Incorrect value set for option %s\n"), __TFUNCTION__, CMDOPTSTR_MAX_DATA_ALLOCATION_LIMIT);
                return false;
            }
        } else if (0 == _tcsicmp(argv[iParsedOptions], CMDOPSTR_AVAILABLE_DATA_BLOCK_COUNT)) {
            pCommandStruct->data.SetDriverConfig.ulFlags1 |= DRIVER_CONFIG_FLAGS_AVAILABLE_DATA_BLOCK_COUNT;
            iParsedOptions++;
            if (argc <= iParsedOptions) {
                _tprintf(_T("\n%S: value missing for option %s\n"), __TFUNCTION__, CMDOPSTR_AVAILABLE_DATA_BLOCK_COUNT);
                return false;
            }

            pCommandStruct->data.SetDriverConfig.ulAvailableDataBlockCountInPercentage = _ttol(argv[iParsedOptions]);
            if ((_ttol(argv[iParsedOptions]) < 0) || (_ttol(argv[iParsedOptions]) > 100)) {
                _tprintf(_T("\n%S: Incorrect value set for option %s\n"), __TFUNCTION__, CMDOPSTR_AVAILABLE_DATA_BLOCK_COUNT);
                return false;
            }
        } else if (0 == _tcsicmp(argv[iParsedOptions], CMDOPSTR_MAX_COALESCED_METADATA_CHANGE_SIZE)) {
            pCommandStruct->data.SetDriverConfig.ulFlags1 |= DRIVER_CONFIG_FLAGS_MAX_COALESCED_METADATA_CHANGE_SIZE;
            iParsedOptions++;
            if (argc <= iParsedOptions) {
                _tprintf(_T("\n%S: value missing for option %s\n"), __TFUNCTION__, CMDOPSTR_MAX_COALESCED_METADATA_CHANGE_SIZE);
                return false;
            }

            pCommandStruct->data.SetDriverConfig.ulMaxCoalescedMetaDataChangeSize = _ttol(argv[iParsedOptions]);
            if (_ttol(argv[iParsedOptions]) < 0) {
                _tprintf(_T("\n%S: Incorrect value set for option %s\n"), __TFUNCTION__, CMDOPSTR_MAX_COALESCED_METADATA_CHANGE_SIZE);
                return false;
            }
        } else if (0 == _tcsicmp(argv[iParsedOptions], CMDOPSTR_DO_VALIDATION_CHECKS)) {
            pCommandStruct->data.SetDriverConfig.ulFlags1 |= DRIVER_CONFIG_FLAGS_VALIDATION_LEVEL;
            iParsedOptions++;
            if (argc <= iParsedOptions) {
                _tprintf(_T("\n%S: value missing for option %s\n"), __TFUNCTION__, CMDOPSTR_DO_VALIDATION_CHECKS);
                return false;
            }

            pCommandStruct->data.SetDriverConfig.ulValidationLevel = _ttol(argv[iParsedOptions]);
            if (_ttol(argv[iParsedOptions]) < 0) {
                _tprintf(_T("\n%S: Incorrect value set for option %s\n"), __TFUNCTION__, CMDOPSTR_DO_VALIDATION_CHECKS);
                return false;
            }
#ifdef DBG
        } else if (0 == _tcsicmp(argv[iParsedOptions], CMDOPSTR_DATA_POOL_SIZE)) {
            pCommandStruct->data.SetDriverConfig.ulFlags1 |= DRIVER_CONFIG_DATA_POOL_SIZE;
            iParsedOptions++;
            if (argc <= iParsedOptions) {
                _tprintf(_T("\n%S: value missing for option %s\n"), __TFUNCTION__, CMDOPSTR_DATA_POOL_SIZE);
                return false;
            }

            pCommandStruct->data.SetDriverConfig.ulDataPoolSize = _ttol(argv[iParsedOptions]);
            if (_ttol(argv[iParsedOptions]) < 0) {
                _tprintf(_T("\n%S: Incorrect value set for option %s\n"), __TFUNCTION__, CMDOPSTR_DATA_POOL_SIZE);
                return false;		    
            }
#endif
        } else if (0 == _tcsicmp(argv[iParsedOptions], CMDOPSTR_MAX_DATA_POOL_SIZE_PER_VOLUME)) {
            pCommandStruct->data.SetDriverConfig.ulFlags1 |= DRIVER_CONFIG_MAX_DATA_POOL_SIZE_PER_VOLUME;
            iParsedOptions++;
            if (argc <= iParsedOptions) {
                _tprintf(_T("\n%S: value missing for option %s\n"), __TFUNCTION__, CMDOPSTR_MAX_DATA_POOL_SIZE_PER_VOLUME);
                return false;
            }

            pCommandStruct->data.SetDriverConfig.ulMaxDataSizePerVolume = _ttol(argv[iParsedOptions]);
            if (_ttol(argv[iParsedOptions]) < 0) {
                _tprintf(_T("\n%S: Incorrect value set for option %s\n"), __TFUNCTION__, CMDOPSTR_MAX_DATA_POOL_SIZE_PER_VOLUME);
                return false;
            }
        } else if (0 == _tcsicmp(argv[iParsedOptions], CMDOPSTR_THRESHOLD_FOR_FILE_WRITE)) {
            pCommandStruct->data.SetDriverConfig.ulFlags1 |= DRIVER_CONFIG_FLAGS_VOL_THRESHOLD_FOR_FILE_WRITE;
            iParsedOptions++;
            if (argc <= iParsedOptions) {
                _tprintf(_T("\n%S: value missing for option %s\n"), __TFUNCTION__, CMDOPSTR_THRESHOLD_FOR_FILE_WRITE);
                return false;
            }

            pCommandStruct->data.SetDriverConfig.ulDaBThresholdPerVolumeForFileWrite = _ttol(argv[iParsedOptions]);            
            if ((_ttol(argv[iParsedOptions]) < 0) || ((_ttol(argv[iParsedOptions]) > 100))) {		    
                _tprintf(_T("\n%S: Incorrect value set for option %s. It should be between 0 to 100\n"), __TFUNCTION__, CMDOPSTR_THRESHOLD_FOR_FILE_WRITE);
                return false;
            }
        } else if (0 == _tcsicmp(argv[iParsedOptions], CMDOPSTR_FREE_THRESHOLD_FOR_FILE_WRITE)) {
            pCommandStruct->data.SetDriverConfig.ulFlags1 |= DRIVER_CONFIG_FLAGS_FREE_THRESHOLD_FOR_FILE_WRITE;
            iParsedOptions++;
            if (argc <= iParsedOptions) {
                _tprintf(_T("\n%S: value missing for option %s\n"), __TFUNCTION__, CMDOPSTR_FREE_THRESHOLD_FOR_FILE_WRITE);
                return false;
            }

            pCommandStruct->data.SetDriverConfig.ulDaBFreeThresholdForFileWrite = _ttol(argv[iParsedOptions]);            
            if ((_ttol(argv[iParsedOptions]) < 0) || ((_ttol(argv[iParsedOptions]) > 100))) {
                _tprintf(_T("\n%S: Incorrect value set for option %s. It should be between 0 to 100\n"), __TFUNCTION__, CMDOPSTR_FREE_THRESHOLD_FOR_FILE_WRITE);
                return false;
            }
        } else if (0 == _tcsicmp(argv[iParsedOptions], CMDOPSTR_MAX_DATA_PER_NON_DATA_MODE_DIRTY_BLOCK)) {
            pCommandStruct->data.SetDriverConfig.ulFlags1 |= DRIVER_CONFIG_FLAGS_MAX_DATA_PER_NON_DATA_MODE_DIRTY_BLOCK;
            iParsedOptions++;
            if (argc <= iParsedOptions) {
                _tprintf(_T("\n%S: value missing for option %s\n"), __TFUNCTION__, CMDOPSTR_MAX_DATA_PER_NON_DATA_MODE_DIRTY_BLOCK);
                return false;
            }

            pCommandStruct->data.SetDriverConfig.ulMaxDataSizePerNonDataModeDirtyBlock = _ttol(argv[iParsedOptions]);            
            if ((_ttol(argv[iParsedOptions]) < MIN_DATA_SIZE_PER_NON_DATA_MODE_DIRTY_BLOCK_IN_MB) || ((_ttol(argv[iParsedOptions]) > MAX_DATA_SIZE_PER_NON_DATA_MODE_DIRTY_BLOCK_IN_MB))) {
                _tprintf(_T("\n%S: Incorrect value set for option %s. Min Value is %d Max Value is %d\n"), __TFUNCTION__, CMDOPSTR_MAX_DATA_PER_NON_DATA_MODE_DIRTY_BLOCK, 
					    MIN_DATA_SIZE_PER_NON_DATA_MODE_DIRTY_BLOCK_IN_MB, MAX_DATA_SIZE_PER_NON_DATA_MODE_DIRTY_BLOCK_IN_MB);
                return false;
            }
        } else if (0 == _tcsicmp(argv[iParsedOptions], CMDOPSTR_MAX_DATA_PER_DATA_MODE_DIRTY_BLOCK)) {
            pCommandStruct->data.SetDriverConfig.ulFlags1 |= DRIVER_CONFIG_FLAGS_MAX_DATA_PER_DATA_MODE_DIRTY_BLOCK;
            iParsedOptions++;
            if (argc <= iParsedOptions) {
                _tprintf(_T("\n%S: value missing for option %s\n"), __TFUNCTION__, CMDOPSTR_MAX_DATA_PER_DATA_MODE_DIRTY_BLOCK);
                return false;
            }

            pCommandStruct->data.SetDriverConfig.ulMaxDataSizePerDataModeDirtyBlock = _ttol(argv[iParsedOptions]);            
            if ((_ttol(argv[iParsedOptions]) < MIN_DATA_SIZE_PER_DATA_MODE_DIRTY_BLOCK_IN_MB) || ((_ttol(argv[iParsedOptions]) > MAX_DATA_SIZE_PER_DATA_MODE_DIRTY_BLOCK_IN_MB))) {
                _tprintf(_T("\n%S: Incorrect value set for option %s. Min Value is %d Max Value is %d\n"), __TFUNCTION__, CMDOPSTR_MAX_DATA_PER_DATA_MODE_DIRTY_BLOCK, 
					    MIN_DATA_SIZE_PER_DATA_MODE_DIRTY_BLOCK_IN_MB, MAX_DATA_SIZE_PER_DATA_MODE_DIRTY_BLOCK_IN_MB);
                return false;
            }
        } else if ((0 == _tcsicmp(argv[iParsedOptions], CMDOPSTR_DATA_LOG_DIRECTORY)) || 
                (0 == _tcsicmp(argv[iParsedOptions], CMDOPSTR_DATA_LOG_DIRECTORY_ALL))) {
            TCHAR *func = argv[iParsedOptions];
            if (0 == _tcsicmp(argv[iParsedOptions], CMDOPSTR_DATA_LOG_DIRECTORY)) {
                 pCommandStruct->data.SetDriverConfig.ulFlags1 |= DRIVER_CONFIG_FLAGS_DATA_LOG_DIRECTORY;
            } else {
                 pCommandStruct->data.SetDriverConfig.ulFlags1 |= DRIVER_CONFIG_FLAGS_DATA_LOG_DIRECTORY_ALL;		    
            }
            iParsedOptions++;
            if (argc <= iParsedOptions) {
                _tprintf(_T("\n%S: value missing for option %s\n"), __TFUNCTION__, func);
                return false;
            }
            size_t usLength = _tcslen(argv[iParsedOptions]);
            TCHAR *path = argv[iParsedOptions];
            if (!ValidatePath(path, usLength)) {
                _tprintf(_T("\n%S: Incorrect value set for option %s. Given Path is %s\n"), __TFUNCTION__, func, path);
                return false;
            }
            pCommandStruct->data.SetDriverConfig.DataFile.usLength = usLength * sizeof(TCHAR);
            memmove(pCommandStruct->data.SetDriverConfig.DataFile.FileName, path, usLength * sizeof(WCHAR));
            pCommandStruct->data.SetDriverConfig.DataFile.FileName[usLength] = L'\0';
        }
        else if (0 == _tcsicmp(argv[iParsedOptions], CMDOPTSTR_FS_FLUSH_PRE_SHUTDOWN)) {
            TCHAR *func = argv[iParsedOptions];
            iParsedOptions++;
            if (argc <= iParsedOptions) {
                _tprintf(_T("\n%S: value missing for option %s\n"), __TFUNCTION__, func);
                return false;
            }

            ULONG   ulEnableFsFlushPreShutdown = _ttol(argv[iParsedOptions]);
            pCommandStruct->data.SetDriverConfig.ulFlags1 |= DRIVER_CONFIG_FLAGS_ENABLE_FS_FLUSH;
            pCommandStruct->data.SetDriverConfig.bEnableFSFlushPreShutdown = (0 != ulEnableFsFlushPreShutdown);
        }
        else {
            _tprintf(_T("\n%S: InValid Input Parameters.\n"), __TFUNCTION__);
            return false;
        }
        iParsedOptions++;
    }

    if (argc != iParsedOptions) {
        _tprintf(_T("\n%S: InValid Input Parameters. Try one option at a time\n"), __TFUNCTION__);
        return false;
    }

    InsertTailList(pCommandLineOptions->CommandList, &pCommandStruct->ListEntry);
    return true;
}

bool ParseGetLcn(TCHAR *argv[], int argc, int &iParsedOptions, PCOMMAND_LINE_OPTIONS pCommandLineOptions)
{
    PCOMMAND_STRUCT     pCommandStruct = NULL;

    iParsedOptions++;

    if ((argc > iParsedOptions) && IS_SUB_OPTION(argv[iParsedOptions])) {
        if (0 == _tcsicmp(argv[iParsedOptions], CMDSTR_HELP)||
            (0 == _tcsicmp(argv[iParsedOptions], CMDSTR_H))) {
            iParsedOptions++;
            help = true;
            return false;
        }
    }

    if (argc <= iParsedOptions) {
        help = true;
        return false;
    }

    pCommandStruct = AllocateCommandStruct();
    if (NULL == pCommandStruct) {
        _tprintf(_T("ParseSetControlFlagsCommand: AllocateCommandStruct failed\n"));
        return false;
    }

    pCommandStruct->ulCommand = CMD_GET_LCN;                
    pCommandStruct->data.GetLcn.StartingVcn = 0;

    if ((argc > iParsedOptions)) {
        size_t usLength = _tcslen(argv[iParsedOptions]);
        pCommandStruct->data.GetLcn.usLength = usLength;
        pCommandStruct->data.GetLcn.FileName = argv[iParsedOptions];
	if (usLength >= FILE_NAME_SIZE_LCN) {
            _tprintf(_T("File name %s size %d is greater than  MAX %d size \n"), pCommandStruct->data.GetLcn.FileName, usLength, FILE_NAME_SIZE_LCN);
            return false;		
	}            
        iParsedOptions++;
    }

    if ((argc > iParsedOptions) && IS_SUB_OPTION(argv[iParsedOptions])) {
        if (0 == _tcsicmp(argv[iParsedOptions], CMDOPTSTR_VCN)) {
            iParsedOptions++;
            if (iParsedOptions >= argc) {
                _tprintf(_T("ParseGetLcn: missing parameter for %s\n"),
                                CMDOPTSTR_VCN);
                return false;
            }
            if (IS_NOT_COMMAND_PAR(argv[iParsedOptions])) {
                _tprintf(_T("ParseGetLcn: Invalid parameter %s for %s\n"), 
                         argv[iParsedOptions], CMDOPTSTR_VCN);
                return false;
            }
            pCommandStruct->data.GetLcn.StartingVcn = _tcstoul(argv[iParsedOptions], NULL, 0);
            iParsedOptions++;
	    }
    }
    if (argc != iParsedOptions) {
        _tprintf(_T("\n%S: InValid Input Parameters.\n"), __TFUNCTION__);
        return false;
    }
    InsertTailList(pCommandLineOptions->CommandList, &pCommandStruct->ListEntry);
    return true;
}

#ifdef _CUSTOM_COMMAND_CODE_
bool
ParseCustomCommand(TCHAR *argv[], int argc, int &iParsedOptions, PCOMMAND_LINE_OPTIONS pCommandLineOptions)
{
    PCOMMAND_STRUCT     pCommandStruct = NULL;

    iParsedOptions++;

    if ((argc > iParsedOptions) && IS_SUB_OPTION(argv[iParsedOptions])) {
        if (0 == _tcsicmp(argv[iParsedOptions], CMDSTR_HELP)||
            (0 == _tcsicmp(argv[iParsedOptions], CMDSTR_H))) {
            iParsedOptions++;
            help = true;
            return false;
        }
    }
    pCommandStruct = AllocateCommandStruct();
    if (NULL == pCommandStruct) {
        _tprintf(_T("ParseSetControlFlagsCommand: AllocateCommandStruct failed\n"));
        return false;
    }

    pCommandStruct->ulCommand = CMD_CUSTOM_COMMAND;

    pCommandStruct->data.CustomCommandData.ulParam1 = 0;
    pCommandStruct->data.CustomCommandData.ulParam2 = 0;

    while((argc > iParsedOptions) && IS_SUB_OPTION(argv[iParsedOptions])) {
        if (0 == _tcsicmp(argv[iParsedOptions], CMDOPTSTR_PARAM1)) {
            iParsedOptions++;
            if (argc <= iParsedOptions) {
                _tprintf(_T("%S: value missing for option %s\n"), __TFUNCTION__, CMDOPTSTR_PARAM1);
                return false;
            }

            pCommandStruct->data.CustomCommandData.ulParam1 = _ttol(argv[iParsedOptions]);
        } else if (0 == _tcsicmp(argv[iParsedOptions], CMDOPTSTR_PARAM2)) {
            iParsedOptions++;
            if (argc <= iParsedOptions) {
                _tprintf(_T("%S: value missing for option %s\n"), __TFUNCTION__, CMDOPTSTR_PARAM2);
                return false;
            }

            pCommandStruct->data.CustomCommandData.ulParam2 = _ttol(argv[iParsedOptions]);
        }
        iParsedOptions++;
    }

    InsertTailList(pCommandLineOptions->CommandList, &pCommandStruct->ListEntry);
    return true;
}
#endif // _CUSTOM_COMMAND_CODE_

bool ValidatePath(CONST TCHAR *path, size_t usLength) {
    TCHAR pathvalidate[UDIRTY_BLOCK_MAX_DATA_LOG_DIRECTORY];
    CONST TCHAR *VolPath = path;
    //Path contains drive letter
    bool VolDrivePath = 1;
    //MIN_LENGTH_DATA_LOG_PATH shd be in form of c:\    
    if (usLength < MIN_LENGTH_DATA_LOG_PATH) {
        _tprintf(_T("\n%S: Length of Path should be greater than %d.\n"), __TFUNCTION__, path, MIN_LENGTH_DATA_LOG_PATH - 1);
        return false;
    } else if (usLength >= UDIRTY_BLOCK_MAX_DATA_LOG_DIRECTORY) {
        _tprintf(_T("\n%S: Length of Path %s is %d greater than %d\n"), 
                __TFUNCTION__, path, usLength, UDIRTY_BLOCK_MAX_DATA_LOG_DIRECTORY);
        return false;
    } else if (path[usLength - 1] != L'\\') {
        _tprintf(_T("\n%S: Path %s does not end with \"\\\"\n"), __TFUNCTION__, path);
        return false;
    }
    //Make the copy of the path to make changes if required
    memmove(pathvalidate, path, usLength * sizeof(WCHAR));
    pathvalidate[usLength] = L'\0';

    if (0 == _tcsncicmp(SYSTEM_ROOT, path, _tcslen(SYSTEM_ROOT))) {
        //SYSTEM_ROOT does not have path in form of drive letter	    
        VolDrivePath = 0;
        //Remove the environmental variable from the path
        pathvalidate[_tcslen(SYSTEM_ROOT)] = L'\0';
        //Expand the environmental variable. pathvalidate[0] contains /
        DWORD dwRet = GetEnvironmentVariable(&pathvalidate[1], pathvalidate, UDIRTY_BLOCK_MAX_DATA_LOG_DIRECTORY);
        if (0 == dwRet) {
            DWORD dwErr = GetLastError();
            _tprintf(_T("\n%S: Path %s contains environment variable %s which does not exist in system\n"), 
                    __TFUNCTION__, path, &pathvalidate[1]);
            return false;
        } else if (UDIRTY_BLOCK_MAX_DATA_LOG_DIRECTORY <= dwRet) {
            _tprintf(_T("\n%S: Length of environmental variable %s after expantion is %d greater than %d\n"), 
                    __TFUNCTION__, &pathvalidate[1], dwRet, UDIRTY_BLOCK_MAX_DATA_LOG_DIRECTORY);
            return false;
        }
        // New length is concatnation of expanded string and path - system_root
        size_t usNewLength = dwRet + (usLength - _tcslen(SYSTEM_ROOT));
        if (usNewLength >= UDIRTY_BLOCK_MAX_DATA_LOG_DIRECTORY) {
            _tprintf(_T("\n%S: Length of Path %s after expantion of environmental variable %s is %d greater than %d\n"), 
                    __TFUNCTION__, path,  &pathvalidate[1], usNewLength, UDIRTY_BLOCK_MAX_DATA_LOG_DIRECTORY);
            return false;
        }
        memmove(&pathvalidate[dwRet], &path[_tcslen(SYSTEM_ROOT)], (usLength  - _tcslen(SYSTEM_ROOT)) * sizeof(WCHAR));
        pathvalidate[usNewLength] = L'\0';
    } else if (0 == _tcsncicmp(DOS_DEVICES_PATH2, path, _tcslen(DOS_DEVICES_PATH2))) {
        if (usLength < (_tcslen(DOS_DEVICES_PATH2) + MIN_LENGTH_DATA_LOG_PATH)) {
            _tprintf(_T("\n%S: Length of Path %s should be greater than %d for path starting with %s.\n"), 
                    __TFUNCTION__, path, _tcslen(DOS_DEVICES_PATH2) + MIN_LENGTH_DATA_LOG_PATH - 1, DOS_DEVICES_PATH2);
            return false;
        }
        if (0 == _tcsncicmp(DOS_DEVICES_PATH2_VOL, path, _tcslen(DOS_DEVICES_PATH2_VOL))) {
            //As the path in form of DOS_DEVICES_PATH2_VOL
            VolDrivePath = 0;
        } else {
            VolPath = path + _tcslen(DOS_DEVICES_PATH2);
        }
    } /* else if (0 == _tcsncicmp(DOS_DEVICES_PATH, path, _tcslen(DOS_DEVICES_PATH))){
        if (usLength < (_tcslen(DOS_DEVICES_PATH) + MIN_LENGTH_DATA_LOG_PATH)) {
            _tprintf(_T("\n%S: Length of Path should be greater than %d for path starting with %s.\n"), 
                    __TFUNCTION__, path, _tcslen(DOS_DEVICES_PATH) + MIN_LENGTH_DATA_LOG_PATH - 1, DOS_DEVICES_PATH);
            return false;
        }
        if (0 == _tcsncicmp(DOS_DEVICES_PATH_VOL, path, _tcslen(DOS_DEVICES_PATH_VOL))) {
            //As the path in form of DOS_DEVICES_PATH_VOL
            VolDrivePath = 0;
        } else {
            VolPath = path + _tcslen(DOS_DEVICES_PATH);
        }
    } */
    if (VolDrivePath) {
        if (FALSE == IS_DRIVE_LETTER(VolPath[0])) {
            _tprintf(_T("\n%S: Path %s does not have valid Drive letter %c between A-Z\n"), 
                    __TFUNCTION__, path, VolPath[0]);
            return false;
        } else if (VolPath[1] != L':') {
            _tprintf(_T("\n%S: Path %s does not have colon after Drive letter %c\n"), 
                    __TFUNCTION__, path, VolPath[0]);
            return false;
        } else if (VolPath[2] != L'\\') {
            _tprintf(_T("\n%S: Path %s does not have \"\\\" after Drive Letter%c:\n"), 
                    __TFUNCTION__, path, VolPath[0]);
            return false;
        }
        TCHAR lpRootPathName[4];
        memmove(lpRootPathName, VolPath, 3*sizeof(WCHAR));
        lpRootPathName[3] = L'\0';
        unsigned int uiDriveType = GetDriveType(lpRootPathName);
        if (DRIVE_REMOTE == uiDriveType) {
            _tprintf(_T("\n%S: Path %s is remote\n"), __TFUNCTION__, path);
            return false;
        } else if (DRIVE_CDROM == uiDriveType) {
            _tprintf(_T("\n%S: Path %s is type of CDROM\n"), __TFUNCTION__, path);
            return false;
        } else if (DRIVE_UNKNOWN == uiDriveType) {
            _tprintf(_T("\n%S: Path %s is unknown\n"), __TFUNCTION__, path);
            return false;
        } else if (DRIVE_NO_ROOT_DIR == uiDriveType) {
            _tprintf(_T("\n%S: Path %s does not have root dir\n"), __TFUNCTION__, path);
            return false;
        }
    }
    DWORD attrib = GetFileAttributes(pathvalidate);
    if ( attrib == 0xFFFFFFFF || !(attrib & FILE_ATTRIBUTE_DIRECTORY) ) {
        _tprintf(_T("\n%S: Path %s is not valid\n"), __TFUNCTION__, path);
        //return false;
    }
    return true;
}

