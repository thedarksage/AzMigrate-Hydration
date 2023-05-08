#ifndef _VFLTCMDSPARSE_H
#define _VFLTCMDSPARSE_H

#if !defined(TCHAR_ARRAY_SIZE)
#define TCHAR_ARRAY_SIZE( a ) ( sizeof( a ) / sizeof( a[ 0 ] ) )
#endif

#if !defined(TCHAR_ARRAY_SIZE)
#define TCHAR_ARRAY_SIZE( a ) ( sizeof( a ) / sizeof( a[ 0 ] ) )
#endif

#define DOS_DEVICES_PATH2               L"\\??\\"
#define DOS_DEVICES_PATH2_VOL           L"\\??\\Volume{"
/*#define DOS_DEVICES_PATH              L"\\DosDevices\\"
#define DOS_DEVICES_PATH_VOL            L"\\DosDevices\\Volume{"*/
#define SYSTEM_ROOT                     L"\\SystemRoot"
#define MIN_LENGTH_DATA_LOG_PATH        3

bool
ParseWaitForKeywordCommand(TCHAR *argv[], int argc, int &iParsedOptions, PCOMMAND_LINE_OPTIONS pCommandLineOptions);
bool
ParseClearDiffsCommand(TCHAR *argv[], int argc, int &iParsedOptions, PCOMMAND_LINE_OPTIONS pCommandLineOptions);
bool
ParseStopFilteringCommand(TCHAR *argv[], int argc, int &iParsedOptions, PCOMMAND_LINE_OPTIONS pCommandLineOptions);
bool
ParseStartFilteringCommand(TCHAR *argv[], int argc, int &iParsedOptions, PCOMMAND_LINE_OPTIONS pCommandLineOptions);
bool
ParseResyncStartNotifyCommand(TCHAR *argv[], int argc, int &iParsedOptions, PCOMMAND_LINE_OPTIONS pCommandLineOptions);
bool
ParseResyncEndNotifyCommand(TCHAR *argv[], int argc, int &iParsedOptions, PCOMMAND_LINE_OPTIONS pCommandLineOptions);
bool
ParseSetDiskClusteredCommand(TCHAR* argv[], int argc, int& iParsedOptions, PCOMMAND_LINE_OPTIONS pCommandLineOptions);
bool
ParseShutdownNotifyCommand(TCHAR *argv[], int argc, int &iParsedOptions, PCOMMAND_LINE_OPTIONS pCommandLineOptions);
bool
ParseCreateThreadCommand(TCHAR *argv[], int argc, int &iParsedOptions, PCOMMAND_LINE_OPTIONS pCommandLineOptions);
bool
ParseEndOfThreadCommand(TCHAR *argv[], int argc, int &iParsedOptions, PCOMMAND_LINE_OPTIONS pCommandLineOptions);
bool
ParseWaitForThreadCommand(TCHAR *argv[], int argc, int &iParsedOptions, PCOMMAND_LINE_OPTIONS pCommandLineOptions);
void
InitializeWriteStreamOptions(WRITE_STREAM_DATA &WriteStreamData);
bool
ParseWriteStreamCommand(TCHAR *argv[], int argc, int &iParsedOptions, PCOMMAND_LINE_OPTIONS pCommandLineOptions);
bool
ParseReadStreamCommand(TCHAR *argv[], int argc, int &iParsedOptions, PCOMMAND_LINE_OPTIONS pCommandLineOptions);
bool
ParseCheckShutdownStatusCommand(TCHAR *argv[], int argc, int &iParsedOptions, PCOMMAND_LINE_OPTIONS pCommandLineOptions);
bool
ParseServiceStartNotifyCommand(TCHAR *argv[], int argc, int &iParsedOptions, PCOMMAND_LINE_OPTIONS pCommandLineOptions);
bool
ParseGetDriverVersionCommand(TCHAR *argv[], int argc, int &iParsedOptions, PCOMMAND_LINE_OPTIONS pCommandLineOptions);
bool
ParseVerboseCommand(TCHAR *argv[], int argc, int &iParsedOptions, PCOMMAND_LINE_OPTIONS pCommandLineOptions);
bool
ParseCheckServiceStartStatusCommand(TCHAR *argv[], int argc, int &iParsedOptions, PCOMMAND_LINE_OPTIONS pCommandLineOptions);
bool
ParsePrintStatisticsCommand(TCHAR *argv[], int argc, int &iParsedOptions, PCOMMAND_LINE_OPTIONS pCommandLineOptions);
bool
ParsePrintAdditionalStatsCommand(TCHAR *argv[], int argc, int &iParsedOptions, PCOMMAND_LINE_OPTIONS pCommandLineOptions);
bool
ParsePrintGlobaTSSNCommand(TCHAR *argv[], int argc, int &iParsedOptions, PCOMMAND_LINE_OPTIONS pCommandLineOptions);
bool
ParseGetVolumeSizeCommand(TCHAR *argv[], int argc, int &iParsedOptions, PCOMMAND_LINE_OPTIONS pCommandLineOptions);
bool
ParseGetWriteOrderStateCommand(TCHAR *argv[], int argc, int &iParsedOptions, PCOMMAND_LINE_OPTIONS pCommandLineOptions);
bool
ParseGetVolumeFlagsCommand(TCHAR *argv[], int argc, int &iParsedOptions, PCOMMAND_LINE_OPTIONS pCommandLineOptions);
bool
ParseGetDataModeCommand(TCHAR *argv[], int argc, int &iParsedOptions, PCOMMAND_LINE_OPTIONS pCommandLineOptions);
bool
ParseClearStatisticsCommand(TCHAR *argv[], int argc, int &iParsedOptions, PCOMMAND_LINE_OPTIONS pCommandLineOptions);
bool
ParseWaitTimeCommand(TCHAR *argv[], int argc, int &iParsedOptions, PCOMMAND_LINE_OPTIONS pCommandLineOptions);
bool
ConvertDebugLevelStringToNum(TCHAR *pString, ULONG &ulDebugLevel);
bool
GetDebugModules(TCHAR *argv[], int argc, int &iParsedOptions, ULONG &ulDebugModules);
bool
ParseSetDebugInfoCommand(TCHAR *argv[], int argc, int &iParsedOptions, PCOMMAND_LINE_OPTIONS pCommandLineOptions);
bool ParseTagVolumeCommand_v2(TCHAR *argv[], int argc, int &iParsedOptions, PCOMMAND_LINE_OPTIONS pCommandLineOptions);
bool ParseTagVolumeCommand(TCHAR *argv[], int argc, int &iParsedOptions, PCOMMAND_LINE_OPTIONS pCommandLineOptions);
bool
ParseSetVolumeFlagsCommand(TCHAR *argv[], int argc, int &iParsedOptions, PCOMMAND_LINE_OPTIONS pCommandLineOptions);
bool
ParseSetDriverFlagsCommand(TCHAR *argv[], int argc, int &iParsedOptions, PCOMMAND_LINE_OPTIONS pCommandLineOptions);
bool
ParseGetDirtyBlocksCommand(TCHAR *argv[], int argc, int &iParsedOptions, PCOMMAND_LINE_OPTIONS pCommandLineOptions);
bool
ParseGetSyncCommand(TCHAR *argv[], int argc, int &iParsedOptions, PCOMMAND_LINE_OPTIONS pCommandLineOptions);
bool
ParseGetSyncCommand_v2(TCHAR *argv[], int argc, int &iParsedOptions, PCOMMAND_LINE_OPTIONS pCommandLineOptions);
bool
ParseCommitBlocksCommand(TCHAR *argv[], int argc, int &iParsedOptions, PCOMMAND_LINE_OPTIONS pCommandLineOptions);
bool
ParseSetResyncRequired(TCHAR *argv[], int argc, int &iParsedOptions, PCOMMAND_LINE_OPTIONS pCommandLineOptions);
bool
ParseSetDataFileWriterThreadPriority(TCHAR *argv[], int argc, int &iParsedOptions, PCOMMAND_LINE_OPTIONS pCommandLineOptions);
bool
ParseSetWorkerThreadPriority(TCHAR *argv[], int argc, int &iParsedOptions, PCOMMAND_LINE_OPTIONS pCommandLineOptions);
bool
ParseSetDataToDiskSizeLimit(TCHAR *argv[], int argc, int &iParsedOptions, PCOMMAND_LINE_OPTIONS pCommandLineOptions);
bool
ParseSetDataToMinFreeDiskSizeLimit(TCHAR *argv[], int argc, int &iParsedOptions, PCOMMAND_LINE_OPTIONS pCommandLineOptions);
bool
ParseSetDataNotifySizeLimit(TCHAR *argv[], int argc, int &iParsedOptions, PCOMMAND_LINE_OPTIONS pCommandLineOptions);
bool
ParseSetIoSizeArray(TCHAR *argv[], int argc, int &iParsedOptions, PCOMMAND_LINE_OPTIONS pCommandLineOptions);
bool
ParseSetDriverInfo(TCHAR *argv[], int argc, int &iParsedOptions, PCOMMAND_LINE_OPTIONS pCommandLineOptions);
bool ParseGetLcn(TCHAR *argv[], int argc, int &iParsedOptions, PCOMMAND_LINE_OPTIONS pCommandLineOptions);
bool ValidatePath(CONST TCHAR *path, size_t usLength);
#ifdef _CUSTOM_COMMAND_CODE_
bool
ParseCustomCommand(TCHAR *argv[], int argc, int &iParsedOptions, PCOMMAND_LINE_OPTIONS pCommandLineOptions);
#endif // _CUSTOM_COMMAND_CODE_
#endif //_VFLTCMDSPARSE_H
