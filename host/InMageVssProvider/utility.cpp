
#include "stdafx.h"
#include "rpcdce.h"
#include <iostream>

void ShowMsg(LPCSTR msg,...)
{
    CHAR InmBuf[4096];
    va_list args;
    va_start( args, msg );
    _vsnprintf_s( InmBuf, NUM_OF_ELEMS( InmBuf ), msg, args );
    InmBuf[NUM_OF_ELEMS( InmBuf ) - 1] = 0;
    va_end( args );
    OutputDebugString( InmBuf );

}

void LogVssEvent(WORD EventLogType,DWORD dwEventVwrLogFlag,LPCSTR pFormat,...)
{
	if(!dwEventVwrLogFlag)//If the EventViewerLoggingFlag is not set then don't log in the Event Viewer
	{
		return;
	}
    CHAR    chInmMsg[256];
    HANDLE   hEventSource;    
	LPCSTR  lpszStrings[1];
    va_list  pArg;
	
    va_start(pArg, pFormat);
    _vsnprintf_s(chInmMsg, NUM_OF_ELEMS( chInmMsg ), pFormat, pArg);
    va_end(pArg);

    lpszStrings[0] = chInmMsg;


    hEventSource = ::RegisterEventSource(NULL, "AzureSiteRecoveryVssProvider");
    if (hEventSource != NULL)
	{

        ::ReportEvent(hEventSource,EventLogType,0,INMAGE_SWPRV_EVENTLOG_INFO_GENERIC_MESSAGE,
                      NULL,1,0,&lpszStrings[0],NULL);
        ::DeregisterEventSource(hEventSource);
    }
}

