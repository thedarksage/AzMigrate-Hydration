#include <windows.h>
#include <stdio.h>
#include <tchar.h>
#include <malloc.h>
#include "ListEntry.h"
#include <InmFltInterface.h>
#include <map>
#include <vector>
#include "drvutil.h"
#include "VFltCmdsParse.h"
#include "VVolCmdsParse.h"

extern bool help;

bool
ParseMountVirtualVolumeCommand(TCHAR *argv[], int argc, int &iParsedOptions, PCOMMAND_LINE_OPTIONS pCommandLineOptions)
{
    PCOMMAND_STRUCT     pCommandStruct = NULL;
    ULONG               cchFileName;

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
        _tprintf(_T("ParseMountVirtualVolumeCommand: AllocateCommandStruct failed\n"));
        return false;
    }
    pCommandStruct->ulCommand = CMD_MOUNT_VIRTUAL_VOLUME;

    // This command has filename as a parameter.
    if (argc <= iParsedOptions) {
        _tprintf(_T("ParseMountVirtualVolumeCommand: missed filename parameter for %s\n"), CMDSTR_MOUNT_VIRTUAL_VOLUME);
        return false;
    }
    
    pCommandStruct->data.MountVirtualVolumeData.VolumeType = ecSparseFileVolume;

    if(IS_DRIVE_LETTER_PAR(argv[iParsedOptions]) && IS_DRIVE_LETTER(argv[iParsedOptions][0]))
        pCommandStruct->data.MountVirtualVolumeData.VolumeType = ecRetentionVolume;

    // let us get the file name
    cchFileName = (ULONG) _tcslen(argv[iParsedOptions]);
    // TODO: if not unicode has to be converted from CP_ASCII to UNICODE
    pCommandStruct->data.MountVirtualVolumeData.ulLengthOfBuffer = (cchFileName + 1) * sizeof(WCHAR);
    pCommandStruct->data.MountVirtualVolumeData.pVolumeImageFileName = 
                        (WCHAR *)malloc(pCommandStruct->data.MountVirtualVolumeData.ulLengthOfBuffer);
    if (NULL == pCommandStruct->data.MountVirtualVolumeData.pVolumeImageFileName) {
        _tprintf(_T("Memory allocation failed for volume file name %s\n"), argv[iParsedOptions]);
        return false;
    }

    tcstowcs(pCommandStruct->data.MountVirtualVolumeData.pVolumeImageFileName, 
             pCommandStruct->data.MountVirtualVolumeData.ulLengthOfBuffer,
             argv[iParsedOptions]);

    iParsedOptions++;

    // This command has driverletter as a parameter.
    if (argc <= iParsedOptions) {
        _tprintf(_T("ParseMountVirtualVolumeCommand: missing drive letter parameter for %s\n"), CMDSTR_MOUNT_VIRTUAL_VOLUME);
        return false;
    }

    if(IS_OPTION(argv[iParsedOptions])){
        // no volume letter specified
        _tprintf(_T("ParseMountVirtualVolumeCommand: Expecting RecoveryPoint instead found Invalid parameter %s for Command %s\n"), 
                                    argv[iParsedOptions], CMDSTR_MOUNT_VIRTUAL_VOLUME);
        return false;
    }

	_tcscpy_s(pCommandStruct->data.MountVirtualVolumeData.MountPoint, TCHAR_ARRAY_SIZE(pCommandStruct->data.MountVirtualVolumeData.MountPoint), argv[iParsedOptions]);

    iParsedOptions++;
    InsertTailList(pCommandLineOptions->CommandList, &pCommandStruct->ListEntry);
    return true;
}

bool
ParseUnmountVirtualVolumeCommand(TCHAR *argv[], int argc, int &iParsedOptions, PCOMMAND_LINE_OPTIONS pCommandLineOptions)
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
        _tprintf(_T("ParseUnmountVirtualVolumeCommand: AllocateCommandStruct failed\n"));
        return false;
    }
    pCommandStruct->ulCommand = CMD_UNMOUNT_VIRTUAL_VOLUME;

    // This command has driverletter as a parameter.
    if (argc <= iParsedOptions) {
        _tprintf(_T("ParseUnmountVirtualVolumeCommand: missing drive letter parameter for %s\n"), CMDSTR_UNMOUNT_VIRTUAL_VOLUME);
        return false;
    }

	_tcscpy_s(pCommandStruct->data.UnmountVirtualVolumeData.MountPoint, TCHAR_ARRAY_SIZE(pCommandStruct->data.UnmountVirtualVolumeData.MountPoint), argv[iParsedOptions]);

    iParsedOptions++;
    InsertTailList(pCommandLineOptions->CommandList, &pCommandStruct->ListEntry);
    return true;
}

bool
ParseSetControlFlagsCommand(TCHAR *argv[], int argc, int &iParsedOptions, PCOMMAND_LINE_OPTIONS pCommandLineOptions)
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

    pCommandStruct->ulCommand = CMD_SET_VV_CONTROL_FLAGS;

    pCommandStruct->data.ControlFlagsInput.ulControlFlags = 0;
    pCommandStruct->data.ControlFlagsInput.eOperation = ecBitOpSet;

    while((argc > iParsedOptions) && IS_SUB_OPTION(argv[iParsedOptions])) {
        if (0 == _tcsicmp(argv[iParsedOptions], CMDOPTSTR_OUT_OF_ORDER_WRITE)) {
            pCommandStruct->data.ControlFlagsInput.ulControlFlags |= VV_CONTROL_FLAG_OUT_OF_ORDER_WRITE;
        } else if (0 == _tcsicmp(argv[iParsedOptions], CMDOPTSTR_RESET_VV_CONTROL_FLAGS)) {
            pCommandStruct->data.ControlFlagsInput.eOperation = ecBitOpReset;
        }
        iParsedOptions++;
    }

    InsertTailList(pCommandLineOptions->CommandList, &pCommandStruct->ListEntry);
    return true;
}

bool ParseStartVirtualVolumeCommand(TCHAR *argv[], int argc, int &iParsedOptions, PCOMMAND_LINE_OPTIONS pCommandLineOptions)
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
        _tprintf(_T("ParseStartVirtualVolumeCommand: AllocateCommandStruct failed\n"));
        return false;
    }

    pCommandStruct->ulCommand = CMD_START_VIRTUAL_VOLUME;

    InsertTailList(pCommandLineOptions->CommandList, &pCommandStruct->ListEntry);
    return true;
}


bool ParseStopVirtualVolumeCommand(TCHAR *argv[], int argc, int &iParsedOptions, PCOMMAND_LINE_OPTIONS pCommandLineOptions)
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
        _tprintf(_T("ParseStopVirtualVolumeCommand: AllocateCommandStruct failed\n"));
        return false;
    }

    pCommandStruct->ulCommand = CMD_STOP_VIRTUAL_VOLUME;

    InsertTailList(pCommandLineOptions->CommandList, &pCommandStruct->ListEntry);
    return true;
}


bool ParseConfigureVirtualVolumeCommand(TCHAR *argv[], int argc, int &iParsedOptions, PCOMMAND_LINE_OPTIONS pCommandLineOptions)
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
        _tprintf(_T("ParseConfigureVirtualVolumeCommand: AllocateCommandStruct failed\n"));
        return false;
    }

    pCommandStruct->ulCommand = CMD_CONFIGURE_VIRTUAL_VOLUME;
    while((argc > iParsedOptions) && IS_SUB_OPTION(argv[iParsedOptions])) {
        if (0 == _tcsicmp(argv[iParsedOptions], CMDOPTSTR_MEM_FILE_HANDLE_CACHE_KB)) {
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
            pCommandStruct->data.ConfigureVirtualVolumeData.ulMemFileHandleCacheInKB = _tcstoul(argv[iParsedOptions], NULL, 0);
            pCommandStruct->data.ConfigureVirtualVolumeData.ulFlags = 0;
            pCommandStruct->data.ConfigureVirtualVolumeData.ulFlags |= FLAG_MEM_FOR_FILE_HANDLE_CACHE;
            iParsedOptions++;

        }
    }

    InsertTailList(pCommandLineOptions->CommandList, &pCommandStruct->ListEntry);
    return true;
}

