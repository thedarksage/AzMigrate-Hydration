#include "stdafx.h"
#include "HostAgentLogger.h"
#include <stdarg.h>
#include <stdio.h>
#include <fstream>
#include <sstream>
#include <string>
#include <iostream>
using namespace std;

#include <ace/Guard_T.h>
#include <ace/Mutex.h>
#include "inmsafecapis.h"

string szLogFileDir = ".\\";
string szLogFileName = "HostAgentConfig";
string szLogFileExt = ".log";
string szFileName = "HostAgentConfig.log";


int maxBackupFileCount = 3;
int nLogLevel = SV_LOG_ALWAYS;
int nLogInitSize = 10 * 1024 * 1024;

LONG nextFileIndex = 0;
long linecount = 0;

bool bBackupDebugLog = true;

char pchLogLevelMapping[][10]={"DISABLE","FATAL","SEVERE","ERROR","WARNING","INFO","DEBUG","ALWAYS"};

std::ofstream svstream;
typedef ACE_Mutex SyncLog_t;
typedef ACE_Guard<SyncLog_t> SyncLogGuard_t;
SyncLog_t SyncLog;


bool HostConfigFileExist(string filename);
void BackupAndResetStream();
static bool DebugPrintfV( SV_LOG_LEVEL LogLevel, const char* format, va_list args );

void SetHostConfigLogFileName(const char* fileName)
{
    szFileName = fileName;
    svstream.clear();
	svstream.open(fileName,ios::out | ios::app );
}
void CloseHostConfigLog()
{
	if(svstream.is_open())
		svstream.close();
}
template <typename T>
std::string ToString(const T &val)
{
	std::ostringstream ss;
	ss << val;
	return ss.str();
}
bool HostConfigFileExist(string filename)
{
	std::ifstream ifile;
	ifile.open(filename.c_str());
	
	if (ifile.is_open() == true)
	{
		ifile.close();
		return true;
	}
	else      
		return false;
}
int GetHostConfigFileSize(string filename)
{
	std::ifstream ifile;
	int fileSize = 0;
	ifile.open(filename.c_str(), ios::binary | ios::ate);
	
	if (ifile.is_open() == true)
	{
		fileSize = ifile.tellg();
		ifile.close();
	}      
	return fileSize;
}

void OutputDebugStringToLog(int nOutputLoggingLevel, char* szDebugStr)
{  
    SyncLogGuard_t guard(SyncLog);

    //first check file size and see if we have to truncate the file
    if(linecount == 500) 
    {
        int fileSize = 0;
        fileSize = GetHostConfigFileSize(szFileName);
        if (fileSize > 0)
        {
            if(fileSize >= nLogInitSize)
				BackupAndResetStream();
            linecount = 1;
        }
    }
    else
    {
        linecount++;
    }

	struct tm *hostConfigToday;
    time_t hostConfigLogTime;
    
    time( &hostConfigLogTime );
    hostConfigToday = localtime( &hostConfigLogTime );

    char hostConfigPresent[70];
    memset(hostConfigPresent,0,70);
	inm_sprintf_s(hostConfigPresent, ARRAYSIZE(hostConfigPresent), "(%02d-%02d-20%02d %02d:%02d:%02d): %7s ",
            hostConfigToday->tm_mon + 1,
            hostConfigToday->tm_mday,
            hostConfigToday->tm_year - 100,
            hostConfigToday->tm_hour,
            hostConfigToday->tm_min,
            hostConfigToday->tm_sec,
            pchLogLevelMapping[nOutputLoggingLevel] 
            );
        
    if ( svstream.fail() )
    {
        svstream.clear(0);
    }
    svstream << hostConfigPresent << " " 
             << ACE_OS::getpid() << " " 
             << ACE_OS::thr_self() << " " 
             << szDebugStr;
    svstream.flush();
}

void __cdecl DebugPrintf( SV_LOG_LEVEL LogLevel, const char* format, ... )
{
    va_list a;
    int iLastError = ACE_OS::last_error();
    
    if( 0 == strlen( format ) )
    {
        return;
    }

    va_start( a, format );

//    if ( InDebugPrintf == false)
    {
//		InDebugPrintf = true;
    	DebugPrintfV( LogLevel, format, a );
//		InDebugPrintf = false;
    }
    va_end( a );
    ACE_OS::last_error(iLastError);
}

static bool DebugPrintfV( SV_LOG_LEVEL LogLevel, const char* format, va_list args )
{
    char szDebugString[16*1024]; 
    if( 0 == lstrlenA( format ) )
    {
        return( false );
    }

    _vsnprintf_s(szDebugString, sizeof(szDebugString) - 1, format, args);
    szDebugString[ sizeof( szDebugString ) - 1 ] = 0;

    if( 0 == lstrlenA( szDebugString ) )
    {
        return( false );
    }
	
    OutputDebugStringToLog( (int) LogLevel, szDebugString );

    return( true );
}

void BackupAndResetStream()
{
	if(bBackupDebugLog)
	{
		svstream.close();
		errno_t err;
		char drive[_MAX_DRIVE];
	    char dir[_MAX_DIR];
	    char fname[_MAX_FNAME];
	    char ext[_MAX_EXT];

		err = _splitpath_s( szFileName.c_str(), drive, _MAX_DRIVE, dir, _MAX_DIR, fname,
                       _MAX_FNAME, ext, _MAX_EXT );

		if (err == 0)
	    {
			szLogFileDir = drive;
			szLogFileDir.append(dir);
			szLogFileName = fname;
			szLogFileExt = ext;
			if (nextFileIndex >= maxBackupFileCount)
				nextFileIndex = 0;
			string backupFile = szLogFileDir;
			backupFile.append(szLogFileName);
		    backupFile.append(ToString(nextFileIndex));
		    backupFile.append(szLogFileExt);
		    if(HostConfigFileExist(backupFile))
		    {
				DeleteFile(backupFile.c_str());
		    }
			rename(szFileName.c_str(), backupFile.c_str());
			//InterlockedIncrement(&nextFileIndex);
			++nextFileIndex;
		}	
		SetHostConfigLogFileName(szFileName.c_str());
	}
}