#include <stdarg.h>
#include <stdio.h>
#include <string>
#include <fstream>
using namespace std;
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstdlib>
#include "../fr_common/portable.h"
#include "../fr_config/svconfig.h"
#include "../fr_common/hostagenthelpers_ported.h"
#include "inmsafecapis.h"
#include "logger.h"
#include "inmsafecapis.h"

#define LOGGER_URL "/cxlogger.php"
#define TAG_FAILED "FAILED"
#define TAG_WARNING "WARNING"

Logger_Agent_Type LogAgenttype = VxAgentLogger;
char pchLogLevelMapping[][10]={"DISABLE","FATAL","SEVERE","ERROR","WARNING","INFO","DEBUG","ALWAYS"};
int nLogLevel = SV_LOG_ALWAYS;
int nLogInitSize = 10 * 1024 * 1024;
int linecount = 0;
ofstream svstream;
string szFileName = "";

bool EnableRemoteLogging = true; 
string AgentHostId = "";
string CxServerIpAddress = "";
int CxServerPort = 80;

void OutputDebugString(int nOutputLoggingLevel, char* szDebugStr);
static bool DebugPrintfToCx(const char *ipAddress, SV_INT port, const char *pchPostURL, SV_LOG_LEVEL LogLevel, const char *szDebugString);
void TruncateFile();

void SetLogLevel(int level)
{
   nLogLevel = level;
}

void SetLogInitSize(int size)
{
    nLogInitSize = size;
}

void SetLogFileName(const char* fileName)
{
    szFileName = fileName;
    svstream.clear();
    svstream.open(fileName,ios::out | ios::app );
}

static void DebugPrintfV( SV_LOG_LEVEL LogLevel, const char* format, va_list args )
{
	/* place holder function */

	return;
}
static bool DebugPrintfV( SV_LOG_LEVEL LogLevel, SV_LONG hr, const char* format, va_list args )
{
	/* place holder function */

	return true;
}
void SV_CDECL DebugPrintf( const char* format, ... )
{
    va_list a;    
	char *pSubStringFailed = TAG_FAILED;
	char *pSubStringWarning = TAG_WARNING;

    if( 0 == strlen( format ) )
    {
        return ;
    }

    va_start( a, format );

	// Filter for Fail
    if (strstr (format, pSubStringFailed))
	{
		DebugPrintfV( SV_LOG_ERROR, format, a );		
	}
	else if ( strstr (format, pSubStringWarning) ) 
	{
		DebugPrintfV( SV_LOG_WARNING, format, a );
	}
	else
	{
		DebugPrintfV( SV_LOG_DEBUG, format, a );
	}

    va_end( a ); 
}

void SV_CDECL DebugPrintf( int nLogLevel, const char* format, ... )
{
    va_list a;
   
    if( 0 == strlen( format ) )
    {
        return;
    }

    va_start( a, format );
	DebugPrintfV((SV_LOG_LEVEL) nLogLevel, format, a );
    va_end( a );
 
}

void SV_CDECL DebugPrintf( SV_LOG_LEVEL LogLevel, const char* format, ... )
{
    va_list a;

    
    if( 0 == strlen( format ) )
    {
        return;
    }

    va_start( a, format );
    DebugPrintfV( LogLevel, format, a );
    va_end( a );
}


void SV_CDECL DebugPrintf( SV_LONG hr, const char* format, ... )
{

    char szDebugString[16*1024];
    va_list a;

    if( 0 == strlen( format ) )
    {
        return;
    }

    va_start( a, format );
    DebugPrintfV( SV_LOG_INFO, hr, format, a );
    va_end( a );

   OutputDebugString( SV_LOG_INFO,szDebugString );

}

void SV_CDECL DebugPrintf(SV_LOG_LEVEL LogLevel,SV_LONG hr,const char*format, ...)
{
    va_list a;

    if( 0 == strlen( format ) )
    {
        return;
    }

    va_start( a, format );
    DebugPrintfV( LogLevel, hr, format, a );
    va_end( a );

}

void OutputDebugString(int nOutputLoggingLevel, char* szDebugStr)
{
    if(SV_LOG_DISABLE == nLogLevel) return;
	
    //first check file size and see if we have to truncate the file
    if(linecount == 100) 
    {
        SV_ULONG *pUnused = 0;
        SV_ULONG size = SVGetFileSize( szFileName.c_str(), pUnused );
        if(size >= (SV_ULONG)nLogInitSize)
        {
            TruncateFile();
        }
        linecount = 1;
    }
    else
    {
        linecount++;
    }
    time_t ltime;
    struct tm *today;
    if( nOutputLoggingLevel <= nLogLevel)
    {
        time( &ltime );
        today= localtime( &ltime );
        char present[30];
        memset(present,0,30);
	
		inm_sprintf_s(present, ARRAYSIZE(present), "Lv %d (%02d-%02d-20%02d %02d:%02d:%02d): ",
            nOutputLoggingLevel, 
            today->tm_mon + 1,
            today->tm_mday,
            today->tm_year - 100,
            today->tm_hour,
            today->tm_min,
            today->tm_sec
            );
        svstream << present << " " << szDebugStr;
        svstream.flush();
    }
}



static bool DebugPrintfToCx(const char *ipAddress, SV_INT port, const char *pchPostURL, SV_LOG_LEVEL LogLevel, const char *szDebugString)
{
	/* place holder function */
	return (false);
}




void TruncateFile()
{
   svstream.close();
   svstream.open(szFileName.c_str(),ios::out | ios::trunc );
}

void CloseDebug()
{
    svstream.close();
}

void SetLoggerAgentType(Logger_Agent_Type)
{
	LogAgenttype=FxAgentLogger;
}

Logger_Agent_Type GetLoggerAgentType(void)
{
	return LogAgenttype;
}

void GetLoggingDetails(char const * configFilename)
{
	
	string value = " ";
    SVERROR hr = SVS_OK;
	
	string configFile = configFilename;
    SVConfig* conf = SVConfig::GetInstance();

    conf->Parse(configFile);
           
	value.erase();
	hr = conf->GetValue(ENABLE_REMOTE_LOG_OPTION_KEY,value);

	if(!hr.failed())
	{
		if (atoi(value.c_str()) <= 0)
			EnableRemoteLogging = false;
		else if (atoi(value.c_str()) >= 1)
			EnableRemoteLogging = true;
	}

	value.erase();
	hr = conf->GetValue(KEY_SV_SERVER,value);
		
	if(!hr.failed())
	{
		CxServerIpAddress = value;

		hr = conf->GetValue(KEY_SV_SERVER_PORT,value);

		if(!hr.failed())
		{
			CxServerPort = atoi(value.c_str());
		}
		else
		{
			CxServerPort = 80;
		}
	}

    string szHostID = "";
    hr = conf->GetValue(KEY_AGENT_HOST_ID,szHostID);
	if(!hr.failed())
	{
		AgentHostId = szHostID;
	}

	return;
}
