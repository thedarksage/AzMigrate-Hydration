#ifndef _VVOLCMDSPARSE_H_
#define _VVOLCMDSPARSE_H_

#if !defined(TCHAR_ARRAY_SIZE)
#define TCHAR_ARRAY_SIZE( a ) ( sizeof( a ) / sizeof( a[ 0 ] ) )
#endif

#if !defined(TCHAR_ARRAY_SIZE)
#define TCHAR_ARRAY_SIZE( a ) ( sizeof( a ) / sizeof( a[ 0 ] ) )
#endif

bool ParseMountVirtualVolumeCommand(TCHAR *argv[], int argc, int &iParsedOptions, PCOMMAND_LINE_OPTIONS pCommandLineOptions);
bool ParseUnmountVirtualVolumeCommand(TCHAR *argv[], int argc, int &iParsedOptions, PCOMMAND_LINE_OPTIONS pCommandLineOptions);
bool ParseSetControlFlagsCommand(TCHAR *argv[], int argc, int &iParsedOptions, PCOMMAND_LINE_OPTIONS pCommandLineOptions);
bool ParseStartVirtualVolumeCommand(TCHAR *argv[], int argc, int &iParsedOptions, PCOMMAND_LINE_OPTIONS pCommandLineOptions);
bool ParseStopVirtualVolumeCommand(TCHAR *argv[], int argc, int &iParsedOptions, PCOMMAND_LINE_OPTIONS pCommandLineOptions);
bool ParseConfigureVirtualVolumeCommand(TCHAR *argv[], int argc, int &iParsedOptions, PCOMMAND_LINE_OPTIONS pCommandLineOptions);
#endif //_VVOLCMDSPARSE_H_