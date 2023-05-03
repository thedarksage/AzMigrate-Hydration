// stdafx.h : include file for standard system include files,
//      or project specific include files that are used frequently,
//      but are changed infrequently

#if !defined(AFX_STDAFX_H__4BC71B7C_7F8C_4576_8895_BB3BB32DF6D5__INCLUDED_)
#define AFX_STDAFX_H__4BC71B7C_7F8C_4576_8895_BB3BB32DF6D5__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#define STRICT
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0400
#endif
#define _ATL_APARTMENT_THREADED

#include <atlbase.h>
#include <assert.h>
#include "../fragent/fragent.h"

//You may derive a class from CComModule and use it if you want to override
//something, but do not change the name of _Module
class CServiceModule 
{
public:
    //
    // Service methods
    //
    HRESULT RegisterServer(BOOL InmbRegTypeLib, BOOL InmbService);
    HRESULT UnregisterServer();
    void Init(_ATL_OBJMAP_ENTRY* Inmp, HINSTANCE Inmh, UINT InmnServiceNameID, const GUID* Inmplibid = NULL);
    void Start();
    void ServiceMain(DWORD dwArgc, LPTSTR* lpszArgv);
    void Handler(DWORD dwOpcode);
    void Run();
    BOOL IsInstalled();
    BOOL Install();
    BOOL Uninstall();
    void LogEvent(LPCTSTR pszFormat, ...);
    void SetServiceStatus(DWORD dwState);
    void SetupAsLocalServer();
    SVERROR UpdateJobState();

private:
    //
    // Service methods
    //
    static void WINAPI _ServiceMain(DWORD dwArgc, LPTSTR* lpszArgv);
    static void WINAPI _Handler(DWORD dwOpcode);
    bool QuitRequested(int waitTimeSeconds);

public:
    //
    // Service state
    //
    TCHAR m_InmszServiceName[256];
    SERVICE_STATUS_HANDLE m_InmhServiceStatus;
    SERVICE_STATUS m_Inmstatus;
    DWORD dwThreadID;
    BOOL m_InmbService;
    TCHAR m_InmszServiceLongName[ 256 ];
    TCHAR m_InmszServiceDescription[ 256 ];
    CFileReplicationAgent m_agent;
	bool m_bShouldStop;
};

extern CServiceModule _Module;
#include <atlcom.h>

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_STDAFX_H__4BC71B7C_7F8C_4576_8895_BB3BB32DF6D5__INCLUDED)
